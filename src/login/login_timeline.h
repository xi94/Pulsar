#pragma once

#include <chrono>
#include <optional>
#include <string_view>

#include "core/enum_array.h"
#include "core/types.h"
#include "login/client_watch.h"

enum class LoginStep : u8 {
	START_CLIENT,
	SIGN_IN,
	START_GAME,
	COUNT,
};

enum class StepState : u8 {
	WAITING,
	RUNNING,
	DONE,
	FAILED,
	LEFT_TO_USER,
};

struct TimedStep {
	StepState state   = StepState::WAITING;
	float     seconds = 0.0f;
};

class LoginTimeline {
  public:
	LoginTimeline() = default;
	LoginTimeline(WatchClock::time_point t_started, bool t_starts_game);

	auto begin(LoginStep t_step, WatchClock::time_point t_now) -> void;
	auto finish(StepState t_outcome_of_running_step, WatchClock::time_point t_now) -> void;

	[[nodiscard]] auto step_count() const -> u32;
	[[nodiscard]] auto step(LoginStep t_step, WatchClock::time_point t_now) const -> TimedStep;
	[[nodiscard]] auto total_seconds(WatchClock::time_point t_now) const -> float;

  private:
	[[nodiscard]] auto end_of(LoginStep t_step, WatchClock::time_point t_now) const -> WatchClock::time_point;

	WatchClock::time_point                                      m_started{};
	bool                                                        m_starts_game = true;
	EnumArray<LoginStep, std::optional<WatchClock::time_point>> m_began{};
	std::optional<LoginStep>                                    m_running;
	std::optional<WatchClock::time_point>                       m_finished;
	StepState                                                   m_outcome = StepState::RUNNING;
};

[[nodiscard]] auto step_label(LoginStep t_step, StepState t_state) -> std::string_view;
