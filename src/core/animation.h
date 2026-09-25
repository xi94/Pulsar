#pragma once

namespace animation {

float ease_toward(float t_value, float t_target, float t_rate, float t_delta_seconds);

void set_enabled(bool t_enabled);
void set_speed(float t_speed);

}
