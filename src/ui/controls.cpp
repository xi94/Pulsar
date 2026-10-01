#include "ui/controls.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "gfx/assets.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "ui/text.h"
#include "ui/text_input.h"
#include "ui/theme.h"

namespace {
constexpr float button_radius = 8.0f;
constexpr float field_border = 1.5f;
constexpr float danger_fill_strength = 0.22f;
constexpr float danger_hover_strength = 0.38f;
constexpr float danger_label_softening = 0.25f;
constexpr float countdown_gap = 2.5f;
constexpr float countdown_thickness = 1.5f;
constexpr float keycap_padding = 5.0f;
constexpr float keycap_radius = 4.0f;
constexpr float keycap_lip = 1.0f;
constexpr float keycap_gap = 3.0f;
constexpr float search_icon_size = 13.0f;
constexpr float search_icon_gap = 7.0f;
constexpr float search_clear_margin = 8.0f;

struct ButtonLook {
	Color fill;
	Color hover_fill;
	Color label;
};

ButtonLook button_look(controls::ButtonStyle t_style, Color t_accent)
{
	const Theme &colors = theme();

	switch (t_style) {
		case controls::ButtonStyle::Neutral:
			return ButtonLook{colors.control, colors.control_hover, colors.text};

		case controls::ButtonStyle::Accent:
			return ButtonLook{t_accent, lightened(t_accent, 20), foreground_on(t_accent)};

		case controls::ButtonStyle::Danger:
			return ButtonLook{mix(colors.control, colors.error, danger_fill_strength),
							  mix(colors.control, colors.error, danger_hover_strength),
							  mix(colors.error, colors.text, danger_label_softening)};

		case controls::ButtonStyle::Ghost:
			return ButtonLook{with_alpha(colors.control, 0), colors.control_hover, colors.text_dim};

		case controls::ButtonStyle::DangerConfirm:
			return ButtonLook{colors.error, lightened(colors.error, 15), foreground_on(colors.error)};
	}

	return ButtonLook{};
}

ButtonLook disabled_button_look()
{
	return ButtonLook{theme().control, theme().control, theme().text_faint};
}

void draw_outline_countdown(DrawList &t_draw_list, Rect t_shape, float t_shape_radius, float t_remaining, Color t_color)
{
	const float radius = std::min(t_shape_radius, std::min(t_shape.w, t_shape.h) * 0.5f) + countdown_gap;

	t_draw_list.add_outline_countdown(t_shape.inset(-countdown_gap), radius, t_remaining, countdown_thickness, t_color);
}
}

Color controls::confirm_red()
{
	const Color error = theme().error;
	const float cool = std::min(error.g, error.b);

	return Color{static_cast<u8>(std::min(255, error.r + 12)), static_cast<u8>(cool * 0.62f),
				 static_cast<u8>(cool * 0.85f), 255};
}

void controls::draw_x(DrawList &t_draw_list, Rect t_rect, Color t_color)
{
	const Vec2 center = t_rect.center();
	const float arm = std::min(t_rect.w, t_rect.h) * 0.24f;
	const float thickness = std::max(2.0f, t_rect.h * 0.09f);

	t_draw_list.add_line({center.x - arm, center.y - arm}, {center.x + arm, center.y + arm}, thickness, t_color);
	t_draw_list.add_line({center.x - arm, center.y + arm}, {center.x + arm, center.y - arm}, thickness, t_color);
}

void controls::draw_check(DrawList &t_draw_list, const Assets &t_assets, Rect t_rect, Color t_color)
{
	// The icon's glyph fills half of its canvas, so it is drawn larger to match the old line-drawn check.
	constexpr float icon_scale = 1.4f;

	const float size = std::min(t_rect.w, t_rect.h) * icon_scale;
	t_draw_list.add_image(t_rect.centered(size, size), t_assets.get(Asset::IconCheck), t_color);
}

void controls::draw_chevron(DrawList &t_draw_list, Rect t_rect, bool t_points_up, Color t_color)
{
	const Vec2 center = t_rect.center();
	const float half_width = t_rect.w * 0.5f;
	const float half_height = t_points_up ? -t_rect.h * 0.5f : t_rect.h * 0.5f;
	const Vec2 tip{center.x, center.y + half_height};

	t_draw_list.add_line({center.x - half_width, center.y - half_height}, tip, 1.5f, t_color);
	t_draw_list.add_line(tip, {center.x + half_width, center.y - half_height}, 1.5f, t_color);
}

