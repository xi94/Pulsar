#include "os/http.h"

#include <chrono>

#include <Windows.h>
#include <winhttp.h>

#include "os/win32/win32.h"

namespace {
constexpr wchar_t K_USER_AGENT[] = L"Pulsar-Updater/1.0";

constexpr int K_RESOLVE_TIMEOUT_MS = 10000;
constexpr int K_CONNECT_TIMEOUT_MS = 10000;
constexpr int K_SEND_TIMEOUT_MS    = 15000;
constexpr int K_RECEIVE_TIMEOUT_MS = 15000;

class InternetHandle {
  public:
	explicit InternetHandle(HINTERNET t_handle)
		: m_handle(t_handle)
	{
	}

	~InternetHandle()
	{
		if (m_handle != nullptr) {
			WinHttpCloseHandle(m_handle);
		}
	}

	InternetHandle(const InternetHandle&)                    = delete;
	auto operator=(const InternetHandle&) -> InternetHandle& = delete;

	operator HINTERNET() const
	{
		return m_handle;
	}

  private:
	HINTERNET m_handle;
};

[[nodiscard]] auto open_request(HINTERNET t_connection, const std::wstring& t_path, bool t_https) -> HINTERNET
{
	const HINTERNET request =
		WinHttpOpenRequest(t_connection, L"GET", t_path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, t_https ? WINHTTP_FLAG_SECURE : 0);

	if (request != nullptr) {
		// The github.com to CDN redirect crosses hosts, which WinHTTP's default policy refuses.
		DWORD redirect_policy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
		WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &redirect_policy, sizeof(redirect_policy));
	}

	return request;
}

auto query_number_header(HINTERNET t_request, DWORD t_header, DWORD* t_out_value) -> bool
{
	DWORD size = sizeof(DWORD);

	return WinHttpQueryHeaders(t_request, WINHTTP_QUERY_FLAG_NUMBER | t_header, WINHTTP_HEADER_NAME_BY_INDEX, t_out_value, &size, WINHTTP_NO_HEADER_INDEX) != 0;
}

[[nodiscard]] auto read_body(HINTERNET t_request, usize t_max_bytes, std::vector<u8>* t_out_body, const os::HttpProgress& t_progress, std::string* t_out_error)
	-> os::HttpResult
{
	DWORD content_length = 0;
	if (query_number_header(t_request, WINHTTP_QUERY_CONTENT_LENGTH, &content_length)) {
		if (content_length > t_max_bytes) {
			*t_out_error = "the server offered a file far larger than any Pulsar build";
			return os::HttpResult::Failed;
		}

		t_out_body->reserve(content_length);

		if (t_progress.total_bytes != nullptr) {
			t_progress.total_bytes->store(content_length, std::memory_order_relaxed);
		}
	}

	const auto started = std::chrono::steady_clock::now();

	for (;;) {
		if (t_progress.cancel_requested != nullptr && t_progress.cancel_requested->load(std::memory_order_relaxed)) {
			return os::HttpResult::Cancelled;
		}

		DWORD available = 0;
		if (!WinHttpQueryDataAvailable(t_request, &available)) {
			*t_out_error = "the connection was interrupted while reading";
			return os::HttpResult::Failed;
		}

		if (available == 0) return os::HttpResult::Ok;

		if (t_out_body->size() + available > t_max_bytes) {
			*t_out_error = "the download grew far larger than any Pulsar build";
			return os::HttpResult::Failed;
		}

		const usize previous_size = t_out_body->size();
		t_out_body->resize(previous_size + available);

		DWORD read = 0;
		if (!WinHttpReadData(t_request, t_out_body->data() + previous_size, available, &read)) {
			*t_out_error = "the connection was interrupted while reading";
			return os::HttpResult::Failed;
		}

		t_out_body->resize(previous_size + read);

		if (t_progress.bytes_downloaded != nullptr) {
			t_progress.bytes_downloaded->store(t_out_body->size(), std::memory_order_relaxed);
		}

		const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - started;
		if (t_progress.bytes_per_second != nullptr && elapsed.count() > 0.0) {
			t_progress.bytes_per_second->store(static_cast<double>(t_out_body->size()) / elapsed.count(), std::memory_order_relaxed);
		}
	}
}
}

namespace os {

auto http_get(std::string_view t_url, usize t_max_bytes, std::vector<u8>* t_out_body, const HttpProgress& t_progress, std::string* t_out_error) -> HttpResult
{
	const std::wstring url_text = win32::to_wide(t_url);

	wchar_t host[256]{};
	wchar_t path[2048]{};
	wchar_t query[2048]{};

	URL_COMPONENTS url{
		.dwStructSize      = sizeof(URL_COMPONENTS),
		.lpszHostName      = host,
		.dwHostNameLength  = ARRAYSIZE(host),
		.lpszUrlPath       = path,
		.dwUrlPathLength   = ARRAYSIZE(path),
		.lpszExtraInfo     = query,
		.dwExtraInfoLength = ARRAYSIZE(query),
	};

	if (!WinHttpCrackUrl(url_text.c_str(), 0, 0, &url)) {
		*t_out_error = "could not parse the update URL";
		return HttpResult::Failed;
	}

	const InternetHandle session{WinHttpOpen(K_USER_AGENT, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)};
	if (session == nullptr) {
		*t_out_error = "could not open an HTTP session";
		return HttpResult::Failed;
	}

	WinHttpSetTimeouts(session, K_RESOLVE_TIMEOUT_MS, K_CONNECT_TIMEOUT_MS, K_SEND_TIMEOUT_MS, K_RECEIVE_TIMEOUT_MS);

	const InternetHandle connection{WinHttpConnect(session, host, url.nPort, 0)};
	if (connection == nullptr) {
		*t_out_error = "could not connect to " + win32::to_utf8(host);
		return HttpResult::Failed;
	}

	const InternetHandle request{open_request(connection, std::wstring{path} + query, url.nScheme == INTERNET_SCHEME_HTTPS)};
	if (request == nullptr) {
		*t_out_error = "could not open an HTTP request";
		return HttpResult::Failed;
	}

	if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
		*t_out_error = "the request failed to send";
		return HttpResult::Failed;
	}

	if (!WinHttpReceiveResponse(request, nullptr)) {
		*t_out_error = "no response was received";
		return HttpResult::Failed;
	}

	DWORD status = 0;
	query_number_header(request, WINHTTP_QUERY_STATUS_CODE, &status);
	if (status < 200 || status >= 300) {
		*t_out_error = "server returned HTTP " + std::to_string(status);
		return HttpResult::Failed;
	}

	return read_body(request, t_max_bytes, t_out_body, t_progress, t_out_error);
}

}
