#include "ui/settings_panel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <optional>
#include <span>

#include <Windows.h>

#include "core/animation.h"
#include "core/profiler.h"
#include "core/str.h"
#include "gfx/assets.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "platform/window.h"
#include "ui/controls.h"
#include "ui/text.h"
#include "ui/theme.h"

namespace {
constexpr float open_ease_rate = 16.0f;
constexpr float toggle_ease_rate = 18.0f;
constexpr float value_ease_rate = 16.0f;
constexpr float reset_appear_rate = 20.0f;
constexpr float reset_spin_rate = 6.0f;

constexpr Vec2 panel_max_size{720.0f, 480.0f};
constexpr Vec2 panel_min_size{580.0f, 380.0f};
constexpr float reference_body_pixel_height = 24.0f;
constexpr float reference_secondary_pixel_height = 20.0f;
constexpr float panel_margin = 48.0f;
constexpr float panel_closed_scale = 0.94f;
constexpr float panel_radius = 16.0f;
constexpr float panel_border = 1.5f;

constexpr float row_padding_x = 26.0f;
constexpr float back_button_size = 32.0f;
constexpr float back_button_margin = 12.0f;
constexpr float back_icon_size = 18.0f;
constexpr float title_gap = 10.0f;
constexpr float label_line_gap = 3.0f;
constexpr float label_control_gap = 16.0f;
constexpr float highlight_inset_x = 4.0f;
constexpr float highlight_inset_y = 3.0f;
constexpr float row_inset_x = 16.0f;
constexpr float two_line_padding_y = 10.0f;
constexpr float single_row_padding_y = 6.0f;
constexpr float card_margin_x = 18.0f;
constexpr float card_radius = 10.0f;
constexpr float group_gap = 6.0f;
constexpr float reveal_ease_rate = 16.0f;
constexpr float rows_top_padding = 8.0f;
constexpr float rail_width = 150.0f;
constexpr float rail_padding = 10.0f;
constexpr float tab_label_inset = 14.0f;
constexpr float tab_indicator_width = 3.0f;
constexpr float tab_indicator_inset = 8.0f;
constexpr float tab_slide_rate = 18.0f;
constexpr float active_tab_tint = 0.18f;
constexpr float hovered_tab_brightening = 0.5f;
constexpr float scrollbar_margin = 4.0f;
constexpr float header_shadow_height = 14.0f;
constexpr float header_shadow_alpha = 0.35f;
constexpr float header_shadow_travel = 24.0f;

constexpr float reset_button_size = 28.0f;
constexpr float reset_button_gap = 10.0f;
constexpr float reset_icon_size = 18.0f;

constexpr float slider_bar_height = 4.0f;
constexpr float slider_rest_bar_height = 2.0f;
constexpr float slider_text_clearance = 7.0f;
constexpr float slider_thumb_radius = 6.0f;
constexpr float slider_thumb_ring = 2.0f;
constexpr float slider_tick_size = 3.0f;
constexpr float slider_hover_rate = 14.0f;
constexpr float slider_caption_gap = 8.0f;
constexpr float inline_slider_gap = 28.0f;
constexpr u32 auto_lock_stops[]{1, 2, 5, 10, 15, 30, 60, 0};
constexpr u32 auto_lock_stop_count = static_cast<u32>(std::size(auto_lock_stops));

constexpr std::string_view panel_title = "Settings";
constexpr float search_max_width = 216.0f;
constexpr float search_min_width = 120.0f;
constexpr float search_header_gap = 16.0f;
constexpr float search_inset = 11.0f;
constexpr float search_icon_size = 13.0f;
constexpr float search_icon_gap = 7.0f;
constexpr float search_clear_margin = 8.0f;
constexpr u32 search_max_length = 48;
constexpr float group_title_top_gap = 16.0f;
constexpr float group_title_bottom_gap = 10.0f;
constexpr float knob_ring = 1.5f;

constexpr float animation_speed_min = 0.25f;
constexpr float animation_speed_max = 3.0f;
constexpr float corner_roundness_min = 0.0f;
constexpr float corner_roundness_max = 1.5f;
constexpr float font_size_min = 10.0f;
constexpr float font_size_max = 24.0f;
constexpr float secondary_font_size_min = 8.0f;
constexpr float secondary_font_size_max = 18.0f;

constexpr float control_column_width = 170.0f;
constexpr float control_radius = 8.0f;
constexpr float stepper_width = 96.0f;
constexpr float swatch_size = 22.0f;
constexpr float swatch_ring_gap = 2.0f;
constexpr float swatch_ring = 1.5f;
constexpr float font_list_width = 290.0f;

constexpr u32 pattern_columns = 3;
constexpr float pattern_select_padding = 10.0f;
constexpr float pattern_select_chevron_room = 31.0f;
constexpr float pattern_popup_padding = 14.0f;
constexpr float pattern_popup_radius = 12.0f;
constexpr float pattern_popup_gap = 6.0f;
constexpr float pattern_popup_margin = 8.0f;
constexpr float pattern_popup_rate = 22.0f;
constexpr float pattern_popup_rise = 6.0f;
constexpr float tile_min_width = 96.0f;
constexpr float tile_label_margin = 12.0f;
constexpr float tile_gap = 10.0f;
constexpr float tile_row_gap = 12.0f;
constexpr float tile_aspect = 0.56f;
constexpr float tile_radius = 8.0f;
constexpr float tile_label_gap = 6.0f;
constexpr float tile_ring_gap = 2.0f;
constexpr float tile_ring = 2.0f;
constexpr float tile_ring_rate = 16.0f;

constexpr std::string_view close_choice_labels[]{"Minimize To Tray", "Quit"};
constexpr float segment_padding_x = 12.0f;
constexpr float segment_inset = 2.0f;
constexpr float segment_slide_rate = 18.0f;
constexpr float theme_list_width = 250.0f;

constexpr float theme_dot_size = 16.0f;
constexpr float theme_dot_overlap = 6.0f;
constexpr float theme_dot_cutout = 2.0f;
constexpr float theme_hover_preview_seconds = 1.0f;
constexpr Vec2 theme_preview_size{theme_dot_size * 2.0f - theme_dot_overlap, theme_dot_size};

constexpr std::string_view tab_names[]{"Appearance", "Behavior", "Privacy", "Security"};
static_assert(std::size(tab_names) == settings_tab_count);

const Settings default_settings{};

constexpr auto theme_names = [] {
	std::array<std::string_view, theme_count> names{};
	for (u32 i = 0; i < theme_count; i += 1) {
		names[i] = theme_labels[i].name;
	}

	return names;
}();

bool is_control_down()
{
	return (GetKeyState(VK_CONTROL) & 0x8000) != 0;
}

float fraction_in(float t_value, float t_min, float t_max)
{
	return std::clamp((t_value - t_min) / (t_max - t_min), 0.0f, 1.0f);
}

float value_at(float t_fraction, float t_min, float t_max)
{
	return t_min + std::clamp(t_fraction, 0.0f, 1.0f) * (t_max - t_min);
}

float header_height(const Fonts &t_fonts)
{
	return t_fonts.body().line_height() + 20.0f;
}

float label_block_height(const Fonts &t_fonts)
{
	return t_fonts.body().line_height() + label_line_gap + t_fonts.secondary().line_height();
}

float row_height(const Fonts &t_fonts)
{
	return label_block_height(t_fonts) + two_line_padding_y * 2.0f;
}

float group_title_height(const Fonts &t_fonts)
{
	return group_title_top_gap + t_fonts.secondary().line_height() + group_title_bottom_gap;
}

float tab_height(const Fonts &t_fonts)
{
	return std::max(34.0f, t_fonts.body().line_height() + 12.0f);
}

float control_height(const Fonts &t_fonts)
{
	return std::max(30.0f, t_fonts.body().line_height() + 10.0f);
}

float single_row_height(const Fonts &t_fonts)
{
	return std::max(control_height(t_fonts), t_fonts.body().line_height()) + single_row_padding_y * 2.0f;
}

float title_baseline(Rect t_row, const Fonts &t_fonts)
{
	return t_row.center().y - label_block_height(t_fonts) * 0.5f + t_fonts.body().ascent();
}

float description_baseline(Rect t_row, const Fonts &t_fonts)
{
	return t_row.center().y - label_block_height(t_fonts) * 0.5f + t_fonts.body().line_height() + label_line_gap +
		   t_fonts.secondary().ascent();
}

float control_center_y(Rect t_row, const Fonts &)
{
	return t_row.center().y;
}

Rect right_aligned_control(Rect t_row, const Fonts &t_fonts, float t_width, float t_height)
{
	return Rect{t_row.right() - row_inset_x - t_width, control_center_y(t_row, t_fonts) - t_height * 0.5f, t_width,
				t_height};
}

Rect back_button_rect(Rect t_header)
{
	return Rect{t_header.x + back_button_margin, t_header.center().y - back_button_size * 0.5f, back_button_size,
				back_button_size};
}

Rect dropdown_rect(Rect t_row, const Fonts &t_fonts)
{
	return right_aligned_control(t_row, t_fonts, control_column_width, control_height(t_fonts));
}

Rect pattern_select_rect(Rect t_row, const Fonts &t_fonts)
{
	float widest = 0.0f;
	for (const OptionLabel &label : background_labels) {
		widest = std::max(widest, text_width(t_fonts.body(), label.name));
	}

	return right_aligned_control(t_row, t_fonts,
								 std::ceil(widest + pattern_select_padding + pattern_select_chevron_room),
								 control_height(t_fonts));
}

Rect stepper_rect(Rect t_row, const Fonts &t_fonts)
{
	return right_aligned_control(t_row, t_fonts, stepper_width, control_height(t_fonts));
}

Rect stepper_minus(Rect t_stepper)
{
	return Rect{t_stepper.x, t_stepper.y, t_stepper.h, t_stepper.h};
}

Rect stepper_plus(Rect t_stepper)
{
	return Rect{t_stepper.right() - t_stepper.h, t_stepper.y, t_stepper.h, t_stepper.h};
}

Rect toggle_rect(Rect t_row, const Fonts &t_fonts)
{
	return right_aligned_control(t_row, t_fonts, 36.0f, 20.0f);
}

Rect swatch_rect(Rect t_row, const Fonts &t_fonts)
{
	return right_aligned_control(t_row, t_fonts, swatch_size, swatch_size);
}

Rect master_password_button_rect(Rect t_row, const Fonts &t_fonts)
{
	return right_aligned_control(t_row, t_fonts, 140.0f, control_height(t_fonts));
}

Rect slider_control_rect(Rect t_row, const Fonts &t_fonts)
{
	return right_aligned_control(t_row, t_fonts, control_column_width, control_height(t_fonts));
}

Rect reset_button_rect(Rect t_control, Rect t_row, const Fonts &t_fonts)
{
	return Rect{t_control.x - reset_button_gap - reset_button_size,
				control_center_y(t_row, t_fonts) - reset_button_size * 0.5f, reset_button_size, reset_button_size};
}

float label_right_edge(Rect t_control, Rect t_row, const Fonts &t_fonts)
{
	return reset_button_rect(t_control, t_row, t_fonts).x - label_control_gap;
}

Rect inline_slider_rect(Rect t_row, Rect t_control, float t_left, const Fonts &t_fonts)
{
	const float height = control_height(t_fonts);

	return Rect{t_left, t_row.center().y - height * 0.5f,
				std::max(0.0f, label_right_edge(t_control, t_row, t_fonts) - t_left), height};
}

float slider_fraction_at(Rect t_slider, float t_x)
{
	return t_slider.w > 0.0f ? std::clamp((t_x - t_slider.x) / t_slider.w, 0.0f, 1.0f) : 0.0f;
}

Rect circle_at(Vec2 t_center, float t_radius)
{
	return Rect{t_center.x - t_radius, t_center.y - t_radius, t_radius * 2.0f, t_radius * 2.0f};
}

void draw_row_label(DrawList &t_draw_list, const Fonts &t_fonts, Rect t_row, const char *t_title,
					const char *t_description, float t_right_edge, u8 t_alpha)
{
	const Font &body = t_fonts.body();
	const float x = t_row.x + row_inset_x;

	if (*t_description == '\0') {
		draw_text_truncated(t_draw_list, body, Vec2{x, body.centered_baseline(t_row)}, t_title, t_right_edge - x,
							faded(theme().text, t_alpha));
		return;
	}

	draw_text_truncated(t_draw_list, body, Vec2{x, title_baseline(t_row, t_fonts)}, t_title, t_right_edge - x,
						faded(theme().text, t_alpha));
	draw_text_truncated(t_draw_list, t_fonts.secondary(), Vec2{x, description_baseline(t_row, t_fonts)}, t_description,
						t_right_edge - x, faded(theme().text_dim, t_alpha));
}

void draw_knob(DrawList &t_draw_list, Rect t_knob, Color t_fill, u8 t_alpha)
{
	t_draw_list.add_bordered_rect(t_knob, rounded(t_knob.w * 0.5f), faded(t_fill, t_alpha),
								  faded(outline_on(t_fill), t_alpha), knob_ring);
}

void draw_toggle(DrawList &t_draw_list, Rect t_toggle, float t_on, Color t_accent, u8 t_alpha)
{
	const Color track = mix(theme().track, t_accent, t_on);
	t_draw_list.add_rounded_rect(t_toggle, rounded(t_toggle.h * 0.5f), faded(track, t_alpha));

	const float knob_size = t_toggle.h - 6.0f;
	const Rect knob{t_toggle.x + 3.0f + (t_toggle.w - t_toggle.h) * t_on, t_toggle.y + 3.0f, knob_size, knob_size};
	draw_knob(t_draw_list, knob, foreground_on(track), t_alpha);
}

void draw_inset_control(DrawList &t_draw_list, Rect t_rect, Color t_border, u8 t_alpha)
{
	t_draw_list.add_bordered_rect(t_rect, rounded(control_radius), faded(theme().field, t_alpha),
								  faded(t_border, t_alpha), 1.0f);
}

struct SliderLook {
	std::string_view readout;
	std::string_view caption;
	float fraction;
	float hover;
	Color accent;
	u32 steps;
};

void draw_slider(DrawList &t_draw_list, const Font &t_font, Rect t_slider, const SliderLook &t_look, u8 t_alpha)
{
	const Theme &colors = theme();
	const float hover = t_look.hover;
	const float thickness = slider_rest_bar_height + (slider_bar_height - slider_rest_bar_height) * hover;
	const Rect bar{t_slider.x, t_slider.bottom() - thickness, t_slider.w, thickness};
	const Rect filled{bar.x, bar.y, bar.w * std::clamp(t_look.fraction, 0.0f, 1.0f), bar.h};
	const Vec2 thumb{filled.right(), bar.center().y};

	const float baseline =
		t_font.centered_baseline(Rect{t_slider.x, t_slider.y, t_slider.w, t_slider.h - slider_text_clearance});
	const float readout_width = text_width(t_font, t_look.readout);
	const float riding_x =
		std::clamp(thumb.x - readout_width * 0.5f, t_slider.x, std::max(t_slider.x, t_slider.right() - readout_width));
	const float caption_right = t_slider.x + text_width(t_font, t_look.caption) + slider_caption_gap;
	const float crowding = std::clamp((caption_right - riding_x) / slider_caption_gap, 0.0f, 1.0f);
	const auto hover_alpha = static_cast<u8>(t_alpha * hover);

	draw_text(t_draw_list, t_font, Vec2{t_slider.x, baseline}, t_look.caption,
			  faded(colors.text_faint, static_cast<u8>(t_alpha * (1.0f - hover * crowding))));
	draw_text(t_draw_list, t_font, Vec2{t_slider.right() - readout_width, baseline}, t_look.readout,
			  faded(colors.text, static_cast<u8>(t_alpha * (1.0f - hover))));

	t_draw_list.add_rounded_rect(bar, rounded(thickness * 0.5f),
								 faded(mix(colors.popup, colors.track, 0.55f + 0.45f * hover), t_alpha));
	if (filled.w > 0.0f) {
		t_draw_list.add_rounded_rect(filled, rounded(thickness * 0.5f), faded(t_look.accent, t_alpha));
	}

	if (hover_alpha == 0) return;

	for (u32 i = 1; i < t_look.steps; i += 1) {
		const float x = bar.x + bar.w * static_cast<float>(i) / static_cast<float>(t_look.steps);
		const Color tick = x <= filled.right() ? with_alpha(foreground_on(t_look.accent), 150) : colors.text_faint;

		t_draw_list.add_rounded_rect(circle_at(Vec2{x, thumb.y}, slider_tick_size * 0.5f),
									 rounded(slider_tick_size * 0.5f), faded(tick, hover_alpha));
	}

	const float radius = slider_thumb_radius * (0.5f + 0.5f * hover);
	const float inner_radius = std::max(0.0f, radius - slider_thumb_ring);

	t_draw_list.add_rounded_rect(circle_at(thumb, radius), rounded(radius), faded(t_look.accent, hover_alpha));
	t_draw_list.add_rounded_rect(circle_at(thumb, inner_radius), rounded(inner_radius),
								 faded(Color{255, 255, 255, 255}, hover_alpha));
	draw_text(t_draw_list, t_font, Vec2{snapped_to_pixel(riding_x), baseline}, t_look.readout,
			  faded(colors.text, hover_alpha));
}

void draw_select(DrawList &t_draw_list, const Font &t_font, Rect t_rect, std::string_view t_label, bool t_open,
				 bool t_hovered, Color t_accent, u8 t_alpha)
{
	constexpr float padding = 10.0f;
	constexpr Vec2 chevron_size{9.0f, 5.0f};

	const Theme &colors = theme();
	const Rect chevron{t_rect.right() - padding - chevron_size.x, t_rect.center().y - chevron_size.y * 0.5f,
					   chevron_size.x, chevron_size.y};

	draw_inset_control(t_draw_list, t_rect, t_open ? t_accent : (t_hovered ? colors.border : colors.separator),
					   t_alpha);
	draw_text_truncated(t_draw_list, t_font, Vec2{t_rect.x + padding, t_font.centered_baseline(t_rect)}, t_label,
						chevron.x - padding - (t_rect.x + padding), faded(colors.text, t_alpha));
	controls::draw_chevron(t_draw_list, chevron, t_open,
						   faded(t_open || t_hovered ? colors.text_dim : colors.text_faint, t_alpha));
}

void draw_stepper(DrawList &t_draw_list, const Font &t_font, Rect t_stepper, float t_value, u8 t_alpha)
{
	constexpr float glyph_half = 4.5f;
	constexpr float glyph_thickness = 1.5f;

	const Theme &colors = theme();
	const Vec2 minus = stepper_minus(t_stepper).center();
	const Vec2 plus = stepper_plus(t_stepper).center();
	const Color glyph = faded(colors.text_dim, t_alpha);

	draw_inset_control(t_draw_list, t_stepper, colors.separator, t_alpha);
	t_draw_list.add_line({minus.x - glyph_half, minus.y}, {minus.x + glyph_half, minus.y}, glyph_thickness, glyph);
	t_draw_list.add_line({plus.x - glyph_half, plus.y}, {plus.x + glyph_half, plus.y}, glyph_thickness, glyph);
	t_draw_list.add_line({plus.x, plus.y - glyph_half}, {plus.x, plus.y + glyph_half}, glyph_thickness, glyph);

	char buffer[8];
	const int written = std::snprintf(buffer, sizeof(buffer), "%d", static_cast<int>(std::lround(t_value)));

	draw_text_centered(t_draw_list, t_font, t_stepper,
					   std::string_view{buffer, static_cast<usize>(std::max(written, 0))}, faded(colors.text, t_alpha));
}

void draw_reset_button(DrawList &t_draw_list, const Texture *t_icon, Rect t_button, float t_visible, float t_spin,
					   bool t_hovered, u8 t_alpha)
{
	if (t_visible <= 0.01f) return;

	const auto alpha = static_cast<u8>(t_alpha * t_visible);

	if (t_hovered) {
		t_draw_list.add_rounded_rect(t_button, rounded(7.0f), faded(theme().control_hover, alpha));
	}

	t_draw_list.add_rotated_image(t_button.centered(reset_icon_size, reset_icon_size),
								  -t_spin * 2.0f * std::numbers::pi_v<float>, t_icon,
								  faded(t_hovered ? theme().text : theme().text_dim, alpha));
}

u32 auto_lock_stop(u32 t_minutes)
{
	if (t_minutes == 0) return auto_lock_stop_count - 1;

	const auto distance = [t_minutes](u32 t_stop) {
		const u32 stop = auto_lock_stops[t_stop];
		return t_minutes > stop ? t_minutes - stop : stop - t_minutes;
	};

	u32 nearest = 0;
	for (u32 i = 1; i + 1 < auto_lock_stop_count; i += 1) {
		if (distance(i) < distance(nearest)) {
			nearest = i;
		}
	}

	return nearest;
}

std::string_view auto_lock_label(u32 t_minutes, char (&t_buffer)[16])
{
	if (t_minutes == 0) return "Never";
	if (t_minutes == 60) return "1 hour";

	const int written = std::snprintf(t_buffer, sizeof(t_buffer), "%u min", t_minutes);

	return std::string_view{t_buffer, static_cast<usize>(std::max(written, 0))};
}

bool matches_every_word(std::string_view t_query, std::span<const std::string_view> t_fields)
{
	usize start = 0;

	while (start < t_query.size()) {
		const usize end = std::min(t_query.find(' ', start), t_query.size());
		const std::string_view word = t_query.substr(start, end - start);
		start = end + 1;

		if (word.empty()) continue;

		const bool found = std::ranges::any_of(t_fields, [word](std::string_view t_field) {
			return find_ignoring_case(t_field, word) != std::string_view::npos;
		});
		if (!found) return false;
	}

	return true;
}

void draw_theme_preview(DrawList &t_draw_list, Rect t_preview, ThemeKind t_kind, Color t_backdrop, u8 t_alpha)
{
	const Theme &preset = theme_preset(t_kind);
	const float radius = theme_dot_size * 0.5f;
	const Rect background_dot{t_preview.x, t_preview.center().y - radius, theme_dot_size, theme_dot_size};
	const Rect accent_dot{t_preview.right() - theme_dot_size, background_dot.y, theme_dot_size, theme_dot_size};

	t_draw_list.add_bordered_rect(background_dot, rounded(radius), faded(preset.window, t_alpha),
								  faded(preset.border, t_alpha), 1.0f);
	t_draw_list.add_rounded_rect(accent_dot.inset(-theme_dot_cutout), rounded(radius + theme_dot_cutout),
								 faded(t_backdrop, t_alpha));
	t_draw_list.add_rounded_rect(accent_dot, rounded(radius), faded(preset.default_accent, t_alpha));
}
}

