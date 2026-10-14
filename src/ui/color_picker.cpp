#include "ui/color_picker.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <ranges>
#include <span>
#include <utility>

#include "core/animation.h"
#include "core/str.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "os/clipboard.h"
#include "os/window.h"
#include "ui/controls.h"
#include "ui/text.h"
#include "ui/theme.h"

namespace {
constexpr float K_POPUP_PADDING     = 12.0f;
constexpr float K_POPUP_RADIUS      = 10.0f;
constexpr float K_MAX_SQUARE_SIZE   = 200.0f;
constexpr float K_MIN_SQUARE_SIZE   = 120.0f;
constexpr float K_STRIP_GAP         = 10.0f;
constexpr float K_ALPHA_STRIP_WIDTH = 20.0f;
constexpr float K_HUE_STRIP_HEIGHT  = 16.0f;
constexpr float K_HANDLE_RADIUS     = 6.0f;
constexpr float K_BOUNDS_MARGIN     = 8.0f;
constexpr float K_ANCHOR_GAP        = 8.0f;

constexpr float K_SECTION_GAP        = 14.0f;
constexpr float K_ROW_GAP            = 8.0f;
constexpr float K_CONTROL_GAP        = 6.0f;
constexpr float K_SWATCH_WIDTH       = 40.0f;
constexpr float K_CONTROL_RADIUS     = 6.0f;
constexpr float K_LABEL_INSET        = 9.0f;
constexpr float K_LABEL_TEXT_OVERLAP = 4.0f;
constexpr float K_GLYPH_SIZE         = 14.0f;
constexpr float K_FEEDBACK_SECONDS   = 1.2f;
constexpr u32   K_HEX_MAX_LENGTH     = 24;
constexpr u32   K_CHANNEL_MAX_LENGTH = 3;

constexpr Color K_COLOR_MARKER{255, 255, 255, 255};

constexpr Color K_HUE_STOPS[]{
	{255, 0, 0, 255},
	{255, 255, 0, 255},
	{0, 255, 0, 255},
	{0, 255, 255, 255},
	{0, 0, 255, 255},
	{255, 0, 255, 255},
	{255, 0, 0, 255},
};

struct Hsv {
	float hue;
	float saturation;
	float value;
};

[[nodiscard]] auto hsv_to_rgb(float t_hue, float t_saturation, float t_value) -> Color
{
	const float chroma    = t_value * t_saturation;
	const float sector    = std::fmod(t_hue, 360.0f) / 60.0f;
	const float secondary = chroma * (1.0f - std::fabs(std::fmod(sector, 2.0f) - 1.0f));

	float r = 0.0f;
	float g = 0.0f;
	float b = 0.0f;

	switch (static_cast<int>(sector)) {
		case 0: {
			r = chroma;
			g = secondary;
			break;
		}

		case 1: {
			r = secondary;
			g = chroma;
			break;
		}

		case 2: {
			g = chroma;
			b = secondary;
			break;
		}

		case 3: {
			g = secondary;
			b = chroma;
			break;
		}

		case 4: {
			r = secondary;
			b = chroma;
			break;
		}

		default: {
			r = chroma;
			b = secondary;
			break;
		}
	}

	const float lift    = t_value - chroma;
	const auto  to_byte = [lift](float t_channel) { return static_cast<u8>(std::clamp((t_channel + lift) * 255.0f + 0.5f, 0.0f, 255.0f)); };

	return Color{to_byte(r), to_byte(g), to_byte(b), 255};
}

[[nodiscard]] auto rgb_to_hsv(Color t_color) -> Hsv
{
	const float r = t_color.r / 255.0f;
	const float g = t_color.g / 255.0f;
	const float b = t_color.b / 255.0f;

	const float max_channel = std::max({r, g, b});
	const float delta       = max_channel - std::min({r, g, b});

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

[[nodiscard]] auto row_height(const Fonts* t_fonts) -> float
{
	return std::max(28.0f, t_fonts->secondary.line_height() + 10.0f);
}

[[nodiscard]] auto fraction_along(float t_position, float t_start, float t_length) -> float
{
	return std::clamp((t_position - t_start) / t_length, 0.0f, 1.0f);
}

[[nodiscard]] auto parse_hex(std::string_view t_digits, u8 t_alpha) -> std::optional<Color>
{
	if (t_digits.size() != 3 && t_digits.size() != 6 && t_digits.size() != 8) return std::nullopt;

	u8 nibbles[8]{};
	for (const usize i : std::views::iota(usize{0}, t_digits.size())) {
		const auto digit = static_cast<unsigned char>(t_digits[i]);
		if (!std::isxdigit(digit)) return std::nullopt;

		nibbles[i] = static_cast<u8>(std::isdigit(digit) ? digit - '0' : std::tolower(digit) - 'a' + 10);
	}

	if (t_digits.size() == 3) {
		return Color{static_cast<u8>(nibbles[0] * 17), static_cast<u8>(nibbles[1] * 17), static_cast<u8>(nibbles[2] * 17), t_alpha};
	}

	const auto byte = [&nibbles](usize t_index) { return static_cast<u8>(nibbles[t_index] * 16 + nibbles[t_index + 1]); };

	return Color{byte(0), byte(2), byte(4), t_digits.size() == 8 ? byte(6) : t_alpha};
}

[[nodiscard]] auto parse_channels(std::string_view t_text, u8 t_alpha) -> std::optional<Color>
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

	return Color{static_cast<u8>(channels[0]), static_cast<u8>(channels[1]), static_cast<u8>(channels[2]), count == 4 ? static_cast<u8>(channels[3]) : t_alpha};
}

[[nodiscard]] auto parse_color(std::string_view t_text, u8 t_alpha) -> std::optional<Color>
{
	std::string_view text = trimmed(t_text);
	if (text.starts_with('#')) {
		text.remove_prefix(1);
	}

	if (const std::optional<Color> hex = parse_hex(text, t_alpha)) return hex;

	return parse_channels(text, t_alpha);
}

[[nodiscard]] auto is_short_hex(std::string_view t_text) -> bool
{
	std::string_view text = trimmed(t_text);
	if (text.starts_with('#')) {
		text.remove_prefix(1);
	}

	return text.size() == 3 && std::ranges::all_of(text, [](char t_c) { return std::isxdigit(static_cast<unsigned char>(t_c)) != 0; });
}

[[nodiscard]] auto format_hex(Color t_color, char (&t_buffer)[12]) -> std::string_view
{
	if (t_color.a == 255) return format_into(t_buffer, "{:02X}{:02X}{:02X}", t_color.r, t_color.g, t_color.b);

	return format_into(t_buffer, "{:02X}{:02X}{:02X}{:02X}", t_color.r, t_color.g, t_color.b, t_color.a);
}

auto draw_handle(DrawList* t_draw_list, Vec2 t_center, Color t_fill) -> void
{
	const float inner_radius = K_HANDLE_RADIUS - 2.5f;
	const Rect  ring{t_center.x - K_HANDLE_RADIUS, t_center.y - K_HANDLE_RADIUS, K_HANDLE_RADIUS * 2.0f, K_HANDLE_RADIUS * 2.0f};
	const Rect  dot{t_center.x - inner_radius, t_center.y - inner_radius, inner_radius * 2.0f, inner_radius * 2.0f};

	t_draw_list->add_rounded_rect(ring, rounded(K_HANDLE_RADIUS), contrasting_text(t_fill));
	t_draw_list->add_rounded_rect(dot, rounded(inner_radius), t_fill);
}

auto draw_copy_glyph(DrawList* t_draw_list, Rect t_rect, Color t_backdrop, Color t_color) -> void
{
	const float size = t_rect.w * 0.68f;
	const Rect  back{t_rect.x, t_rect.y, size, size};
	const Rect  front{t_rect.right() - size, t_rect.bottom() - size, size, size};

	t_draw_list->add_bordered_rect(back, rounded(2.5f), t_backdrop, t_color, 1.5f);
	t_draw_list->add_bordered_rect(front, rounded(2.5f), t_backdrop, t_color, 1.5f);
}

auto draw_paste_glyph(DrawList* t_draw_list, Rect t_rect, Color t_backdrop, Color t_color) -> void
{
	const Rect board{t_rect.x + t_rect.w * 0.12f, t_rect.y + t_rect.h * 0.12f, t_rect.w * 0.76f, t_rect.h * 0.88f};
	const Rect clip{t_rect.center().x - t_rect.w * 0.2f, t_rect.y, t_rect.w * 0.4f, t_rect.h * 0.26f};

	t_draw_list->add_bordered_rect(board, rounded(2.5f), t_backdrop, t_color, 1.5f);
	t_draw_list->add_rounded_rect(clip, rounded(1.5f), t_color);
}
}

