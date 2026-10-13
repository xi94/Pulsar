#include "ui/settings_panel.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <optional>
#include <span>
#include <type_traits>

#include "core/animation.h"
#include "core/profiler.h"
#include "core/str.h"
#include "gfx/assets.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "os/installation.h"
#include "os/window.h"
#include "render/renderer.h"
#include "ui/controls.h"
#include "ui/text.h"
#include "ui/theme.h"
#include "ui/window_layout.h"

namespace {
constexpr float K_OPEN_EASE_RATE    = 16.0f;
constexpr float K_TOGGLE_EASE_RATE  = 18.0f;
constexpr float K_VALUE_EASE_RATE   = 16.0f;
constexpr float K_RESET_APPEAR_RATE = 20.0f;
constexpr float K_RESET_SPIN_RATE   = 6.0f;

constexpr Vec2  K_PANEL_MAX_SIZE{720.0f, 480.0f};
constexpr Vec2  K_PANEL_MIN_SIZE{580.0f, 380.0f};
constexpr float K_REFERENCE_BODY_PIXEL_HEIGHT      = 24.0f;
constexpr float K_REFERENCE_SECONDARY_PIXEL_HEIGHT = 20.0f;
constexpr float K_PANEL_MARGIN                     = 48.0f;
constexpr float K_PANEL_CLOSED_SCALE               = 0.94f;
constexpr float K_PANEL_RADIUS                     = 16.0f;
constexpr float K_PANEL_BORDER                     = 1.5f;

constexpr float K_ROW_PADDING_X           = 26.0f;
constexpr float K_BACK_BUTTON_SIZE        = 32.0f;
constexpr float K_BACK_BUTTON_MARGIN      = 12.0f;
constexpr float K_BACK_ICON_SIZE          = 18.0f;
constexpr float K_TITLE_GAP               = 10.0f;
constexpr float K_LABEL_LINE_GAP          = 3.0f;
constexpr float K_LABEL_CONTROL_GAP       = 16.0f;
constexpr float K_HIGHLIGHT_INSET_X       = 4.0f;
constexpr float K_HIGHLIGHT_INSET_Y       = 3.0f;
constexpr float K_ROW_INSET_X             = 16.0f;
constexpr float K_TWO_LINE_PADDING_Y      = 10.0f;
constexpr float K_SINGLE_ROW_PADDING_Y    = 6.0f;
constexpr float K_CARD_MARGIN_X           = 18.0f;
constexpr float K_CARD_RADIUS             = 10.0f;
constexpr float K_GROUP_GAP               = 6.0f;
constexpr float K_REVEAL_EASE_RATE        = 16.0f;
constexpr float K_ROWS_TOP_PADDING        = 8.0f;
constexpr float K_RAIL_WIDTH              = 150.0f;
constexpr float K_RAIL_PADDING            = 10.0f;
constexpr float K_TAB_LABEL_INSET         = 12.0f;
constexpr float K_TAB_GAP                 = 4.0f;
constexpr float K_TAB_SLIDE_RATE          = 18.0f;
constexpr float K_HOVERED_TAB_FILL        = 0.5f;
constexpr float K_CARD_WASH               = 0.35f;
constexpr float K_HOVERED_TAB_BRIGHTENING = 0.5f;
constexpr float K_SCROLLBAR_MARGIN        = 4.0f;
constexpr float K_HEADER_SHADOW_HEIGHT    = 14.0f;
constexpr float K_HEADER_SHADOW_ALPHA     = 0.35f;
constexpr float K_HEADER_SHADOW_TRAVEL    = 24.0f;

constexpr float K_RESET_BUTTON_SIZE = 28.0f;
constexpr float K_RESET_BUTTON_GAP  = 10.0f;
constexpr float K_RESET_ICON_SIZE   = 18.0f;
constexpr float K_RESET_COLUMN      = K_RESET_BUTTON_GAP + K_RESET_BUTTON_SIZE;

constexpr float K_SLIDER_BAR_HEIGHT      = 4.0f;
constexpr float K_SLIDER_REST_BAR_HEIGHT = 2.0f;
constexpr float K_SLIDER_TEXT_CLEARANCE  = 7.0f;
constexpr float K_SLIDER_THUMB_RADIUS    = 6.0f;
constexpr float K_SLIDER_THUMB_RING      = 2.0f;
constexpr float K_SLIDER_TICK_SIZE       = 3.0f;
constexpr float K_SLIDER_HOVER_RATE      = 14.0f;
constexpr float K_SLIDER_CAPTION_GAP     = 8.0f;
constexpr u32   K_AUTO_LOCK_STOPS[]{1, 2, 5, 10, 15, 30, 60, 0};
constexpr u32   K_AUTO_LOCK_STOP_COUNT = static_cast<u32>(std::size(K_AUTO_LOCK_STOPS));

constexpr std::string_view K_PANEL_TITLE            = "Settings";
constexpr float            K_SEARCH_MAX_WIDTH       = 216.0f;
constexpr float            K_SEARCH_MIN_WIDTH       = 120.0f;
constexpr float            K_SEARCH_HEADER_GAP      = 16.0f;
constexpr float            K_SEARCH_INSET           = 11.0f;
constexpr u32              K_SEARCH_MAX_LENGTH      = 48;
constexpr float            K_GROUP_TITLE_TOP_GAP    = 16.0f;
constexpr float            K_GROUP_TITLE_BOTTOM_GAP = 10.0f;
constexpr float            K_KNOB_RING              = 1.5f;

constexpr float K_CONTROL_COLUMN_WIDTH = 170.0f;
constexpr float K_CONTROL_RADIUS       = 8.0f;
constexpr float K_STEPPER_WIDTH        = 96.0f;
constexpr float K_SWATCH_SIZE          = 22.0f;
constexpr float K_SWATCH_RING_GAP      = 2.0f;
constexpr float K_SWATCH_RING          = 1.5f;
constexpr float K_FONT_LIST_WIDTH      = 290.0f;

constexpr u32   K_PATTERN_COLUMNS             = 3;
constexpr float K_PATTERN_SELECT_PADDING      = 10.0f;
constexpr float K_PATTERN_SELECT_CHEVRON_ROOM = 31.0f;
constexpr float K_PATTERN_POPUP_PADDING       = 14.0f;
constexpr float K_PATTERN_POPUP_RADIUS        = 10.0f;
constexpr float K_PATTERN_POPUP_GAP           = 6.0f;
constexpr float K_PATTERN_POPUP_MARGIN        = 8.0f;
constexpr float K_PATTERN_POPUP_RATE          = 22.0f;
constexpr float K_PATTERN_POPUP_RISE          = 6.0f;
constexpr float K_TILE_MIN_WIDTH              = 96.0f;
constexpr float K_TILE_LABEL_MARGIN           = 12.0f;
constexpr float K_TILE_GAP                    = 10.0f;
constexpr float K_TILE_ROW_GAP                = 12.0f;
constexpr float K_TILE_ASPECT                 = 0.56f;
constexpr float K_TILE_RADIUS                 = 8.0f;
constexpr float K_TILE_LABEL_GAP              = 6.0f;
constexpr float K_TILE_RING_GAP               = 2.0f;
constexpr float K_TILE_RING                   = 2.0f;
constexpr float K_TILE_RING_RATE              = 16.0f;

constexpr std::string_view K_CLOSE_CHOICE_LABELS[]{"Minimize to tray", "Quit"};
constexpr std::string_view K_CARET_STYLE_LABELS[]{"Bar", "Block", "Underline"};
constexpr std::string_view K_STARTUP_LABELS[]{"Off", "Minimized", "Open"};
const std::string_view     K_RENDERER_LABELS[]{graphics_api_name(GraphicsApi::NATIVE), graphics_api_name(GraphicsApi::OPENGL)};
constexpr float            K_SEGMENT_PADDING_X  = 12.0f;
constexpr float            K_SEGMENT_INSET      = 2.0f;
constexpr float            K_SEGMENT_SLIDE_RATE = 18.0f;
constexpr float            K_THEME_LIST_WIDTH   = 300.0f;

constexpr float K_THEME_DOT_SIZE              = 16.0f;
constexpr float K_THEME_DOT_OVERLAP           = 6.0f;
constexpr float K_THEME_DOT_CUTOUT            = 2.0f;
constexpr float K_CIRCLE_ROUNDNESS            = 1.0f;
constexpr float K_THEME_HOVER_PREVIEW_SECONDS = 1.0f;
constexpr Vec2  K_THEME_PREVIEW_SIZE{K_THEME_DOT_SIZE * 2.0f - K_THEME_DOT_OVERLAP, K_THEME_DOT_SIZE};

// Settings a slider moves count as unchanged until they differ by more than the slider can show.
constexpr float K_SAME_VALUE = 0.001f;

const Settings K_DEFAULT_SETTINGS{};

constexpr EnumArray<ThemeKind, std::string_view> K_THEME_NAMES = [] {
	EnumArray<ThemeKind, std::string_view> names{};
	for (const ThemeKind theme : each<ThemeKind>()) {
		names[theme] = K_THEME_LABELS[theme].name;
	}

	return names;
}();

template <auto MEMBER>
[[nodiscard]] auto at_default(const Settings* t_settings) -> bool
{
	const auto& value    = t_settings->*MEMBER;
	const auto& fallback = K_DEFAULT_SETTINGS.*MEMBER;

	if constexpr (std::is_floating_point_v<std::remove_cvref_t<decltype(value)>>) {
		return std::fabs(value - fallback) < K_SAME_VALUE;
	} else {
		return value == fallback;
	}
}

[[nodiscard]] auto fraction_in(float t_value, float t_min, float t_max) -> float
{
	return std::clamp((t_value - t_min) / (t_max - t_min), 0.0f, 1.0f);
}

[[nodiscard]] auto value_at(float t_fraction, float t_min, float t_max) -> float
{
	return t_min + std::clamp(t_fraction, 0.0f, 1.0f) * (t_max - t_min);
}

[[nodiscard]] auto header_height(const Fonts* t_fonts) -> float
{
	return t_fonts->body.line_height() + 20.0f;
}

[[nodiscard]] auto label_block_height(const Fonts* t_fonts) -> float
{
	return t_fonts->body.line_height() + K_LABEL_LINE_GAP + t_fonts->secondary.line_height();
}

[[nodiscard]] auto row_height(const Fonts* t_fonts) -> float
{
	return label_block_height(t_fonts) + K_TWO_LINE_PADDING_Y * 2.0f;
}

[[nodiscard]] auto group_title_height(const Fonts* t_fonts) -> float
{
	return K_GROUP_TITLE_TOP_GAP + t_fonts->secondary.line_height() + K_GROUP_TITLE_BOTTOM_GAP;
}

[[nodiscard]] auto tab_height(const Fonts* t_fonts) -> float
{
	return std::max(34.0f, t_fonts->body.line_height() + 12.0f);
}

[[nodiscard]] auto control_height(const Fonts* t_fonts) -> float
{
	return std::max(30.0f, t_fonts->body.line_height() + 10.0f);
}

[[nodiscard]] auto single_row_height(const Fonts* t_fonts) -> float
{
	return std::max(control_height(t_fonts), t_fonts->body.line_height()) + K_SINGLE_ROW_PADDING_Y * 2.0f;
}

[[nodiscard]] auto title_baseline(Rect t_row, const Fonts* t_fonts) -> float
{
	return t_row.center().y - label_block_height(t_fonts) * 0.5f + t_fonts->body.ascent;
}

[[nodiscard]] auto description_baseline(Rect t_row, const Fonts* t_fonts) -> float
{
	return t_row.center().y - label_block_height(t_fonts) * 0.5f + t_fonts->body.line_height() + K_LABEL_LINE_GAP + t_fonts->secondary.ascent;
}

[[nodiscard]] auto right_aligned_control(Rect t_row, float t_width, float t_height) -> Rect
{
	return Rect{t_row.right() - K_ROW_INSET_X - t_width, t_row.center().y - t_height * 0.5f, t_width, t_height};
}

[[nodiscard]] auto back_button_rect(Rect t_header) -> Rect
{
	return Rect{t_header.x + K_BACK_BUTTON_MARGIN, t_header.center().y - K_BACK_BUTTON_SIZE * 0.5f, K_BACK_BUTTON_SIZE, K_BACK_BUTTON_SIZE};
}

[[nodiscard]] auto dropdown_rect(Rect t_row, const Fonts* t_fonts) -> Rect
{
	return right_aligned_control(t_row, K_CONTROL_COLUMN_WIDTH, control_height(t_fonts));
}

[[nodiscard]] auto pattern_select_rect(Rect t_row, const Fonts* t_fonts) -> Rect
{
	float widest = 0.0f;
	for (const OptionLabel& label : K_BACKGROUND_LABELS) {
		widest = std::max(widest, text_width(t_fonts->body, label.name));
	}

	return right_aligned_control(t_row, std::ceil(widest + K_PATTERN_SELECT_PADDING + K_PATTERN_SELECT_CHEVRON_ROOM), control_height(t_fonts));
}

[[nodiscard]] auto stepper_rect(Rect t_row, const Fonts* t_fonts) -> Rect
{
	return right_aligned_control(t_row, K_STEPPER_WIDTH, control_height(t_fonts));
}

[[nodiscard]] auto stepper_minus(Rect t_stepper) -> Rect
{
	return Rect{t_stepper.x, t_stepper.y, t_stepper.h, t_stepper.h};
}

[[nodiscard]] auto stepper_plus(Rect t_stepper) -> Rect
{
	return Rect{t_stepper.right() - t_stepper.h, t_stepper.y, t_stepper.h, t_stepper.h};
}

[[nodiscard]] auto toggle_rect(Rect t_row) -> Rect
{
	return right_aligned_control(t_row, 36.0f, 20.0f);
}

[[nodiscard]] auto swatch_rect(Rect t_row) -> Rect
{
	return right_aligned_control(t_row, K_SWATCH_SIZE, K_SWATCH_SIZE);
}

[[nodiscard]] auto riot_client_button_rect(Rect t_row, const Fonts* t_fonts) -> Rect
{
	return right_aligned_control(t_row, 96.0f, control_height(t_fonts));
}

[[nodiscard]] auto master_password_button_rect(Rect t_row, const Fonts* t_fonts) -> Rect
{
	return right_aligned_control(t_row, 140.0f, control_height(t_fonts));
}

[[nodiscard]] auto slider_control_rect(Rect t_row, const Fonts* t_fonts) -> Rect
{
	return right_aligned_control(t_row, K_CONTROL_COLUMN_WIDTH, control_height(t_fonts));
}

[[nodiscard]] auto reset_button_rect(Rect t_control, Rect t_row) -> Rect
{
	return Rect{t_control.x - K_RESET_COLUMN, t_row.center().y - K_RESET_BUTTON_SIZE * 0.5f, K_RESET_BUTTON_SIZE, K_RESET_BUTTON_SIZE};
}

[[nodiscard]] auto label_right_edge(Rect t_control) -> float
{
	return t_control.x - K_RESET_COLUMN - K_LABEL_CONTROL_GAP;
}

[[nodiscard]] auto inline_slider_rect(Rect t_row, Rect t_control, float t_left, const Fonts* t_fonts) -> Rect
{
	const float height = control_height(t_fonts);
	const float right  = t_control.x - K_RESET_COLUMN - K_LABEL_CONTROL_GAP;

	return Rect{t_left, t_row.center().y - height * 0.5f, std::max(0.0f, right - t_left), height};
}

[[nodiscard]] auto slider_fraction_at(Rect t_slider, float t_x) -> float
{
	return t_slider.w > 0.0f ? std::clamp((t_x - t_slider.x) / t_slider.w, 0.0f, 1.0f) : 0.0f;
}

[[nodiscard]] auto circle_at(Vec2 t_center, float t_radius) -> Rect
{
	return Rect{t_center.x - t_radius, t_center.y - t_radius, t_radius * 2.0f, t_radius * 2.0f};
}

auto draw_row_label(DrawList* t_draw_list, const Fonts* t_fonts, Rect t_row, const char* t_title, const char* t_description, float t_right_edge, u8 t_alpha)
	-> void
{
	const Font& body = t_fonts->body;
	const float x    = t_row.x + K_ROW_INSET_X;

	if (*t_description == '\0') {
		draw_text_truncated(t_draw_list, body, Vec2{x, body.centered_baseline(t_row)}, t_title, t_right_edge - x, faded(g_theme.text, t_alpha));
		return;
	}

	draw_text_truncated(t_draw_list, body, Vec2{x, title_baseline(t_row, t_fonts)}, t_title, t_right_edge - x, faded(g_theme.text, t_alpha));
	draw_text_truncated(t_draw_list, t_fonts->secondary, Vec2{x, description_baseline(t_row, t_fonts)}, t_description, t_right_edge - x,
	                    faded(g_theme.text_dim, t_alpha));
}

auto draw_knob(DrawList* t_draw_list, Rect t_knob, Color t_fill, u8 t_alpha) -> void
{
	t_draw_list->add_bordered_rect(t_knob, rounded(t_knob.w * 0.5f), faded(t_fill, t_alpha), faded(outline_on(t_fill), t_alpha), K_KNOB_RING);
}

auto draw_toggle(DrawList* t_draw_list, Rect t_toggle, float t_on, Color t_accent, u8 t_alpha) -> void
{
	const Color track = mix(g_theme.track, t_accent, t_on);
	t_draw_list->add_rounded_rect(t_toggle, rounded(t_toggle.h * 0.5f), faded(track, t_alpha));

	const float knob_size = t_toggle.h - 6.0f;
	const Rect  knob{t_toggle.x + 3.0f + (t_toggle.w - t_toggle.h) * t_on, t_toggle.y + 3.0f, knob_size, knob_size};
	draw_knob(t_draw_list, knob, foreground_on(track), t_alpha);
}

auto draw_inset_control(DrawList* t_draw_list, Rect t_rect, Color t_border, u8 t_alpha) -> void
{
	t_draw_list->add_bordered_rect(t_rect, rounded(K_CONTROL_RADIUS), faded(g_theme.field, t_alpha), faded(t_border, t_alpha), 1.0f);
}

struct SliderLook {
	std::string_view readout;
	std::string_view caption;
	float            fraction;
	float            hover;
	Color            accent;
	u32              steps;
};

auto draw_slider(DrawList* t_draw_list, const Font& t_font, Rect t_slider, const SliderLook& t_look, u8 t_alpha) -> void
{
	const float hover     = t_look.hover;
	const float thickness = K_SLIDER_REST_BAR_HEIGHT + (K_SLIDER_BAR_HEIGHT - K_SLIDER_REST_BAR_HEIGHT) * hover;
	const Rect  bar{t_slider.x, t_slider.bottom() - thickness, t_slider.w, thickness};
	const Rect  filled{bar.x, bar.y, bar.w * std::clamp(t_look.fraction, 0.0f, 1.0f), bar.h};
	const Vec2  thumb{filled.right(), bar.center().y};

	const float baseline      = t_font.centered_baseline(Rect{t_slider.x, t_slider.y, t_slider.w, t_slider.h - K_SLIDER_TEXT_CLEARANCE});
	const float readout_width = text_width(t_font, t_look.readout);
	const float riding_x      = std::clamp(thumb.x - readout_width * 0.5f, t_slider.x, std::max(t_slider.x, t_slider.right() - readout_width));
	const float caption_right = t_slider.x + text_width(t_font, t_look.caption) + K_SLIDER_CAPTION_GAP;
	const float crowding      = std::clamp((caption_right - riding_x) / K_SLIDER_CAPTION_GAP, 0.0f, 1.0f);
	const auto  hover_alpha   = static_cast<u8>(t_alpha * hover);

	draw_text(t_draw_list, t_font, Vec2{t_slider.x, baseline}, t_look.caption, faded(g_theme.text_faint, static_cast<u8>(t_alpha * (1.0f - hover * crowding))));
	draw_text(t_draw_list, t_font, Vec2{t_slider.right() - readout_width, baseline}, t_look.readout,
	          faded(g_theme.text, static_cast<u8>(t_alpha * (1.0f - hover))));

	t_draw_list->add_rounded_rect(bar, rounded(thickness * 0.5f), faded(mix(g_theme.popup, g_theme.track, 0.55f + 0.45f * hover), t_alpha));
	if (filled.w > 0.0f) {
		t_draw_list->add_rounded_rect(filled, rounded(thickness * 0.5f), faded(t_look.accent, t_alpha));
	}

	if (hover_alpha == 0) return;

	for (u32 i = 1; i < t_look.steps; i += 1) {
		const float x    = bar.x + bar.w * static_cast<float>(i) / static_cast<float>(t_look.steps);
		const Color tick = x <= filled.right() ? with_alpha(foreground_on(t_look.accent), 150) : g_theme.text_faint;

		t_draw_list->add_rounded_rect(circle_at(Vec2{x, thumb.y}, K_SLIDER_TICK_SIZE * 0.5f), rounded(K_SLIDER_TICK_SIZE * 0.5f), faded(tick, hover_alpha));
	}

	const float radius       = K_SLIDER_THUMB_RADIUS * (0.5f + 0.5f * hover);
	const float inner_radius = std::max(0.0f, radius - K_SLIDER_THUMB_RING);

	t_draw_list->add_rounded_rect(circle_at(thumb, radius), rounded(radius), faded(t_look.accent, hover_alpha));
	t_draw_list->add_rounded_rect(circle_at(thumb, inner_radius), rounded(inner_radius), faded(Color{255, 255, 255, 255}, hover_alpha));
	draw_text(t_draw_list, t_font, Vec2{snapped_to_pixel(riding_x), baseline}, t_look.readout, faded(g_theme.text, hover_alpha));
}

auto draw_select(DrawList* t_draw_list, const Font& t_font, Rect t_rect, std::string_view t_label, bool t_open, bool t_hovered, Color t_accent, u8 t_alpha)
	-> void
{
	constexpr float PADDING = 10.0f;
	constexpr Vec2  CHEVRON_SIZE{9.0f, 5.0f};

	const Rect chevron{t_rect.right() - PADDING - CHEVRON_SIZE.x, t_rect.center().y - CHEVRON_SIZE.y * 0.5f, CHEVRON_SIZE.x, CHEVRON_SIZE.y};

	draw_inset_control(t_draw_list, t_rect, t_open ? t_accent : (t_hovered ? g_theme.border : g_theme.separator), t_alpha);
	draw_text_truncated(t_draw_list, t_font, Vec2{t_rect.x + PADDING, t_font.centered_baseline(t_rect)}, t_label, chevron.x - PADDING - (t_rect.x + PADDING),
	                    faded(g_theme.text, t_alpha));
	controls::draw_chevron(t_draw_list, chevron, t_open, faded(t_open || t_hovered ? g_theme.text_dim : g_theme.text_faint, t_alpha));
}

auto draw_stepper(DrawList* t_draw_list, const Font& t_font, Rect t_stepper, float t_value, u8 t_alpha) -> void
{
	constexpr float GLYPH_HALF      = 4.5f;
	constexpr float GLYPH_THICKNESS = 1.5f;

	const Vec2  minus = stepper_minus(t_stepper).center();
	const Vec2  plus  = stepper_plus(t_stepper).center();
	const Color glyph = faded(g_theme.text_dim, t_alpha);

	draw_inset_control(t_draw_list, t_stepper, g_theme.separator, t_alpha);
	t_draw_list->add_line({minus.x - GLYPH_HALF, minus.y}, {minus.x + GLYPH_HALF, minus.y}, GLYPH_THICKNESS, glyph);
	t_draw_list->add_line({plus.x - GLYPH_HALF, plus.y}, {plus.x + GLYPH_HALF, plus.y}, GLYPH_THICKNESS, glyph);
	t_draw_list->add_line({plus.x, plus.y - GLYPH_HALF}, {plus.x, plus.y + GLYPH_HALF}, GLYPH_THICKNESS, glyph);

	char      buffer[8];
	const int written = std::snprintf(buffer, sizeof(buffer), "%d", static_cast<int>(std::lround(t_value)));

	draw_text_centered(t_draw_list, t_font, t_stepper, std::string_view{buffer, static_cast<usize>(std::max(written, 0))}, faded(g_theme.text, t_alpha));
}

auto draw_reset_button(DrawList* t_draw_list, const Texture* t_icon, Rect t_button, float t_visible, float t_spin, bool t_hovered, u8 t_alpha) -> void
{
	if (t_visible <= 0.01f) return;

	const auto alpha = static_cast<u8>(t_alpha * t_visible);

	if (t_hovered) {
		t_draw_list->add_rounded_rect(t_button, rounded(7.0f), faded(g_theme.control_hover, alpha));
	}

	t_draw_list->add_rotated_image(t_button.centered(K_RESET_ICON_SIZE, K_RESET_ICON_SIZE), -t_spin * 2.0f * std::numbers::pi_v<float>, t_icon,
	                               faded(t_hovered ? g_theme.text : g_theme.text_dim, alpha));
}

[[nodiscard]] auto auto_lock_stop(u32 t_minutes) -> u32
{
	if (t_minutes == 0) return K_AUTO_LOCK_STOP_COUNT - 1;

	const auto distance = [t_minutes](u32 t_stop) {
		const u32 stop = K_AUTO_LOCK_STOPS[t_stop];
		return t_minutes > stop ? t_minutes - stop : stop - t_minutes;
	};

	u32 nearest = 0;
	for (u32 i = 1; i + 1 < K_AUTO_LOCK_STOP_COUNT; i += 1) {
		if (distance(i) < distance(nearest)) {
			nearest = i;
		}
	}

	return nearest;
}

[[nodiscard]] auto auto_lock_label(u32 t_minutes, char (&t_buffer)[16]) -> std::string_view
{
	if (t_minutes == 0) return "Never";
	if (t_minutes == 60) return "1 hour";

	const int written = std::snprintf(t_buffer, sizeof(t_buffer), "%u min", t_minutes);

	return std::string_view{t_buffer, static_cast<usize>(std::max(written, 0))};
}

[[nodiscard]] auto matches_every_word(std::string_view t_query, std::span<const std::string_view> t_fields) -> bool
{
	usize start = 0;

	while (start < t_query.size()) {
		const usize            end  = std::min(t_query.find(' ', start), t_query.size());
		const std::string_view word = t_query.substr(start, end - start);
		start                       = end + 1;

		if (word.empty()) continue;

		const bool found =
			std::ranges::any_of(t_fields, [word](std::string_view t_field) { return find_ignoring_case(t_field, word) != std::string_view::npos; });
		if (!found) return false;
	}

	return true;
}

// Colour dots stay round at any Corner roundness, and inside popups that fix their own.
auto draw_theme_preview(DrawList* t_draw_list, Rect t_preview, ThemeKind t_kind, Color t_backdrop, u8 t_alpha) -> void
{
	const RoundnessScope circles{K_CIRCLE_ROUNDNESS};

	const Theme& preset = theme_preset(t_kind);
	const float  radius = K_THEME_DOT_SIZE * 0.5f;
	const Rect   background_dot{t_preview.x, t_preview.center().y - radius, K_THEME_DOT_SIZE, K_THEME_DOT_SIZE};
	const Rect   accent_dot{t_preview.right() - K_THEME_DOT_SIZE, background_dot.y, K_THEME_DOT_SIZE, K_THEME_DOT_SIZE};

	t_draw_list->add_bordered_rect(background_dot, rounded(radius), faded(preset.window, t_alpha), faded(preset.border, t_alpha), 1.0f);
	t_draw_list->add_rounded_rect(accent_dot.inset(-K_THEME_DOT_CUTOUT), rounded(radius + K_THEME_DOT_CUTOUT), faded(t_backdrop, t_alpha));
	t_draw_list->add_rounded_rect(accent_dot, rounded(radius), faded(preset.default_accent, t_alpha));
}
}

