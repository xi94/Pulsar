#pragma once

#include <cmath>

#include "core/types.h"

class Draggable {
  public:
	auto begin(Vec2 t_point) -> void
	{
		m_pressed = true;
		m_moved   = false;
		m_start   = t_point;
		m_current = t_point;
	}

	auto update(Vec2 t_point) -> void
	{
		constexpr float DRAG_THRESHOLD = 4.0f;

		if (!m_pressed) return;

		m_current = t_point;
		m_moved   = m_moved || std::hypot(m_current.x - m_start.x, m_current.y - m_start.y) > DRAG_THRESHOLD;
	}

	auto end() -> void
	{
		m_pressed = false;
		m_moved   = false;
	}

	[[nodiscard]] auto is_pressed() const -> bool
	{
		return m_pressed;
	}

	[[nodiscard]] auto has_moved() const -> bool
	{
		return m_moved;
	}

	[[nodiscard]] auto delta_x() const -> float
	{
		return m_current.x - m_start.x;
	}

  private:
	bool m_pressed = false;
	bool m_moved   = false;
	Vec2 m_start{};
	Vec2 m_current{};
};
