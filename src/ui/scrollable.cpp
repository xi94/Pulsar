#include "ui/scrollable.h"

#include <algorithm>

#include "core/animation.h"
#include "gfx/draw_list.h"
#include "ui/theme.h"

namespace {
constexpr float ease_rate = 16.0f;
constexpr float pixels_per_notch = 48.0f;
constexpr float min_thumb_height = 24.0f;
constexpr float thumb_grab_margin = 4.0f;
constexpr float edge_fade_height = 28.0f;
constexpr float thumb_hover_strength = 0.3f;

float max_offset(const ScrollGeometry &t_geometry)
{
	return std::max(0.0f, t_geometry.content_height - t_geometry.visible_height);
}

float thumb_height(const ScrollGeometry &t_geometry)
{
	return std::max(min_thumb_height, t_geometry.track.h * t_geometry.visible_height / t_geometry.content_height);
}

Rect thumb_rect(float t_offset, const ScrollGeometry &t_geometry)
{
	const float scrollable = max_offset(t_geometry);
	const float height = thumb_height(t_geometry);
	const float progress = scrollable > 0.0f ? std::clamp(t_offset / scrollable, 0.0f, 1.0f) : 0.0f;
	const Rect &track = t_geometry.track;

	return Rect{track.x, track.y + (track.h - height) * progress, track.w, height};
}

Rect thumb_grab_rect(float t_offset, const ScrollGeometry &t_geometry)
{
	return thumb_rect(t_offset, t_geometry).inset(-thumb_grab_margin, 0.0f);
}
}

bool Scrollable::is_needed(const ScrollGeometry &t_geometry)
{
	return t_geometry.content_height > t_geometry.visible_height + 0.5f;
}

float Scrollable::offset() const
{
	return snapped_to_pixel(m_offset);
}

void Scrollable::update(float t_delta_seconds)
{
	m_offset = animation::ease_toward(m_offset, m_target, ease_rate, t_delta_seconds, animation::settled_pixels);
}

void Scrollable::draw(DrawList &t_draw_list, const ScrollGeometry &t_geometry, Vec2 t_mouse, u8 t_alpha) const
{
	if (!is_needed(t_geometry)) return;

	const Theme &colors = theme();
	const Rect &track = t_geometry.track;
	const Rect thumb = thumb_rect(m_offset, t_geometry);
	const bool hovered = m_dragging || thumb_grab_rect(m_offset, t_geometry).contains(t_mouse);
	const Color hovered_thumb =
		with_alpha(mix(colors.scroll_thumb, colors.text, thumb_hover_strength), colors.scroll_thumb.a);

	t_draw_list.add_rounded_rect(track, rounded(track.w * 0.5f), faded(colors.separator, t_alpha));
	t_draw_list.add_rounded_rect(thumb, rounded(thumb.w * 0.5f),
								 faded(hovered ? hovered_thumb : colors.scroll_thumb, t_alpha));
}

Scrollable::EdgeFades Scrollable::edge_fades(Rect t_area, const ScrollGeometry &t_geometry) const
{
	const Rect none{t_area.x, t_area.y, 0.0f, 0.0f};
	if (!is_needed(t_geometry)) return EdgeFades{none, none};

	const float height = std::min(edge_fade_height, t_area.h * 0.5f);
	const bool top = m_offset > 0.5f;
	const bool bottom = m_offset < max_offset(t_geometry) - 0.5f;

	return EdgeFades{top ? Rect{t_area.x, t_area.y, t_area.w, height} : none,
					 bottom ? Rect{t_area.x, t_area.bottom() - height, t_area.w, height} : none};
}

void Scrollable::draw_edge_fade(DrawList &t_draw_list, Rect t_area, const ScrollGeometry &t_geometry,
								Color t_edge) const
{
	const EdgeFades fades = edge_fades(t_area, t_geometry);
	const Color clear = faded(t_edge, 0);

	t_draw_list.push_clip(t_area);

	if (fades.top.h > 0.0f) {
		t_draw_list.add_gradient(fades.top, t_edge, t_edge, clear, clear);
	}

	if (fades.bottom.h > 0.0f) {
		t_draw_list.add_gradient(fades.bottom, clear, clear, t_edge, t_edge);
	}

	t_draw_list.pop_clip();
}

bool Scrollable::on_pointer_down(Vec2 t_point, const ScrollGeometry &t_geometry)
{
	if (!is_needed(t_geometry) || !thumb_grab_rect(m_target, t_geometry).contains(t_point)) return false;

	m_dragging = true;
	m_drag_start_y = t_point.y;
	m_drag_start_target = m_target;

	return true;
}

void Scrollable::on_pointer_move(float t_y, const ScrollGeometry &t_geometry)
{
	if (!m_dragging) return;

	const float travel = t_geometry.track.h - thumb_height(t_geometry);
	const float scrollable = max_offset(t_geometry);
	if (travel <= 0.0f || scrollable <= 0.0f) return;

	m_target = std::clamp(m_drag_start_target + (t_y - m_drag_start_y) * scrollable / travel, 0.0f, scrollable);
	m_offset = m_target;
}

void Scrollable::on_pointer_up()
{
	m_dragging = false;
}

void Scrollable::on_scroll(float t_wheel_delta, const ScrollGeometry &t_geometry)
{
	scroll_by(-t_wheel_delta * pixels_per_notch, t_geometry);
}

void Scrollable::scroll_by(float t_pixels, const ScrollGeometry &t_geometry)
{
	m_target = std::clamp(m_target + t_pixels, 0.0f, max_offset(t_geometry));
}

void Scrollable::jump_to(float t_offset, const ScrollGeometry &t_geometry)
{
	m_target = std::clamp(t_offset, 0.0f, max_offset(t_geometry));
	m_offset = m_target;
}

void Scrollable::reveal(float t_top, float t_bottom, float t_view_top, float t_view_bottom,
						const ScrollGeometry &t_geometry)
{
	const float settle_shift = m_offset - m_target;
	const float hidden_above = t_view_top - (t_top + settle_shift);
	const float hidden_below = (t_bottom + settle_shift) - t_view_bottom;

	if (hidden_above > 0.0f) {
		scroll_by(-hidden_above, t_geometry);
	} else if (hidden_below > 0.0f) {
		scroll_by(hidden_below, t_geometry);
	}
}
