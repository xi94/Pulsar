#include "os/http.h"

#include <chrono>

#include "os/macos/macos.h"

namespace {
constexpr const char*    K_USER_AGENT              = "Pulsar-Updater/1.0";
constexpr NSTimeInterval K_IDLE_TIMEOUT_SECONDS    = 15.0;
constexpr i64            K_CANCEL_POLL_NANOSECONDS = 100'000'000;

struct Download {
	std::vector<u8>*                      body;
	usize                                 max_bytes;
	const os::HttpProgress*               progress;
	std::chrono::steady_clock::time_point started;
	dispatch_semaphore_t                  finished;
	std::string                           error;
	std::atomic<bool>                     cancelled{false};
};
}

@interface PulsarDownloadDelegate : NSObject <NSURLSessionDataDelegate>
- (instancetype)initWithDownload:(Download*)t_download;
@end

@implementation PulsarDownloadDelegate {
	Download* m_download;
}

- (instancetype)initWithDownload:(Download*)t_download
{
	self = [super init];
	if (self != nil) {
		m_download = t_download;
	}

	return self;
}

- (void)URLSession:(NSURLSession*)t_session
			  dataTask:(NSURLSessionDataTask*)t_task
	didReceiveResponse:(NSURLResponse*)t_response
	 completionHandler:(void (^)(NSURLSessionResponseDisposition))t_completion
{
	const NSInteger status   = [t_response isKindOfClass:NSHTTPURLResponse.class] ? static_cast<NSHTTPURLResponse*>(t_response).statusCode : 0;
	const long long expected = t_response.expectedContentLength;

	if (status < 200 || status >= 300) {
		m_download->error = "server returned HTTP " + std::to_string(status);
		t_completion(NSURLSessionResponseCancel);
		return;
	}

	if (expected > 0 && static_cast<u64>(expected) > m_download->max_bytes) {
		m_download->error = "the server offered a file far larger than any Pulsar build";
		t_completion(NSURLSessionResponseCancel);
		return;
	}

	if (expected > 0) {
		m_download->body->reserve(static_cast<usize>(expected));

		if (m_download->progress->total_bytes != nullptr) {
			m_download->progress->total_bytes->store(static_cast<u64>(expected), std::memory_order_relaxed);
		}
	}

	t_completion(NSURLSessionResponseAllow);
}

- (void)URLSession:(NSURLSession*)t_session dataTask:(NSURLSessionDataTask*)t_task didReceiveData:(NSData*)t_data
{
	Download*        download = m_download;
	std::vector<u8>* body     = download->body;

	if (body->size() + t_data.length > download->max_bytes) {
		download->error = "the download grew far larger than any Pulsar build";
		[t_task cancel];
		return;
	}

	[t_data enumerateByteRangesUsingBlock:^(const void* t_bytes, NSRange t_range, BOOL*) {
		const auto* bytes = static_cast<const u8*>(t_bytes);
		body->insert(body->end(), bytes, bytes + t_range.length);
	}];

	const os::HttpProgress* progress = download->progress;
	if (progress->bytes_downloaded != nullptr) {
		progress->bytes_downloaded->store(body->size(), std::memory_order_relaxed);
	}

	const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - download->started;
	if (progress->bytes_per_second != nullptr && elapsed.count() > 0.0) {
		progress->bytes_per_second->store(static_cast<double>(body->size()) / elapsed.count(), std::memory_order_relaxed);
	}
}

- (void)URLSession:(NSURLSession*)t_session task:(NSURLSessionTask*)t_task didCompleteWithError:(NSError*)t_error
{
	if (t_error != nil && m_download->error.empty() && !m_download->cancelled.load(std::memory_order_relaxed)) {
		m_download->error = os::macos::to_utf8(t_error.localizedDescription);
	}

	dispatch_semaphore_signal(m_download->finished);
}

@end

namespace os {

auto http_get(std::string_view t_url, usize t_max_bytes, std::vector<u8>* t_out_body, const HttpProgress& t_progress, std::string* t_out_error) -> HttpResult
{
	@autoreleasepool {
		NSURL* url = [NSURL URLWithString:macos::to_ns_string(t_url)];
		if (url == nil) {
			*t_out_error = "could not parse the update URL";
			return HttpResult::FAILED;
		}

		Download download{
			.body      = t_out_body,
			.max_bytes = t_max_bytes,
			.progress  = &t_progress,
			.started   = std::chrono::steady_clock::now(),
			.finished  = dispatch_semaphore_create(0),
		};

		NSURLSessionConfiguration* configuration = NSURLSessionConfiguration.ephemeralSessionConfiguration;
		configuration.timeoutIntervalForRequest  = K_IDLE_TIMEOUT_SECONDS;
		configuration.HTTPAdditionalHeaders      = @{@"User-Agent" : @(K_USER_AGENT)};

		NSOperationQueue* delegate_queue           = [[NSOperationQueue alloc] init];
		delegate_queue.maxConcurrentOperationCount = 1;

		NSURLSession*         session = [NSURLSession sessionWithConfiguration:configuration
																	  delegate:[[PulsarDownloadDelegate alloc] initWithDownload:&download]
																 delegateQueue:delegate_queue];
		NSURLSessionDataTask* task    = [session dataTaskWithURL:url];
		[task resume];

		while (dispatch_semaphore_wait(download.finished, dispatch_time(DISPATCH_TIME_NOW, K_CANCEL_POLL_NANOSECONDS)) != 0) {
			const bool cancel_requested = t_progress.cancel_requested != nullptr && t_progress.cancel_requested->load(std::memory_order_relaxed);

			if (cancel_requested && !download.cancelled.exchange(true, std::memory_order_relaxed)) {
				[task cancel];
			}
		}

		[session finishTasksAndInvalidate];

		if (download.cancelled.load(std::memory_order_relaxed)) return HttpResult::CANCELLED;

		if (!download.error.empty()) {
			*t_out_error = download.error;
			return HttpResult::FAILED;
		}

		return HttpResult::OK;
	}
}

}
