#include "core/text_editor.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>
#include <optional>

#include "core/str.h"
#include "os/clipboard.h"

namespace {
constexpr float K_MULTI_CLICK_SLOP = 4.0f;
constexpr u32   K_CLICKS_PER_CYCLE = 3;

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

auto TextEditor::set_value(std::string_view t_value) -> void
{
	std::memset(m_text, 0, sizeof(m_text));

	usize length = std::min<usize>(t_value.size(), m_max_length);
	while (length > 0 && length < t_value.size() && is_utf8_continuation(t_value[length])) {
		length -= 1;
	}

	m_length = static_cast<u32>(length);
	std::memcpy(m_text, t_value.data(), m_length);

	move_cursor(m_length, false);
}

auto TextEditor::set_max_length(u32 t_max_length) -> void
{
	m_max_length = std::min(t_max_length, K_TEXT_INPUT_CAPACITY);
}

auto TextEditor::selection() const -> TextRange
{
	return TextRange{std::min(m_anchor, m_cursor), std::max(m_anchor, m_cursor)};
}

auto TextEditor::shown_text(char (&t_mask)[K_TEXT_INPUT_CAPACITY]) const -> std::string_view
{
	if (!m_masked) return value();

	std::memset(t_mask, '*', m_length);

	return std::string_view{t_mask, m_length};
}

auto TextEditor::can_apply(TextEdit t_edit) const -> bool
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

auto TextEditor::edit(TextEdit t_edit) -> bool
{
	if (!can_apply(t_edit)) return false;

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

	return true;
}

auto TextEditor::type(u32 t_codepoint) -> bool
{
	if (!is_printable(t_codepoint)) return false;

	char      encoded[4];
	const u32 length = encode_utf8(t_codepoint, encoded);
	insert(std::string_view{encoded, length});

	return true;
}

auto TextEditor::press_key(os::Key t_key, os::Modifiers t_held) -> bool
{
	if (const std::optional<TextEdit> shortcut = t_held.shortcut ? shortcut_for(t_key) : std::nullopt) {
		edit(*shortcut);
		return true;
	}

	char                   mask[K_TEXT_INPUT_CAPACITY];
	const std::string_view shown = shown_text(mask);
	const TextRange        range = selection();

	const u32  previous           = t_held.word_step ? previous_word_start(shown, m_cursor) : static_cast<u32>(previous_codepoint(value(), m_cursor));
	const u32  next               = t_held.word_step ? next_word_start(shown, m_cursor) : static_cast<u32>(next_codepoint(value(), m_cursor));
	const bool collapse_selection = has_selection() && !t_held.shift;

	switch (t_key) {
		using enum os::Key;

		case BACKSPACE: {
			erase(has_selection() ? range : TextRange{previous, m_cursor});
			return true;
		}

		case FORWARD_DELETE: {
			erase(has_selection() ? range : TextRange{m_cursor, next});
			return true;
		}

		case LEFT: {
			move_cursor(collapse_selection ? range.start : previous, t_held.shift);
			return true;
		}

		case RIGHT: {
			move_cursor(collapse_selection ? range.end : next, t_held.shift);
			return true;
		}

		case HOME: {
			move_cursor(0, t_held.shift);
			return true;
		}

		case END: {
			move_cursor(m_length, t_held.shift);
			return true;
		}

		case E: {
			if (!t_held.shortcut) return false;

			move_cursor(m_length, false);
			return true;
		}

		default: {
			return false;
		}
	}
}

auto TextEditor::press_at(u32 t_index, float t_x) -> void
{
	const u64  now_ms       = milliseconds_now();
	const bool quick_repeat = now_ms - m_last_click_ms <= os::double_click_ms();
	const bool same_spot    = std::fabs(t_x - m_last_click_x) <= K_MULTI_CLICK_SLOP;

	m_click_count   = quick_repeat && same_spot ? m_click_count % K_CLICKS_PER_CYCLE + 1 : 1;
	m_last_click_ms = now_ms;
	m_last_click_x  = t_x;

	char mask[K_TEXT_INPUT_CAPACITY];

	if (m_click_count == 1) {
		move_cursor(t_index, os::modifiers().shift);
	} else if (m_click_count == 2) {
		select(word_at(shown_text(mask), t_index));
	} else {
		select(TextRange{0, m_length});
	}

	m_selecting = true;
}

auto TextEditor::drag_to(u32 t_index) -> bool
{
	if (!m_selecting || m_click_count != 1) return false;

	m_cursor = t_index;

	return true;
}

auto TextEditor::release() -> void
{
	m_selecting = false;
}

auto TextEditor::right_click_at(u32 t_index) -> void
{
	const TextRange range = selection();

	if (t_index < range.start || t_index > range.end) {
		move_cursor(t_index, false);
	}
}

auto TextEditor::move_cursor(u32 t_index, bool t_extend_selection) -> void
{
	m_cursor = t_index;

	if (!t_extend_selection) {
		m_anchor = t_index;
	}
}

auto TextEditor::select(TextRange t_range) -> void
{
	m_anchor = t_range.start;
	m_cursor = t_range.end;
}

auto TextEditor::erase(TextRange t_range) -> void
{
	const u32 removed = t_range.end - t_range.start;

	std::memmove(m_text + t_range.start, m_text + t_range.end, m_length - t_range.end);
	m_length -= removed;
	std::memset(m_text + m_length, 0, removed);

	move_cursor(t_range.start, false);
}

auto TextEditor::insert(std::string_view t_text) -> void
{
	erase(selection());

	char accepted[K_TEXT_INPUT_CAPACITY];
	u32  accepted_count = 0;

	for (usize index = 0; index < t_text.size();) {
		const Utf8Codepoint    codepoint = decode_utf8(t_text, index);
		const std::string_view bytes     = t_text.substr(index, codepoint.length);
		const bool             invalid   = codepoint.length == 1 && static_cast<u8>(bytes[0]) >= 0x80;
		index += codepoint.length;

		if (invalid || !is_printable(codepoint.value)) continue;
		if (m_length + accepted_count + codepoint.length > m_max_length) break;

		std::ranges::copy(bytes, accepted + accepted_count);
		accepted_count += codepoint.length;
	}

	std::memmove(m_text + m_cursor + accepted_count, m_text + m_cursor, m_length - m_cursor);
	std::memcpy(m_text + m_cursor, accepted, accepted_count);
	m_length += accepted_count;

	move_cursor(m_cursor + accepted_count, false);
}
