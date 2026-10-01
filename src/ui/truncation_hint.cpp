#include "ui/truncation_hint.h"

#include <algorithm>
#include <cstring>
#include <optional>

#include "core/animation.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "ui/controls.h"
#include "ui/text.h"
#include "ui/theme.h"

namespace {
constexpr float show_delay_seconds = 0.45f;
constexpr float fade_rate = 20.0f;
constexpr float padding_x = 10.0f;
constexpr float padding_y = 7.0f;
constexpr float corner_radius = 8.0f;
constexpr float anchor_gap = 6.0f;
constexpr float edge_margin = 8.0f;
constexpr float max_width = 360.0f;
constexpr float rise_distance = 4.0f;
constexpr u32 max_lines = 8;

u32 layout_lines(const Font &t_font, std::string_view t_text, float t_max_width, std::string_view (&t_lines)[max_lines])
{
	std::string_view wrapped[max_lines];
	const u32 wrapped_count = wrap_text(t_font, t_text, t_max_width, wrapped);
	u32 count = 0;

	for (u32 i = 0; i < wrapped_count && count < max_lines; i += 1) {
		std::string_view rest = wrapped[i];

		while (count < max_lines) {
			usize fitting = rest.size();
			while (fitting > 1 && text_width(t_font, rest.substr(0, fitting)) > t_max_width) {
				fitting -= 1;
			}

			t_lines[count] = rest.substr(0, fitting);
			count += 1;
			rest.remove_prefix(fitting);

			if (rest.empty()) break;
		}
	}

	return count;
}
}

TruncationHint::TruncationHint(const Fonts &t_fonts)
	: m_fonts(t_fonts)
{
}

void TruncationHint::capture(const DrawList &t_draw_list)
{
	const std::optional<TruncatedText> hovered = hovered_truncated_text(t_draw_list);
	m_requested = hovered.has_value();
	if (!hovered) return;

	if (hovered->text != std::string_view{m_text, m_length}) {
		m_length = std::min(hovered->text.size(), sizeof(m_text));
		std::memcpy(m_text, hovered->text.data(), m_length);

		if (m_visible_amount <= 0.01f) {
			m_hover_seconds = 0.0f;
		}
	}

	m_anchor = hovered->bounds;
}

void TruncationHint::update(float t_delta_seconds, bool t_suppressed)
{
	const bool requested = m_requested && !t_suppressed;
	m_hover_seconds = requested ? m_hover_seconds + t_delta_seconds : 0.0f;

	const bool showing = requested && m_hover_seconds >= show_delay_seconds;
	if (requested && !showing) {
		animation::request_frame_after(show_delay_seconds - m_hover_seconds);
	}

	m_visible_amount = animation::ease_toward(m_visible_amount, showing ? 1.0f : 0.0f, fade_rate, t_delta_seconds);
}

void TruncationHint::draw(DrawList &t_draw_list, Rect t_bounds) const
{
	if (m_visible_amount <= 0.001f || m_length == 0) return;

	const Font &font = m_fonts.secondary;
	const float wrap_width = std::max(40.0f, std::min(max_width, t_bounds.w - (edge_margin + padding_x) * 2.0f));

	std::string_view lines[max_lines];
	const u32 line_count = std::max(1u, layout_lines(font, std::string_view{m_text, m_length}, wrap_width, lines));

	float widest = 0.0f;
	for (u32 i = 0; i < line_count; i += 1) {
		widest = std::max(widest, text_width(font, lines[i]));
	}

	const float width = widest + padding_x * 2.0f;
	const float height = line_count * font.line_height() + padding_y * 2.0f;
	const float rise = snapped_to_pixel((1.0f - m_visible_amount) * rise_distance);

	float y = m_anchor.bottom() + anchor_gap + rise;
	if (y + height > t_bounds.bottom() - edge_margin) {
		y = m_anchor.y - anchor_gap - height - rise;
	}

	const float min_x = t_bounds.x + edge_margin;
	const float max_x = std::max(min_x, t_bounds.right() - edge_margin - width);
	const Rect card{snapped_to_pixel(std::clamp(m_anchor.x - padding_x, min_x, max_x)), snapped_to_pixel(y), width,
					height};
	const auto alpha = to_alpha(m_visible_amount);

	controls::draw_popup_shadow(t_draw_list, card, corner_radius, m_visible_amount);
	t_draw_list.add_bordered_rect(card, rounded(corner_radius), faded(theme().popup, alpha),
								  faded(theme().border, alpha), 1.0f);

	for (u32 i = 0; i < line_count; i += 1) {
		draw_text(t_draw_list, font,
				  Vec2{card.x + padding_x, card.y + padding_y + font.ascent + i * font.line_height()}, lines[i],
				  faded(theme().text, alpha));
	}
}
