#pragma once

#include <atomic>
#include <chrono>
#include <string>
#include <string_view>

#include <Windows.h>

#include "core/ui_automation.h"

class RiotClient {
  public:
	static constexpr u32 wait_forever_ms = 0xFFFFFFFFu;

	RiotClient() = default;
	~RiotClient();

	RiotClient(const RiotClient &) = delete;
	RiotClient &operator=(const RiotClient &) = delete;

	static bool is_game_in_progress();
	static void kill_all_client_processes();
	static std::string_view launch_product_for(std::string_view t_game_title);

	bool resolve_executable_path();
	bool launch(std::string_view t_launch_product);

	bool wait_for_window(u32 t_timeout_ms, const std::atomic<bool> &t_cancel) const;
	bool bring_to_foreground(u32 t_timeout_ms, const std::atomic<bool> &t_cancel) const;
	bool take_keyboard_focus(u32 t_timeout_ms, const std::atomic<bool> &t_cancel) const;

	bool submit_login(const UiAutomation &t_automation, std::string_view t_username, std::string_view t_password,
					  u32 t_timeout_ms, const std::atomic<bool> &t_cancel) const;

	bool wait_for_login_error(const UiAutomation &t_automation, std::wstring &t_out_message, u32 t_timeout_ms,
							  const std::atomic<bool> &t_cancel, const std::wstring *t_error_to_ignore) const;

	bool click_play_when_ready(const UiAutomation &t_automation, u32 t_timeout_ms,
							   const std::atomic<bool> &t_cancel) const;

  private:
	HWND find_client_window() const;
	HWND wait_for_responsive_window(u32 t_timeout_ms, const std::atomic<bool> &t_cancel) const;
	UiElement current_window_element(const UiAutomation &t_automation) const;

	mutable HWND m_cached_window = nullptr;
	mutable UiElement m_cached_window_element;
	mutable std::chrono::steady_clock::time_point m_cached_window_expiry{};

	std::wstring m_executable_path;
	HANDLE m_process = nullptr;
	u32 m_process_id = 0;
};
