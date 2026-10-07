#pragma once

#include <optional>
#include <string_view>

#include "core/library.h"
#include "login/login_attempt.h"
#include "ui/commands.h"

struct Settings;

// The one login that can run at a time. The account modal and the library both start logins here and draw its eased progress.
class LoginSession {
  public:
	static constexpr u32 K_STEP_COUNT = 4;

	LoginSession(Library* t_library, Settings* t_settings, CommandQueue* t_commands);

	auto request(u32 t_game, AccountRef t_account) -> void;
	auto cancel() -> void;
	auto dismiss() -> void;
	auto update(float t_delta_seconds) -> void;

	auto follow_insert(AccountRef t_inserted) -> void;
	auto follow_removal(AccountRef t_removed) -> void;

	[[nodiscard]] auto is_busy() const -> bool;
	[[nodiscard]] auto is_finished() const -> bool;
	[[nodiscard]] auto shows(u32 t_game, AccountRef t_account) const -> bool;
	[[nodiscard]] auto stage() const -> LoginStage;
	[[nodiscard]] auto step() const -> u32;
	[[nodiscard]] auto asks_for_permission() const -> bool;

	[[nodiscard]] auto status() const -> std::string_view
	{
		return m_status_to;
	}

	[[nodiscard]] auto previous_status() const -> std::string_view
	{
		return m_status_from;
	}

	[[nodiscard]] auto status_change() const -> float
	{
		return m_status_change;
	}

	[[nodiscard]] auto progress() const -> float
	{
		return m_progress;
	}

	[[nodiscard]] auto outcome() const -> float
	{
		return m_outcome;
	}

	[[nodiscard]] auto seconds() const -> float
	{
		return m_seconds;
	}

	[[nodiscard]] auto finished_seconds() const -> float
	{
		return m_finished_seconds;
	}

  private:
	struct PendingLogin {
		u32        game;
		AccountRef account;
	};

	auto start(PendingLogin t_login) -> void;
	auto record_result() -> void;
	auto ease_progress(float t_delta_seconds) -> void;
	[[nodiscard]] auto current_status() const -> std::string_view;

	Library*      m_library;
	Settings*     m_settings;
	CommandQueue* m_commands;

	LoginAttempt                m_login;
	std::optional<PendingLogin> m_queued;
	std::optional<AccountRef>   m_login_account;
	u32                         m_login_game = 0;
	std::optional<AccountRef>   m_shown;
	u32                         m_shown_game = 0;

	LoginStage m_progress_stage   = LoginStage::IDLE;
	float      m_seconds          = 0.0f;
	float      m_stage_seconds    = 0.0f;
	float      m_progress         = 0.0f;
	float      m_outcome          = 0.0f;
	float      m_status_change    = 1.0f;
	float      m_finished_seconds = 0.0f;
	char       m_status_from[160]{};
	char       m_status_to[160]{};
};
