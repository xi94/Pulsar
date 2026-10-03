#include "login/login_attempt.h"

#include <string>
#include <utility>

#include <sodium.h>

#include "core/debug_log.h"
#include "core/str.h"
#include "core/thread_util.h"

namespace {
constexpr const char* K_LOG_CATEGORY = "login";

constexpr u32  K_PLAY_BUTTON_TIMEOUT_MS = 12000;
constexpr auto K_CANCEL_GRACE_PERIOD    = std::chrono::milliseconds(5000);
constexpr auto K_SHUTDOWN_JOIN_TIMEOUT  = std::chrono::milliseconds(3000);
constexpr auto K_FINISHED_JOIN_TIMEOUT  = std::chrono::milliseconds(50);

constexpr const char* K_INVALID_CREDENTIALS_MESSAGE = "Invalid username or password.";
constexpr const char* K_SERVER_ERROR_MESSAGE        = "Something went wrong - Riot's servers might be overloaded. Try again in a moment.";
constexpr const char* K_UNRECOGNIZED_ERROR_MESSAGE  = "The Riot Client didn't sign in - check the username and password and try again.";
constexpr const char* K_GAME_IN_PROGRESS_MESSAGE    = "A game is already running - close it before switching accounts.";
constexpr const char* K_NO_RIOT_CLIENT_MESSAGE      = "Couldn't find the Riot Client - set its location in Settings.";
constexpr const char* K_LAUNCH_FAILED_MESSAGE       = "Couldn't launch the Riot Client.";
constexpr const char* K_UNRESPONSIVE_CLIENT_MESSAGE = "The Riot Client stopped responding - try again.";
constexpr const char* K_AUTOMATION_FAILED_MESSAGE   = "Couldn't start Windows UI Automation - try again.";

[[nodiscard]] auto stage_name(LoginStage t_stage) -> const char*
{
	switch (t_stage) {
		case LoginStage::Idle:
			return "IDLE";
		case LoginStage::WaitingForProcess:
			return "WAITING_FOR_PROCESS";
		case LoginStage::Connecting:
			return "CONNECTING";
		case LoginStage::Authenticating:
			return "AUTHENTICATING";
		case LoginStage::Launching:
			return "LAUNCHING";
		case LoginStage::Success:
			return "SUCCESS";
		case LoginStage::Error:
			return "ERROR";
		case LoginStage::Cancelled:
			return "CANCELLED";
	}

	return "?";
}

auto set_stage(LoginWork* t_work, LoginStage t_stage) -> void
{
	debug_log::write(K_LOG_CATEGORY, "stage -> %s%s", stage_name(t_stage), t_work->message[0] != '\0' ? " (with a message)" : "");
	t_work->stage.store(t_stage, std::memory_order_release);
}

auto fail(LoginWork* t_work, const char* t_message) -> void
{
	copy_to(t_message, t_work->message);
	set_stage(t_work, LoginStage::Error);
}

[[nodiscard]] auto stop_if_cancelled(LoginWork* t_work) -> bool
{
	if (!t_work->cancel_requested.load(std::memory_order_relaxed)) return false;

	set_stage(t_work, LoginStage::Cancelled);

	return true;
}

[[nodiscard]] auto is_invalid_credentials(const std::string& t_error) -> bool
{
	return t_error.find("credentials") != std::string::npos;
}

[[nodiscard]] auto failure_message(const std::string& t_error) -> const char*
{
	if (t_error.empty()) return K_UNRECOGNIZED_ERROR_MESSAGE;

	return is_invalid_credentials(t_error) ? K_INVALID_CREDENTIALS_MESSAGE : K_SERVER_ERROR_MESSAGE;
}

[[nodiscard]] auto submit_and_wait_for_error(LoginWork* t_work, std::string* t_out_error, const std::string* t_error_to_ignore = nullptr) -> bool
{
	t_out_error->clear();

	return t_work->riot_client.submit_login(t_work->username, t_work->password) && t_work->riot_client.wait_for_login_result(t_out_error, t_error_to_ignore);
}

[[nodiscard]] auto start_fresh_client(LoginWork* t_work) -> bool
{
	if (RiotClient::is_game_in_progress()) {
		fail(t_work, K_GAME_IN_PROGRESS_MESSAGE);
		return false;
	}

	if (stop_if_cancelled(t_work)) return false;

	if (!t_work->riot_client.resolve_executable_path(t_work->remembered_client_path)) {
		t_work->client_missing = true;
		fail(t_work, K_NO_RIOT_CLIENT_MESSAGE);
		return false;
	}

	if (t_work->riot_client.executable_path() != t_work->remembered_client_path) {
		t_work->found_client_path = t_work->riot_client.executable_path();
	}

	RiotClient::kill_all_client_processes(&t_work->cancel_requested);

	if (stop_if_cancelled(t_work)) return false;

	if (!t_work->riot_client.launch(t_work->launch_product)) {
		fail(t_work, K_LAUNCH_FAILED_MESSAGE);
		return false;
	}

	t_work->riot_client.wait_for_responsive_window();

	return !stop_if_cancelled(t_work);
}

[[nodiscard]] auto focus_client(LoginWork* t_work) -> bool
{
	set_stage(t_work, LoginStage::Connecting);

	t_work->riot_client.bring_to_foreground();
	t_work->riot_client.take_keyboard_focus();

	return !stop_if_cancelled(t_work);
}

[[nodiscard]] auto authenticate(LoginWork* t_work) -> bool
{
	set_stage(t_work, LoginStage::Authenticating);

	std::string error;
	bool        error_shown = submit_and_wait_for_error(t_work, &error);

	if (stop_if_cancelled(t_work)) return false;

	if (error_shown && !error.empty() && !is_invalid_credentials(error)) {
		if (!focus_client(t_work)) return false;

		set_stage(t_work, LoginStage::Authenticating);

		const std::string previous_error = error;
		error_shown                      = submit_and_wait_for_error(t_work, &error, &previous_error);

		if (stop_if_cancelled(t_work)) return false;
	}

	if (error_shown) {
		fail(t_work, failure_message(error));
		return false;
	}

	return true;
}

auto sign_in_and_play(LoginWork* t_work) -> void
{
	if (!authenticate(t_work)) return;

	set_stage(t_work, LoginStage::Launching);

	std::string      late_error;
	const PlayResult play = t_work->riot_client.click_play_when_ready(K_PLAY_BUTTON_TIMEOUT_MS, &late_error);

	if (stop_if_cancelled(t_work)) return;

	if (play == PlayResult::LoginError) {
		fail(t_work, failure_message(late_error));
		return;
	}

	set_stage(t_work, LoginStage::Success);
}

auto run_login(LoginWork* t_work) -> void
{
	t_work->message[0] = '\0';
	set_stage(t_work, LoginStage::WaitingForProcess);

	if (!start_fresh_client(t_work) || !focus_client(t_work)) return;

	if (t_work->riot_client.start_automation()) {
		sign_in_and_play(t_work);
	} else {
		debug_log::write(K_LOG_CATEGORY, "UI automation failed to start - see the uia lines just above for the HRESULT");
		fail(t_work, K_AUTOMATION_FAILED_MESSAGE);
	}

	t_work->riot_client.stop_automation();
}

auto worker_main(const std::shared_ptr<LoginWork>& t_work) -> void
{
	debug_log::write(K_LOG_CATEGORY, "worker started");
	run_login(t_work.get());
	sodium_memzero(t_work->password, sizeof(t_work->password));

	t_work->worker_finished.store(true, std::memory_order_release);

	debug_log::write(K_LOG_CATEGORY, "worker finished (stage %s), unwinding", stage_name(t_work->stage.load(std::memory_order_acquire)));
}
}

