#include "ui/controls.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "gfx/assets.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "ui/text.h"
#include "ui/theme.h"

namespace {
constexpr float button_radius = 8.0f;
constexpr float field_border = 1.5f;
constexpr float danger_fill_strength = 0.22f;
constexpr float danger_hover_strength = 0.38f;
constexpr float danger_label_softening = 0.25f;
constexpr float countdown_gap = 2.5f;
constexpr float countdown_thickness = 1.5f;

struct ButtonLook {
	Color fill;
	Color hover_fill;
	Color label;
};

ButtonLook button_look(controls::ButtonStyle t_style, Color t_accent)
{
	const Theme &colors = theme();

	switch (t_style) {
		case controls::ButtonStyle::neutral:
			return ButtonLook{colors.control, colors.control_hover, colors.text};

		case controls::ButtonStyle::accent:
			return ButtonLook{t_accent, lightened(t_accent, 20), foreground_on(t_accent)};

		case controls::ButtonStyle::danger:
			return ButtonLook{mix(colors.control, colors.error, danger_fill_strength),
							  mix(colors.control, colors.error, danger_hover_strength),
							  mix(colors.error, colors.text, danger_label_softening)};

		case controls::ButtonStyle::ghost:
			return ButtonLook{with_alpha(colors.control, 0), colors.control_hover, colors.text_dim};

		case controls::ButtonStyle::danger_confirm:
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

void controls::draw_icon(DrawList &t_draw_list, Rect t_rect, const Texture *t_icon, Color t_tint)
{
	t_draw_list.add_image(t_rect, t_icon, t_tint);
}

void controls::draw_x(DrawList &t_draw_list, Rect t_rect, Color t_color)
{
	const Vec2 center = t_rect.center();
	const float arm = std::min(t_rect.w, t_rect.h) * 0.24f;
	const float thickness = std::max(2.0f, t_rect.h * 0.09f);

	t_draw_list.add_line({center.x - arm, center.y - arm}, {center.x + arm, center.y + arm}, thickness, t_color);
	t_draw_list.add_line({center.x - arm, center.y + arm}, {center.x + arm, center.y - arm}, thickness, t_color);
}

void controls::draw_check(DrawList &t_draw_list, Rect t_rect, Color t_color)
{
	const float scale = std::min(t_rect.w, t_rect.h) / 20.0f;
	const Vec2 center = t_rect.center();
	const Vec2 bottom{center.x - 2.0f * scale, center.y + 5.0f * scale};

	t_draw_list.add_line({center.x - 7.0f * scale, center.y}, bottom, 2.0f, t_color);
	t_draw_list.add_line(bottom, {center.x + 7.0f * scale, center.y - 6.0f * scale}, 2.0f, t_color);
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
	draw_icon(t_draw_list, t_rect, t_assets.get(t_revealed ? Asset::icon_eye_visible : Asset::icon_eye_hidden),
			  t_color);
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

	draw_icon(t_draw_list, t_rect, t_assets.get(Asset::icon_favorite), t_color);
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
	const bool ghost = t_style == ButtonStyle::ghost;

	if (lifted && !ghost) {
		draw_lift(t_draw_list, t_rect, button_radius, look.fill, t_alpha);
	}

	const Color label = ghost && lifted ? theme().text : look.label;

	t_draw_list.add_rounded_rect(t_rect, rounded(button_radius), faded(lifted ? look.hover_fill : look.fill, t_alpha));
	draw_text_centered(t_draw_list, t_font, t_rect, t_label, faded(label, t_alpha));
}

void controls::draw_dropdown(DrawList &t_draw_list, const Font &t_font, Rect t_rect, std::string_view t_label,
							 bool t_open, bool t_hovered, Color t_accent, u8 t_alpha)
{
	constexpr float padding = 12.0f;
	constexpr Vec2 chevron_size{9.0f, 5.0f};

	const Theme &colors = theme();
	const Rect chevron{t_rect.right() - padding - chevron_size.x, t_rect.center().y - chevron_size.y * 0.5f,
					   chevron_size.x, chevron_size.y};
	const float label_x = t_rect.x + padding;

	t_draw_list.add_bordered_rect(t_rect, rounded(button_radius),
								  faded(t_open || t_hovered ? colors.control_hover : colors.control, t_alpha),
								  faded(t_open ? t_accent : colors.border, t_alpha), 1.0f);
	draw_text_truncated(t_draw_list, t_font, Vec2{label_x, t_font.centered_baseline(t_rect)}, t_label,
						chevron.x - padding - label_x, faded(colors.text, t_alpha));
	draw_chevron(t_draw_list, chevron, t_open, faded(t_open ? colors.text : colors.text_dim, t_alpha));
}

void controls::draw_checkbox(DrawList &t_draw_list, Rect t_box, bool t_checked, bool t_enabled, Color t_accent)
{
	constexpr float radius = 4.0f;
	constexpr float border = 1.5f;
	constexpr float check_inset = 2.0f;
	constexpr float disabled_strength = 0.55f;

	const Theme &colors = theme();

	if (!t_checked) {
		t_draw_list.add_bordered_rect(t_box, rounded(radius), colors.field, colors.border, border);
		return;
	}

	const Color fill = t_enabled ? t_accent : mix(colors.popup, t_accent, disabled_strength);
	t_draw_list.add_rounded_rect(t_box, rounded(radius), fill);
	draw_check(t_draw_list, t_box.inset(check_inset), foreground_on(fill));
}
