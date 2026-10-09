#include "ui/controls.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <numbers>
#include <span>

#include "core/library.h"
#include "core/settings.h"
#include "core/str.h"
#include "gfx/assets.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "ui/text.h"
#include "ui/text_input.h"
#include "ui/theme.h"

namespace {
constexpr float K_BUTTON_RADIUS          = 8.0f;
constexpr float K_CIRCLE_ROUNDNESS       = 1.0f;
constexpr u8    K_GLASS_HIGHLIGHT_ALPHA  = 10;
constexpr float K_GLASS_ROW_ALPHA        = 26.0f;
constexpr float K_BLURRED_SCRIM_SHARE    = 0.5f;
constexpr float K_FIELD_BORDER           = 1.5f;
constexpr float K_DANGER_FILL_STRENGTH   = 0.22f;
constexpr float K_DANGER_HOVER_STRENGTH  = 0.38f;
constexpr float K_DANGER_LABEL_SOFTENING = 0.25f;
constexpr float K_COUNTDOWN_GAP          = 2.5f;
constexpr float K_COUNTDOWN_THICKNESS    = 1.5f;
constexpr float K_KEYCAP_PADDING         = 5.0f;
constexpr float K_KEYCAP_RADIUS          = 4.0f;
constexpr float K_KEYCAP_LIP             = 1.0f;
constexpr float K_KEYCAP_GAP             = 3.0f;
constexpr float K_SEARCH_ICON_SIZE       = 13.0f;
constexpr float K_SEARCH_ICON_GAP        = 7.0f;
constexpr float K_SEARCH_CLEAR_MARGIN    = 8.0f;
constexpr float K_REGION_CHIP_PADDING    = 6.0f;
constexpr u8    K_REGION_CHIP_ALPHA      = 22;
constexpr float K_DETAIL_DOT_SIZE        = 3.0f;
constexpr float K_DETAIL_DOT_GAP         = 7.0f;

constexpr float K_GLASS_BLUR_REFERENCE = 0.5f;

// Each GlassSurface's blur as a Gaussian sigma in logical pixels at 50% Blur, and how opaque its tint is at the default Tint.
struct GlassLook {
	float spread;
	float tint;
};

constexpr GlassLook K_GLASS_LOOKS[]{
	{.spread = 4.0f, .tint = 0.64f}, {.spread = 6.0f, .tint = 0.64f}, {.spread = 9.0f, .tint = 0.64f},
	{.spread = 9.0f, .tint = 0.90f}, {.spread = 14.0f, .tint = 0.0f},
};

static_assert(std::size(K_GLASS_LOOKS) == static_cast<usize>(controls::GlassSurface::BACKDROP) + 1);

bool  g_glass_available  = false;
bool  g_glass_blurs      = false;
float g_glass_tint       = K_GLASS_TINT_DEFAULT;
float g_glass_blur_scale = 1.0f;

struct ButtonLook {
	Color fill;
	Color hover_fill;
	Color label;
};

[[nodiscard]] auto button_look(controls::ButtonStyle t_style, Color t_accent) -> ButtonLook
{
	switch (t_style) {
		using enum controls::ButtonStyle;

		case NEUTRAL: {
			return ButtonLook{g_theme.control, g_theme.control_hover, g_theme.text};
		}

		case ACCENT: {
			return ButtonLook{t_accent, lightened(t_accent, 20), foreground_on(t_accent)};
		}

		case DANGER: {
			return ButtonLook{mix(g_theme.control, g_theme.error, K_DANGER_FILL_STRENGTH), mix(g_theme.control, g_theme.error, K_DANGER_HOVER_STRENGTH),
			                  mix(g_theme.error, g_theme.text, K_DANGER_LABEL_SOFTENING)};
		}

		case GHOST: {
			return ButtonLook{with_alpha(g_theme.control, 0), g_theme.control_hover, g_theme.text_dim};
		}

		case DANGER_CONFIRM: {
			return ButtonLook{g_theme.error, lightened(g_theme.error, 15), foreground_on(g_theme.error)};
		}
	}

	return ButtonLook{};
}

// Strokes are stretched by half their thickness at both ends so the corners of an outline close up.
auto stroke_closed(DrawList* t_draw_list, std::span<const Vec2> t_points, float t_thickness, Color t_color) -> void
{
	for (usize i = 0; i < t_points.size(); i += 1) {
		const Vec2  from   = t_points[i];
		const Vec2  to     = t_points[(i + 1) % t_points.size()];
		const float length = std::hypot(to.x - from.x, to.y - from.y);
		if (length <= 0.0f) continue;

		const Vec2 reach{(to.x - from.x) / length * t_thickness * 0.5f, (to.y - from.y) / length * t_thickness * 0.5f};
		t_draw_list->add_line(Vec2{from.x - reach.x, from.y - reach.y}, Vec2{to.x + reach.x, to.y + reach.y}, t_thickness, t_color);
	}
}

[[nodiscard]] auto disabled_button_look() -> ButtonLook
{
	return ButtonLook{g_theme.control, g_theme.control, g_theme.text_faint};
}

auto draw_outline_countdown(DrawList* t_draw_list, Rect t_shape, float t_shape_radius, float t_remaining, Color t_color) -> void
{
	const float radius = std::min(t_shape_radius, std::min(t_shape.w, t_shape.h) * 0.5f) + K_COUNTDOWN_GAP;

	t_draw_list->add_outline_countdown(t_shape.inset(-K_COUNTDOWN_GAP), radius, t_remaining, K_COUNTDOWN_THICKNESS, t_color);
}

[[nodiscard]] auto glass_look(controls::GlassSurface t_surface) -> GlassLook
{
	return K_GLASS_LOOKS[static_cast<u32>(t_surface)];
}

// The Blur setting scales every surface's blur from its 50% value, so 100% doubles it.
[[nodiscard]] auto glass_spread(controls::GlassSurface t_surface) -> float
{
	return glass_look(t_surface).spread * g_glass_blur_scale;
}

// The Tint setting scales how much of each surface is see-through from its default, so every surface keeps its place against the
// others and 100% is solid everywhere.
[[nodiscard]] auto glass_tint(controls::GlassSurface t_surface) -> u8
{
	const float clear = (1.0f - glass_look(t_surface).tint) * (1.0f - g_glass_tint) / (1.0f - K_GLASS_TINT_DEFAULT);

	return to_alpha(std::clamp(1.0f - clear, 0.0f, 1.0f));
}
}

