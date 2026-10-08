#include "ui/truncation_hint.h"

#include <algorithm>
#include <cstring>
#include <optional>

#include "core/animation.h"
#include "core/str.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "ui/controls.h"
#include "ui/text.h"
#include "ui/theme.h"

namespace {
constexpr float K_SHOW_DELAY_SECONDS = 0.45f;
constexpr float K_FADE_RATE          = 20.0f;
constexpr float K_PADDING_X          = 10.0f;
constexpr float K_PADDING_Y          = 7.0f;
constexpr float K_CORNER_RADIUS      = 8.0f;
constexpr float K_ANCHOR_GAP         = 6.0f;
constexpr float K_EDGE_MARGIN        = 8.0f;
constexpr float K_MAX_WIDTH          = 360.0f;
constexpr float K_RISE_DISTANCE      = 4.0f;
constexpr u32   K_MAX_LINES          = 8;

[[nodiscard]] auto layout_lines(const Font& t_font, std::string_view t_text, float t_max_width, std::string_view (&t_lines)[K_MAX_LINES]) -> u32
{
	std::string_view wrapped[K_MAX_LINES];
	const u32        wrapped_count = wrap_text(t_font, t_text, t_max_width, wrapped);
	u32              count         = 0;

	for (u32 i = 0; i < wrapped_count && count < K_MAX_LINES; i += 1) {
		std::string_view rest = wrapped[i];

		while (count < K_MAX_LINES) {
			usize fitting = rest.size();
			while (fitting > 0 && text_width(t_font, rest.substr(0, fitting)) > t_max_width) {
				fitting = previous_codepoint(rest, fitting);
			}

			if (fitting == 0) {
				fitting = next_codepoint(rest, 0);
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

TruncationHint::TruncationHint(const Fonts* t_fonts)
	: m_fonts(t_fonts)
{
}

auto TruncationHint::capture(const DrawList* t_draw_list) -> void
{
	const std::optional<TruncatedText> hovered = hovered_truncated_text(t_draw_list);
	m_requested                                = hovered.has_value();
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

auto TruncationHint::update(float t_delta_seconds, bool t_suppressed) -> void
{
	const bool requested = m_requested && !t_suppressed;
	m_hover_seconds      = requested ? m_hover_seconds + t_delta_seconds : 0.0f;

	const bool showing = requested && m_hover_seconds >= K_SHOW_DELAY_SECONDS;
	if (requested && !showing) {
		animation::request_frame_after(K_SHOW_DELAY_SECONDS - m_hover_seconds);
	}

	m_visible_amount = animation::ease_toward(m_visible_amount, showing ? 1.0f : 0.0f, K_FADE_RATE, t_delta_seconds);
}

auto TruncationHint::draw(DrawList* t_draw_list, Rect t_bounds) const -> void
{
	if (m_visible_amount <= 0.001f || m_length == 0) return;

	const RoundnessScope corners{controls::K_POPUP_ROUNDNESS};

	const Font& font       = m_fonts->secondary;
	const float wrap_width = std::max(40.0f, std::min(K_MAX_WIDTH, t_bounds.w - (K_EDGE_MARGIN + K_PADDING_X) * 2.0f));

	std::string_view lines[K_MAX_LINES];
	const u32        line_count = std::max(1u, layout_lines(font, std::string_view{m_text, m_length}, wrap_width, lines));

	float widest = 0.0f;
	for (u32 i = 0; i < line_count; i += 1) {
		widest = std::max(widest, text_width(font, lines[i]));
	}

	const float width  = widest + K_PADDING_X * 2.0f;
	const float height = line_count * font.line_height() + K_PADDING_Y * 2.0f;
	const float rise   = snapped_to_pixel((1.0f - m_visible_amount) * K_RISE_DISTANCE);

	float y = m_anchor.bottom() + K_ANCHOR_GAP + rise;
	if (y + height > t_bounds.bottom() - K_EDGE_MARGIN) {
		y = m_anchor.y - K_ANCHOR_GAP - height - rise;
	}

	const float min_x = t_bounds.x + K_EDGE_MARGIN;
	const float max_x = std::max(min_x, t_bounds.right() - K_EDGE_MARGIN - width);
	const Rect  card{snapped_to_pixel(std::clamp(m_anchor.x - K_PADDING_X, min_x, max_x)), snapped_to_pixel(y), width, height};
	const auto  alpha = to_alpha(m_visible_amount);

	controls::draw_popup_shadow(t_draw_list, card, K_CORNER_RADIUS, m_visible_amount);
	controls::draw_glass(t_draw_list, card, rounded(K_CORNER_RADIUS), controls::GlassSurface::SOFT, alpha);

	for (u32 i = 0; i < line_count; i += 1) {
		draw_text(t_draw_list, font, Vec2{card.x + K_PADDING_X, card.y + K_PADDING_Y + font.ascent + i * font.line_height()}, lines[i],
		          faded(g_theme.text, alpha));
	}
}