const SettingsPanel::GroupSpec SettingsPanel::group_specs[group_count]{
	{SettingsTab::appearance, "Look"}, {SettingsTab::appearance, "Background"}, {SettingsTab::appearance, "Text"},
	{SettingsTab::behavior, "Motion"}, {SettingsTab::behavior, "General"},		{SettingsTab::privacy, "Protection"},
	{SettingsTab::security, "Vault"},
};

const SettingsPanel::RowSpec SettingsPanel::row_specs[row_spec_count]{
	{&Rows::theme, SettingsTab::appearance, 0, "Theme", "", "dark light mode palette colour colors interface"},
	{&Rows::accent, SettingsTab::appearance, 0, "Accent Color", "", "colour highlight buttons"},
	{&Rows::corner_roundness, SettingsTab::appearance, 0, "Corner Roundness", "", "radius rounded corners"},
	{&Rows::background, SettingsTab::appearance, 1, "Pattern", "",
	 "background backdrop texture dots grid lines polka topography starfield stars scanlines crosshatch wallpaper "
	 "strength intensity opacity subtle"},
	{&Rows::background_light, SettingsTab::appearance, 1, "Soft Light", "",
	 "background backdrop glow gradient top light depth strength intensity"},
	{&Rows::background_grain, SettingsTab::appearance, 1, "Grain", "",
	 "background backdrop noise film texture strength intensity"},
	{&Rows::font, SettingsTab::appearance, 2, "Font", "", "typeface family text"},
	{&Rows::font_size, SettingsTab::appearance, 2, "Font Size", "", "text scale zoom bigger interface"},
	{&Rows::secondary_font_size, SettingsTab::appearance, 2, "Small Text Size", "",
	 "secondary font labels hints scale smaller"},
	{&Rows::animations, SettingsTab::behavior, 3, "Animations", "",
	 "motion effects reduce animate popups speed fast slow"},
	{&Rows::notifications, SettingsTab::behavior, 4, "Notifications", "", "toast popup alert confirmation messages"},
	{&Rows::close_to_tray, SettingsTab::behavior, 4, "When Closing", "",
	 "close to tray minimize quit exit background system tray hide"},
	{&Rows::hide_from_capture, SettingsTab::privacy, 5, "Hide From Screen Capture",
	 "Hide accounts from screenshares, recordings and screenshots.", "stream record share obs discord"},
	{&Rows::block_overlay_injection, SettingsTab::privacy, 5, "Block Overlay Injection",
	 "Block overlays and keyloggers. Restart to apply.", "security inject dll"},
	{&Rows::auto_lock, SettingsTab::security, 6, "Auto-Lock", "Lock the vault after being idle.",
	 "timeout idle inactive away"},
	{&Rows::master_password, SettingsTab::security, 6, "Master Password", "Encrypts saved passwords.",
	 "change reset vault encryption"},
};

