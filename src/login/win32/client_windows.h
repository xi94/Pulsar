#pragma once

#include <string>
#include <vector>

#include <Windows.h>

[[nodiscard]] auto executable_name_of(DWORD t_process_id) -> std::wstring;
[[nodiscard]] auto is_client_interface_process(DWORD t_process_id) -> bool;
[[nodiscard]] auto has_client_window_class(HWND t_window) -> bool;
[[nodiscard]] auto is_client_window(HWND t_window) -> bool;
[[nodiscard]] auto client_windows() -> std::vector<HWND>;
[[nodiscard]] auto main_client_window() -> HWND;
