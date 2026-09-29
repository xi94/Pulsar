#include "core/animation.h"

#include <algorithm>
#include <cmath>

namespace {
bool g_enabled = true;
float g_speed = 1.0f;
}

float animation::ease_toward(float t_value, float t_target, float t_rate, float t_delta_seconds,
							 float t_settle_distance)
{
	if (!g_enabled) return t_target;

	const float eased = t_value + (t_target - t_value) * (1.0f - std::exp(-t_rate * g_speed * t_delta_seconds));

	return std::fabs(t_target - eased) < t_settle_distance ? t_target : eased;
}

float animation::spring_toward(float t_value, float &t_velocity, float t_target, float t_stiffness,
							   float t_damping_ratio, float t_delta_seconds, float t_settle_distance)
{
	constexpr float max_step_seconds = 1.0f / 240.0f;

	if (!g_enabled) {
		t_velocity = 0.0f;
		return t_target;
	}

	const float damping = 2.0f * t_damping_ratio * std::sqrt(t_stiffness);
	float remaining = t_delta_seconds * g_speed;
	float value = t_value;

	while (remaining > 0.0f) {
		const float step = std::min(remaining, max_step_seconds);
		const float acceleration = t_stiffness * (t_target - value) - damping * t_velocity;

		t_velocity += acceleration * step;
		value += t_velocity * step;
		remaining -= step;
	}

	if (std::fabs(t_target - value) < t_settle_distance && std::fabs(t_velocity) < t_settle_distance * 10.0f) {
		t_velocity = 0.0f;
		return t_target;
	}

	return value;
}

float animation::step_toward(float t_value, float t_target, float t_duration_seconds, float t_delta_seconds)
{
	if (!g_enabled || t_duration_seconds <= 0.0f) return t_target;

	const float step = t_delta_seconds * g_speed / t_duration_seconds;

	return t_value < t_target ? std::min(t_value + step, t_target) : std::max(t_value - step, t_target);
}

void animation::set_enabled(bool t_enabled)
{
	g_enabled = t_enabled;
}

void animation::set_speed(float t_speed)
{
	g_speed = t_speed;
}
