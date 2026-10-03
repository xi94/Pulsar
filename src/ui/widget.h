#pragma once

#include <concepts>

#include "core/types.h"
#include "os/input.h"

class DrawList;

class Widget {
  public:
	virtual ~Widget() = default;

	virtual auto update(float) -> void {}

	virtual auto draw(DrawList* t_draw_list) -> void = 0;

	virtual auto on_pointer_down(Vec2) -> bool
	{
		return false;
	}

	virtual auto on_pointer_move(Vec2) -> bool
	{
		return false;
	}

	virtual auto on_pointer_up(Vec2) -> bool
	{
		return false;
	}

	virtual auto on_right_click(Vec2) -> bool
	{
		return false;
	}

	virtual auto on_scroll(Vec2, float) -> bool
	{
		return false;
	}

	virtual auto on_key_down(os::Key) -> bool
	{
		return false;
	}

	virtual auto on_char(u32) -> bool
	{
		return false;
	}

	[[nodiscard]] virtual auto is_blocking() const -> bool
	{
		return false;
	}

	[[nodiscard]] virtual auto cursor() const -> CursorKind
	{
		return CursorKind::Arrow;
	}

	auto set_mouse(Vec2 t_mouse) -> void
	{
		m_mouse = t_mouse;
	}

	[[nodiscard]] auto is_visible() const -> bool
	{
		return m_visible;
	}

	auto set_visible(bool t_visible) -> void
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
	auto push(Widget* t_widget) -> void;
	auto push_overlay(Widget* t_widget) -> void;

	auto update(Vec2 t_mouse, float t_delta_seconds) -> void;
	auto draw(DrawList* t_draw_list) -> void;

	[[nodiscard]] auto dispatch(const os::InputEvent& t_event) -> bool;
	[[nodiscard]] auto cursor() const -> CursorKind;

  private:
	static constexpr u32 K_MAX_WIDGETS = 16;

	auto visit_top_down(std::predicate<Widget*> auto&& t_visitor) const -> bool;

	Widget* m_widgets[K_MAX_WIDGETS]{};
	u32     m_widget_count = 0;
	Widget* m_overlays[K_MAX_WIDGETS]{};
	u32     m_overlay_count = 0;
};
