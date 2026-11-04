#pragma once

#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/types.h"
#include "login/client_watch.h"

enum class SubmitWith : u8 {
	SIGN_IN_BUTTON_OR_ENTER,
	ENTER,
};

class RiotClient {
  public:
	explicit RiotClient(const std::atomic<bool>* t_cancel);
	~RiotClient();

	RiotClient(const RiotClient&)                    = delete;
	auto operator=(const RiotClient&) -> RiotClient& = delete;

	static auto prepare_automation() -> void;
	[[nodiscard]] static auto running_game() -> std::optional<std::string_view>;
	static auto kill_all_client_processes(const std::atomic<bool>* t_cancel) -> void;
	[[nodiscard]] static auto automation_failure_message() -> const char*;
	[[nodiscard]] static auto request_automation_permission() -> bool;
	[[nodiscard]] static auto permission_missing_message() -> const char*;
	static auto open_automation_permission_settings() -> void;
	[[nodiscard]] static auto supports_product(std::string_view t_launch_product) -> bool;
	[[nodiscard]] static auto forwards_launch_to_open_client() -> bool;

	[[nodiscard]] static auto paths_in_installs_file(std::string_view t_json) -> std::vector<std::string>;
	[[nodiscard]] static auto without_stored_sign_in(std::string_view t_private_settings) -> std::string;
	[[nodiscard]] static auto forget_stored_sign_in() -> bool;
	[[nodiscard]] static auto executable_name() -> const char*;
	[[nodiscard]] static auto default_install_folder() -> std::string;
	[[nodiscard]] static auto executable_near(std::string_view t_chosen_path) -> std::string;

	[[nodiscard]] auto resolve_executable_path(std::string_view t_remembered_path) -> bool;
	[[nodiscard]] auto launch(std::string_view t_launch_product) -> bool;
	[[nodiscard]] auto has_open_window() const -> bool;
	auto bring_to_front() -> void;

	[[nodiscard]] auto start_automation() -> bool;
	auto stop_automation() -> void;

	[[nodiscard]] auto look() -> ClientView;
	[[nodiscard]] auto fill_and_submit(std::string_view t_username, std::string_view t_password, SubmitWith t_submit) -> bool;
	auto press_play() -> bool;

	std::string m_executable_path;

  private:
	struct Native;

	const std::atomic<bool>* m_cancel;
	std::unique_ptr<Native>  m_native;
};