ColorPicker::ColorPicker(const Fonts* t_fonts, const Assets* t_assets)
	: m_fonts(t_fonts)
	, m_assets(t_assets)
{
	m_fields[ColorField::HEX].set_max_length(K_HEX_MAX_LENGTH);

	for (const ColorField field : K_CHANNEL_FIELDS) {
		m_fields[field].set_max_length(K_CHANNEL_MAX_LENGTH);
	}
}

auto ColorPicker::layout() const -> ColorPicker::Layout
{
	const float row           = row_height(m_fonts);
	const float fixed_height  = K_POPUP_PADDING * 2.0f + K_STRIP_GAP + K_HUE_STRIP_HEIGHT + K_SECTION_GAP + row + K_ROW_GAP + row;
	const float room          = m_bounds.h - K_BOUNDS_MARGIN * 2.0f;
	const float square        = std::clamp(room - fixed_height, K_MIN_SQUARE_SIZE, K_MAX_SQUARE_SIZE);
	const float content_width = square + K_STRIP_GAP + K_ALPHA_STRIP_WIDTH;
	const float width         = K_POPUP_PADDING * 2.0f + content_width;
	const float height        = fixed_height + square;

	const float top_limit    = m_bounds.y + K_BOUNDS_MARGIN;
	const float bottom_limit = m_bounds.bottom() - K_BOUNDS_MARGIN;
	const float left_limit   = m_bounds.x + K_BOUNDS_MARGIN;
	const float right_limit  = m_bounds.right() - K_BOUNDS_MARGIN;

	Vec2 origin{m_anchor.right() - width, m_anchor.bottom() + K_ANCHOR_GAP};

	if (origin.y + height > bottom_limit) {
		origin.y = m_anchor.y - K_ANCHOR_GAP - height;

		if (origin.y < top_limit) {
			origin = Vec2{m_anchor.x - K_ANCHOR_GAP - width, m_anchor.center().y - height * 0.5f};
		}
	}

	origin.x = std::clamp(origin.x, left_limit, std::max(left_limit, right_limit - width));
	origin.y = std::clamp(origin.y, top_limit, std::max(top_limit, bottom_limit - height));

	Layout result{};
	result.popup = Rect{origin.x, origin.y, width, height};

	const float left = result.popup.x + K_POPUP_PADDING;
	float       top  = result.popup.y + K_POPUP_PADDING;

	result.square = Rect{left, top, square, square};
	result.alpha  = Rect{result.square.right() + K_STRIP_GAP, top, K_ALPHA_STRIP_WIDTH, square};
	result.hue    = Rect{left, result.square.bottom() + K_STRIP_GAP, square, K_HUE_STRIP_HEIGHT};

	top           = result.hue.bottom() + K_SECTION_GAP;
	result.swatch = Rect{left, top, K_SWATCH_WIDTH, row};
	result.paste  = Rect{left + content_width - row, top, row, row};
	result.copy   = Rect{result.paste.x - K_CONTROL_GAP - row, top, row, row};

	const float hex_x              = result.swatch.right() + K_CONTROL_GAP;
	result.fields[ColorField::HEX] = Rect{hex_x, top, result.copy.x - K_CONTROL_GAP - hex_x, row};

	top += row + K_ROW_GAP;
	constexpr auto CHANNEL_COUNT = static_cast<float>(std::size(K_CHANNEL_FIELDS));
	const float    channel_width = (content_width - K_CONTROL_GAP * (CHANNEL_COUNT - 1.0f)) / CHANNEL_COUNT;
	float          channel_x     = left;

	for (const ColorField field : K_CHANNEL_FIELDS) {
		result.fields[field] = Rect{channel_x, top, channel_width, row};
		channel_x += channel_width + K_CONTROL_GAP;
	}

	return result;
}

