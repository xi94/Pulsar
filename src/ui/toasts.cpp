#include "ui/toasts.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "core/animation.h"
#include "core/profiler.h"
#include "core/settings.h"
#include "core/str.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "platform/window.h"
#include "ui/controls.h"
#include "ui/text.h"

namespace {
constexpr float margin_x = 14.0f;
constexpr float margin_y = 12.0f;
constexpr float padding_x = 11.0f;
constexpr float padding_y = 8.0f;
constexpr float corner_radius = 7.0f;
constexpr float icon_size = 15.0f;
constexpr float icon_gap = 8.0f;
constexpr float icon_turn_seconds = 5.0f;
constexpr float bar_height = 3.0f;
constexpr float bar_gap = 7.0f;
constexpr float breathing_width = 26.0f;
constexpr float min_card_width = 150.0f;
constexpr float max_card_width = 300.0f;
constexpr float lifetime_seconds = 4.0f;
constexpr float presence_ease_rate = 18.0f;
constexpr float clickable_presence = 0.6f;

constexpr u32 bar_glow_layers = 2;
constexpr float bar_glow_spread = 2.0f;
constexpr float bar_glow_alpha = 54.0f;
constexpr float sweep_width = 46.0f;
constexpr float sweeps_per_second = 0.55f;
constexpr float sweep_alpha = 120.0f;

constexpr Color color_card{24, 24, 28, 244};
constexpr Color color_border{52, 52, 60, 255};
constexpr Color color_text{222, 222, 228, 255};
constexpr Color color_bar_track{48, 48, 56, 255};
}

Toasts::Toasts(const Settings &t_settings, const Fonts &t_fonts, const Assets &t_assets, const Window &t_window,
			   CommandQueue &t_commands)
	: m_settings(t_settings)
	, m_fonts(t_fonts)
	, m_assets(t_assets)
	, m_window(t_window)
	, m_commands(t_commands)
{
}

void Toasts::notify(const Notification &t_notification)
{
	show(t_notification, lifetime_seconds, false);
}

void Toasts::notify_countdown(std::string_view t_message, float t_seconds)
{
	show(Notification{.message = t_message}, t_seconds, true);
}

void Toasts::dismiss_countdown()
{
	if (!m_countdown) return;

	m_showing = false;
	m_countdown = false;
}

void Toasts::show(const Notification &t_notification, float t_seconds, bool t_countdown)
{
	if (!m_settings.show_notifications) return;

	copy_to(t_notification.message, m_message);
	m_icon = t_notification.icon;
	m_spin_icon = t_notification.spin_icon;
	m_on_click = t_notification.on_click;
	m_total_seconds = t_seconds;
	m_remaining_seconds = t_seconds;
	m_showing = true;
	m_countdown = t_countdown;
}

float Toasts::icon_column_width() const
{
	return m_icon ? icon_size + icon_gap : 0.0f;
}

Rect Toasts::card_rect() const
{
	const Font &font = m_fonts.secondary();
	const float chrome = padding_x * 2.0f + icon_column_width();
	const float width =
		std::clamp(chrome + text_width(font, m_message) + breathing_width, min_card_width, max_card_width);

	std::string_view lines[max_lines];
	const u32 line_count = std::max(1u, wrap_text(font, m_message, width - chrome, lines));
	const float height = padding_y * 2.0f + line_count * font.line_height() + bar_gap + bar_height;
	const float bottom = static_cast<float>(m_window.height()) - status_bar_height - margin_y;

	return Rect{margin_x, bottom - height, width, height};
}

Rect Toasts::animated_card_rect() const
{
	Rect card = card_rect();
	card.x -= (1.0f - m_presence) * (card.w + margin_x);

	return card;
}

bool Toasts::is_clickable_at(Vec2 t_point) const
{
	return !m_countdown && m_showing && m_presence >= clickable_presence && animated_card_rect().contains(t_point);
}

void Toasts::update(float t_delta_seconds)
{
	PULSAR_PROFILE_SCOPE("Toasts.Update");

	const bool held_by_hover = !m_countdown && is_clickable_at(m_mouse);

	if (m_showing && !held_by_hover) {
		m_remaining_seconds = std::max(0.0f, m_remaining_seconds - t_delta_seconds);
		m_showing = m_remaining_seconds > 0.0f;
	}

	m_presence = animation::ease_toward(m_presence, m_showing ? 1.0f : 0.0f, presence_ease_rate, t_delta_seconds);
	m_elapsed_seconds += t_delta_seconds;
}

