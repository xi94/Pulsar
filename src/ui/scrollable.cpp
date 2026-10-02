#include "ui/scrollable.h"

#include <algorithm>

#include "core/animation.h"
#include "gfx/draw_list.h"
#include "ui/theme.h"

namespace {
constexpr float K_EASE_RATE               = 16.0f;
constexpr float K_PIXELS_PER_NOTCH        = 48.0f;
constexpr float K_MIN_THUMB_HEIGHT        = 24.0f;
constexpr float K_THUMB_GRAB_MARGIN       = 4.0f;
constexpr float K_EDGE_FADE_HEIGHT        = 28.0f;
constexpr float K_THUMB_HOVER_STRENGTH    = 0.3f;
constexpr float K_THIN_WIDTH              = 3.0f;
constexpr float K_GROW_EASE_RATE          = 18.0f;
constexpr float K_SHRINK_EASE_RATE        = 9.0f;
constexpr float K_SCROLL_ACTIVITY_SECONDS = 0.9f;
constexpr float K_RELEASE_LINGER_SECONDS  = 0.35f;
constexpr float K_MAX_ANIMATION_STEP      = 1.0f / 30.0f;

[[nodiscard]] auto max_offset(const ScrollGeometry& t_geometry) -> float
{
	return std::max(0.0f, t_geometry.content_height - t_geometry.visible_height);
}

[[nodiscard]] auto thumb_height(const ScrollGeometry& t_geometry) -> float
{
	return std::max(K_MIN_THUMB_HEIGHT, t_geometry.track.h * t_geometry.visible_height / t_geometry.content_height);
}

[[nodiscard]] auto thumb_rect(float t_offset, const ScrollGeometry& t_geometry) -> Rect
{
	const float scrollable = max_offset(t_geometry);
	const float height     = thumb_height(t_geometry);
	const float progress   = scrollable > 0.0f ? std::clamp(t_offset / scrollable, 0.0f, 1.0f) : 0.0f;
	const Rect& track      = t_geometry.track;

	return Rect{track.x, track.y + (track.h - height) * progress, track.w, height};
}

[[nodiscard]] auto thumb_grab_rect(float t_offset, const ScrollGeometry& t_geometry) -> Rect
{
	return thumb_rect(t_offset, t_geometry).inset(-K_THUMB_GRAB_MARGIN, 0.0f);
}
}

auto Scrollable::is_needed(const ScrollGeometry& t_geometry) -> bool
{
	return t_geometry.content_height > t_geometry.visible_height + 0.5f;
}

auto Scrollable::offset() const -> float
{
	return snapped_to_pixel(m_offset);
}

auto Scrollable::update(float t_delta_seconds) -> void
{
	// A frame after an idle wait carries the whole wait, which would finish an ease in one jump.
	const float step = std::min(t_delta_seconds, K_MAX_ANIMATION_STEP);
	m_offset         = animation::ease_toward(m_offset, m_target, K_EASE_RATE, step, animation::K_SETTLED_PIXELS);

	const bool held = m_dragging || m_track_hovered;
	if (m_was_held && !held) {
		m_activity_seconds = std::max(m_activity_seconds, K_RELEASE_LINGER_SECONDS);
	}

	m_was_held         = held;
	m_activity_seconds = std::max(0.0f, m_activity_seconds - t_delta_seconds);

	if (m_activity_seconds > 0.0f) {
		animation::request_frame_after(m_activity_seconds);
	}

	const bool active = held || m_activity_seconds > 0.0f;
	m_thickness       = animation::ease_toward(m_thickness, active ? 1.0f : 0.0f, active ? K_GROW_EASE_RATE : K_SHRINK_EASE_RATE, step);
}