auto controls::confirm_red() -> Color
{
	const Color error = g_theme.error;
	const float cool  = std::min(error.g, error.b);

	return Color{static_cast<u8>(std::min(255, error.r + 12)), static_cast<u8>(cool * 0.62f), static_cast<u8>(cool * 0.85f), 255};
}

auto controls::draw_x(DrawList* t_draw_list, Rect t_rect, Color t_color) -> void
{
	const Vec2  center    = t_rect.center();
	const float arm       = std::min(t_rect.w, t_rect.h) * 0.24f;
	const float thickness = std::max(2.0f, t_rect.h * 0.09f);

	t_draw_list->add_line({center.x - arm, center.y - arm}, {center.x + arm, center.y + arm}, thickness, t_color);
	t_draw_list->add_line({center.x - arm, center.y + arm}, {center.x + arm, center.y - arm}, thickness, t_color);
}

auto controls::draw_check(DrawList* t_draw_list, const Assets* t_assets, Rect t_rect, Color t_color) -> void
{
	// The icon's glyph fills half of its canvas, so it is drawn larger to match the old line-drawn check.
	constexpr float ICON_SCALE = 1.4f;

	const float size = std::min(t_rect.w, t_rect.h) * ICON_SCALE;
	t_draw_list->add_image(t_rect.centered(size, size), t_assets->get(Asset::ICON_CHECK), t_color);
}

