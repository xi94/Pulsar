#include "core/login_attempt.h"

#include <string>
#include <utility>

#include "core/debug_log.h"
#include "core/str.h"
#include "core/thread_util.h"
#include "core/ui_automation.h"

namespace {
constexpr const char *log_category = "login";

constexpr u32 login_form_timeout_ms = 10000;
constexpr u32 login_result_timeout_ms = 6000;
constexpr u32 foreground_timeout_ms = 5000;
constexpr u32 focus_timeout_ms = 5000;
constexpr u32 play_button_timeout_ms = 8000;
constexpr auto cancel_grace_period = std::chrono::milliseconds(5000);
constexpr auto shutdown_join_timeout = std::chrono::milliseconds(3000);
constexpr auto finished_join_timeout = std::chrono::milliseconds(50);

constexpr const char *invalid_credentials_message = "Invalid username or password.";
constexpr const char *server_error_message =
	"Something went wrong - Riot's servers might be overloaded. Try again in a moment.";
constexpr const char *game_in_progress_message = "A game is already running - close it before switching accounts.";
constexpr const char *no_riot_client_message = "Couldn't find the Riot Client - is it installed?";
constexpr const char *launch_failed_message = "Couldn't launch the Riot Client.";
constexpr const char *window_timeout_message = "The Riot Client didn't respond in time.";
constexpr const char *form_timeout_message = "Couldn't find the Riot Client's login form.";
constexpr const char *unresponsive_client_message = "The Riot Client stopped responding - try again.";
constexpr const char *automation_failed_message = "Couldn't start Windows UI Automation - try again.";

enum class SubmitResult : u8 {
	form_not_found,
	error_shown,
	no_error_shown,
};

const char *stage_name(LoginStage t_stage)
{
	switch (t_stage) {
		case LoginStage::idle:
			return "IDLE";
		case LoginStage::waiting_for_process:
			return "WAITING_FOR_PROCESS";
		case LoginStage::connecting:
			return "CONNECTING";
		case LoginStage::authenticating:
			return "AUTHENTICATING";
		case LoginStage::launching:
			return "LAUNCHING";
		case LoginStage::success:
			return "SUCCESS";
		case LoginStage::error:
			return "ERROR";
		case LoginStage::cancelled:
			return "CANCELLED";
	}

	return "?";
}

void set_stage(LoginWork &t_work, LoginStage t_stage)
{
	debug_log::write(log_category, "stage -> %s%s", stage_name(t_stage),
					 t_work.message[0] != '\0' ? " (with a message)" : "");
	t_work.stage.store(t_stage, std::memory_order_release);
}

void fail(LoginWork &t_work, const char *t_message)
{
	copy_to(t_message, t_work.message);
	set_stage(t_work, LoginStage::error);
}

bool stop_if_cancelled(LoginWork &t_work)
{
	if (!t_work.cancel_requested.load(std::memory_order_relaxed)) return false;

	set_stage(t_work, LoginStage::cancelled);

	return true;
}

void fail_unless_cancelled(LoginWork &t_work, const char *t_message)
{
	if (!stop_if_cancelled(t_work)) {
		fail(t_work, t_message);
	}
}

bool is_invalid_credentials(const std::wstring &t_error)
{
	return t_error.find(L"credentials") != std::wstring::npos;
}

SubmitResult submit_and_wait_for_result(LoginWork &t_work, const UiAutomation &t_automation, std::wstring &t_out_error,
										const std::wstring *t_error_to_ignore = nullptr)
{
	t_out_error.clear();

	if (!t_work.riot_client.submit_login(t_automation, t_work.username, t_work.password, login_form_timeout_ms,
										 t_work.cancel_requested)) {
		return SubmitResult::form_not_found;
	}

	const bool error_shown = t_work.riot_client.wait_for_login_error(t_automation, t_out_error, login_result_timeout_ms,
																	 t_work.cancel_requested, t_error_to_ignore);

	return error_shown ? SubmitResult::error_shown : SubmitResult::no_error_shown;
}

bool start_fresh_client(LoginWork &t_work)
{
	if (RiotClient::is_game_in_progress()) {
		fail(t_work, game_in_progress_message);
		return false;
	}

	if (stop_if_cancelled(t_work)) return false;

	RiotClient::kill_all_client_processes();

	if (!t_work.riot_client.resolve_executable_path()) {
		fail(t_work, no_riot_client_message);
		return false;
	}

	if (!t_work.riot_client.launch(RiotClient::launch_product_for(t_work.game_title))) {
		fail(t_work, launch_failed_message);
		return false;
	}

	if (!t_work.riot_client.wait_for_window(RiotClient::wait_forever_ms, t_work.cancel_requested)) {
		fail_unless_cancelled(t_work, window_timeout_message);
		return false;
	}

	return !stop_if_cancelled(t_work);
}

bool focus_client(LoginWork &t_work)
{
	set_stage(t_work, LoginStage::connecting);

	t_work.riot_client.bring_to_foreground(foreground_timeout_ms, t_work.cancel_requested);
	t_work.riot_client.take_keyboard_focus(focus_timeout_ms, t_work.cancel_requested);

	return !stop_if_cancelled(t_work);
}

const char *form_failure_message(const UiAutomation &t_automation)
{
	return t_automation.has_wedged() ? unresponsive_client_message : form_timeout_message;
}

bool authenticate(LoginWork &t_work, const UiAutomation &t_automation)
{
	set_stage(t_work, LoginStage::authenticating);

	std::wstring error;
	SubmitResult result = submit_and_wait_for_result(t_work, t_automation, error);

	if (result == SubmitResult::form_not_found) {
		fail_unless_cancelled(t_work, form_failure_message(t_automation));
		return false;
	}

	const bool transient_error = result == SubmitResult::error_shown && !is_invalid_credentials(error);
	if (transient_error) {
		if (!focus_client(t_work)) return false;

		set_stage(t_work, LoginStage::authenticating);

		const std::wstring previous_error = error;
		result = submit_and_wait_for_result(t_work, t_automation, error, &previous_error);

		if (result == SubmitResult::form_not_found) {
			fail_unless_cancelled(t_work, form_failure_message(t_automation));
			return false;
		}
	}

	if (result == SubmitResult::error_shown) {
		fail(t_work, is_invalid_credentials(error) ? invalid_credentials_message : server_error_message);
		return false;
	}

	// A wedged client shows no error either, so silence only means success while automation is still answering.
	if (t_automation.has_wedged()) {
		debug_log::write(log_category, "UI Automation gave up on the client - refusing to infer success from silence");
		fail(t_work, unresponsive_client_message);
		return false;
	}

	return !stop_if_cancelled(t_work);
}

void run_login(LoginWork &t_work)
{
	t_work.message[0] = '\0';
	set_stage(t_work, LoginStage::waiting_for_process);

	if (!start_fresh_client(t_work) || !focus_client(t_work)) return;

	UiAutomation automation;
	if (!automation.init()) {
		debug_log::write(log_category, "UiAutomation::init failed - see the uia lines just above for the HRESULT");
		fail(t_work, automation_failed_message);
		return;
	}

	if (!authenticate(t_work, automation)) return;

	set_stage(t_work, LoginStage::launching);
	t_work.riot_client.click_play_when_ready(automation, play_button_timeout_ms, t_work.cancel_requested);

	if (stop_if_cancelled(t_work)) return;

	set_stage(t_work, LoginStage::success);
}

void worker_main(std::shared_ptr<LoginWork> t_work)
{
	debug_log::write(log_category, "worker started");
	run_login(*t_work);

	t_work->worker_finished.store(true, std::memory_order_release);

	debug_log::write(log_category, "worker finished (stage %s), unwinding",
					 stage_name(t_work->stage.load(std::memory_order_acquire)));
}
}

