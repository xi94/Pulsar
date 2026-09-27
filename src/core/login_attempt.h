#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <string_view>
#include <thread>

#include "core/riot_client.h"

enum class LoginStage : u8 {
	idle,
	waiting_for_process,
	connecting,
	authenticating,
	launching,
	success,
	error,
	cancelled,
};

struct LoginWork {
	std::atomic<LoginStage> stage{LoginStage::idle};
	std::atomic<bool> cancel_requested{false};
	std::atomic<bool> worker_finished{false};

	char username[64]{};
	char password[128]{};
	char game_title[32]{};
	char message[160]{};

	RiotClient riot_client;
};

class LoginAttempt {
  public:
	LoginAttempt() = default;
	~LoginAttempt();

	LoginAttempt(const LoginAttempt &) = delete;
	LoginAttempt &operator=(const LoginAttempt &) = delete;

	void start(std::string_view t_username, std::string_view t_password, std::string_view t_game_title);
	void cancel();
	void update();

	bool is_active() const
	{
		return m_active;
	}

	LoginStage stage() const;
	std::string_view terminal_message() const;

	static bool is_terminal(LoginStage t_stage);

  private:
	void abandon_worker();

	std::shared_ptr<LoginWork> m_work;
	std::thread m_worker;
	bool m_active = false;
	std::chrono::steady_clock::time_point m_cancel_deadline{};
};
