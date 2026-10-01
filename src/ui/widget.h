#pragma once

#include "core/types.h"

class DrawList;
struct InputEvent;

class Widget {
  public:
	virtual ~Widget() = default;

	virtual void update(float) {}

	virtual void draw(DrawList &t_draw_list) = 0;

	virtual bool on_pointer_down(Vec2)
	{
		return false;
	}

	virtual bool on_pointer_move(Vec2)
	{
		return false;
	}

	virtual bool on_pointer_up(Vec2)
	{
		return false;
	}

	virtual bool on_right_click(Vec2)
	{
		return false;
	}

	virtual bool on_scroll(Vec2, float)
	{
		return false;
	}

	virtual bool on_key_down(u32)
	{
		return false;
	}

	virtual bool on_char(u32)
	{
		return false;
	}

	virtual bool is_blocking() const
	{
		return false;
	}

	virtual CursorKind cursor() const
	{
		return CursorKind::Arrow;
	}

	void set_mouse(Vec2 t_mouse)
	{
		m_mouse = t_mouse;
	}

	bool is_visible() const
	{
		return m_visible;
	}

	void set_visible(bool t_visible)
	{
		m_visible = t_visible;
	}

  protected:
	Vec2 m_mouse{-1.0f, -1.0f};

  private:
	bool m_visible = true;
};

class WidgetStack {
  public:
	void push(Widget &t_widget);
	void push_overlay(Widget &t_widget);

	void update(Vec2 t_mouse, float t_delta_seconds);
	void draw(DrawList &t_draw_list);

	bool dispatch(const InputEvent &t_event);
	CursorKind cursor() const;

  private:
	static constexpr u32 max_widgets = 16;

	template <typename Visitor>
	bool visit_top_down(Visitor &&t_visitor) const;

	Widget *m_widgets[max_widgets]{};
	u32 m_widget_count = 0;
	Widget *m_overlays[max_widgets]{};
	u32 m_overlay_count = 0;
};
