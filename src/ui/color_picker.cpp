#include "ui/color_picker.h"

#include <algorithm>
#include <cmath>
#include <span>

#include "gfx/draw_list.h"
#include "ui/controls.h"
#include "ui/theme.h"

namespace {
constexpr float popup_padding = 12.0f;
constexpr float popup_radius = 12.0f;
constexpr float square_size = 180.0f;
constexpr float strip_gap = 10.0f;
constexpr float alpha_strip_width = 20.0f;
constexpr float hue_strip_height = 16.0f;
constexpr float handle_radius = 6.0f;
constexpr float window_margin = 8.0f;
constexpr float anchor_gap = 8.0f;

constexpr Color color_marker{255, 255, 255, 255};

constexpr Color hue_stops[]{
	{255, 0, 0, 255}, {255, 255, 0, 255}, {0, 255, 0, 255}, {0, 255, 255, 255},
	{0, 0, 255, 255}, {255, 0, 255, 255}, {255, 0, 0, 255},
};

struct Hsv {
	float hue;
	float saturation;
	float value;
};

Color hsv_to_rgb(float t_hue, float t_saturation, float t_value)
{
	const float chroma = t_value * t_saturation;
	const float sector = std::fmod(t_hue, 360.0f) / 60.0f;
	const float secondary = chroma * (1.0f - std::fabs(std::fmod(sector, 2.0f) - 1.0f));

	float r = 0.0f;
	float g = 0.0f;
	float b = 0.0f;

	switch (static_cast<int>(sector)) {
		case 0:
			r = chroma;
			g = secondary;
			break;
		case 1:
			r = secondary;
			g = chroma;
			break;
		case 2:
			g = chroma;
			b = secondary;
			break;
		case 3:
			g = secondary;
			b = chroma;
			break;
		case 4:
			r = secondary;
			b = chroma;
			break;
		default:
			r = chroma;
			b = secondary;
			break;
	}

	const float lift = t_value - chroma;
	const auto to_byte = [lift](float t_channel) {
		return static_cast<u8>(std::clamp((t_channel + lift) * 255.0f, 0.0f, 255.0f));
	};

	return Color{to_byte(r), to_byte(g), to_byte(b), 255};
}

Hsv rgb_to_hsv(Color t_color)
{
	const float r = t_color.r / 255.0f;
	const float g = t_color.g / 255.0f;
	const float b = t_color.b / 255.0f;

	const float max_channel = std::max({r, g, b});
	const float delta = max_channel - std::min({r, g, b});

	float hue = 0.0f;
	if (delta > 0.0001f) {
		if (max_channel == r) {
			hue = 60.0f * std::fmod((g - b) / delta, 6.0f);
		} else if (max_channel == g) {
			hue = 60.0f * ((b - r) / delta + 2.0f);
		} else {
			hue = 60.0f * ((r - g) / delta + 4.0f);
		}
	}

	return Hsv{hue < 0.0f ? hue + 360.0f : hue, max_channel > 0.0001f ? delta / max_channel : 0.0f, max_channel};
}

Rect saturation_value_rect(Rect t_popup)
{
	return Rect{t_popup.x + popup_padding, t_popup.y + popup_padding, square_size, square_size};
}

Rect hue_rect(Rect t_popup)
{
	const Rect square = saturation_value_rect(t_popup);

	return Rect{square.x, square.bottom() + strip_gap, square_size, hue_strip_height};
}

Rect alpha_rect(Rect t_popup)
{
	const Rect square = saturation_value_rect(t_popup);

	return Rect{square.right() + strip_gap, square.y, alpha_strip_width, square_size};
}

float fraction_along(float t_position, float t_start, float t_length)
{
	return std::clamp((t_position - t_start) / t_length, 0.0f, 1.0f);
}

void draw_handle(DrawList &t_draw_list, Vec2 t_center, Color t_fill)
{
	const float inner_radius = handle_radius - 2.5f;

	t_draw_list.add_rounded_rect(
		Rect{t_center.x - handle_radius, t_center.y - handle_radius, handle_radius * 2.0f, handle_radius * 2.0f},
		rounded(handle_radius), foreground_on(t_fill));
	t_draw_list.add_rounded_rect(
		Rect{t_center.x - inner_radius, t_center.y - inner_radius, inner_radius * 2.0f, inner_radius * 2.0f},
		rounded(inner_radius), t_fill);
}
}

Rect ColorPicker::popup_rect() const
{
	const float width = popup_padding * 2.0f + square_size + strip_gap + alpha_strip_width;
	const float height = popup_padding * 2.0f + square_size + strip_gap + hue_strip_height;
	const float max_x = std::max(window_margin, m_window_size.x - window_margin - width);
	const float max_y = std::max(window_margin, m_window_size.y - window_margin - height);

	float y = m_anchor.bottom() + anchor_gap;
	if (y + height > m_window_size.y - window_margin) {
		y = m_anchor.y - anchor_gap - height;
	}

	return Rect{std::clamp(m_anchor.right() - width, window_margin, max_x), std::clamp(y, window_margin, max_y), width,
				height};
}

