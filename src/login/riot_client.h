#pragma once

#include <atomic>
#include <chrono>
#include <string>
#include <string_view>

#include <Windows.h>

#include "login/ui_automation.h"

enum class PlayResult : u8 {
	Clicked,
	NotFound,
	LoginError,
};

class RiotClient {
  public:
	RiotClient() = default;
	~RiotClient();

	RiotClient(const RiotClient&)                    = delete;
	auto operator=(const RiotClient&) -> RiotClient& = delete;

	[[nodiscard]] static auto is_game_in_progress() -> bool;
	static auto kill_all_client_processes(const std::atomic<bool>* t_cancel) -> void;

	[[nodiscard]] static auto executable_near(const std::wstring& t_chosen_path) -> std::wstring;

	[[nodiscard]] auto resolve_executable_path(const std::wstring& t_remembered_path) -> bool;
	[[nodiscard]] auto launch(std::string_view t_launch_product) -> bool;

	[[nodiscard]] auto executable_path() const -> const std::wstring&
	{
		return m_executable_path;
	}

	auto wait_for_responsive_window(const std::atomic<bool>* t_cancel) const -> HWND;
	auto bring_to_foreground(const std::atomic<bool>* t_cancel) const -> bool;
	auto take_keyboard_focus(const std::atomic<bool>* t_cancel) const -> bool;

	[[nodiscard]] auto
	submit_login(const UiAutomation& t_automation, std::string_view t_username, std::string_view t_password, const std::atomic<bool>* t_cancel) const -> bool;

	[[nodiscard]] auto wait_for_login_result(const UiAutomation&      t_automation,
	                                         std::wstring*            t_out_error,
	                                         const std::atomic<bool>* t_cancel,
	                                         const std::wstring*      t_error_to_ignore) const -> bool;

	[[nodiscard]] auto
	click_play_when_ready(const UiAutomation& t_automation, u32 t_timeout_ms, const std::atomic<bool>* t_cancel, std::wstring* t_out_error) const -> PlayResult;

  private:
	[[nodiscard]] auto find_client_window() const -> HWND;
	[[nodiscard]] auto current_window_element(const UiAutomation& t_automation) const -> UiElement;

	mutable HWND                                  m_cached_window = nullptr;
	mutable UiElement                             m_cached_window_element;
	mutable std::chrono::steady_clock::time_point m_cached_window_expiry;

	mutable bool m_form_found_by_name = true;

	std::wstring m_executable_path;
	HANDLE       m_process    = nullptr;
	u32          m_process_id = 0;
};