auto controls::draw_chevron(DrawList* t_draw_list, Rect t_rect, bool t_points_up, Color t_color) -> void
{
	const Vec2  center      = t_rect.center();
	const float half_width  = t_rect.w * 0.5f;
	const float half_height = t_points_up ? -t_rect.h * 0.5f : t_rect.h * 0.5f;
	const Vec2  tip{center.x, center.y + half_height};

	t_draw_list->add_line({center.x - half_width, center.y - half_height}, tip, 1.5f, t_color);
	t_draw_list->add_line(tip, {center.x + half_width, center.y - half_height}, 1.5f, t_color);
}

auto controls::draw_lock(DrawList* t_draw_list, Rect t_rect, Color t_color, Color t_backdrop, bool t_open) -> void
{
	const float stroke = std::max(1.5f, t_rect.w * 0.11f);
	const float lift   = t_open ? t_rect.h * 0.12f : 0.0f;
	const Rect  shackle{t_rect.x + t_rect.w * 0.24f, t_rect.y + t_rect.h * 0.04f - lift, t_rect.w * 0.52f, t_rect.h * 0.6f};
	const Rect  body{t_rect.x + t_rect.w * 0.1f, t_rect.y + t_rect.h * 0.42f, t_rect.w * 0.8f, t_rect.h * 0.54f};
	const float shackle_radius = shackle.w * 0.5f;
	const float body_radius    = t_rect.w * 0.16f;
	const float keyhole        = stroke * 0.9f;
	const auto  radii          = [](float t_radius) { return CornerRadii{t_radius, t_radius, t_radius, t_radius}; };

	t_draw_list->add_rounded_rect(shackle, radii(shackle_radius), t_color);
	t_draw_list->add_rounded_rect(shackle.inset(stroke), radii(shackle_radius - stroke), t_backdrop);

	if (t_open) {
		const float gap_top = shackle.y + shackle_radius;
		t_draw_list->add_rect(Rect{shackle.right() - stroke - 0.5f, gap_top, stroke + 1.0f, shackle.bottom() - gap_top}, t_backdrop);
	}

	t_draw_list->add_rounded_rect(body, radii(body_radius), t_color);
	t_draw_list->add_rounded_rect(body.inset(stroke), radii(std::max(0.0f, body_radius - stroke)), t_backdrop);
	t_draw_list->add_rounded_rect(body.centered(keyhole, keyhole), radii(keyhole * 0.5f), t_color);
}

auto controls::keycap_width(const Font& t_font, std::string_view t_label) -> float
{
	return std::ceil(text_width(t_font, t_label) + K_KEYCAP_PADDING * 2.0f);
}

auto controls::keycap_height(const Font& t_font) -> float
{
	return std::ceil(t_font.line_height() + K_KEYCAP_LIP + 2.0f);
}

auto controls::draw_keycap_frame(DrawList* t_draw_list, Rect t_cap, Color t_backdrop, u8 t_alpha) -> void
{
	const Rect face{t_cap.x + 1.0f, t_cap.y + 1.0f, t_cap.w - 2.0f, t_cap.h - 2.0f - K_KEYCAP_LIP};

	t_draw_list->add_rounded_rect(t_cap, rounded(K_KEYCAP_RADIUS), faded(g_theme.border, t_alpha));
	t_draw_list->add_rounded_rect(face, rounded(K_KEYCAP_RADIUS - 1.0f), faded(t_backdrop, t_alpha));
}

auto controls::draw_mouse_keycap(DrawList* t_draw_list, Rect t_cap, Color t_backdrop, u8 t_alpha) -> void
{
	draw_keycap_frame(t_draw_list, t_cap, t_backdrop, t_alpha);

	const float height = std::round((t_cap.h - K_KEYCAP_LIP) * 0.6f);
	const float width  = std::round(height * 0.7f);
	const Rect  mouse{snapped_to_pixel(t_cap.center().x - width * 0.5f), snapped_to_pixel(t_cap.y + (t_cap.h - K_KEYCAP_LIP - height) * 0.5f), width, height};
	const Color color = faded(keycap_label_color(), t_alpha);

	t_draw_list->add_rounded_rect(mouse, rounded(width * 0.5f), color);
	t_draw_list->add_rounded_rect(mouse.inset(1.0f), rounded(width * 0.5f - 1.0f), faded(t_backdrop, t_alpha));
	t_draw_list->add_rect(Rect{mouse.center().x - 0.5f, mouse.y + 2.0f, 1.0f, std::round(height * 0.25f)}, color);
}

