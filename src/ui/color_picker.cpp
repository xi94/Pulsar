#include "ui/color_picker.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <span>
#include <utility>

#include <Windows.h>

#include "core/animation.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "core/str.h"
#include "platform/clipboard.h"
#include "ui/controls.h"
#include "ui/text.h"
#include "ui/theme.h"

namespace {
constexpr float popup_padding = 12.0f;
constexpr float popup_radius = 10.0f;
constexpr float max_square_size = 200.0f;
constexpr float min_square_size = 120.0f;
constexpr float strip_gap = 10.0f;
constexpr float alpha_strip_width = 20.0f;
constexpr float hue_strip_height = 16.0f;
constexpr float handle_radius = 6.0f;
constexpr float bounds_margin = 8.0f;
constexpr float anchor_gap = 8.0f;

constexpr float section_gap = 14.0f;
constexpr float row_gap = 8.0f;
constexpr float control_gap = 6.0f;
constexpr float swatch_width = 40.0f;
constexpr float control_radius = 6.0f;
constexpr float label_inset = 9.0f;
constexpr float label_text_overlap = 4.0f;
constexpr float glyph_size = 14.0f;
constexpr float feedback_seconds = 1.2f;
constexpr u32 hex_max_length = 24;
constexpr u32 channel_max_length = 3;

constexpr Color color_marker{255, 255, 255, 255};

constexpr Color hue_stops[]{
	{255, 0, 0, 255}, {255, 255, 0, 255}, {0, 255, 0, 255}, {0, 255, 255, 255},
	{0, 0, 255, 255}, {255, 0, 255, 255}, {255, 0, 0, 255},
};

constexpr std::string_view field_labels[]{"#", "R", "G", "B"};

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
		return static_cast<u8>(std::clamp((t_channel + lift) * 255.0f + 0.5f, 0.0f, 255.0f));
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

float row_height(const Fonts &t_fonts)
{
	return std::max(28.0f, t_fonts.secondary().line_height() + 10.0f);
}

float fraction_along(float t_position, float t_start, float t_length)
{
	return std::clamp((t_position - t_start) / t_length, 0.0f, 1.0f);
}

std::string_view trimmed(std::string_view t_text)
{
	while (!t_text.empty() && std::isspace(static_cast<unsigned char>(t_text.front()))) {
		t_text.remove_prefix(1);
	}

	while (!t_text.empty() && std::isspace(static_cast<unsigned char>(t_text.back()))) {
		t_text.remove_suffix(1);
	}

	return t_text;
}

std::optional<Color> parse_hex(std::string_view t_digits, u8 t_alpha)
{
	if (t_digits.size() != 3 && t_digits.size() != 6 && t_digits.size() != 8) return std::nullopt;

	u8 nibbles[8]{};
	for (usize i = 0; i < t_digits.size(); i += 1) {
		const auto digit = static_cast<unsigned char>(t_digits[i]);
		if (!std::isxdigit(digit)) return std::nullopt;

		nibbles[i] = static_cast<u8>(std::isdigit(digit) ? digit - '0' : std::tolower(digit) - 'a' + 10);
	}

	if (t_digits.size() == 3) {
		return Color{static_cast<u8>(nibbles[0] * 17), static_cast<u8>(nibbles[1] * 17),
					 static_cast<u8>(nibbles[2] * 17), t_alpha};
	}

	const auto byte = [&nibbles](usize t_index) {
		return static_cast<u8>(nibbles[t_index] * 16 + nibbles[t_index + 1]);
	};

	return Color{byte(0), byte(2), byte(4), t_digits.size() == 8 ? byte(6) : t_alpha};
}

std::optional<Color> parse_channels(std::string_view t_text, u8 t_alpha)
{
	u32 channels[4]{};
	u32 count = 0;

	for (usize i = 0; i < t_text.size() && count < 4;) {
		if (!std::isdigit(static_cast<unsigned char>(t_text[i]))) {
			i += 1;
			continue;
		}

		u32 value = 0;
		while (i < t_text.size() && std::isdigit(static_cast<unsigned char>(t_text[i]))) {
			value = std::min(value * 10 + static_cast<u32>(t_text[i] - '0'), 1000u);
			i += 1;
		}

		if (value > 255) return std::nullopt;

		channels[count] = value;
		count += 1;
	}

	if (count < 3) return std::nullopt;

	return Color{static_cast<u8>(channels[0]), static_cast<u8>(channels[1]), static_cast<u8>(channels[2]),
				 count == 4 ? static_cast<u8>(channels[3]) : t_alpha};
}

std::optional<Color> parse_color(std::string_view t_text, u8 t_alpha)
{
	std::string_view text = trimmed(t_text);
	if (text.starts_with('#')) {
		text.remove_prefix(1);
	}

	if (const std::optional<Color> hex = parse_hex(text, t_alpha)) return hex;

	return parse_channels(text, t_alpha);
}

bool is_short_hex(std::string_view t_text)
{
	std::string_view text = trimmed(t_text);
	if (text.starts_with('#')) {
		text.remove_prefix(1);
	}

	return text.size() == 3 &&
		   std::ranges::all_of(text, [](char t_c) { return std::isxdigit(static_cast<unsigned char>(t_c)) != 0; });
}

std::string_view format_hex(Color t_color, char (&t_buffer)[12])
{
	const int written =
		t_color.a == 255
			? std::snprintf(t_buffer, sizeof(t_buffer), "%02X%02X%02X", t_color.r, t_color.g, t_color.b)
			: std::snprintf(t_buffer, sizeof(t_buffer), "%02X%02X%02X%02X", t_color.r, t_color.g, t_color.b, t_color.a);

	return std::string_view{t_buffer, static_cast<usize>(std::max(written, 0))};
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

void draw_copy_glyph(DrawList &t_draw_list, Rect t_rect, Color t_backdrop, Color t_color)
{
	const float size = t_rect.w * 0.68f;
	const Rect back{t_rect.x, t_rect.y, size, size};
	const Rect front{t_rect.right() - size, t_rect.bottom() - size, size, size};

	t_draw_list.add_bordered_rect(back, rounded(2.5f), t_backdrop, t_color, 1.5f);
	t_draw_list.add_bordered_rect(front, rounded(2.5f), t_backdrop, t_color, 1.5f);
}

void draw_paste_glyph(DrawList &t_draw_list, Rect t_rect, Color t_backdrop, Color t_color)
{
	const Rect board{t_rect.x + t_rect.w * 0.12f, t_rect.y + t_rect.h * 0.12f, t_rect.w * 0.76f, t_rect.h * 0.88f};
	const Rect clip{t_rect.center().x - t_rect.w * 0.2f, t_rect.y, t_rect.w * 0.4f, t_rect.h * 0.26f};

	t_draw_list.add_bordered_rect(board, rounded(2.5f), t_backdrop, t_color, 1.5f);
	t_draw_list.add_rounded_rect(clip, rounded(1.5f), t_color);
}
}

ColorPicker::ColorPicker(const Fonts &t_fonts, const Assets &t_assets)
	: m_fonts(t_fonts)
	, m_assets(t_assets)
{
	m_fields[hex_field].set_max_length(hex_max_length);

	for (u32 i = 1; i < field_count; i += 1) {
		m_fields[i].set_max_length(channel_max_length);
	}
}

ColorPicker::Layout ColorPicker::layout() const
{
	const float row = row_height(m_fonts);
	const float fixed_height = popup_padding * 2.0f + strip_gap + hue_strip_height + section_gap + row + row_gap + row;
	const float room = m_bounds.h - bounds_margin * 2.0f;
	const float square = std::clamp(room - fixed_height, min_square_size, max_square_size);
	const float content_width = square + strip_gap + alpha_strip_width;
	const float width = popup_padding * 2.0f + content_width;
	const float height = fixed_height + square;

	const float top_limit = m_bounds.y + bounds_margin;
	const float bottom_limit = m_bounds.bottom() - bounds_margin;
	const float left_limit = m_bounds.x + bounds_margin;
	const float right_limit = m_bounds.right() - bounds_margin;

	Vec2 origin{m_anchor.right() - width, m_anchor.bottom() + anchor_gap};

	if (origin.y + height > bottom_limit) {
		origin.y = m_anchor.y - anchor_gap - height;

		if (origin.y < top_limit) {
			origin = Vec2{m_anchor.x - anchor_gap - width, m_anchor.center().y - height * 0.5f};
		}
	}

	origin.x = std::clamp(origin.x, left_limit, std::max(left_limit, right_limit - width));
	origin.y = std::clamp(origin.y, top_limit, std::max(top_limit, bottom_limit - height));

	Layout result{};
	result.popup = Rect{origin.x, origin.y, width, height};

	const float left = result.popup.x + popup_padding;
	float top = result.popup.y + popup_padding;

	result.square = Rect{left, top, square, square};
	result.alpha = Rect{result.square.right() + strip_gap, top, alpha_strip_width, square};
	result.hue = Rect{left, result.square.bottom() + strip_gap, square, hue_strip_height};

	top = result.hue.bottom() + section_gap;
	result.swatch = Rect{left, top, swatch_width, row};
	result.paste = Rect{left + content_width - row, top, row, row};
	result.copy = Rect{result.paste.x - control_gap - row, top, row, row};

	const float hex_x = result.swatch.right() + control_gap;
	result.fields[hex_field] = Rect{hex_x, top, result.copy.x - control_gap - hex_x, row};

	top += row + row_gap;
	const float channel_width = (content_width - control_gap * (channel_count - 1)) / channel_count;
	for (u32 i = 0; i < channel_count; i += 1) {
		result.fields[1 + i] = Rect{left + i * (channel_width + control_gap), top, channel_width, row};
	}

	return result;
}

Rect ColorPicker::field_text_rect(const Layout &t_layout, u32 t_field) const
{
	const Rect field = t_layout.fields[t_field];
	const float text_x =
		field.x + label_inset + text_width(m_fonts.secondary(), field_labels[t_field]) - label_text_overlap;

	return Rect{text_x, field.y, field.right() - text_x, field.h};
}

i32 ColorPicker::field_at(const Layout &t_layout, Vec2 t_point) const
{
	for (u32 i = 0; i < field_count; i += 1) {
		if (t_layout.fields[i].contains(t_point)) return static_cast<i32>(i);
	}

	return -1;
}

i32 ColorPicker::focused_field() const
{
	for (u32 i = 0; i < field_count; i += 1) {
		if (m_fields[i].is_focused()) return static_cast<i32>(i);
	}

	return -1;
}

bool ColorPicker::contains(Vec2 t_point) const
{
	return m_open && layout().popup.contains(t_point);
}

bool ColorPicker::take_changed()
{
	return std::exchange(m_changed, false);
}

void ColorPicker::set_color(Color t_color)
{
	const Hsv hsv = rgb_to_hsv(t_color);

	if (hsv.saturation > 0.0001f && hsv.value > 0.0001f) {
		m_hue = hsv.hue;
	}

	m_saturation = hsv.saturation;
	m_value = hsv.value;
	m_color = t_color;
	m_changed = true;
}

void ColorPicker::apply_hsv()
{
	m_color = with_alpha(hsv_to_rgb(m_hue, m_saturation, m_value), m_color.a);
	m_changed = true;
	sync_fields(-1);
}

void ColorPicker::sync_fields(i32 t_skipped_field)
{
	char hex[12];
	char channel[4];
	const u8 channels[channel_count]{m_color.r, m_color.g, m_color.b};

	for (u32 i = 0; i < field_count; i += 1) {
		if (static_cast<i32>(i) == t_skipped_field) continue;

		std::string_view text;
		if (i == hex_field) {
			text = format_hex(m_color, hex);
		} else {
			const int written = std::snprintf(channel, sizeof(channel), "%u", channels[i - 1]);
			text = std::string_view{channel, static_cast<usize>(std::max(written, 0))};
		}

		m_fields[i].set_value(text);
		copy_to(text, m_synced_text[i]);
	}
}

void ColorPicker::read_field(u32 t_field)
{
	const std::string_view text = m_fields[t_field].value();
	copy_to(text, m_synced_text[t_field]);

	if (t_field == hex_field) {
		const std::optional<Color> parsed = parse_color(text, m_color.a);
		if (parsed && !is_short_hex(text)) {
			set_color(*parsed);
			sync_fields(static_cast<i32>(hex_field));
		}

		return;
	}

	if (text.empty() || text.size() > channel_max_length ||
		!std::ranges::all_of(text, [](char t_c) { return std::isdigit(static_cast<unsigned char>(t_c)) != 0; })) {
		return;
	}

	u32 value = 0;
	for (const char digit : text) {
		value = value * 10 + static_cast<u32>(digit - '0');
	}

	Color next = m_color;
	u8 *channels[channel_count]{&next.r, &next.g, &next.b};
	*channels[t_field - 1] = static_cast<u8>(std::min(value, 255u));

	set_color(next);
	sync_fields(static_cast<i32>(t_field));
}

void ColorPicker::focus_field(i32 t_field)
{
	const i32 previous = focused_field();

	if (previous == static_cast<i32>(hex_field) && previous != t_field) {
		const std::string_view text = m_fields[hex_field].value();
		const std::optional<Color> parsed = parse_color(text, m_color.a);

		if (parsed && is_short_hex(text)) {
			set_color(*parsed);
		}
	}

	for (u32 i = 0; i < field_count; i += 1) {
		m_fields[i].set_focused(static_cast<i32>(i) == t_field);
	}

	if (previous >= 0 && previous != t_field) {
		sync_fields(t_field);
	}
}

void ColorPicker::copy_hex()
{
	char hex[12];
	char text[16];
	const std::string_view digits = format_hex(m_color, hex);
	const int written = std::snprintf(text, sizeof(text), "#%.*s", static_cast<int>(digits.size()), digits.data());

	set_clipboard_text(std::string_view{text, static_cast<usize>(std::max(written, 0))});
	m_copied_seconds = feedback_seconds;
	m_paste_failed_seconds = 0.0f;
}

void ColorPicker::paste_color()
{
	const std::optional<Color> parsed = parse_color(clipboard_text(), m_color.a);

	if (!parsed) {
		m_paste_failed_seconds = feedback_seconds;
		m_copied_seconds = 0.0f;
		return;
	}

	set_color(*parsed);
	focus_field(-1);
	sync_fields(-1);
}

void ColorPicker::end_drags()
{
	m_saturation_value_drag.end();
	m_hue_drag.end();
	m_alpha_drag.end();
}

void ColorPicker::open(Color t_initial, Rect t_anchor, Rect t_bounds)
{
	const Hsv hsv = rgb_to_hsv(t_initial);
	m_hue = hsv.hue;
	m_saturation = hsv.saturation;
	m_value = hsv.value;
	m_color = t_initial;
	m_initial = t_initial;
	m_changed = false;

	m_anchor = t_anchor;
	m_bounds = t_bounds;
	m_open = true;
	m_press = Press::none;
	m_copied_seconds = 0.0f;
	m_paste_failed_seconds = 0.0f;

	end_drags();
	focus_field(-1);
	sync_fields(-1);
}

void ColorPicker::close()
{
	focus_field(-1);
	m_open = false;
	m_press = Press::none;
	end_drags();
}

void ColorPicker::update(float t_delta_seconds)
{
	m_copied_seconds = std::max(0.0f, m_copied_seconds - t_delta_seconds);
	m_paste_failed_seconds = std::max(0.0f, m_paste_failed_seconds - t_delta_seconds);

	if (m_copied_seconds > 0.0f || m_paste_failed_seconds > 0.0f) {
		animation::request_frame_after(std::max(m_copied_seconds, m_paste_failed_seconds));
	}

	if (!m_open) return;

	for (u32 i = 0; i < field_count; i += 1) {
		m_fields[i].update(t_delta_seconds);

		if (m_fields[i].value() != std::string_view{m_synced_text[i]}) {
			read_field(i);
		}
	}
}

bool ColorPicker::on_pointer_down(Vec2 t_point)
{
	if (!m_open) return false;

	const Layout current = layout();
	if (!current.popup.contains(t_point)) return false;

	m_press = Press::none;
	const i32 field = field_at(current, t_point);

	if (field >= 0) {
		focus_field(field);
		m_fields[field].on_pointer_down(m_fonts.secondary(), field_text_rect(current, static_cast<u32>(field)),
										t_point.x);
		m_press = Press::field;
		return true;
	}

	focus_field(-1);

	if (current.square.contains(t_point)) {
		m_saturation_value_drag.begin(t_point);
	} else if (current.hue.contains(t_point)) {
		m_hue_drag.begin(t_point);
	} else if (current.alpha.contains(t_point)) {
		m_alpha_drag.begin(t_point);
	} else if (current.swatch.contains(t_point) && t_point.x < current.swatch.center().x) {
		m_press = Press::revert;
	} else if (current.copy.contains(t_point)) {
		m_press = Press::copy;
	} else if (current.paste.contains(t_point)) {
		m_press = Press::paste;
	}

	on_pointer_move(t_point);

	return true;
}

void ColorPicker::on_pointer_move(Vec2 t_point)
{
	if (!m_open) return;

	const Layout current = layout();

	for (u32 i = 0; i < field_count; i += 1) {
		if (m_fields[i].is_selecting()) {
			m_fields[i].on_pointer_move(m_fonts.secondary(), field_text_rect(current, i), t_point.x);
		}
	}

	if (m_saturation_value_drag.is_pressed()) {
		m_saturation_value_drag.update(t_point);
		m_saturation = fraction_along(t_point.x, current.square.x, current.square.w);
		m_value = 1.0f - fraction_along(t_point.y, current.square.y, current.square.h);
		apply_hsv();
	}

	if (m_hue_drag.is_pressed()) {
		m_hue_drag.update(t_point);
		m_hue = fraction_along(t_point.x, current.hue.x, current.hue.w) * 359.9f;
		apply_hsv();
	}

	if (m_alpha_drag.is_pressed()) {
		m_alpha_drag.update(t_point);
		m_color.a = static_cast<u8>((1.0f - fraction_along(t_point.y, current.alpha.y, current.alpha.h)) * 255.0f);
		m_changed = true;
		sync_fields(-1);
	}
}

bool ColorPicker::on_pointer_up(Vec2 t_point)
{
	const bool was_pressed = m_press != Press::none || is_dragging();
	const Press press = std::exchange(m_press, Press::none);

	for (TextInput &field : m_fields) {
		field.on_pointer_up();
	}

	end_drags();

	if (!m_open || !was_pressed) return false;

	const Layout current = layout();

	switch (press) {
		case Press::revert:
			if (current.swatch.contains(t_point) && t_point.x < current.swatch.center().x) {
				set_color(m_initial);
				sync_fields(-1);
			}
			break;

		case Press::copy:
			if (current.copy.contains(t_point)) {
				copy_hex();
			}
			break;

		case Press::paste:
			if (current.paste.contains(t_point)) {
				paste_color();
			}
			break;

		case Press::none:
		case Press::field:
			break;
	}

	return true;
}

TextInput *ColorPicker::on_right_click(Vec2 t_point)
{
	if (!m_open) return nullptr;

	const Layout current = layout();
	const i32 field = field_at(current, t_point);
	if (field < 0) return nullptr;

	focus_field(field);
	m_fields[field].on_right_click(m_fonts.secondary(), field_text_rect(current, static_cast<u32>(field)), t_point.x);

	return &m_fields[field];
}

bool ColorPicker::on_key_down(u32 t_key)
{
	if (!m_open) return false;

	const i32 focused = focused_field();
	const bool control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

	if (focused < 0) {
		if (t_key == VK_ESCAPE) {
			close();
			return true;
		}

		if (control && t_key == 'C') {
			copy_hex();
			return true;
		}

		if (control && t_key == 'V') {
			paste_color();
			return true;
		}

		return false;
	}

	switch (t_key) {
		case VK_RETURN:
		case VK_ESCAPE:
			focus_field(-1);
			return true;

		case VK_TAB: {
			const auto count = static_cast<i32>(field_count);
			const i32 step = (GetKeyState(VK_SHIFT) & 0x8000) != 0 ? count - 1 : 1;
			const i32 next = (focused + step) % count;

			focus_field(next);
			m_fields[next].apply(TextEdit::select_all);
			return true;
		}

		default:
			m_fields[focused].on_key_down(t_key);
			return true;
	}
}

bool ColorPicker::on_char(u32 t_character)
{
	const i32 focused = focused_field();
	if (!m_open || focused < 0) return false;

	const bool digit = t_character >= '0' && t_character <= '9';
	const bool hex_digit =
		digit || (t_character >= 'a' && t_character <= 'f') || (t_character >= 'A' && t_character <= 'F');
	const bool accepted = focused == static_cast<i32>(hex_field) ? hex_digit || t_character == '#' : digit;

	if (accepted) {
		m_fields[focused].on_char(t_character);
	}

	return true;
}

std::optional<ColorPickerHint> ColorPicker::hint(Vec2 t_mouse) const
{
	if (!m_open || is_dragging()) return std::nullopt;

	const Layout current = layout();

	if (current.copy.contains(t_mouse)) return ColorPickerHint{"Copy hex code", current.copy};
	if (current.paste.contains(t_mouse)) return ColorPickerHint{"Paste a color", current.paste};

	if (current.swatch.contains(t_mouse) && t_mouse.x < current.swatch.center().x) {
		return ColorPickerHint{"Back to the original color", current.swatch};
	}

	return std::nullopt;
}

CursorKind ColorPicker::cursor(Vec2 t_mouse) const
{
	if (!m_open) return CursorKind::arrow;
	if (is_dragging()) return CursorKind::drag;

	for (const TextInput &field : m_fields) {
		if (field.is_selecting()) return CursorKind::ibeam;
	}

	const Layout current = layout();
	if (field_at(current, t_mouse) >= 0) return CursorKind::ibeam;

	const bool over_control = current.square.contains(t_mouse) || current.hue.contains(t_mouse) ||
							  current.alpha.contains(t_mouse) || current.copy.contains(t_mouse) ||
							  current.paste.contains(t_mouse) ||
							  (current.swatch.contains(t_mouse) && t_mouse.x < current.swatch.center().x);

	return over_control ? CursorKind::hand : CursorKind::arrow;
}

void ColorPicker::draw(DrawList &t_draw_list, Vec2 t_mouse)
{
	if (!m_open) return;

	const Theme &colors = theme();
	const Layout current = layout();
	const Rect popup = current.popup;
	const Font &font = m_fonts.secondary();
	const Color picked = with_alpha(m_color, 255);

	controls::draw_popup_shadow(t_draw_list, popup.inset(-1.0f), popup_radius, 1.0f);
	t_draw_list.add_bordered_rect(popup.inset(-1.0f), rounded(popup_radius), colors.popup, colors.border, 1.0f);

	t_draw_list.add_color_picker_square(current.square, m_hue);
	draw_handle(t_draw_list,
				Vec2{current.square.x + m_saturation * current.square.w,
					 current.square.y + (1.0f - m_value) * current.square.h},
				picked);

	const std::span<const Color> stops = hue_stops;
	const float segment_width = current.hue.w / static_cast<float>(stops.size() - 1);

	for (usize i = 0; i + 1 < stops.size(); i += 1) {
		const Rect segment{current.hue.x + i * segment_width, current.hue.y, segment_width, current.hue.h};
		t_draw_list.add_gradient(segment, stops[i], stops[i + 1], stops[i], stops[i + 1]);
	}

	const float hue_marker_x = current.hue.x + m_hue / 360.0f * current.hue.w;
	t_draw_list.add_rect(Rect{hue_marker_x - 1.5f, current.hue.y - 2.0f, 3.0f, current.hue.h + 4.0f}, color_marker);

	t_draw_list.add_gradient(current.alpha, picked, picked, with_alpha(picked, 0), with_alpha(picked, 0));

	const float alpha_marker_y = current.alpha.y + (1.0f - m_color.a / 255.0f) * current.alpha.h;
	t_draw_list.add_rect(Rect{current.alpha.x - 2.0f, alpha_marker_y - 1.5f, current.alpha.w + 4.0f, 3.0f},
						 color_marker);

	t_draw_list.add_rect(
		Rect{current.square.x, current.swatch.y - section_gap * 0.5f, current.alpha.right() - current.square.x, 1.0f},
		colors.separator);

	const Rect swatch = current.swatch;
	const float half = swatch.w * 0.5f;
	const float swatch_radius = control_radius;
	t_draw_list.add_rounded_rect(swatch.inset(-1.0f), rounded(swatch_radius + 1.0f), colors.border);
	t_draw_list.add_rounded_rect(Rect{swatch.x, swatch.y, half, swatch.h},
								 rounded(swatch_radius, 0.0f, 0.0f, swatch_radius), with_alpha(m_initial, 255));
	t_draw_list.add_rounded_rect(Rect{swatch.x + half, swatch.y, half, swatch.h},
								 rounded(0.0f, swatch_radius, swatch_radius, 0.0f), picked);

	for (u32 i = 0; i < field_count; i += 1) {
		const Rect field = current.fields[i];
		const bool focused = m_fields[i].is_focused();

		controls::draw_field(t_draw_list, field, control_radius, focused ? colors.text_dim : colors.control,
							 focused ? colors.row_hover : colors.field, 255);
		draw_text(t_draw_list, font, Vec2{field.x + label_inset, font.centered_baseline(field)}, field_labels[i],
				  colors.text_faint);
		m_fields[i].draw(t_draw_list, font, field_text_rect(current, i), colors.text, colors.text);
	}

	const Rect buttons[]{current.copy, current.paste};
	for (const Rect &button : buttons) {
		if (button.contains(t_mouse)) {
			t_draw_list.add_rounded_rect(button, rounded(control_radius), colors.control_hover);
		}
	}

	const Color copy_color = current.copy.contains(t_mouse) ? colors.text : colors.text_dim;
	const Rect copy_glyph = current.copy.centered(glyph_size, glyph_size);
	if (m_copied_seconds > 0.0f) {
		controls::draw_check(t_draw_list, m_assets, copy_glyph, colors.success);
	} else {
		draw_copy_glyph(t_draw_list, copy_glyph, current.copy.contains(t_mouse) ? colors.control_hover : colors.popup,
						copy_color);
	}

	const Color paste_color = m_paste_failed_seconds > 0.0f		? colors.error
							  : current.paste.contains(t_mouse) ? colors.text
																: colors.text_dim;
	draw_paste_glyph(t_draw_list, current.paste.centered(glyph_size, glyph_size),
					 current.paste.contains(t_mouse) ? colors.control_hover : colors.popup, paste_color);
}
