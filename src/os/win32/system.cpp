#include "os/system.h"

#include <vector>

#include "os/win32/win32.h"

namespace os {

auto local_time() -> LocalTime
{
	SYSTEMTIME time;
	GetLocalTime(&time);

	return LocalTime{.hour = time.wHour, .minute = time.wMinute, .second = time.wSecond, .millisecond = time.wMilliseconds};
}

auto current_thread_id() -> u64
{
	return GetCurrentThreadId();
}

auto write_to_debugger(const char* t_text) -> void
{
	OutputDebugStringA(t_text);
}

auto environment_variable(const char* t_name) -> std::optional<std::string>
{
	const std::wstring name   = win32::to_wide(t_name);
	const DWORD        needed = GetEnvironmentVariableW(name.c_str(), nullptr, 0);
	if (needed == 0) return std::nullopt;

	std::vector<wchar_t> value(needed);
	const DWORD          length = GetEnvironmentVariableW(name.c_str(), value.data(), needed);

	return win32::to_utf8(std::wstring_view{value.data(), length});
}

auto find_ignoring_case(std::string_view t_text, std::string_view t_query) -> usize
{
	const std::wstring text  = win32::to_wide(t_text);
	const std::wstring query = win32::to_wide(t_query);
	if (query.empty()) return 0;

	const int found = FindNLSStringEx(LOCALE_NAME_USER_DEFAULT, FIND_FROMSTART | LINGUISTIC_IGNORECASE, text.c_str(), static_cast<int>(text.size()),
	                                  query.c_str(), static_cast<int>(query.size()), nullptr, nullptr, nullptr, 0);
	if (found < 0) return std::string_view::npos;

	return win32::to_utf8(std::wstring_view{text}.substr(0, static_cast<usize>(found))).size();
}

}