auto controls::keycap_label_color() -> Color
{
	return mix(g_theme.text_faint, g_theme.text_dim, 0.5f);
}

auto controls::draw_keycap(DrawList* t_draw_list, const Font& t_font, Rect t_cap, std::string_view t_label, Color t_backdrop, u8 t_alpha) -> void
{
	draw_keycap_frame(t_draw_list, t_cap, t_backdrop, t_alpha);
	draw_text_centered(t_draw_list, t_font, Rect{t_cap.x, t_cap.y, t_cap.w, t_cap.h - K_KEYCAP_LIP}, t_label, faded(keycap_label_color(), t_alpha));
}

auto controls::shortcut_width(const Font& t_font, std::string_view t_combo) -> float
{
	float width = 0.0f;

	for (usize start = 0; start < t_combo.size();) {
		const usize end = std::min(t_combo.find('+', start + 1), t_combo.size());
		width += keycap_width(t_font, t_combo.substr(start, end - start)) + (start > 0 ? K_KEYCAP_GAP : 0.0f);
		start = end + 1;
	}

	return width;
}

auto controls::draw_shortcut(DrawList* t_draw_list, const Font& t_font, Vec2 t_right_center, std::string_view t_combo, Color t_backdrop, u8 t_alpha) -> void
{
	const float height = keycap_height(t_font);
	float       x      = t_right_center.x - shortcut_width(t_font, t_combo);

	for (usize start = 0; start < t_combo.size();) {
		const usize            end   = std::min(t_combo.find('+', start + 1), t_combo.size());
		const std::string_view key   = t_combo.substr(start, end - start);
		const float            width = keycap_width(t_font, key);

		draw_keycap(t_draw_list, t_font, Rect{snapped_to_pixel(x), snapped_to_pixel(t_right_center.y - height * 0.5f), width, height}, key, t_backdrop,
		            t_alpha);
		x += width + K_KEYCAP_GAP;
		start = end + 1;
	}
}

auto controls::draw_magnifier(DrawList* t_draw_list, Rect t_rect, Color t_color) -> void
{
	constexpr u32 LENS_SEGMENTS = 20;

	const float size      = std::min(t_rect.w, t_rect.h);
	const float thickness = std::max(1.5f, size * 0.11f);
	const float radius    = size * 0.34f;
	const Vec2  center{t_rect.x + radius + thickness * 0.5f, t_rect.y + radius + thickness * 0.5f};

	Vec2 previous{center.x + radius, center.y};
	for (u32 i = 1; i <= LENS_SEGMENTS; i += 1) {
		const float angle = static_cast<float>(i) / LENS_SEGMENTS * 2.0f * std::numbers::pi_v<float>;
		const Vec2  point{center.x + radius * std::cos(angle), center.y + radius * std::sin(angle)};

		t_draw_list->add_line(previous, point, thickness, t_color);
		previous = point;
	}

	const float handle_start = radius * std::numbers::sqrt2_v<float> * 0.5f;
	t_draw_list->add_line({center.x + handle_start, center.y + handle_start}, {t_rect.x + size, t_rect.y + size}, thickness, t_color);
}

auto controls::caution_color() -> Color
{
	return luminance(g_theme.surface) > 0.3f ? Color{176, 112, 16, 255} : Color{240, 190, 90, 255};
}

// The theme's window or text colour, whichever reads on the background: the darker one on a light colour, the lighter one on a dark one.
auto controls::ink_on(Color t_background) -> Color
{
	const bool window_is_darker = luminance(g_theme.window) < luminance(g_theme.text);
	const bool light_background = luminance(t_background) > 0.35f;

	return light_background == window_is_darker ? g_theme.window : g_theme.text;
}

