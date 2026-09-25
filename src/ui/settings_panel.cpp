#include "ui/settings_panel.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <numbers>

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

namespace {
constexpr float open_ease_rate = 16.0f;
constexpr float toggle_ease_rate = 18.0f;
constexpr float value_ease_rate = 16.0f;
constexpr float reset_appear_rate = 20.0f;
constexpr float reset_spin_rate = 6.0f;

constexpr Vec2 panel_max_size{570.0f, 480.0f};
constexpr float reference_body_pixel_height = 24.0f;
constexpr float reference_secondary_pixel_height = 20.0f;
constexpr float panel_margin = 48.0f;
constexpr float panel_closed_scale = 0.94f;
constexpr float panel_radius = 16.0f;
constexpr float panel_border = 1.5f;

constexpr float row_padding_x = 26.0f;
constexpr float close_size = 26.0f;
constexpr float label_top_gap = 14.0f;
constexpr float label_line_gap = 6.0f;
constexpr float label_bottom_gap = 16.0f;
constexpr float label_control_gap = 16.0f;
constexpr float highlight_inset_x = 12.0f;
constexpr float highlight_radius = 8.0f;
constexpr float heading_top_gap = 22.0f;
constexpr float heading_bottom_gap = 8.0f;
constexpr float scrollbar_margin = 4.0f;

constexpr float reset_button_size = 28.0f;
constexpr float reset_button_gap = 10.0f;
constexpr float reset_icon_size = 18.0f;

constexpr float slider_track_width = 130.0f;
constexpr float slider_bar_height = 6.0f;
constexpr float slider_thumb_radius = 8.0f;
constexpr float slider_readout_gap = 8.0f;
constexpr float slider_readout_width = 48.0f;
constexpr float knob_ring = 1.5f;

constexpr float animation_speed_min = 0.25f;
constexpr float animation_speed_max = 3.0f;
constexpr float corner_roundness_min = 0.0f;
constexpr float corner_roundness_max = 1.5f;
constexpr float font_size_min = 10.0f;
constexpr float font_size_max = 24.0f;
constexpr float secondary_font_size_min = 8.0f;
constexpr float secondary_font_size_max = 18.0f;

constexpr Color color_background{26, 26, 29, 255};
constexpr Color color_border{70, 70, 76, 255};
constexpr Color color_text{224, 224, 228, 255};
constexpr Color color_text_dim{140, 140, 146, 255};
constexpr Color color_separator{58, 58, 64, 255};
constexpr Color color_control{40, 40, 45, 255};
constexpr Color color_toggle_off{70, 70, 76, 255};
constexpr Color color_row_hover{36, 36, 41, 255};
constexpr Color color_reset_idle{150, 150, 158, 255};
constexpr Color color_reset_hover{64, 64, 72, 255};
constexpr Color color_scroll_thumb{120, 120, 128, 190};

const Settings default_settings{};

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

float footer_height(const Fonts &t_fonts)
{
	return std::max(32.0f, t_fonts.secondary().line_height() + 16.0f);
}

float row_height(const Fonts &t_fonts)
{
	return label_top_gap + t_fonts.body().line_height() + label_line_gap + t_fonts.secondary().line_height() +
		   label_bottom_gap;
}

float heading_height(const Fonts &t_fonts)
{
	return heading_top_gap + t_fonts.secondary().line_height() + heading_bottom_gap;
}

float control_height(const Fonts &t_fonts)
{
	return std::max(34.0f, t_fonts.body().line_height() + 12.0f);
}

float title_baseline(Rect t_row, const Fonts &t_fonts)
{
	return t_row.y + label_top_gap + t_fonts.body().ascent();
}

float description_baseline(Rect t_row, const Fonts &t_fonts)
{
	return t_row.y + label_top_gap + t_fonts.body().line_height() + label_line_gap + t_fonts.secondary().ascent();
}

float control_center_y(Rect t_row, const Fonts &t_fonts)
{
	return (title_baseline(t_row, t_fonts) + description_baseline(t_row, t_fonts)) * 0.5f;
}

Rect right_aligned_control(Rect t_row, const Fonts &t_fonts, float t_width, float t_height)
{
	return Rect{t_row.right() - row_padding_x - t_width, control_center_y(t_row, t_fonts) - t_height * 0.5f, t_width,
				t_height};
}

Rect close_button_rect(Rect t_panel, const Fonts &t_fonts)
{
	return Rect{t_panel.right() - 14.0f - close_size, t_panel.y + (header_height(t_fonts) - close_size) * 0.5f,
				close_size, close_size};
}

Rect font_field_rect(Rect t_row, const Fonts &t_fonts)
{
	constexpr float width = 190.0f;
	const float height = control_height(t_fonts);

	return Rect{t_row.right() - row_padding_x - width, t_row.y + (t_row.h - height) * 0.5f, width, height};
}

Rect stepper_rect(Rect t_row, const Fonts &t_fonts)
{
	return right_aligned_control(t_row, t_fonts, 108.0f, 28.0f);
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
	return right_aligned_control(t_row, t_fonts, 40.0f, 22.0f);
}

Rect swatch_rect(Rect t_row, const Fonts &t_fonts)
{
	return right_aligned_control(t_row, t_fonts, 40.0f, 24.0f);
}

Rect master_password_button_rect(Rect t_row, const Fonts &t_fonts)
{
	return right_aligned_control(t_row, t_fonts, 140.0f, control_height(t_fonts));
}

Rect slider_track_rect(Rect t_row, const Fonts &t_fonts)
{
	return right_aligned_control(t_row, t_fonts, slider_track_width, 22.0f);
}

Rect slider_control_rect(Rect t_row, const Fonts &t_fonts)
{
	const Rect track = slider_track_rect(t_row, t_fonts);
	const float readout = slider_readout_gap + slider_readout_width;

	return Rect{track.x - readout, track.y, track.w + readout, track.h};
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

void draw_row_label(DrawList &t_draw_list, const Fonts &t_fonts, Rect t_row, const char *t_title,
					const char *t_description, float t_right_edge, u8 t_alpha)
{
	const float x = t_row.x + row_padding_x;

	draw_text_truncated(t_draw_list, t_fonts.body(), Vec2{x, title_baseline(t_row, t_fonts)}, t_title, t_right_edge - x,
						faded(color_text, t_alpha));
	draw_text_truncated(t_draw_list, t_fonts.secondary(), Vec2{x, description_baseline(t_row, t_fonts)}, t_description,
						t_right_edge - x, faded(color_text_dim, t_alpha));
}

void draw_heading(DrawList &t_draw_list, const Fonts &t_fonts, Rect t_strip, const char *t_title, u8 t_alpha)
{
	constexpr float text_gap = 10.0f;
	constexpr float lead_width = 16.0f;

	const Font &font = t_fonts.secondary();
	const float baseline = t_strip.bottom() - heading_bottom_gap - (font.line_height() - font.ascent());
	const float rule_y = baseline - (font.ascent() + font.descent()) * 0.5f;
	const float lead_x = t_strip.x + row_padding_x;
	const float text_x = lead_x + lead_width + text_gap;
	const float rule_x = text_x + text_width(font, t_title) + text_gap;
	const float rule_right = t_strip.right() - row_padding_x;
	const Color rule_color = faded(color_separator, t_alpha);

	t_draw_list.add_rect(Rect{lead_x, rule_y, lead_width, 1.0f}, rule_color);
	draw_text(t_draw_list, font, Vec2{text_x, baseline}, t_title, faded(color_text, t_alpha));

	if (rule_right > rule_x) {
		t_draw_list.add_rect(Rect{rule_x, rule_y, rule_right - rule_x, 1.0f}, rule_color);
	}
}

void draw_knob(DrawList &t_draw_list, Rect t_knob, Color t_fill, u8 t_alpha)
{
	t_draw_list.add_bordered_rect(t_knob, rounded(t_knob.w * 0.5f), faded(t_fill, t_alpha),
								  faded(outline_on(t_fill), t_alpha), knob_ring);
}

void draw_toggle(DrawList &t_draw_list, Rect t_toggle, float t_on, Color t_accent, u8 t_alpha)
{
	const Color track = mix(color_toggle_off, t_accent, t_on);
	t_draw_list.add_rounded_rect(t_toggle, rounded(t_toggle.h * 0.5f), faded(track, t_alpha));

	const float knob_size = t_toggle.h - 6.0f;
	const Rect knob{t_toggle.x + 3.0f + (t_toggle.w - t_toggle.h) * t_on, t_toggle.y + 3.0f, knob_size, knob_size};
	draw_knob(t_draw_list, knob, foreground_on(track), t_alpha);
}

void draw_slider(DrawList &t_draw_list, const Font &t_font, Rect t_track, float t_fraction, std::string_view t_readout,
				 Color t_accent, u8 t_alpha)
{
	const float center_y = t_track.center().y;
	const float readout_x = t_track.x - slider_readout_gap - text_width(t_font, t_readout);

	draw_text(t_draw_list, t_font, Vec2{readout_x, t_font.centered_baseline(t_track)}, t_readout,
			  faded(color_text, t_alpha));

	const Rect bar{t_track.x, center_y - slider_bar_height * 0.5f, t_track.w, slider_bar_height};
	const Rect filled{bar.x, bar.y, bar.w * std::clamp(t_fraction, 0.0f, 1.0f), bar.h};

	t_draw_list.add_rounded_rect(bar, rounded(slider_bar_height * 0.5f), faded(color_toggle_off, t_alpha));
	if (filled.w > 0.0f) {
		t_draw_list.add_rounded_rect(filled, rounded(slider_bar_height * 0.5f), faded(t_accent, t_alpha));
	}

	const Rect knob{filled.right() - slider_thumb_radius, center_y - slider_thumb_radius, slider_thumb_radius * 2.0f,
					slider_thumb_radius * 2.0f};
	draw_knob(t_draw_list, knob, foreground_on(t_accent), t_alpha);
}

void draw_stepper(DrawList &t_draw_list, const Font &t_font, Rect t_stepper, float t_value, u8 t_alpha)
{
	const Rect minus = stepper_minus(t_stepper);
	const Rect plus = stepper_plus(t_stepper);
	const Color glyph = faded(color_text, t_alpha);

	t_draw_list.add_rounded_rect(minus, rounded(6.0f), faded(color_control, t_alpha));
	t_draw_list.add_rounded_rect(plus, rounded(6.0f), faded(color_control, t_alpha));

	const Vec2 minus_center = minus.center();
	const Vec2 plus_center = plus.center();
	t_draw_list.add_line({minus_center.x - 6.0f, minus_center.y}, {minus_center.x + 6.0f, minus_center.y}, 2.0f, glyph);
	t_draw_list.add_line({plus_center.x - 6.0f, plus_center.y}, {plus_center.x + 6.0f, plus_center.y}, 2.0f, glyph);
	t_draw_list.add_line({plus_center.x, plus_center.y - 6.0f}, {plus_center.x, plus_center.y + 6.0f}, 2.0f, glyph);

	char buffer[8];
	const int written = std::snprintf(buffer, sizeof(buffer), "%d", static_cast<int>(std::lround(t_value)));
	const Rect number{minus.right(), t_stepper.y, plus.x - minus.right(), t_stepper.h};

	draw_text_centered(t_draw_list, t_font, number, std::string_view{buffer, static_cast<usize>(std::max(written, 0))},
					   glyph);
}

void draw_reset_button(DrawList &t_draw_list, const Texture *t_icon, Rect t_button, float t_visible, float t_spin,
					   bool t_hovered, u8 t_alpha)
{
	if (t_visible <= 0.01f) return;

	const auto alpha = static_cast<u8>(t_alpha * t_visible);

	if (t_hovered) {
		t_draw_list.add_rounded_rect(t_button, rounded(7.0f), faded(color_reset_hover, alpha));
	}

	t_draw_list.add_rotated_image(t_button.centered(reset_icon_size, reset_icon_size),
								  -t_spin * 2.0f * std::numbers::pi_v<float>, t_icon,
								  faded(t_hovered ? color_text : color_reset_idle, alpha));
}
}

const SettingsPanel::Toggle SettingsPanel::toggles[toggle_count]{
	{&Settings::show_notifications, &Rows::notifications, "Notifications",
	 "Confirms saves, deletions and resets in the bottom-left corner."},
	{&Settings::animations_enabled, &Rows::animations, "Animations",
	 "Applies animations to popups, scrolling, and the caret."},
	{&Settings::hide_accounts_from_capture, &Rows::hide_from_capture, "Hide From Screen Capture",
	 "Excludes the account list from screenshots and screen sharing."},
	{&Settings::block_overlay_injection, &Rows::block_overlay_injection, "Block Overlay Injection",
	 "Keeps Discord's overlay and hook-based keyloggers out. Disables IMEs; restart to apply."},
	{&Settings::close_to_tray, &Rows::close_to_tray, "Close To Tray",
	 "Closing hides the app to the system tray instead of quitting."},
};

SettingsPanel::SettingsPanel(Settings &t_settings, Fonts &t_fonts, Renderer &t_renderer, const Window &t_window,
							 const Assets &t_assets, CommandQueue &t_commands)
	: m_settings(t_settings)
	, m_fonts(t_fonts)
	, m_renderer(t_renderer)
	, m_window(t_window)
	, m_assets(t_assets)
	, m_commands(t_commands)
{
	sync_with_settings();
}

void SettingsPanel::sync_with_settings()
{
	m_font_name.set_value(m_settings.font_name);

	m_font_size_shown = m_settings.font_size;
	m_secondary_font_size_shown = m_settings.secondary_font_size;
	m_animation_speed_shown = m_settings.animation_speed;
	m_corner_roundness_shown = m_settings.corner_roundness;
	m_accent_shown[0] = m_settings.accent.r;
	m_accent_shown[1] = m_settings.accent.g;
	m_accent_shown[2] = m_settings.accent.b;

	for (u32 i = 0; i < toggle_count; i += 1) {
		m_toggles_shown[i] = m_settings.*toggles[i].value ? 1.0f : 0.0f;
	}
}

void SettingsPanel::open()
{
	m_open = true;
}

void SettingsPanel::close()
{
	m_open = false;
	m_font_name.set_focused(false);
	m_color_picker.close();
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

	const float scale = panel_closed_scale + (1.0f - panel_closed_scale) * m_open_amount;

	Layout result{};
	result.panel = Rect{0.0f, 0.0f, window.x, window.y}.centered(width * scale, height * scale);
	result.inner = result.panel.inset(panel_border);

	Rect remaining = result.inner;
	result.header = remaining.split_top(header_height(m_fonts));
	result.footer = remaining.split_bottom(footer_height(m_fonts));
	result.rows_region = remaining;

	return result;
}

SettingsPanel::Rows SettingsPanel::rows(const Layout &t_layout) const
{
	const float row = row_height(m_fonts);
	const float heading = heading_height(m_fonts);

	Rect cursor{t_layout.rows_region.x, t_layout.rows_region.y - m_rows_scroll.offset(), t_layout.rows_region.w,
				1.0e6f};
	const float top = cursor.y;

	Rows result{};
	result.appearance_heading = cursor.split_top(heading - heading_top_gap * 0.5f);
	result.font = cursor.split_top(row);
	result.font_size = cursor.split_top(row);
	result.secondary_font_size = cursor.split_top(row);
	result.accent = cursor.split_top(row);
	result.corner_roundness = cursor.split_top(row);
	result.notifications = cursor.split_top(row);

	result.motion_heading = cursor.split_top(heading);
	result.animations = cursor.split_top(row);
	result.animation_speed = cursor.split_top(row);

	result.privacy_heading = cursor.split_top(heading);
	result.hide_from_capture = cursor.split_top(row);
	result.block_overlay_injection = cursor.split_top(row);
	result.close_to_tray = cursor.split_top(row);

	result.security_heading = cursor.split_top(heading);
	result.master_password = cursor.split_top(row);

	result.content_height = cursor.y - top;

	return result;
}

ScrollGeometry SettingsPanel::rows_scroll(const Layout &t_layout, const Rows &t_rows) const
{
	const Rect region = t_layout.rows_region;
	const Rect track{region.right() - scrollbar_width - scrollbar_margin, region.y, scrollbar_width, region.h};

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

bool SettingsPanel::is_font_field_hit(const Layout &t_layout, const Rows &t_rows, Vec2 t_point) const
{
	return !m_color_picker.is_open() && hits(t_layout, t_rows.font, font_field_rect(t_rows.font, m_fonts), t_point);
}

Rect SettingsPanel::reset_row(const Rows &t_rows, ResettableSetting t_setting) const
{
	switch (t_setting) {
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
		case ResettableSetting::animations:
			return t_rows.animations;
		case ResettableSetting::animation_speed:
			return t_rows.animation_speed;
		case ResettableSetting::notifications:
			return t_rows.notifications;
		case ResettableSetting::hide_from_capture:
			return t_rows.hide_from_capture;
		case ResettableSetting::block_overlay_injection:
			return t_rows.block_overlay_injection;
		case ResettableSetting::close_to_tray:
			return t_rows.close_to_tray;
		case ResettableSetting::count:
			break;
	}

	return Rect{};
}

Rect SettingsPanel::reset_control(const Rows &t_rows, ResettableSetting t_setting) const
{
	const Rect row = reset_row(t_rows, t_setting);

	switch (t_setting) {
		case ResettableSetting::font:
			return font_field_rect(row, m_fonts);
		case ResettableSetting::font_size:
		case ResettableSetting::secondary_font_size:
			return stepper_rect(row, m_fonts);
		case ResettableSetting::accent:
			return swatch_rect(row, m_fonts);
		case ResettableSetting::corner_roundness:
		case ResettableSetting::animation_speed:
			return slider_control_rect(row, m_fonts);
		case ResettableSetting::animations:
		case ResettableSetting::notifications:
		case ResettableSetting::hide_from_capture:
		case ResettableSetting::block_overlay_injection:
		case ResettableSetting::close_to_tray:
			return toggle_rect(row, m_fonts);
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
		case ResettableSetting::font:
			return std::strcmp(m_settings.font_name, defaults.font_name) == 0 &&
				   m_font_name.value() == defaults.font_name;
		case ResettableSetting::font_size:
			return same(m_settings.font_size, defaults.font_size);
		case ResettableSetting::secondary_font_size:
			return same(m_settings.secondary_font_size, defaults.secondary_font_size);
		case ResettableSetting::accent:
			return std::memcmp(&m_settings.accent, &defaults.accent, sizeof(Color)) == 0;
		case ResettableSetting::corner_roundness:
			return same(m_settings.corner_roundness, defaults.corner_roundness);
		case ResettableSetting::animations:
			return m_settings.animations_enabled == defaults.animations_enabled;
		case ResettableSetting::animation_speed:
			return same(m_settings.animation_speed, defaults.animation_speed);
		case ResettableSetting::notifications:
			return m_settings.show_notifications == defaults.show_notifications;
		case ResettableSetting::hide_from_capture:
			return m_settings.hide_accounts_from_capture == defaults.hide_accounts_from_capture;
		case ResettableSetting::block_overlay_injection:
			return m_settings.block_overlay_injection == defaults.block_overlay_injection;
		case ResettableSetting::close_to_tray:
			return m_settings.close_to_tray == defaults.close_to_tray;
		case ResettableSetting::count:
			break;
	}

	return true;
}

void SettingsPanel::reset(ResettableSetting t_setting)
{
	const Settings &defaults = default_settings;

	switch (t_setting) {
		case ResettableSetting::font:
			m_font_name.set_value(defaults.font_name);
			apply_fonts();
			break;
		case ResettableSetting::font_size:
			m_settings.font_size = defaults.font_size;
			apply_fonts();
			break;
		case ResettableSetting::secondary_font_size:
			m_settings.secondary_font_size = defaults.secondary_font_size;
			apply_fonts();
			break;
		case ResettableSetting::accent:
			m_settings.accent = defaults.accent;
			m_color_picker.close();
			break;
		case ResettableSetting::corner_roundness:
			m_settings.corner_roundness = defaults.corner_roundness;
			break;
		case ResettableSetting::animations:
			m_settings.animations_enabled = defaults.animations_enabled;
			animation::set_enabled(m_settings.animations_enabled);
			break;
		case ResettableSetting::animation_speed:
			m_settings.animation_speed = defaults.animation_speed;
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
		case ResettableSetting::count:
			return;
	}

	m_reset_spin[static_cast<u32>(t_setting)] = 1.0f;
}

void SettingsPanel::apply_fonts()
{
	if (m_fonts.load(m_renderer, m_font_name.value(), m_settings.font_size, m_settings.secondary_font_size,
					 m_window.dpi_scale())) {
		copy_to(m_font_name.value(), m_settings.font_name);
	}
}

void SettingsPanel::apply_animation_speed(Rect t_track, float t_x)
{
	m_settings.animation_speed = value_at((t_x - t_track.x) / t_track.w, animation_speed_min, animation_speed_max);
	animation::set_speed(m_settings.animation_speed);
}

void SettingsPanel::apply_corner_roundness(Rect t_track, float t_x)
{
	m_settings.corner_roundness = value_at((t_x - t_track.x) / t_track.w, corner_roundness_min, corner_roundness_max);
	set_corner_roundness(m_settings.corner_roundness);
}

void SettingsPanel::step_font_size(Rect t_stepper, float &t_value, float t_min, float t_max, Vec2 t_point)
{
	if (stepper_minus(t_stepper).contains(t_point)) {
		t_value = std::max(t_min, t_value - 1.0f);
		apply_fonts();
	} else if (stepper_plus(t_stepper).contains(t_point)) {
		t_value = std::min(t_max, t_value + 1.0f);
		apply_fonts();
	}
}

void SettingsPanel::update(float t_delta_seconds)
{
	m_open_amount = animation::ease_toward(m_open_amount, m_open ? 1.0f : 0.0f, open_ease_rate, t_delta_seconds);

	for (u32 i = 0; i < toggle_count; i += 1) {
		const float target = m_settings.*toggles[i].value ? 1.0f : 0.0f;
		m_toggles_shown[i] = animation::ease_toward(m_toggles_shown[i], target, toggle_ease_rate, t_delta_seconds);
	}

	const auto ease_value = [t_delta_seconds](float t_shown, float t_actual) {
		return animation::ease_toward(t_shown, t_actual, value_ease_rate, t_delta_seconds);
	};

	m_font_size_shown = ease_value(m_font_size_shown, m_settings.font_size);
	m_secondary_font_size_shown = ease_value(m_secondary_font_size_shown, m_settings.secondary_font_size);
	m_animation_speed_shown = m_animation_speed_drag.is_pressed()
								  ? m_settings.animation_speed
								  : ease_value(m_animation_speed_shown, m_settings.animation_speed);
	m_corner_roundness_shown = m_corner_roundness_drag.is_pressed()
								   ? m_settings.corner_roundness
								   : ease_value(m_corner_roundness_shown, m_settings.corner_roundness);

	set_corner_roundness(m_corner_roundness_shown);

	const u8 accent[3]{m_settings.accent.r, m_settings.accent.g, m_settings.accent.b};
	for (u32 channel = 0; channel < 3; channel += 1) {
		m_accent_shown[channel] = ease_value(m_accent_shown[channel], accent[channel]);
	}

	update_reset_buttons(t_delta_seconds);

	m_font_name.update(t_delta_seconds);
	m_rows_scroll.update(t_delta_seconds);
	m_tooltip.update(t_delta_seconds);
}

void SettingsPanel::update_reset_buttons(float t_delta_seconds)
{
	const Layout current = layout();
	const Rows current_rows = rows(current);
	const bool pointer_live = is_blocking() && !m_color_picker.is_open() && current.rows_region.contains(m_mouse);

	for (u32 i = 0; i < resettable_count; i += 1) {
		const auto setting = static_cast<ResettableSetting>(i);
		const bool at_default = is_default(setting);

		m_reset_visible[i] = animation::ease_toward(m_reset_visible[i], is_blocking() && !at_default ? 1.0f : 0.0f,
													reset_appear_rate, t_delta_seconds);
		m_reset_spin[i] = animation::ease_toward(m_reset_spin[i], 0.0f, reset_spin_rate, t_delta_seconds);

		const Rect button = reset_button(current_rows, setting);
		if (pointer_live && !at_default && is_on_screen(current, reset_row(current_rows, setting)) &&
			button.contains(m_mouse)) {
			m_tooltip.request("Restore default setting.", button);
		}
	}
}

bool SettingsPanel::on_pointer_down(Vec2 t_point)
{
	if (!is_blocking()) return false;

	const Layout current = layout();
	const Rows current_rows = rows(current);

	if (m_rows_scroll.on_pointer_down(t_point, rows_scroll(current, current_rows))) return true;

	if (m_color_picker.on_pointer_down(t_point)) {
		m_settings.accent = m_color_picker.color();
		return true;
	}

	const Rect speed_track = slider_track_rect(current_rows.animation_speed, m_fonts);
	if (hits(current, current_rows.animation_speed, speed_track, t_point)) {
		m_animation_speed_drag.begin(t_point);
		apply_animation_speed(speed_track, t_point.x);
		return true;
	}

	const Rect roundness_track = slider_track_rect(current_rows.corner_roundness, m_fonts);
	if (hits(current, current_rows.corner_roundness, roundness_track, t_point)) {
		m_corner_roundness_drag.begin(t_point);
		apply_corner_roundness(roundness_track, t_point.x);
		return true;
	}

	if (is_font_field_hit(current, current_rows, t_point)) {
		m_font_name.set_focused(true);
		m_font_name.on_pointer_down(m_fonts.body(), font_field_rect(current_rows.font, m_fonts), t_point.x);
	}

	return true;
}

bool SettingsPanel::on_pointer_move(Vec2 t_point)
{
	if (!is_blocking()) return false;

	const Layout current = layout();
	const Rows current_rows = rows(current);

	m_rows_scroll.on_pointer_move(t_point.y, rows_scroll(current, current_rows));

	if (m_color_picker.is_dragging()) {
		m_color_picker.on_pointer_move(t_point);
		m_settings.accent = m_color_picker.color();
	}

	if (m_animation_speed_drag.is_pressed()) {
		m_animation_speed_drag.update(t_point);
		apply_animation_speed(slider_track_rect(current_rows.animation_speed, m_fonts), t_point.x);
	}

	if (m_corner_roundness_drag.is_pressed()) {
		m_corner_roundness_drag.update(t_point);
		apply_corner_roundness(slider_track_rect(current_rows.corner_roundness, m_fonts), t_point.x);
	}

	if (m_font_name.is_selecting()) {
		m_font_name.on_pointer_move(m_fonts.body(), font_field_rect(current_rows.font, m_fonts), t_point.x);
	}

	return true;
}

bool SettingsPanel::on_pointer_up(Vec2 t_point)
{
	if (!is_blocking()) return false;

	const bool ended_drag = m_rows_scroll.is_dragging() || m_color_picker.is_dragging() ||
							m_animation_speed_drag.is_pressed() || m_corner_roundness_drag.is_pressed() ||
							m_font_name.is_selecting();

	m_rows_scroll.on_pointer_up();
	m_color_picker.on_pointer_up();
	m_animation_speed_drag.end();
	m_corner_roundness_drag.end();
	m_font_name.on_pointer_up();

	if (!ended_drag) {
		handle_click(t_point);
	}

	return true;
}

void SettingsPanel::handle_click(Vec2 t_point)
{
	const Layout current = layout();
	const Rect close_button = close_button_rect(current.panel, m_fonts);

	if (close_button.contains(t_point) || !current.panel.contains(t_point)) {
		close();
		return;
	}

	if (!current.rows_region.contains(t_point)) return;

	const Rows current_rows = rows(current);

	for (u32 i = 0; i < resettable_count && !m_color_picker.is_open(); i += 1) {
		const auto setting = static_cast<ResettableSetting>(i);

		if (!is_default(setting) &&
			hits(current, reset_row(current_rows, setting), reset_button(current_rows, setting), t_point)) {
			reset(setting);
			return;
		}
	}

	m_font_name.set_focused(false);

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
			m_color_picker.open(m_settings.accent, swatch, m_window.size());
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

	const Layout current = layout();
	const Rows current_rows = rows(current);

	if (is_font_field_hit(current, current_rows, t_point)) {
		m_font_name.set_focused(true);
		m_font_name.on_right_click(m_fonts.body(), font_field_rect(current_rows.font, m_fonts), t_point.x);
		m_commands.push(Command{.type = CommandType::show_text_menu, .position = t_point, .text_input = &m_font_name});
	}

	return true;
}

bool SettingsPanel::on_scroll(Vec2, float t_wheel_delta)
{
	if (!is_blocking()) return false;

	const Layout current = layout();
	m_rows_scroll.on_scroll(t_wheel_delta, rows_scroll(current, rows(current)));

	return true;
}

bool SettingsPanel::on_key_down(u32 t_key)
{
	if (!is_blocking()) return false;

	if (t_key == VK_ESCAPE) {
		close();
	} else if (m_font_name.is_focused() && t_key == VK_RETURN) {
		apply_fonts();
	} else {
		m_font_name.on_key_down(t_key);
	}

	return true;
}

bool SettingsPanel::on_char(u32 t_character)
{
	if (!is_blocking()) return false;

	m_font_name.on_char(t_character);

	return true;
}

CursorKind SettingsPanel::cursor() const
{
	if (!is_blocking()) return CursorKind::arrow;

	const bool dragging = m_rows_scroll.is_dragging() || m_color_picker.is_dragging() ||
						  m_animation_speed_drag.is_pressed() || m_corner_roundness_drag.is_pressed();
	if (dragging) return CursorKind::drag;
	if (m_font_name.is_selecting()) return CursorKind::ibeam;

	if (m_color_picker.is_open()) {
		const CursorKind picker = m_color_picker.cursor(m_mouse);
		if (picker != CursorKind::arrow) return picker;
	}

	const Layout current = layout();
	const Rect close_button = close_button_rect(current.panel, m_fonts);

	if (close_button.contains(m_mouse)) return CursorKind::hand;
	if (!current.rows_region.contains(m_mouse)) return CursorKind::arrow;

	const Rows current_rows = rows(current);

	for (u32 i = 0; i < resettable_count; i += 1) {
		const auto setting = static_cast<ResettableSetting>(i);

		if (!is_default(setting) &&
			hits(current, reset_row(current_rows, setting), reset_button(current_rows, setting), m_mouse)) {
			return CursorKind::hand;
		}
	}

	if (is_font_field_hit(current, current_rows, m_mouse)) return CursorKind::ibeam;

	const Rect font_size = stepper_rect(current_rows.font_size, m_fonts);
	const Rect secondary_size = stepper_rect(current_rows.secondary_font_size, m_fonts);

	const struct {
		Rect row;
		Rect control;
	} clickable[]{
		{current_rows.font_size, stepper_minus(font_size)},
		{current_rows.font_size, stepper_plus(font_size)},
		{current_rows.secondary_font_size, stepper_minus(secondary_size)},
		{current_rows.secondary_font_size, stepper_plus(secondary_size)},
		{current_rows.animation_speed, slider_track_rect(current_rows.animation_speed, m_fonts)},
		{current_rows.corner_roundness, slider_track_rect(current_rows.corner_roundness, m_fonts)},
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
	const Font &body = m_fonts.body();
	const Font &secondary = m_fonts.secondary();
	const Vec2 window = m_window.size();

	t_draw_list.add_rect(Rect{0.0f, 0.0f, window.x, window.y}, Color{0, 0, 0, static_cast<u8>(140.0f * m_open_amount)});
	t_draw_list.add_bordered_rect(t_layout.panel, rounded(panel_radius), faded(color_background, t_alpha),
								  faded(color_border, t_alpha), panel_border);

	draw_text(t_draw_list, body, Vec2{t_layout.header.x + row_padding_x, body.centered_baseline(t_layout.header)},
			  "Settings", faded(color_text, t_alpha));

	const Rect close_button = close_button_rect(t_layout.panel, m_fonts);
	controls::draw_x(t_draw_list, close_button,
					 faded(close_button.contains(m_mouse) ? color_text : color_text_dim, t_alpha));

	const float rule_width = t_layout.header.w - row_padding_x * 2.0f;
	t_draw_list.add_rect(Rect{t_layout.header.x + row_padding_x, t_layout.header.bottom(), rule_width, 1.0f},
						 faded(color_separator, t_alpha));
	t_draw_list.add_rect(Rect{t_layout.footer.x + row_padding_x, t_layout.footer.y, rule_width, 1.0f},
						 faded(color_separator, t_alpha));

	draw_text(t_draw_list, secondary,
			  Vec2{t_layout.footer.x + row_padding_x, secondary.centered_baseline(t_layout.footer)},
			  "Escape to dismiss", faded(color_text_dim, t_alpha));
}

void SettingsPanel::draw_row_highlight(DrawList &t_draw_list, const Layout &t_layout, const Rows &t_rows,
									   u8 t_alpha) const
{
	if (!is_blocking() || m_color_picker.is_open() || !t_layout.rows_region.contains(m_mouse)) return;

	const Rect hoverable[]{
		t_rows.font,
		t_rows.font_size,
		t_rows.secondary_font_size,
		t_rows.accent,
		t_rows.corner_roundness,
		t_rows.notifications,
		t_rows.animations,
		t_rows.animation_speed,
		t_rows.hide_from_capture,
		t_rows.block_overlay_injection,
		t_rows.close_to_tray,
		t_rows.master_password,
	};

	for (const Rect &row : hoverable) {
		const Rect highlight = row.inset(highlight_inset_x, 0.0f);
		if (!is_on_screen(t_layout, row) || !highlight.contains(m_mouse)) continue;

		t_draw_list.add_rounded_rect(highlight, rounded(highlight_radius), faded(color_row_hover, t_alpha));
		return;
	}
}

void SettingsPanel::draw_headings(DrawList &t_draw_list, const Layout &t_layout, const Rows &t_rows, u8 t_alpha) const
{
	const struct {
		Rect strip;
		const char *title;
	} headings[]{
		{t_rows.appearance_heading, "Appearance"},
		{t_rows.motion_heading, "Motion"},
		{t_rows.privacy_heading, "Privacy"},
		{t_rows.security_heading, "Security"},
	};

	for (const auto &heading : headings) {
		if (is_on_screen(t_layout, heading.strip)) {
			draw_heading(t_draw_list, m_fonts, heading.strip, heading.title, t_alpha);
		}
	}
}

void SettingsPanel::draw_appearance(DrawList &t_draw_list, const Layout &t_layout, const Rows &t_rows, u8 t_alpha)
{
	const Font &body = m_fonts.body();
	const Color accent = m_settings.accent;

	if (is_on_screen(t_layout, t_rows.font)) {
		const Rect field = font_field_rect(t_rows.font, m_fonts);
		const bool focused = m_font_name.is_focused();

		draw_row_label(t_draw_list, m_fonts, t_rows.font, "Font", "The system font file applied to the UI.",
					   label_right_edge(field, t_rows.font, m_fonts), t_alpha);
		t_draw_list.add_bordered_rect(field, rounded(6.0f),
									  faded(focused ? mix(color_background, accent, 0.25f) : color_background, t_alpha),
									  faded(focused ? accent : color_control, t_alpha), 1.5f);
		m_font_name.draw(t_draw_list, body, field, faded(color_text, t_alpha), faded(accent, t_alpha));
	}

	if (is_on_screen(t_layout, t_rows.font_size)) {
		const Rect stepper = stepper_rect(t_rows.font_size, m_fonts);
		draw_row_label(t_draw_list, m_fonts, t_rows.font_size, "Font Size", "Determines the scale of the whole UI.",
					   label_right_edge(stepper, t_rows.font_size, m_fonts), t_alpha);
		draw_stepper(t_draw_list, body, stepper, m_font_size_shown, t_alpha);
	}

	if (is_on_screen(t_layout, t_rows.secondary_font_size)) {
		const Rect stepper = stepper_rect(t_rows.secondary_font_size, m_fonts);
		draw_row_label(t_draw_list, m_fonts, t_rows.secondary_font_size, "Secondary Font Size",
					   "Scale of labels/hints and other small text.",
					   label_right_edge(stepper, t_rows.secondary_font_size, m_fonts), t_alpha);
		draw_stepper(t_draw_list, body, stepper, m_secondary_font_size_shown, t_alpha);
	}

	if (is_on_screen(t_layout, t_rows.accent)) {
		const Rect swatch = swatch_rect(t_rows.accent, m_fonts);
		const Color shown{static_cast<u8>(std::lround(m_accent_shown[0])),
						  static_cast<u8>(std::lround(m_accent_shown[1])),
						  static_cast<u8>(std::lround(m_accent_shown[2])), accent.a};

		draw_row_label(t_draw_list, m_fonts, t_rows.accent, "Accent Color",
					   "Click to pick the selection/button accent color.",
					   label_right_edge(swatch, t_rows.accent, m_fonts), t_alpha);
		t_draw_list.add_rounded_rect(swatch, rounded(6.0f), faded(shown, t_alpha));
	}
}

void SettingsPanel::draw_sliders(DrawList &t_draw_list, const Layout &t_layout, const Rows &t_rows, u8 t_alpha) const
{
	const Font &body = m_fonts.body();
	char readout[8];

	if (is_on_screen(t_layout, t_rows.corner_roundness)) {
		const Rect row = t_rows.corner_roundness;
		const int written = std::snprintf(readout, sizeof(readout), "%.0f%%", m_corner_roundness_shown * 100.0f);

		draw_row_label(t_draw_list, m_fonts, row, "Corner Roundness", "How round those corners actually are.",
					   label_right_edge(slider_control_rect(row, m_fonts), row, m_fonts), t_alpha);
		draw_slider(t_draw_list, body, slider_track_rect(row, m_fonts),
					fraction_in(m_corner_roundness_shown, corner_roundness_min, corner_roundness_max),
					std::string_view{readout, static_cast<usize>(std::max(written, 0))}, m_settings.accent, t_alpha);
	}

	if (is_on_screen(t_layout, t_rows.animation_speed)) {
		const Rect row = t_rows.animation_speed;
		const int written = std::snprintf(readout, sizeof(readout), "%.2fx", m_animation_speed_shown);

		draw_row_label(t_draw_list, m_fonts, row, "Animation Speed", "How fast popups, scrolling, and toggles animate.",
					   label_right_edge(slider_control_rect(row, m_fonts), row, m_fonts), t_alpha);
		draw_slider(t_draw_list, body, slider_track_rect(row, m_fonts),
					fraction_in(m_animation_speed_shown, animation_speed_min, animation_speed_max),
					std::string_view{readout, static_cast<usize>(std::max(written, 0))}, m_settings.accent, t_alpha);
	}
}

void SettingsPanel::draw_toggles(DrawList &t_draw_list, const Layout &t_layout, const Rows &t_rows, u8 t_alpha) const
{
	for (u32 i = 0; i < toggle_count; i += 1) {
		const Toggle &toggle = toggles[i];
		const Rect row = t_rows.*toggle.row;
		if (!is_on_screen(t_layout, row)) continue;

		const Rect control = toggle_rect(row, m_fonts);
		draw_row_label(t_draw_list, m_fonts, row, toggle.title, toggle.description,
					   label_right_edge(control, row, m_fonts), t_alpha);
		draw_toggle(t_draw_list, control, m_toggles_shown[i], m_settings.accent, t_alpha);
	}
}

void SettingsPanel::draw_master_password(DrawList &t_draw_list, const Layout &t_layout, const Rows &t_rows,
										 u8 t_alpha) const
{
	const Rect row = t_rows.master_password;
	if (!is_on_screen(t_layout, row)) return;

	const Rect button = master_password_button_rect(row, m_fonts);
	draw_row_label(t_draw_list, m_fonts, row, "Master Password", "Encrypts your saved account passwords.",
				   label_right_edge(button, row, m_fonts), t_alpha);

	const Color fill = button.contains(m_mouse) ? lightened(color_control, 10) : color_control;
	t_draw_list.add_rounded_rect(button, rounded(6.0f), faded(fill, t_alpha));
	draw_text_centered(t_draw_list, m_fonts.body(), button, "Reset Password", faded(color_text, t_alpha));
}

void SettingsPanel::draw_reset_buttons(DrawList &t_draw_list, const Layout &t_layout, const Rows &t_rows,
									   u8 t_alpha) const
{
	const bool pointer_live = !m_color_picker.is_open() && t_layout.rows_region.contains(m_mouse);

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

	draw_chrome(t_draw_list, current, alpha);

	t_draw_list.push_clip(current.rows_region);
	draw_row_highlight(t_draw_list, current, current_rows, alpha);
	draw_headings(t_draw_list, current, current_rows, alpha);
	draw_appearance(t_draw_list, current, current_rows, alpha);
	draw_sliders(t_draw_list, current, current_rows, alpha);
	draw_toggles(t_draw_list, current, current_rows, alpha);
	draw_master_password(t_draw_list, current, current_rows, alpha);
	draw_reset_buttons(t_draw_list, current, current_rows, alpha);
	t_draw_list.pop_clip();

	m_rows_scroll.draw_edge_fade(t_draw_list, current.rows_region, scroll, faded(color_background, alpha));
	m_rows_scroll.draw(t_draw_list, scroll, faded(color_scroll_thumb, alpha), m_mouse);

	if (is_on_screen(current, current_rows.accent)) {
		m_color_picker.draw(t_draw_list);
	}

	m_tooltip.draw(t_draw_list, m_fonts, current.panel, alpha);
}