const SettingsPanel::PercentSlider SettingsPanel::percent_sliders[percent_slider_count]{
	{&Rows::background, &Settings::background_intensity,
	 [](const Settings &t_settings) { return t_settings.background_style != BackgroundStyle::none; }},
	{&Rows::background_light, &Settings::background_light_intensity,
	 [](const Settings &t_settings) { return t_settings.background_light; }},
	{&Rows::background_grain, &Settings::background_grain_intensity,
	 [](const Settings &t_settings) { return t_settings.background_grain; }},
};

const SettingsPanel::Toggle SettingsPanel::toggles[toggle_count]{
	{&Settings::show_notifications, &Rows::notifications},
	{&Settings::animations_enabled, &Rows::animations},
	{&Settings::hide_accounts_from_capture, &Rows::hide_from_capture},
	{&Settings::block_overlay_injection, &Rows::block_overlay_injection},
	{&Settings::background_light, &Rows::background_light},
	{&Settings::background_grain, &Rows::background_grain},
};

SettingsPanel::SettingsPanel(Settings &t_settings, Fonts &t_fonts, Renderer &t_renderer, const Window &t_window,
							 const Assets &t_assets, CommandQueue &t_commands)
	: m_settings(t_settings)
	, m_fonts(t_fonts)
	, m_renderer(t_renderer)
	, m_window(t_window)
	, m_assets(t_assets)
	, m_commands(t_commands)
	, m_font_list(t_fonts, t_settings,
				  ListPopupOptions{
					  .search_placeholder = "Search fonts...",
					  .empty_message = "No fonts match your search",
					  .min_width = font_list_width,
				  })
	, m_theme_list(t_fonts, t_settings,
				   ListPopupOptions{
					   .search_placeholder = "Search themes",
					   .empty_message = "No themes match your search",
					   .min_width = theme_list_width,
					   .preview_size = theme_preview_size,
					   .draw_preview =
						   [](DrawList &t_draw_list, Rect t_preview, u32 t_theme, Color t_backdrop, u8 t_alpha) {
							   draw_theme_preview(t_draw_list, t_preview, static_cast<ThemeKind>(t_theme), t_backdrop,
												  t_alpha);
						   },
					   .hover_preview_seconds = theme_hover_preview_seconds,
				   })
	, m_color_picker(t_fonts)
{
	m_search.set_max_length(search_max_length);
	m_search.set_placeholder("Search settings");

	sync_with_settings();
}

void SettingsPanel::sync_with_settings()
{
	refresh_font_label();

	m_font_size_shown = m_settings.font_size;
	m_secondary_font_size_shown = m_settings.secondary_font_size;
	m_animation_speed_shown = m_settings.animation_speed;
	m_corner_roundness_shown = m_settings.corner_roundness;
	for (u32 i = 0; i < percent_slider_count; i += 1) {
		m_percent_shown[i] = m_settings.*percent_sliders[i].value;
		m_percent_reveal[i] = percent_sliders[i].shown(m_settings) ? 1.0f : 0.0f;
	}

	m_animation_speed_reveal = m_settings.animations_enabled ? 1.0f : 0.0f;
	m_auto_lock_shown = static_cast<float>(auto_lock_stop(m_settings.auto_lock_minutes));
	m_accent_shown[0] = m_settings.accent.r;
	m_accent_shown[1] = m_settings.accent.g;
	m_accent_shown[2] = m_settings.accent.b;

	for (u32 i = 0; i < toggle_count; i += 1) {
		m_toggles_shown[i] = m_settings.*toggles[i].value ? 1.0f : 0.0f;
	}

	for (u32 i = 0; i < background_count; i += 1) {
		m_pattern_ring[i] = i == static_cast<u32>(m_settings.background_style) ? 1.0f : 0.0f;
	}

	m_close_choice_shown = m_settings.close_to_tray ? 0.0f : 1.0f;
}

void SettingsPanel::restore_committed_previews(Settings &t_settings) const
{
	if (m_theme_before_preview) {
		t_settings.theme = m_theme_before_preview->theme;
		t_settings.accent = m_theme_before_preview->accent;
	}
}

void SettingsPanel::open()
{
	m_open = true;
	m_search.set_focused(false);
	clear_search();

	if (m_installed_fonts.names.empty()) {
		m_installed_fonts = installed_fonts();
	}

	refresh_font_label();
}

void SettingsPanel::close()
{
	m_open = false;
	m_search.set_focused(false);
	for (Draggable &drag : m_slider_drags) {
		drag.end();
	}
	m_font_list.close();
	m_theme_list.close();
	m_color_picker.close();
	m_pattern_open = false;
	m_tooltip.reset();
}

SettingsPanel::Layout SettingsPanel::layout() const
{
	const Vec2 window = m_window.size();
	const float size_scale = std::max({1.0f, m_fonts.body().pixel_height() / reference_body_pixel_height,
									   m_fonts.secondary().pixel_height() / reference_secondary_pixel_height});
	const Vec2 max_size{panel_max_size.x * size_scale, panel_max_size.y * size_scale};

	float width = std::min(max_size.x, std::max(0.0f, window.x - panel_margin * 2.0f));
	float height = std::min(max_size.y, std::max(0.0f, window.y - panel_margin * 2.0f));

	const float aspect = max_size.x / max_size.y;
	if (width / height > aspect) {
		width = height * aspect;
	} else {
		height = width / aspect;
	}

	Layout result{};
	result.docked = width < panel_min_size.x * size_scale || height < panel_min_size.y * size_scale;
	result.panel = result.docked ? m_window.content_rect().inset(0.0f, 1.0f)
								 : Rect{0.0f, 0.0f, window.x, window.y}.centered(width, height);
	result.inner = result.panel.inset(panel_border);

	Rect remaining = result.inner;
	result.header = remaining.split_top(header_height(m_fonts));
	result.rail = Rect{remaining.x, remaining.y, rail_width, remaining.h};
	result.rows_region =
		Rect{result.rail.right() + 1.0f, remaining.y, remaining.right() - result.rail.right() - 1.0f, remaining.h};

	return result;
}

SettingsPanel::Rows SettingsPanel::rows(const Layout &t_layout) const
{
	const Rect hidden{0.0f, -1.0e6f, 0.0f, 0.0f};
	const Rect region = t_layout.rows_region;

	Rect cursor{region.x + card_margin_x, region.y + rows_top_padding - m_rows_scroll.offset(),
				region.w - card_margin_x * 2.0f, 1.0e6f};
	const float top = cursor.y;

	Rows result{};
	std::fill(std::begin(result.cards), std::end(result.cards), hidden);
	std::fill(std::begin(result.group_titles), std::end(result.group_titles), hidden);

	std::optional<u32> open_group;
	float card_top = 0.0f;

	const auto close_card = [&]() {
		if (open_group) {
			result.cards[*open_group] = Rect{cursor.x, card_top, cursor.w, cursor.y - card_top};
		}
	};

	for (const RowSpec &spec : row_specs) {
		const float height = row_extent(spec);

		if (!is_listed(spec) || height <= 0.0f) {
			result.*spec.row = hidden;
			continue;
		}

		if (open_group != spec.group) {
			close_card();

			if (open_group) {
				cursor.split_top(group_gap);
			}

			result.group_titles[spec.group] = cursor.split_top(group_title_height(m_fonts));
			open_group = spec.group;
			card_top = cursor.y;
		}

		result.*spec.row = cursor.split_top(height);
		result.listed_count += 1;
	}

	close_card();
	result.content_height = cursor.y - top + rows_top_padding * 2.0f;

	return result;
}

const SettingsPanel::RowSpec &SettingsPanel::spec_of(Rect Rows::*t_row)
{
	for (const RowSpec &spec : row_specs) {
		if (spec.row == t_row) return spec;
	}

	return row_specs[0];
}

bool SettingsPanel::is_searching() const
{
	return !m_search.value().empty();
}

bool SettingsPanel::matches_search(const RowSpec &t_spec) const
{
	const std::string_view fields[]{t_spec.title, t_spec.description, t_spec.keywords,
									tab_names[static_cast<u32>(t_spec.tab)], group_specs[t_spec.group].title};

	return matches_every_word(m_search.value(), fields);
}

bool SettingsPanel::is_listed(const RowSpec &t_spec) const
{
	return is_searching() ? matches_search(t_spec) : t_spec.tab == m_tab;
}

float SettingsPanel::row_extent(const RowSpec &t_spec) const
{
	return *t_spec.description == '\0' ? single_row_height(m_fonts) : row_height(m_fonts);
}

float SettingsPanel::inline_slider_left(Rect t_row) const
{
	float widest = 0.0f;

	for (Rect Rows::*row : {&Rows::background, &Rows::background_light, &Rows::background_grain, &Rows::animations}) {
		widest = std::max(widest, text_width(m_fonts.body(), spec_of(row).title));
	}

	return snapped_to_pixel(t_row.x + row_inset_x + widest + inline_slider_gap);
}

Rect SettingsPanel::slider_line(const Rows &t_rows, SliderKind t_slider) const
{
	switch (t_slider) {
		case SliderKind::corner_roundness:
			return t_rows.corner_roundness;
		case SliderKind::pattern_strength:
			return t_rows.background;
		case SliderKind::light_strength:
			return t_rows.background_light;
		case SliderKind::grain_strength:
			return t_rows.background_grain;
		case SliderKind::animation_speed:
			return t_rows.animations;
		case SliderKind::auto_lock:
			return t_rows.auto_lock;
		case SliderKind::count:
			break;
	}

	return Rect{};
}

Rect SettingsPanel::slider_rect(const Rows &t_rows, SliderKind t_slider) const
{
	const Rect line = slider_line(t_rows, t_slider);

	switch (t_slider) {
		case SliderKind::corner_roundness:
		case SliderKind::auto_lock:
			return slider_control_rect(line, m_fonts);
		case SliderKind::pattern_strength:
		case SliderKind::light_strength:
		case SliderKind::grain_strength:
			return inline_slider_rect(line, pattern_select_rect(line, m_fonts), inline_slider_left(line), m_fonts);
		case SliderKind::animation_speed:
			return inline_slider_rect(line, toggle_rect(line, m_fonts), inline_slider_left(line), m_fonts);
		case SliderKind::count:
			break;
	}

	return Rect{};
}

Rect SettingsPanel::slider_hit_rect(const Rows &t_rows, SliderKind t_slider) const
{
	const Rect line = slider_line(t_rows, t_slider);
	const Rect slider = slider_rect(t_rows, t_slider);

	return Rect{slider.x, line.y, slider.w, line.h};
}

float SettingsPanel::slider_visibility(SliderKind t_slider) const
{
	switch (t_slider) {
		case SliderKind::pattern_strength:
		case SliderKind::light_strength:
		case SliderKind::grain_strength:
			return m_percent_reveal[static_cast<u32>(t_slider) - static_cast<u32>(SliderKind::pattern_strength)];
		case SliderKind::animation_speed:
			return m_animation_speed_reveal;
		case SliderKind::corner_roundness:
		case SliderKind::auto_lock:
		case SliderKind::count:
			break;
	}

	return 1.0f;
}

