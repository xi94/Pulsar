#include "ui/text_input.h"

#include <algorithm>
#include <cmath>
#include <span>
#include <utility>

#include "core/animation.h"
#include "core/str.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "ui/controls.h"
#include "ui/text.h"
#include "ui/theme.h"

namespace {
constexpr float K_CARET_BLINK_PERIOD = 1.0f;
constexpr float K_CARET_WIDTH        = 1.5f;
constexpr float K_TEXT_PADDING       = 8.0f;
constexpr float K_HIGHLIGHT_INSET    = 4.0f;
constexpr float K_CARET_OVERHANG     = 2.0f;
constexpr u8    K_SELECTION_ALPHA    = 70;

constexpr float            K_UNDERLINE_HEIGHT = 2.0f;
constexpr std::string_view K_END_OF_TEXT_CELL = "0";
}

auto TextInput::set_value(std::string_view t_value) -> void
{
	TextEditor::set_value(t_value);
	m_scroll_x = 0.0f;
	restart_caret_blink();
}

auto TextInput::set_focused(bool t_focused) -> void
{
	if (t_focused && !m_focused) {
		m_caret.restart();
		restart_caret_blink();
	} else if (!t_focused && m_focused) {
		m_caret.let_go();
	}

	m_focused = t_focused;
}

auto TextInput::apply(TextEdit t_edit) -> void
{
	if (edit(t_edit)) {
		restart_caret_blink();
	}
}

auto TextInput::on_char(u32 t_codepoint) -> void
{
	if (m_focused && type(t_codepoint)) {
		restart_caret_blink();
	}
}

auto TextInput::on_key_down(os::Key t_key, os::Modifiers t_held) -> void
{
	if (m_focused && press_key(t_key, t_held)) {
		restart_caret_blink();
	}
}

auto TextInput::on_pointer_down(const Font& t_font, Rect t_field, float t_x) -> void
{
	press_at(index_at(t_font, t_field, t_x), t_x);
	restart_caret_blink();
}

auto TextInput::on_pointer_move(const Font& t_font, Rect t_field, float t_x) -> void
{
	if (m_selecting && drag_to(index_at(t_font, t_field, t_x))) {
		restart_caret_blink();
	}
}

auto TextInput::on_pointer_up() -> void
{
	release();
}

auto TextInput::on_right_click(const Font& t_font, Rect t_field, float t_x) -> void
{
	right_click_at(index_at(t_font, t_field, t_x));
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

auto TextInput::draw(DrawList* t_draw_list, const Font& t_font, Rect t_field, Color t_text_color, Color t_caret_color, std::optional<Rect> t_placeholder_box) -> void
{
	char                   mask[K_TEXT_INPUT_CAPACITY];
	const std::string_view shown = shown_text(mask);

	const Rect  content       = t_field.inset(K_TEXT_PADDING, 0.0f);
	const float visible_width = std::max(0.0f, content.w - K_CARET_WIDTH);
	const float caret_offset  = text_width(t_font, shown.substr(0, cursor()));

	m_scroll_x = std::clamp(m_scroll_x, caret_offset - visible_width, caret_offset);
	m_scroll_x = std::clamp(m_scroll_x, 0.0f, std::max(0.0f, text_width(t_font, shown) - visible_width));

	const float origin_x         = content.x - m_scroll_x;
	const float baseline         = t_font.centered_baseline(t_field);
	const float highlight_y      = std::max(t_field.y + K_HIGHLIGHT_INSET, baseline - t_font.ascent - K_CARET_OVERHANG);
	const float highlight_bottom = std::min(t_field.bottom() - K_HIGHLIGHT_INSET, baseline - t_font.descent + K_CARET_OVERHANG);
	const float highlight_height = std::max(0.0f, highlight_bottom - highlight_y);
	const Vec2  caret_origin{content.x, highlight_y};
	const Vec2  caret_extent = caret_size(t_font, shown, highlight_height);
	const Vec2  caret_target{caret_offset - m_scroll_x, highlight_height - caret_extent.y};
	const Vec2  text_origin{origin_x, baseline};

	m_caret.follow(caret_target, caret_origin, highlight_height, std::exchange(m_trail_seconds, 0.0f), m_focused);

	t_draw_list->push_clip(content);

	if (m_focused && has_selection()) {
		const TextRange range   = selection();
		const float     start_x = origin_x + text_width(t_font, shown.substr(0, range.start));
		const float     end_x   = origin_x + text_width(t_font, shown.substr(0, range.end));
		const Rect      highlight{start_x, highlight_y, end_x - start_x, highlight_height};

		t_draw_list->add_rect(highlight, faded(t_caret_color, K_SELECTION_ALPHA));
	}

	if (shown.empty()) {
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
	draw_text(t_draw_list, t_font, text_origin, shown, t_text_color);

	// A block caret shows the character under it in a contrasting colour, as a terminal does.
	if (caret_shown && caret_style() == CaretStyle::BLOCK) {
		const Rect under_caret{caret_origin.x + m_caret.head().x, caret_origin.y + m_caret.head().y, caret_extent.x, caret_extent.y};
		t_draw_list->push_clip(under_caret);
		draw_text(t_draw_list, t_font, text_origin, shown, controls::contrasting_theme_text(t_caret_color));
		t_draw_list->pop_clip();
	}

	t_draw_list->pop_clip();
}

auto TextInput::index_at(const Font& t_font, Rect t_field, float t_x) const -> u32
{
	char        mask[K_TEXT_INPUT_CAPACITY];
	const float origin_x = t_field.x + K_TEXT_PADDING - m_scroll_x;

	return text_index_at(t_font, shown_text(mask), t_x - origin_x);
}

auto TextInput::caret_size(const Font& t_font, std::string_view t_shown, float t_height) const -> Vec2
{
	if (caret_style() == CaretStyle::BAR) return Vec2{K_CARET_WIDTH, t_height};

	const usize next  = next_codepoint(t_shown, cursor());
	const float width = next > cursor() ? text_width(t_font, t_shown.substr(cursor(), next - cursor())) : text_width(t_font, K_END_OF_TEXT_CELL);

	return Vec2{std::max(width, K_CARET_WIDTH), caret_style() == CaretStyle::BLOCK ? t_height : K_UNDERLINE_HEIGHT};
}

auto TextInput::draw_caret(DrawList* t_draw_list, Vec2 t_origin, Vec2 t_size, Color t_color) const -> void
{
	const CaretTrail trail = m_caret.trail(t_origin, t_size, t_color);

	if (trail.count > 0) {
		t_draw_list->add_convex_polygon(std::span{trail.points, trail.count}, std::span{trail.colors, trail.count});
	}

	const Rect head{t_origin.x + m_caret.head().x, t_origin.y + m_caret.head().y, t_size.x, t_size.y};
	t_draw_list->add_rect(head, t_color);
}

auto TextInput::restart_caret_blink() -> void
{
	m_caret_blink_seconds = 0.0f;
}
