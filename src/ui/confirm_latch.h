#pragma once

#include "core/types.h"

class ConfirmLatch {
  public:
	static constexpr float window_seconds = 2.5f;

	bool confirm(i32 t_target);
	void disarm();
	void update(float t_delta_seconds);

	bool is_armed(i32 t_target) const
	{
		return m_target >= 0 && m_target == t_target;
	}

	bool is_armed() const
	{
		return m_target >= 0;
	}

	float armed_amount(i32 t_target) const
	{
		return is_armed(t_target) ? m_armed_amount : 0.0f;
	}

  private:
	i32 m_target = -1;
	float m_remaining_seconds = 0.0f;
	float m_armed_amount = 0.0f;
};