void ColorPicker::end_drags()
{
	m_saturation_value_drag.end();
	m_hue_drag.end();
	m_alpha_drag.end();
}

void ColorPicker::open(Color t_initial, Rect t_anchor, Vec2 t_window_size)
{
	const Hsv hsv = rgb_to_hsv(t_initial);
	m_hue = hsv.hue;
	m_saturation = hsv.saturation;
	m_value = hsv.value;
	m_alpha = t_initial.a;

	m_anchor = t_anchor;
	m_window_size = t_window_size;
	m_open = true;

	end_drags();
}

void ColorPicker::close()
{
	m_open = false;
	end_drags();
}

Color ColorPicker::color() const
{
	return with_alpha(hsv_to_rgb(m_hue, m_saturation, m_value), m_alpha);
}

bool ColorPicker::on_pointer_down(Vec2 t_point)
{
	const Rect popup = popup_rect();
	if (!m_open || !popup.contains(t_point)) return false;

	if (saturation_value_rect(popup).contains(t_point)) {
		m_saturation_value_drag.begin(t_point);
	} else if (hue_rect(popup).contains(t_point)) {
		m_hue_drag.begin(t_point);
	} else if (alpha_rect(popup).contains(t_point)) {
		m_alpha_drag.begin(t_point);
	}

	on_pointer_move(t_point);

	return true;
}

void ColorPicker::on_pointer_move(Vec2 t_point)
{
	const Rect popup = popup_rect();

	if (m_saturation_value_drag.is_pressed()) {
		const Rect square = saturation_value_rect(popup);
		m_saturation_value_drag.update(t_point);
		m_saturation = fraction_along(t_point.x, square.x, square.w);
		m_value = 1.0f - fraction_along(t_point.y, square.y, square.h);
	}

	if (m_hue_drag.is_pressed()) {
		const Rect hue = hue_rect(popup);
		m_hue_drag.update(t_point);
		m_hue = fraction_along(t_point.x, hue.x, hue.w) * 360.0f;
	}

	if (m_alpha_drag.is_pressed()) {
		const Rect alpha = alpha_rect(popup);
		m_alpha_drag.update(t_point);
		m_alpha = static_cast<u8>((1.0f - fraction_along(t_point.y, alpha.y, alpha.h)) * 255.0f);
	}
}

void ColorPicker::on_pointer_up()
{
	end_drags();
}

CursorKind ColorPicker::cursor(Vec2 t_mouse) const
{
	if (!m_open) return CursorKind::arrow;
	if (is_dragging()) return CursorKind::drag;

	const Rect popup = popup_rect();
	const bool over_control = saturation_value_rect(popup).contains(t_mouse) || hue_rect(popup).contains(t_mouse) ||
							  alpha_rect(popup).contains(t_mouse);

	return over_control ? CursorKind::hand : CursorKind::arrow;
}

void ColorPicker::draw(DrawList &t_draw_list) const
{
	if (!m_open) return;

	const Rect popup = popup_rect();
	controls::draw_popup_shadow(t_draw_list, popup.inset(-1.0f), popup_radius, 1.0f);
	t_draw_list.add_bordered_rect(popup.inset(-1.0f), rounded(popup_radius), theme().popup, theme().border, 1.0f);

	const Color picked = hsv_to_rgb(m_hue, m_saturation, m_value);

	const Rect square = saturation_value_rect(popup);
	t_draw_list.add_color_picker_square(square, m_hue);
	draw_handle(t_draw_list, Vec2{square.x + m_saturation * square.w, square.y + (1.0f - m_value) * square.h}, picked);

	const Rect hue = hue_rect(popup);
	const std::span<const Color> stops = hue_stops;
	const float segment_width = hue.w / static_cast<float>(stops.size() - 1);

	for (usize i = 0; i + 1 < stops.size(); i += 1) {
		const Rect segment{hue.x + i * segment_width, hue.y, segment_width, hue.h};
		t_draw_list.add_gradient(segment, stops[i], stops[i + 1], stops[i], stops[i + 1]);
	}

	const float hue_marker_x = hue.x + m_hue / 360.0f * hue.w;
	t_draw_list.add_rect(Rect{hue_marker_x - 1.5f, hue.y - 2.0f, 3.0f, hue.h + 4.0f}, color_marker);

	const Rect alpha = alpha_rect(popup);
	t_draw_list.add_gradient(alpha, with_alpha(picked, 255), with_alpha(picked, 255), with_alpha(picked, 0),
							 with_alpha(picked, 0));

	const float alpha_marker_y = alpha.y + (1.0f - m_alpha / 255.0f) * alpha.h;
	t_draw_list.add_rect(Rect{alpha.x - 2.0f, alpha_marker_y - 1.5f, alpha.w + 4.0f, 3.0f}, color_marker);
}