LoginAttempt::~LoginAttempt()
{
	if (m_work != nullptr) {
		m_work->cancel_requested.store(true, std::memory_order_relaxed);
	}

	if (m_worker.joinable()) {
		debug_log::write(K_LOG_CATEGORY, "shutting down with a worker still running - cancelling and joining");
	}

	const debug_log::Scope scope(K_LOG_CATEGORY, "shutdown join of the login worker");
	join_worker(K_SHUTDOWN_JOIN_TIMEOUT);
}

auto LoginAttempt::is_terminal(LoginStage t_stage) -> bool
{
	return t_stage == LoginStage::Success || t_stage == LoginStage::Error || t_stage == LoginStage::Cancelled;
}

auto LoginAttempt::start(std::string_view t_username, std::string_view t_password, std::string_view t_launch_product, std::string_view t_client_path) -> void
{
	if (m_active && !is_terminal(stage())) {
		debug_log::write(K_LOG_CATEGORY, "start refused - the previous attempt is still active (stage %s)", stage_name(stage()));
		return;
	}

	join_worker(K_FINISHED_JOIN_TIMEOUT);

	auto work = std::make_shared<LoginWork>();
	copy_to(t_username, work->username);
	copy_to(t_password, work->password);
	copy_to(t_launch_product, work->launch_product);
	work->remembered_client_path = std::string{t_client_path};
	work->stage.store(LoginStage::WaitingForProcess, std::memory_order_relaxed);

	m_work   = work;
	m_worker = std::thread(worker_main, std::move(work));
	m_active = true;

	debug_log::write(K_LOG_CATEGORY, "attempt started for \"%s\"", m_work->launch_product);
}

