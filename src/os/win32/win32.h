#pragma once

#include <string>
#include <string_view>

#include <Windows.h>

#include "core/app_identity.h"
#include "core/types.h"
#include "os/input.h"

namespace os {
class Window;
}

namespace os::win32 {

constexpr const wchar_t* K_APP_NAME_WIDE                  = L"" PULSAR_APP_NAME;
constexpr const wchar_t* K_MAIN_WINDOW_CLASS_NAME         = L"PulsarDesktopAppWindow";
constexpr const wchar_t* K_SETUP_WINDOW_CLASS_NAME        = L"PulsarDesktopAppSetup";
constexpr const wchar_t* K_QUIT_INSTANCE_MESSAGE_NAME     = L"PulsarDesktopAppQuitForSetup";
constexpr const wchar_t* K_TRAY_WINDOW_CLASS_NAME         = L"PulsarDesktopAppTray";
constexpr const wchar_t* K_TRAY_MENU_CLASS_NAME           = L"PulsarDesktopAppTrayMenu";
constexpr const wchar_t* K_SINGLE_INSTANCE_MUTEX_NAME     = L"Local\\Pulsar.DesktopApp.SingleInstance";
constexpr const wchar_t* K_ACTIVATE_INSTANCE_MESSAGE_NAME = L"PulsarDesktopAppActivateExistingInstance";
constexpr const wchar_t* K_APP_USER_MODEL_ID              = L"Pulsar.DesktopApp.AccountManager";

enum class AppIconSize : u8 {
	SMALL_ICON,
	LARGE_ICON,
};

class ComScope {
  public:
	ComScope();
	~ComScope();

	ComScope(const ComScope&)                    = delete;
	auto operator=(const ComScope&) -> ComScope& = delete;

  private:
	bool m_initialized;
};

[[nodiscard]] auto to_wide(std::string_view t_utf8) -> std::wstring;
[[nodiscard]] auto to_utf8(std::wstring_view t_wide) -> std::string;

[[nodiscard]] auto window_handle(const Window* t_window) -> HWND;
[[nodiscard]] auto key_from_virtual_key(WPARAM t_virtual_key) -> Key;

[[nodiscard]] auto load_app_icon(AppIconSize t_size) -> HICON;
[[nodiscard]] auto app_icon_pixel_size(AppIconSize t_size) -> int;

}
