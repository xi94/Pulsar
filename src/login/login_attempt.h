#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

#include "login/riot_client.h"

enum class LoginStage : u8 {
	Idle,
	WaitingForProcess,
	Connecting,
	Authenticating,
	Launching,
	Success,
	Error,
	Cancelled,
};

struct LoginWork {
	std::atomic<LoginStage> stage{LoginStage::Idle};
	std::atomic<bool>       cancel_requested{false};
	std::atomic<bool>       worker_finished{false};

	char username[64]{};
	char password[128]{};
	char launch_product[32]{};
	char message[160]{};

	std::string remembered_client_path;
	std::string found_client_path;
	bool        client_missing = false;

	RiotClient riot_client{&cancel_requested};
};

class LoginAttempt {
  public:
	LoginAttempt() = default;
	~LoginAttempt();

	LoginAttempt(const LoginAttempt&)                    = delete;
	auto operator=(const LoginAttempt&) -> LoginAttempt& = delete;

	auto start(std::string_view t_username, std::string_view t_password, std::string_view t_launch_product, std::string_view t_client_path) -> void;
	auto cancel() -> void;
	auto update() -> void;

	[[nodiscard]] auto is_active() const -> bool
	{
		return m_active;
	}

	[[nodiscard]] auto stage() const -> LoginStage;
	[[nodiscard]] auto terminal_message() const -> std::string_view;
	[[nodiscard]] auto found_client_path() const -> std::string;
	[[nodiscard]] auto is_client_missing() const -> bool;

	[[nodiscard]] static auto is_terminal(LoginStage t_stage) -> bool;

  private:
	auto abandon_worker() -> void;
	auto join_worker(std::chrono::milliseconds t_timeout) -> void;

	std::shared_ptr<LoginWork>            m_work;
	std::thread                           m_worker;
	bool                                  m_active = false;
	std::chrono::steady_clock::time_point m_cancel_deadline;
};
