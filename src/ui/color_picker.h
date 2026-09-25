#pragma once

#include "core/types.h"
#include "ui/draggable.h"

class DrawList;

class ColorPicker {
  public:
	void open(Color t_initial, Rect t_anchor, Vec2 t_window_size);
	void close();

	bool is_open() const
	{
		return m_open;
	}

	bool is_dragging() const
	{
		return m_saturation_value_drag.is_pressed() || m_hue_drag.is_pressed() || m_alpha_drag.is_pressed();
	}

	Color color() const;

	bool on_pointer_down(Vec2 t_point);
	void on_pointer_move(Vec2 t_point);
	void on_pointer_up();

	CursorKind cursor(Vec2 t_mouse) const;
	void draw(DrawList &t_draw_list) const;

  private:
	Rect popup_rect() const;
	void end_drags();

	bool m_open = false;
	Rect m_anchor{};
	Vec2 m_window_size{};

	float m_hue = 0.0f;
	float m_saturation = 0.0f;
	float m_value = 0.0f;
	u8 m_alpha = 255;

	Draggable m_saturation_value_drag;
	Draggable m_hue_drag;
	Draggable m_alpha_drag;
};