const EnumArray<SettingsPanel::SettingsTab, std::string_view> SettingsPanel::K_TAB_NAMES{{"Appearance", "Behavior", "Privacy", "Security"}};

const EnumArray<SettingsPanel::SettingsGroup, SettingsPanel::GroupSpec> SettingsPanel::K_GROUPS{{
	{SettingsTab::APPEARANCE, "Look"},
	{SettingsTab::APPEARANCE, "Background"},
	{SettingsTab::APPEARANCE, "Text"},
	{SettingsTab::APPEARANCE, "Cursor"},
	{SettingsTab::APPEARANCE, "Glass"},
	{SettingsTab::BEHAVIOR, "Motion"},
	{SettingsTab::BEHAVIOR, "General"},
	{SettingsTab::BEHAVIOR, "Riot Client"},
	{SettingsTab::PRIVACY, "Protection"},
	{SettingsTab::SECURITY, "Vault"},
}};

const SettingsPanel::RowSpec SettingsPanel::K_ROW_SPECS[]{
	{&Rows::theme, SettingsGroup::LOOK, "Theme", "", "dark light mode palette colour colors interface"},
	{&Rows::accent, SettingsGroup::LOOK, "Accent color", "", "colour highlight buttons"},
	{&Rows::corner_roundness, SettingsGroup::LOOK, "Corner roundness", "", "radius rounded corners"},
	{&Rows::background, SettingsGroup::BACKGROUND, "Pattern", "",
	 "background backdrop texture dots grid lines polka topography starfield stars scanlines crosshatch wallpaper "
	 "strength intensity opacity subtle"},
	{&Rows::background_light, SettingsGroup::BACKGROUND, "Soft light", "", "background backdrop glow gradient top light depth strength intensity"},
	{&Rows::background_grain, SettingsGroup::BACKGROUND, "Grain", "", "background backdrop noise film texture strength intensity"},
	{&Rows::snow, SettingsGroup::BACKGROUND, "Snow", "Gently falling snow behind your games. Uses a bit more power.",
	 "weather winter snowfall seasonal christmas effect"},
	{&Rows::font, SettingsGroup::TEXT, "Font", "", "typeface family text"},
	{&Rows::font_size, SettingsGroup::TEXT, "Font size", "", "text scale zoom bigger interface"},
	{&Rows::secondary_font_size, SettingsGroup::TEXT, "Small text size", "", "secondary font labels hints scale smaller"},
	{&Rows::caret_style, SettingsGroup::CURSOR, "Cursor style", "", "caret text typing shape bar beam line block underline"},
	{&Rows::caret_trail, SettingsGroup::CURSOR, "Cursor trail", "", "caret typing smooth smear glide hop neovide kitty strength length"},
	{&Rows::glass, SettingsGroup::GLASS, "Frosted glass", "",
	 "blur popups menus translucent transparent acrylic vibrancy frost background strength tint opacity"},
	{&Rows::glass_tint, SettingsGroup::GLASS, "", "", "glass tint popups menus opacity darkness transparency see through"},
	{&Rows::animations, SettingsGroup::MOTION, "Animations", "", "motion effects reduce animate popups speed fast slow"},
	{&Rows::notifications, SettingsGroup::GENERAL, "Notifications", "", "toast popup alert confirmation messages"},
	{&Rows::start_with_system, SettingsGroup::GENERAL, "Start with Windows", "",
	 "startup boot sign in login launch automatically autostart run minimized tray open hidden"},
	{&Rows::close_to_tray, SettingsGroup::GENERAL, "When closing", "", "close to tray minimize quit exit background system tray hide"},
	{&Rows::renderer, SettingsGroup::GENERAL, "Renderer", "Restart Pulsar to switch.", "graphics gpu direct3d directx metal opengl driver"},
	{&Rows::riot_client, SettingsGroup::RIOT_CLIENT, "Location", "Found automatically when you log in.",
	 "riot client path folder install location exe riotclientservices browse locate find launcher"},
	{&Rows::hide_from_capture, SettingsGroup::PROTECTION, "Hide from screen capture", "Hide Pulsar from screenshares, recordings and screenshots.",
	 "stream record share obs discord"},
	{&Rows::block_overlay_injection, SettingsGroup::PROTECTION, "Block overlay injection", "Block overlays and keyloggers. Restart to apply.",
	 "security inject dll"},
	{&Rows::auto_lock, SettingsGroup::VAULT, "Auto-lock", "Lock the vault after being idle.", "timeout idle inactive away"},
	{&Rows::master_password, SettingsGroup::VAULT, "Master password", "Encrypts saved passwords.", "change reset vault encryption"},
};

