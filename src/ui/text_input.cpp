#include "ui/text_input.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <optional>

#include <Windows.h>

#include "core/animation.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "platform/clipboard.h"
#include "platform/window.h"
#include "ui/text.h"
#include "ui/theme.h"

namespace {
constexpr float caret_blink_period = 1.0f;
constexpr float caret_width = 1.5f;
constexpr float text_padding = 8.0f;
constexpr float highlight_inset = 4.0f;
constexpr u8 selection_alpha = 70;
constexpr float multi_click_slop = 4.0f;
constexpr u32 clicks_per_cycle = 3;

enum class CharClass : u8 {
	Space,
	Word,
	Symbol,
};

CharClass class_of(char t_character)
{
	if (t_character == ' ') return CharClass::Space;
	if (std::isalnum(static_cast<unsigned char>(t_character)) || t_character == '_') return CharClass::Word;

	return CharClass::Symbol;
}

bool is_printable(u32 t_character)
{
	return t_character >= 0x20 && t_character <= 0x7E;
}

std::optional<TextEdit> shortcut_for(u32 t_key)
{
	switch (t_key) {
		case 'A':
			return TextEdit::SelectAll;
		case 'C':
			return TextEdit::Copy;
		case 'X':
			return TextEdit::Cut;
		case 'V':
			return TextEdit::Paste;
		default:
			return std::nullopt;
	}
}

u32 previous_word_start(std::string_view t_text, u32 t_index)
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

u32 next_word_start(std::string_view t_text, u32 t_index)
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

TextRange word_at(std::string_view t_text, u32 t_index)
{
	if (t_text.empty()) return TextRange{0, 0};

	const auto length = static_cast<u32>(t_text.size());
	const u32 inside = std::min(t_index, length - 1);
	const CharClass word = class_of(t_text[inside]);

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

void TextInput::set_value(std::string_view t_value)
{
	std::memset(m_text, 0, sizeof(m_text));

	m_length = static_cast<u32>(std::min<usize>(t_value.size(), m_max_length));
	std::memcpy(m_text, t_value.data(), m_length);

	move_cursor(m_length, false);
	m_scroll_x = 0.0f;
	restart_caret_blink();
}

void TextInput::set_max_length(u32 t_max_length)
{
	m_max_length = std::min(t_max_length, text_input_capacity);
}

bool TextInput::can_apply(TextEdit t_edit) const
{
	switch (t_edit) {
		case TextEdit::Cut:
		case TextEdit::Copy:
			return has_selection() && !m_masked;
		case TextEdit::Paste:
			return clipboard_has_text();
		case TextEdit::SelectAll:
			return m_length > 0;
	}

	return false;
}

void TextInput::apply(TextEdit t_edit)
{
	if (!can_apply(t_edit)) return;

	const TextRange range = selection();
	const std::string_view selected{m_text + range.start, range.end - range.start};

	switch (t_edit) {
		case TextEdit::Cut:
			set_clipboard_text(selected);
			erase(range);
			break;

		case TextEdit::Copy:
			set_clipboard_text(selected);
			break;

		case TextEdit::Paste:
			insert(clipboard_text());
			break;

		case TextEdit::SelectAll:
			select(TextRange{0, m_length});
			break;
	}

	restart_caret_blink();
}

void TextInput::on_char(u32 t_character)
{
	if (!m_focused || !is_printable(t_character)) return;

	const char typed = static_cast<char>(t_character);
	insert(std::string_view{&typed, 1});
	restart_caret_blink();
}

void TextInput::on_key_down(u32 t_key)
{
	if (!m_focused) return;

	const bool control = is_key_down(VK_CONTROL);
	const bool shift = is_key_down(VK_SHIFT);

	if (const std::optional<TextEdit> edit = control ? shortcut_for(t_key) : std::nullopt) {
		apply(*edit);
		return;
	}

	char mask[text_input_capacity];
	const std::string_view shown = shown_text(mask);
	const TextRange range = selection();

	const u32 previous = control ? previous_word_start(shown, m_cursor) : (m_cursor > 0 ? m_cursor - 1 : 0);
	const u32 next = control ? next_word_start(shown, m_cursor) : std::min(m_cursor + 1, m_length);
	const bool collapse_selection = has_selection() && !shift;

	switch (t_key) {
		case VK_BACK:
			erase(has_selection() ? range : TextRange{previous, m_cursor});
			break;

		case VK_DELETE:
			erase(has_selection() ? range : TextRange{m_cursor, next});
			break;

		case VK_LEFT:
			move_cursor(collapse_selection ? range.start : previous, shift);
			break;

		case VK_RIGHT:
			move_cursor(collapse_selection ? range.end : next, shift);
			break;

		case VK_HOME:
			move_cursor(0, shift);
			break;

		case VK_END:
			move_cursor(m_length, shift);
			break;

		case 'E':
			if (!control) return;

			move_cursor(m_length, false);
			break;

		default:
			return;
	}

	restart_caret_blink();
}

void TextInput::on_pointer_down(const Font &t_font, Rect t_field, float t_x)
{
	const u64 now_ms = GetTickCount64();
	const bool quick_repeat = now_ms - m_last_click_ms <= GetDoubleClickTime();
	const bool same_spot = std::fabs(t_x - m_last_click_x) <= multi_click_slop;

	m_click_count = quick_repeat && same_spot ? m_click_count % clicks_per_cycle + 1 : 1;
	m_last_click_ms = now_ms;
	m_last_click_x = t_x;

	char mask[text_input_capacity];
	const u32 index = index_at(t_font, t_field, t_x);

	if (m_click_count == 1) {
		move_cursor(index, is_key_down(VK_SHIFT));
	} else if (m_click_count == 2) {
		select(word_at(shown_text(mask), index));
	} else {
		select(TextRange{0, m_length});
	}

	m_selecting = true;
	restart_caret_blink();
}

void TextInput::on_pointer_move(const Font &t_font, Rect t_field, float t_x)
{
	if (!m_selecting || m_click_count != 1) return;

	m_cursor = index_at(t_font, t_field, t_x);
	restart_caret_blink();
}

void TextInput::on_pointer_up()
{
	m_selecting = false;
}

void TextInput::on_right_click(const Font &t_font, Rect t_field, float t_x)
{
	const u32 index = index_at(t_font, t_field, t_x);
	const TextRange range = selection();

	if (index < range.start || index > range.end) {
		move_cursor(index, false);
	}
}

void TextInput::update(float t_delta_seconds)
{
	m_caret_blink_seconds = std::fmod(m_caret_blink_seconds + t_delta_seconds, caret_blink_period);

	if (m_focused) {
		const float half_period = caret_blink_period * 0.5f;
		const float next_toggle = m_caret_blink_seconds < half_period ? half_period - m_caret_blink_seconds : caret_blink_period - m_caret_blink_seconds;
		animation::request_frame_after(next_toggle);
	}
}

void TextInput::draw(DrawList *t_draw_list, const Font &t_font, Rect t_field, Color t_text_color, Color t_caret_color, std::optional<Rect> t_placeholder_box)
{
	char mask[text_input_capacity];
	const std::string_view shown = shown_text(mask);

	const Rect content = t_field.inset(text_padding, 0.0f);
	const float visible_width = std::max(0.0f, content.w - caret_width);
	const float caret_offset = text_width(t_font, shown.substr(0, m_cursor));

	m_scroll_x = std::clamp(m_scroll_x, caret_offset - visible_width, caret_offset);
	m_scroll_x = std::clamp(m_scroll_x, 0.0f, std::max(0.0f, text_width(t_font, shown) - visible_width));

	const float origin_x = content.x - m_scroll_x;
	const float highlight_y = t_field.y + highlight_inset;
	const float highlight_height = t_field.h - highlight_inset * 2.0f;

	t_draw_list->push_clip(content);

	if (m_focused && has_selection()) {
		const TextRange range = selection();
		const float start_x = origin_x + text_width(t_font, shown.substr(0, range.start));
		const float end_x = origin_x + text_width(t_font, shown.substr(0, range.end));

		t_draw_list->add_rect(Rect{start_x, highlight_y, end_x - start_x, highlight_height}, faded(t_caret_color, selection_alpha));
	}

	const float baseline = t_font.centered_baseline(t_field);

	if (m_length == 0) {
		const float placeholder_x = t_placeholder_box ? t_placeholder_box->center().x - text_width(t_font, m_placeholder) * 0.5f : content.x;
		draw_text(t_draw_list, t_font, Vec2{placeholder_x, baseline}, m_placeholder, faded(theme().text_faint, t_text_color.a));
	}

	draw_text(t_draw_list, t_font, Vec2{origin_x, baseline}, shown, t_text_color);

	if (m_focused && m_caret_blink_seconds < caret_blink_period * 0.5f) {
		t_draw_list->add_rect(Rect{origin_x + caret_offset, highlight_y, caret_width, highlight_height}, t_caret_color);
	}

	t_draw_list->pop_clip();
}

TextRange TextInput::selection() const
{
	return TextRange{std::min(m_anchor, m_cursor), std::max(m_anchor, m_cursor)};
}

std::string_view TextInput::shown_text(char (&t_mask)[text_input_capacity]) const
{
	if (!m_masked) return value();

	std::memset(t_mask, '*', m_length);

	return std::string_view{t_mask, m_length};
}

u32 TextInput::index_at(const Font &t_font, Rect t_field, float t_x) const
{
	char mask[text_input_capacity];
	const float origin_x = t_field.x + text_padding - m_scroll_x;

	return text_index_at(t_font, shown_text(mask), t_x - origin_x);
}

void TextInput::move_cursor(u32 t_index, bool t_extend_selection)
{
	m_cursor = t_index;

	if (!t_extend_selection) {
		m_anchor = t_index;
	}
}

void TextInput::select(TextRange t_range)
{
	m_anchor = t_range.start;
	m_cursor = t_range.end;
}

void TextInput::erase(TextRange t_range)
{
	const u32 removed = t_range.end - t_range.start;

	std::memmove(m_text + t_range.start, m_text + t_range.end, m_length - t_range.end);
	m_length -= removed;
	std::memset(m_text + m_length, 0, removed);

	move_cursor(t_range.start, false);
}

void TextInput::insert(std::string_view t_text)
{
	erase(selection());

	char accepted[text_input_capacity];
	u32 accepted_count = 0;

	for (const char character : t_text) {
		if (m_length + accepted_count >= m_max_length) break;

		if (is_printable(static_cast<unsigned char>(character))) {
			accepted[accepted_count] = character;
			accepted_count += 1;
		}
	}

	std::memmove(m_text + m_cursor + accepted_count, m_text + m_cursor, m_length - m_cursor);
	std::memcpy(m_text + m_cursor, accepted, accepted_count);
	m_length += accepted_count;

	move_cursor(m_cursor + accepted_count, false);
}

void TextInput::restart_caret_blink()
{
	m_caret_blink_seconds = 0.0f;
}
