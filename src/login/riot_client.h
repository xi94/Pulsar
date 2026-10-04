#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <string_view>

#include "core/types.h"

enum class PlayResult : u8 {
	Clicked,
	NotFound,
	LoginError,
};

class RiotClient {
  public:
	explicit RiotClient(const std::atomic<bool>* t_cancel);
	~RiotClient();

	RiotClient(const RiotClient&)                    = delete;
	auto operator=(const RiotClient&) -> RiotClient& = delete;

	static auto prepare_automation() -> void;
	[[nodiscard]] static auto is_game_in_progress() -> bool;
	static auto kill_all_client_processes(const std::atomic<bool>* t_cancel) -> void;
	[[nodiscard]] static auto automation_failure_message() -> const char*;
	[[nodiscard]] static auto supports_product(std::string_view t_launch_product) -> bool;

	[[nodiscard]] static auto executable_name() -> const char*;
	[[nodiscard]] static auto default_install_folder() -> std::string;
	[[nodiscard]] static auto executable_near(std::string_view t_chosen_path) -> std::string;

	[[nodiscard]] auto resolve_executable_path(std::string_view t_remembered_path) -> bool;
	[[nodiscard]] auto launch(std::string_view t_launch_product) -> bool;

	[[nodiscard]] auto executable_path() const -> const std::string&
	{
		return m_executable_path;
	}

	auto wait_for_responsive_window() -> void;
	auto bring_to_foreground() -> bool;
	auto take_keyboard_focus() -> bool;

	[[nodiscard]] auto start_automation() -> bool;
	auto stop_automation() -> void;

	[[nodiscard]] auto submit_login(std::string_view t_username, std::string_view t_password) -> bool;
	[[nodiscard]] auto wait_for_login_result(std::string* t_out_error, const std::string* t_error_to_ignore) -> bool;
	[[nodiscard]] auto click_play_when_ready(u32 t_timeout_ms, std::string* t_out_error) -> PlayResult;

  private:
	struct Native;

	const std::atomic<bool>* m_cancel;
	std::string              m_executable_path;
	std::unique_ptr<Native>  m_native;
};
