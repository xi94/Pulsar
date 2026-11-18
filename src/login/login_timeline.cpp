#include "login/login_timeline.h"

namespace {
struct StepLabels {
	std::string_view underway;
	std::string_view done;
	std::string_view not_done;
};

constexpr EnumArray<LoginStep, StepLabels> K_STEP_LABELS{{
	{"Starting Riot Client", "Riot Client started", "Riot Client not started"},
	{"Signing in", "Signed in", "Not signed in"},
	{"Starting game", "Game started", "Game not started"},
}};

[[nodiscard]] auto seconds_between(WatchClock::time_point t_from, WatchClock::time_point t_to) -> float
{
	return static_cast<float>(std::chrono::duration<double>(t_to - t_from).count());
}
}

LoginTimeline::LoginTimeline(WatchClock::time_point t_started, bool t_starts_game)
	: m_started(t_started)
	, m_starts_game(t_starts_game)
{
}

auto LoginTimeline::begin(LoginStep t_step, WatchClock::time_point t_now) -> void
{
	if (m_running == t_step || m_finished) return;

	if (!m_began[t_step]) {
		m_began[t_step] = t_now;
	}

	m_running = t_step;
}

auto LoginTimeline::finish(StepState t_outcome_of_running_step, WatchClock::time_point t_now) -> void
{
	if (m_finished) return;

	m_finished = t_now;
	m_outcome  = t_outcome_of_running_step;
}

auto LoginTimeline::step_count() const -> u32
{
	return m_starts_game ? 3 : 2;
}

auto LoginTimeline::step(LoginStep t_step, WatchClock::time_point t_now) const -> TimedStep
{
	if (!m_began[t_step]) return TimedStep{};

	const float seconds = seconds_between(*m_began[t_step], end_of(t_step, t_now));

	if (t_step != m_running) return TimedStep{.state = StepState::DONE, .seconds = seconds};

	return TimedStep{.state = m_finished ? m_outcome : StepState::RUNNING, .seconds = seconds};
}

auto LoginTimeline::total_seconds(WatchClock::time_point t_now) const -> float
{
	return seconds_between(m_started, m_finished.value_or(t_now));
}

auto LoginTimeline::end_of(LoginStep t_step, WatchClock::time_point t_now) const -> WatchClock::time_point
{
	for (const LoginStep later : each<LoginStep>()) {
		if (later > t_step && m_began[later]) return *m_began[later];
	}

	return m_finished.value_or(t_now);
}

auto step_label(LoginStep t_step, StepState t_state) -> std::string_view
{
	const StepLabels& labels = K_STEP_LABELS[t_step];

	if (t_state == StepState::DONE) return labels.done;
	if (t_state == StepState::LEFT_TO_USER || t_state == StepState::FAILED) return labels.not_done;

	return labels.underway;
}
