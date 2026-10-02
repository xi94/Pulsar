#pragma once

namespace animation {

constexpr float K_SETTLED_AMOUNT = 0.002f;
constexpr float K_SETTLED_PIXELS = 0.25f;

[[nodiscard]] auto ease_toward(float t_value, float t_target, float t_rate, float t_delta_seconds, float t_settle_distance = K_SETTLED_AMOUNT) -> float;

[[nodiscard]] auto
spring_toward(float t_value, float* t_velocity, float t_target, float t_stiffness, float t_damping_ratio, float t_delta_seconds, float t_settle_distance)
	-> float;

[[nodiscard]] auto step_toward(float t_value, float t_target, float t_duration_seconds, float t_delta_seconds) -> float;

auto request_frame() -> void;
auto request_frame_after(float t_seconds) -> void;
[[nodiscard]] auto take_idle_wait(float t_limit_seconds) -> float;

auto set_enabled(bool t_enabled) -> void;
auto set_speed(float t_speed) -> void;

}