const EnumArray<SettingsPanel::SliderKind, SettingsPanel::SliderSpec> SettingsPanel::K_SLIDERS{{
	{
		.row     = &Rows::corner_roundness,
		.place   = SliderPlace::OWN_ROW,
		.caption = "",
		.value   = &Settings::corner_roundness,
		.min     = K_CORNER_ROUNDNESS_MIN,
		.max     = K_CORNER_ROUNDNESS_MAX,
		.shown   = nullptr,
	},
	{
		.row     = &Rows::background,
		.place   = SliderPlace::INLINE,
		.caption = "Strength",
		.value   = &Settings::background_intensity,
		.min     = 0.0f,
		.max     = 1.0f,
		.shown   = [](const Settings* t_settings) { return t_settings->background_style != BackgroundStyle::NONE; },
	},
	{
		.row     = &Rows::background_light,
		.place   = SliderPlace::INLINE,
		.caption = "Strength",
		.value   = &Settings::background_light_intensity,
		.min     = 0.0f,
		.max     = 1.0f,
		.shown   = [](const Settings* t_settings) { return t_settings->background_light; },
	},
	{
		.row     = &Rows::background_grain,
		.place   = SliderPlace::INLINE,
		.caption = "Strength",
		.value   = &Settings::background_grain_intensity,
		.min     = 0.0f,
		.max     = 1.0f,
		.shown   = [](const Settings* t_settings) { return t_settings->background_grain; },
	},
	{
		.row     = &Rows::caret_trail,
		.place   = SliderPlace::INLINE,
		.caption = "Strength",
		.value   = &Settings::caret_trail_strength,
		.min     = 0.0f,
		.max     = 1.0f,
		.shown   = [](const Settings* t_settings) { return t_settings->caret_trail; },
	},
	{
		.row     = &Rows::glass,
		.place   = SliderPlace::INLINE,
		.caption = "Blur",
		.value   = &Settings::glass_blur,
		.min     = 0.0f,
		.max     = 1.0f,
		.shown   = [](const Settings* t_settings) { return t_settings->glass; },
	},
	{
		.row     = &Rows::glass_tint,
		.place   = SliderPlace::INLINE,
		.caption = "Tint",
		.value   = &Settings::glass_tint,
		.min     = K_GLASS_TINT_MIN,
		.max     = K_GLASS_TINT_MAX,
		.shown   = [](const Settings* t_settings) { return t_settings->glass; },
	},
	{
		.row     = &Rows::animations,
		.place   = SliderPlace::BESIDE_TOGGLE,
		.caption = "Speed",
		.value   = &Settings::animation_speed,
		.min     = K_ANIMATION_SPEED_MIN,
		.max     = K_ANIMATION_SPEED_MAX,
		.shown   = [](const Settings* t_settings) { return t_settings->animations_enabled; },
	},
	{
		.row     = &Rows::auto_lock,
		.place   = SliderPlace::OWN_ROW,
		.caption = "",
		.value   = nullptr,
		.min     = 0.0f,
		.max     = 0.0f,
		.shown   = nullptr,
	},
}};

const EnumArray<SettingsPanel::ChoiceKind, SettingsPanel::ChoiceSpec> SettingsPanel::K_CHOICES{{
	{
		.row    = &Rows::caret_style,
		.labels = K_CARET_STYLE_LABELS,
		.chosen = [](const Settings* t_settings) { return enum_index(t_settings->caret_style); },
		.choose = [](Settings* t_settings, u32 t_segment) { t_settings->caret_style = enum_at<CaretStyle>(t_segment); },
	},
	{
		.row    = &Rows::start_with_system,
		.labels = K_STARTUP_LABELS,
		.chosen = [](const Settings* t_settings) -> u32 {
			if (!t_settings->start_with_system) return 0;
			return t_settings->start_minimized ? 1 : 2;
		},
		.choose =
			[](Settings* t_settings, u32 t_segment) {
				t_settings->start_with_system = t_segment > 0;
				t_settings->start_minimized   = t_segment > 0 ? t_segment == 1 : t_settings->start_minimized;
			},
	},
	{
		.row    = &Rows::close_to_tray,
		.labels = K_CLOSE_CHOICE_LABELS,
		.chosen = [](const Settings* t_settings) -> u32 { return t_settings->close_to_tray ? 0 : 1; },
		.choose = [](Settings* t_settings, u32 t_segment) { t_settings->close_to_tray = t_segment == 0; },
	},
	{
		.row    = &Rows::renderer,
		.labels = K_RENDERER_LABELS,
		.chosen = [](const Settings* t_settings) { return enum_index(t_settings->renderer); },
		.choose = [](Settings* t_settings, u32 t_segment) { t_settings->renderer = enum_at<GraphicsApi>(t_segment); },
	},
}};

template <auto MEMBER>
auto SettingsPanel::restore_default(SettingsPanel* t_panel) -> void
{
	t_panel->m_settings->*MEMBER = K_DEFAULT_SETTINGS.*MEMBER;
}

const EnumArray<SettingsPanel::ResettableSetting, SettingsPanel::ResetSpec> SettingsPanel::K_RESETS{{
	{&Rows::theme, ResetAnchor::DROPDOWN, at_default<&Settings::theme>, [](SettingsPanel* t_panel) { t_panel->select_theme(K_DEFAULT_SETTINGS.theme); }},
	{&Rows::font, ResetAnchor::DROPDOWN, [](const Settings* t_settings) { return std::string_view{t_settings->font_name} == K_DEFAULT_SETTINGS.font_name; },
	 [](SettingsPanel* t_panel) { t_panel->restore_font(); }},
	{&Rows::font_size, ResetAnchor::STEPPER, at_default<&Settings::font_size>,
	 [](SettingsPanel* t_panel) { t_panel->restore_font_size(&Settings::font_size); }},
	{&Rows::secondary_font_size, ResetAnchor::STEPPER, at_default<&Settings::secondary_font_size>,
	 [](SettingsPanel* t_panel) { t_panel->restore_font_size(&Settings::secondary_font_size); }},
	{&Rows::accent, ResetAnchor::SWATCH, [](const Settings* t_settings) { return t_settings->accent == theme_preset(t_settings->theme).default_accent; },
	 [](SettingsPanel* t_panel) {
		 t_panel->m_settings->accent = theme_preset(t_panel->m_settings->theme).default_accent;
		 t_panel->m_color_picker.close();
	 }},
	{&Rows::corner_roundness, ResetAnchor::SLIDER, at_default<&Settings::corner_roundness>, restore_default<&Settings::corner_roundness>},
	{&Rows::background, ResetAnchor::PATTERN_SELECT, at_default<&Settings::background_style>, restore_default<&Settings::background_style>},
	{&Rows::background, ResetAnchor::SLIDER, at_default<&Settings::background_intensity>, restore_default<&Settings::background_intensity>},
	{&Rows::background_light, ResetAnchor::TOGGLE, at_default<&Settings::background_light>, restore_default<&Settings::background_light>},
	{&Rows::background_light, ResetAnchor::SLIDER, at_default<&Settings::background_light_intensity>, restore_default<&Settings::background_light_intensity>},
	{&Rows::background_grain, ResetAnchor::TOGGLE, at_default<&Settings::background_grain>, restore_default<&Settings::background_grain>},
	{&Rows::background_grain, ResetAnchor::SLIDER, at_default<&Settings::background_grain_intensity>, restore_default<&Settings::background_grain_intensity>},
	{&Rows::snow, ResetAnchor::TOGGLE, at_default<&Settings::snow>, restore_default<&Settings::snow>},
	{&Rows::caret_style, ResetAnchor::CHOICE, at_default<&Settings::caret_style>, restore_default<&Settings::caret_style>},
	{&Rows::caret_trail, ResetAnchor::TOGGLE, at_default<&Settings::caret_trail>, restore_default<&Settings::caret_trail>},
	{&Rows::caret_trail, ResetAnchor::SLIDER, at_default<&Settings::caret_trail_strength>, restore_default<&Settings::caret_trail_strength>},
	{&Rows::glass, ResetAnchor::TOGGLE, at_default<&Settings::glass>, restore_default<&Settings::glass>},
	{&Rows::glass, ResetAnchor::SLIDER, at_default<&Settings::glass_blur>, restore_default<&Settings::glass_blur>},
	{&Rows::glass_tint, ResetAnchor::SLIDER, at_default<&Settings::glass_tint>, restore_default<&Settings::glass_tint>},
	{&Rows::animations, ResetAnchor::TOGGLE, at_default<&Settings::animations_enabled>, restore_default<&Settings::animations_enabled>},
	{&Rows::animations, ResetAnchor::SLIDER, at_default<&Settings::animation_speed>, restore_default<&Settings::animation_speed>},
	{&Rows::notifications, ResetAnchor::TOGGLE, at_default<&Settings::show_notifications>, restore_default<&Settings::show_notifications>},
	{&Rows::start_with_system, ResetAnchor::CHOICE,
	 [](const Settings* t_settings) { return at_default<&Settings::start_with_system>(t_settings) && at_default<&Settings::start_minimized>(t_settings); },
	 [](SettingsPanel* t_panel) {
		 restore_default<&Settings::start_with_system>(t_panel);
		 restore_default<&Settings::start_minimized>(t_panel);
	 }},
	{&Rows::close_to_tray, ResetAnchor::CHOICE, at_default<&Settings::close_to_tray>, restore_default<&Settings::close_to_tray>},
	{&Rows::renderer, ResetAnchor::CHOICE, at_default<&Settings::renderer>, restore_default<&Settings::renderer>},
	{&Rows::riot_client, ResetAnchor::BROWSE_BUTTON, [](const Settings* t_settings) { return t_settings->riot_client_path[0] == '\0'; },
	 [](SettingsPanel* t_panel) {
		 t_panel->m_settings->riot_client_path[0] = '\0';
		 t_panel->m_commands->push(Command{.type = CommandType::SAVE_CHANGES});
	 }},
	{&Rows::hide_from_capture, ResetAnchor::TOGGLE, at_default<&Settings::hide_from_capture>, restore_default<&Settings::hide_from_capture>},
	{&Rows::block_overlay_injection, ResetAnchor::TOGGLE, at_default<&Settings::block_overlay_injection>, restore_default<&Settings::block_overlay_injection>},
	{&Rows::auto_lock, ResetAnchor::SLIDER, at_default<&Settings::auto_lock_minutes>, restore_default<&Settings::auto_lock_minutes>},
}};

SettingsPanel::SettingsPanel(Settings*         t_settings,
                             Fonts*            t_fonts,
                             Renderer*         t_renderer,
                             const os::Window* t_window,
                             const Assets*     t_assets,
                             CommandQueue*     t_commands)
	: m_settings(t_settings)
	, m_fonts(t_fonts)
	, m_renderer(t_renderer)
	, m_window(t_window)
	, m_assets(t_assets)
	, m_commands(t_commands)
	, m_font_list(t_fonts,
	              t_assets,
	              t_settings,
	              ListPopupOptions{
					  .search_placeholder = "Search fonts...",
					  .empty_message      = "No fonts match your search",
					  .min_width          = K_FONT_LIST_WIDTH,
				  })
	, m_theme_list(t_fonts,
	               t_assets,
	               t_settings,
	               ListPopupOptions{
					   .search_placeholder = "Search themes",
					   .empty_message      = "No themes match your search",
					   .min_width          = K_THEME_LIST_WIDTH,
					   .preview_size       = K_THEME_PREVIEW_SIZE,
					   .draw_preview =
						   [](DrawList* t_draw_list, Rect t_preview, u32 t_theme, Color t_backdrop, u8 t_alpha) {
							   draw_theme_preview(t_draw_list, t_preview, enum_at<ThemeKind>(t_theme), t_backdrop, t_alpha);
						   },
					   .hover_preview_seconds = K_THEME_HOVER_PREVIEW_SECONDS,
				   })
	, m_color_picker(t_fonts, t_assets)
{
	m_search.set_max_length(K_SEARCH_MAX_LENGTH);
	m_search.set_placeholder("Search settings");

	sync_with_settings();
	start_font_scan();
}

