#pragma once

namespace animation {

constexpr float settled_amount = 0.002f;
constexpr float settled_pixels = 0.25f;

float ease_toward(float t_value, float t_target, float t_rate, float t_delta_seconds,
				  float t_settle_distance = settled_amount);

float spring_toward(float t_value, float &t_velocity, float t_target, float t_stiffness, float t_damping_ratio,
					float t_delta_seconds, float t_settle_distance);

float step_toward(float t_value, float t_target, float t_duration_seconds, float t_delta_seconds);

void request_frame();
void request_frame_after(float t_seconds);
float take_idle_wait(float t_limit_seconds);

void set_enabled(bool t_enabled);
void set_speed(float t_speed);

}