auto ColorPicker::field_text_rect(const Layout& t_layout, ColorField t_field) const -> Rect
{
	const Rect  field  = t_layout.fields[t_field];
	const float text_x = field.x + K_LABEL_INSET + text_width(m_fonts->secondary, K_FIELD_LABELS[t_field]) - K_LABEL_TEXT_OVERLAP;

	return Rect{text_x, field.y, field.right() - text_x, field.h};
}

auto ColorPicker::field_at(const Layout& t_layout, Vec2 t_point) const -> std::optional<ColorPicker::ColorField>
{
	for (const ColorField field : each<ColorField>()) {
		if (t_layout.fields[field].contains(t_point)) return field;
	}

	return std::nullopt;
}

auto ColorPicker::focused_field() const -> std::optional<ColorPicker::ColorField>
{
	for (const ColorField field : each<ColorField>()) {
		if (m_fields[field].m_focused) return field;
	}

	return std::nullopt;
}

auto ColorPicker::contains(Vec2 t_point) const -> bool
{
	return m_open && layout().popup.contains(t_point);
}

auto ColorPicker::take_changed() -> bool
{
	return std::exchange(m_changed, false);
}

auto ColorPicker::set_color(Color t_color) -> void
{
	const Hsv hsv = rgb_to_hsv(t_color);

	if (hsv.saturation > 0.0001f && hsv.value > 0.0001f) {
		m_hue = hsv.hue;
	}

	m_saturation = hsv.saturation;
	m_value      = hsv.value;
	m_color      = t_color;
	m_changed    = true;
}