auto SettingsPanel::sync_with_settings() -> void
{
	refresh_font_label();

	m_font_size_shown           = m_settings->font_size;
	m_secondary_font_size_shown = m_settings->secondary_font_size;
	m_accent_shown[0]           = m_settings->accent.r;
	m_accent_shown[1]           = m_settings->accent.g;
	m_accent_shown[2]           = m_settings->accent.b;

	for (const SliderKind slider : each<SliderKind>()) {
		m_slider_shown[slider]  = slider_value(slider);
		m_slider_reveal[slider] = slider_offered(slider) ? 1.0f : 0.0f;
	}

	for (const ChoiceKind choice : each<ChoiceKind>()) {
		m_choice_shown[choice] = static_cast<float>(K_CHOICES[choice].chosen(m_settings));
	}

	for (u32 i = 0; i < K_TOGGLE_COUNT; i += 1) {
		m_toggles_shown[i] = m_settings->*K_TOGGLES[i].value ? 1.0f : 0.0f;
	}

	for (const BackgroundStyle style : each<BackgroundStyle>()) {
		m_pattern_ring[style] = style == m_settings->background_style ? 1.0f : 0.0f;
	}
}

auto SettingsPanel::restore_committed_previews(Settings* t_settings) const -> void
{
	if (m_theme_before_preview) {
		t_settings->theme  = m_theme_before_preview->theme;
		t_settings->accent = m_theme_before_preview->accent;
	}
}

auto SettingsPanel::open() -> void
{
	m_open = true;
	settle_resets();
	m_search.set_focused(false);
	clear_search();

	start_font_scan();
	refresh_font_label();
}

auto SettingsPanel::close() -> void
{
	m_open = false;
	m_search.set_focused(false);
	for (Draggable& drag : m_slider_drags) {
		drag.end();
	}
	m_font_list.close();
	m_theme_list.close();
	m_color_picker.close();
	m_pattern_open = false;
	m_tooltip.reset();
}

auto SettingsPanel::layout() const -> SettingsPanel::Layout
{
	const Vec2  window = m_window->size();
	const float size_scale =
		std::max({1.0f, m_fonts->body.pixel_height / K_REFERENCE_BODY_PIXEL_HEIGHT, m_fonts->secondary.pixel_height / K_REFERENCE_SECONDARY_PIXEL_HEIGHT});
	const Vec2 max_size{K_PANEL_MAX_SIZE.x * size_scale, K_PANEL_MAX_SIZE.y * size_scale};

	float width  = std::min(max_size.x, std::max(0.0f, window.x - K_PANEL_MARGIN * 2.0f));
	float height = std::min(max_size.y, std::max(0.0f, window.y - K_PANEL_MARGIN * 2.0f));

	const float aspect = max_size.x / max_size.y;
	if (width / height > aspect) {
		width = height * aspect;
	} else {
		height = width / aspect;
	}

	Layout result{};
	result.docked = width < K_PANEL_MIN_SIZE.x * size_scale || height < K_PANEL_MIN_SIZE.y * size_scale;
	result.panel  = result.docked ? content_rect(m_window->size()).inset(0.0f, 1.0f) : Rect{0.0f, 0.0f, window.x, window.y}.centered(width, height);
	result.inner  = result.panel.inset(K_PANEL_BORDER);

	Rect remaining     = result.inner;
	result.header      = remaining.split_top(header_height(m_fonts));
	result.rail        = Rect{remaining.x, remaining.y, K_RAIL_WIDTH, remaining.h};
	result.rows_region = Rect{result.rail.right() + 1.0f, remaining.y, remaining.right() - result.rail.right() - 1.0f, remaining.h};

	return result;
}

auto SettingsPanel::rows(const Layout& t_layout) const -> SettingsPanel::Rows
{
	const Rect hidden{0.0f, -1.0e6f, 0.0f, 0.0f};
	const Rect region = t_layout.rows_region;

	Rect        cursor{region.x + K_CARD_MARGIN_X, region.y + K_ROWS_TOP_PADDING - m_rows_scroll.offset(), region.w - K_CARD_MARGIN_X * 2.0f, 1.0e6f};
	const float top = cursor.y;

	Rows result{};
	for (const SettingsGroup group : each<SettingsGroup>()) {
		result.cards[group]        = hidden;
		result.group_titles[group] = hidden;
	}

	std::optional<SettingsGroup> open_group;
	float                        card_top = 0.0f;

	const auto close_card = [&]() {
		if (open_group) {
			result.cards[*open_group] = Rect{cursor.x, card_top, cursor.w, cursor.y - card_top};
		}
	};

	for (const RowSpec& spec : K_ROW_SPECS) {
		const float height = row_extent(spec);

		if (!is_listed(spec) || height <= 0.0f) {
			result.*spec.row = hidden;
			continue;
		}

		if (open_group != spec.group) {
			close_card();

			if (open_group) {
				cursor.split_top(K_GROUP_GAP);
			}

			result.group_titles[spec.group] = cursor.split_top(group_title_height(m_fonts));
			open_group                      = spec.group;
			card_top                        = cursor.y;
		}

		result.*spec.row = cursor.split_top(height);
		result.listed_count += 1;
	}

	close_card();
	result.content_height = cursor.y - top + K_ROWS_TOP_PADDING * 2.0f;
	result.row_width      = cursor.w;

	return result;
}

auto SettingsPanel::spec_of(Rect Rows::* t_row) -> const SettingsPanel::RowSpec&
{
	for (const RowSpec& spec : K_ROW_SPECS) {
		if (spec.row == t_row) return spec;
	}

	return K_ROW_SPECS[0];
}

auto SettingsPanel::is_searching() const -> bool
{
	return !m_search.value().empty();
}

auto SettingsPanel::matches_search(const RowSpec& t_spec) const -> bool
{
	const GroupSpec&       group = K_GROUPS[t_spec.group];
	const std::string_view fields[]{t_spec.title, t_spec.description, t_spec.keywords, K_TAB_NAMES[group.tab], group.title};

	return matches_every_word(m_search.value(), fields);
}

auto SettingsPanel::is_listed(const RowSpec& t_spec) const -> bool
{
	if (t_spec.row == &Rows::start_with_system && !os::installation::is_supported()) return false;

	return is_searching() ? matches_search(t_spec) : K_GROUPS[t_spec.group].tab == m_tab;
}

// Glass tint folds out under Frosted glass, opening and closing with it.
auto SettingsPanel::row_extent(const RowSpec& t_spec) const -> float
{
	const float height = *t_spec.description == '\0' ? single_row_height(m_fonts) : row_height(m_fonts);

	return t_spec.row == &Rows::glass_tint ? height * m_slider_reveal[SliderKind::GLASS_TINT] : height;
}

auto SettingsPanel::slider_on(Rect Rows::* t_row) -> std::optional<SettingsPanel::SliderKind>
{
	for (const SliderKind slider : each<SliderKind>()) {
		if (K_SLIDERS[slider].row == t_row) return slider;
	}

	return std::nullopt;
}

// A slider shows while the setting it fine-tunes is turned on.
auto SettingsPanel::slider_offered(SliderKind t_slider) const -> bool
{
	const SliderSpec& spec = K_SLIDERS[t_slider];

	return spec.shown == nullptr || spec.shown(m_settings);
}

auto SettingsPanel::inline_slider_left(Rect t_row) const -> float
{
	float widest = 0.0f;

	for (const SliderSpec& slider : K_SLIDERS) {
		if (slider.place != SliderPlace::OWN_ROW) {
			widest = std::max(widest, text_width(m_fonts->body, spec_of(slider.row).title));
		}
	}

	return snapped_to_pixel(t_row.x + K_ROW_INSET_X + widest + K_LABEL_CONTROL_GAP + K_RESET_COLUMN);
}

auto SettingsPanel::slider_rect(const Rows& t_rows, SliderKind t_slider) const -> Rect
{
	const SliderSpec& spec = K_SLIDERS[t_slider];
	const Rect        row  = t_rows.*spec.row;

	switch (spec.place) {
		using enum SliderPlace;

		case OWN_ROW: {
			return slider_control_rect(row, m_fonts);
		}

		case INLINE: {
			return inline_slider_rect(row, pattern_select_rect(row, m_fonts), inline_slider_left(row), m_fonts);
		}

		case BESIDE_TOGGLE: {
			return inline_slider_rect(row, toggle_rect(row), inline_slider_left(row), m_fonts);
		}
	}

	return Rect{};
}

auto SettingsPanel::slider_hit_rect(const Rows& t_rows, SliderKind t_slider) const -> Rect
{
	const Rect row    = t_rows.*K_SLIDERS[t_slider].row;
	const Rect slider = slider_rect(t_rows, t_slider);

	return Rect{slider.x, row.y, slider.w, row.h};
}

// The setting a slider shows, in the setting's own units. Auto-lock's is the stop it sits at.
auto SettingsPanel::slider_value(SliderKind t_slider) const -> float
{
	if (t_slider == SliderKind::AUTO_LOCK) return static_cast<float>(auto_lock_stop(m_settings->auto_lock_minutes));

	return m_settings->*K_SLIDERS[t_slider].value;
}

auto SettingsPanel::slider_fraction(SliderKind t_slider) const -> float
{
	const float shown = m_slider_shown[t_slider];

	if (t_slider == SliderKind::AUTO_LOCK) return shown / static_cast<float>(K_AUTO_LOCK_STOP_COUNT - 1);

	const SliderSpec& spec = K_SLIDERS[t_slider];

	return fraction_in(shown, spec.min, spec.max);
}

auto SettingsPanel::slider_readout(SliderKind t_slider, char (&t_buffer)[16]) const -> std::string_view
{
	if (t_slider == SliderKind::AUTO_LOCK) return auto_lock_label(m_settings->auto_lock_minutes, t_buffer);

	const float shown   = m_slider_shown[t_slider];
	const int   written = t_slider == SliderKind::ANIMATION_SPEED ? std::snprintf(t_buffer, sizeof(t_buffer), "%.2fx", shown)
	                                                              : std::snprintf(t_buffer, sizeof(t_buffer), "%.0f%%", shown * 100.0f);

	return std::string_view{t_buffer, static_cast<usize>(std::max(written, 0))};
}

auto SettingsPanel::slider_at(const Layout& t_layout, const Rows& t_rows, Vec2 t_point) const -> std::optional<SettingsPanel::SliderKind>
{
	for (const SliderKind slider : each<SliderKind>()) {
		if (m_slider_reveal[slider] > 0.5f && hits(t_layout, t_rows.*K_SLIDERS[slider].row, slider_hit_rect(t_rows, slider), t_point)) {
			return slider;
		}
	}

	return std::nullopt;
}

auto SettingsPanel::apply_slider(SliderKind t_slider, float t_fraction) -> void
{
	if (t_slider == SliderKind::AUTO_LOCK) {
		const auto stop               = static_cast<u32>(std::lround(t_fraction * static_cast<float>(K_AUTO_LOCK_STOP_COUNT - 1)));
		m_settings->auto_lock_minutes = K_AUTO_LOCK_STOPS[std::min(stop, K_AUTO_LOCK_STOP_COUNT - 1)];
		return;
	}

	const SliderSpec& spec  = K_SLIDERS[t_slider];
	m_settings->*spec.value = value_at(t_fraction, spec.min, spec.max);

	if (t_slider == SliderKind::ANIMATION_SPEED) {
		animation::set_speed(m_settings->animation_speed);
	}
}

auto SettingsPanel::choice_on(Rect Rows::* t_row) -> std::optional<SettingsPanel::ChoiceKind>
{
	for (const ChoiceKind choice : each<ChoiceKind>()) {
		if (K_CHOICES[choice].row == t_row) return choice;
	}

	return std::nullopt;
}

auto SettingsPanel::choice_rect(const Rows& t_rows, ChoiceKind t_choice) const -> Rect
{
	const ChoiceSpec& spec = K_CHOICES[t_choice];

	return segment_choice_rect(t_rows.*spec.row, spec.labels);
}

auto SettingsPanel::pattern_tile_size() const -> Vec2
{
	const Font& secondary = m_fonts->secondary;
	float       width     = K_TILE_MIN_WIDTH;

	for (const OptionLabel& label : K_BACKGROUND_LABELS) {
		width = std::max(width, std::ceil(text_width(secondary, label.name) + K_TILE_LABEL_MARGIN));
	}

	return Vec2{width, std::round(width * K_TILE_ASPECT) + K_TILE_LABEL_GAP + secondary.line_height()};
}

auto SettingsPanel::pattern_popup_rect(const Rows& t_rows) const -> Rect
{
	const Vec2  tile  = pattern_tile_size();
	const auto  count = static_cast<u32>(K_ENUM_COUNT<BackgroundStyle>);
	const u32   lines = (count + K_PATTERN_COLUMNS - 1) / K_PATTERN_COLUMNS;
	const float width =
		K_PATTERN_POPUP_PADDING * 2.0f + static_cast<float>(K_PATTERN_COLUMNS) * tile.x + static_cast<float>(K_PATTERN_COLUMNS - 1) * K_TILE_GAP;
	const float height = K_PATTERN_POPUP_PADDING * 2.0f + static_cast<float>(lines) * tile.y + static_cast<float>(lines - 1) * K_TILE_ROW_GAP;
	const Rect  anchor = pattern_select_rect(t_rows.background, m_fonts);
	const Rect  bounds = content_rect(m_window->size()).inset(K_PATTERN_POPUP_MARGIN);

	float y = anchor.bottom() + K_PATTERN_POPUP_GAP;
	if (y + height > bounds.bottom()) {
		y = anchor.y - K_PATTERN_POPUP_GAP - height;
	}

	const float x = std::clamp(anchor.right() - width, bounds.x, std::max(bounds.x, bounds.right() - width));
	y             = std::clamp(y, bounds.y, std::max(bounds.y, bounds.bottom() - height));

	return Rect{snapped_to_pixel(x), snapped_to_pixel(y), width, height};
}

