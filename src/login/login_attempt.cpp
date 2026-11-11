#include "login/login_attempt.h"

#include <algorithm>
#include <string>
#include <thread>
#include <tuple>
#include <utility>

#include <sodium.h>

#include "core/debug_log.h"
#include "core/str.h"
#include "core/thread_util.h"
#include "games.h"

namespace {
constexpr const char* K_LOG_CATEGORY = "login";

constexpr auto K_POLL_INTERVAL           = std::chrono::milliseconds(100);
constexpr auto K_OPEN_FORM_GRACE         = std::chrono::milliseconds(1500);
constexpr auto K_FORWARDED_LAUNCH_SETTLE = std::chrono::milliseconds(1000);
constexpr auto K_HIDDEN_FORM_PATIENCE    = std::chrono::seconds(60);
constexpr auto K_GAME_START_PATIENCE     = std::chrono::seconds(60);
constexpr auto K_GAME_START_SETTLE       = std::chrono::milliseconds(1500);
constexpr auto K_BUSY_RETRY_PAUSE        = std::chrono::milliseconds(3000);
constexpr u32  K_MAX_BUSY_RETRIES        = 2;
constexpr u32  K_MAX_UNANSWERED_RETRIES  = 2;
constexpr auto K_CANCEL_GRACE_PERIOD     = std::chrono::milliseconds(5000);
constexpr auto K_SHUTDOWN_JOIN_TIMEOUT   = std::chrono::milliseconds(3000);
constexpr auto K_FINISHED_JOIN_TIMEOUT   = std::chrono::milliseconds(50);

constexpr const char* K_INVALID_CREDENTIALS_MESSAGE = "Wrong username or password.";
constexpr const char* K_SERVER_ERROR_MESSAGE        = "Riot's servers are busy. Try again soon.";
constexpr const char* K_UNRECOGNIZED_ERROR_MESSAGE  = "Sign-in failed. Unrecognized error.";
constexpr const char* K_RIOT_ERROR_PREFIX           = "Riot Client: ";
constexpr const char* K_UNKNOWN_GAME_TITLE          = "the running game";
constexpr const char* K_NO_RIOT_CLIENT_MESSAGE      = "Riot Client not found. Set it in Settings.";
constexpr const char* K_LAUNCH_FAILED_MESSAGE       = "The Riot Client wouldn't start.";
constexpr const char* K_UNRESPONSIVE_CLIENT_MESSAGE = "The Riot Client stopped responding.";
constexpr const char* K_ALREADY_SIGNED_IN_MESSAGE   = "The Riot Client is signed in to another account. Sign out there first.";
constexpr const char* K_NO_RESPONSE_MESSAGE         = "The Riot Client didn't respond to the sign-in. Try again.";
constexpr const char* K_PRESS_PLAY_MESSAGE          = "Logged in. Press Play in the Riot Client.";
constexpr const char* K_UPDATE_NEEDED_MESSAGE       = "Logged in. An update is available in the Riot Client.";
constexpr const char* K_GAME_NOT_STARTED_MESSAGE    = "Logged in, but the game didn't start. Check the Riot Client.";
constexpr const char* K_USER_OPENED_CLIENT_MESSAGE  = "You opened the Riot Client, so Pulsar stopped logging in.";

[[nodiscard]] auto stage_name(LoginStage t_stage) -> const char*
{
	switch (t_stage) {
		using enum LoginStage;

		case IDLE: {
			return "IDLE";
		}

		case WAITING_FOR_PROCESS: {
			return "WAITING_FOR_PROCESS";
		}

		case CONNECTING: {
			return "CONNECTING";
		}

		case AUTHENTICATING: {
			return "AUTHENTICATING";
		}

		case LAUNCHING: {
			return "LAUNCHING";
		}

		case SUCCESS: {
			return "SUCCESS";
		}

		case FAILED: {
			return "FAILED";
		}

		case CANCELLED: {
			return "CANCELLED";
		}
	}

	return "?";
}

[[nodiscard]] auto progress_label(LoginStage t_stage) -> std::string_view
{
	switch (t_stage) {
		using enum LoginStage;

		case IDLE: {
			return "";
		}

		case WAITING_FOR_PROCESS: {
			return "Starting the Riot Client";
		}

		case CONNECTING: {
			return "Waiting for the sign-in form";
		}

		case AUTHENTICATING: {
			return "Signing in";
		}

		case LAUNCHING: {
			return "Starting the game";
		}

		case SUCCESS: {
			return "Done";
		}

		case FAILED: {
			return "Failed";
		}

		case CANCELLED: {
			return "Cancelled";
		}
	}

	return "";
}

auto show_progress(LoginWork* t_work, LoginStage t_stage) -> void
{
	char                   progress[K_LOGIN_MESSAGE_CAPACITY + 32];
	const std::string_view label = progress_label(t_stage);

	t_work->riot_client.show_progress(t_work->message[0] != '\0' ? format_into(progress, "{}: {}", label, t_work->message) : label);
}

[[nodiscard]] auto step_of(LoginStage t_stage) -> std::optional<LoginStep>
{
	switch (t_stage) {
		using enum LoginStage;

		case WAITING_FOR_PROCESS:
		case CONNECTING: {
			return LoginStep::START_CLIENT;
		}

		case AUTHENTICATING: {
			return LoginStep::SIGN_IN;
		}

		case LAUNCHING: {
			return LoginStep::START_GAME;
		}

		case IDLE:
		case SUCCESS:
		case FAILED:
		case CANCELLED: {
			break;
		}
	}

	return std::nullopt;
}

[[nodiscard]] auto outcome_of_last_step(const LoginWork* t_work, LoginStage t_final_stage) -> StepState
{
	if (t_final_stage != LoginStage::SUCCESS) return StepState::FAILED;

	return t_work->message[0] == '\0' ? StepState::DONE : StepState::LEFT_TO_USER;
}

auto record_on_timeline(LoginWork* t_work, LoginStage t_stage) -> void
{
	const std::lock_guard lock(t_work->timeline_lock);

	if (const std::optional<LoginStep> step = step_of(t_stage)) {
		t_work->timeline.begin(*step, WatchClock::now());
	} else if (LoginAttempt::is_terminal(t_stage)) {
		t_work->timeline.finish(outcome_of_last_step(t_work, t_stage), WatchClock::now());
	}
}

auto set_stage(LoginWork* t_work, LoginStage t_stage) -> void
{
	record_on_timeline(t_work, t_stage);
	debug_log::write(K_LOG_CATEGORY, "stage -> %s%s", stage_name(t_stage), t_work->message[0] != '\0' ? " (with a message)" : "");
	show_progress(t_work, t_stage);
	t_work->stage.store(t_stage, std::memory_order_release);
}

auto fail(LoginWork* t_work, std::string_view t_message) -> void
{
	copy_to(t_message, t_work->message);
	set_stage(t_work, LoginStage::FAILED);
}

auto fail_with_riot_error(LoginWork* t_work, std::string_view t_riot_error) -> void
{
	std::ignore = login_failure_message(t_riot_error, t_work->message);
	set_stage(t_work, LoginStage::FAILED);
}

[[nodiscard]] auto is_cancelled(const LoginWork* t_work) -> bool
{
	return t_work->cancel_requested.load(std::memory_order_relaxed) || t_work->riot_client.was_opened_by_user();
}

[[nodiscard]] auto stop_if_cancelled(LoginWork* t_work) -> bool
{
	if (t_work->cancel_requested.load(std::memory_order_relaxed)) {
		set_stage(t_work, LoginStage::CANCELLED);
		return true;
	}

	if (t_work->riot_client.was_opened_by_user()) {
		fail(t_work, K_USER_OPENED_CLIENT_MESSAGE);
		return true;
	}

	return false;
}

[[nodiscard]] auto wait_unless_cancelled(const LoginWork* t_work, std::chrono::milliseconds t_duration) -> bool
{
	const auto until = WatchClock::now() + t_duration;

	while (WatchClock::now() < until) {
		if (is_cancelled(t_work)) return false;

		std::this_thread::sleep_for(K_POLL_INTERVAL);
	}

	return !is_cancelled(t_work);
}

[[nodiscard]] auto resolve_client(LoginWork* t_work) -> bool
{
	if (!t_work->riot_client.resolve_executable_path(t_work->remembered_client_path)) {
		t_work->client_missing = true;
		fail(t_work, K_NO_RIOT_CLIENT_MESSAGE);
		return false;
	}

	if (t_work->riot_client.m_executable_path != t_work->remembered_client_path) {
		t_work->found_client_path = t_work->riot_client.m_executable_path;
	}

	return true;
}

auto hide_client_if_wanted(LoginWork* t_work) -> void
{
	if (!t_work->options.hide_client) return;

	if (t_work->riot_client.start_hiding(t_work->options.show_live_view)) {
		debug_log::write(K_LOG_CATEGORY, "keeping the Riot Client hidden while signing in");
	} else {
		debug_log::write(K_LOG_CATEGORY, "couldn't hide the Riot Client - signing in with it shown");
	}
}

[[nodiscard]] auto take_over_open_client(LoginWork* t_work) -> bool
{
	RiotClient* client = &t_work->riot_client;
	if (!RiotClient::forwards_launch_to_open_client() || !client->has_open_window() || !client->start_automation()) return false;

	const auto deadline = WatchClock::now() + K_OPEN_FORM_GRACE;

	while (WatchClock::now() < deadline && !is_cancelled(t_work)) {
		if (client->look().login_form_shown) {
			debug_log::write(K_LOG_CATEGORY, "the open Riot Client is on its sign-in screen - signing in there instead of restarting it");
			hide_client_if_wanted(t_work);
			return client->launch(t_work->launch_product) && wait_unless_cancelled(t_work, K_FORWARDED_LAUNCH_SETTLE);
		}

		std::this_thread::sleep_for(K_POLL_INTERVAL);
	}

	client->stop_automation();

	return false;
}

[[nodiscard]] auto restart_client(LoginWork* t_work) -> bool
{
	RiotClient* client = &t_work->riot_client;

	RiotClient::kill_all_client_processes(&t_work->cancel_requested);
	if (stop_if_cancelled(t_work)) return false;

	if (RiotClient::forget_stored_sign_in()) {
		debug_log::write(K_LOG_CATEGORY, "forgot the Riot Client's remembered sign-in so it opens at the sign-in form");
	}

	hide_client_if_wanted(t_work);

	if (!client->launch(t_work->launch_product)) {
		fail(t_work, K_LAUNCH_FAILED_MESSAGE);
		return false;
	}

	if (!client->is_hiding()) {
		client->bring_to_front();
	}

	if (stop_if_cancelled(t_work)) return false;

	if (!client->start_automation()) {
		debug_log::write(K_LOG_CATEGORY, "UI automation failed to start - see the uia lines just above for the HRESULT");
		fail(t_work, RiotClient::automation_failure_message());
		return false;
	}

	return true;
}

[[nodiscard]] auto game_title(std::string_view t_launch_product) -> std::string_view
{
	const auto game = std::ranges::find(K_GAME_INFOS, t_launch_product, &GameInfo::launch_product);

	return game != std::end(K_GAME_INFOS) ? game->short_title : K_UNKNOWN_GAME_TITLE;
}

[[nodiscard]] auto prepare_client(LoginWork* t_work) -> bool
{
	if (const std::optional<std::string_view> running = RiotClient::running_game()) {
		char message[K_LOGIN_MESSAGE_CAPACITY];
		fail(t_work, format_into(message, "Close {} first to switch accounts.", game_title(*running)));
		return false;
	}

	if (stop_if_cancelled(t_work) || !resolve_client(t_work)) return false;
	if (take_over_open_client(t_work)) return !stop_if_cancelled(t_work);

	return restart_client(t_work);
}

[[nodiscard]] auto wait_for_login_form(LoginWork* t_work) -> bool
{
	set_stage(t_work, LoginStage::CONNECTING);
	debug_log::write(K_LOG_CATEGORY, "waiting for the sign-in form (until cancelled)");

	RiotClient* client = &t_work->riot_client;
	FormWatch   watch;
	const auto  started = WatchClock::now();

	for (;;) {
		const std::optional<FormWait> seen = watch.observe(client->look(), WatchClock::now());

		if (seen == FormWait::FORM_SHOWN) {
			client->watch_for_user_opening();
			return true;
		}

		if (seen == FormWait::ALREADY_SIGNED_IN) {
			fail(t_work, K_ALREADY_SIGNED_IN_MESSAGE);
			return false;
		}

		if (stop_if_cancelled(t_work)) return false;

		// The client may be updating itself, which the user should be able to see.
		if (client->is_hiding() && WatchClock::now() - started >= K_HIDDEN_FORM_PATIENCE) {
			debug_log::write(K_LOG_CATEGORY, "the hidden client hasn't reached its sign-in form - showing it and still waiting");
			client->hand_back(HandBack::SHOWN);
		}

		std::this_thread::sleep_for(K_POLL_INTERVAL);
	}
}

[[nodiscard]] auto watch_sign_in(LoginWork* t_work, const ClientView& t_before_submit) -> SignInResult
{
	SignInWatch watch(t_before_submit, WatchClock::now());

	while (!is_cancelled(t_work)) {
		if (std::optional<SignInResult> result = watch.observe(t_work->riot_client.look(), WatchClock::now())) return std::move(*result);

		std::this_thread::sleep_for(K_POLL_INTERVAL);
	}

	return SignInResult{SignInOutcome::TIMED_OUT, {}};
}

[[nodiscard]] auto sign_in(LoginWork* t_work) -> bool
{
	u32 busy_retries       = 0;
	u32 unanswered_retries = 0;

	for (;;) {
		set_stage(t_work, LoginStage::AUTHENTICATING);

		const ClientView   before_submit = t_work->riot_client.look();
		const bool         submitted     = t_work->riot_client.fill_and_submit(t_work->username, t_work->password);
		const SignInResult result        = submitted ? watch_sign_in(t_work, before_submit) : SignInResult{SignInOutcome::NO_RESPONSE, {}};

		if (stop_if_cancelled(t_work)) return false;

		switch (result.outcome) {
			using enum SignInOutcome;

			case SIGNED_IN: {
				return true;
			}

			case NO_RESPONSE: {
				if (unanswered_retries == K_MAX_UNANSWERED_RETRIES) {
					fail(t_work, K_NO_RESPONSE_MESSAGE);
					return false;
				}

				unanswered_retries += 1;
				debug_log::write(K_LOG_CATEGORY, "the client never answered the sign-in - submitting again (%u of %u)", unanswered_retries, K_MAX_UNANSWERED_RETRIES);
				continue;
			}

			case TIMED_OUT: {
				fail(t_work, K_NO_RESPONSE_MESSAGE);
				return false;
			}

			case REFUSED: {
				const bool busy = classify_login_failure(result.riot_error) == LoginFailure::RIOT_BUSY;

				if (!busy || busy_retries == K_MAX_BUSY_RETRIES) {
					debug_log::write(K_LOG_CATEGORY, "the client refused the sign-in: \"%s\"", result.riot_error.c_str());
					fail_with_riot_error(t_work, result.riot_error);
					return false;
				}

				busy_retries += 1;
				debug_log::write(K_LOG_CATEGORY, "Riot's servers are busy - trying again shortly (%u of %u)", busy_retries, K_MAX_BUSY_RETRIES);

				if (!wait_unless_cancelled(t_work, K_BUSY_RETRY_PAUSE)) {
					std::ignore = stop_if_cancelled(t_work);
					return false;
				}

				continue;
			}
		}
	}
}

auto finish_once_game_started(LoginWork* t_work) -> void
{
	debug_log::write(K_LOG_CATEGORY, "the game has started");
	std::ignore = wait_unless_cancelled(t_work, K_GAME_START_SETTLE);
	set_stage(t_work, LoginStage::SUCCESS);
}

auto press_play(LoginWork* t_work) -> void
{
	set_stage(t_work, LoginStage::LAUNCHING);

	RiotClient*                           client = &t_work->riot_client;
	PlayWatch                             watch(WatchClock::now());
	std::optional<WatchClock::time_point> waiting_for_game_since;

	for (;;) {
		if (stop_if_cancelled(t_work)) return;

		// A hidden client goes back minimized only once the game is really on its way, so it never freezes in the middle of launching it.
		if (client->is_hiding() && client->has_game_started()) {
			finish_once_game_started(t_work);
			return;
		}

		const ClientView             view = client->look();
		const WatchClock::time_point now  = WatchClock::now();

		switch (watch.observe(view, now)) {
			using enum PlayStep;

			case WAIT: {
				break;
			}

			case PRESS: {
				debug_log::write(K_LOG_CATEGORY, "Play has settled - pressing it (press %u)", watch.presses());
				client->press_play();
				break;
			}

			case PRESSED: {
				if (!client->is_hiding()) {
					set_stage(t_work, LoginStage::SUCCESS);
					return;
				}

				if (!waiting_for_game_since) {
					waiting_for_game_since = now;
				}

				if (now - *waiting_for_game_since >= K_GAME_START_PATIENCE) {
					debug_log::write(K_LOG_CATEGORY, "Play was pressed but no game started");
					copy_to(K_GAME_NOT_STARTED_MESSAGE, t_work->message);
					set_stage(t_work, LoginStage::SUCCESS);
					return;
				}

				break;
			}

			case UPDATE_NEEDED: {
				debug_log::write(K_LOG_CATEGORY, "the game needs an update before it can be played - leaving that to the user");
				copy_to(K_UPDATE_NEEDED_MESSAGE, t_work->message);
				set_stage(t_work, LoginStage::SUCCESS);
				return;
			}

			case NOT_FOUND: {
				debug_log::write(K_LOG_CATEGORY, "Play never became ready - leaving the game for the user to start");
				copy_to(K_PRESS_PLAY_MESSAGE, t_work->message);
				set_stage(t_work, LoginStage::SUCCESS);
				return;
			}

			case LOGIN_ERROR: {
				fail_with_riot_error(t_work, view.login_error.value_or(std::string{}));
				return;
			}
		}

		std::this_thread::sleep_for(K_POLL_INTERVAL);
	}
}

auto finish_after_sign_in(LoginWork* t_work) -> void
{
	if (t_work->options.launch_game) {
		press_play(t_work);
	} else {
		set_stage(t_work, LoginStage::SUCCESS);
	}
}

auto run_login(LoginWork* t_work) -> void
{
	t_work->message[0] = '\0';
	set_stage(t_work, LoginStage::WAITING_FOR_PROCESS);

	if (!RiotClient::request_automation_permission()) {
		t_work->permission_missing = true;
		fail(t_work, RiotClient::permission_missing_message());
		return;
	}

	if (prepare_client(t_work) && wait_for_login_form(t_work) && sign_in(t_work)) {
		finish_after_sign_in(t_work);
	}

	t_work->riot_client.hand_back(hand_back_after(t_work->stage.load(std::memory_order_acquire), t_work->message));
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

auto classify_login_failure(std::string_view t_riot_error) -> LoginFailure
{
	if (find_ignoring_case(t_riot_error, "credentials") != std::string_view::npos) return LoginFailure::INVALID_CREDENTIALS;
	if (find_ignoring_case(t_riot_error, "trouble signing you in") != std::string_view::npos) return LoginFailure::RIOT_BUSY;

	return LoginFailure::UNRECOGNIZED;
}

auto login_failure_message(std::string_view t_riot_error, char (&t_buffer)[K_LOGIN_MESSAGE_CAPACITY]) -> std::string_view
{
	switch (classify_login_failure(t_riot_error)) {
		using enum LoginFailure;

		case INVALID_CREDENTIALS: {
			copy_to(K_INVALID_CREDENTIALS_MESSAGE, t_buffer);
			break;
		}

		case RIOT_BUSY: {
			copy_to(K_SERVER_ERROR_MESSAGE, t_buffer);
			break;
		}

		case UNRECOGNIZED: {
			copy_to(t_riot_error.empty() ? std::string{K_UNRECOGNIZED_ERROR_MESSAGE} : K_RIOT_ERROR_PREFIX + std::string{t_riot_error}, t_buffer);
			break;
		}
	}

	return t_buffer;
}

auto hand_back_after(LoginStage t_final_stage, std::string_view t_message) -> HandBack
{
	return t_final_stage == LoginStage::SUCCESS && t_message.empty() ? HandBack::MINIMIZED : HandBack::SHOWN;
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
	return t_stage == LoginStage::SUCCESS || t_stage == LoginStage::FAILED || t_stage == LoginStage::CANCELLED;
}

auto LoginAttempt::start(std::string_view t_username, std::string_view t_password, std::string_view t_launch_product, std::string_view t_client_path, LoginOptions t_options) -> void
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
	work->options                = t_options;
	work->timeline               = LoginTimeline(WatchClock::now(), t_options.launch_game);
	work->stage.store(LoginStage::WAITING_FOR_PROCESS, std::memory_order_relaxed);

	m_work   = work;
	m_worker = std::thread(worker_main, std::move(work));
	m_active = true;

	debug_log::write(K_LOG_CATEGORY, "attempt started for \"%s\" (%s, %s)", m_work->launch_product, t_options.launch_game ? "pressing Play after signing in" : "only signing in", t_options.hide_client ? "client hidden" : "client shown");
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
	return m_work != nullptr ? m_work->stage.load(std::memory_order_acquire) : LoginStage::IDLE;
}

auto LoginAttempt::terminal_message() const -> std::string_view
{
	return m_work != nullptr ? std::string_view{m_work->message} : std::string_view{};
}

auto LoginAttempt::timeline() const -> LoginTimeline
{
	if (m_work == nullptr) return {};

	const std::lock_guard lock(m_work->timeline_lock);

	return m_work->timeline;
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

auto LoginAttempt::is_permission_missing() const -> bool
{
	return m_work != nullptr && is_terminal(stage()) && m_work->permission_missing;
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
		abandoned->stage.store(LoginStage::CANCELLED, std::memory_order_relaxed);
	} else {
		copy_to(K_UNRESPONSIVE_CLIENT_MESSAGE, abandoned->message);
		abandoned->stage.store(LoginStage::FAILED, std::memory_order_relaxed);
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
