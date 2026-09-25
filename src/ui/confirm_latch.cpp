#include "ui/confirm_latch.h"

#include "core/animation.h"

namespace {
constexpr float arm_ease_rate = 18.0f;
}

bool ConfirmLatch::confirm(i32 t_target)
{
	if (is_armed(t_target)) {
		disarm();
		return true;
	}

	m_target = t_target;
	m_remaining_seconds = window_seconds;

	return false;
}

void ConfirmLatch::disarm()
{
	m_target = -1;
	m_remaining_seconds = 0.0f;
}

void ConfirmLatch::update(float t_delta_seconds)
{
	if (is_armed()) {
		m_remaining_seconds -= t_delta_seconds;

		if (m_remaining_seconds <= 0.0f) {
			disarm();
		}
	}

	m_armed_amount = animation::ease_toward(m_armed_amount, is_armed() ? 1.0f : 0.0f, arm_ease_rate, t_delta_seconds);
}