// An outlined arrow standing on a bar: the symbol keyboards print on Caps Lock. Drawn on a 24-unit grid.
auto controls::draw_caps_lock(DrawList* t_draw_list, Rect t_rect, Color t_color) -> void
{
	const float size      = std::min(t_rect.w, t_rect.h);
	const float unit      = size / 24.0f;
	const float thickness = std::max(1.25f, size * 0.09f);
	const auto  at        = [&](float t_x, float t_y) { return Vec2{t_rect.x + t_x * unit, t_rect.y + t_y * unit}; };

	const Vec2 arrow[]{at(9.0f, 11.5f), at(5.0f, 11.5f), at(12.0f, 4.0f), at(19.0f, 11.5f), at(15.0f, 11.5f), at(15.0f, 16.0f), at(9.0f, 16.0f)};
	stroke_closed(t_draw_list, arrow, thickness, t_color);
	t_draw_list->add_line(at(9.0f - thickness * 0.5f / unit, 20.0f), at(15.0f + thickness * 0.5f / unit, 20.0f), thickness, t_color);
}

auto controls::draw_alert(DrawList* t_draw_list, Rect t_rect, Color t_color) -> void
{
	constexpr u32 RING_SEGMENTS = 24;

	const float size      = std::min(t_rect.w, t_rect.h);
	const float thickness = std::max(1.25f, size * 0.09f);
	const float radius    = size * 0.5f - thickness * 0.5f;
	const Vec2  center    = t_rect.center();

	Vec2 ring[RING_SEGMENTS]{};
	for (u32 i = 0; i < RING_SEGMENTS; i += 1) {
		const float angle = static_cast<float>(i) / RING_SEGMENTS * 2.0f * std::numbers::pi_v<float>;
		ring[i]           = Vec2{center.x + radius * std::cos(angle), center.y + radius * std::sin(angle)};
	}

	stroke_closed(t_draw_list, ring, thickness, t_color);
	t_draw_list->add_line(Vec2{center.x, center.y - radius * 0.5f}, Vec2{center.x, center.y + radius * 0.12f}, thickness, t_color);
	t_draw_list->add_rect(Rect{center.x - thickness * 0.5f, center.y + radius * 0.38f, thickness, thickness}, t_color);
}

auto controls::notice_width(const Font& t_font, std::string_view t_text) -> float
{
	return std::round(t_font.line_height() * 0.9f) + 6.0f + text_width(t_font, t_text);
}

// An icon and a line of text: the Caps Lock warning and error lines under password fields.
auto controls::draw_notice(DrawList* t_draw_list, const Font& t_font, Vec2 t_top_left, NoticeKind t_kind, std::string_view t_text, Color t_color) -> void
{
	const float line = t_font.line_height();
	const float size = std::round(line * 0.9f);
	const Rect  icon{t_top_left.x, std::round(t_top_left.y + (line - size) * 0.5f), size, size};

	if (t_kind == NoticeKind::CAPS_LOCK) {
		draw_caps_lock(t_draw_list, icon, t_color);
	} else {
		draw_alert(t_draw_list, icon.inset(size * 0.06f), t_color);
	}

	draw_text(t_draw_list, t_font, Vec2{icon.right() + 6.0f, t_top_left.y + t_font.ascent}, t_text, t_color);
}

auto controls::draw_eye(DrawList* t_draw_list, const Assets* t_assets, Rect t_rect, bool t_revealed, Color t_color) -> void
{
	t_draw_list->add_image(t_rect, t_assets->get(t_revealed ? Asset::ICON_EYE_VISIBLE : Asset::ICON_EYE_HIDDEN), t_color);
}

