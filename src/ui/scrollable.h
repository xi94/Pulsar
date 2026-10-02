#pragma once

#include "core/types.h"

class DrawList;

constexpr float K_SCROLLBAR_WIDTH = 8.0f;

struct ScrollGeometry {
	Rect  track;
	float content_height;
	float visible_height;
};

class Scrollable {
  public:
	[[nodiscard]] static auto is_needed(const ScrollGeometry& t_geometry) -> bool;

	auto update(float t_delta_seconds) -> void;
	auto draw(DrawList* t_draw_list, const ScrollGeometry& t_geometry, Vec2 t_mouse, u8 t_alpha) const -> void;
	struct EdgeFades {
		Rect top;
		Rect bottom;
	};

	[[nodiscard]] auto edge_fades(Rect t_area, const ScrollGeometry& t_geometry) const -> EdgeFades;
	auto draw_edge_fade(DrawList* t_draw_list, Rect t_area, const ScrollGeometry& t_geometry, Color t_edge) const -> void;

	auto on_pointer_down(Vec2 t_point, const ScrollGeometry& t_geometry) -> bool;
	auto on_pointer_move(float t_y, const ScrollGeometry& t_geometry) -> void;
	auto on_pointer_up() -> void;
	auto on_scroll(float t_wheel_delta, const ScrollGeometry& t_geometry) -> void;

	auto scroll_by(float t_pixels, const ScrollGeometry& t_geometry) -> void;
	auto jump_to(float t_offset, const ScrollGeometry& t_geometry) -> void;
	auto reveal(float t_top, float t_bottom, float t_view_top, float t_view_bottom, const ScrollGeometry& t_geometry) -> void;

	[[nodiscard]] auto is_dragging() const -> bool
	{
		return m_dragging;
	}

	[[nodiscard]] auto is_over_track(Vec2 t_point, const ScrollGeometry& t_geometry) const -> bool
	{
		return is_needed(t_geometry) && t_geometry.track.contains(t_point);
	}

	[[nodiscard]] auto offset() const -> float;

  private:
	float        m_offset            = 0.0f;
	float        m_target            = 0.0f;
	bool         m_dragging          = false;
	float        m_drag_start_y      = 0.0f;
	float        m_drag_start_target = 0.0f;
	float        m_thickness         = 0.0f;
	float        m_activity_seconds  = 0.0f;
	mutable bool m_track_hovered     = false;
	bool         m_was_held          = false;
};
