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
#include "os/window.h"
#include "ui/controls.h"
#include "ui/text.h"
#include "ui/theme.h"
#include "ui/window_layout.h"

namespace {
constexpr float K_MARGIN_BOTTOM      = 12.0f;
constexpr float K_PADDING_X          = 16.0f;
constexpr float K_PADDING_Y          = 9.0f;
constexpr float K_MAX_CORNER_RADIUS  = 22.0f;
constexpr float K_SLIDE_DISTANCE     = 14.0f;
constexpr float K_ICON_SIZE          = 15.0f;
constexpr float K_ICON_GAP           = 8.0f;
constexpr float K_ICON_TURN_SECONDS  = 5.0f;
constexpr float K_BAR_HEIGHT         = 3.0f;
constexpr float K_BAR_GAP            = 7.0f;
constexpr float K_BREATHING_WIDTH    = 26.0f;
constexpr float K_MIN_CARD_WIDTH     = 150.0f;
constexpr float K_MAX_CARD_WIDTH     = 300.0f;
constexpr float K_LIFETIME_SECONDS   = 4.0f;
constexpr float K_PRESENCE_EASE_RATE = 18.0f;
constexpr float K_CLICKABLE_PRESENCE = 0.6f;

constexpr u32   K_BAR_GLOW_LAYERS   = 2;
constexpr float K_BAR_GLOW_SPREAD   = 2.0f;
constexpr float K_BAR_GLOW_ALPHA    = 54.0f;
constexpr float K_SWEEP_WIDTH       = 46.0f;
constexpr float K_SWEEPS_PER_SECOND = 0.55f;
constexpr float K_SWEEP_ALPHA       = 120.0f;
}

Toasts::Toasts(const Settings* t_settings, const Fonts* t_fonts, const Assets* t_assets, const os::Window* t_window, CommandQueue* t_commands)
	: m_settings(t_settings)
	, m_fonts(t_fonts)
	, m_assets(t_assets)
	, m_window(t_window)
	, m_commands(t_commands)
{
}

auto Toasts::notify(const Notification& t_notification) -> void
{
	show(t_notification, t_notification.seconds > 0.0f ? t_notification.seconds : K_LIFETIME_SECONDS, false);
}

auto Toasts::notify_countdown(std::string_view t_message, float t_seconds) -> void
{
	show(Notification{.message = t_message}, t_seconds, true);
}

auto Toasts::dismiss() -> void
{
	m_showing   = false;
	m_countdown = false;
	m_on_click.reset();
}

auto Toasts::is_offering(CommandType t_type) const -> bool
{
	return m_showing && m_on_click && m_on_click->type == t_type;
}

auto Toasts::show(const Notification& t_notification, float t_seconds, bool t_countdown) -> void
{
	animation::request_frame();

	if (!m_settings->show_notifications && !t_notification.always_show) return;

	copy_to(t_notification.message, m_message);
	m_icon              = t_notification.icon;
	m_spin_icon         = t_notification.spin_icon;
	m_on_click          = t_notification.on_click;
	m_total_seconds     = t_seconds;
	m_remaining_seconds = t_seconds;
	m_showing           = true;
	m_countdown         = t_countdown;
}

auto Toasts::icon_column_width() const -> float
{
	return m_icon ? K_ICON_SIZE + K_ICON_GAP : 0.0f;
}

auto Toasts::card_rect() const -> Rect
{
	const Font& font   = m_fonts->secondary;
	const float chrome = K_PADDING_X * 2.0f + icon_column_width();
	const float width  = std::clamp(chrome + text_width(font, m_message) + K_BREATHING_WIDTH, K_MIN_CARD_WIDTH, K_MAX_CARD_WIDTH);

	std::string_view lines[K_MAX_LINES];
	const u32        line_count = std::max(1u, wrap_text(font, m_message, width - chrome, lines));
	const float      height     = K_PADDING_Y * 2.0f + line_count * font.line_height() + K_BAR_GAP + K_BAR_HEIGHT;
	const Vec2       window     = m_window->size();
	const float      bottom     = window.y - K_STATUS_BAR_HEIGHT - K_MARGIN_BOTTOM;

	return Rect{(window.x - width) * 0.5f, bottom - height, width, height};
}

auto Toasts::animated_card_rect() const -> Rect
{
	Rect card = card_rect();
	card.y += snapped_to_pixel((1.0f - m_presence) * K_SLIDE_DISTANCE);

	return card;
}

auto Toasts::is_clickable_at(Vec2 t_point) const -> bool
{
	return !m_countdown && m_showing && m_presence >= K_CLICKABLE_PRESENCE && animated_card_rect().contains(t_point);
}

auto Toasts::update(float t_delta_seconds) -> void
{
	PULSAR_PROFILE_SCOPE("Toasts.Update");

	const bool held_by_hover = !m_countdown && is_clickable_at(m_mouse);

	if (m_showing && !held_by_hover) {
		m_remaining_seconds = std::max(0.0f, m_remaining_seconds - t_delta_seconds);
		m_showing           = m_remaining_seconds > 0.0f;
	}

	m_presence =
		animation::ease_toward(m_presence, m_showing ? 1.0f : 0.0f, K_PRESENCE_EASE_RATE, t_delta_seconds, animation::K_SETTLED_PIXELS / K_SLIDE_DISTANCE);
	m_elapsed_seconds += t_delta_seconds;

	if (m_showing || m_presence > 0.0f) {
		animation::request_frame();
	}
}