auto controls::draw_favorite(DrawList* t_draw_list, const Assets* t_assets, Rect t_rect, bool t_filled, Color t_color) -> void
{
	if (t_filled) {
		constexpr u32   STAR_POINTS = 5;
		constexpr float INNER_RATIO = 0.5f;

		// Sized to sit under the icon's outline, which is drawn on top and keeps the edge crisp.
		const float size = std::min(t_rect.w, t_rect.h);
		const Vec2  center{t_rect.x + t_rect.w * 0.5f, t_rect.y + t_rect.h * 0.53f};
		const float outer = size * 0.31f;
		const float inner = outer * INNER_RATIO;

		const auto corner = [&](u32 t_index) {
			const float radius = t_index % 2 == 0 ? outer : inner;
			const float angle  = -std::numbers::pi_v<float> * 0.5f + t_index * std::numbers::pi_v<float> / STAR_POINTS;

			return Vec2{center.x + radius * std::cos(angle), center.y + radius * std::sin(angle)};
		};

		for (u32 i = 0; i < STAR_POINTS * 2; i += 1) {
			t_draw_list->add_triangle(center, corner(i), corner(i + 1), t_color);
		}
	}

	t_draw_list->add_image(t_rect, t_assets->get(Asset::ICON_FAVORITE), t_color);
}

auto controls::draw_lift(DrawList* t_draw_list, Rect t_rect, float t_radius, Color t_glow, u8 t_alpha) -> void
{
	constexpr float SPREAD   = 9.0f;
	constexpr float STRENGTH = 70.0f;

	t_draw_list->add_shadow(t_rect, t_radius, SPREAD, with_alpha(t_glow, static_cast<u8>(STRENGTH * t_alpha / 255.0f)));
}

// Round buttons stay circles at any Corner roundness.
auto controls::draw_circular_hover(DrawList* t_draw_list, Rect t_rect, Color t_glow, Color t_fill, u8 t_alpha) -> void
{
	const RoundnessScope circle{K_CIRCLE_ROUNDNESS};

	draw_lift(t_draw_list, t_rect, t_rect.w * 0.5f, t_glow, t_alpha);
	t_draw_list->add_rounded_rect(t_rect, rounded(t_rect.w * 0.5f), faded(t_fill, t_alpha));
}

auto controls::draw_panel_shadow(DrawList* t_draw_list, Rect t_panel, float t_radius, float t_amount) -> void
{
	constexpr float BLUR     = 22.0f;
	constexpr float DROP     = 8.0f;
	constexpr float STRENGTH = 120.0f;

	const Rect shadow{t_panel.x, t_panel.y + DROP, t_panel.w, t_panel.h};
	t_draw_list->add_shadow(shadow, t_radius, BLUR, with_alpha(g_theme.shadow, static_cast<u8>(STRENGTH * t_amount)));
}

auto controls::draw_popup_shadow(DrawList* t_draw_list, Rect t_popup, float t_radius, float t_amount) -> void
{
	constexpr float BLUR     = 10.0f;
	constexpr float DROP     = 3.0f;
	constexpr float STRENGTH = 90.0f;

	const Rect shadow{t_popup.x, t_popup.y + DROP, t_popup.w, t_popup.h};
	t_draw_list->add_shadow(shadow, t_radius, BLUR, with_alpha(g_theme.shadow, static_cast<u8>(STRENGTH * t_amount)));
}

auto controls::draw_field(DrawList* t_draw_list, Rect t_rect, float t_radius, Color t_border, Color t_fill, u8 t_alpha) -> void
{
	const RoundnessScope corners{user_roundness()};

	t_draw_list->add_bordered_rect(t_rect, rounded(t_radius), faded(t_fill, t_alpha), faded(t_border, t_alpha), K_FIELD_BORDER);
}

auto controls::draw_circular_countdown(DrawList* t_draw_list, Rect t_circle, float t_remaining, Color t_color) -> void
{
	draw_outline_countdown(t_draw_list, t_circle, t_circle.w * 0.5f, t_remaining, t_color);
}

auto controls::set_glass(bool t_supported, bool t_enabled, float t_tint, float t_blur) -> void
{
	g_glass_available  = t_supported && t_enabled;
	g_glass_blurs      = g_glass_available && t_blur > 0.001f;
	g_glass_tint       = t_tint;
	g_glass_blur_scale = t_blur / K_GLASS_BLUR_REFERENCE;
}

// Rows on glass are lit with a thin wash of the text colour, so the glass still shows through them.
auto controls::glass_highlight(float t_amount) -> Color
{
	return with_alpha(g_theme.text, static_cast<u8>(K_GLASS_ROW_ALPHA * std::clamp(t_amount, 0.0f, 1.0f)));
}