LoginAttempt::~LoginAttempt()
{
	if (m_work != nullptr) {
		m_work->cancel_requested.store(true, std::memory_order_relaxed);
	}

	if (m_worker.joinable()) {
		debug_log::write(log_category, "shutting down with a worker still running - cancelling and joining");
	}

	const debug_log::Scope scope(log_category, "shutdown join of the login worker");
	join_or_abandon(m_worker, shutdown_join_timeout);
}

bool LoginAttempt::is_terminal(LoginStage t_stage)
{
	return t_stage == LoginStage::success || t_stage == LoginStage::error || t_stage == LoginStage::cancelled;
}

void LoginAttempt::start(std::string_view t_username, std::string_view t_password, std::string_view t_game_title)
{
	if (m_active && !is_terminal(stage())) {
		debug_log::write(log_category, "start refused - the previous attempt is still active (stage %s)",
						 stage_name(stage()));
		return;
	}

	join_or_abandon(m_worker, finished_join_timeout);

	auto work = std::make_shared<LoginWork>();
	copy_to(t_username, work->username);
	copy_to(t_password, work->password);
	copy_to(t_game_title, work->game_title);
	work->stage.store(LoginStage::waiting_for_process, std::memory_order_relaxed);

	m_work = work;
	m_worker = std::thread(worker_main, std::move(work));
	m_active = true;

	debug_log::write(log_category, "attempt started for game \"%s\"", m_work->game_title);
}