float SettingsPanel::slider_fraction(SliderKind t_slider) const
{
	switch (t_slider) {
		case SliderKind::corner_roundness:
			return fraction_in(m_corner_roundness_shown, corner_roundness_min, corner_roundness_max);
		case SliderKind::pattern_strength:
		case SliderKind::light_strength:
		case SliderKind::grain_strength:
			return m_percent_shown[static_cast<u32>(t_slider) - static_cast<u32>(SliderKind::pattern_strength)];
		case SliderKind::animation_speed:
			return fraction_in(m_animation_speed_shown, animation_speed_min, animation_speed_max);
		case SliderKind::auto_lock:
			return m_auto_lock_shown / static_cast<float>(auto_lock_stop_count - 1);
		case SliderKind::count:
			break;
	}

	return 0.0f;
}

std::string_view SettingsPanel::slider_readout(SliderKind t_slider, char (&t_buffer)[16]) const
{
	int written = 0;

	switch (t_slider) {
		case SliderKind::corner_roundness:
			written = std::snprintf(t_buffer, sizeof(t_buffer), "%.0f%%", m_corner_roundness_shown * 100.0f);
			break;
		case SliderKind::pattern_strength:
		case SliderKind::light_strength:
		case SliderKind::grain_strength:
			written = std::snprintf(t_buffer, sizeof(t_buffer), "%.0f%%", slider_fraction(t_slider) * 100.0f);
			break;
		case SliderKind::animation_speed:
			written = std::snprintf(t_buffer, sizeof(t_buffer), "%.2fx", m_animation_speed_shown);
			break;
		case SliderKind::auto_lock:
			return auto_lock_label(m_settings.auto_lock_minutes, t_buffer);
		case SliderKind::count:
			break;
	}

	return std::string_view{t_buffer, static_cast<usize>(std::max(written, 0))};
}

std::optional<SettingsPanel::SliderKind> SettingsPanel::slider_at(const Layout &t_layout, const Rows &t_rows,
																  Vec2 t_point) const
{
	for (u32 i = 0; i < slider_count; i += 1) {
		const auto slider = static_cast<SliderKind>(i);

		if (slider_visibility(slider) > 0.5f &&
			hits(t_layout, slider_line(t_rows, slider), slider_hit_rect(t_rows, slider), t_point)) {
			return slider;
		}
	}

	return std::nullopt;
}

void SettingsPanel::apply_slider(SliderKind t_slider, float t_fraction)
{
	switch (t_slider) {
		case SliderKind::corner_roundness:
			m_settings.corner_roundness = value_at(t_fraction, corner_roundness_min, corner_roundness_max);
			set_corner_roundness(m_settings.corner_roundness);
			break;
		case SliderKind::pattern_strength:
		case SliderKind::light_strength:
		case SliderKind::grain_strength: {
			const u32 index = static_cast<u32>(t_slider) - static_cast<u32>(SliderKind::pattern_strength);
			m_settings.*percent_sliders[index].value = value_at(t_fraction, 0.0f, 1.0f);
			break;
		}
		case SliderKind::animation_speed:
			m_settings.animation_speed = value_at(t_fraction, animation_speed_min, animation_speed_max);
			animation::set_speed(m_settings.animation_speed);
			break;
		case SliderKind::auto_lock: {
			const auto stop = static_cast<u32>(std::lround(t_fraction * static_cast<float>(auto_lock_stop_count - 1)));
			m_settings.auto_lock_minutes = auto_lock_stops[std::min(stop, auto_lock_stop_count - 1)];
			break;
		}
		case SliderKind::count:
			break;
	}
}

Vec2 SettingsPanel::pattern_tile_size() const
{
	const Font &secondary = m_fonts.secondary();
	float width = tile_min_width;

	for (const OptionLabel &label : background_labels) {
		width = std::max(width, std::ceil(text_width(secondary, label.name) + tile_label_margin));
	}

	return Vec2{width, std::round(width * tile_aspect) + tile_label_gap + secondary.line_height()};
}

Rect SettingsPanel::pattern_popup_rect(const Rows &t_rows) const
{
	const Vec2 tile = pattern_tile_size();
	const u32 lines = (background_count + pattern_columns - 1) / pattern_columns;
	const float width = pattern_popup_padding * 2.0f + static_cast<float>(pattern_columns) * tile.x +
						static_cast<float>(pattern_columns - 1) * tile_gap;
	const float height = pattern_popup_padding * 2.0f + static_cast<float>(lines) * tile.y +
						 static_cast<float>(lines - 1) * tile_row_gap;
	const Rect anchor = pattern_select_rect(t_rows.background, m_fonts);
	const Rect bounds = m_window.content_rect().inset(pattern_popup_margin);

	float y = anchor.bottom() + pattern_popup_gap;
	if (y + height > bounds.bottom()) {
		y = anchor.y - pattern_popup_gap - height;
	}

	const float x = std::clamp(anchor.right() - width, bounds.x, std::max(bounds.x, bounds.right() - width));
	y = std::clamp(y, bounds.y, std::max(bounds.y, bounds.bottom() - height));

	return Rect{snapped_to_pixel(x), snapped_to_pixel(y), width, height};
}

Rect SettingsPanel::pattern_tile(Rect t_popup, u32 t_index) const
{
	const Vec2 tile = pattern_tile_size();
	const auto column = static_cast<float>(t_index % pattern_columns);
	const auto line = static_cast<float>(t_index / pattern_columns);

	return Rect{t_popup.x + pattern_popup_padding + column * (tile.x + tile_gap),
				t_popup.y + pattern_popup_padding + line * (tile.y + tile_row_gap), tile.x, tile.y};
}

std::optional<u32> SettingsPanel::pattern_at(Rect t_popup, Vec2 t_point) const
{
	for (u32 i = 0; i < background_count; i += 1) {
		if (pattern_tile(t_popup, i).contains(t_point)) return i;
	}

	return std::nullopt;
}

Rect SettingsPanel::close_choice_rect(Rect t_row) const
{
	float width = segment_inset * 2.0f;
	for (const std::string_view label : close_choice_labels) {
		width += text_width(m_fonts.body(), label) + segment_padding_x * 2.0f;
	}

	return right_aligned_control(t_row, m_fonts, width, control_height(m_fonts));
}

Rect SettingsPanel::close_segment(Rect t_choice, u32 t_index) const
{
	const float first_width = text_width(m_fonts.body(), close_choice_labels[0]) + segment_padding_x * 2.0f;
	const Rect inner = t_choice.inset(segment_inset);

	return t_index == 0 ? Rect{inner.x, inner.y, first_width, inner.h}
						: Rect{inner.x + first_width, inner.y, inner.w - first_width, inner.h};
}

Rect SettingsPanel::search_rect(const Layout &t_layout) const
{
	const Rect &header = t_layout.header;
	const float left =
		back_button_rect(header).right() + title_gap + text_width(m_fonts.body(), panel_title) + search_header_gap;
	const float right = header.right() - row_padding_x;
	const float width = std::min(search_max_width, right - left);
	if (width < search_min_width) return Rect{};

	const float height = std::max(26.0f, m_fonts.secondary().line_height() + 9.0f);

	return Rect{snapped_to_pixel(left + (right - left - width) * 0.5f), header.center().y - height * 0.5f, width,
				height};
}

Rect SettingsPanel::search_text_rect(Rect t_search) const
{
	const float left = t_search.x + search_inset + search_icon_size + search_icon_gap;
	const float right = search_clear_rect(t_search).x - search_icon_gap * 0.5f;

	return Rect{left, t_search.y, std::max(0.0f, right - left), t_search.h};
}

Rect SettingsPanel::search_clear_rect(Rect t_search) const
{
	return Rect{t_search.right() - search_clear_margin - search_icon_size,
				t_search.center().y - search_icon_size * 0.5f, search_icon_size, search_icon_size};
}

void SettingsPanel::refresh_search()
{
	const std::string_view query = m_search.value();
	if (query == std::string_view{m_applied_query}) return;

	copy_to(query, m_applied_query);
	m_rows_scroll = Scrollable{};
	m_color_picker.close();
	m_pattern_open = false;
	m_tooltip.reset();
}

void SettingsPanel::clear_search()
{
	m_search.set_value("");
	refresh_search();
}

void SettingsPanel::focus_search()
{
	m_search.set_focused(true);
	m_search.apply(TextEdit::select_all);
}

Rect SettingsPanel::tab_rect(const Layout &t_layout, SettingsTab t_tab) const
{
	const float height = tab_height(m_fonts);
	const Rect &rail = t_layout.rail;

	return Rect{rail.x + rail_padding, rail.y + rail_padding + static_cast<u32>(t_tab) * height,
				rail.w - rail_padding * 2.0f, height};
}

std::optional<SettingsTab> SettingsPanel::tab_at(const Layout &t_layout, Vec2 t_point) const
{
	for (u32 i = 0; i < settings_tab_count; i += 1) {
		const auto tab = static_cast<SettingsTab>(i);
		if (tab_rect(t_layout, tab).contains(t_point)) return tab;
	}

	return std::nullopt;
}

void SettingsPanel::select_tab(SettingsTab t_tab)
{
	if (t_tab == m_tab) return;

	m_tab = t_tab;
	m_rows_scroll = Scrollable{};
	m_color_picker.close();
	m_pattern_open = false;
	m_tooltip.reset();
}

ScrollGeometry SettingsPanel::rows_scroll(const Layout &t_layout, const Rows &t_rows) const
{
	const Rect region = t_layout.rows_region;
	const float bottom_margin =
		t_layout.docked ? scrollbar_margin : std::max(scrollbar_margin, scaled_radius(panel_radius) * 0.75f);
	const Rect track{region.right() - scrollbar_width - scrollbar_margin, region.y + scrollbar_margin, scrollbar_width,
					 std::max(0.0f, region.h - scrollbar_margin - bottom_margin)};

	return ScrollGeometry{track, t_rows.content_height, region.h};
}

bool SettingsPanel::is_on_screen(const Layout &t_layout, Rect t_row) const
{
	return t_row.overlaps_vertically(t_layout.rows_region);
}

bool SettingsPanel::hits(const Layout &t_layout, Rect t_row, Rect t_control, Vec2 t_point) const
{
	return is_on_screen(t_layout, t_row) && t_layout.rows_region.contains(t_point) && t_control.contains(t_point);
}

ListPopup *SettingsPanel::open_list()
{
	if (m_font_list.is_open()) return &m_font_list;
	if (m_theme_list.is_open()) return &m_theme_list;

	return nullptr;
}

const ListPopup *SettingsPanel::open_list() const
{
	if (m_font_list.is_open()) return &m_font_list;
	if (m_theme_list.is_open()) return &m_theme_list;

	return nullptr;
}

bool SettingsPanel::has_popup_open() const
{
	return m_color_picker.is_open() || m_pattern_open || open_list() != nullptr;
}

bool SettingsPanel::is_row_hovered(const Layout &t_layout, Rect t_row) const
{
	return is_blocking() && !has_popup_open() && t_layout.rows_region.contains(m_mouse) &&
		   is_on_screen(t_layout, t_row) && t_row.inset(highlight_inset_x, highlight_inset_y).contains(m_mouse);
}

Rect SettingsPanel::reset_row(const Rows &t_rows, ResettableSetting t_setting) const
{
	switch (t_setting) {
		case ResettableSetting::theme:
			return t_rows.theme;
		case ResettableSetting::font:
			return t_rows.font;
		case ResettableSetting::font_size:
			return t_rows.font_size;
		case ResettableSetting::secondary_font_size:
			return t_rows.secondary_font_size;
		case ResettableSetting::accent:
			return t_rows.accent;
		case ResettableSetting::corner_roundness:
			return t_rows.corner_roundness;
		case ResettableSetting::background:
			return t_rows.background;
		case ResettableSetting::background_light:
			return t_rows.background_light;
		case ResettableSetting::background_grain:
			return t_rows.background_grain;
		case ResettableSetting::animations:
			return t_rows.animations;
		case ResettableSetting::notifications:
			return t_rows.notifications;
		case ResettableSetting::hide_from_capture:
			return t_rows.hide_from_capture;
		case ResettableSetting::block_overlay_injection:
			return t_rows.block_overlay_injection;
		case ResettableSetting::close_to_tray:
			return t_rows.close_to_tray;
		case ResettableSetting::auto_lock:
			return t_rows.auto_lock;
		case ResettableSetting::count:
			break;
	}

	return Rect{};
}

