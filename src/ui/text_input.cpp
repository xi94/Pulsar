#include "ui/text_input.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>
#include <optional>
#include <span>
#include <utility>

#include "core/animation.h"
#include "core/str.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "os/clipboard.h"
#include "os/input.h"
#include "ui/text.h"
#include "ui/theme.h"

namespace {
constexpr float K_CARET_BLINK_PERIOD = 1.0f;
constexpr float K_CARET_WIDTH        = 1.5f;
constexpr float K_TEXT_PADDING       = 8.0f;
constexpr float K_HIGHLIGHT_INSET    = 4.0f;
constexpr float K_CARET_OVERHANG     = 2.0f;
constexpr u8    K_SELECTION_ALPHA    = 70;
constexpr float K_MULTI_CLICK_SLOP   = 4.0f;
constexpr u32   K_CLICKS_PER_CYCLE   = 3;

constexpr float            K_CARET_TYPING_RATE     = 50.0f;
constexpr float            K_CARET_JUMP_RATE       = 27.0f;
constexpr float            K_TRAIL_TAIL_RATE_SHORT = 14.0f;
constexpr float            K_TRAIL_TAIL_RATE_LONG  = 3.5f;
constexpr float            K_TRAIL_TAIL_SPREAD     = 0.45f;
constexpr float            K_TRAIL_FAINT_ALPHA_MIN = 30.0f;
constexpr float            K_TRAIL_FAINT_ALPHA_MAX = 80.0f;
constexpr float            K_TRAIL_MIN_LENGTH      = 0.5f;
constexpr float            K_TRAIL_MAX_STEP        = 0.1f;
constexpr auto             K_HOP_WINDOW            = std::chrono::milliseconds(250);
constexpr float            K_UNDERLINE_HEIGHT      = 2.0f;
constexpr std::string_view K_END_OF_TEXT_CELL      = "0";

struct TrailPoint {
	Vec2  position;
	Color color;
};

struct CaretMark {
	Vec2                                  position;
	std::chrono::steady_clock::time_point released_at;
};

CaretStyle g_caret_style    = CaretStyle::BAR;
bool       g_caret_trail    = true;
float      g_trail_strength = 0.5f;
CaretMark  g_last_caret{};

// The outline around a set of points (Andrew's monotone chain), so the trail is one shape however the caret moves.
[[nodiscard]] auto convex_hull(std::span<TrailPoint> t_points, std::span<TrailPoint> t_out) -> u32
{
	std::ranges::sort(t_points, [](const TrailPoint& t_a, const TrailPoint& t_b) {
		return t_a.position.x < t_b.position.x || (t_a.position.x == t_b.position.x && t_a.position.y < t_b.position.y);
	});

	const auto turn = [](Vec2 t_origin, Vec2 t_a, Vec2 t_b) {
		return (t_a.x - t_origin.x) * (t_b.y - t_origin.y) - (t_a.y - t_origin.y) * (t_b.x - t_origin.x);
	};

	u32        count = 0;
	const auto add   = [&](const TrailPoint& t_point, u32 t_floor) {
		while (count >= t_floor && turn(t_out[count - 2].position, t_out[count - 1].position, t_point.position) <= 0.0f) {
			count -= 1;
		}

		t_out[count] = t_point;
		count += 1;
	};

	for (const TrailPoint& point : t_points) {
		add(point, 2);
	}

	const u32 lower_end = count + 1;
	for (usize i = t_points.size() - 1; i > 0; i -= 1) {
		add(t_points[i - 1], lower_end);
	}

	return count - 1;
}

// The colour that reads best on top of a block caret: the theme's window or text colour, whichever is darker on a light caret and
// whichever is lighter on a dark one.
[[nodiscard]] auto ink_on(Color t_background) -> Color
{
	const bool window_is_darker = luminance(g_theme.window) < luminance(g_theme.text);
	const bool light_background = luminance(t_background) > 0.35f;

	return light_background == window_is_darker ? g_theme.window : g_theme.text;
}

enum class CharClass : u8 {
	SPACE,
	WORD,
	SYMBOL,
};

[[nodiscard]] auto class_of(char t_character) -> CharClass
{
	if (t_character == ' ') return CharClass::SPACE;
	if (static_cast<u8>(t_character) >= 0x80) return CharClass::WORD;
	if (std::isalnum(static_cast<unsigned char>(t_character)) || t_character == '_') return CharClass::WORD;

	return CharClass::SYMBOL;
}

[[nodiscard]] auto is_printable(u32 t_codepoint) -> bool
{
	return t_codepoint >= 0x20 && t_codepoint != 0x7F && (t_codepoint < 0x80 || t_codepoint >= 0xA0) && !is_blocked_script(t_codepoint);
}

[[nodiscard]] auto milliseconds_now() -> u64
{
	return static_cast<u64>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

[[nodiscard]] auto shortcut_for(os::Key t_key) -> std::optional<TextEdit>
{
	switch (t_key) {
		using enum os::Key;

		case A: {
			return TextEdit::SELECT_ALL;
		}

		case C: {
			return TextEdit::COPY;
		}

		case X: {
			return TextEdit::CUT;
		}

		case V: {
			return TextEdit::PASTE;
		}

		default: {
			return std::nullopt;
		}
	}
}

[[nodiscard]] auto previous_word_start(std::string_view t_text, u32 t_index) -> u32
{
	while (t_index > 0 && class_of(t_text[t_index - 1]) == CharClass::SPACE) {
		t_index -= 1;
	}

	const CharClass word = t_index > 0 ? class_of(t_text[t_index - 1]) : CharClass::SPACE;
	while (t_index > 0 && class_of(t_text[t_index - 1]) == word) {
		t_index -= 1;
	}

	return t_index;
}

[[nodiscard]] auto next_word_start(std::string_view t_text, u32 t_index) -> u32
{
	const auto length = static_cast<u32>(t_text.size());

	const CharClass word = t_index < length ? class_of(t_text[t_index]) : CharClass::SPACE;
	while (t_index < length && class_of(t_text[t_index]) == word) {
		t_index += 1;
	}

	while (t_index < length && class_of(t_text[t_index]) == CharClass::SPACE) {
		t_index += 1;
	}

	return t_index;
}

[[nodiscard]] auto word_at(std::string_view t_text, u32 t_index) -> TextRange
{
	if (t_text.empty()) return TextRange{0, 0};

	const auto      length = static_cast<u32>(t_text.size());
	const u32       inside = std::min(t_index, length - 1);
	const CharClass word   = class_of(t_text[inside]);

	TextRange range{inside, inside + 1};
	while (range.start > 0 && class_of(t_text[range.start - 1]) == word) {
		range.start -= 1;
	}

	while (range.end < length && class_of(t_text[range.end]) == word) {
		range.end += 1;
	}

	return range;
}
}

auto TextInput::set_value(std::string_view t_value) -> void
{
	std::memset(m_text, 0, sizeof(m_text));

	usize length = std::min<usize>(t_value.size(), m_max_length);
	while (length > 0 && length < t_value.size() && is_utf8_continuation(t_value[length])) {
		length -= 1;
	}

	m_length = static_cast<u32>(length);
	std::memcpy(m_text, t_value.data(), m_length);

	move_cursor(m_length, false);
	m_scroll_x = 0.0f;
	restart_caret_blink();
}

auto TextInput::set_max_length(u32 t_max_length) -> void
{
	m_max_length = std::min(t_max_length, K_TEXT_INPUT_CAPACITY);
}

auto TextInput::can_apply(TextEdit t_edit) const -> bool
{
	switch (t_edit) {
		using enum TextEdit;

		case CUT:
		case COPY: {
			return has_selection() && !m_masked;
		}

		case PASTE: {
			return os::clipboard_has_text();
		}

		case SELECT_ALL: {
			return m_length > 0;
		}
	}

	return false;
}

auto TextInput::apply(TextEdit t_edit) -> void
{
	if (!can_apply(t_edit)) return;

	const TextRange        range = selection();
	const std::string_view selected{m_text + range.start, range.end - range.start};

	switch (t_edit) {
		using enum TextEdit;

		case CUT: {
			os::set_clipboard_text(selected);
			erase(range);
			break;
		}

		case COPY: {
			os::set_clipboard_text(selected);
			break;
		}

		case PASTE: {
			insert(os::clipboard_text());
			break;
		}

		case SELECT_ALL: {
			select(TextRange{0, m_length});
			break;
		}
	}

	restart_caret_blink();
}

auto TextInput::on_char(u32 t_codepoint) -> void
{
	if (!m_focused || !is_printable(t_codepoint)) return;

	char      encoded[4];
	const u32 length = encode_utf8(t_codepoint, encoded);
	insert(std::string_view{encoded, length});
	restart_caret_blink();
}

auto TextInput::on_key_down(os::Key t_key) -> void
{
	if (!m_focused) return;

	const os::Modifiers held = os::modifiers();

	if (const std::optional<TextEdit> edit = held.shortcut ? shortcut_for(t_key) : std::nullopt) {
		apply(*edit);
		return;
	}

	char                   mask[K_TEXT_INPUT_CAPACITY];
	const std::string_view shown = shown_text(mask);
	const TextRange        range = selection();

	const u32  previous           = held.word_step ? previous_word_start(shown, m_cursor) : static_cast<u32>(previous_codepoint(value(), m_cursor));
	const u32  next               = held.word_step ? next_word_start(shown, m_cursor) : static_cast<u32>(next_codepoint(value(), m_cursor));
	const bool collapse_selection = has_selection() && !held.shift;

	switch (t_key) {
		using enum os::Key;

		case BACKSPACE: {
			erase(has_selection() ? range : TextRange{previous, m_cursor});
			break;
		}

		case FORWARD_DELETE: {
			erase(has_selection() ? range : TextRange{m_cursor, next});
			break;
		}

		case LEFT: {
			move_cursor(collapse_selection ? range.start : previous, held.shift);
			break;
		}

		case RIGHT: {
			move_cursor(collapse_selection ? range.end : next, held.shift);
			break;
		}

		case HOME: {
			move_cursor(0, held.shift);
			break;
		}

		case END: {
			move_cursor(m_length, held.shift);
			break;
		}

		case E: {
			if (!held.shortcut) return;

			move_cursor(m_length, false);
			break;
		}

		default: {
			return;
		}
	}

	restart_caret_blink();
}

auto TextInput::on_pointer_down(const Font& t_font, Rect t_field, float t_x) -> void
{
	const u64  now_ms       = milliseconds_now();
	const bool quick_repeat = now_ms - m_last_click_ms <= os::double_click_ms();
	const bool same_spot    = std::fabs(t_x - m_last_click_x) <= K_MULTI_CLICK_SLOP;

	m_click_count   = quick_repeat && same_spot ? m_click_count % K_CLICKS_PER_CYCLE + 1 : 1;
	m_last_click_ms = now_ms;
	m_last_click_x  = t_x;

	char      mask[K_TEXT_INPUT_CAPACITY];
	const u32 index = index_at(t_font, t_field, t_x);

	if (m_click_count == 1) {
		move_cursor(index, os::modifiers().shift);
	} else if (m_click_count == 2) {
		select(word_at(shown_text(mask), index));
	} else {
		select(TextRange{0, m_length});
	}

	m_selecting = true;
	restart_caret_blink();
}

auto TextInput::on_pointer_move(const Font& t_font, Rect t_field, float t_x) -> void
{
	if (!m_selecting || m_click_count != 1) return;

	m_cursor = index_at(t_font, t_field, t_x);
	restart_caret_blink();
}

auto TextInput::on_pointer_up() -> void
{
	m_selecting = false;
}

auto TextInput::on_right_click(const Font& t_font, Rect t_field, float t_x) -> void
{
	const u32       index = index_at(t_font, t_field, t_x);
	const TextRange range = selection();

	if (index < range.start || index > range.end) {
		move_cursor(index, false);
	}
}

auto set_caret_style(CaretStyle t_style, bool t_trail, float t_trail_strength) -> void
{
	g_caret_style    = t_style;
	g_caret_trail    = t_trail;
	g_trail_strength = std::clamp(t_trail_strength, 0.0f, 1.0f);
}

auto TextInput::set_focused(bool t_focused) -> void
{
	if (t_focused && !m_focused) {
		m_trail_placed = false;
		restart_caret_blink();
	} else if (!t_focused && m_focused) {
		g_last_caret.released_at = std::chrono::steady_clock::now();
	}

	m_focused = t_focused;
}

auto TextInput::update(float t_delta_seconds) -> void
{
	m_trail_seconds += t_delta_seconds;
	m_caret_blink_seconds = std::fmod(m_caret_blink_seconds + t_delta_seconds, K_CARET_BLINK_PERIOD);

	if (m_focused) {
		const float half_period = K_CARET_BLINK_PERIOD * 0.5f;
		const float next_toggle = m_caret_blink_seconds < half_period ? half_period - m_caret_blink_seconds : K_CARET_BLINK_PERIOD - m_caret_blink_seconds;
		animation::request_frame_after(next_toggle);
	}
}

auto TextInput::draw(DrawList* t_draw_list, const Font& t_font, Rect t_field, Color t_text_color, Color t_caret_color, std::optional<Rect> t_placeholder_box)
	-> void
{
	char                   mask[K_TEXT_INPUT_CAPACITY];
	const std::string_view shown = shown_text(mask);

	const Rect  content       = t_field.inset(K_TEXT_PADDING, 0.0f);
	const float visible_width = std::max(0.0f, content.w - K_CARET_WIDTH);
	const float caret_offset  = text_width(t_font, shown.substr(0, m_cursor));

	m_scroll_x = std::clamp(m_scroll_x, caret_offset - visible_width, caret_offset);
	m_scroll_x = std::clamp(m_scroll_x, 0.0f, std::max(0.0f, text_width(t_font, shown) - visible_width));

	const float origin_x         = content.x - m_scroll_x;
	const float baseline         = t_font.centered_baseline(t_field);
	const float highlight_y      = std::max(t_field.y + K_HIGHLIGHT_INSET, baseline - t_font.ascent - K_CARET_OVERHANG);
	const float highlight_bottom = std::min(t_field.bottom() - K_HIGHLIGHT_INSET, baseline - t_font.descent + K_CARET_OVERHANG);
	const float highlight_height = std::max(0.0f, highlight_bottom - highlight_y);
	const Vec2  caret_origin{content.x, highlight_y};
	const Vec2  caret_extent = caret_size(t_font, shown, highlight_height);

	move_caret(Vec2{caret_offset - m_scroll_x, highlight_height - caret_extent.y}, caret_origin, highlight_height);

	t_draw_list->push_clip(content);

	if (m_focused && has_selection()) {
		const TextRange range   = selection();
		const float     start_x = origin_x + text_width(t_font, shown.substr(0, range.start));
		const float     end_x   = origin_x + text_width(t_font, shown.substr(0, range.end));

		t_draw_list->add_rect(Rect{start_x, highlight_y, end_x - start_x, highlight_height}, faded(t_caret_color, K_SELECTION_ALPHA));
	}

	if (m_length == 0) {
		const float placeholder_x = t_placeholder_box ? t_placeholder_box->center().x - text_width(t_font, m_placeholder) * 0.5f : content.x;
		draw_text(t_draw_list, t_font, Vec2{placeholder_x, baseline}, m_placeholder, faded(g_theme.text_faint, t_text_color.a));
	}

	t_draw_list->pop_clip();

	// Outside the field's clip, so a caret hopping in from another field shows on its way over. The text goes on top of it.
	const bool caret_shown = m_focused && m_caret_blink_seconds < K_CARET_BLINK_PERIOD * 0.5f;
	if (caret_shown) {
		draw_caret(t_draw_list, caret_origin, caret_extent, t_caret_color);
	}

	t_draw_list->push_clip(content);
	draw_text(t_draw_list, t_font, Vec2{origin_x, baseline}, shown, t_text_color);

	// A block caret shows the character under it in a contrasting colour, as a terminal does.
	if (caret_shown && g_caret_style == CaretStyle::BLOCK) {
		t_draw_list->push_clip(Rect{caret_origin.x + m_caret_head.x, caret_origin.y + m_caret_head.y, caret_extent.x, caret_extent.y});
		draw_text(t_draw_list, t_font, Vec2{origin_x, baseline}, shown, ink_on(t_caret_color));
		t_draw_list->pop_clip();
	}

	t_draw_list->pop_clip();
}

auto TextInput::selection() const -> TextRange
{
	return TextRange{std::min(m_anchor, m_cursor), std::max(m_anchor, m_cursor)};
}

auto TextInput::shown_text(char (&t_mask)[K_TEXT_INPUT_CAPACITY]) const -> std::string_view
{
	if (!m_masked) return value();

	std::memset(t_mask, '*', m_length);

	return std::string_view{t_mask, m_length};
}

auto TextInput::index_at(const Font& t_font, Rect t_field, float t_x) const -> u32
{
	char        mask[K_TEXT_INPUT_CAPACITY];
	const float origin_x = t_field.x + K_TEXT_PADDING - m_scroll_x;

	return text_index_at(t_font, shown_text(mask), t_x - origin_x);
}

auto TextInput::move_cursor(u32 t_index, bool t_extend_selection) -> void
{
	m_cursor = t_index;

	if (!t_extend_selection) {
		m_anchor = t_index;
	}
}

auto TextInput::select(TextRange t_range) -> void
{
	m_anchor = t_range.start;
	m_cursor = t_range.end;
}

auto TextInput::erase(TextRange t_range) -> void
{
	const u32 removed = t_range.end - t_range.start;

	std::memmove(m_text + t_range.start, m_text + t_range.end, m_length - t_range.end);
	m_length -= removed;
	std::memset(m_text + m_length, 0, removed);

	move_cursor(t_range.start, false);
}

auto TextInput::insert(std::string_view t_text) -> void
{
	erase(selection());

	char accepted[K_TEXT_INPUT_CAPACITY];
	u32  accepted_count = 0;

	for (usize index = 0; index < t_text.size();) {
		const Utf8Codepoint codepoint = decode_utf8(t_text, index);
		const bool          invalid   = codepoint.length == 1 && static_cast<u8>(t_text[index]) >= 0x80;
		index += codepoint.length;

		if (invalid || !is_printable(codepoint.value)) continue;
		if (m_length + accepted_count + codepoint.length > m_max_length) break;

		std::memcpy(accepted + accepted_count, t_text.data() + index - codepoint.length, codepoint.length);
		accepted_count += codepoint.length;
	}

	std::memmove(m_text + m_cursor + accepted_count, m_text + m_cursor, m_length - m_cursor);
	std::memcpy(m_text + m_cursor, accepted, accepted_count);
	m_length += accepted_count;

	move_cursor(m_cursor + accepted_count, false);
}

auto TextInput::caret_size(const Font& t_font, std::string_view t_shown, float t_height) const -> Vec2
{
	if (g_caret_style == CaretStyle::BAR) return Vec2{K_CARET_WIDTH, t_height};

	const usize next  = next_codepoint(t_shown, m_cursor);
	const float width = next > m_cursor ? text_width(t_font, t_shown.substr(m_cursor, next - m_cursor)) : text_width(t_font, K_END_OF_TEXT_CELL);

	return Vec2{std::max(width, K_CARET_WIDTH), g_caret_style == CaretStyle::BLOCK ? t_height : K_UNDERLINE_HEIGHT};
}

// The caret glides to its new spot: quickly for a move the size of typing, so it keeps up with the text, and more slowly for longer jumps.
// Its tail follows behind, and the stretch between them is drawn as a fading ribbon, the smear kitty and Neovide leave behind their
// cursors. A field that just took focus starts at the caret of the field that let go of focus, so the caret flies over.
auto TextInput::move_caret(Vec2 t_target, Vec2 t_origin, float t_line_height) -> void
{
	const float seconds = std::min(std::exchange(m_trail_seconds, 0.0f), K_TRAIL_MAX_STEP);

	if (!m_trail_placed) {
		const bool hops = g_caret_trail && m_focused && std::chrono::steady_clock::now() - g_last_caret.released_at < K_HOP_WINDOW;
		m_caret_head    = hops ? Vec2{g_last_caret.position.x - t_origin.x, g_last_caret.position.y - t_origin.y} : t_target;
		m_caret_tail    = m_caret_head;
		m_caret_target  = t_target;
		m_caret_rate    = K_CARET_JUMP_RATE;
		m_trail_placed  = true;
	}

	if (t_target.x != m_caret_target.x || t_target.y != m_caret_target.y) {
		const bool typing = t_target.y == m_caret_head.y && std::fabs(t_target.x - m_caret_head.x) <= t_line_height;
		m_caret_rate      = typing ? K_CARET_TYPING_RATE : K_CARET_JUMP_RATE;
		m_caret_target    = t_target;
	}

	m_caret_head.x = animation::ease_toward(m_caret_head.x, t_target.x, m_caret_rate, seconds, animation::K_SETTLED_PIXELS);
	m_caret_head.y = animation::ease_toward(m_caret_head.y, t_target.y, m_caret_rate, seconds, animation::K_SETTLED_PIXELS);

	if (g_caret_trail) {
		const float rate = K_TRAIL_TAIL_RATE_SHORT * std::pow(K_TRAIL_TAIL_RATE_LONG / K_TRAIL_TAIL_RATE_SHORT, g_trail_strength);

		m_caret_tail.x = animation::ease_toward(m_caret_tail.x, t_target.x, rate, seconds, animation::K_SETTLED_PIXELS);
		m_caret_tail.y = animation::ease_toward(m_caret_tail.y, t_target.y, rate, seconds, animation::K_SETTLED_PIXELS);
	} else {
		m_caret_tail = m_caret_head;
	}

	if (m_focused) {
		g_last_caret.position = Vec2{t_origin.x + m_caret_head.x, t_origin.y + m_caret_head.y};
	}
}

// The trail is the outline around the caret and a shrunken copy of it where the tail is: the area the caret sweeps through. The caret is
// that outline's front edge, so nothing pokes out of it whichever way the caret moves.
auto TextInput::draw_caret(DrawList* t_draw_list, Vec2 t_origin, Vec2 t_size, Color t_color) const -> void
{
	const Rect head{t_origin.x + m_caret_head.x, t_origin.y + m_caret_head.y, t_size.x, t_size.y};
	const Vec2 tail{t_origin.x + m_caret_tail.x + t_size.x * 0.5f, t_origin.y + m_caret_tail.y + t_size.y * 0.5f};

	if (std::hypot(head.center().x - tail.x, head.center().y - tail.y) > K_TRAIL_MIN_LENGTH) {
		const auto  alpha = static_cast<u8>(K_TRAIL_FAINT_ALPHA_MIN + (K_TRAIL_FAINT_ALPHA_MAX - K_TRAIL_FAINT_ALPHA_MIN) * g_trail_strength);
		const Color faint = faded(t_color, alpha);
		const Vec2  tail_half{t_size.x * 0.5f * K_TRAIL_TAIL_SPREAD, t_size.y * 0.5f * K_TRAIL_TAIL_SPREAD};

		TrailPoint corners[8]{
			{{head.x, head.y}, t_color},
			{{head.right(), head.y}, t_color},
			{{head.right(), head.bottom()}, t_color},
			{{head.x, head.bottom()}, t_color},
			{{tail.x - tail_half.x, tail.y - tail_half.y}, faint},
			{{tail.x + tail_half.x, tail.y - tail_half.y}, faint},
			{{tail.x + tail_half.x, tail.y + tail_half.y}, faint},
			{{tail.x - tail_half.x, tail.y + tail_half.y}, faint},
		};

		TrailPoint outline[16]{};
		const u32  count = convex_hull(corners, outline);

		Vec2  points[16]{};
		Color colors[16]{};
		for (u32 i = 0; i < count; i += 1) {
			points[i] = outline[i].position;
			colors[i] = outline[i].color;
		}

		t_draw_list->add_convex_polygon(std::span{points, count}, std::span{colors, count});
	}

	t_draw_list->add_rect(head, t_color);
}

auto TextInput::restart_caret_blink() -> void
{
	m_caret_blink_seconds = 0.0f;
}