auto ColorPicker::apply_hsv() -> void
{
	m_color   = with_alpha(hsv_to_rgb(m_hue, m_saturation, m_value), m_color.a);
	m_changed = true;
	sync_fields();
}

auto ColorPicker::sync_fields(std::optional<ColorField> t_skipped_field) -> void
{
	char hex[12];
	char channel[4];

	for (const ColorField field : each<ColorField>()) {
		if (field == t_skipped_field) continue;

		std::string_view text;
		if (field == ColorField::HEX) {
			text = format_hex(m_color, hex);
		} else {
			text = format_into(channel, "{}", m_color.*K_CHANNELS[field]);
		}

		m_fields[field].set_value(text);
		copy_to(text, m_synced_text[field]);
	}
}

auto ColorPicker::read_field(ColorField t_field) -> void
{
	const std::string_view text = m_fields[t_field].value();
	copy_to(text, m_synced_text[t_field]);

	if (t_field == ColorField::HEX) {
		const std::optional<Color> parsed = parse_color(text, m_color.a);
		if (parsed && !is_short_hex(text)) {
			set_color(*parsed);
			sync_fields(ColorField::HEX);
		}

		return;
	}

	if (text.empty() || text.size() > K_CHANNEL_MAX_LENGTH || !std::ranges::all_of(text, [](char t_c) { return std::isdigit(static_cast<unsigned char>(t_c)) != 0; })) {
		return;
	}

	u32 value = 0;
	for (const char digit : text) {
		value = value * 10 + static_cast<u32>(digit - '0');
	}

	Color next                = m_color;
	next.*K_CHANNELS[t_field] = static_cast<u8>(std::min(value, 255u));

	set_color(next);
	sync_fields(t_field);
}

auto ColorPicker::focus_field(std::optional<ColorField> t_field) -> void
{
	const std::optional<ColorField> previous = focused_field();

	if (previous == ColorField::HEX && previous != t_field) {
		const std::string_view     text   = m_fields[ColorField::HEX].value();
		const std::optional<Color> parsed = parse_color(text, m_color.a);

		if (parsed && is_short_hex(text)) {
			set_color(*parsed);
		}
	}

	for (const ColorField field : each<ColorField>()) {
		m_fields[field].set_focused(field == t_field);
	}

	if (previous && previous != t_field) {
		sync_fields(t_field);
	}
}

auto ColorPicker::copy_hex() -> void
{
	char                   hex[12];
	char                   text[16];
	const std::string_view digits = format_hex(m_color, hex);

	os::set_clipboard_text(format_into(text, "#{}", digits));
	m_copied_seconds       = K_FEEDBACK_SECONDS;
	m_paste_failed_seconds = 0.0f;
}

auto ColorPicker::paste_color() -> void
{
	const std::optional<Color> parsed = parse_color(os::clipboard_text(), m_color.a);

	if (!parsed) {
		m_paste_failed_seconds = K_FEEDBACK_SECONDS;
		m_copied_seconds       = 0.0f;
		return;
	}

	set_color(*parsed);
	focus_field(std::nullopt);
	sync_fields();
}

auto ColorPicker::end_drags() -> void
{
	m_saturation_value_drag.end();
	m_hue_drag.end();
	m_alpha_drag.end();
}