Rect SettingsPanel::reset_control(const Rows &t_rows, ResettableSetting t_setting) const
{
	const Rect row = reset_row(t_rows, t_setting);

	switch (t_setting) {
		case ResettableSetting::theme:
		case ResettableSetting::font:
			return dropdown_rect(row, m_fonts);
		case ResettableSetting::background:
			return pattern_select_rect(row, m_fonts);
		case ResettableSetting::font_size:
		case ResettableSetting::secondary_font_size:
			return stepper_rect(row, m_fonts);
		case ResettableSetting::accent:
			return swatch_rect(row, m_fonts).inset(-(swatch_ring_gap + swatch_ring));
		case ResettableSetting::corner_roundness:
			return slider_control_rect(row, m_fonts);
		case ResettableSetting::close_to_tray:
			return close_choice_rect(row);
		case ResettableSetting::animations:
		case ResettableSetting::notifications:
		case ResettableSetting::hide_from_capture:
		case ResettableSetting::block_overlay_injection:
		case ResettableSetting::background_light:
		case ResettableSetting::background_grain:
			return toggle_rect(row, m_fonts);
		case ResettableSetting::auto_lock:
			return slider_control_rect(row, m_fonts);
		case ResettableSetting::count:
			break;
	}

	return Rect{};
}

Rect SettingsPanel::reset_button(const Rows &t_rows, ResettableSetting t_setting) const
{
	return reset_button_rect(reset_control(t_rows, t_setting), reset_row(t_rows, t_setting), m_fonts);
}

bool SettingsPanel::is_default(ResettableSetting t_setting) const
{
	const Settings &defaults = default_settings;
	const auto same = [](float t_a, float t_b) { return std::fabs(t_a - t_b) < 0.001f; };

	switch (t_setting) {
		case ResettableSetting::theme:
			return m_settings.theme == defaults.theme;
		case ResettableSetting::font:
			return std::string_view{m_settings.font_name} == defaults.font_name;
		case ResettableSetting::font_size:
			return same(m_settings.font_size, defaults.font_size);
		case ResettableSetting::secondary_font_size:
			return same(m_settings.secondary_font_size, defaults.secondary_font_size);
		case ResettableSetting::accent:
			return m_settings.accent == theme_preset(m_settings.theme).default_accent;
		case ResettableSetting::corner_roundness:
			return same(m_settings.corner_roundness, defaults.corner_roundness);
		case ResettableSetting::background:
			return m_settings.background_style == defaults.background_style &&
				   same(m_settings.background_intensity, defaults.background_intensity);
		case ResettableSetting::background_light:
			return m_settings.background_light == defaults.background_light &&
				   same(m_settings.background_light_intensity, defaults.background_light_intensity);
		case ResettableSetting::background_grain:
			return m_settings.background_grain == defaults.background_grain &&
				   same(m_settings.background_grain_intensity, defaults.background_grain_intensity);
		case ResettableSetting::animations:
			return m_settings.animations_enabled == defaults.animations_enabled &&
				   same(m_settings.animation_speed, defaults.animation_speed);
		case ResettableSetting::notifications:
			return m_settings.show_notifications == defaults.show_notifications;
		case ResettableSetting::hide_from_capture:
			return m_settings.hide_accounts_from_capture == defaults.hide_accounts_from_capture;
		case ResettableSetting::block_overlay_injection:
			return m_settings.block_overlay_injection == defaults.block_overlay_injection;
		case ResettableSetting::close_to_tray:
			return m_settings.close_to_tray == defaults.close_to_tray;
		case ResettableSetting::auto_lock:
			return m_settings.auto_lock_minutes == defaults.auto_lock_minutes;
		case ResettableSetting::count:
			break;
	}

	return true;
}

void SettingsPanel::reset(ResettableSetting t_setting)
{
	const Settings &defaults = default_settings;

	switch (t_setting) {
		case ResettableSetting::theme:
			select_theme(defaults.theme);
			break;
		case ResettableSetting::font:
			if (load_fonts(defaults.font_name)) {
				copy_to(defaults.font_name, m_settings.font_name);
				refresh_font_label();
			}
			break;
		case ResettableSetting::font_size:
			m_settings.font_size = defaults.font_size;
			load_fonts(m_settings.font_name);
			break;
		case ResettableSetting::secondary_font_size:
			m_settings.secondary_font_size = defaults.secondary_font_size;
			load_fonts(m_settings.font_name);
			break;
		case ResettableSetting::accent:
			m_settings.accent = theme_preset(m_settings.theme).default_accent;
			m_color_picker.close();
			break;
		case ResettableSetting::corner_roundness:
			m_settings.corner_roundness = defaults.corner_roundness;
			break;
		case ResettableSetting::background:
			m_settings.background_style = defaults.background_style;
			m_settings.background_intensity = defaults.background_intensity;
			break;
		case ResettableSetting::background_light:
			m_settings.background_light = defaults.background_light;
			m_settings.background_light_intensity = defaults.background_light_intensity;
			break;
		case ResettableSetting::background_grain:
			m_settings.background_grain = defaults.background_grain;
			m_settings.background_grain_intensity = defaults.background_grain_intensity;
			break;
		case ResettableSetting::animations:
			m_settings.animations_enabled = defaults.animations_enabled;
			m_settings.animation_speed = defaults.animation_speed;
			animation::set_enabled(m_settings.animations_enabled);
			animation::set_speed(m_settings.animation_speed);
			break;
		case ResettableSetting::notifications:
			m_settings.show_notifications = defaults.show_notifications;
			break;
		case ResettableSetting::hide_from_capture:
			m_settings.hide_accounts_from_capture = defaults.hide_accounts_from_capture;
			break;
		case ResettableSetting::block_overlay_injection:
			m_settings.block_overlay_injection = defaults.block_overlay_injection;
			break;
		case ResettableSetting::close_to_tray:
			m_settings.close_to_tray = defaults.close_to_tray;
			break;
		case ResettableSetting::auto_lock:
			m_settings.auto_lock_minutes = defaults.auto_lock_minutes;
			break;
		case ResettableSetting::count:
			return;
	}

	m_reset_spin[static_cast<u32>(t_setting)] = 1.0f;
}

bool SettingsPanel::load_fonts(std::string_view t_file)
{
	return m_fonts.load(m_renderer, t_file, m_settings.font_size, m_settings.secondary_font_size, m_window.dpi_scale());
}

void SettingsPanel::open_font_list()
{
	m_color_picker.close();
	m_installed_fonts = installed_fonts();
	m_font_names.assign(m_installed_fonts.names.begin(), m_installed_fonts.names.end());
	m_font_list.open(m_font_names, m_installed_fonts.index_of_file(m_settings.font_name));
}

void SettingsPanel::open_theme_list()
{
	m_color_picker.close();
	m_theme_before_preview = ThemeChoice{m_settings.theme, m_settings.accent};
	m_theme_list.open(theme_names, static_cast<u32>(m_settings.theme));
}

void SettingsPanel::choose_font(u32 t_index)
{
	const std::string &file = m_installed_fonts.files[t_index];
	if (!load_fonts(file)) return;

	copy_to(file, m_settings.font_name);
	refresh_font_label();
}

void SettingsPanel::refresh_font_label()
{
	const std::string_view file = m_settings.font_name;

	if (const std::optional<u32> index = m_installed_fonts.index_of_file(file)) {
		m_font_label = m_installed_fonts.names[*index];
		return;
	}

	const usize name_start = file.find_last_of("\\/") + 1;
	m_font_label = file.substr(name_start, file.rfind('.') - name_start);
}

void SettingsPanel::show_theme(ThemeChoice t_choice)
{
	m_settings.theme = t_choice.theme;
	m_settings.accent = t_choice.accent;
	m_color_picker.close();
	fade_to_theme(t_choice.theme);
}

void SettingsPanel::select_theme(ThemeKind t_theme)
{
	m_theme_before_preview.reset();
	show_theme(ThemeChoice{t_theme, theme_preset(t_theme).default_accent});
}

void SettingsPanel::choose_theme(ThemeKind t_theme)
{
	const bool kept_original = m_theme_before_preview && m_theme_before_preview->theme == t_theme;
	const Color accent = kept_original ? m_theme_before_preview->accent : theme_preset(t_theme).default_accent;

	m_theme_before_preview.reset();
	show_theme(ThemeChoice{t_theme, accent});
}

void SettingsPanel::cycle_theme(i32 t_step)
{
	const auto count = static_cast<i32>(theme_count);
	const i32 next = ((static_cast<i32>(m_settings.theme) + t_step) % count + count) % count;

	select_theme(static_cast<ThemeKind>(next));
}

void SettingsPanel::update_theme_preview()
{
	if (!m_theme_before_preview) return;

	const ThemeChoice original = *m_theme_before_preview;
	ThemeChoice shown = original;

	if (!m_theme_list.is_open()) {
		m_theme_before_preview.reset();
	} else if (const std::optional<u32> previewed = m_theme_list.previewed_item()) {
		const auto kind = static_cast<ThemeKind>(*previewed);
		if (kind != original.theme) {
			shown = ThemeChoice{kind, theme_preset(kind).default_accent};
		}
	}

	if (shown != ThemeChoice{m_settings.theme, m_settings.accent}) {
		show_theme(shown);
	}
}

void SettingsPanel::choose_background(u32 t_index)
{
	m_pattern_open = false;
	m_settings.background_style = static_cast<BackgroundStyle>(t_index);
}

void SettingsPanel::pull_picked_color()
{
	if (m_color_picker.take_changed()) {
		m_settings.accent = m_color_picker.color();
	}
}

void SettingsPanel::step_font_size(Rect t_stepper, float &t_value, float t_min, float t_max, Vec2 t_point)
{
	if (stepper_minus(t_stepper).contains(t_point)) {
		t_value = std::max(t_min, t_value - 1.0f);
		load_fonts(m_settings.font_name);
	} else if (stepper_plus(t_stepper).contains(t_point)) {
		t_value = std::min(t_max, t_value + 1.0f);
		load_fonts(m_settings.font_name);
	}
}

