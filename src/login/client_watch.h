#pragma once

#include <chrono>
#include <optional>
#include <span>
#include <string>

#include "core/types.h"

using WatchClock = std::chrono::steady_clock;

// What the Riot Client shows at one moment. A login error holds Riot's own words, or an empty string when they couldn't be read.
struct ClientView {
	bool                       window_shown     = false;
	bool                       login_form_shown = false;
	std::optional<std::string> login_error;
	bool                       play_shown   = false;
	bool                       play_enabled = false;
	bool                       update_shown = false;
};

struct ElementBounds {
	i32 left   = 0;
	i32 top    = 0;
	i32 right  = 0;
	i32 bottom = 0;

	auto operator==(const ElementBounds&) const -> bool = default;
};

// The sign-in arrow has no name, so it is told apart from the other buttons on the form by what it does and where it sits.
struct FormButton {
	ElementBounds bounds;
	bool          named     = false;
	bool          invokable = false;
	bool          enabled   = false;
};

enum class FormWait : u8 {
	FORM_SHOWN,
	ALREADY_SIGNED_IN,
};

enum class SignInOutcome : u8 {
	SIGNED_IN,
	REFUSED,
	NO_RESPONSE,
	TIMED_OUT,
};

struct SignInResult {
	SignInOutcome outcome;
	std::string   riot_error;
};

enum class PlayStep : u8 {
	WAIT,
	PRESS,
	PRESSED,
	NOT_FOUND,
	UPDATE_NEEDED,
	LOGIN_ERROR,
};

class FormWatch {
  public:
	[[nodiscard]] auto observe(const ClientView& t_view, WatchClock::time_point t_now) -> std::optional<FormWait>;

  private:
	std::optional<WatchClock::time_point> m_signed_in_since;
};

class SignInWatch {
  public:
	SignInWatch(const ClientView& t_before_submit, WatchClock::time_point t_submitted);

	[[nodiscard]] auto observe(const ClientView& t_view, WatchClock::time_point t_now) -> std::optional<SignInResult>;

  private:
	std::optional<std::string>            m_stale_error;
	WatchClock::time_point                m_submitted;
	WatchClock::time_point                m_last_sign_of_life;
	std::optional<WatchClock::time_point> m_form_gone_since;
};

class PlayWatch {
  public:
	explicit PlayWatch(WatchClock::time_point t_started);

	[[nodiscard]] auto observe(const ClientView& t_view, WatchClock::time_point t_now) -> PlayStep;

	[[nodiscard]] auto presses() const -> u32
	{
		return m_presses;
	}

  private:
	auto press_now(WatchClock::time_point t_now) -> PlayStep;

	WatchClock::time_point                m_started;
	std::optional<WatchClock::time_point> m_ready_since;
	std::optional<WatchClock::time_point> m_update_shown_since;
	std::optional<WatchClock::time_point> m_last_press;
	u32                                   m_presses = 0;
};

// The arrow is the one button that turned enabled when the form was filled in. Failing that, it is the lowest unnamed button under the password box, inside its column.
[[nodiscard]] auto find_sign_in_arrow(std::span<const FormButton> t_before_filling, std::span<const FormButton> t_after_filling, ElementBounds t_password_field) -> std::optional<usize>;
