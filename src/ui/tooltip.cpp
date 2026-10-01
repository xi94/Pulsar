#include "ui/tooltip.h"

#include <algorithm>
#include <cstring>

#include "core/animation.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "ui/text.h"
#include "ui/theme.h"

namespace {
constexpr float show_delay_seconds = 0.35f;
constexpr float fade_rate = 22.0f;
constexpr float padding_x = 10.0f;
constexpr float padding_y = 6.0f;
constexpr float corner_radius = 6.0f;
constexpr float anchor_gap = 8.0f;
constexpr float edge_margin = 6.0f;
constexpr float rise_distance = 4.0f;
}

void Tooltip::request(std::string_view t_text, Rect t_anchor)
{
	const std::string_view current{m_text, m_length};

	if (t_text != current) {
		m_length = std::min(t_text.size(), sizeof(m_text) - 1);
		std::memcpy(m_text, t_text.data(), m_length);

		if (m_visible_amount <= 0.01f) {
			m_hover_seconds = 0.0f;
		}
	}

	m_anchor = t_anchor;
	m_requested_this_frame = true;
}

void Tooltip::update(float t_delta_seconds)
{
	m_hover_seconds = m_requested_this_frame ? m_hover_seconds + t_delta_seconds : 0.0f;

	const bool showing = m_requested_this_frame && m_hover_seconds >= show_delay_seconds;
	if (m_requested_this_frame && !showing) {
		animation::request_frame_after(show_delay_seconds - m_hover_seconds);
	}

	m_visible_amount = animation::ease_toward(m_visible_amount, showing ? 1.0f : 0.0f, fade_rate, t_delta_seconds);

	m_requested_this_frame = false;
}

void Tooltip::reset()
{
	*this = Tooltip{};
}

void Tooltip::draw(DrawList *t_draw_list, const Fonts *t_fonts, Rect t_bounds, u8 t_alpha) const
{
	if (m_visible_amount <= 0.001f || m_length == 0) return;

	const Font &font = t_fonts->secondary;
	const std::string_view text{m_text, m_length};

	const float width = text_width(font, text) + padding_x * 2.0f;
	const float height = font.line_height() + padding_y * 2.0f;
	const float rise = snapped_to_pixel((1.0f - m_visible_amount) * rise_distance);

	float y = m_anchor.y - height - anchor_gap + rise;
	if (y < t_bounds.y + edge_margin) {
		y = m_anchor.bottom() + anchor_gap - rise;
	}

	const float min_x = t_bounds.x + edge_margin;
	const float max_x = std::max(min_x, t_bounds.right() - edge_margin - width);
	const Rect bubble{std::clamp(m_anchor.x + (m_anchor.w - width) * 0.5f, min_x, max_x), y, width, height};

	const auto alpha = static_cast<u8>(t_alpha * m_visible_amount);

	t_draw_list->add_bordered_rect(bubble, rounded(corner_radius), faded(theme().popup, alpha), faded(theme().border, alpha), 1.0f);
	draw_text(t_draw_list, font, Vec2{bubble.x + padding_x, font.centered_baseline(bubble)}, text, faded(theme().text, alpha));
}