auto LoginAttempt::cancel() -> void
{
	if (!m_active || m_work == nullptr || is_terminal(stage())) return;

	debug_log::write(K_LOG_CATEGORY, "cancel requested at stage %s", stage_name(stage()));
	m_work->cancel_requested.store(true, std::memory_order_relaxed);
	m_cancel_deadline = std::chrono::steady_clock::now() + K_CANCEL_GRACE_PERIOD;
}

auto LoginAttempt::stage() const -> LoginStage
{
	return m_work != nullptr ? m_work->stage.load(std::memory_order_acquire) : LoginStage::Idle;
}

auto LoginAttempt::terminal_message() const -> std::string_view
{
	return m_work != nullptr ? std::string_view{m_work->message} : std::string_view{};
}

auto LoginAttempt::found_client_path() const -> std::string
{
	if (m_work == nullptr || !is_terminal(stage())) return {};

	return m_work->found_client_path;
}

auto LoginAttempt::is_client_missing() const -> bool
{
	return m_work != nullptr && is_terminal(stage()) && m_work->client_missing;
}

auto LoginAttempt::update() -> void
{
	if (!m_active || m_work == nullptr) return;

	if (m_work->worker_finished.load(std::memory_order_acquire)) {
		const debug_log::Scope scope(K_LOG_CATEGORY, "render-thread join of a finished login worker");
		join_worker(K_FINISHED_JOIN_TIMEOUT);
		m_active = false;

		debug_log::write(K_LOG_CATEGORY, "attempt retired (final stage %s)", stage_name(stage()));

		return;
	}

	const bool cancel_ignored = m_work->cancel_requested.load(std::memory_order_relaxed) && std::chrono::steady_clock::now() >= m_cancel_deadline;
	if (cancel_ignored) {
		abandon_worker();
	}
}

auto LoginAttempt::abandon_worker() -> void
{
	const bool user_cancelled = m_work->cancel_requested.load(std::memory_order_relaxed);

	debug_log::write(K_LOG_CATEGORY, "ABANDONING the login worker - it never acknowledged the cancel (stage %s)", stage_name(stage()));

	// The detached worker keeps its own reference to the old work, so it can never write into the next attempt.
	m_work->cancel_requested.store(true, std::memory_order_relaxed);
	join_worker(std::chrono::milliseconds{0});

	auto abandoned = std::make_shared<LoginWork>();
	if (user_cancelled) {
		abandoned->stage.store(LoginStage::Cancelled, std::memory_order_relaxed);
	} else {
		copy_to(K_UNRESPONSIVE_CLIENT_MESSAGE, abandoned->message);
		abandoned->stage.store(LoginStage::Error, std::memory_order_relaxed);
	}

	abandoned->worker_finished.store(true, std::memory_order_relaxed);

	m_work   = std::move(abandoned);
	m_active = false;
}

auto LoginAttempt::join_worker(std::chrono::milliseconds t_timeout) -> void
{
	if (m_worker.joinable()) {
		join_or_abandon(&m_worker, &m_work->worker_finished, t_timeout);
	}
}
