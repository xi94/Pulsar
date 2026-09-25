#include "core/animation.h"

#include <cmath>

namespace {
constexpr float settled_distance = 0.002f;

bool g_enabled = true;
float g_speed = 1.0f;
}

float animation::ease_toward(float t_value, float t_target, float t_rate, float t_delta_seconds)
{
	if (!g_enabled) return t_target;

	const float eased = t_value + (t_target - t_value) * (1.0f - std::exp(-t_rate * g_speed * t_delta_seconds));

	return std::fabs(t_target - eased) < settled_distance ? t_target : eased;
}

void animation::set_enabled(bool t_enabled)
{
	g_enabled = t_enabled;
}

void animation::set_speed(float t_speed)
{
	g_speed = t_speed;
}