auto controls::draw_popup_backdrop(DrawList* t_draw_list, Rect t_rect, float t_amount) -> void
{
	if (g_glass_blurs) {
		t_draw_list->add_blurred_backdrop(t_rect, rounded(0.0f), glass_spread(GlassSurface::BACKDROP), to_alpha(t_amount));
	}

	const float share = g_glass_blurs ? K_BLURRED_SCRIM_SHARE : 1.0f;
	t_draw_list->add_rect(t_rect, faded(g_theme.scrim, to_alpha(t_amount * share)));
}

auto controls::draw_glass(DrawList* t_draw_list, Rect t_rect, CornerRadii t_radii, GlassSurface t_surface, u8 t_alpha, bool t_framed) -> void
{
	if (g_glass_available) {
		if (g_glass_blurs) {
			t_draw_list->add_blurred_backdrop(t_rect, t_radii, glass_spread(t_surface), t_alpha);
		}

		t_draw_list->add_rounded_rect(t_rect, t_radii, faded(with_alpha(g_theme.popup, glass_tint(t_surface)), t_alpha));
	} else {
		t_draw_list->add_rounded_rect(t_rect, t_radii, faded(g_theme.popup, t_alpha));
	}

	if (!t_framed) return;

	const float inset = std::max(t_radii.top_left, t_radii.top_right);
	t_draw_list->add_rect(Rect{t_rect.x + inset, t_rect.y + 1.0f, std::max(0.0f, t_rect.w - inset * 2.0f), 1.0f},
	                      faded(Color{255, 255, 255, K_GLASS_HIGHLIGHT_ALPHA}, t_alpha));
	t_draw_list->add_rounded_outline(t_rect, t_radii, 1.0f, faded(g_theme.border, t_alpha));
}

auto controls::draw_button(DrawList*        t_draw_list,
                           const Font&      t_font,
                           Rect             t_rect,
                           std::string_view t_label,
                           ButtonStyle      t_style,
                           Color            t_accent,
                           bool             t_enabled,
                           bool             t_hovered,
                           u8               t_alpha) -> void
{
	const RoundnessScope corners{user_roundness()};

	const ButtonLook look   = t_enabled ? button_look(t_style, t_accent) : disabled_button_look();
	const bool       lifted = t_enabled && t_hovered;
	const bool       ghost  = t_style == ButtonStyle::GHOST;

	if (lifted && !ghost) {
		draw_lift(t_draw_list, t_rect, K_BUTTON_RADIUS, look.fill, t_alpha);
	}

	const Color label = ghost && lifted ? g_theme.text : look.label;

	t_draw_list->add_rounded_rect(t_rect, rounded(K_BUTTON_RADIUS), faded(lifted ? look.hover_fill : look.fill, t_alpha));
	draw_text_centered(t_draw_list, t_font, t_rect, t_label, faded(label, t_alpha));
}

auto controls::search_text_rect(Rect t_search, float t_inset) -> Rect
{
	const float left  = t_search.x + t_inset + K_SEARCH_ICON_SIZE + K_SEARCH_ICON_GAP;
	const float right = search_clear_rect(t_search).x - K_SEARCH_ICON_GAP * 0.5f;

	return Rect{left, t_search.y, std::max(0.0f, right - left), t_search.h};
}

auto controls::search_clear_rect(Rect t_search) -> Rect
{
	return Rect{t_search.right() - K_SEARCH_CLEAR_MARGIN - K_SEARCH_ICON_SIZE, t_search.center().y - K_SEARCH_ICON_SIZE * 0.5f, K_SEARCH_ICON_SIZE,
	            K_SEARCH_ICON_SIZE};
}

