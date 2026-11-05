#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

#include "login/riot_client.h"

enum class LoginStage : u8 {
	IDLE,
	WAITING_FOR_PROCESS,
	CONNECTING,
	AUTHENTICATING,
	LAUNCHING,
	SUCCESS,
	FAILED,
	CANCELLED,
};

constexpr usize K_LOGIN_MESSAGE_CAPACITY = 160;

struct LoginOptions {
	bool launch_game    = true;
	bool hide_client    = true;
	bool show_live_view = false;
};

struct LoginWork {
	std::atomic<LoginStage> stage{LoginStage::IDLE};
	std::atomic<bool>       cancel_requested{false};
	std::atomic<bool>       worker_finished{false};

	char         username[64]{};
	char         password[128]{};
	char         launch_product[32]{};
	char         message[K_LOGIN_MESSAGE_CAPACITY]{};
	LoginOptions options;

	std::string remembered_client_path;
	std::string found_client_path;
	bool        client_missing     = false;
	bool        permission_missing = false;

	RiotClient riot_client{&cancel_requested};
};

enum class LoginFailure : u8 {
	INVALID_CREDENTIALS,
	RIOT_BUSY,
	UNRECOGNIZED,
};

// The client's wording is Riot's to change, so this only looks for a few words, and anything else is shown in Riot's own words.
[[nodiscard]] auto classify_login_failure(std::string_view t_riot_error) -> LoginFailure;
[[nodiscard]] auto login_failure_message(std::string_view t_riot_error, char (&t_buffer)[K_LOGIN_MESSAGE_CAPACITY]) -> std::string_view;

// A hidden client goes back minimized only when the login finished with nothing left for the user to do in it.
[[nodiscard]] auto hand_back_after(LoginStage t_final_stage, std::string_view t_message) -> HandBack;

class LoginAttempt {
  public:
	LoginAttempt() = default;
	~LoginAttempt();

	LoginAttempt(const LoginAttempt&)                    = delete;
	auto operator=(const LoginAttempt&) -> LoginAttempt& = delete;

	auto start(std::string_view t_username, std::string_view t_password, std::string_view t_launch_product, std::string_view t_client_path, LoginOptions t_options) -> void;
	auto cancel() -> void;
	auto update() -> void;

	[[nodiscard]] auto stage() const -> LoginStage;
	[[nodiscard]] auto terminal_message() const -> std::string_view;
	[[nodiscard]] auto found_client_path() const -> std::string;
	[[nodiscard]] auto is_client_missing() const -> bool;
	[[nodiscard]] auto is_permission_missing() const -> bool;

	[[nodiscard]] static auto is_terminal(LoginStage t_stage) -> bool;

	bool m_active = false;

  private:
	auto abandon_worker() -> void;
	auto join_worker(std::chrono::milliseconds t_timeout) -> void;

	std::shared_ptr<LoginWork>            m_work;
	std::thread                           m_worker;
	std::chrono::steady_clock::time_point m_cancel_deadline;
};
