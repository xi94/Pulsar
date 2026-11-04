#include "login/client_watch.h"

namespace {
constexpr auto K_SIGNED_IN_CONFIRMATION = std::chrono::seconds(3);
constexpr auto K_UNANSWERED_SUBMIT      = std::chrono::seconds(10);
constexpr auto K_SIGN_IN_TIMEOUT        = std::chrono::seconds(60);
constexpr auto K_PLAY_SETTLE            = std::chrono::milliseconds(1500);
constexpr auto K_PRESS_CONFIRMATION     = std::chrono::seconds(5);
constexpr auto K_PLAY_TIMEOUT           = std::chrono::seconds(30);
constexpr u32  K_MAX_PLAY_PRESSES       = 2;
}

auto FormWatch::observe(const ClientView& t_view, WatchClock::time_point t_now) -> std::optional<FormWait>
{
	if (t_view.login_form_shown) return FormWait::FORM_SHOWN;

	if (!t_view.play_shown) {
		m_signed_in_since.reset();
		return std::nullopt;
	}

	if (!m_signed_in_since) m_signed_in_since = t_now;
	if (t_now - *m_signed_in_since < K_SIGNED_IN_CONFIRMATION) return std::nullopt;

	return FormWait::ALREADY_SIGNED_IN;
}

SignInWatch::SignInWatch(const ClientView& t_before_submit, WatchClock::time_point t_submitted)
	: m_stale_error(t_before_submit.login_error)
	, m_submitted(t_submitted)
	, m_last_sign_of_life(t_submitted)
{
}

auto SignInWatch::observe(const ClientView& t_view, WatchClock::time_point t_now) -> std::optional<SignInResult>
{
	if (m_stale_error && t_view.login_error != m_stale_error) {
		m_stale_error.reset();
		m_last_sign_of_life = t_now;
	}

	if (t_view.login_form_shown && t_view.login_error && !m_stale_error) return SignInResult{SignInOutcome::REFUSED, *t_view.login_error};
	if (t_view.play_shown) return SignInResult{SignInOutcome::SIGNED_IN, {}};

	// The client hides the form while it talks to Riot, so only a long absence counts as signed in.
	if (!t_view.window_shown || !t_view.login_form_shown) {
		m_last_sign_of_life = t_now;
	}

	if (t_view.window_shown && !t_view.login_form_shown) {
		if (!m_form_gone_since) m_form_gone_since = t_now;
		if (t_now - *m_form_gone_since >= K_SIGNED_IN_CONFIRMATION) return SignInResult{SignInOutcome::SIGNED_IN, {}};
	} else {
		m_form_gone_since.reset();
	}

	if (t_now - m_submitted >= K_SIGN_IN_TIMEOUT) return SignInResult{SignInOutcome::TIMED_OUT, {}};
	if (t_now - m_last_sign_of_life >= K_UNANSWERED_SUBMIT) return SignInResult{SignInOutcome::NO_RESPONSE, {}};

	return std::nullopt;
}

PlayWatch::PlayWatch(WatchClock::time_point t_started)
	: m_started(t_started)
{
}

auto PlayWatch::observe(const ClientView& t_view, WatchClock::time_point t_now) -> PlayStep
{
	if (t_view.login_form_shown && t_view.login_error) return PlayStep::LOGIN_ERROR;

	const bool ready = t_view.play_shown && t_view.play_enabled;

	if (m_last_press) {
		if (!ready) return PlayStep::PRESSED;
		if (t_now - *m_last_press < K_PRESS_CONFIRMATION) return PlayStep::WAIT;
		if (m_presses >= K_MAX_PLAY_PRESSES) return PlayStep::PRESSED;

		return press_now(t_now);
	}

	if (!ready) {
		m_ready_since.reset();
		return t_now - m_started >= K_PLAY_TIMEOUT ? PlayStep::NOT_FOUND : PlayStep::WAIT;
	}

	if (!m_ready_since) m_ready_since = t_now;
	if (t_now - *m_ready_since < K_PLAY_SETTLE) return PlayStep::WAIT;

	return press_now(t_now);
}

auto PlayWatch::press_now(WatchClock::time_point t_now) -> PlayStep
{
	m_last_press = t_now;
	m_presses += 1;

	return PlayStep::PRESS;
}