auto ColorPicker::open(Color t_initial, Rect t_anchor, Rect t_bounds) -> void
{
	const Hsv hsv = rgb_to_hsv(t_initial);
	m_hue         = hsv.hue;
	m_saturation  = hsv.saturation;
	m_value       = hsv.value;
	m_color       = t_initial;
	m_initial     = t_initial;
	m_changed     = false;

	m_anchor               = t_anchor;
	m_bounds               = t_bounds;
	m_open                 = true;
	m_press                = Press::NONE;
	m_copied_seconds       = 0.0f;
	m_paste_failed_seconds = 0.0f;

	end_drags();
	focus_field(std::nullopt);
	sync_fields();
}

auto ColorPicker::close() -> void
{
	focus_field(std::nullopt);
	m_open  = false;
	m_press = Press::NONE;
	end_drags();
}

auto ColorPicker::update(float t_delta_seconds) -> void
{
	m_copied_seconds       = std::max(0.0f, m_copied_seconds - t_delta_seconds);
	m_paste_failed_seconds = std::max(0.0f, m_paste_failed_seconds - t_delta_seconds);

	if (m_copied_seconds > 0.0f || m_paste_failed_seconds > 0.0f) {
		animation::request_frame_after(std::max(m_copied_seconds, m_paste_failed_seconds));
	}

	if (!m_open) return;

	for (const ColorField field : each<ColorField>()) {
		m_fields[field].update(t_delta_seconds);

		if (m_fields[field].value() != std::string_view{m_synced_text[field]}) {
			read_field(field);
		}
	}
}

auto ColorPicker::on_pointer_down(Vec2 t_point) -> bool
{
	if (!m_open) return false;

	const Layout current = layout();
	if (!current.popup.contains(t_point)) return false;

	m_press = Press::NONE;

	if (const std::optional<ColorField> field = field_at(current, t_point)) {
		focus_field(field);
		m_fields[*field].on_pointer_down(m_fonts->secondary, field_text_rect(current, *field), t_point.x);
		m_press = Press::FIELD;
		return true;
	}

	focus_field(std::nullopt);

	if (current.square.contains(t_point)) {
		m_saturation_value_drag.begin(t_point);
	} else if (current.hue.contains(t_point)) {
		m_hue_drag.begin(t_point);
	} else if (current.alpha.contains(t_point)) {
		m_alpha_drag.begin(t_point);
	} else if (current.swatch.contains(t_point) && t_point.x < current.swatch.center().x) {
		m_press = Press::REVERT;
	} else if (current.copy.contains(t_point)) {
		m_press = Press::COPY;
	} else if (current.paste.contains(t_point)) {
		m_press = Press::PASTE;
	}

	on_pointer_move(t_point);

	return true;
}

auto ColorPicker::on_pointer_move(Vec2 t_point) -> void
{
	if (!m_open) return;

	const Layout current = layout();

	for (const ColorField field : each<ColorField>()) {
		if (m_fields[field].m_selecting) {
			m_fields[field].on_pointer_move(m_fonts->secondary, field_text_rect(current, field), t_point.x);
		}
	}

	if (m_saturation_value_drag.m_pressed) {
		m_saturation_value_drag.update(t_point);
		m_saturation = fraction_along(t_point.x, current.square.x, current.square.w);
		m_value      = 1.0f - fraction_along(t_point.y, current.square.y, current.square.h);
		apply_hsv();
	}

	if (m_hue_drag.m_pressed) {
		m_hue_drag.update(t_point);
		m_hue = fraction_along(t_point.x, current.hue.x, current.hue.w) * 359.9f;
		apply_hsv();
	}

	if (m_alpha_drag.m_pressed) {
		m_alpha_drag.update(t_point);
		m_color.a = static_cast<u8>((1.0f - fraction_along(t_point.y, current.alpha.y, current.alpha.h)) * 255.0f);
		m_changed = true;
		sync_fields();
	}
}

auto ColorPicker::on_pointer_up(Vec2 t_point) -> bool
{
	const bool  was_pressed = m_press != Press::NONE || is_dragging();
	const Press press       = std::exchange(m_press, Press::NONE);

	for (TextInput& field : m_fields) {
		field.on_pointer_up();
	}

	end_drags();

	if (!m_open || !was_pressed) return false;

	const Layout current = layout();

	switch (press) {
		using enum Press;

		case REVERT: {
			if (current.swatch.contains(t_point) && t_point.x < current.swatch.center().x) {
				set_color(m_initial);
				sync_fields();
			}
			break;
		}

		case COPY: {
			if (current.copy.contains(t_point)) {
				copy_hex();
			}
			break;
		}

		case PASTE: {
			if (current.paste.contains(t_point)) {
				paste_color();
			}
			break;
		}

		case NONE:
		case FIELD: {
			break;
		}
	}

	return true;
}

