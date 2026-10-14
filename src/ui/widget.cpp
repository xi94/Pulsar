#include "ui/widget.h"

#include <cassert>
#include <ranges>
#include <span>

#include "os/input.h"

namespace {
constexpr Vec2 K_MOUSE_OUTSIDE_WINDOW{-1.0f, -1.0f};

[[nodiscard]] auto deliver(Widget* t_widget, const os::InputEvent& t_event) -> bool
{
	switch (t_event.type) {
		using enum os::InputEventType;

		case MOUSE_DOWN: {
			return t_widget->on_pointer_down(t_event.position);
		}

		case MOUSE_MOVE: {
			return t_widget->on_pointer_move(t_event.position);
		}

		case MOUSE_UP: {
			return t_widget->on_pointer_up(t_event.position);
		}

		case RIGHT_CLICK: {
			return t_widget->on_right_click(t_event.position);
		}

		case MOUSE_WHEEL: {
			return t_widget->on_scroll(t_event.position, t_event.wheel_delta);
		}

		case KEY_DOWN: {
			return t_widget->on_key_down(t_event.key);
		}

		case CHARACTER: {
			return t_widget->on_char(t_event.codepoint);
		}
	}

	return false;
}
}

auto WidgetStack::push(Widget* t_widget) -> void
{
	assert(m_widget_count < K_MAX_WIDGETS);

	m_widgets[m_widget_count] = t_widget;
	m_widget_count += 1;
}

auto WidgetStack::push_overlay(Widget* t_widget) -> void
{
	assert(m_overlay_count < K_MAX_WIDGETS);

	m_overlays[m_overlay_count] = t_widget;
	m_overlay_count += 1;
}

auto WidgetStack::visit_top_down(std::predicate<Widget*> auto&& t_visitor) const -> bool
{
	for (Widget* overlay : std::span{m_overlays, m_overlay_count} | std::views::reverse) {
		if (t_visitor(overlay)) return true;
	}

	for (Widget* widget : std::span{m_widgets, m_widget_count} | std::views::reverse) {
		if (t_visitor(widget)) return true;
	}

	return false;
}

auto WidgetStack::update(Vec2 t_mouse, float t_delta_seconds) -> void
{
	bool covered_by_blocker = false;

	visit_top_down([&](Widget* t_widget) {
		t_widget->m_mouse = covered_by_blocker ? K_MOUSE_OUTSIDE_WINDOW : t_mouse;
		t_widget->update(t_delta_seconds);
		covered_by_blocker = covered_by_blocker || (t_widget->m_visible && t_widget->is_blocking());

		return false;
	});
}

auto WidgetStack::draw(DrawList* t_draw_list) -> void
{
	for (Widget* widget : std::span{m_widgets, m_widget_count}) {
		if (widget->m_visible) {
			widget->draw(t_draw_list);
		}
	}

	for (Widget* widget : std::span{m_overlays, m_overlay_count}) {
		if (widget->m_visible) {
			widget->draw(t_draw_list);
		}
	}
}

auto WidgetStack::dispatch(const os::InputEvent& t_event) -> bool
{
	return visit_top_down([&](Widget* t_widget) { return t_widget->m_visible && (deliver(t_widget, t_event) || t_widget->is_blocking()); });
}

auto WidgetStack::cursor() const -> CursorKind
{
	CursorKind wanted = CursorKind::ARROW;

	visit_top_down([&](Widget* t_widget) {
		if (t_widget->m_visible) {
			wanted = t_widget->cursor();
		}

		return wanted != CursorKind::ARROW;
	});

	return wanted;
}