void SettingsPanel::update(float t_delta_seconds)
{
	const float scale_travel = m_window.size().x * 0.5f * (1.0f - panel_closed_scale);
	m_open_amount = animation::ease_toward(m_open_amount, m_open ? 1.0f : 0.0f, open_ease_rate, t_delta_seconds,
										   animation::settled_pixels / scale_travel);

	for (u32 i = 0; i < toggle_count; i += 1) {
		const float target = m_settings.*toggles[i].value ? 1.0f : 0.0f;
		m_toggles_shown[i] = animation::ease_toward(m_toggles_shown[i], target, toggle_ease_rate, t_delta_seconds);
	}

	const auto ease_value = [t_delta_seconds](float t_shown, float t_actual) {
		return animation::ease_toward(t_shown, t_actual, value_ease_rate, t_delta_seconds);
	};

	m_font_size_shown = ease_value(m_font_size_shown, m_settings.font_size);
	m_secondary_font_size_shown = ease_value(m_secondary_font_size_shown, m_settings.secondary_font_size);
	const auto dragging = [this](SliderKind t_slider) {
		return m_slider_drags[static_cast<u32>(t_slider)].is_pressed();
	};

	m_animation_speed_shown = dragging(SliderKind::animation_speed)
								  ? m_settings.animation_speed
								  : ease_value(m_animation_speed_shown, m_settings.animation_speed);
	m_corner_roundness_shown = dragging(SliderKind::corner_roundness)
								   ? m_settings.corner_roundness
								   : ease_value(m_corner_roundness_shown, m_settings.corner_roundness);
	for (u32 i = 0; i < percent_slider_count; i += 1) {
		const PercentSlider &slider = percent_sliders[i];
		const auto kind = static_cast<SliderKind>(static_cast<u32>(SliderKind::pattern_strength) + i);

		m_percent_shown[i] =
			dragging(kind) ? m_settings.*slider.value : ease_value(m_percent_shown[i], m_settings.*slider.value);
		m_percent_reveal[i] = animation::ease_toward(m_percent_reveal[i], slider.shown(m_settings) ? 1.0f : 0.0f,
													 reveal_ease_rate, t_delta_seconds);
	}

	m_animation_speed_reveal = animation::ease_toward(
		m_animation_speed_reveal, m_settings.animations_enabled ? 1.0f : 0.0f, reveal_ease_rate, t_delta_seconds);

	m_auto_lock_shown = ease_value(m_auto_lock_shown, static_cast<float>(auto_lock_stop(m_settings.auto_lock_minutes)));

	for (u32 i = 0; i < background_count; i += 1) {
		const float target = i == static_cast<u32>(m_settings.background_style) ? 1.0f : 0.0f;
		m_pattern_ring[i] = animation::ease_toward(m_pattern_ring[i], target, tile_ring_rate, t_delta_seconds);
	}

	m_close_choice_shown = animation::ease_toward(m_close_choice_shown, m_settings.close_to_tray ? 0.0f : 1.0f,
												  segment_slide_rate, t_delta_seconds);
	m_pattern_open_amount = animation::ease_toward(m_pattern_open_amount, m_pattern_open ? 1.0f : 0.0f,
												   pattern_popup_rate, t_delta_seconds);

	set_corner_roundness(m_corner_roundness_shown);

	const u8 accent[3]{m_settings.accent.r, m_settings.accent.g, m_settings.accent.b};
	for (u32 channel = 0; channel < 3; channel += 1) {
		m_accent_shown[channel] = ease_value(m_accent_shown[channel], accent[channel]);
	}

	m_tab_indicator = animation::ease_toward(m_tab_indicator, static_cast<float>(m_tab), tab_slide_rate,
											 t_delta_seconds, animation::settled_pixels / tab_height(m_fonts));

	m_search.update(t_delta_seconds);
	refresh_search();

	m_color_picker.update(t_delta_seconds);
	pull_picked_color();

	update_hover_hints(t_delta_seconds);

	const Rows current_rows = rows(layout());
	const Rect popup_bounds = m_window.content_rect();

	m_font_list.update(t_delta_seconds, dropdown_rect(current_rows.font, m_fonts), popup_bounds);
	m_theme_list.update(t_delta_seconds, dropdown_rect(current_rows.theme, m_fonts), popup_bounds);
	update_theme_preview();

	m_rows_scroll.update(t_delta_seconds);
	m_tooltip.update(t_delta_seconds);
}

void SettingsPanel::update_hover_hints(float t_delta_seconds)
{
	const Layout current = layout();
	const Rows current_rows = rows(current);
	const bool pointer_live = is_blocking() && !has_popup_open() && current.rows_region.contains(m_mouse);

	const Rect back = back_button_rect(current.header);
	if (is_blocking() && !has_popup_open() && back.contains(m_mouse)) {
		m_tooltip.request("Back", back);
	}

	if (const std::optional<ColorPickerHint> hint = m_color_picker.hint(m_mouse)) {
		m_tooltip.request(hint->text, hint->anchor);
	}

	for (u32 i = 0; i < slider_count; i += 1) {
		const auto slider = static_cast<SliderKind>(i);
		const bool engaged = m_slider_drags[i].is_pressed() || (pointer_live && slider_visibility(slider) > 0.5f &&
																hits(current, slider_line(current_rows, slider),
																	 slider_hit_rect(current_rows, slider), m_mouse));

		m_slider_hover[i] =
			animation::ease_toward(m_slider_hover[i], engaged ? 1.0f : 0.0f, slider_hover_rate, t_delta_seconds);
	}

	for (u32 i = 0; i < resettable_count; i += 1) {
		const auto setting = static_cast<ResettableSetting>(i);
		const bool at_default = is_default(setting);

		m_reset_visible[i] = animation::ease_toward(
			m_reset_visible[i],
			pointer_live && !at_default && is_row_hovered(current, reset_row(current_rows, setting)) ? 1.0f : 0.0f,
			reset_appear_rate, t_delta_seconds);
		m_reset_spin[i] = animation::ease_toward(m_reset_spin[i], 0.0f, reset_spin_rate, t_delta_seconds);

		const Rect button = reset_button(current_rows, setting);
		if (pointer_live && !at_default && is_on_screen(current, reset_row(current_rows, setting)) &&
			button.contains(m_mouse)) {
			m_tooltip.request("Reset to default", button);
		}
	}
}

bool SettingsPanel::on_pointer_down(Vec2 t_point)
{
	if (!is_blocking()) return false;

	if (ListPopup *list = open_list()) {
		list->on_pointer_down(t_point);
		return true;
	}

	if (m_pattern_open) return true;

	const Layout current = layout();
	const Rect search = search_rect(current);
	const bool over_clear = is_searching() && search_clear_rect(search).contains(t_point);

	if (search.contains(t_point) && !over_clear) {
		m_color_picker.close();
		m_search.set_focused(true);
		m_search.on_pointer_down(m_fonts.secondary(), search_text_rect(search), t_point.x);
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
		m_slider_drags[static_cast<u32>(*slider)].begin(t_point);
		apply_slider(*slider, slider_fraction_at(slider_rect(current_rows, *slider), t_point.x));
	}

	return true;
}

bool SettingsPanel::on_pointer_move(Vec2 t_point)
{
	if (!is_blocking()) return false;

	if (ListPopup *list = open_list()) {
		list->on_pointer_move(t_point);
		return true;
	}

	const Layout current = layout();
	const Rows current_rows = rows(current);

	m_rows_scroll.on_pointer_move(t_point.y, rows_scroll(current, current_rows));

	m_color_picker.on_pointer_move(t_point);
	pull_picked_color();

	for (u32 i = 0; i < slider_count; i += 1) {
		if (!m_slider_drags[i].is_pressed()) continue;

		const auto slider = static_cast<SliderKind>(i);
		m_slider_drags[i].update(t_point);
		apply_slider(slider, slider_fraction_at(slider_rect(current_rows, slider), t_point.x));
	}

	if (m_search.is_selecting()) {
		m_search.on_pointer_move(m_fonts.secondary(), search_text_rect(search_rect(current)), t_point.x);
	}

	return true;
}

bool SettingsPanel::on_pointer_up(Vec2 t_point)
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
			choose_theme(static_cast<ThemeKind>(*chosen));
		}

		return true;
	}

	if (m_color_picker.on_pointer_up(t_point)) {
		pull_picked_color();
		return true;
	}

	const bool ended_drag = m_rows_scroll.is_dragging() ||
							std::ranges::any_of(m_slider_drags, &Draggable::is_pressed) || m_search.is_selecting();

	m_rows_scroll.on_pointer_up();
	for (Draggable &drag : m_slider_drags) {
		drag.end();
	}

	m_search.on_pointer_up();

	if (!ended_drag && !m_color_picker.contains(t_point)) {
		handle_click(t_point);
	}

	return true;
}

