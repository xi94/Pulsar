#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "login/client_watch.h"
#include "login/login_timeline.h"
#include "test.h"

namespace {
const WatchClock::time_point K_START{};

[[nodiscard]] auto at(double t_seconds) -> WatchClock::time_point
{
	return K_START + std::chrono::duration_cast<WatchClock::duration>(std::chrono::duration<double>(t_seconds));
}

[[nodiscard]] auto login_form(std::optional<std::string> t_error = std::nullopt) -> ClientView
{
	return ClientView{.window_shown = true, .login_form_shown = true, .login_error = std::move(t_error)};
}

[[nodiscard]] auto between_screens() -> ClientView
{
	return ClientView{.window_shown = true};
}

[[nodiscard]] auto game_page(bool t_play_enabled) -> ClientView
{
	return ClientView{.window_shown = true, .play_shown = true, .play_enabled = t_play_enabled};
}

[[nodiscard]] auto outcome_of(const std::optional<SignInResult>& t_result) -> std::optional<SignInOutcome>
{
	return t_result ? std::optional{t_result->outcome} : std::nullopt;
}

constexpr ElementBounds K_PASSWORD_FIELD{.left = 570, .top = 592, .right = 854, .bottom = 636};
constexpr usize         K_ARROW_INDEX = 6;

[[nodiscard]] auto button_at(i32 t_left, i32 t_top, i32 t_size, bool t_named, bool t_enabled) -> FormButton
{
	return FormButton{.bounds = {.left = t_left, .top = t_top, .right = t_left + t_size, .bottom = t_top + t_size}, .named = t_named, .invokable = true, .enabled = t_enabled};
}

// The buttons the Riot Client's sign-in form shows: window controls, five social sign-in buttons under the password box, the unnamed arrow, and the version label.
[[nodiscard]] auto sign_in_form_buttons(bool t_arrow_enabled) -> std::vector<FormButton>
{
	return {
		button_at(1984, 288, 32, true, true),
		button_at(568, 666, 52, false, true),
		button_at(627, 666, 52, false, true),
		button_at(686, 666, 52, false, true),
		button_at(745, 666, 52, false, true),
		button_at(804, 666, 52, false, true),
		button_at(680, 938, 64, false, t_arrow_enabled),
		button_at(806, 1084, 50, true, true),
	};
}
}

TEST_CASE("the sign-in arrow is the button that turned enabled when the form was filled in")
{
	CHECK(find_sign_in_arrow(sign_in_form_buttons(false), sign_in_form_buttons(true), K_PASSWORD_FIELD) == K_ARROW_INDEX);
}

TEST_CASE("without a list from before filling, the arrow is the lowest unnamed button under the password box")
{
	CHECK(find_sign_in_arrow({}, sign_in_form_buttons(true), K_PASSWORD_FIELD) == K_ARROW_INDEX);
}

TEST_CASE("an arrow that never turned enabled is not pressed")
{
	CHECK(!find_sign_in_arrow(sign_in_form_buttons(false), sign_in_form_buttons(false), K_PASSWORD_FIELD));
}

TEST_CASE("when several buttons turn enabled, the arrow is told apart by where it sits")
{
	std::vector<FormButton> before = sign_in_form_buttons(false);
	before[1].enabled              = false;

	CHECK(find_sign_in_arrow(before, sign_in_form_buttons(true), K_PASSWORD_FIELD) == K_ARROW_INDEX);
}

TEST_CASE("buttons outside the password box's column are never taken for the arrow")
{
	const std::vector<FormButton> after{button_at(1200, 938, 64, false, true)};

	CHECK(!find_sign_in_arrow({}, after, K_PASSWORD_FIELD));
}

TEST_CASE("the sign-in form showing ends the wait for it")
{
	FormWatch watch;

	CHECK(!watch.observe(between_screens(), at(0.0)));
	CHECK(watch.observe(login_form(), at(0.5)) == FormWait::FORM_SHOWN);
}

TEST_CASE("a client already signed in to some account is noticed after a few seconds")
{
	FormWatch watch;

	CHECK(!watch.observe(game_page(true), at(0.0)));
	CHECK(!watch.observe(game_page(true), at(2.0)));
	CHECK(watch.observe(game_page(true), at(3.0)) == FormWait::ALREADY_SIGNED_IN);
}

TEST_CASE("an error shown after submitting refuses the sign-in in Riot's words")
{
	SignInWatch watch(login_form(), at(0.0));

	CHECK(!watch.observe(login_form(), at(0.5)));

	const std::optional<SignInResult> result = watch.observe(login_form("Bad password"), at(1.0));
	REQUIRE(result.has_value());
	CHECK(result->outcome == SignInOutcome::REFUSED);
	CHECK(result->riot_error == "Bad password");
}

