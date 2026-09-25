#include "ui/controls.h"

#include <algorithm>

#include "gfx/assets.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "ui/text.h"

namespace {
constexpr float button_radius = 8.0f;
constexpr float field_border = 1.5f;
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

void controls::draw_eye(DrawList &t_draw_list, const Assets &t_assets, Rect t_rect, bool t_revealed, Color t_color)
{
	draw_icon(t_draw_list, t_rect, t_assets.get(t_revealed ? Asset::icon_eye_visible : Asset::icon_eye_hidden),
			  t_color);
}

void controls::draw_lift(DrawList &t_draw_list, Rect t_rect, float t_radius, Color t_glow, u8 t_alpha)
{
	constexpr int layers = 5;
	constexpr float max_spread = 9.0f;
	constexpr float base_alpha = 34.0f;

	for (int layer = layers; layer >= 1; layer -= 1) {
		const float t = static_cast<float>(layer) / layers;
		const float spread = max_spread * t;
		const auto layer_alpha = static_cast<u8>(base_alpha * (1.0f - t) * (1.0f - t) * (t_alpha / 255.0f));
		if (layer_alpha == 0) continue;

		t_draw_list.add_rounded_rect(t_rect.inset(-spread), rounded(t_radius + spread),
									 with_alpha(t_glow, layer_alpha));
	}
}

void controls::draw_circular_hover(DrawList &t_draw_list, Rect t_rect, Color t_glow, Color t_fill, u8 t_alpha)
{
	draw_lift(t_draw_list, t_rect, t_rect.w * 0.5f, t_glow, t_alpha);
	t_draw_list.add_rounded_rect(t_rect, rounded(t_rect.w * 0.5f), faded(t_fill, t_alpha));
}

void controls::draw_panel_shadow(DrawList &t_draw_list, Rect t_panel, float t_radius, float t_amount)
{
	constexpr int layers = 4;
	constexpr float max_spread = 20.0f;
	constexpr float drop = 10.0f;
	constexpr float layer_alpha = 18.0f;

	for (int layer = layers; layer >= 1; layer -= 1) {
		const float t = static_cast<float>(layer) / layers;
		const float spread = max_spread * t;
		const Rect shadow{t_panel.x - spread, t_panel.y - spread + drop, t_panel.w + spread * 2.0f,
						  t_panel.h + spread * 2.0f};

		t_draw_list.add_rounded_rect(shadow, rounded(t_radius + spread * 0.4f),
									 Color{0, 0, 0, static_cast<u8>(layer_alpha * t * t_amount)});
	}
}

void controls::draw_field(DrawList &t_draw_list, Rect t_rect, float t_radius, Color t_border, Color t_fill, u8 t_alpha)
{
	t_draw_list.add_bordered_rect(t_rect, rounded(t_radius), faded(t_fill, t_alpha), faded(t_border, t_alpha),
								  field_border);
}

void controls::draw_accent_button(DrawList &t_draw_list, const Font &t_font, Rect t_rect, std::string_view t_label,
								  Color t_accent, bool t_enabled, bool t_hovered, Color t_disabled_fill,
								  Color t_disabled_label, u8 t_alpha)
{
	const Color fill = !t_enabled ? t_disabled_fill : (t_hovered ? lightened(t_accent, 20) : t_accent);

	if (t_hovered) {
		draw_lift(t_draw_list, t_rect, button_radius, t_accent, t_alpha);
	}

	t_draw_list.add_bordered_rect(t_rect, rounded(button_radius), faded(fill, t_alpha),
								  faded(outline_on(fill), t_alpha), 1.0f);
	draw_text_centered(t_draw_list, t_font, t_rect, t_label,
					   faded(t_enabled ? foreground_on(fill) : t_disabled_label, t_alpha));
}

void controls::draw_button(DrawList &t_draw_list, const Font &t_font, Rect t_rect, std::string_view t_label,
						   const ButtonColors &t_colors, bool t_hovered, u8 t_alpha)
{
	if (t_hovered) {
		draw_lift(t_draw_list, t_rect, button_radius, t_colors.lift, t_alpha);
	}

	t_draw_list.add_rounded_rect(t_rect, rounded(button_radius),
								 faded(t_hovered ? t_colors.hover_fill : t_colors.fill, t_alpha));
	draw_text_centered(t_draw_list, t_font, t_rect, t_label, faded(t_colors.label, t_alpha));
}