void LoginAttempt::cancel()
{
	if (!m_active || m_work == nullptr || is_terminal(stage())) return;

	debug_log::write(log_category, "cancel requested at stage %s", stage_name(stage()));
	m_work->cancel_requested.store(true, std::memory_order_relaxed);
	m_cancel_deadline = std::chrono::steady_clock::now() + cancel_grace_period;
}

LoginStage LoginAttempt::stage() const
{
	return m_work != nullptr ? m_work->stage.load(std::memory_order_acquire) : LoginStage::idle;
}

std::string_view LoginAttempt::terminal_message() const
{
	return m_work != nullptr ? std::string_view{m_work->message} : std::string_view{};
}

void LoginAttempt::update()
{
	if (!m_active || m_work == nullptr) return;

	if (m_work->worker_finished.load(std::memory_order_acquire)) {
		const debug_log::Scope scope(log_category, "render-thread join of a finished login worker");
		join_or_abandon(m_worker, finished_join_timeout);
		m_active = false;

		debug_log::write(log_category, "attempt retired (final stage %s)", stage_name(stage()));

		return;
	}

	const bool cancel_ignored = m_work->cancel_requested.load(std::memory_order_relaxed) &&
								std::chrono::steady_clock::now() >= m_cancel_deadline;
	if (cancel_ignored) {
		abandon_worker();
	}
}

void LoginAttempt::abandon_worker()
{
	const bool user_cancelled = m_work->cancel_requested.load(std::memory_order_relaxed);

	debug_log::write(log_category, "ABANDONING the login worker - it never acknowledged the cancel (stage %s)",
					 stage_name(stage()));

	// The detached worker keeps its own reference to the old work, so it can never write into the next attempt.
	m_work->cancel_requested.store(true, std::memory_order_relaxed);
	join_or_abandon(m_worker, std::chrono::milliseconds{0});

	auto abandoned = std::make_shared<LoginWork>();
	if (user_cancelled) {
		abandoned->stage.store(LoginStage::cancelled, std::memory_order_relaxed);
	} else {
		copy_to(unresponsive_client_message, abandoned->message);
		abandoned->stage.store(LoginStage::error, std::memory_order_relaxed);
	}

	abandoned->worker_finished.store(true, std::memory_order_relaxed);

	m_work = std::move(abandoned);
	m_active = false;
}