TEST_CASE("an error left over from an earlier try only counts once it goes away and comes back")
{
	SignInWatch watch(login_form("Busy"), at(0.0));

	CHECK(!watch.observe(login_form("Busy"), at(1.0)));
	CHECK(!watch.observe(login_form(), at(2.0)));
	CHECK(outcome_of(watch.observe(login_form("Busy"), at(3.0))) == SignInOutcome::REFUSED);
}

TEST_CASE("a different error replaces a leftover one at once")
{
	SignInWatch watch(login_form("Busy"), at(0.0));

	CHECK(outcome_of(watch.observe(login_form("Locked"), at(1.0))) == SignInOutcome::REFUSED);
}

TEST_CASE("a tooltip away from the sign-in form is not a login error")
{
	SignInWatch watch(login_form(), at(0.0));
	ClientView  stray = between_screens();
	stray.login_error = "";

	CHECK(!watch.observe(stray, at(1.0)));
	CHECK(outcome_of(watch.observe(stray, at(4.0))) == SignInOutcome::SIGNED_IN);
}

TEST_CASE("the form staying away for a few seconds means the client signed in")
{
	SignInWatch watch(login_form(), at(0.0));

	CHECK(!watch.observe(between_screens(), at(0.5)));
	CHECK(!watch.observe(between_screens(), at(3.0)));
	CHECK(outcome_of(watch.observe(between_screens(), at(3.5))) == SignInOutcome::SIGNED_IN);
}

TEST_CASE("the form flickering away and back is not a sign-in")
{
	SignInWatch watch(login_form(), at(0.0));

	CHECK(!watch.observe(between_screens(), at(1.0)));
	CHECK(!watch.observe(login_form(), at(2.0)));
	CHECK(!watch.observe(between_screens(), at(3.0)));
	CHECK(!watch.observe(between_screens(), at(5.0)));
}

TEST_CASE("Play showing means the client signed in")
{
	SignInWatch watch(login_form(), at(0.0));

	CHECK(outcome_of(watch.observe(game_page(false), at(0.5))) == SignInOutcome::SIGNED_IN);
}

TEST_CASE("a submit the client never reacts to is noticed")
{
	SignInWatch watch(login_form(), at(0.0));

	CHECK(!watch.observe(login_form(), at(9.0)));
	CHECK(outcome_of(watch.observe(login_form(), at(10.0))) == SignInOutcome::NO_RESPONSE);
}

TEST_CASE("a sign-in that never settles times out")
{
	SignInWatch watch(login_form(), at(0.0));

	for (int second = 1; second < 60; second += 1) {
		const ClientView view = second % 2 == 0 ? login_form() : between_screens();
		REQUIRE(!watch.observe(view, at(second)));
	}

	CHECK(outcome_of(watch.observe(login_form(), at(60.0))) == SignInOutcome::TIMED_OUT);
}

TEST_CASE("Play is only pressed once it has stayed ready for a moment")
{
	PlayWatch watch(at(0.0));

	CHECK(watch.observe(game_page(false), at(0.5)) == PlayStep::WAIT);
	CHECK(watch.observe(game_page(true), at(1.0)) == PlayStep::WAIT);
	CHECK(watch.observe(game_page(true), at(2.0)) == PlayStep::WAIT);
	CHECK(watch.observe(game_page(true), at(2.5)) == PlayStep::PRESS);
}

TEST_CASE("Play going unready starts the wait over")
{
	PlayWatch watch(at(0.0));

	CHECK(watch.observe(game_page(true), at(0.0)) == PlayStep::WAIT);
	CHECK(watch.observe(game_page(false), at(1.0)) == PlayStep::WAIT);
	CHECK(watch.observe(game_page(true), at(1.2)) == PlayStep::WAIT);
	CHECK(watch.observe(game_page(true), at(2.5)) == PlayStep::WAIT);
	CHECK(watch.observe(game_page(true), at(2.7)) == PlayStep::PRESS);
}

TEST_CASE("a press that takes is reported as pressed")
{
	PlayWatch watch(at(0.0));

	CHECK(watch.observe(game_page(true), at(0.0)) == PlayStep::WAIT);
	CHECK(watch.observe(game_page(true), at(1.5)) == PlayStep::PRESS);
	CHECK(watch.observe(game_page(false), at(2.0)) == PlayStep::PRESSED);
}