void SettingsPanel::handle_click(Vec2 t_point)
{
	const Layout current = layout();

	if (m_pattern_open) {
		const Rect popup = pattern_popup_rect(rows(current));

		if (const std::optional<u32> pattern = pattern_at(popup, t_point)) {
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
	if (is_searching() && search_clear_rect(search).contains(t_point)) {
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

	for (u32 i = 0; i < resettable_count && !has_popup_open(); i += 1) {
		const auto setting = static_cast<ResettableSetting>(i);

		if (!is_default(setting) &&
			hits(current, reset_row(current_rows, setting), reset_button(current_rows, setting), t_point)) {
			reset(setting);
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

	const Rect close_choice = close_choice_rect(current_rows.close_to_tray);
	if (hits(current, current_rows.close_to_tray, close_choice, t_point)) {
		m_settings.close_to_tray = t_point.x < close_segment(close_choice, 1).x;
		return;
	}

	if (is_on_screen(current, current_rows.font_size)) {
		step_font_size(stepper_rect(current_rows.font_size, m_fonts), m_settings.font_size, font_size_min,
					   font_size_max, t_point);
	}

	if (is_on_screen(current, current_rows.secondary_font_size)) {
		step_font_size(stepper_rect(current_rows.secondary_font_size, m_fonts), m_settings.secondary_font_size,
					   secondary_font_size_min, secondary_font_size_max, t_point);
	}

	for (const Toggle &toggle : toggles) {
		const Rect row = current_rows.*toggle.row;

		if (hits(current, row, toggle_rect(row, m_fonts), t_point)) {
			m_settings.*toggle.value = !(m_settings.*toggle.value);
			animation::set_enabled(m_settings.animations_enabled);
		}
	}

	const Rect swatch = swatch_rect(current_rows.accent, m_fonts);
	if (hits(current, current_rows.accent, swatch, t_point)) {
		if (m_color_picker.is_open()) {
			m_color_picker.close();
		} else {
			m_color_picker.open(m_settings.accent, swatch, m_window.content_rect());
		}

		return;
	}

	if (m_color_picker.is_open()) {
		m_color_picker.close();
		return;
	}

	const Rect reset_password = master_password_button_rect(current_rows.master_password, m_fonts);
	if (hits(current, current_rows.master_password, reset_password, t_point)) {
		m_commands.push(Command{.type = CommandType::request_new_master_password});
		close();
	}
}

bool SettingsPanel::on_right_click(Vec2 t_point)
{
	if (!is_blocking()) return false;

	ListPopup *list = open_list();

	if (list == nullptr && m_color_picker.contains(t_point)) {
		if (TextInput *field = m_color_picker.on_right_click(t_point)) {
			m_commands.push(Command{.type = CommandType::show_text_menu, .position = t_point, .text_input = field});
		}

		return true;
	}

	const Rect search = search_rect(layout());
	if (list == nullptr && search.contains(t_point)) {
		m_search.set_focused(true);
		m_search.on_right_click(m_fonts.secondary(), search_text_rect(search), t_point.x);
		m_commands.push(Command{.type = CommandType::show_text_menu, .position = t_point, .text_input = &m_search});
		return true;
	}

	if (TextInput *list_search = list != nullptr ? list->on_right_click(t_point) : nullptr) {
		m_commands.push(Command{.type = CommandType::show_text_menu, .position = t_point, .text_input = list_search});
	}

	return true;
}

bool SettingsPanel::on_scroll(Vec2 t_point, float t_wheel_delta)
{
	if (!is_blocking()) return false;

	if (ListPopup *list = open_list()) {
		list->on_scroll(t_wheel_delta);
		return true;
	}

	if (m_pattern_open) return true;

	const Layout current = layout();
	const Rows current_rows = rows(current);

	if (is_control_down() && hits(current, current_rows.theme, dropdown_rect(current_rows.theme, m_fonts), t_point)) {
		m_theme_wheel += t_wheel_delta;

		while (std::fabs(m_theme_wheel) >= 1.0f) {
			const float notch = m_theme_wheel > 0.0f ? 1.0f : -1.0f;
			cycle_theme(notch > 0.0f ? -1 : 1);
			m_theme_wheel -= notch;
		}

		return true;
	}

	m_rows_scroll.on_scroll(t_wheel_delta, rows_scroll(current, current_rows));

	return true;
}

bool SettingsPanel::on_key_down(u32 t_key)
{
	if (!is_blocking()) return false;

	if (m_font_list.is_open()) {
		if (const std::optional<u32> chosen = m_font_list.on_key_down(t_key)) {
			choose_font(*chosen);
		}
	} else if (m_theme_list.is_open()) {
		if (const std::optional<u32> chosen = m_theme_list.on_key_down(t_key)) {
			choose_theme(static_cast<ThemeKind>(*chosen));
		}
	} else if (m_pattern_open) {
		m_pattern_open = t_key != VK_ESCAPE;
	} else if (m_color_picker.on_key_down(t_key)) {
		pull_picked_color();
	} else if (t_key == VK_ESCAPE && is_searching()) {
		clear_search();
	} else if (t_key == VK_ESCAPE && m_search.is_focused()) {
		m_search.set_focused(false);
	} else if (t_key == VK_ESCAPE) {
		close();
	} else if (t_key == VK_TAB && is_control_down()) {
		const u32 step = (GetKeyState(VK_SHIFT) & 0x8000) != 0 ? settings_tab_count - 1 : 1;
		clear_search();
		select_tab(static_cast<SettingsTab>((static_cast<u32>(m_tab) + step) % settings_tab_count));
	} else if (t_key == 'F' && is_control_down()) {
		focus_search();
	} else if (m_search.is_focused()) {
		m_search.on_key_down(t_key);
	}

	return true;
}

bool SettingsPanel::on_char(u32 t_character)
{
	if (!is_blocking()) return false;

	if (ListPopup *list = open_list()) {
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

CursorKind SettingsPanel::cursor() const
{
	if (!is_blocking()) return CursorKind::arrow;
	if (const ListPopup *list = open_list()) return list->cursor(m_mouse);

	if (m_pattern_open) {
		return pattern_at(pattern_popup_rect(rows(layout())), m_mouse) ? CursorKind::hand : CursorKind::arrow;
	}

	const bool dragging = m_rows_scroll.is_dragging() || m_color_picker.is_dragging() ||
						  std::ranges::any_of(m_slider_drags, &Draggable::is_pressed);
	if (dragging) return CursorKind::drag;
	if (m_search.is_selecting()) return CursorKind::ibeam;

	if (m_color_picker.is_open()) {
		const CursorKind picker = m_color_picker.cursor(m_mouse);
		if (picker != CursorKind::arrow) return picker;
	}

	const Layout current = layout();
	if (back_button_rect(current.header).contains(m_mouse)) return CursorKind::hand;

	const Rect search = search_rect(current);
	if (is_searching() && search_clear_rect(search).contains(m_mouse)) return CursorKind::hand;
	if (search.contains(m_mouse)) return CursorKind::ibeam;

	if (tab_at(current, m_mouse)) return CursorKind::hand;
	if (!current.rows_region.contains(m_mouse)) return CursorKind::arrow;

	const Rows current_rows = rows(current);

	for (u32 i = 0; i < resettable_count; i += 1) {
		const auto setting = static_cast<ResettableSetting>(i);

		if (!is_default(setting) &&
			hits(current, reset_row(current_rows, setting), reset_button(current_rows, setting), m_mouse)) {
			return CursorKind::hand;
		}
	}

	if (slider_at(current, current_rows, m_mouse)) return CursorKind::hand;

	const Rect font_size = stepper_rect(current_rows.font_size, m_fonts);
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
		{current_rows.close_to_tray, close_choice_rect(current_rows.close_to_tray)},
		{current_rows.accent, swatch_rect(current_rows.accent, m_fonts)},
		{current_rows.master_password, master_password_button_rect(current_rows.master_password, m_fonts)},
	};

	for (const auto &target : clickable) {
		if (hits(current, target.row, target.control, m_mouse)) return CursorKind::hand;
	}

	for (const Toggle &toggle : toggles) {
		const Rect row = current_rows.*toggle.row;
		if (hits(current, row, toggle_rect(row, m_fonts), m_mouse)) return CursorKind::hand;
	}

	return m_rows_scroll.is_over_track(m_mouse, rows_scroll(current, current_rows)) ? CursorKind::hand
																					: CursorKind::arrow;
}

void SettingsPanel::draw_chrome(DrawList &t_draw_list, const Layout &t_layout, u8 t_alpha) const
{
	const Theme &colors = theme();
	const Font &body = m_fonts.body();

	if (t_layout.docked) {
		t_draw_list.add_rect(t_layout.panel, faded(colors.surface, t_alpha));
	} else {
		t_draw_list.add_bordered_rect(t_layout.panel, rounded(panel_radius), faded(colors.surface, t_alpha),
									  faded(colors.border, t_alpha), panel_border);
	}

	const Rect back = back_button_rect(t_layout.header);
	const bool back_hovered = back.contains(m_mouse);

	t_draw_list.add_rounded_rect(back, rounded(back.w * 0.5f),
								 faded(back_hovered ? colors.control_hover : colors.control, t_alpha));
	controls::draw_icon(t_draw_list, back.centered(back_icon_size, back_icon_size),
						m_assets.get(Asset::icon_arrow_back),
						faded(back_hovered ? colors.text : colors.text_dim, t_alpha));

	draw_text(t_draw_list, body, Vec2{back.right() + title_gap, body.centered_baseline(t_layout.header)}, panel_title,
			  faded(colors.text, t_alpha));
}

void SettingsPanel::draw_rail(DrawList &t_draw_list, const Layout &t_layout, u8 t_alpha) const
{
	const Theme &colors = theme();
	const Font &body = m_fonts.body();
	const bool pointer_live = is_blocking() && !has_popup_open();
	const bool searching = is_searching();
	const float height = tab_height(m_fonts);

	t_draw_list.add_rect(Rect{t_layout.rail.right(), t_layout.rail.y, 1.0f, t_layout.rail.h},
						 faded(colors.separator, t_alpha));

	if (!searching) {
		const Rect first_tab = tab_rect(t_layout, SettingsTab::appearance);
		const Rect active{first_tab.x, snapped_to_pixel(first_tab.y + m_tab_indicator * height), first_tab.w, height};
		t_draw_list.add_rounded_rect(active, rounded(control_radius), faded(colors.row_hover, t_alpha));

		const Rect indicator{first_tab.x,
							 snapped_to_pixel(first_tab.y + m_tab_indicator * height) + tab_indicator_inset,
							 tab_indicator_width, height - tab_indicator_inset * 2.0f};
		t_draw_list.add_rounded_rect(indicator, rounded(tab_indicator_width * 0.5f), faded(m_settings.accent, t_alpha));
	}

	for (u32 i = 0; i < settings_tab_count; i += 1) {
		const auto tab = static_cast<SettingsTab>(i);
		const Rect item = tab_rect(t_layout, tab);
		const bool hovered = pointer_live && item.contains(m_mouse);

		Color label = colors.text_dim;
		if (tab == m_tab && !searching) {
			label = mix(colors.text, m_settings.accent, active_tab_tint);
		} else if (hovered) {
			label = mix(colors.text_dim, colors.text, hovered_tab_brightening);
		}

		draw_text_truncated(t_draw_list, body, Vec2{item.x + tab_label_inset, body.centered_baseline(item)},
							tab_names[i], item.w - tab_label_inset * 2.0f, faded(label, t_alpha));
	}
}

void SettingsPanel::draw_label(DrawList &t_draw_list, const Rows &t_rows, Rect Rows::*t_row, Rect t_control,
							   u8 t_alpha) const
{
	const RowSpec &spec = spec_of(t_row);
	const Rect row = t_rows.*t_row;

	draw_row_label(t_draw_list, m_fonts, row, spec.title, spec.description, label_right_edge(t_control, row, m_fonts),
				   t_alpha);
}

void SettingsPanel::draw_dropdown_row(DrawList &t_draw_list, const Layout &t_layout, const Rows &t_rows,
									  Rect Rows::*t_row, std::string_view t_value, bool t_open, u8 t_alpha) const
{
	const Rect row = t_rows.*t_row;
	if (!is_on_screen(t_layout, row)) return;

	const Rect button = dropdown_rect(row, m_fonts);
	const bool hovered = !has_popup_open() && hits(t_layout, row, button, m_mouse);

	draw_label(t_draw_list, t_rows, t_row, button, t_alpha);
	draw_select(t_draw_list, m_fonts.body(), button, t_value, t_open, hovered, m_settings.accent, t_alpha);
}

void SettingsPanel::draw_appearance(DrawList &t_draw_list, const Layout &t_layout, const Rows &t_rows, u8 t_alpha) const
{
	const Font &body = m_fonts.body();
	const Color accent = m_settings.accent;

	draw_dropdown_row(t_draw_list, t_layout, t_rows, &Rows::theme,
					  theme_labels[static_cast<u32>(m_settings.theme)].name, m_theme_list.is_open(), t_alpha);
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
		const Theme &colors = theme();
		const Rect swatch = swatch_rect(t_rows.accent, m_fonts);
		const Rect ring = swatch.inset(-(swatch_ring_gap + swatch_ring));
		const Rect gap = swatch.inset(-swatch_ring_gap);
		const bool hovered = !has_popup_open() && hits(t_layout, t_rows.accent, ring, m_mouse);
		const Color ring_color = m_color_picker.is_open() ? accent : (hovered ? colors.text_dim : colors.border);
		const Color shown{static_cast<u8>(std::lround(m_accent_shown[0])),
						  static_cast<u8>(std::lround(m_accent_shown[1])),
						  static_cast<u8>(std::lround(m_accent_shown[2])), accent.a};

		draw_label(t_draw_list, t_rows, &Rows::accent, ring, t_alpha);
		t_draw_list.add_rounded_rect(ring, rounded(ring.w * 0.5f), faded(ring_color, t_alpha));
		t_draw_list.add_rounded_rect(gap, rounded(gap.w * 0.5f), faded(colors.popup, t_alpha));
		t_draw_list.add_rounded_rect(swatch, rounded(swatch.w * 0.5f), faded(shown, t_alpha));
	}
}

void SettingsPanel::draw_pattern_row(DrawList &t_draw_list, const Layout &t_layout, const Rows &t_rows,
									 u8 t_alpha) const
{
	const Rect row = t_rows.background;
	if (!is_on_screen(t_layout, row)) return;

	const Rect button = pattern_select_rect(row, m_fonts);
	const bool hovered = !has_popup_open() && hits(t_layout, row, button, m_mouse);
	const RowSpec &spec = spec_of(&Rows::background);

	draw_row_label(t_draw_list, m_fonts, row, spec.title, spec.description, inline_slider_left(row) - label_control_gap,
				   t_alpha);
	draw_select(t_draw_list, m_fonts.body(), button,
				background_labels[static_cast<u32>(m_settings.background_style)].name, m_pattern_open, hovered,
				m_settings.accent, t_alpha);
}

void SettingsPanel::draw_pattern_popup(DrawList &t_draw_list, const Rows &t_rows, u8 t_alpha) const
{
	if (m_pattern_open_amount <= 0.01f) return;

	const Theme &colors = theme();
	const Font &secondary = m_fonts.secondary();
	const float amount = m_pattern_open_amount;
	const auto alpha = static_cast<u8>(t_alpha * amount);
	const Rect resting = pattern_popup_rect(t_rows);
	const Rect popup{resting.x, snapped_to_pixel(resting.y - pattern_popup_rise * (1.0f - amount)), resting.w,
					 resting.h};
	const float preview_height = std::round(pattern_tile_size().x * tile_aspect);

	controls::draw_popup_shadow(t_draw_list, popup, pattern_popup_radius, amount);
	t_draw_list.add_bordered_rect(popup, rounded(pattern_popup_radius), faded(colors.popup, alpha),
								  faded(colors.border, alpha), 1.0f);

	for (u32 i = 0; i < background_count; i += 1) {
		const Rect cell = pattern_tile(popup, i);
		const Rect preview{cell.x, cell.y, cell.w, preview_height};
		const float ring = m_pattern_ring[i];
		const bool hovered = m_pattern_open && pattern_tile(resting, i).contains(m_mouse);

		if (ring > 0.01f) {
			const float outer = tile_ring_gap + tile_ring;

			t_draw_list.add_rounded_rect(preview.inset(-outer), rounded(tile_radius + outer),
										 faded(m_settings.accent, static_cast<u8>(alpha * ring)));
			t_draw_list.add_rounded_rect(preview.inset(-tile_ring_gap), rounded(tile_radius + tile_ring_gap),
										 faded(colors.popup, alpha));
		}

		t_draw_list.add_rounded_rect(preview, rounded(tile_radius),
									 faded(hovered ? colors.border : colors.separator, alpha));
		t_draw_list.add_pattern_swatch(preview.inset(1.0f), rounded(tile_radius - 1.0f), faded(colors.window, alpha),
									   i);

		const std::string_view name = background_labels[i].name;
		const Color label = hovered ? colors.text : mix(colors.text_dim, colors.text, ring);
		draw_text(t_draw_list, secondary,
				  Vec2{snapped_to_pixel(preview.center().x - text_width(secondary, name) * 0.5f),
					   preview.bottom() + tile_label_gap + secondary.ascent()},
				  name, faded(label, alpha));
	}
}

void SettingsPanel::draw_close_choice(DrawList &t_draw_list, const Layout &t_layout, const Rows &t_rows,
									  u8 t_alpha) const
{
	const Rect row = t_rows.close_to_tray;
	if (!is_on_screen(t_layout, row)) return;

	const Theme &colors = theme();
	const Font &body = m_fonts.body();
	const Rect choice = close_choice_rect(row);
	const Rect first = close_segment(choice, 0);
	const Rect second = close_segment(choice, 1);
	const float slide = m_close_choice_shown;
	const Rect thumb{first.x + (second.x - first.x) * slide, first.y, first.w + (second.w - first.w) * slide, first.h};
	const bool pointer_live = !has_popup_open() && t_layout.rows_region.contains(m_mouse);

	draw_label(t_draw_list, t_rows, &Rows::close_to_tray, choice, t_alpha);
	draw_inset_control(t_draw_list, choice, colors.separator, t_alpha);
	t_draw_list.add_bordered_rect(thumb, rounded(control_radius - segment_inset), faded(colors.popup, t_alpha),
								  faded(colors.separator, t_alpha), 1.0f);

	for (u32 i = 0; i < std::size(close_choice_labels); i += 1) {
		const Rect segment = close_segment(choice, i);
		const float selected = i == 0 ? 1.0f - slide : slide;
		const bool hovered = pointer_live && segment.contains(m_mouse);
		const Color idle = hovered ? mix(colors.text_dim, colors.text, hovered_tab_brightening) : colors.text_dim;

		draw_text_centered(t_draw_list, body, segment, close_choice_labels[i],
						   faded(mix(idle, colors.text, selected), t_alpha));
	}
}

void SettingsPanel::draw_sliders(DrawList &t_draw_list, const Layout &t_layout, const Rows &t_rows, u8 t_alpha) const
{
	const auto last_stop = static_cast<float>(auto_lock_stop_count - 1);
	const float never_amount = std::clamp(m_auto_lock_shown - (last_stop - 1.0f), 0.0f, 1.0f);
	char readout[16];

	for (u32 i = 0; i < slider_count; i += 1) {
		const auto slider = static_cast<SliderKind>(i);
		const Rect line = slider_line(t_rows, slider);
		const float visibility = slider_visibility(slider);
		const auto alpha = static_cast<u8>(t_alpha * visibility * visibility);
		if (!is_on_screen(t_layout, line) || alpha == 0) continue;

		const bool in_column = slider == SliderKind::corner_roundness || slider == SliderKind::auto_lock;
		const bool auto_lock = slider == SliderKind::auto_lock;
		const Rect bounds = slider_rect(t_rows, slider);

		if (in_column) {
			draw_label(t_draw_list, t_rows, auto_lock ? &Rows::auto_lock : &Rows::corner_roundness, bounds, t_alpha);
		}

		const SliderLook look{
			.readout = slider_readout(slider, readout),
			.caption = in_column ? "" : (slider == SliderKind::animation_speed ? "Speed" : "Strength"),
			.fraction = slider_fraction(slider),
			.hover = m_slider_hover[i],
			.accent = auto_lock ? mix(m_settings.accent, theme().text_faint, never_amount) : m_settings.accent,
			.steps = auto_lock ? auto_lock_stop_count - 1 : 0,
		};

		draw_slider(t_draw_list, m_fonts.secondary(), bounds, look, alpha);
	}
}

void SettingsPanel::draw_search(DrawList &t_draw_list, const Layout &t_layout, u8 t_alpha)
{
	const Rect search = search_rect(t_layout);
	if (search.w <= 0.0f) return;

	const Theme &colors = theme();
	const bool focused = m_search.is_focused();
	const bool searching = is_searching();

	controls::draw_field(t_draw_list, search, search.h * 0.5f, focused ? colors.text_dim : colors.control,
						 focused ? colors.row_hover : colors.field, t_alpha);

	const Rect icon{search.x + search_inset, search.center().y - search_icon_size * 0.5f, search_icon_size,
					search_icon_size};
	controls::draw_magnifier(t_draw_list, icon,
							 faded(focused || searching ? colors.text_dim : colors.text_faint, t_alpha));
	m_search.draw(t_draw_list, m_fonts.secondary(), search_text_rect(search), faded(colors.text, t_alpha),
				  faded(m_settings.accent, t_alpha), search);

	if (!searching) return;

	const Rect clear = search_clear_rect(search);
	controls::draw_x(t_draw_list, clear.inset(1.5f),
					 faded(clear.contains(m_mouse) ? colors.text : colors.text_faint, t_alpha));
}

void SettingsPanel::draw_cards(DrawList &t_draw_list, const Layout &t_layout, const Rows &t_rows, u8 t_alpha) const
{
	const Theme &colors = theme();

	for (const Rect &card : t_rows.cards) {
		if (!is_on_screen(t_layout, card)) continue;

		t_draw_list.add_bordered_rect(card, rounded(card_radius), faded(colors.popup, t_alpha),
									  faded(colors.separator, t_alpha), 1.0f);
	}

	std::optional<u32> group;

	for (const RowSpec &spec : row_specs) {
		const Rect row = t_rows.*spec.row;
		if (row.h <= 0.0f) continue;

		const bool first = group != spec.group;
		group = spec.group;

		if (first || !is_on_screen(t_layout, row)) continue;

		t_draw_list.add_rect(Rect{row.x + row_inset_x, row.y, row.w - row_inset_x * 2.0f, 1.0f},
							 faded(colors.separator, t_alpha));
	}
}

void SettingsPanel::draw_captions(DrawList &t_draw_list, const Layout &t_layout, const Rows &t_rows, u8 t_alpha) const
{
	const Font &secondary = m_fonts.secondary();

	for (u32 i = 0; i < group_count; i += 1) {
		const Rect title = t_rows.group_titles[i];
		if (!is_on_screen(t_layout, title)) continue;

		draw_text(t_draw_list, secondary,
				  Vec2{title.x + row_inset_x * 0.5f, title.bottom() - group_title_bottom_gap - secondary.descent()},
				  group_specs[i].title, faded(theme().text_faint, t_alpha));
	}
}

void SettingsPanel::draw_no_results(DrawList &t_draw_list, const Layout &t_layout, u8 t_alpha) const
{
	draw_text_centered(t_draw_list, m_fonts.secondary(), t_layout.rows_region, "No settings match your search",
					   faded(theme().text_dim, t_alpha));
}

void SettingsPanel::draw_toggles(DrawList &t_draw_list, const Layout &t_layout, const Rows &t_rows, u8 t_alpha) const
{
	for (u32 i = 0; i < toggle_count; i += 1) {
		const Toggle &toggle = toggles[i];
		const Rect row = t_rows.*toggle.row;
		if (!is_on_screen(t_layout, row)) continue;

		const Rect control = toggle_rect(row, m_fonts);
		draw_label(t_draw_list, t_rows, toggle.row, control, t_alpha);
		draw_toggle(t_draw_list, control, m_toggles_shown[i], m_settings.accent, t_alpha);
	}
}

void SettingsPanel::draw_master_password(DrawList &t_draw_list, const Layout &t_layout, const Rows &t_rows,
										 u8 t_alpha) const
{
	const Rect row = t_rows.master_password;
	if (!is_on_screen(t_layout, row)) return;

	const Rect button = master_password_button_rect(row, m_fonts);
	draw_label(t_draw_list, t_rows, &Rows::master_password, button, t_alpha);

	controls::draw_button(t_draw_list, m_fonts.body(), button, "Reset Password", controls::ButtonStyle::neutral,
						  m_settings.accent, true, !has_popup_open() && hits(t_layout, row, button, m_mouse), t_alpha);
}

void SettingsPanel::draw_reset_buttons(DrawList &t_draw_list, const Layout &t_layout, const Rows &t_rows,
									   u8 t_alpha) const
{
	const bool pointer_live = !has_popup_open() && t_layout.rows_region.contains(m_mouse);

	for (u32 i = 0; i < resettable_count; i += 1) {
		const auto setting = static_cast<ResettableSetting>(i);
		if (!is_on_screen(t_layout, reset_row(t_rows, setting))) continue;

		const Rect button = reset_button(t_rows, setting);
		draw_reset_button(t_draw_list, m_assets.get(Asset::icon_reset), button, m_reset_visible[i], m_reset_spin[i],
						  pointer_live && button.contains(m_mouse), t_alpha);
	}
}

void SettingsPanel::draw(DrawList &t_draw_list)
{
	PULSAR_PROFILE_SCOPE("SettingsPanel.Draw");

	if (m_open_amount <= 0.001f) return;

	const auto alpha = static_cast<u8>(255.0f * m_open_amount);
	const Layout current = layout();
	const Rows current_rows = rows(current);
	const ScrollGeometry scroll = rows_scroll(current, current_rows);

	if (!current.docked) {
		const Vec2 window = m_window.size();
		t_draw_list.add_rect(Rect{0.0f, 0.0f, window.x, window.y}, faded(theme().scrim, alpha));
	}

	const float scale = current.docked ? 1.0f : panel_closed_scale + (1.0f - panel_closed_scale) * m_open_amount;
	t_draw_list.push_scale(current.panel.center(), scale);

	draw_chrome(t_draw_list, current, alpha);
	draw_search(t_draw_list, current, alpha);
	draw_rail(t_draw_list, current, alpha);

	const Rect region = current.rows_region;
	const float divider_y = snapped_to_pixel(current.header.bottom());
	const float divider_thickness = snapped_to_pixel(1.0f);
	const Rect content{region.x, divider_y + divider_thickness, region.w,
					   region.bottom() - divider_y - divider_thickness};

	t_draw_list.push_clip(content);
	draw_cards(t_draw_list, current, current_rows, alpha);
	draw_captions(t_draw_list, current, current_rows, alpha);
	draw_appearance(t_draw_list, current, current_rows, alpha);
	draw_pattern_row(t_draw_list, current, current_rows, alpha);
	draw_close_choice(t_draw_list, current, current_rows, alpha);
	draw_toggles(t_draw_list, current, current_rows, alpha);
	draw_master_password(t_draw_list, current, current_rows, alpha);
	draw_sliders(t_draw_list, current, current_rows, alpha);
	draw_reset_buttons(t_draw_list, current, current_rows, alpha);

	if (is_searching() && current_rows.listed_count == 0) {
		draw_no_results(t_draw_list, current, alpha);
	}

	t_draw_list.pop_clip();

	const Rect card_span{region.x + card_margin_x, content.y, region.w - card_margin_x * 2.0f, content.h};
	const Scrollable::EdgeFades fades = m_rows_scroll.edge_fades(card_span, scroll);
	const float header_shadow = std::clamp(m_rows_scroll.offset() / header_shadow_travel, 0.0f, 1.0f);

	if (header_shadow > 0.0f) {
		const Color shadow = faded(theme().shadow, static_cast<u8>(alpha * header_shadow_alpha * header_shadow));
		t_draw_list.add_gradient(Rect{region.x, content.y, region.w, header_shadow_height}, shadow, shadow,
								 faded(shadow, 0), faded(shadow, 0));
	}

	if (fades.bottom.h > 0.0f) {
		const Color surface = faded(theme().surface, alpha);
		t_draw_list.add_gradient(fades.bottom, faded(surface, 0), faded(surface, 0), surface, surface);
	}

	t_draw_list.add_rect(Rect{current.header.x, divider_y, current.header.w, divider_thickness},
						 faded(theme().separator, alpha));
	m_rows_scroll.draw(t_draw_list, scroll, m_mouse, alpha);

	if (is_on_screen(current, current_rows.accent)) {
		m_color_picker.draw(t_draw_list, m_mouse);
	}

	draw_pattern_popup(t_draw_list, current_rows, alpha);

	m_font_list.draw(t_draw_list, m_mouse);
	m_theme_list.draw(t_draw_list, m_mouse);

	m_tooltip.draw(t_draw_list, m_fonts, current.panel, alpha);

	t_draw_list.pop_scale();
}