void controls::draw_lock(DrawList &t_draw_list, Rect t_rect, Color t_color, Color t_backdrop, bool t_open)
{
	const float stroke = std::max(1.5f, t_rect.w * 0.11f);
	const float lift = t_open ? t_rect.h * 0.12f : 0.0f;
	const Rect shackle{t_rect.x + t_rect.w * 0.24f, t_rect.y + t_rect.h * 0.04f - lift, t_rect.w * 0.52f,
					   t_rect.h * 0.6f};
	const Rect body{t_rect.x + t_rect.w * 0.1f, t_rect.y + t_rect.h * 0.42f, t_rect.w * 0.8f, t_rect.h * 0.54f};
	const float shackle_radius = shackle.w * 0.5f;
	const float body_radius = t_rect.w * 0.16f;
	const float keyhole = stroke * 0.9f;
	const auto radii = [](float t_radius) { return CornerRadii{t_radius, t_radius, t_radius, t_radius}; };

	t_draw_list.add_rounded_rect(shackle, radii(shackle_radius), t_color);
	t_draw_list.add_rounded_rect(shackle.inset(stroke), radii(shackle_radius - stroke), t_backdrop);

	if (t_open) {
		const float gap_top = shackle.y + shackle_radius;
		t_draw_list.add_rect(Rect{shackle.right() - stroke - 0.5f, gap_top, stroke + 1.0f, shackle.bottom() - gap_top},
							 t_backdrop);
	}

	t_draw_list.add_rounded_rect(body, radii(body_radius), t_color);
	t_draw_list.add_rounded_rect(body.inset(stroke), radii(std::max(0.0f, body_radius - stroke)), t_backdrop);
	t_draw_list.add_rounded_rect(body.centered(keyhole, keyhole), radii(keyhole * 0.5f), t_color);
}

float controls::keycap_width(const Font &t_font, std::string_view t_label)
{
	return std::ceil(text_width(t_font, t_label) + keycap_padding * 2.0f);
}

float controls::keycap_height(const Font &t_font)
{
	return std::ceil(t_font.line_height() + keycap_lip + 2.0f);
}

void controls::draw_keycap_frame(DrawList &t_draw_list, Rect t_cap, Color t_backdrop, u8 t_alpha)
{
	const Rect face{t_cap.x + 1.0f, t_cap.y + 1.0f, t_cap.w - 2.0f, t_cap.h - 2.0f - keycap_lip};

	t_draw_list.add_rounded_rect(t_cap, rounded(keycap_radius), faded(theme().border, t_alpha));
	t_draw_list.add_rounded_rect(face, rounded(keycap_radius - 1.0f), faded(t_backdrop, t_alpha));
}

void controls::draw_mouse_keycap(DrawList &t_draw_list, Rect t_cap, Color t_backdrop, u8 t_alpha)
{
	draw_keycap_frame(t_draw_list, t_cap, t_backdrop, t_alpha);

	const float height = std::round((t_cap.h - keycap_lip) * 0.6f);
	const float width = std::round(height * 0.7f);
	const Rect mouse{snapped_to_pixel(t_cap.center().x - width * 0.5f),
					 snapped_to_pixel(t_cap.y + (t_cap.h - keycap_lip - height) * 0.5f), width, height};
	const Color color = faded(keycap_label_color(), t_alpha);

	t_draw_list.add_rounded_rect(mouse, rounded(width * 0.5f), color);
	t_draw_list.add_rounded_rect(mouse.inset(1.0f), rounded(width * 0.5f - 1.0f), faded(t_backdrop, t_alpha));
	t_draw_list.add_rect(Rect{mouse.center().x - 0.5f, mouse.y + 2.0f, 1.0f, std::round(height * 0.25f)}, color);
}

Color controls::keycap_label_color()
{
	return mix(theme().text_faint, theme().text_dim, 0.5f);
}

void controls::draw_keycap(DrawList &t_draw_list, const Font &t_font, Rect t_cap, std::string_view t_label,
						   Color t_backdrop, u8 t_alpha)
{
	draw_keycap_frame(t_draw_list, t_cap, t_backdrop, t_alpha);
	draw_text_centered(t_draw_list, t_font, Rect{t_cap.x, t_cap.y, t_cap.w, t_cap.h - keycap_lip}, t_label,
					   faded(keycap_label_color(), t_alpha));
}

