#pragma once

#include "core/types.h"

class DrawList;

constexpr float scrollbar_width = 8.0f;

struct ScrollGeometry {
	Rect track;
	float content_height;
	float visible_height;
};

class Scrollable {
  public:
	static bool is_needed(const ScrollGeometry &t_geometry);

	void update(float t_delta_seconds);
	void draw(DrawList &t_draw_list, const ScrollGeometry &t_geometry, Vec2 t_mouse, u8 t_alpha) const;
	struct EdgeFades {
		Rect top;
		Rect bottom;
	};

	EdgeFades edge_fades(Rect t_area, const ScrollGeometry &t_geometry) const;
	void draw_edge_fade(DrawList &t_draw_list, Rect t_area, const ScrollGeometry &t_geometry, Color t_edge) const;

	bool on_pointer_down(Vec2 t_point, const ScrollGeometry &t_geometry);
	void on_pointer_move(float t_y, const ScrollGeometry &t_geometry);
	void on_pointer_up();
	void on_scroll(float t_wheel_delta, const ScrollGeometry &t_geometry);

	void scroll_by(float t_pixels, const ScrollGeometry &t_geometry);
	void jump_to(float t_offset, const ScrollGeometry &t_geometry);
	void reveal(float t_top, float t_bottom, float t_view_top, float t_view_bottom, const ScrollGeometry &t_geometry);

	bool is_dragging() const
	{
		return m_dragging;
	}

	bool is_over_track(Vec2 t_point, const ScrollGeometry &t_geometry) const
	{
		return is_needed(t_geometry) && t_geometry.track.contains(t_point);
	}

	float offset() const;

  private:
	float m_offset = 0.0f;
	float m_target = 0.0f;
	bool m_dragging = false;
	float m_drag_start_y = 0.0f;
	float m_drag_start_target = 0.0f;
};
