#pragma once

#include <optional>
#include <string_view>

#include "core/library.h"
#include "login/login_attempt.h"
#include "ui/commands.h"

struct Settings;

class LoginSession {
  public:
	static constexpr u32 K_STEP_COUNT = 4;

	LoginSession(Library* t_library, Settings* t_settings, CommandQueue* t_commands);

	auto request(u32 t_game, AccountRef t_account) -> void;
	auto retry() -> void;
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
	[[nodiscard]] auto step_progress() const -> float;
	[[nodiscard]] auto asks_for_permission() const -> bool;

	char                      m_status[160]{};
	char                      m_previous_status[160]{};
	float                     m_status_change    = 1.0f;
	float                     m_progress         = 0.0f;
	float                     m_outcome          = 0.0f;
	float                     m_seconds          = 0.0f;
	float                     m_finished_seconds = 0.0f;
	std::optional<AccountRef> m_shown_account;

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
	u32                         m_shown_game = 0;

	LoginStage m_progress_stage = LoginStage::IDLE;
	float      m_stage_seconds  = 0.0f;
};
