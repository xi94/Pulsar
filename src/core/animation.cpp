#include "core/animation.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
// The speed slider's 1.0x plays every animation this much faster than its written rate; at the bare rates the UI felt sluggish.
constexpr float K_BASE_SPEED = 1.5f;

bool  g_enabled         = true;
float g_speed           = K_BASE_SPEED;
bool  g_frame_requested = false;
float g_wake_after      = std::numeric_limits<float>::max();
}

auto animation::ease_toward(float t_value, float t_target, float t_rate, float t_delta_seconds, float t_settle_distance) -> float
{
	if (!g_enabled) return t_target;

	const float eased = t_value + (t_target - t_value) * (1.0f - std::exp(-t_rate * g_speed * t_delta_seconds));

	if (std::fabs(t_target - eased) < t_settle_distance) return t_target;

	request_frame();

	return eased;
}

auto animation::spring_toward(float  t_value,
                              float* t_velocity,
                              float  t_target,
                              float  t_stiffness,
                              float  t_damping_ratio,
                              float  t_delta_seconds,
                              float  t_settle_distance) -> float
{
	constexpr float MAX_STEP_SECONDS = 1.0f / 240.0f;

	if (!g_enabled) {
		*t_velocity = 0.0f;
		return t_target;
	}

	const float damping   = 2.0f * t_damping_ratio * std::sqrt(t_stiffness);
	float       remaining = t_delta_seconds * g_speed;
	float       value     = t_value;
	float       velocity  = *t_velocity;

	while (remaining > 0.0f) {
		const float step         = std::min(remaining, MAX_STEP_SECONDS);
		const float acceleration = t_stiffness * (t_target - value) - damping * velocity;

		velocity += acceleration * step;
		value += velocity * step;
		remaining -= step;
	}

	if (std::fabs(t_target - value) < t_settle_distance && std::fabs(velocity) < t_settle_distance * 10.0f) {
		*t_velocity = 0.0f;
		return t_target;
	}

	*t_velocity = velocity;
	request_frame();

	return value;
}

auto animation::step_toward(float t_value, float t_target, float t_duration_seconds, float t_delta_seconds) -> float
{
	if (!g_enabled || t_duration_seconds <= 0.0f) return t_target;

	const float step = t_delta_seconds * g_speed / t_duration_seconds;

	const float stepped = t_value < t_target ? std::min(t_value + step, t_target) : std::max(t_value - step, t_target);
	if (stepped != t_target) {
		request_frame();
	}

	return stepped;
}

auto animation::request_frame() -> void
{
	g_frame_requested = true;
}

auto animation::request_frame_after(float t_seconds) -> void
{
	g_wake_after = std::min(g_wake_after, std::max(t_seconds, 0.0f));
}

auto animation::take_idle_wait(float t_limit_seconds) -> float
{
	const float wait = g_frame_requested ? 0.0f : std::min(g_wake_after, t_limit_seconds);

	g_frame_requested = false;
	g_wake_after      = std::numeric_limits<float>::max();

	return wait;
}

auto animation::set_enabled(bool t_enabled) -> void
{
	g_enabled = t_enabled;
}

auto animation::is_enabled() -> bool
{
	return g_enabled;
}

auto animation::set_speed(float t_speed) -> void
{
	g_speed = t_speed * K_BASE_SPEED;
}
