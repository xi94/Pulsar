#include "ui/text_input.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>
#include <optional>

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

enum class CharClass : u8 {
	Space,
	Word,
	Symbol,
};

[[nodiscard]] auto class_of(char t_character) -> CharClass
{
	if (t_character == ' ') return CharClass::Space;
	if (static_cast<u8>(t_character) >= 0x80) return CharClass::Word;
	if (std::isalnum(static_cast<unsigned char>(t_character)) || t_character == '_') return CharClass::Word;

	return CharClass::Symbol;
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
		case os::Key::A:
			return TextEdit::SelectAll;
		case os::Key::C:
			return TextEdit::Copy;
		case os::Key::X:
			return TextEdit::Cut;
		case os::Key::V:
			return TextEdit::Paste;
		default:
			return std::nullopt;
	}
}

[[nodiscard]] auto previous_word_start(std::string_view t_text, u32 t_index) -> u32
{
	while (t_index > 0 && class_of(t_text[t_index - 1]) == CharClass::Space) {
		t_index -= 1;
	}

	const CharClass word = t_index > 0 ? class_of(t_text[t_index - 1]) : CharClass::Space;
	while (t_index > 0 && class_of(t_text[t_index - 1]) == word) {
		t_index -= 1;
	}

	return t_index;
}

[[nodiscard]] auto next_word_start(std::string_view t_text, u32 t_index) -> u32
{
	const auto length = static_cast<u32>(t_text.size());

	const CharClass word = t_index < length ? class_of(t_text[t_index]) : CharClass::Space;
	while (t_index < length && class_of(t_text[t_index]) == word) {
		t_index += 1;
	}

	while (t_index < length && class_of(t_text[t_index]) == CharClass::Space) {
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
		case TextEdit::Cut:
		case TextEdit::Copy:
			return has_selection() && !m_masked;
		case TextEdit::Paste:
			return os::clipboard_has_text();
		case TextEdit::SelectAll:
			return m_length > 0;
	}

	return false;
}

auto TextInput::apply(TextEdit t_edit) -> void
{
	if (!can_apply(t_edit)) return;

	const TextRange        range = selection();
	const std::string_view selected{m_text + range.start, range.end - range.start};

	switch (t_edit) {
		case TextEdit::Cut:
			os::set_clipboard_text(selected);
			erase(range);
			break;

		case TextEdit::Copy:
			os::set_clipboard_text(selected);
			break;

		case TextEdit::Paste:
			insert(os::clipboard_text());
			break;

		case TextEdit::SelectAll:
			select(TextRange{0, m_length});
			break;
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
		case os::Key::Backspace:
			erase(has_selection() ? range : TextRange{previous, m_cursor});
			break;

		case os::Key::Delete:
			erase(has_selection() ? range : TextRange{m_cursor, next});
			break;

		case os::Key::Left:
			move_cursor(collapse_selection ? range.start : previous, held.shift);
			break;

		case os::Key::Right:
			move_cursor(collapse_selection ? range.end : next, held.shift);
			break;

		case os::Key::Home:
			move_cursor(0, held.shift);
			break;

		case os::Key::End:
			move_cursor(m_length, held.shift);
			break;

		case os::Key::E:
			if (!held.shortcut) return;

			move_cursor(m_length, false);
			break;

		default:
			return;
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

auto TextInput::update(float t_delta_seconds) -> void
{
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

	draw_text(t_draw_list, t_font, Vec2{origin_x, baseline}, shown, t_text_color);

	if (m_focused && m_caret_blink_seconds < K_CARET_BLINK_PERIOD * 0.5f) {
		t_draw_list->add_rect(Rect{origin_x + caret_offset, highlight_y, K_CARET_WIDTH, highlight_height}, t_caret_color);
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

auto TextInput::restart_caret_blink() -> void
{
	m_caret_blink_seconds = 0.0f;
}
