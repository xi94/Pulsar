#include "ui/login_session.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <string>
#include <utility>

#include "core/animation.h"
#include "core/settings.h"
#include "core/str.h"

namespace {
constexpr float K_PROGRESS_EASE_RATE     = 5.0f;
constexpr float K_PROGRESS_CREEP_SECONDS = 2.5f;
constexpr float K_STATUS_EASE_RATE       = 10.0f;
constexpr float K_OUTCOME_EASE_RATE      = 8.0f;

struct StageSpan {
	float start;
	float end;
};

[[nodiscard]] auto stage_span(LoginStage t_stage) -> std::optional<StageSpan>
{
	switch (t_stage) {
		using enum LoginStage;

		case IDLE: {
			return StageSpan{0.02f, 0.1f};
		}

		case WAITING_FOR_PROCESS: {
			return StageSpan{0.08f, 0.3f};
		}

		case CONNECTING: {
			return StageSpan{0.32f, 0.55f};
		}

		case AUTHENTICATING: {
			return StageSpan{0.58f, 0.82f};
		}

		case LAUNCHING: {
			return StageSpan{0.85f, 0.97f};
		}

		case SUCCESS: {
			return StageSpan{1.0f, 1.0f};
		}

		case FAILED:
		case CANCELLED: {
			break;
		}
	}

	return std::nullopt;
}

[[nodiscard]] auto stage_step(LoginStage t_stage) -> u32
{
	switch (t_stage) {
		using enum LoginStage;

		case WAITING_FOR_PROCESS: {
			return 1;
		}

		case CONNECTING: {
			return 2;
		}

		case AUTHENTICATING: {
			return 3;
		}

		case LAUNCHING: {
			return 4;
		}

		case IDLE:
		case SUCCESS:
		case FAILED:
		case CANCELLED: {
			break;
		}
	}

	return 0;
}

[[nodiscard]] auto stage_message(LoginStage t_stage) -> std::string_view
{
	switch (t_stage) {
		using enum LoginStage;

		case IDLE: {
			return "";
		}

		case WAITING_FOR_PROCESS: {
			return "Launching Riot Client...";
		}

		case CONNECTING: {
			return "Waiting for Riot Client...";
		}

		case AUTHENTICATING: {
			return "Logging in...";
		}

		case LAUNCHING: {
			return "Launching game...";
		}

		case SUCCESS: {
			return "Logged in!";
		}

		case FAILED: {
			return "Something went wrong.";
		}

		case CANCELLED: {
			return "Cancelled.";
		}
	}

	return "";
}
}

LoginSession::LoginSession(Library* t_library, Settings* t_settings, CommandQueue* t_commands)
	: m_library(t_library)
	, m_settings(t_settings)
	, m_commands(t_commands)
{
}

auto LoginSession::request(u32 t_game, AccountRef t_account) -> void
{
	m_shown            = t_account;
	m_shown_game       = t_game;
	m_progress_stage   = LoginStage::IDLE;
	m_seconds          = 0.0f;
	m_stage_seconds    = 0.0f;
	m_progress         = 0.0f;
	m_outcome          = 0.0f;
	m_status_change    = 1.0f;
	m_finished_seconds = 0.0f;
	m_status_from[0]   = '\0';
	m_status_to[0]     = '\0';

	const PendingLogin login{t_game, t_account};

	if (m_login.is_active() && !LoginAttempt::is_terminal(m_login.stage())) {
		m_login.cancel();
		m_login_account.reset();
		m_queued = login;
		return;
	}

	m_queued.reset();
	start(login);
}

auto LoginSession::start(PendingLogin t_login) -> void
{
	if (t_login.game >= m_library->game_count) return;
	if (t_login.account.game >= m_library->game_count || t_login.account.index >= m_library->games[t_login.account.game].account_count) return;

	const Account* account = m_library->account(t_login.account);
	m_login_account        = t_login.account;
	m_login_game           = t_login.game;
	m_login.start(account->username, account->password, m_library->games[t_login.game].launch_product, m_settings->riot_client_path);
}

auto LoginSession::cancel() -> void
{
	m_queued.reset();
	m_login_account.reset();
	m_shown.reset();
	m_login.cancel();
}

auto LoginSession::dismiss() -> void
{
	if (is_finished()) {
		m_shown.reset();
	}
}