auto SettingsPanel::pattern_tile(Rect t_popup, BackgroundStyle t_style) const -> Rect
{
	const Vec2 tile   = pattern_tile_size();
	const u32  index  = enum_index(t_style);
	const auto column = static_cast<float>(index % K_PATTERN_COLUMNS);
	const auto line   = static_cast<float>(index / K_PATTERN_COLUMNS);

	return Rect{t_popup.x + K_PATTERN_POPUP_PADDING + column * (tile.x + K_TILE_GAP), t_popup.y + K_PATTERN_POPUP_PADDING + line * (tile.y + K_TILE_ROW_GAP),
	            tile.x, tile.y};
}

auto SettingsPanel::pattern_at(Rect t_popup, Vec2 t_point) const -> std::optional<BackgroundStyle>
{
	for (const BackgroundStyle style : each<BackgroundStyle>()) {
		if (pattern_tile(t_popup, style).contains(t_point)) return style;
	}

	return std::nullopt;
}

auto SettingsPanel::segment_choice_rect(Rect t_row, std::span<const std::string_view> t_labels) const -> Rect
{
	float width = K_SEGMENT_INSET * 2.0f;
	for (const std::string_view label : t_labels) {
		width += text_width(m_fonts->body, label) + K_SEGMENT_PADDING_X * 2.0f;
	}

	return right_aligned_control(t_row, width, control_height(m_fonts));
}

auto SettingsPanel::choice_segment(Rect t_choice, std::span<const std::string_view> t_labels, u32 t_index) const -> Rect
{
	const Rect inner = t_choice.inset(K_SEGMENT_INSET);
	float      x     = inner.x;

	for (u32 i = 0; i < t_index; i += 1) {
		x += text_width(m_fonts->body, t_labels[i]) + K_SEGMENT_PADDING_X * 2.0f;
	}

	const bool  last  = t_index + 1 == t_labels.size();
	const float width = last ? inner.right() - x : text_width(m_fonts->body, t_labels[t_index]) + K_SEGMENT_PADDING_X * 2.0f;

	return Rect{x, inner.y, width, inner.h};
}

auto SettingsPanel::search_rect(const Layout& t_layout) const -> Rect
{
	const Rect& header = t_layout.header;
	const float left   = back_button_rect(header).right() + K_TITLE_GAP + text_width(m_fonts->body, K_PANEL_TITLE) + K_SEARCH_HEADER_GAP;
	const float right  = header.right() - K_ROW_PADDING_X;
	const float width  = std::min(K_SEARCH_MAX_WIDTH, right - left);
	if (width < K_SEARCH_MIN_WIDTH) return Rect{};

	const float height = std::max(26.0f, m_fonts->secondary.line_height() + 9.0f);

	return Rect{snapped_to_pixel(left + (right - left - width) * 0.5f), header.center().y - height * 0.5f, width, height};
}

auto SettingsPanel::refresh_search() -> void
{
	const std::string_view query = m_search.value();
	if (query == std::string_view{m_applied_query}) return;

	copy_to(query, m_applied_query);
	m_rows_scroll = Scrollable{};
	m_color_picker.close();
	m_pattern_open = false;
	m_tooltip.reset();
}

auto SettingsPanel::clear_search() -> void
{
	m_search.set_value("");
	refresh_search();
}

auto SettingsPanel::focus_search() -> void
{
	m_search.set_focused(true);
	m_search.apply(TextEdit::SELECT_ALL);
}

auto SettingsPanel::tab_rect(const Layout& t_layout, SettingsTab t_tab) const -> Rect
{
	const float height = tab_height(m_fonts);
	const Rect& rail   = t_layout.rail;

	return Rect{rail.x + K_RAIL_PADDING, rail.y + K_RAIL_PADDING + static_cast<float>(enum_index(t_tab)) * height, rail.w - K_RAIL_PADDING * 2.0f, height};
}

auto SettingsPanel::tab_at(const Layout& t_layout, Vec2 t_point) const -> std::optional<SettingsPanel::SettingsTab>
{
	for (const SettingsTab tab : each<SettingsTab>()) {
		if (tab_rect(t_layout, tab).contains(t_point)) return tab;
	}

	return std::nullopt;
}

auto SettingsPanel::select_tab(SettingsTab t_tab) -> void
{
	if (t_tab == m_tab) return;

	m_tab         = t_tab;
	m_rows_scroll = Scrollable{};
	m_color_picker.close();
	m_pattern_open = false;
	m_tooltip.reset();
}

auto SettingsPanel::rows_scroll(const Layout& t_layout, const Rows& t_rows) const -> ScrollGeometry
{
	const Rect  region        = t_layout.rows_region;
	const float bottom_margin = t_layout.docked ? K_SCROLLBAR_MARGIN : std::max(K_SCROLLBAR_MARGIN, K_PANEL_RADIUS * controls::K_POPUP_ROUNDNESS * 0.75f);
	const Rect  track{region.right() - K_SCROLLBAR_WIDTH - K_SCROLLBAR_MARGIN, region.y + K_SCROLLBAR_MARGIN, K_SCROLLBAR_WIDTH,
	                  std::max(0.0f, region.h - K_SCROLLBAR_MARGIN - bottom_margin)};

	return ScrollGeometry{track, t_rows.content_height, region.h};
}

auto SettingsPanel::is_on_screen(const Layout& t_layout, Rect t_row) const -> bool
{
	return t_row.overlaps_vertically(t_layout.rows_region);
}

auto SettingsPanel::hits(const Layout& t_layout, Rect t_row, Rect t_control, Vec2 t_point) const -> bool
{
	return is_on_screen(t_layout, t_row) && t_layout.rows_region.contains(t_point) && t_control.contains(t_point);
}

auto SettingsPanel::open_list() -> ListPopup*
{
	if (m_font_list.is_open()) return &m_font_list;
	if (m_theme_list.is_open()) return &m_theme_list;

	return nullptr;
}

auto SettingsPanel::open_list() const -> const ListPopup*
{
	if (m_font_list.is_open()) return &m_font_list;
	if (m_theme_list.is_open()) return &m_theme_list;

	return nullptr;
}

auto SettingsPanel::has_popup_open() const -> bool
{
	return m_color_picker.is_open() || m_pattern_open || open_list() != nullptr;
}

auto SettingsPanel::is_row_hovered(const Layout& t_layout, Rect t_row) const -> bool
{
	return is_blocking() && !has_popup_open() && t_layout.rows_region.contains(m_mouse) && is_on_screen(t_layout, t_row) &&
	       t_row.inset(K_HIGHLIGHT_INSET_X, K_HIGHLIGHT_INSET_Y).contains(m_mouse);
}

auto SettingsPanel::reset_control(const Rows& t_rows, ResettableSetting t_setting) const -> Rect
{
	const ResetSpec& spec = K_RESETS[t_setting];
	const Rect       row  = t_rows.*spec.row;

	switch (spec.anchor) {
		using enum ResetAnchor;

		case DROPDOWN: {
			return dropdown_rect(row, m_fonts);
		}

		case PATTERN_SELECT: {
			return pattern_select_rect(row, m_fonts);
		}

		case STEPPER: {
			return stepper_rect(row, m_fonts);
		}

		case SWATCH: {
			return swatch_rect(row).inset(-(K_SWATCH_RING_GAP + K_SWATCH_RING));
		}

		case SLIDER: {
			const std::optional<SliderKind> slider = slider_on(spec.row);
			return slider ? slider_rect(t_rows, *slider) : Rect{};
		}

		case CHOICE: {
			const std::optional<ChoiceKind> choice = choice_on(spec.row);
			return choice ? choice_rect(t_rows, *choice) : Rect{};
		}

		case TOGGLE: {
			return toggle_rect(row);
		}

		case BROWSE_BUTTON: {
			return riot_client_button_rect(row, m_fonts);
		}
	}

	return Rect{};
}

auto SettingsPanel::reset_button(const Rows& t_rows, ResettableSetting t_setting) const -> Rect
{
	return reset_button_rect(reset_control(t_rows, t_setting), t_rows.*K_RESETS[t_setting].row);
}

auto SettingsPanel::reset_slider(ResettableSetting t_setting) const -> std::optional<SettingsPanel::SliderKind>
{
	const ResetSpec& spec = K_RESETS[t_setting];

	return spec.anchor == ResetAnchor::SLIDER ? slider_on(spec.row) : std::nullopt;
}

// Resets for sliders that share a row with another control only show while the pointer is on that row, so a row never shows two at rest.
auto SettingsPanel::shows_reset_on_hover(ResettableSetting t_setting) const -> bool
{
	const std::optional<SliderKind> slider = reset_slider(t_setting);

	return slider && K_SLIDERS[*slider].place != SliderPlace::OWN_ROW;
}

auto SettingsPanel::settle_resets() -> void
{
	for (const ResettableSetting setting : each<ResettableSetting>()) {
		if (!shows_reset_on_hover(setting)) {
			m_reset_visible[setting] = can_reset(setting) ? 1.0f : 0.0f;
		}
	}
}

auto SettingsPanel::can_reset(ResettableSetting t_setting) const -> bool
{
	if (K_RESETS[t_setting].is_default(m_settings)) return false;

	const std::optional<SliderKind> slider = reset_slider(t_setting);

	return !slider || m_slider_reveal[*slider] > 0.5f;
}

auto SettingsPanel::reset_to_default(ResettableSetting t_setting) -> void
{
	m_reset_spin[t_setting] = 1.0f;
	K_RESETS[t_setting].restore(this);

	animation::set_enabled(m_settings->animations_enabled);
	animation::set_speed(m_settings->animation_speed);
}

auto SettingsPanel::load_fonts(std::string_view t_file) -> bool
{
	return m_fonts->load(m_renderer, t_file, m_settings->font_size, m_settings->secondary_font_size, m_window->dpi_scale());
}

auto SettingsPanel::open_font_list() -> void
{
	m_color_picker.close();
	take_font_scan(true);
	m_font_names.assign(m_installed_fonts.names.begin(), m_installed_fonts.names.end());
	m_font_list.open(m_font_names, m_installed_fonts.index_of_file(m_settings->font_name));
}

auto SettingsPanel::open_theme_list() -> void
{
	m_color_picker.close();
	m_theme_before_preview = ThemeChoice{m_settings->theme, m_settings->accent};
	m_theme_list.open(K_THEME_NAMES.items, enum_index(m_settings->theme));
}

auto SettingsPanel::choose_font(u32 t_index) -> void
{
	const std::string& file = m_installed_fonts.files[t_index];
	if (!load_fonts(file)) return;

	copy_to(file, m_settings->font_name);
	refresh_font_label();
}

auto SettingsPanel::restore_font() -> void
{
	if (!load_fonts(K_DEFAULT_SETTINGS.font_name)) return;

	copy_to(K_DEFAULT_SETTINGS.font_name, m_settings->font_name);
	refresh_font_label();
}

auto SettingsPanel::restore_font_size(float Settings::* t_size) -> void
{
	m_settings->*t_size = K_DEFAULT_SETTINGS.*t_size;
	load_fonts(m_settings->font_name);
}

// Listing every installed font takes a moment on macOS, so it happens off the UI thread: once at startup and again whenever settings
// opens, which picks up fonts installed since.
auto SettingsPanel::start_font_scan() -> void
{
	if (m_font_scan.valid()) return;

	m_font_scan = std::async(std::launch::async, installed_fonts);
}