auto ColorPicker::on_right_click(Vec2 t_point) -> TextInput*
{
	if (!m_open) return nullptr;

	const Layout                    current = layout();
	const std::optional<ColorField> field   = field_at(current, t_point);
	if (!field) return nullptr;

	focus_field(field);
	m_fields[*field].on_right_click(m_fonts->secondary, field_text_rect(current, *field), t_point.x);

	return &m_fields[*field];
}

auto ColorPicker::on_key_down(os::Key t_key) -> bool
{
	if (!m_open) return false;

	const std::optional<ColorField> focused = focused_field();
	const bool                      control = os::modifiers().shortcut;

	if (!focused) {
		if (t_key == os::Key::ESCAPE) {
			close();
			return true;
		}

		if (control && t_key == os::Key::C) {
			copy_hex();
			return true;
		}

		if (control && t_key == os::Key::V) {
			paste_color();
			return true;
		}

		return false;
	}

	switch (t_key) {
		using enum os::Key;

		case ENTER:
		case ESCAPE: {
			focus_field(std::nullopt);
			return true;
		}

		case TAB: {
			const ColorField next = stepped(*focused, os::modifiers().shift ? -1 : 1);

			focus_field(next);
			m_fields[next].apply(TextEdit::SELECT_ALL);
			return true;
		}

		default: {
			m_fields[*focused].on_key_down(t_key);
			return true;
		}
	}
}

auto ColorPicker::on_char(u32 t_character) -> bool
{
	const std::optional<ColorField> focused = focused_field();
	if (!m_open || !focused) return false;

	const bool digit     = t_character >= '0' && t_character <= '9';
	const bool hex_digit = digit || (t_character >= 'a' && t_character <= 'f') || (t_character >= 'A' && t_character <= 'F');
	const bool accepted  = focused == ColorField::HEX ? hex_digit || t_character == '#' : digit;

	if (accepted) {
		m_fields[*focused].on_char(t_character);
	}

	return true;
}

auto ColorPicker::hint(Vec2 t_mouse) const -> std::optional<ColorPickerHint>
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

auto ColorPicker::cursor(Vec2 t_mouse) const -> CursorKind
{
	if (!m_open) return CursorKind::ARROW;
	if (is_dragging()) return CursorKind::DRAG;

	for (const TextInput& field : m_fields) {
		if (field.m_selecting) return CursorKind::I_BEAM;
	}

	const Layout current = layout();
	if (field_at(current, t_mouse)) return CursorKind::I_BEAM;

	const bool over_control = current.square.contains(t_mouse) || current.hue.contains(t_mouse) || current.alpha.contains(t_mouse) || current.copy.contains(t_mouse) || current.paste.contains(t_mouse) || (current.swatch.contains(t_mouse) && t_mouse.x < current.swatch.center().x);

	return over_control ? CursorKind::HAND : CursorKind::ARROW;
}