auto LoginSession::record_result() -> void
{
	if (!m_login_account || !LoginAttempt::is_terminal(m_login.stage())) return;

	if (m_login.stage() == LoginStage::SUCCESS) {
		m_library->account(*m_login_account)->last_used = std::time(nullptr);
		m_commands->push(Command{.type = CommandType::SAVE_CHANGES});
	}

	if (const std::string found = m_login.found_client_path(); !found.empty()) {
		copy_to(found, m_settings->riot_client_path);
		m_commands->push(Command{.type = CommandType::SAVE_CHANGES});
	}

	if (m_login.is_client_missing()) {
		m_commands->push(Command{.type = CommandType::LOCATE_RIOT_CLIENT, .index = static_cast<i32>(m_login_game), .account = *m_login_account});
	}

	m_login_account.reset();
}

auto LoginSession::update(float t_delta_seconds) -> void
{
	m_login.update();
	record_result();

	if (m_queued && !m_login.is_active()) {
		const PendingLogin login = *std::exchange(m_queued, std::nullopt);

		m_seconds = 0.0f;
		start(login);
	}

	if (!m_shown) return;

	m_seconds += t_delta_seconds;
	ease_progress(t_delta_seconds);

	if (is_finished()) {
		m_finished_seconds += t_delta_seconds;
	} else {
		m_finished_seconds = 0.0f;
		animation::request_frame();
	}
}

auto LoginSession::ease_progress(float t_delta_seconds) -> void
{
	const LoginStage shown_stage = stage();

	if (shown_stage != m_progress_stage) {
		m_progress_stage = shown_stage;
		m_stage_seconds  = 0.0f;
	}

	m_stage_seconds += t_delta_seconds;

	const std::string_view current = current_status();
	if (current != std::string_view{m_status_to}) {
		copy_to(m_status_to, m_status_from);
		copy_to(current, m_status_to);
		m_status_change = 0.0f;
	}

	if (const std::optional<StageSpan> span = stage_span(shown_stage)) {
		const float creep  = 1.0f - std::exp(-m_stage_seconds / K_PROGRESS_CREEP_SECONDS);
		const float target = span->start + (span->end - span->start) * creep;
		m_progress         = animation::ease_toward(m_progress, std::max(target, m_progress), K_PROGRESS_EASE_RATE, t_delta_seconds);
	}

	m_status_change = animation::ease_toward(m_status_change, 1.0f, K_STATUS_EASE_RATE, t_delta_seconds);
	m_outcome       = animation::ease_toward(m_outcome, is_finished() ? 1.0f : 0.0f, K_OUTCOME_EASE_RATE, t_delta_seconds);
}

auto LoginSession::follow_insert(AccountRef t_inserted) -> void
{
	shift_after_insert(&m_login_account, t_inserted);
	shift_after_insert(&m_shown, t_inserted);

	if (m_queued) {
		std::optional<AccountRef> queued = m_queued->account;
		shift_after_insert(&queued, t_inserted);
		m_queued->account = *queued;
	}
}

auto LoginSession::follow_removal(AccountRef t_removed) -> void
{
	shift_after_removal(&m_login_account, t_removed);
	shift_after_removal(&m_shown, t_removed);

	if (m_queued) {
		std::optional<AccountRef> queued = m_queued->account;
		shift_after_removal(&queued, t_removed);

		if (queued) {
			m_queued->account = *queued;
		} else {
			m_queued.reset();
		}
	}
}

auto LoginSession::is_busy() const -> bool
{
	return m_queued.has_value() || m_login.is_active();
}

auto LoginSession::is_finished() const -> bool
{
	return m_shown.has_value() && !m_queued && LoginAttempt::is_terminal(m_login.stage());
}

auto LoginSession::shows(u32 t_game, AccountRef t_account) const -> bool
{
	return m_shown == t_account && m_shown_game == t_game;
}

auto LoginSession::stage() const -> LoginStage
{
	return m_queued ? LoginStage::IDLE : m_login.stage();
}

auto LoginSession::step() const -> u32
{
	return m_queued ? 0 : stage_step(m_login.stage());
}

auto LoginSession::asks_for_permission() const -> bool
{
	return !m_queued && m_login.is_permission_missing();
}

auto LoginSession::current_status() const -> std::string_view
{
	if (m_queued) return "Switching account...";

	const LoginStage current = m_login.stage();
	if (LoginAttempt::is_terminal(current) && !m_login.terminal_message().empty()) return m_login.terminal_message();

	return stage_message(current);
}