auto SettingsPanel::take_font_scan(bool t_wait) -> void
{
	if (!m_font_scan.valid()) return;
	if (!t_wait && m_font_scan.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;

	m_installed_fonts = m_font_scan.get();
	refresh_font_label();
}

auto SettingsPanel::refresh_font_label() -> void
{
	const std::string_view file = m_settings->font_name;

	if (const std::optional<u32> index = m_installed_fonts.index_of_file(file)) {
		m_font_label = m_installed_fonts.names[*index];
		return;
	}

	const usize name_start = file.find_last_of("\\/") + 1;
	m_font_label           = file.substr(name_start, file.rfind('.') - name_start);
}

auto SettingsPanel::show_theme(ThemeChoice t_choice) -> void
{
	m_settings->theme  = t_choice.theme;
	m_settings->accent = t_choice.accent;
	m_color_picker.close();
	fade_to_theme(t_choice.theme);
}

auto SettingsPanel::select_theme(ThemeKind t_theme) -> void
{
	m_theme_before_preview.reset();
	show_theme(ThemeChoice{t_theme, theme_preset(t_theme).default_accent});
}

auto SettingsPanel::choose_theme(ThemeKind t_theme) -> void
{
	const bool  kept_original = m_theme_before_preview && m_theme_before_preview->theme == t_theme;
	const Color accent        = kept_original ? m_theme_before_preview->accent : theme_preset(t_theme).default_accent;

	m_theme_before_preview.reset();
	show_theme(ThemeChoice{t_theme, accent});
}

auto SettingsPanel::update_theme_preview() -> void
{
	if (!m_theme_before_preview) return;

	const ThemeChoice original = *m_theme_before_preview;
	ThemeChoice       shown    = original;

	if (!m_theme_list.is_open()) {
		m_theme_before_preview.reset();
	} else if (const std::optional<u32> previewed = m_theme_list.previewed_item()) {
		const auto kind = enum_at<ThemeKind>(*previewed);
		if (kind != original.theme) {
			shown = ThemeChoice{kind, theme_preset(kind).default_accent};
		}
	}

	if (shown != ThemeChoice{m_settings->theme, m_settings->accent}) {
		show_theme(shown);
	}
}

auto SettingsPanel::choose_background(BackgroundStyle t_style) -> void
{
	m_pattern_open               = false;
	m_settings->background_style = t_style;
}

auto SettingsPanel::pull_picked_color() -> void
{
	if (m_color_picker.take_changed()) {
		m_settings->accent = m_color_picker.color();
	}
}

auto SettingsPanel::step_font_size(Rect t_stepper, float* t_value, float t_min, float t_max, Vec2 t_point) -> void
{
	if (stepper_minus(t_stepper).contains(t_point)) {
		*t_value = std::max(t_min, *t_value - 1.0f);
		load_fonts(m_settings->font_name);
	} else if (stepper_plus(t_stepper).contains(t_point)) {
		*t_value = std::min(t_max, *t_value + 1.0f);
		load_fonts(m_settings->font_name);
	}
}

auto SettingsPanel::update(float t_delta_seconds) -> void
{
	if (m_font_scan.valid()) {
		take_font_scan(false);
		animation::request_frame();
	}

	const float scale_travel = m_window->size().x * 0.5f * (1.0f - K_PANEL_CLOSED_SCALE);
	m_open_amount = animation::ease_toward(m_open_amount, m_open ? 1.0f : 0.0f, K_OPEN_EASE_RATE, t_delta_seconds, animation::K_SETTLED_PIXELS / scale_travel);

	for (u32 i = 0; i < K_TOGGLE_COUNT; i += 1) {
		const float target = m_settings->*K_TOGGLES[i].value ? 1.0f : 0.0f;
		m_toggles_shown[i] = animation::ease_toward(m_toggles_shown[i], target, K_TOGGLE_EASE_RATE, t_delta_seconds);
	}

	const auto ease_value = [t_delta_seconds](float t_shown, float t_actual) {
		return animation::ease_toward(t_shown, t_actual, K_VALUE_EASE_RATE, t_delta_seconds);
	};

	m_font_size_shown           = ease_value(m_font_size_shown, m_settings->font_size);
	m_secondary_font_size_shown = ease_value(m_secondary_font_size_shown, m_settings->secondary_font_size);

	// A slider being dragged follows the pointer exactly. Otherwise it eases to the setting, as when it is reset.
	for (const SliderKind slider : each<SliderKind>()) {
		const float actual = slider_value(slider);

		m_slider_shown[slider]  = m_slider_drags[slider].is_pressed() ? actual : ease_value(m_slider_shown[slider], actual);
		m_slider_reveal[slider] = animation::ease_toward(m_slider_reveal[slider], slider_offered(slider) ? 1.0f : 0.0f, K_REVEAL_EASE_RATE, t_delta_seconds);
	}

	for (const ChoiceKind choice : each<ChoiceKind>()) {
		const auto chosen      = static_cast<float>(K_CHOICES[choice].chosen(m_settings));
		m_choice_shown[choice] = animation::ease_toward(m_choice_shown[choice], chosen, K_SEGMENT_SLIDE_RATE, t_delta_seconds);
	}

	for (const BackgroundStyle style : each<BackgroundStyle>()) {
		const float target    = style == m_settings->background_style ? 1.0f : 0.0f;
		m_pattern_ring[style] = animation::ease_toward(m_pattern_ring[style], target, K_TILE_RING_RATE, t_delta_seconds);
	}

	m_pattern_open_amount = animation::ease_toward(m_pattern_open_amount, m_pattern_open ? 1.0f : 0.0f, K_PATTERN_POPUP_RATE, t_delta_seconds);

	set_corner_roundness(m_slider_shown[SliderKind::CORNER_ROUNDNESS]);

	const u8 accent[3]{m_settings->accent.r, m_settings->accent.g, m_settings->accent.b};
	for (u32 channel = 0; channel < 3; channel += 1) {
		m_accent_shown[channel] = ease_value(m_accent_shown[channel], accent[channel]);
	}

	m_tab_indicator = animation::ease_toward(m_tab_indicator, static_cast<float>(enum_index(m_tab)), K_TAB_SLIDE_RATE, t_delta_seconds,
	                                         animation::K_SETTLED_PIXELS / tab_height(m_fonts));

	m_search.update(t_delta_seconds);
	refresh_search();

	m_color_picker.update(t_delta_seconds);
	pull_picked_color();

	update_hover_hints(t_delta_seconds);

	const Rows current_rows = rows(layout());
	const Rect popup_bounds = content_rect(m_window->size());

	m_font_list.update(t_delta_seconds, dropdown_rect(current_rows.font, m_fonts), popup_bounds);
	m_theme_list.update(t_delta_seconds, dropdown_rect(current_rows.theme, m_fonts), popup_bounds);
	update_theme_preview();

	m_rows_scroll.update(t_delta_seconds);
	m_tooltip.update(t_delta_seconds);
}

auto SettingsPanel::update_hover_hints(float t_delta_seconds) -> void
{
	const Layout current      = layout();
	const Rows   current_rows = rows(current);
	const bool   pointer_live = is_blocking() && !has_popup_open() && current.rows_region.contains(m_mouse);

	const Rect back = back_button_rect(current.header);
	if (is_blocking() && !has_popup_open() && back.contains(m_mouse)) {
		m_tooltip.request("Back", back);
	}

	if (const std::optional<ColorPickerHint> hint = m_color_picker.hint(m_mouse)) {
		m_tooltip.request(hint->text, hint->anchor);
	}

	for (const SliderKind slider : each<SliderKind>()) {
		const bool over    = pointer_live && m_slider_reveal[slider] > 0.5f &&
		                     hits(current, current_rows.*K_SLIDERS[slider].row, slider_hit_rect(current_rows, slider), m_mouse);
		const bool engaged = m_slider_drags[slider].is_pressed() || over;

		m_slider_hover[slider] = animation::ease_toward(m_slider_hover[slider], engaged ? 1.0f : 0.0f, K_SLIDER_HOVER_RATE, t_delta_seconds);
	}

	for (const ResettableSetting setting : each<ResettableSetting>()) {
		const bool resettable = can_reset(setting);
		const Rect row        = current_rows.*K_RESETS[setting].row;
		const bool shown      = shows_reset_on_hover(setting)
		                            ? pointer_live && resettable && is_row_hovered(current, Rect{row.x, row.y, current_rows.row_width, row.h})
		                            : resettable;

		m_reset_visible[setting] = animation::ease_toward(m_reset_visible[setting], shown ? 1.0f : 0.0f, K_RESET_APPEAR_RATE, t_delta_seconds);
		m_reset_spin[setting]    = animation::ease_toward(m_reset_spin[setting], 0.0f, K_RESET_SPIN_RATE, t_delta_seconds);

		const Rect button = reset_button(current_rows, setting);
		if (pointer_live && resettable && is_on_screen(current, row) && button.contains(m_mouse)) {
			m_tooltip.request("Reset to default", button);
		}
	}
}

auto SettingsPanel::on_pointer_down(Vec2 t_point) -> bool
{
	if (!is_blocking()) return false;

	if (ListPopup* list = open_list()) {
		list->on_pointer_down(t_point);
		return true;
	}

	if (m_pattern_open) return true;

	const Layout current    = layout();
	const Rect   search     = search_rect(current);
	const bool   over_clear = is_searching() && controls::search_clear_rect(search).contains(t_point);

	if (search.contains(t_point) && !over_clear) {
		m_color_picker.close();
		m_search.set_focused(true);
		m_search.on_pointer_down(m_fonts->secondary, controls::search_text_rect(search, K_SEARCH_INSET), t_point.x);
		return true;
	}

	if (!over_clear) {
		m_search.set_focused(false);
	}

	const Rows current_rows = rows(current);

	if (m_rows_scroll.on_pointer_down(t_point, rows_scroll(current, current_rows))) return true;

	if (m_color_picker.on_pointer_down(t_point)) {
		pull_picked_color();
		return true;
	}

	if (const std::optional<SliderKind> slider = slider_at(current, current_rows, t_point)) {
		m_slider_drags[*slider].begin(t_point);
		apply_slider(*slider, slider_fraction_at(slider_rect(current_rows, *slider), t_point.x));
	}

	return true;
}

auto SettingsPanel::on_pointer_move(Vec2 t_point) -> bool
{
	if (!is_blocking()) return false;

	if (ListPopup* list = open_list()) {
		list->on_pointer_move(t_point);
		return true;
	}

	const Layout current      = layout();
	const Rows   current_rows = rows(current);

	m_rows_scroll.on_pointer_move(t_point.y, rows_scroll(current, current_rows));

	m_color_picker.on_pointer_move(t_point);
	pull_picked_color();

	for (const SliderKind slider : each<SliderKind>()) {
		if (!m_slider_drags[slider].is_pressed()) continue;

		m_slider_drags[slider].update(t_point);
		apply_slider(slider, slider_fraction_at(slider_rect(current_rows, slider), t_point.x));
	}

	if (m_search.is_selecting()) {
		m_search.on_pointer_move(m_fonts->secondary, controls::search_text_rect(search_rect(current), K_SEARCH_INSET), t_point.x);
	}

	return true;
}

auto SettingsPanel::on_pointer_up(Vec2 t_point) -> bool
{
	if (!is_blocking()) return false;

	if (m_font_list.is_open()) {
		if (const std::optional<u32> chosen = m_font_list.on_pointer_up(t_point)) {
			choose_font(*chosen);
		}

		return true;
	}

	if (m_theme_list.is_open()) {
		if (const std::optional<u32> chosen = m_theme_list.on_pointer_up(t_point)) {
			choose_theme(enum_at<ThemeKind>(*chosen));
		}

		return true;
	}

	if (m_color_picker.on_pointer_up(t_point)) {
		pull_picked_color();
		return true;
	}

	const bool ended_drag = m_rows_scroll.is_dragging() || std::ranges::any_of(m_slider_drags, &Draggable::is_pressed) || m_search.is_selecting();

	m_rows_scroll.on_pointer_up();
	for (Draggable& drag : m_slider_drags) {
		drag.end();
	}

	m_search.on_pointer_up();

	if (!ended_drag && !m_color_picker.contains(t_point)) {
		handle_click(t_point);
	}

	return true;
}

auto SettingsPanel::handle_click(Vec2 t_point) -> void
{
	const Layout current = layout();

	if (m_pattern_open) {
		const Rect popup = pattern_popup_rect(rows(current));

		if (const std::optional<BackgroundStyle> pattern = pattern_at(popup, t_point)) {
			choose_background(*pattern);
		} else if (!popup.contains(t_point)) {
			m_pattern_open = false;
		}

		return;
	}

	if (back_button_rect(current.header).contains(t_point) || !current.panel.contains(t_point)) {
		close();
		return;
	}

	const Rect search = search_rect(current);
	if (is_searching() && controls::search_clear_rect(search).contains(t_point)) {
		clear_search();
		m_search.set_focused(true);
		return;
	}

	if (search.contains(t_point)) return;

	if (const std::optional<SettingsTab> tab = tab_at(current, t_point)) {
		clear_search();
		select_tab(*tab);
		return;
	}

	if (!current.rows_region.contains(t_point)) return;

	const Rows current_rows = rows(current);

	for (const ResettableSetting setting : each<ResettableSetting>()) {
		if (!has_popup_open() && can_reset(setting) && hits(current, current_rows.*K_RESETS[setting].row, reset_button(current_rows, setting), t_point)) {
			reset_to_default(setting);
			return;
		}
	}

	if (hits(current, current_rows.theme, dropdown_rect(current_rows.theme, m_fonts), t_point)) {
		open_theme_list();
		return;
	}

	if (hits(current, current_rows.font, dropdown_rect(current_rows.font, m_fonts), t_point)) {
		open_font_list();
		return;
	}

	if (hits(current, current_rows.background, pattern_select_rect(current_rows.background, m_fonts), t_point)) {
		m_color_picker.close();
		m_pattern_open = true;
		return;
	}

	for (const ChoiceKind choice : each<ChoiceKind>()) {
		const ChoiceSpec& spec = K_CHOICES[choice];
		const Rect        area = choice_rect(current_rows, choice);
		if (!hits(current, current_rows.*spec.row, area, t_point)) continue;

		for (u32 segment = 0; segment < spec.labels.size(); segment += 1) {
			if (choice_segment(area, spec.labels, segment).contains(t_point)) {
				spec.choose(m_settings, segment);
			}
		}

		return;
	}

	if (is_on_screen(current, current_rows.font_size)) {
		step_font_size(stepper_rect(current_rows.font_size, m_fonts), &m_settings->font_size, K_FONT_SIZE_MIN, K_FONT_SIZE_MAX, t_point);
	}

	if (is_on_screen(current, current_rows.secondary_font_size)) {
		step_font_size(stepper_rect(current_rows.secondary_font_size, m_fonts), &m_settings->secondary_font_size, K_SECONDARY_FONT_SIZE_MIN,
		               K_SECONDARY_FONT_SIZE_MAX, t_point);
	}

	for (const Toggle& toggle : K_TOGGLES) {
		const Rect row = current_rows.*toggle.row;

		if (hits(current, row, toggle_rect(row), t_point)) {
			m_settings->*toggle.value = !(m_settings->*toggle.value);
			animation::set_enabled(m_settings->animations_enabled);
		}
	}

	const Rect swatch = swatch_rect(current_rows.accent);
	if (hits(current, current_rows.accent, swatch, t_point)) {
		if (m_color_picker.is_open()) {
			m_color_picker.close();
		} else {
			m_color_picker.open(m_settings->accent, swatch, content_rect(m_window->size()));
		}

		return;
	}

	if (m_color_picker.is_open()) {
		m_color_picker.close();
		return;
	}

	const Rect browse_client = riot_client_button_rect(current_rows.riot_client, m_fonts);
	if (hits(current, current_rows.riot_client, browse_client, t_point)) {
		m_commands->push(Command{.type = CommandType::LOCATE_RIOT_CLIENT});
		return;
	}

	const Rect reset_password = master_password_button_rect(current_rows.master_password, m_fonts);
	if (hits(current, current_rows.master_password, reset_password, t_point)) {
		m_commands->push(Command{.type = CommandType::REQUEST_NEW_MASTER_PASSWORD});
		close();
	}
}

auto SettingsPanel::on_right_click(Vec2 t_point) -> bool
{
	if (!is_blocking()) return false;

	ListPopup* list = open_list();

	if (list == nullptr && m_color_picker.contains(t_point)) {
		if (TextInput* field = m_color_picker.on_right_click(t_point)) {
			m_commands->push(Command{.type = CommandType::SHOW_TEXT_MENU, .position = t_point, .text_input = field});
		}

		return true;
	}

	const Rect search = search_rect(layout());
	if (list == nullptr && search.contains(t_point)) {
		m_search.set_focused(true);
		m_search.on_right_click(m_fonts->secondary, controls::search_text_rect(search, K_SEARCH_INSET), t_point.x);
		m_commands->push(Command{.type = CommandType::SHOW_TEXT_MENU, .position = t_point, .text_input = &m_search});
		return true;
	}

	if (TextInput* list_search = list != nullptr ? list->on_right_click(t_point) : nullptr) {
		m_commands->push(Command{.type = CommandType::SHOW_TEXT_MENU, .position = t_point, .text_input = list_search});
	}

	return true;
}

auto SettingsPanel::on_scroll(Vec2 t_point, float t_wheel_delta) -> bool
{
	if (!is_blocking()) return false;

	if (ListPopup* list = open_list()) {
		list->on_scroll(t_wheel_delta);
		return true;
	}

	if (m_pattern_open) return true;

	const Layout current      = layout();
	const Rows   current_rows = rows(current);

	if (os::modifiers().shortcut && hits(current, current_rows.theme, dropdown_rect(current_rows.theme, m_fonts), t_point)) {
		m_theme_wheel += t_wheel_delta;

		while (std::fabs(m_theme_wheel) >= 1.0f) {
			const float notch = m_theme_wheel > 0.0f ? 1.0f : -1.0f;
			select_theme(stepped(m_settings->theme, notch > 0.0f ? -1 : 1));
			m_theme_wheel -= notch;
		}

		return true;
	}

	m_rows_scroll.on_scroll(t_wheel_delta, rows_scroll(current, current_rows));

	return true;
}

auto SettingsPanel::on_key_down(os::Key t_key) -> bool
{
	if (!is_blocking()) return false;

	if (m_font_list.is_open()) {
		if (const std::optional<u32> chosen = m_font_list.on_key_down(t_key)) {
			choose_font(*chosen);
		}
	} else if (m_theme_list.is_open()) {
		if (const std::optional<u32> chosen = m_theme_list.on_key_down(t_key)) {
			choose_theme(enum_at<ThemeKind>(*chosen));
		}
	} else if (m_pattern_open) {
		m_pattern_open = t_key != os::Key::ESCAPE;
	} else if (m_color_picker.on_key_down(t_key)) {
		pull_picked_color();
	} else if (t_key == os::Key::ESCAPE && is_searching()) {
		clear_search();
	} else if (t_key == os::Key::ESCAPE && m_search.is_focused()) {
		m_search.set_focused(false);
	} else if (t_key == os::Key::ESCAPE) {
		close();
	} else if (t_key == os::Key::TAB && os::modifiers().control) {
		clear_search();
		select_tab(stepped(m_tab, os::modifiers().shift ? -1 : 1));
	} else if (t_key == os::Key::F && os::modifiers().shortcut) {
		focus_search();
	} else if (m_search.is_focused()) {
		m_search.on_key_down(t_key);
	}

	return true;
}

auto SettingsPanel::on_char(u32 t_character) -> bool
{
	if (!is_blocking()) return false;

	if (ListPopup* list = open_list()) {
		list->on_char(t_character);
		return true;
	}

	if (m_color_picker.on_char(t_character)) {
		pull_picked_color();
		return true;
	}

	const bool printable = t_character >= 32 && t_character != 127;
	if (printable && !m_search.is_focused() && search_rect(layout()).w > 0.0f) {
		m_search.set_focused(true);
	}

	if (m_search.is_focused()) {
		m_search.on_char(t_character);
	}

	return true;
}

auto SettingsPanel::cursor() const -> CursorKind
{
	if (!is_blocking()) return CursorKind::ARROW;
	if (const ListPopup* list = open_list()) return list->cursor(m_mouse);

	if (m_pattern_open) {
		return pattern_at(pattern_popup_rect(rows(layout())), m_mouse) ? CursorKind::HAND : CursorKind::ARROW;
	}

	const bool dragging = m_rows_scroll.is_dragging() || m_color_picker.is_dragging() || std::ranges::any_of(m_slider_drags, &Draggable::is_pressed);
	if (dragging) return CursorKind::DRAG;
	if (m_search.is_selecting()) return CursorKind::I_BEAM;

	if (m_color_picker.is_open()) {
		const CursorKind picker = m_color_picker.cursor(m_mouse);
		if (picker != CursorKind::ARROW) return picker;
	}

	const Layout current = layout();
	if (back_button_rect(current.header).contains(m_mouse)) return CursorKind::HAND;

	const Rect search = search_rect(current);
	if (is_searching() && controls::search_clear_rect(search).contains(m_mouse)) return CursorKind::HAND;
	if (search.contains(m_mouse)) return CursorKind::I_BEAM;

	if (tab_at(current, m_mouse)) return CursorKind::HAND;
	if (!current.rows_region.contains(m_mouse)) return CursorKind::ARROW;

	const Rows current_rows = rows(current);

	for (const ResettableSetting setting : each<ResettableSetting>()) {
		if (can_reset(setting) && hits(current, current_rows.*K_RESETS[setting].row, reset_button(current_rows, setting), m_mouse)) {
			return CursorKind::HAND;
		}
	}

	if (slider_at(current, current_rows, m_mouse)) return CursorKind::HAND;

	for (const ChoiceKind choice : each<ChoiceKind>()) {
		if (hits(current, current_rows.*K_CHOICES[choice].row, choice_rect(current_rows, choice), m_mouse)) return CursorKind::HAND;
	}

	const Rect font_size      = stepper_rect(current_rows.font_size, m_fonts);
	const Rect secondary_size = stepper_rect(current_rows.secondary_font_size, m_fonts);

	const struct {
		Rect row;
		Rect control;
	} clickable[]{
		{current_rows.theme, dropdown_rect(current_rows.theme, m_fonts)},
		{current_rows.font, dropdown_rect(current_rows.font, m_fonts)},
		{current_rows.background, pattern_select_rect(current_rows.background, m_fonts)},
		{current_rows.font_size, stepper_minus(font_size)},
		{current_rows.font_size, stepper_plus(font_size)},
		{current_rows.secondary_font_size, stepper_minus(secondary_size)},
		{current_rows.secondary_font_size, stepper_plus(secondary_size)},
		{current_rows.accent, swatch_rect(current_rows.accent)},
		{current_rows.riot_client, riot_client_button_rect(current_rows.riot_client, m_fonts)},
		{current_rows.master_password, master_password_button_rect(current_rows.master_password, m_fonts)},
	};

	for (const auto& target : clickable) {
		if (hits(current, target.row, target.control, m_mouse)) return CursorKind::HAND;
	}

	for (const Toggle& toggle : K_TOGGLES) {
		const Rect row = current_rows.*toggle.row;
		if (hits(current, row, toggle_rect(row), m_mouse)) return CursorKind::HAND;
	}

	return m_rows_scroll.is_over_track(m_mouse, rows_scroll(current, current_rows)) ? CursorKind::HAND : CursorKind::ARROW;
}

auto SettingsPanel::draw_chrome(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) const -> void
{
	const Font& body = m_fonts->body;

	if (t_layout.docked) {
		controls::draw_glass(t_draw_list, t_layout.panel, rounded(0.0f), controls::GlassSurface::SHEET, t_alpha, false);
	} else {
		const RoundnessScope corners{controls::K_POPUP_ROUNDNESS};

		controls::draw_glass(t_draw_list, t_layout.panel, rounded(K_PANEL_RADIUS), controls::GlassSurface::SHEET, t_alpha, false);
		t_draw_list->add_rounded_outline(t_layout.panel, rounded(K_PANEL_RADIUS), K_PANEL_BORDER, faded(g_theme.border, t_alpha));
	}

	const Rect back         = back_button_rect(t_layout.header);
	const bool back_hovered = back.contains(m_mouse);

	t_draw_list->add_rounded_rect(back, rounded(back.w * 0.5f), faded(back_hovered ? g_theme.control_hover : g_theme.control, t_alpha));
	t_draw_list->add_image(back.centered(K_BACK_ICON_SIZE, K_BACK_ICON_SIZE), m_assets->get(Asset::ICON_ARROW_BACK),
	                       faded(back_hovered ? g_theme.text : g_theme.text_dim, t_alpha));

	draw_text(t_draw_list, body, Vec2{back.right() + K_TITLE_GAP, body.centered_baseline(t_layout.header)}, K_PANEL_TITLE, faded(g_theme.text, t_alpha));
}

auto SettingsPanel::draw_rail(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) const -> void
{
	const Font& body         = m_fonts->body;
	const bool  pointer_live = is_blocking() && !has_popup_open();
	const bool  searching    = is_searching();
	const float height       = tab_height(m_fonts);

	t_draw_list->add_rect(Rect{t_layout.rail.right(), t_layout.rail.y, 1.0f, t_layout.rail.h}, faded(g_theme.separator, t_alpha));

	for (const SettingsTab tab : each<SettingsTab>()) {
		const Rect item      = tab_rect(t_layout, tab);
		const bool is_active = tab == m_tab && !searching;

		if (!is_active && pointer_live && item.contains(m_mouse)) {
			t_draw_list->add_rounded_rect(item.inset(0.0f, K_TAB_GAP * 0.5f), rounded(K_CONTROL_RADIUS),
			                              faded(controls::glass_highlight(K_HOVERED_TAB_FILL), t_alpha));
		}
	}

	if (!searching) {
		const Rect first_tab = tab_rect(t_layout, SettingsTab::APPEARANCE);
		const Rect active{first_tab.x, snapped_to_pixel(first_tab.y + m_tab_indicator * height), first_tab.w, height};
		t_draw_list->add_rounded_rect(active.inset(0.0f, K_TAB_GAP * 0.5f), rounded(K_CONTROL_RADIUS), faded(controls::glass_highlight(), t_alpha));
	}

	for (const SettingsTab tab : each<SettingsTab>()) {
		const Rect item    = tab_rect(t_layout, tab);
		const bool hovered = pointer_live && item.contains(m_mouse);

		Color label = g_theme.text_dim;
		if (tab == m_tab && !searching) {
			label = g_theme.text;
		} else if (hovered) {
			label = mix(g_theme.text_dim, g_theme.text, K_HOVERED_TAB_BRIGHTENING);
		}

		draw_text_truncated(t_draw_list, body, Vec2{item.x + K_TAB_LABEL_INSET, body.centered_baseline(item)}, K_TAB_NAMES[tab],
		                    item.w - K_TAB_LABEL_INSET * 2.0f, faded(label, t_alpha));
	}
}

auto SettingsPanel::renderer_note(char (&t_buffer)[96]) const -> const char*
{
	const GraphicsApi running   = m_renderer->api();
	const GraphicsApi requested = m_renderer->requested_api();
	const GraphicsApi chosen    = m_settings->renderer;

	if (running != requested && chosen == requested) {
		std::snprintf(t_buffer, sizeof(t_buffer), "%s isn't available, so Pulsar uses %s.", graphics_api_name(requested).data(),
		              graphics_api_name(running).data());
	} else if (chosen != running) {
		std::snprintf(t_buffer, sizeof(t_buffer), "Restart Pulsar to switch to %s.", graphics_api_name(chosen).data());
	} else {
		return spec_of(&Rows::renderer).description;
	}

	return t_buffer;
}

auto SettingsPanel::draw_label(DrawList* t_draw_list, const Rows& t_rows, Rect Rows::* t_row, Rect t_control, u8 t_alpha) const -> void
{
	const RowSpec& spec = spec_of(t_row);
	const Rect     row  = t_rows.*t_row;
	char           note[96];
	const char*    description = t_row == &Rows::renderer ? renderer_note(note) : spec.description;

	draw_row_label(t_draw_list, m_fonts, row, spec.title, description, label_right_edge(t_control), t_alpha);
}

auto SettingsPanel::draw_dropdown_row(DrawList*     t_draw_list,
                                      const Layout& t_layout,
                                      const Rows&   t_rows,
                                      Rect Rows::*     t_row,
                                      std::string_view t_value,
                                      bool             t_open,
                                      u8               t_alpha) const -> void
{
	const Rect row = t_rows.*t_row;
	if (!is_on_screen(t_layout, row)) return;

	const Rect button  = dropdown_rect(row, m_fonts);
	const bool hovered = !has_popup_open() && hits(t_layout, row, button, m_mouse);

	draw_label(t_draw_list, t_rows, t_row, button, t_alpha);
	draw_select(t_draw_list, m_fonts->body, button, t_value, t_open, hovered, m_settings->accent, t_alpha);
}

auto SettingsPanel::draw_appearance(DrawList* t_draw_list, const Layout& t_layout, const Rows& t_rows, u8 t_alpha) const -> void
{
	const Font& body   = m_fonts->body;
	const Color accent = m_settings->accent;

	draw_dropdown_row(t_draw_list, t_layout, t_rows, &Rows::theme, K_THEME_LABELS[m_settings->theme].name, m_theme_list.is_open(), t_alpha);
	draw_dropdown_row(t_draw_list, t_layout, t_rows, &Rows::font, m_font_label, m_font_list.is_open(), t_alpha);

	if (is_on_screen(t_layout, t_rows.font_size)) {
		const Rect stepper = stepper_rect(t_rows.font_size, m_fonts);
		draw_label(t_draw_list, t_rows, &Rows::font_size, stepper, t_alpha);
		draw_stepper(t_draw_list, body, stepper, m_font_size_shown, t_alpha);
	}

	if (is_on_screen(t_layout, t_rows.secondary_font_size)) {
		const Rect stepper = stepper_rect(t_rows.secondary_font_size, m_fonts);
		draw_label(t_draw_list, t_rows, &Rows::secondary_font_size, stepper, t_alpha);
		draw_stepper(t_draw_list, body, stepper, m_secondary_font_size_shown, t_alpha);
	}

	if (is_on_screen(t_layout, t_rows.accent)) {
		const Rect  swatch     = swatch_rect(t_rows.accent);
		const Rect  ring       = swatch.inset(-(K_SWATCH_RING_GAP + K_SWATCH_RING));
		const bool  hovered    = !has_popup_open() && hits(t_layout, t_rows.accent, ring, m_mouse);
		const Color ring_color = m_color_picker.is_open() ? accent : (hovered ? g_theme.text_dim : g_theme.border);
		const Color shown{static_cast<u8>(std::lround(m_accent_shown[0])), static_cast<u8>(std::lround(m_accent_shown[1])),
		                  static_cast<u8>(std::lround(m_accent_shown[2])), accent.a};

		draw_label(t_draw_list, t_rows, &Rows::accent, ring, t_alpha);

		const RoundnessScope circles{K_CIRCLE_ROUNDNESS};
		t_draw_list->add_rounded_outline(ring, rounded(ring.w * 0.5f), K_SWATCH_RING, faded(ring_color, t_alpha));
		t_draw_list->add_rounded_rect(swatch, rounded(swatch.w * 0.5f), faded(shown, t_alpha));
	}
}

auto SettingsPanel::draw_pattern_row(DrawList* t_draw_list, const Layout& t_layout, const Rows& t_rows, u8 t_alpha) const -> void
{
	const Rect row = t_rows.background;
	if (!is_on_screen(t_layout, row)) return;

	const Rect     button  = pattern_select_rect(row, m_fonts);
	const bool     hovered = !has_popup_open() && hits(t_layout, row, button, m_mouse);
	const RowSpec& spec    = spec_of(&Rows::background);

	draw_row_label(t_draw_list, m_fonts, row, spec.title, spec.description, inline_slider_left(row) - K_RESET_COLUMN - K_LABEL_CONTROL_GAP, t_alpha);
	draw_select(t_draw_list, m_fonts->body, button, K_BACKGROUND_LABELS[m_settings->background_style].name, m_pattern_open, hovered, m_settings->accent,
	            t_alpha);
}

auto SettingsPanel::draw_pattern_popup(DrawList* t_draw_list, const Rows& t_rows, u8 t_alpha) const -> void
{
	if (m_pattern_open_amount <= 0.01f) return;

	const Font& secondary = m_fonts->secondary;
	const float amount    = m_pattern_open_amount;
	const auto  alpha     = static_cast<u8>(t_alpha * amount);
	const Rect  resting   = pattern_popup_rect(t_rows);
	const Rect  popup{resting.x, snapped_to_pixel(resting.y - K_PATTERN_POPUP_RISE * (1.0f - amount)), resting.w, resting.h};
	const float preview_height = std::round(pattern_tile_size().x * K_TILE_ASPECT);

	{
		const RoundnessScope corners{controls::K_POPUP_ROUNDNESS};

		controls::draw_popup_shadow(t_draw_list, popup, K_PATTERN_POPUP_RADIUS, amount);
		controls::draw_glass(t_draw_list, popup, rounded(K_PATTERN_POPUP_RADIUS), controls::GlassSurface::MENU, alpha);
	}

	for (const BackgroundStyle style : each<BackgroundStyle>()) {
		const Rect  cell = pattern_tile(popup, style);
		const Rect  preview{cell.x, cell.y, cell.w, preview_height};
		const float ring    = m_pattern_ring[style];
		const bool  hovered = m_pattern_open && pattern_tile(resting, style).contains(m_mouse);

		if (ring > 0.01f) {
			const float outer = K_TILE_RING_GAP + K_TILE_RING;

			t_draw_list->add_rounded_outline(preview.inset(-outer), rounded(K_TILE_RADIUS + outer), K_TILE_RING,
			                                 faded(m_settings->accent, static_cast<u8>(alpha * ring)));
		}

		t_draw_list->add_rounded_rect(preview, rounded(K_TILE_RADIUS), faded(hovered ? g_theme.border : g_theme.separator, alpha));
		t_draw_list->add_pattern_swatch(preview.inset(1.0f), rounded(K_TILE_RADIUS - 1.0f), faded(g_theme.window, alpha), style);

		const std::string_view name  = K_BACKGROUND_LABELS[style].name;
		const Color            label = hovered ? g_theme.text : mix(g_theme.text_dim, g_theme.text, ring);
		draw_text(t_draw_list, secondary,
		          Vec2{snapped_to_pixel(preview.center().x - text_width(secondary, name) * 0.5f), preview.bottom() + K_TILE_LABEL_GAP + secondary.ascent}, name,
		          faded(label, alpha));
	}
}

// The segments slide their highlight between options, so the eased selection falls between two of them while it moves.
auto SettingsPanel::draw_choice(DrawList* t_draw_list, const Layout& t_layout, const Rows& t_rows, ChoiceKind t_choice, u8 t_alpha) const -> void
{
	const ChoiceSpec& spec = K_CHOICES[t_choice];
	const Rect        row  = t_rows.*spec.row;
	if (!is_on_screen(t_layout, row)) return;

	const std::span<const std::string_view> labels   = spec.labels;
	const float                             selected = m_choice_shown[t_choice];

	const Font& body   = m_fonts->body;
	const Rect  choice = segment_choice_rect(row, labels);
	const auto  from   = static_cast<u32>(selected);
	const Rect  first  = choice_segment(choice, labels, from);
	const Rect  second = choice_segment(choice, labels, std::min(from + 1, static_cast<u32>(labels.size()) - 1));
	const float slide  = selected - static_cast<float>(from);
	const Rect  thumb{first.x + (second.x - first.x) * slide, first.y, first.w + (second.w - first.w) * slide, first.h};
	const bool  pointer_live = !has_popup_open() && t_layout.rows_region.contains(m_mouse);

	draw_label(t_draw_list, t_rows, spec.row, choice, t_alpha);
	draw_inset_control(t_draw_list, choice, g_theme.separator, t_alpha);
	t_draw_list->add_bordered_rect(thumb, rounded(K_CONTROL_RADIUS - K_SEGMENT_INSET), faded(g_theme.popup, t_alpha), faded(g_theme.separator, t_alpha), 1.0f);

	for (u32 i = 0; i < labels.size(); i += 1) {
		const Rect  segment   = choice_segment(choice, labels, i);
		const float closeness = std::max(0.0f, 1.0f - std::fabs(static_cast<float>(i) - selected));
		const bool  hovered   = pointer_live && segment.contains(m_mouse);
		const Color idle      = hovered ? mix(g_theme.text_dim, g_theme.text, K_HOVERED_TAB_BRIGHTENING) : g_theme.text_dim;

		draw_text_centered(t_draw_list, body, segment, labels[i], faded(mix(idle, g_theme.text, closeness), t_alpha));
	}
}

auto SettingsPanel::draw_sliders(DrawList* t_draw_list, const Layout& t_layout, const Rows& t_rows, u8 t_alpha) const -> void
{
	const auto  last_stop    = static_cast<float>(K_AUTO_LOCK_STOP_COUNT - 1);
	const float never_amount = std::clamp(m_slider_shown[SliderKind::AUTO_LOCK] - (last_stop - 1.0f), 0.0f, 1.0f);
	char        readout[16];

	for (const SliderKind slider : each<SliderKind>()) {
		const SliderSpec& spec       = K_SLIDERS[slider];
		const Rect        row        = t_rows.*spec.row;
		const float       visibility = m_slider_reveal[slider];
		const auto        alpha      = static_cast<u8>(t_alpha * visibility * visibility);
		if (!is_on_screen(t_layout, row) || alpha == 0) continue;

		const bool auto_lock = slider == SliderKind::AUTO_LOCK;
		const Rect bounds    = slider_rect(t_rows, slider);

		if (spec.place == SliderPlace::OWN_ROW) {
			draw_label(t_draw_list, t_rows, spec.row, bounds, t_alpha);
		}

		const SliderLook look{
			.readout  = slider_readout(slider, readout),
			.caption  = spec.caption,
			.fraction = slider_fraction(slider),
			.hover    = m_slider_hover[slider],
			.accent   = auto_lock ? mix(m_settings->accent, g_theme.text_faint, never_amount) : m_settings->accent,
			.steps    = auto_lock ? K_AUTO_LOCK_STOP_COUNT - 1 : 0,
		};

		draw_slider(t_draw_list, m_fonts->secondary, bounds, look, alpha);
	}
}

auto SettingsPanel::draw_search(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) -> void
{
	const Rect search = search_rect(t_layout);
	if (search.w <= 0.0f) return;

	controls::draw_search_field(t_draw_list, m_fonts->secondary, search, K_SEARCH_INSET, &m_search, m_mouse, m_settings->accent, t_alpha);
}

auto SettingsPanel::draw_cards(DrawList* t_draw_list, const Layout& t_layout, const Rows& t_rows, u8 t_alpha) const -> void
{
	for (const Rect& card : t_rows.cards) {
		if (!is_on_screen(t_layout, card)) continue;

		t_draw_list->add_rounded_rect(card, rounded(K_CARD_RADIUS), faded(controls::glass_highlight(K_CARD_WASH), t_alpha));
		t_draw_list->add_rounded_outline(card, rounded(K_CARD_RADIUS), 1.0f, faded(g_theme.separator, t_alpha));
	}

	std::optional<SettingsGroup> group;

	for (const RowSpec& spec : K_ROW_SPECS) {
		const Rect row = t_rows.*spec.row;
		if (row.h <= 0.0f) continue;

		const bool first = group != spec.group;
		group            = spec.group;

		if (first || spec.row == &Rows::glass_tint || !is_on_screen(t_layout, row)) continue;

		t_draw_list->add_rect(Rect{row.x + K_ROW_INSET_X, row.y, t_rows.row_width - K_ROW_INSET_X * 2.0f, 1.0f}, faded(g_theme.separator, t_alpha));
	}
}

auto SettingsPanel::draw_captions(DrawList* t_draw_list, const Layout& t_layout, const Rows& t_rows, u8 t_alpha) const -> void
{
	const Font& secondary = m_fonts->secondary;

	for (const SettingsGroup group : each<SettingsGroup>()) {
		const Rect title = t_rows.group_titles[group];
		if (!is_on_screen(t_layout, title)) continue;

		draw_text(t_draw_list, secondary, Vec2{title.x + K_ROW_INSET_X * 0.5f, title.bottom() - K_GROUP_TITLE_BOTTOM_GAP - secondary.descent},
		          K_GROUPS[group].title, faded(g_theme.text_faint, t_alpha));
	}
}

auto SettingsPanel::draw_no_results(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) const -> void
{
	draw_text_centered(t_draw_list, m_fonts->secondary, t_layout.rows_region, "No settings match your search", faded(g_theme.text_dim, t_alpha));
}

auto SettingsPanel::draw_toggles(DrawList* t_draw_list, const Layout& t_layout, const Rows& t_rows, u8 t_alpha) const -> void
{
	for (u32 i = 0; i < K_TOGGLE_COUNT; i += 1) {
		const Toggle& toggle = K_TOGGLES[i];
		const Rect    row    = t_rows.*toggle.row;
		if (!is_on_screen(t_layout, row)) continue;

		const Rect control = toggle_rect(row);
		draw_label(t_draw_list, t_rows, toggle.row, control, t_alpha);
		draw_toggle(t_draw_list, control, m_toggles_shown[i], m_settings->accent, t_alpha);
	}
}

auto SettingsPanel::draw_riot_client(DrawList* t_draw_list, const Layout& t_layout, const Rows& t_rows, u8 t_alpha) const -> void
{
	const Rect row = t_rows.riot_client;
	if (!is_on_screen(t_layout, row)) return;

	const RowSpec& spec        = spec_of(&Rows::riot_client);
	const char*    description = m_settings->riot_client_path[0] != '\0' ? m_settings->riot_client_path : spec.description;
	const Rect     button      = riot_client_button_rect(row, m_fonts);

	draw_row_label(t_draw_list, m_fonts, row, spec.title, description, label_right_edge(button), t_alpha);
	controls::draw_button(t_draw_list, m_fonts->body, button, "Browse", controls::ButtonStyle::NEUTRAL, m_settings->accent, true,
	                      !has_popup_open() && hits(t_layout, row, button, m_mouse), t_alpha);
}

auto SettingsPanel::draw_master_password(DrawList* t_draw_list, const Layout& t_layout, const Rows& t_rows, u8 t_alpha) const -> void
{
	const Rect row = t_rows.master_password;
	if (!is_on_screen(t_layout, row)) return;

	const Rect button = master_password_button_rect(row, m_fonts);
	draw_label(t_draw_list, t_rows, &Rows::master_password, button, t_alpha);

	controls::draw_button(t_draw_list, m_fonts->body, button, "Reset password", controls::ButtonStyle::NEUTRAL, m_settings->accent, true,
	                      !has_popup_open() && hits(t_layout, row, button, m_mouse), t_alpha);
}

auto SettingsPanel::draw_reset_buttons(DrawList* t_draw_list, const Layout& t_layout, const Rows& t_rows, u8 t_alpha) const -> void
{
	const bool pointer_live = !has_popup_open() && t_layout.rows_region.contains(m_mouse);

	for (const ResettableSetting setting : each<ResettableSetting>()) {
		if (!is_on_screen(t_layout, t_rows.*K_RESETS[setting].row)) continue;

		const Rect button = reset_button(t_rows, setting);
		draw_reset_button(t_draw_list, m_assets->get(Asset::ICON_RESET), button, m_reset_visible[setting], m_reset_spin[setting],
		                  pointer_live && button.contains(m_mouse), t_alpha);
	}
}

auto SettingsPanel::draw(DrawList* t_draw_list) -> void
{
	PULSAR_PROFILE_SCOPE("SettingsPanel.Draw");

	if (m_open_amount <= 0.001f) return;

	const auto           alpha        = to_alpha(m_open_amount);
	const Layout         current      = layout();
	const Rows           current_rows = rows(current);
	const ScrollGeometry scroll       = rows_scroll(current, current_rows);

	if (!current.docked) {
		const Vec2 window = m_window->size();
		controls::draw_popup_backdrop(t_draw_list, Rect{0.0f, 0.0f, window.x, window.y}, m_open_amount);
	}

	const float scale = current.docked ? 1.0f : K_PANEL_CLOSED_SCALE + (1.0f - K_PANEL_CLOSED_SCALE) * m_open_amount;
	t_draw_list->push_scale(current.panel.center(), scale);

	draw_chrome(t_draw_list, current, alpha);
	draw_search(t_draw_list, current, alpha);
	draw_rail(t_draw_list, current, alpha);

	const Rect  region            = current.rows_region;
	const float divider_y         = snapped_to_pixel(current.header.bottom());
	const float divider_thickness = snapped_to_pixel(1.0f);
	const Rect  content{region.x, divider_y + divider_thickness, region.w, region.bottom() - divider_y - divider_thickness};

	t_draw_list->push_clip(content);
	draw_cards(t_draw_list, current, current_rows, alpha);
	draw_captions(t_draw_list, current, current_rows, alpha);
	draw_appearance(t_draw_list, current, current_rows, alpha);
	draw_pattern_row(t_draw_list, current, current_rows, alpha);

	for (const ChoiceKind choice : each<ChoiceKind>()) {
		draw_choice(t_draw_list, current, current_rows, choice, alpha);
	}

	draw_toggles(t_draw_list, current, current_rows, alpha);
	draw_riot_client(t_draw_list, current, current_rows, alpha);
	draw_master_password(t_draw_list, current, current_rows, alpha);
	draw_sliders(t_draw_list, current, current_rows, alpha);
	draw_reset_buttons(t_draw_list, current, current_rows, alpha);

	if (is_searching() && current_rows.listed_count == 0) {
		draw_no_results(t_draw_list, current, alpha);
	}

	t_draw_list->pop_clip();

	const float header_shadow = std::clamp(m_rows_scroll.offset() / K_HEADER_SHADOW_TRAVEL, 0.0f, 1.0f);

	if (header_shadow > 0.0f) {
		const Color shadow = faded(g_theme.shadow, static_cast<u8>(alpha * K_HEADER_SHADOW_ALPHA * header_shadow));
		t_draw_list->add_gradient(Rect{region.x, content.y, region.w, K_HEADER_SHADOW_HEIGHT}, shadow, shadow, faded(shadow, 0), faded(shadow, 0));
	}

	t_draw_list->add_rect(Rect{current.header.x, divider_y, current.header.w, divider_thickness}, faded(g_theme.separator, alpha));
	m_rows_scroll.draw(t_draw_list, scroll, m_mouse, alpha);

	if (is_on_screen(current, current_rows.accent)) {
		m_color_picker.draw(t_draw_list, m_mouse);
	}

	draw_pattern_popup(t_draw_list, current_rows, alpha);

	m_font_list.draw(t_draw_list, m_mouse);
	m_theme_list.draw(t_draw_list, m_mouse);

	m_tooltip.draw(t_draw_list, m_fonts, current.panel, alpha);

	t_draw_list->pop_scale();
}