auto ColorPicker::draw(DrawList* t_draw_list, Vec2 t_mouse) -> void
{
	if (!m_open) return;

	const Layout current = layout();
	const Rect   popup   = current.popup;
	const Font&  font    = m_fonts->secondary;
	const Color  picked  = with_alpha(m_color, 255);

	{
		const RoundnessScope corners{controls::K_POPUP_ROUNDNESS};

		controls::draw_popup_shadow(t_draw_list, popup.inset(-1.0f), K_POPUP_RADIUS, 1.0f);
		controls::draw_glass(t_draw_list, popup.inset(-1.0f), rounded(K_POPUP_RADIUS), controls::GlassSurface::MENU, 255);
	}

	const Vec2 handle{current.square.x + m_saturation * current.square.w, current.square.y + (1.0f - m_value) * current.square.h};

	t_draw_list->add_color_picker_square(current.square, m_hue);
	draw_handle(t_draw_list, handle, picked);

	const std::span<const Color> stops         = K_HUE_STOPS;
	const float                  segment_width = current.hue.w / static_cast<float>(stops.size() - 1);

	for (const usize i : std::views::iota(usize{0}, stops.size() - 1)) {
		const Rect segment{current.hue.x + i * segment_width, current.hue.y, segment_width, current.hue.h};
		t_draw_list->add_gradient(segment, stops[i], stops[i + 1], stops[i], stops[i + 1]);
	}

	const float hue_marker_x = current.hue.x + m_hue / 360.0f * current.hue.w;
	const Rect  hue_marker{hue_marker_x - 1.5f, current.hue.y - 2.0f, 3.0f, current.hue.h + 4.0f};
	const Color clear = with_alpha(picked, 0);

	t_draw_list->add_rect(hue_marker, K_COLOR_MARKER);
	t_draw_list->add_gradient(current.alpha, picked, picked, clear, clear);

	const float alpha_marker_y = current.alpha.y + (1.0f - m_color.a / 255.0f) * current.alpha.h;
	const Rect  alpha_marker{current.alpha.x - 2.0f, alpha_marker_y - 1.5f, current.alpha.w + 4.0f, 3.0f};
	const Rect  section_line{current.square.x, current.swatch.y - K_SECTION_GAP * 0.5f, current.alpha.right() - current.square.x, 1.0f};

	t_draw_list->add_rect(alpha_marker, K_COLOR_MARKER);
	t_draw_list->add_rect(section_line, g_theme.separator);

	const Rect  swatch        = current.swatch;
	const float half          = swatch.w * 0.5f;
	const float swatch_radius = K_CONTROL_RADIUS;
	const Rect  before{swatch.x, swatch.y, half, swatch.h};
	const Rect  after{swatch.x + half, swatch.y, half, swatch.h};

	t_draw_list->add_rounded_rect(swatch.inset(-1.0f), rounded(swatch_radius + 1.0f), g_theme.border);
	t_draw_list->add_rounded_rect(before, rounded(swatch_radius, 0.0f, 0.0f, swatch_radius), with_alpha(m_initial, 255));
	t_draw_list->add_rounded_rect(after, rounded(0.0f, swatch_radius, swatch_radius, 0.0f), picked);

	for (const ColorField edit : each<ColorField>()) {
		const Rect  field   = current.fields[edit];
		const bool  focused = m_fields[edit].m_focused;
		const Color border  = focused ? g_theme.text_dim : g_theme.control;
		const Color fill    = focused ? g_theme.row_hover : g_theme.field;
		const Vec2  label_baseline{field.x + K_LABEL_INSET, font.centered_baseline(field)};

		controls::draw_field(t_draw_list, field, K_CONTROL_RADIUS, border, fill, 255);
		draw_text(t_draw_list, font, label_baseline, K_FIELD_LABELS[edit], g_theme.text_faint);
		m_fields[edit].draw(t_draw_list, font, field_text_rect(current, edit), g_theme.text, g_theme.text);
	}

	const Rect buttons[]{current.copy, current.paste};
	for (const Rect& button : buttons) {
		if (button.contains(t_mouse)) {
			t_draw_list->add_rounded_rect(button, rounded(K_CONTROL_RADIUS), g_theme.control_hover);
		}
	}

	const bool  copy_hovered  = current.copy.contains(t_mouse);
	const Color copy_color    = copy_hovered ? g_theme.text : g_theme.text_dim;
	const Color copy_backdrop = copy_hovered ? g_theme.control_hover : g_theme.popup;
	const Rect  copy_glyph    = current.copy.centered(K_GLYPH_SIZE, K_GLYPH_SIZE);
	if (m_copied_seconds > 0.0f) {
		controls::draw_check(t_draw_list, m_assets, copy_glyph, g_theme.success);
	} else {
		draw_copy_glyph(t_draw_list, copy_glyph, copy_backdrop, copy_color);
	}

	const bool paste_hovered = current.paste.contains(t_mouse);
	Color      paste_color   = paste_hovered ? g_theme.text : g_theme.text_dim;
	if (m_paste_failed_seconds > 0.0f) {
		paste_color = g_theme.error;
	}

	const Rect  paste_glyph    = current.paste.centered(K_GLYPH_SIZE, K_GLYPH_SIZE);
	const Color paste_backdrop = paste_hovered ? g_theme.control_hover : g_theme.popup;
	draw_paste_glyph(t_draw_list, paste_glyph, paste_backdrop, paste_color);
}