float controls::shortcut_width(const Font &t_font, std::string_view t_combo)
{
	float width = 0.0f;

	for (usize start = 0; start < t_combo.size();) {
		const usize end = std::min(t_combo.find('+', start + 1), t_combo.size());
		width += keycap_width(t_font, t_combo.substr(start, end - start)) + (start > 0 ? keycap_gap : 0.0f);
		start = end + 1;
	}

	return width;
}

void controls::draw_shortcut(DrawList &t_draw_list, const Font &t_font, Vec2 t_right_center, std::string_view t_combo,
							 Color t_backdrop, u8 t_alpha)
{
	const float height = keycap_height(t_font);
	float x = t_right_center.x - shortcut_width(t_font, t_combo);

	for (usize start = 0; start < t_combo.size();) {
		const usize end = std::min(t_combo.find('+', start + 1), t_combo.size());
		const std::string_view key = t_combo.substr(start, end - start);
		const float width = keycap_width(t_font, key);

		draw_keycap(t_draw_list, t_font,
					Rect{snapped_to_pixel(x), snapped_to_pixel(t_right_center.y - height * 0.5f), width, height}, key,
					t_backdrop, t_alpha);
		x += width + keycap_gap;
		start = end + 1;
	}
}

void controls::draw_magnifier(DrawList &t_draw_list, Rect t_rect, Color t_color)
{
	constexpr u32 lens_segments = 20;

	const float size = std::min(t_rect.w, t_rect.h);
	const float thickness = std::max(1.5f, size * 0.11f);
	const float radius = size * 0.34f;
	const Vec2 center{t_rect.x + radius + thickness * 0.5f, t_rect.y + radius + thickness * 0.5f};

	Vec2 previous{center.x + radius, center.y};
	for (u32 i = 1; i <= lens_segments; i += 1) {
		const float angle = static_cast<float>(i) / lens_segments * 2.0f * std::numbers::pi_v<float>;
		const Vec2 point{center.x + radius * std::cos(angle), center.y + radius * std::sin(angle)};

		t_draw_list.add_line(previous, point, thickness, t_color);
		previous = point;
	}

	const float handle_start = radius * std::numbers::sqrt2_v<float> * 0.5f;
	t_draw_list.add_line({center.x + handle_start, center.y + handle_start}, {t_rect.x + size, t_rect.y + size},
						 thickness, t_color);
}

void controls::draw_eye(DrawList &t_draw_list, const Assets &t_assets, Rect t_rect, bool t_revealed, Color t_color)
{
	t_draw_list.add_image(t_rect, t_assets.get(t_revealed ? Asset::IconEyeVisible : Asset::IconEyeHidden), t_color);
}

void controls::draw_favorite(DrawList &t_draw_list, const Assets &t_assets, Rect t_rect, bool t_filled, Color t_color)
{
	if (t_filled) {
		constexpr u32 points = 5;
		constexpr float inner_ratio = 0.5f;

		// Sized to sit under the icon's outline, which is drawn on top and keeps the edge crisp.
		const float size = std::min(t_rect.w, t_rect.h);
		const Vec2 center{t_rect.x + t_rect.w * 0.5f, t_rect.y + t_rect.h * 0.53f};
		const float outer = size * 0.31f;
		const float inner = outer * inner_ratio;

		const auto corner = [&](u32 t_index) {
			const float radius = t_index % 2 == 0 ? outer : inner;
			const float angle = -std::numbers::pi_v<float> * 0.5f + t_index * std::numbers::pi_v<float> / points;

			return Vec2{center.x + radius * std::cos(angle), center.y + radius * std::sin(angle)};
		};

		for (u32 i = 0; i < points * 2; i += 1) {
			t_draw_list.add_triangle(center, corner(i), corner(i + 1), t_color);
		}
	}

	t_draw_list.add_image(t_rect, t_assets.get(Asset::IconFavorite), t_color);
}

void controls::draw_lift(DrawList &t_draw_list, Rect t_rect, float t_radius, Color t_glow, u8 t_alpha)
{
	constexpr float spread = 9.0f;
	constexpr float strength = 70.0f;

	t_draw_list.add_shadow(t_rect, t_radius, spread, with_alpha(t_glow, static_cast<u8>(strength * t_alpha / 255.0f)));
}

void controls::draw_circular_hover(DrawList &t_draw_list, Rect t_rect, Color t_glow, Color t_fill, u8 t_alpha)
{
	draw_lift(t_draw_list, t_rect, t_rect.w * 0.5f, t_glow, t_alpha);
	t_draw_list.add_rounded_rect(t_rect, rounded(t_rect.w * 0.5f), faded(t_fill, t_alpha));
}

