#include "login/win32/client_windows.h"

#include <algorithm>
#include <string_view>

namespace {
constexpr const wchar_t*    K_INTERFACE_PROCESS_NAMES[]{L"Riot Client.exe", L"RiotClientUx.exe"};
constexpr std::wstring_view K_INTERFACE_WINDOW_CLASS = L"Chrome_WidgetWin_1";

auto CALLBACK collect_client_window(HWND t_window, LPARAM t_windows) -> BOOL
{
	if (is_client_window(t_window)) {
		reinterpret_cast<std::vector<HWND>*>(t_windows)->push_back(t_window);
	}

	return TRUE;
}

[[nodiscard]] auto normal_area_of(HWND t_window) -> LONG
{
	WINDOWPLACEMENT placement{.length = sizeof(placement)};
	if (!GetWindowPlacement(t_window, &placement)) return 0;

	const RECT& normal = placement.rcNormalPosition;

	return (normal.right - normal.left) * (normal.bottom - normal.top);
}
}

auto executable_name_of(DWORD t_process_id) -> std::wstring
{
	const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, t_process_id);
	if (process == nullptr) return {};

	wchar_t    path[MAX_PATH * 2];
	DWORD      length  = ARRAYSIZE(path);
	const bool queried = QueryFullProcessImageNameW(process, 0, path, &length) != FALSE;
	CloseHandle(process);

	if (!queried) return {};

	const std::wstring_view full_path{path, length};

	return std::wstring{full_path.substr(full_path.find_last_of(L'\\') + 1)};
}

auto is_client_interface_process(DWORD t_process_id) -> bool
{
	const std::wstring executable = executable_name_of(t_process_id);

	return std::ranges::any_of(K_INTERFACE_PROCESS_NAMES, [&executable](const wchar_t* t_name) { return CompareStringOrdinal(executable.c_str(), -1, t_name, -1, TRUE) == CSTR_EQUAL; });
}

auto has_client_window_class(HWND t_window) -> bool
{
	wchar_t   class_name[64];
	const int class_length = GetClassNameW(t_window, class_name, ARRAYSIZE(class_name));

	return std::wstring_view{class_name, static_cast<usize>(std::max(class_length, 0))} == K_INTERFACE_WINDOW_CLASS;
}

auto is_client_window(HWND t_window) -> bool
{
	if (GetAncestor(t_window, GA_PARENT) != GetDesktopWindow() || !has_client_window_class(t_window)) return false;

	DWORD process_id = 0;
	GetWindowThreadProcessId(t_window, &process_id);

	return is_client_interface_process(process_id);
}

auto client_windows() -> std::vector<HWND>
{
	std::vector<HWND> windows;
	EnumWindows(collect_client_window, reinterpret_cast<LPARAM>(&windows));

	return windows;
}

auto main_client_window() -> HWND
{
	HWND largest      = nullptr;
	LONG largest_area = 0;

	for (const HWND window : client_windows()) {
		if (!IsWindowVisible(window) || GetWindow(window, GW_OWNER) != nullptr) continue;

		const LONG area = normal_area_of(window);
		if (area > largest_area) {
			largest      = window;
			largest_area = area;
		}
	}

	return largest;
}
