#pragma once

#include "core/types.h"

constexpr u32 K_CARET_TRAIL_CORNERS = 8;

struct CaretTrail {
	Vec2  points[K_CARET_TRAIL_CORNERS]{};
	Color colors[K_CARET_TRAIL_CORNERS]{};
	u32   count = 0;
};

auto set_caret_style(CaretStyle t_style, bool t_trail, float t_trail_strength) -> void;
[[nodiscard]] auto caret_style() -> CaretStyle;

class CaretMotion {
  public:
	auto restart() -> void;
	auto let_go() -> void;
	auto follow(Vec2 t_target, Vec2 t_origin, float t_line_height, float t_seconds, bool t_focused) -> void;

	[[nodiscard]] auto head() const -> Vec2
	{
		return m_head;
	}

	[[nodiscard]] auto tail() const -> Vec2
	{
		return m_tail;
	}

	[[nodiscard]] auto is_settled() const -> bool;
	[[nodiscard]] auto trail(Vec2 t_origin, Vec2 t_size, Color t_color) const -> CaretTrail;

  private:
	Vec2  m_head{};
	Vec2  m_tail{};
	Vec2  m_target{};
	float m_rate   = 0.0f;
	bool  m_placed = false;
};