void controls::draw_panel_shadow(DrawList &t_draw_list, Rect t_panel, float t_radius, float t_amount)
{
	constexpr float blur = 22.0f;
	constexpr float drop = 8.0f;
	constexpr float strength = 120.0f;

	const Rect shadow{t_panel.x, t_panel.y + drop, t_panel.w, t_panel.h};
	t_draw_list.add_shadow(shadow, t_radius, blur, with_alpha(theme().shadow, static_cast<u8>(strength * t_amount)));
}

void controls::draw_popup_shadow(DrawList &t_draw_list, Rect t_popup, float t_radius, float t_amount)
{
	constexpr float blur = 10.0f;
	constexpr float drop = 3.0f;
	constexpr float strength = 90.0f;

	const Rect shadow{t_popup.x, t_popup.y + drop, t_popup.w, t_popup.h};
	t_draw_list.add_shadow(shadow, t_radius, blur, with_alpha(theme().shadow, static_cast<u8>(strength * t_amount)));
}

void controls::draw_field(DrawList &t_draw_list, Rect t_rect, float t_radius, Color t_border, Color t_fill, u8 t_alpha)
{
	t_draw_list.add_bordered_rect(t_rect, rounded(t_radius), faded(t_fill, t_alpha), faded(t_border, t_alpha),
								  field_border);
}

void controls::draw_circular_countdown(DrawList &t_draw_list, Rect t_circle, float t_remaining, Color t_color)
{
	draw_outline_countdown(t_draw_list, t_circle, scaled_radius(t_circle.w * 0.5f), t_remaining, t_color);
}

void controls::draw_button(DrawList &t_draw_list, const Font &t_font, Rect t_rect, std::string_view t_label,
						   ButtonStyle t_style, Color t_accent, bool t_enabled, bool t_hovered, u8 t_alpha)
{
	const ButtonLook look = t_enabled ? button_look(t_style, t_accent) : disabled_button_look();
	const bool lifted = t_enabled && t_hovered;
	const bool ghost = t_style == ButtonStyle::Ghost;

	if (lifted && !ghost) {
		draw_lift(t_draw_list, t_rect, button_radius, look.fill, t_alpha);
	}

	const Color label = ghost && lifted ? theme().text : look.label;

	t_draw_list.add_rounded_rect(t_rect, rounded(button_radius), faded(lifted ? look.hover_fill : look.fill, t_alpha));
	draw_text_centered(t_draw_list, t_font, t_rect, t_label, faded(label, t_alpha));
}

Rect controls::search_text_rect(Rect t_search, float t_inset)
{
	const float left = t_search.x + t_inset + search_icon_size + search_icon_gap;
	const float right = search_clear_rect(t_search).x - search_icon_gap * 0.5f;

	return Rect{left, t_search.y, std::max(0.0f, right - left), t_search.h};
}

Rect controls::search_clear_rect(Rect t_search)
{
	return Rect{t_search.right() - search_clear_margin - search_icon_size,
				t_search.center().y - search_icon_size * 0.5f, search_icon_size, search_icon_size};
}

void controls::draw_search_field(DrawList &t_draw_list, const Font &t_font, Rect t_search, float t_inset,
								 TextInput &t_input, Vec2 t_mouse, Color t_accent, u8 t_alpha)
{
	const Theme &colors = theme();
	const bool focused = t_input.is_focused();
	const bool has_query = !t_input.value().empty();

	draw_field(t_draw_list, t_search, t_search.h * 0.5f, focused ? colors.text_dim : colors.control,
			   focused ? colors.row_hover : colors.field, t_alpha);

	const Rect icon{t_search.x + t_inset, t_search.center().y - search_icon_size * 0.5f, search_icon_size,
					search_icon_size};
	draw_magnifier(t_draw_list, icon, faded(focused || has_query ? colors.text_dim : colors.text_faint, t_alpha));
	t_input.draw(t_draw_list, t_font, search_text_rect(t_search, t_inset), faded(colors.text, t_alpha),
				 faded(t_accent, t_alpha), t_search);

	if (!has_query) return;

	const Rect clear = search_clear_rect(t_search);
	draw_x(t_draw_list, clear.inset(1.5f), faded(clear.contains(t_mouse) ? colors.text : colors.text_faint, t_alpha));
}
