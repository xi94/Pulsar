#include "ui/widget.h"

#include <cassert>
#include <span>

#include "os/input.h"

namespace {
constexpr Vec2 K_MOUSE_OUTSIDE_WINDOW{-1.0f, -1.0f};

[[nodiscard]] auto deliver(Widget* t_widget, const os::InputEvent& t_event) -> bool
{
	switch (t_event.type) {
		case os::InputEventType::MouseDown:
			return t_widget->on_pointer_down(t_event.position);
		case os::InputEventType::MouseMove:
			return t_widget->on_pointer_move(t_event.position);
		case os::InputEventType::MouseUp:
			return t_widget->on_pointer_up(t_event.position);
		case os::InputEventType::RightClick:
			return t_widget->on_right_click(t_event.position);
		case os::InputEventType::MouseWheel:
			return t_widget->on_scroll(t_event.position, t_event.wheel_delta);
		case os::InputEventType::KeyDown:
			return t_widget->on_key_down(t_event.key);
		case os::InputEventType::Character:
			return t_widget->on_char(t_event.codepoint);
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
	for (u32 i = m_overlay_count; i > 0; i -= 1) {
		if (t_visitor(m_overlays[i - 1])) return true;
	}

	for (u32 i = m_widget_count; i > 0; i -= 1) {
		if (t_visitor(m_widgets[i - 1])) return true;
	}

	return false;
}

auto WidgetStack::update(Vec2 t_mouse, float t_delta_seconds) -> void
{
	bool covered_by_blocker = false;

	visit_top_down([&](Widget* t_widget) {
		t_widget->set_mouse(covered_by_blocker ? K_MOUSE_OUTSIDE_WINDOW : t_mouse);
		t_widget->update(t_delta_seconds);
		covered_by_blocker = covered_by_blocker || (t_widget->is_visible() && t_widget->is_blocking());

		return false;
	});
}

auto WidgetStack::draw(DrawList* t_draw_list) -> void
{
	for (Widget* widget : std::span{m_widgets, m_widget_count}) {
		if (widget->is_visible()) {
			widget->draw(t_draw_list);
		}
	}

	for (Widget* widget : std::span{m_overlays, m_overlay_count}) {
		if (widget->is_visible()) {
			widget->draw(t_draw_list);
		}
	}
}

auto WidgetStack::dispatch(const os::InputEvent& t_event) -> bool
{
	return visit_top_down([&](Widget* t_widget) { return t_widget->is_visible() && (deliver(t_widget, t_event) || t_widget->is_blocking()); });
}

auto WidgetStack::cursor() const -> CursorKind
{
	CursorKind wanted = CursorKind::Arrow;

	visit_top_down([&](Widget* t_widget) {
		if (t_widget->is_visible()) {
			wanted = t_widget->cursor();
		}

		return wanted != CursorKind::Arrow;
	});

	return wanted;
}