TEST_CASE("a press that doesn't take is tried once more")
{
	PlayWatch watch(at(0.0));

	CHECK(watch.observe(game_page(true), at(0.0)) == PlayStep::WAIT);
	CHECK(watch.observe(game_page(true), at(1.5)) == PlayStep::PRESS);
	CHECK(watch.observe(game_page(true), at(6.0)) == PlayStep::WAIT);
	CHECK(watch.observe(game_page(true), at(6.5)) == PlayStep::PRESS);
	CHECK(watch.presses() == 2);
	CHECK(watch.observe(game_page(true), at(11.5)) == PlayStep::PRESSED);
}

TEST_CASE("Play that never becomes ready is left to the user")
{
	PlayWatch watch(at(0.0));

	CHECK(watch.observe(between_screens(), at(29.0)) == PlayStep::WAIT);
	CHECK(watch.observe(game_page(false), at(30.0)) == PlayStep::NOT_FOUND);
}

TEST_CASE("an error on the sign-in form stops the wait for Play")
{
	PlayWatch watch(at(0.0));

	CHECK(watch.observe(login_form("Session expired"), at(1.0)) == PlayStep::LOGIN_ERROR);
}

TEST_CASE("an Update button where Play would be ends the wait once it has settled")
{
	PlayWatch        watch(at(0.0));
	const ClientView update_page{.window_shown = true, .update_shown = true};

	CHECK(watch.observe(update_page, at(1.0)) == PlayStep::WAIT);
	CHECK(watch.observe(update_page, at(2.5)) == PlayStep::UPDATE_NEEDED);
}

TEST_CASE("a client signed in on a page that needs an update still counts as signed in")
{
	FormWatch        watch;
	const ClientView update_page{.window_shown = true, .update_shown = true};

	CHECK(!watch.observe(update_page, at(0.0)));
	CHECK(watch.observe(update_page, at(3.0)) == FormWait::ALREADY_SIGNED_IN);
}

TEST_CASE("a finished step keeps the time it took while the running one counts up")
{
	LoginTimeline timeline(at(0.0), true);
	timeline.begin(LoginStep::START_CLIENT, at(0.0));
	timeline.begin(LoginStep::SIGN_IN, at(6.5));

	const TimedStep started = timeline.step(LoginStep::START_CLIENT, at(9.0));
	const TimedStep signing = timeline.step(LoginStep::SIGN_IN, at(9.0));

	CHECK(started.state == StepState::DONE);
	CHECK(started.seconds == 6.5f);
	CHECK(signing.state == StepState::RUNNING);
	CHECK(signing.seconds == 2.5f);
	CHECK(timeline.step(LoginStep::START_GAME, at(9.0)).state == StepState::WAITING);
	CHECK(timeline.total_seconds(at(9.0)) == 9.0f);
}

TEST_CASE("signing in again after a retry keeps counting from the first try")
{
	LoginTimeline timeline(at(0.0), true);
	timeline.begin(LoginStep::SIGN_IN, at(1.0));
	timeline.begin(LoginStep::SIGN_IN, at(4.0));

	CHECK(timeline.step(LoginStep::SIGN_IN, at(5.0)).seconds == 4.0f);
}

TEST_CASE("a finished login stops every clock and marks how its last step ended")
{
	LoginTimeline timeline(at(0.0), true);
	timeline.begin(LoginStep::START_CLIENT, at(0.0));
	timeline.begin(LoginStep::SIGN_IN, at(2.0));
	timeline.finish(StepState::FAILED, at(3.0));

	const TimedStep signing = timeline.step(LoginStep::SIGN_IN, at(60.0));

	CHECK(signing.state == StepState::FAILED);
	CHECK(signing.seconds == 1.0f);
	CHECK(timeline.step(LoginStep::START_CLIENT, at(60.0)).state == StepState::DONE);
	CHECK(timeline.total_seconds(at(60.0)) == 3.0f);
}

TEST_CASE("a login that only signs in has no step for starting the game")
{
	CHECK(LoginTimeline(at(0.0), false).step_count() == 2);
	CHECK(LoginTimeline(at(0.0), true).step_count() == 3);
}

TEST_CASE("a step reads as finished once it is done")
{
	CHECK(step_label(LoginStep::SIGN_IN, StepState::RUNNING) == "Signing in");
	CHECK(step_label(LoginStep::SIGN_IN, StepState::DONE) == "Signed in");
	CHECK(step_label(LoginStep::START_GAME, StepState::LEFT_TO_USER) == "Game not started");
}
