#pragma once

namespace animation {

constexpr float settled_amount = 0.002f;
constexpr float settled_pixels = 0.25f;

float ease_toward(float t_value, float t_target, float t_rate, float t_delta_seconds,
				  float t_settle_distance = settled_amount);

void set_enabled(bool t_enabled);
void set_speed(float t_speed);

}