auto Scrollable::draw(DrawList* t_draw_list, const ScrollGeometry& t_geometry, Vec2 t_mouse, u8 t_alpha) const -> void
{
	const bool track_hovered = is_needed(t_geometry) && t_geometry.track.inset(-K_THUMB_GRAB_MARGIN, 0.0f).contains(t_mouse);

	if (track_hovered != m_track_hovered) {
		m_track_hovered = track_hovered;
		animation::request_frame();
	}

	if (!is_needed(t_geometry)) return;

	const float width         = std::min(t_geometry.track.w, K_THIN_WIDTH + (t_geometry.track.w - K_THIN_WIDTH) * m_thickness);
	const float inset         = (t_geometry.track.w - width) * 0.5f;
	const Rect  track         = t_geometry.track.inset(inset, 0.0f);
	const Rect  thumb         = thumb_rect(m_offset, t_geometry).inset(inset, 0.0f);
	const bool  hovered       = m_dragging || thumb_grab_rect(m_offset, t_geometry).contains(t_mouse);
	const Color hovered_thumb = with_alpha(mix(g_theme.scroll_thumb, g_theme.text, K_THUMB_HOVER_STRENGTH), g_theme.scroll_thumb.a);

	if (m_thickness > 0.001f) {
		t_draw_list->add_rounded_rect(track, rounded(track.w * 0.5f), faded(g_theme.separator, static_cast<u8>(static_cast<float>(t_alpha) * m_thickness)));
	}

	t_draw_list->add_rounded_rect(thumb, rounded(thumb.w * 0.5f), faded(hovered ? hovered_thumb : g_theme.scroll_thumb, t_alpha));
}

auto Scrollable::edge_fades(Rect t_area, const ScrollGeometry& t_geometry) const -> Scrollable::EdgeFades
{
	const Rect none{t_area.x, t_area.y, 0.0f, 0.0f};
	if (!is_needed(t_geometry)) return EdgeFades{none, none};

	const float height = std::min(K_EDGE_FADE_HEIGHT, t_area.h * 0.5f);
	const bool  top    = m_offset > 0.5f;
	const bool  bottom = m_offset < max_offset(t_geometry) - 0.5f;

	return EdgeFades{top ? Rect{t_area.x, t_area.y, t_area.w, height} : none, bottom ? Rect{t_area.x, t_area.bottom() - height, t_area.w, height} : none};
}

auto Scrollable::draw_edge_fade(DrawList* t_draw_list, Rect t_area, const ScrollGeometry& t_geometry, Color t_edge) const -> void
{
	const EdgeFades fades = edge_fades(t_area, t_geometry);
	const Color     clear = faded(t_edge, 0);

	t_draw_list->push_clip(t_area);

	if (fades.top.h > 0.0f) {
		t_draw_list->add_gradient(fades.top, t_edge, t_edge, clear, clear);
	}

	if (fades.bottom.h > 0.0f) {
		t_draw_list->add_gradient(fades.bottom, clear, clear, t_edge, t_edge);
	}

	t_draw_list->pop_clip();
}

auto Scrollable::on_pointer_down(Vec2 t_point, const ScrollGeometry& t_geometry) -> bool
{
	if (!is_needed(t_geometry) || !thumb_grab_rect(m_target, t_geometry).contains(t_point)) return false;

	m_dragging          = true;
	m_drag_start_y      = t_point.y;
	m_drag_start_target = m_target;

	return true;
}

auto Scrollable::on_pointer_move(float t_y, const ScrollGeometry& t_geometry) -> void
{
	if (!m_dragging) return;

	const float travel     = t_geometry.track.h - thumb_height(t_geometry);
	const float scrollable = max_offset(t_geometry);
	if (travel <= 0.0f || scrollable <= 0.0f) return;

	m_target = std::clamp(m_drag_start_target + (t_y - m_drag_start_y) * scrollable / travel, 0.0f, scrollable);
	m_offset = m_target;
}

auto Scrollable::on_pointer_up() -> void
{
	m_dragging = false;
}

auto Scrollable::on_scroll(float t_wheel_delta, const ScrollGeometry& t_geometry) -> void
{
	scroll_by(-t_wheel_delta * K_PIXELS_PER_NOTCH, t_geometry);

	if (is_needed(t_geometry)) {
		m_activity_seconds = K_SCROLL_ACTIVITY_SECONDS;
		animation::request_frame();
	}
}

auto Scrollable::scroll_by(float t_pixels, const ScrollGeometry& t_geometry) -> void
{
	m_target = std::clamp(m_target + t_pixels, 0.0f, max_offset(t_geometry));
}

auto Scrollable::jump_to(float t_offset, const ScrollGeometry& t_geometry) -> void
{
	m_target = std::clamp(t_offset, 0.0f, max_offset(t_geometry));
	m_offset = m_target;
}

auto Scrollable::reveal(float t_top, float t_bottom, float t_view_top, float t_view_bottom, const ScrollGeometry& t_geometry) -> void
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