auto controls::draw_search_field(DrawList*   t_draw_list,
                                 const Font& t_font,
                                 Rect        t_search,
                                 float       t_inset,
                                 TextInput*  t_input,
                                 Vec2        t_mouse,
                                 Color       t_accent,
                                 u8          t_alpha) -> void
{
	const bool focused   = t_input->is_focused();
	const bool has_query = !t_input->value().empty();

	draw_field(t_draw_list, t_search, t_search.h * 0.5f, focused ? g_theme.text_dim : g_theme.control, focused ? g_theme.row_hover : g_theme.field, t_alpha);

	const Rect icon{t_search.x + t_inset, t_search.center().y - K_SEARCH_ICON_SIZE * 0.5f, K_SEARCH_ICON_SIZE, K_SEARCH_ICON_SIZE};
	draw_magnifier(t_draw_list, icon, faded(focused || has_query ? g_theme.text_dim : g_theme.text_faint, t_alpha));
	t_input->draw(t_draw_list, t_font, search_text_rect(t_search, t_inset), faded(g_theme.text, t_alpha), faded(t_accent, t_alpha), t_search);

	if (!has_query) return;

	const Rect clear = search_clear_rect(t_search);
	draw_x(t_draw_list, clear.inset(1.5f), faded(clear.contains(t_mouse) ? g_theme.text : g_theme.text_faint, t_alpha));
}

auto controls::region_chip_width(const Font& t_font, std::string_view t_region) -> float
{
	return t_region.empty() ? 0.0f : text_width(t_font, t_region) + K_REGION_CHIP_PADDING * 2.0f;
}

auto controls::draw_region_chip(DrawList* t_draw_list, const Font& t_font, float t_x, float t_center_y, std::string_view t_region, u8 t_alpha) -> void
{
	const float height = t_font.line_height() + 2.0f;
	const Rect  chip{t_x, t_center_y - height * 0.5f, region_chip_width(t_font, t_region), height};

	t_draw_list->add_rounded_rect(chip, rounded(height * 0.5f), faded(with_alpha(g_theme.text, K_REGION_CHIP_ALPHA), t_alpha));
	draw_text_centered(t_draw_list, t_font, chip, t_region, faded(g_theme.text_dim, t_alpha));
}

auto controls::draw_account_details(DrawList* t_draw_list, const Font& t_font, Vec2 t_baseline, float t_max_width, const Account& t_account, u8 t_alpha) -> void
{
	const std::string_view note = t_account.note;

	char             relative[32];
	char             last_used[48];
	std::string_view when;

	if (t_account.last_used != 0) {
		when = relative_time(t_account.last_used, std::time(nullptr), relative);

		if (note.empty()) {
			const int written = std::snprintf(last_used, sizeof(last_used), "Last used %.*s", static_cast<int>(when.size()), when.data());
			when              = std::string_view{last_used, static_cast<usize>(std::max(written, 0))};
		}
	}

	if (note.empty()) {
		draw_text_truncated(t_draw_list, t_font, t_baseline, when, t_max_width, faded(g_theme.text_faint, t_alpha));
		return;
	}

	const float when_width = when.empty() ? 0.0f : text_width(t_font, when) + K_DETAIL_DOT_GAP * 2.0f + K_DETAIL_DOT_SIZE;
	const float note_width = std::min(text_width(t_font, note), std::max(0.0f, t_max_width - when_width));

	draw_text_truncated(t_draw_list, t_font, t_baseline, note, note_width + 0.5f, faded(g_theme.text_dim, t_alpha));

	if (when.empty()) return;

	const float dot_x  = t_baseline.x + note_width + K_DETAIL_DOT_GAP;
	const float dot_y  = t_baseline.y - t_font.ascent * 0.33f - K_DETAIL_DOT_SIZE * 0.5f;
	const float when_x = dot_x + K_DETAIL_DOT_SIZE + K_DETAIL_DOT_GAP;

	t_draw_list->add_rounded_rect(Rect{dot_x, dot_y, K_DETAIL_DOT_SIZE, K_DETAIL_DOT_SIZE}, rounded(K_DETAIL_DOT_SIZE * 0.5f),
	                              faded(g_theme.text_faint, t_alpha));
	draw_text_truncated(t_draw_list, t_font, Vec2{when_x, t_baseline.y}, when, t_baseline.x + t_max_width - when_x, faded(g_theme.text_faint, t_alpha));
}
