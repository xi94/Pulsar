#pragma once

#include <cmath>

#include "core/types.h"

class Draggable {
  public:
	void begin(Vec2 t_point)
	{
		m_pressed = true;
		m_moved = false;
		m_start = t_point;
		m_current = t_point;
	}

	void update(Vec2 t_point)
	{
		constexpr float drag_threshold = 4.0f;

		if (!m_pressed) return;

		m_current = t_point;
		m_moved = m_moved || std::hypot(m_current.x - m_start.x, m_current.y - m_start.y) > drag_threshold;
	}

	void end()
	{
		m_pressed = false;
		m_moved = false;
	}

	bool is_pressed() const
	{
		return m_pressed;
	}

	bool has_moved() const
	{
		return m_moved;
	}

	float delta_x() const
	{
		return m_current.x - m_start.x;
	}

  private:
	bool m_pressed = false;
	bool m_moved = false;
	Vec2 m_start{};
	Vec2 m_current{};
};