bool Toasts::on_pointer_down(Vec2 t_point)
{
	return is_clickable_at(t_point);
}

bool Toasts::on_pointer_up(Vec2 t_point)
{
	if (!is_clickable_at(t_point)) return false;

	if (m_on_click) {
		m_commands.push(*m_on_click);
	}

	m_showing = false;

	return true;
}

CursorKind Toasts::cursor() const
{
	return is_clickable_at(m_mouse) ? CursorKind::hand : CursorKind::arrow;
}

void Toasts::draw(DrawList &t_draw_list)
{
	PULSAR_PROFILE_SCOPE("Toasts.Draw");

	if (m_presence < 0.01f) return;

	const Font &font = m_fonts.secondary();
	const Rect card = animated_card_rect();
	const auto alpha = static_cast<u8>(std::clamp(m_presence, 0.0f, 1.0f) * 255.0f);

	t_draw_list.add_bordered_rect(card, rounded(corner_radius), faded(color_card, alpha), faded(color_border, alpha),
								  1.0f);

	if (m_icon) {
		const float first_line_center = card.y + padding_y + font.line_height() * 0.5f;
		const Rect icon{card.x + padding_x, first_line_center - icon_size * 0.5f, icon_size, icon_size};
		const float turn = std::fmod(m_elapsed_seconds, icon_turn_seconds) / icon_turn_seconds;
		const float radians = m_spin_icon ? turn * 2.0f * std::numbers::pi_v<float> : 0.0f;

		t_draw_list.add_rotated_image(icon, radians, m_assets.get(*m_icon), faded(m_settings.accent, alpha));
	}

	const float text_x = card.x + padding_x + icon_column_width();
	draw_wrapped_text(t_draw_list, font, Vec2{text_x, card.y + padding_y + font.ascent()},
					  card.right() - padding_x - text_x, m_message, faded(color_text, alpha), max_lines);

	draw_time_left_bar(t_draw_list, card, alpha);
}

void Toasts::draw_time_left_bar(DrawList &t_draw_list, Rect t_card, u8 t_alpha) const
{
	const Color accent = m_settings.accent;
	const Rect track{t_card.x + padding_x, t_card.bottom() - padding_y - bar_height, t_card.w - padding_x * 2.0f,
					 bar_height};
	const float remaining = m_total_seconds > 0.0f ? m_remaining_seconds / m_total_seconds : 0.0f;
	const Rect fill{track.x, track.y, track.w * std::clamp(remaining, 0.0f, 1.0f), track.h};

	t_draw_list.add_rounded_rect(track, rounded(bar_height * 0.5f), faded(color_bar_track, t_alpha));
	if (fill.w <= 0.0f) return;

	for (u32 layer = 1; layer <= bar_glow_layers; layer += 1) {
		const float spread = bar_glow_spread * layer;
		const auto glow_alpha = static_cast<u8>(bar_glow_alpha / layer * m_presence);
		t_draw_list.add_rounded_rect(fill.inset(-spread), rounded(bar_height * 0.5f + spread),
									 with_alpha(accent, glow_alpha));
	}

	t_draw_list.add_rounded_rect(fill, rounded(bar_height * 0.5f), faded(accent, t_alpha));

	t_draw_list.push_clip(fill.inset(0.0f, -bar_glow_spread * bar_glow_layers));

	const float sweep_x =
		fill.x - sweep_width + std::fmod(m_elapsed_seconds * sweeps_per_second, 1.0f) * (fill.w + sweep_width);
	const Color sweep_edge = with_alpha(lightened(accent, 70), 0);
	const Color sweep_peak = with_alpha(lightened(accent, 70), static_cast<u8>(sweep_alpha * m_presence));
	const float half_sweep = sweep_width * 0.5f;

	t_draw_list.add_gradient(Rect{sweep_x, fill.y, half_sweep, fill.h}, sweep_edge, sweep_peak, sweep_edge, sweep_peak);
	t_draw_list.add_gradient(Rect{sweep_x + half_sweep, fill.y, half_sweep, fill.h}, sweep_peak, sweep_edge, sweep_peak,
							 sweep_edge);

	t_draw_list.pop_clip();
}
