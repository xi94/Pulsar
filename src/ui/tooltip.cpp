#include "ui/tooltip.h"

#include <algorithm>
#include <cstring>

#include "core/animation.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "ui/text.h"
#include "ui/theme.h"

namespace {
constexpr float K_SHOW_DELAY_SECONDS = 0.35f;
constexpr float K_FADE_RATE          = 22.0f;
constexpr float K_PADDING_X          = 10.0f;
constexpr float K_PADDING_Y          = 6.0f;
constexpr float K_CORNER_RADIUS      = 6.0f;
constexpr float K_ANCHOR_GAP         = 8.0f;
constexpr float K_EDGE_MARGIN        = 6.0f;
constexpr float K_RISE_DISTANCE      = 4.0f;
}

auto Tooltip::request(std::string_view t_text, Rect t_anchor) -> void
{
	const std::string_view current{m_text, m_length};

	if (t_text != current) {
		m_length = std::min(t_text.size(), sizeof(m_text) - 1);
		std::memcpy(m_text, t_text.data(), m_length);

		if (m_visible_amount <= 0.01f) {
			m_hover_seconds = 0.0f;
		}
	}

	m_anchor               = t_anchor;
	m_requested_this_frame = true;
}

auto Tooltip::update(float t_delta_seconds) -> void
{
	m_hover_seconds = m_requested_this_frame ? m_hover_seconds + t_delta_seconds : 0.0f;

	const bool showing = m_requested_this_frame && m_hover_seconds >= K_SHOW_DELAY_SECONDS;
	if (m_requested_this_frame && !showing) {
		animation::request_frame_after(K_SHOW_DELAY_SECONDS - m_hover_seconds);
	}

	m_visible_amount = animation::ease_toward(m_visible_amount, showing ? 1.0f : 0.0f, K_FADE_RATE, t_delta_seconds);

	m_requested_this_frame = false;
}

auto Tooltip::reset() -> void
{
	*this = Tooltip{};
}

auto Tooltip::draw(DrawList* t_draw_list, const Fonts* t_fonts, Rect t_bounds, u8 t_alpha) const -> void
{
	if (m_visible_amount <= 0.001f || m_length == 0) return;

	const Font&            font = t_fonts->secondary;
	const std::string_view text{m_text, m_length};

	const float width  = text_width(font, text) + K_PADDING_X * 2.0f;
	const float height = font.line_height() + K_PADDING_Y * 2.0f;
	const float rise   = snapped_to_pixel((1.0f - m_visible_amount) * K_RISE_DISTANCE);

	float y = m_anchor.y - height - K_ANCHOR_GAP + rise;
	if (y < t_bounds.y + K_EDGE_MARGIN) {
		y = m_anchor.bottom() + K_ANCHOR_GAP - rise;
	}

	const float min_x = t_bounds.x + K_EDGE_MARGIN;
	const float max_x = std::max(min_x, t_bounds.right() - K_EDGE_MARGIN - width);
	const Rect  bubble{std::clamp(m_anchor.x + (m_anchor.w - width) * 0.5f, min_x, max_x), y, width, height};

	const auto alpha = static_cast<u8>(t_alpha * m_visible_amount);

	t_draw_list->add_bordered_rect(bubble, rounded(K_CORNER_RADIUS), faded(g_theme.popup, alpha), faded(g_theme.border, alpha), 1.0f);
	draw_text(t_draw_list, font, Vec2{bubble.x + K_PADDING_X, font.centered_baseline(bubble)}, text, faded(g_theme.text, alpha));
}