auto Toasts::on_pointer_down(Vec2 t_point) -> bool
{
	return is_clickable_at(t_point);
}

auto Toasts::on_pointer_up(Vec2 t_point) -> bool
{
	if (!is_clickable_at(t_point)) return false;

	if (m_on_click) {
		m_commands->push(*m_on_click);
	}

	m_showing = false;

	return true;
}

auto Toasts::cursor() const -> CursorKind
{
	return is_clickable_at(m_mouse) ? CursorKind::Hand : CursorKind::Arrow;
}

auto Toasts::draw(DrawList* t_draw_list) -> void
{
	PULSAR_PROFILE_SCOPE("Toasts.Draw");

	if (m_presence < 0.01f) return;

	const Font& font  = m_fonts->secondary;
	const Rect  card  = animated_card_rect();
	const auto  alpha = to_alpha(m_presence);

	const float corner_radius = std::min(card.h * 0.5f, K_MAX_CORNER_RADIUS);
	controls::draw_popup_shadow(t_draw_list, card, corner_radius, m_presence);
	t_draw_list->add_bordered_rect(card, rounded(corner_radius), faded(with_alpha(g_theme.popup, 244), alpha), faded(g_theme.border, alpha), 1.0f);

	if (m_icon) {
		const float first_line_center = card.y + K_PADDING_Y + font.line_height() * 0.5f;
		const Rect  icon{card.x + K_PADDING_X, first_line_center - K_ICON_SIZE * 0.5f, K_ICON_SIZE, K_ICON_SIZE};
		const float turn    = std::fmod(m_elapsed_seconds, K_ICON_TURN_SECONDS) / K_ICON_TURN_SECONDS;
		const float radians = m_spin_icon ? turn * 2.0f * std::numbers::pi_v<float> : 0.0f;

		t_draw_list->add_rotated_image(icon, radians, m_assets->get(*m_icon), faded(m_settings->accent, alpha));
	}

	const float text_x = card.x + K_PADDING_X + icon_column_width();
	draw_wrapped_text(t_draw_list, font, Vec2{text_x, card.y + K_PADDING_Y + font.ascent}, card.right() - K_PADDING_X - text_x, m_message,
	                  faded(g_theme.text, alpha), K_MAX_LINES);

	draw_time_left_bar(t_draw_list, card, alpha);
}

auto Toasts::draw_time_left_bar(DrawList* t_draw_list, Rect t_card, u8 t_alpha) const -> void
{
	const Color accent = m_settings->accent;
	const Rect  track{t_card.x + K_PADDING_X, t_card.bottom() - K_PADDING_Y - K_BAR_HEIGHT, t_card.w - K_PADDING_X * 2.0f, K_BAR_HEIGHT};
	const float remaining = m_total_seconds > 0.0f ? m_remaining_seconds / m_total_seconds : 0.0f;
	const Rect  fill{track.x, track.y, track.w * std::clamp(remaining, 0.0f, 1.0f), track.h};

	t_draw_list->add_rounded_rect(track, rounded(K_BAR_HEIGHT * 0.5f), faded(g_theme.control, t_alpha));
	if (fill.w <= 0.0f) return;

	for (u32 layer = 1; layer <= K_BAR_GLOW_LAYERS; layer += 1) {
		const float spread     = K_BAR_GLOW_SPREAD * layer;
		const auto  glow_alpha = static_cast<u8>(K_BAR_GLOW_ALPHA / layer * m_presence);
		t_draw_list->add_rounded_rect(fill.inset(-spread), rounded(K_BAR_HEIGHT * 0.5f + spread), with_alpha(accent, glow_alpha));
	}

	t_draw_list->add_rounded_rect(fill, rounded(K_BAR_HEIGHT * 0.5f), faded(accent, t_alpha));

	t_draw_list->push_clip(fill.inset(0.0f, -K_BAR_GLOW_SPREAD * K_BAR_GLOW_LAYERS));

	const float sweep_x    = fill.x - K_SWEEP_WIDTH + std::fmod(m_elapsed_seconds * K_SWEEPS_PER_SECOND, 1.0f) * (fill.w + K_SWEEP_WIDTH);
	const Color sweep_edge = with_alpha(lightened(accent, 70), 0);
	const Color sweep_peak = with_alpha(lightened(accent, 70), static_cast<u8>(K_SWEEP_ALPHA * m_presence));
	const float half_sweep = K_SWEEP_WIDTH * 0.5f;

	t_draw_list->add_gradient(Rect{sweep_x, fill.y, half_sweep, fill.h}, sweep_edge, sweep_peak, sweep_edge, sweep_peak);
	t_draw_list->add_gradient(Rect{sweep_x + half_sweep, fill.y, half_sweep, fill.h}, sweep_peak, sweep_edge, sweep_peak, sweep_edge);

	t_draw_list->pop_clip();
}
