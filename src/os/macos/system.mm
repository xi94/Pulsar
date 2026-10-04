#include "os/system.h"

#include <cstdlib>
#include <ctime>

#include <os/log.h>
#include <pthread.h>

#include "core/app_identity.h"
#include "os/macos/macos.h"

namespace os {

auto local_time() -> LocalTime
{
	timespec now{};
	clock_gettime(CLOCK_REALTIME, &now);

	tm local{};
	localtime_r(&now.tv_sec, &local);

	return LocalTime{
		.hour        = static_cast<u32>(local.tm_hour),
		.minute      = static_cast<u32>(local.tm_min),
		.second      = static_cast<u32>(local.tm_sec),
		.millisecond = static_cast<u32>(now.tv_nsec / 1000000),
	};
}

auto current_thread_id() -> u64
{
	u64 id = 0;
	pthread_threadid_np(nullptr, &id);

	return id;
}

auto write_to_debugger(const char* t_text) -> void
{
	static const os_log_t LOG = os_log_create(PULSAR_BUNDLE_IDENTIFIER, "debug");

	os_log(LOG, "%{public}s", t_text);
}

auto environment_variable(const char* t_name) -> std::optional<std::string>
{
	const char* value = std::getenv(t_name);
	if (value == nullptr) return std::nullopt;

	return std::string{value};
}

auto find_ignoring_case(std::string_view t_text, std::string_view t_query) -> usize
{
	if (t_query.empty()) return 0;

	NSString*     text  = macos::to_ns_string(t_text);
	NSString*     query = macos::to_ns_string(t_query);
	const NSRange found = [text rangeOfString:query options:NSCaseInsensitiveSearch range:NSMakeRange(0, text.length) locale:NSLocale.currentLocale];
	if (found.location == NSNotFound) return std::string_view::npos;

	return macos::to_utf8([text substringToIndex:found.location]).size();
}

}
