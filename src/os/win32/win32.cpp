#include "os/win32/win32.h"

#include <Windows.h>
#include <objbase.h>

#include "os/window.h"

namespace os::win32 {

ComScope::ComScope()
	: m_initialized(SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)))
{
}

ComScope::~ComScope()
{
	if (m_initialized) {
		CoUninitialize();
	}
}

auto to_wide(std::string_view t_utf8) -> std::wstring
{
	const auto utf8_length = static_cast<int>(t_utf8.size());
	const int  length      = MultiByteToWideChar(CP_UTF8, 0, t_utf8.data(), utf8_length, nullptr, 0);
	if (length <= 0) return {};

	std::wstring wide(static_cast<usize>(length), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, t_utf8.data(), utf8_length, wide.data(), length);

	return wide;
}

auto to_utf8(std::wstring_view t_wide) -> std::string
{
	const auto wide_length = static_cast<int>(t_wide.size());
	const int  length      = WideCharToMultiByte(CP_UTF8, 0, t_wide.data(), wide_length, nullptr, 0, nullptr, nullptr);
	if (length <= 0) return {};

	std::string utf8(static_cast<usize>(length), '\0');
	WideCharToMultiByte(CP_UTF8, 0, t_wide.data(), wide_length, utf8.data(), length, nullptr, nullptr);

	return utf8;
}

auto window_handle(const Window* t_window) -> HWND
{
	return static_cast<HWND>(t_window->native_handle());
}

}
