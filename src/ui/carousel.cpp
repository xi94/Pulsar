#include "ui/carousel.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

#include <Windows.h>

#include "core/animation.h"
#include "core/profiler.h"
#include "core/settings.h"
#include "gfx/assets.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "platform/window.h"
#include "ui/controls.h"
#include "ui/text.h"
#include "ui/theme.h"

namespace {
constexpr float card_width = 220.0f;
constexpr float card_height = 300.0f;
constexpr float card_spacing = 36.0f;
constexpr float card_corner_radius = 14.0f;
constexpr float drag_pixels_per_card = card_width + card_spacing;
constexpr float scroll_ease_rate = 12.0f;
constexpr float mode_transition_ease_rate = 16.0f;
constexpr float mode_slide_distance = 18.0f;
constexpr float zoom_ease_rate = 9.0f;
constexpr float edge_fade_width = 64.0f;

constexpr float overview_padding = 28.0f;
constexpr float overview_hint_room = 48.0f;
constexpr float overview_gap = 18.0f;
constexpr float overview_ease_rate = 11.0f;
constexpr float overview_retarget_threshold = 0.9f;
constexpr float lift_ease_rate = 16.0f;
constexpr float lift_growth = 0.06f;
constexpr float hint_ease_rate = 14.0f;
constexpr float reorder_stiffness = 420.0f;
constexpr float reorder_damping_ratio = 0.86f;
constexpr float reorder_scroll_zone = 48.0f;
constexpr float reorder_scroll_speed = 720.0f;
constexpr float long_press_seconds = 0.5f;
constexpr float long_press_slop = 6.0f;
constexpr float press_delay_seconds = 0.08f;
constexpr float press_lift = 0.8f;
constexpr u8 placeholder_alpha = 34;
constexpr float hint_padding_x = 14.0f;
constexpr float hint_padding_y = 7.0f;
constexpr float hint_margin = 14.0f;
constexpr float hint_rise = 8.0f;
constexpr float hint_dot_size = 3.0f;
constexpr float hint_dot_gap = 9.0f;

constexpr Vec2 reference_view_size{1042.0f, 609.0f};
constexpr float min_view_scale = 0.75f;
constexpr float max_view_scale = 1.5f;

constexpr Vec2 grid_card_min_size{160.0f, 220.0f};
constexpr Vec2 grid_card_max_size{224.0f, 308.0f};
constexpr float grid_gap = 24.0f;
constexpr float grid_padding = 24.0f;
constexpr float grid_hover_growth = 0.06f;
constexpr float grid_hover_ease_rate = 14.0f;

constexpr float list_thumb_min_size = 56.0f;
constexpr float list_thumb_max_size = 96.0f;
constexpr float list_row_padding_y = 14.0f;
constexpr float list_padding = 16.0f;
constexpr float list_gap = 8.0f;
constexpr float list_corner_radius = 10.0f;

constexpr float switcher_hold_seconds = 0.7f;
constexpr float switcher_ease_rate = 18.0f;
constexpr float switcher_width = 150.0f;
constexpr float switcher_padding = 6.0f;
constexpr float switcher_radius = 10.0f;
constexpr float switcher_margin = 16.0f;
constexpr float switcher_slide_distance = 8.0f;
constexpr float switcher_icon_size = 24.0f;
constexpr float switcher_icon_gap = 10.0f;
constexpr float switcher_content_inset = 12.0f;
constexpr float switcher_track_column_min = 26.0f;
constexpr float switcher_track_width = 4.0f;
constexpr float switcher_indicator_height = 18.0f;
constexpr float switcher_indicator_padding = 7.0f;

constexpr float status_icon_size = 16.0f;
constexpr float status_icon_gap = 8.0f;
constexpr float status_padding_right = 14.0f;
constexpr float baseline_nudge = 2.0f;

constexpr Color color_image{255, 255, 255, 255};
constexpr u8 card_border_alpha = 160;
constexpr u8 card_border_highlighted_alpha = 235;
constexpr u8 switcher_tick_alpha = 190;

constexpr i32 zoom_stop_count = 7;
constexpr i32 grid_first_stop = 1;
constexpr i32 grid_last_stop = 3;
constexpr i32 list_first_stop = 4;

constexpr ViewMode switcher_row_modes[]{ViewMode::list, ViewMode::grid, ViewMode::carousel};
constexpr i32 switcher_row_stops[]{list_first_stop, grid_first_stop, 0};
constexpr u32 switcher_row_count = static_cast<u32>(std::size(switcher_row_modes));

ViewMode mode_at_stop(i32 t_stop)
{
	if (t_stop <= 0) return ViewMode::carousel;
	if (t_stop <= grid_last_stop) return ViewMode::grid;

	return ViewMode::list;
}

float stop_percent(i32 t_stop)
{
	return static_cast<float>(t_stop) / (zoom_stop_count - 1) * 100.0f;
}

float zoom_within(float t_percent, i32 t_first_stop, i32 t_last_stop)
{
	const float first = stop_percent(t_first_stop);
	const float last = stop_percent(t_last_stop);

	return std::clamp((t_percent - first) / (last - first), 0.0f, 1.0f);
}

float view_scale_for(Rect t_bounds)
{
	const float fit = std::min(t_bounds.w / reference_view_size.x, t_bounds.h / reference_view_size.y);

	return std::clamp(fit, min_view_scale, max_view_scale);
}

Vec2 grid_card_size(float t_zoom_percent, float t_view_scale)
{
	const float t = zoom_within(t_zoom_percent, grid_first_stop, grid_last_stop);

	return Vec2{(grid_card_min_size.x + (grid_card_max_size.x - grid_card_min_size.x) * t) * t_view_scale,
				(grid_card_min_size.y + (grid_card_max_size.y - grid_card_min_size.y) * t) * t_view_scale};
}

float list_thumb_size(float t_zoom_percent, float t_view_scale)
{
	const float t = zoom_within(t_zoom_percent, list_first_stop, zoom_stop_count - 1);

	return snapped_to_pixel((list_thumb_min_size + (list_thumb_max_size - list_thumb_min_size) * t) * t_view_scale);
}

float list_row_height(float t_zoom_percent, float t_view_scale)
{
	return list_thumb_size(t_zoom_percent, t_view_scale) + list_row_padding_y * 2.0f;
}

u32 grid_columns(float t_width, float t_zoom_percent, float t_view_scale)
{
	const float usable = t_width - grid_padding * 2.0f + grid_gap;
	const float card_width_with_gap = grid_card_size(t_zoom_percent, t_view_scale).x + grid_gap;

	return std::max<u32>(1, static_cast<u32>(usable / card_width_with_gap));
}

float card_scale(float t_slots_from_center)
{
	const float closeness = std::max(0.0f, 1.0f - std::min(t_slots_from_center, 2.0f) / 2.0f);

	return 0.90f + 0.28f * closeness;
}

float center_offset_of_slot(float t_slot)
{
	constexpr i32 integration_steps = 24;

	const float distance = std::fabs(t_slot);
	if (distance < 0.0001f) return 0.0f;

	const float step = distance / integration_steps;
	float covered = 0.0f;
	float previous_width = card_width * card_scale(0.0f);

	for (i32 i = 1; i <= integration_steps; i += 1) {
		const float width = card_width * card_scale(step * i);
		covered += (previous_width + width) * 0.5f * step;
		previous_width = width;
	}

	return std::copysign(covered + distance * card_spacing, t_slot);
}

struct CardLook {
	Color border;
	float border_thickness;
	float glow_size;
	u8 glow_alpha;
};

CardLook card_look(bool t_highlighted, bool t_centered)
{
	if (!t_highlighted) return CardLook{with_alpha(theme().border, card_border_alpha), 2.0f, 0.0f, 0};
	if (t_centered) return CardLook{with_alpha(theme().text, card_border_highlighted_alpha), 2.5f, 18.0f, 255};

	return CardLook{with_alpha(theme().text, card_border_highlighted_alpha), 2.0f, 14.0f, 225};
}

std::string_view mode_name(ViewMode t_mode)
{
	switch (t_mode) {
		case ViewMode::carousel:
			return "Carousel";
		case ViewMode::grid:
			return "Grid";
		case ViewMode::list:
			return "List";
	}

	return "";
}

Asset mode_icon(ViewMode t_mode)
{
	switch (t_mode) {
		case ViewMode::grid:
			return Asset::icon_grid;
		case ViewMode::list:
			return Asset::icon_list;
		case ViewMode::carousel:
			break;
	}

	return Asset::icon_carousel;
}

float track_column_width(const Font &t_font)
{
	return std::max(switcher_track_column_min, text_width(t_font, "100%") + switcher_indicator_padding * 2.0f + 6.0f);
}

i32 stop_at_track_position(Rect t_track, float t_y)
{
	const float t = std::clamp(1.0f - (t_y - t_track.y) / t_track.h, 0.0f, 1.0f);

	return static_cast<i32>(std::round(t * (zoom_stop_count - 1)));
}

Rect lerp_rect(Rect t_from, Rect t_to, float t_amount)
{
	return Rect{t_from.x + (t_to.x - t_from.x) * t_amount, t_from.y + (t_to.y - t_from.y) * t_amount,
				t_from.w + (t_to.w - t_from.w) * t_amount, t_from.h + (t_to.h - t_from.h) * t_amount};
}

Rect translated(Rect t_rect, Vec2 t_offset)
{
	return Rect{t_rect.x + t_offset.x, t_rect.y + t_offset.y, t_rect.w, t_rect.h};
}

Rect scaled_from_center(Rect t_rect, float t_scale)
{
	return t_rect.inset(t_rect.w * (1.0f - t_scale) * 0.5f, t_rect.h * (1.0f - t_scale) * 0.5f);
}

bool is_control_held()
{
	return (GetKeyState(VK_CONTROL) & 0x8000) != 0;
}
}

Carousel::Carousel(const Library &t_library, const Settings &t_settings, const Fonts &t_fonts, const Assets &t_assets,
				   CommandQueue &t_commands)
	: m_library(t_library)
	, m_settings(t_settings)
	, m_fonts(t_fonts)
	, m_assets(t_assets)
	, m_commands(t_commands)
{
	for (u32 i = 0; i < max_games; i += 1) {
		m_order[i] = static_cast<u8>(i);
		m_slot_of[i] = static_cast<u8>(i);
	}
}

void Carousel::restore(i32 t_zoom_stop, i32 t_selected_game)
{
	m_zoom_stop = std::clamp(t_zoom_stop, 0, zoom_stop_count - 1);
	m_zoom_percent = stop_percent(m_zoom_stop);
	m_mode = mode_at_stop(m_zoom_stop);
	m_previous_mode = m_mode;
	m_mode_transition = 0.0f;

	const i32 game = std::clamp(t_selected_game, 0, std::max(0, static_cast<i32>(game_count()) - 1));
	m_scroll = clamp_scroll(game_count() > 0 ? static_cast<float>(m_slot_of[game]) : 0.0f);
	m_target_scroll = m_scroll;
	m_focused_game = selected_game();
}

void Carousel::set_order(std::span<const u8> t_order)
{
	bool used[max_games]{};
	u32 count = 0;

	for (const u8 game : t_order) {
		if (game >= game_count() || used[game]) continue;

		used[game] = true;
		m_order[count] = game;
		count += 1;
	}

	for (u32 game = 0; game < game_count(); game += 1) {
		if (used[game]) continue;

		m_order[count] = static_cast<u8>(game);
		count += 1;
	}

	for (u32 slot = 0; slot < count; slot += 1) {
		m_slot_of[m_order[slot]] = static_cast<u8>(slot);
	}
}

i32 Carousel::selected_game() const
{
	if (game_count() == 0) return 0;

	return m_order[static_cast<u32>(clamp_scroll(std::round(m_target_scroll)))];
}

float Carousel::clamp_scroll(float t_offset) const
{
	return game_count() == 0 ? 0.0f : std::clamp(t_offset, 0.0f, static_cast<float>(game_count() - 1));
}

float Carousel::view_scale() const
{
	return view_scale_for(m_bounds);
}

Rect Carousel::carousel_slot(float t_offset) const
{
	const float view = view_scale();
	const float scale = card_scale(std::fabs(t_offset)) * view;
	const float width = card_width * scale;
	const float height = card_height * scale;
	const float center_x = m_bounds.center().x + center_offset_of_slot(t_offset) * view;

	return Rect{center_x - width * 0.5f, m_bounds.y + (m_bounds.h - height) * 0.5f, width, height};
}

Rect Carousel::overview_slot(u32 t_slot) const
{
	const u32 count = std::max<u32>(1, game_count());
	const float aspect = card_width / card_height;
	const Rect area{m_bounds.x + overview_padding, m_bounds.y + overview_padding, m_bounds.w - overview_padding * 2.0f,
					m_bounds.h - overview_padding * 2.0f - overview_hint_room};

	const float by_width = (area.w - (count - 1) * overview_gap) / count;
	const float width = std::min({by_width, area.h * aspect, card_width * view_scale()});
	const float height = width / aspect;
	const float row_width = count * width + (count - 1) * overview_gap;

	return Rect{area.center().x - row_width * 0.5f + t_slot * (width + overview_gap), area.center().y - height * 0.5f,
				width, height};
}

Rect Carousel::grid_slot(u32 t_slot) const
{
	const Vec2 size = grid_card_size(m_zoom_percent, view_scale());
	const u32 columns = grid_columns(m_bounds.w, m_zoom_percent, view_scale());
	const u32 column = t_slot % columns;
	const u32 row = t_slot / columns;

	const float total_width = columns * size.x + (columns - 1) * grid_gap;
	const float x = m_bounds.x + (m_bounds.w - total_width) * 0.5f + column * (size.x + grid_gap);
	const float y = m_bounds.y + grid_padding + row * (size.y + grid_gap) - m_wrap_scroll.offset();

	return Rect{x, y, size.x, size.y};
}

Rect Carousel::list_slot(u32 t_slot) const
{
	const float height = list_row_height(m_zoom_percent, view_scale());
	const float y = m_bounds.y + list_padding + t_slot * (height + list_gap) - m_wrap_scroll.offset();

	return Rect{m_bounds.x + list_padding, y, m_bounds.w - list_padding * 2.0f, height};
}

Rect Carousel::slot_rect(ViewMode t_mode, u32 t_slot) const
{
	switch (t_mode) {
		case ViewMode::grid:
			return grid_slot(t_slot);
		case ViewMode::list:
			return list_slot(t_slot);
		case ViewMode::carousel:
			break;
	}

	const Rect carousel = carousel_slot(static_cast<float>(t_slot) - m_scroll);

	return m_overview > 0.0f ? lerp_rect(carousel, overview_slot(t_slot), m_overview) : carousel;
}

Rect Carousel::shown_card(ViewMode t_mode, u32 t_game) const
{
	if (m_reorder.active && static_cast<i32>(t_game) == m_reorder.game && t_mode == m_mode) return dragged_rect();

	return translated(slot_rect(t_mode, m_slot_of[t_game]), m_card_offset[t_game]);
}

Rect Carousel::dragged_rect() const
{
	const Rect slot = slot_rect(m_mode, m_slot_of[m_reorder.game]);

	return Rect{m_mouse.x - m_reorder.grab.x * slot.w, m_mouse.y - m_reorder.grab.y * slot.h, slot.w, slot.h};
}

float Carousel::lift_scale(u32 t_game) const
{
	return static_cast<i32>(t_game) == m_reorder.game ? 1.0f + lift_growth * m_lift : 1.0f;
}

bool Carousel::is_raised(ViewMode t_mode, u32 t_game) const
{
	return t_mode == m_mode && static_cast<i32>(t_game) == m_reorder.game;
}

Carousel::CardState Carousel::card_state(ViewMode t_mode, u32 t_game, Rect t_card) const
{
	if (m_reorder.active && static_cast<i32>(t_game) == m_reorder.game) return CardState{true, false};

	const bool hovered = !m_reorder.active && t_card.contains(m_mouse);
	if (t_mode != ViewMode::carousel) return CardState{hovered || is_focus_shown(t_game), false};

	const bool centered = std::fabs(static_cast<float>(m_slot_of[t_game]) - m_scroll) < 0.5f && m_overview < 0.5f;

	return CardState{centered || hovered, centered};
}

Rect Carousel::list_thumb(Rect t_row) const
{
	const float size = list_thumb_size(m_zoom_percent, view_scale());

	return Rect{t_row.x + 12.0f, t_row.y + (t_row.h - size) * 0.5f, size, size};
}

Rect Carousel::grown_grid_card(Rect t_card, u32 t_game) const
{
	const float growth = 1.0f + grid_hover_growth * m_card_hover[t_game];

	return t_card.inset(t_card.w * (1.0f - growth) * 0.5f, t_card.h * (1.0f - growth) * 0.5f);
}

ArtSource Carousel::art_source(u32 t_game) const
{
	const Rect card = shown_card(m_mode, t_game);
	const float scale = lift_scale(t_game);

	switch (m_mode) {
		case ViewMode::grid:
			return ArtSource{scaled_from_center(grown_grid_card(card, t_game), scale), card_corner_radius, false};

		case ViewMode::list:
			return ArtSource{list_thumb(scaled_from_center(card, scale)), list_corner_radius,
							 m_library.game(t_game).icon != nullptr};

		case ViewMode::carousel:
			break;
	}

	return ArtSource{scaled_from_center(card, scale), card_corner_radius, false};
}

float Carousel::wrap_content_height() const
{
	if (game_count() == 0) return 0.0f;

	if (m_mode == ViewMode::grid) {
		const u32 columns = grid_columns(m_bounds.w, m_zoom_percent, view_scale());
		const u32 rows = (game_count() + columns - 1) / columns;

		return grid_padding * 2.0f + rows * grid_card_size(m_zoom_percent, view_scale()).y + (rows - 1) * grid_gap;
	}

	if (m_mode == ViewMode::list) {
		return list_padding * 2.0f + game_count() * list_row_height(m_zoom_percent, view_scale()) +
			   (game_count() - 1) * list_gap;
	}

	return 0.0f;
}

ScrollGeometry Carousel::wrap_scroll_geometry() const
{
	const Rect track{m_bounds.right() - scrollbar_width - 8.0f, m_bounds.y + 8.0f, scrollbar_width, m_bounds.h - 16.0f};

	return ScrollGeometry{track, wrap_content_height(), m_bounds.h};
}

i32 Carousel::game_at(Vec2 t_point) const
{
	if (m_mode != ViewMode::carousel && (t_point.y < m_bounds.y || t_point.y >= m_bounds.bottom())) return -1;

	i32 closest = -1;
	float closest_distance = 0.0f;

	for (u32 game = 0; game < game_count(); game += 1) {
		if (!shown_card(m_mode, game).contains(t_point)) continue;

		const float distance = std::fabs(static_cast<float>(m_slot_of[game]) - m_scroll);
		if (closest < 0 || distance < closest_distance) {
			closest = static_cast<i32>(game);
			closest_distance = distance;
		}
	}

	return closest;
}

Rect Carousel::status_indicator_rect() const
{
	const float width = status_icon_size + status_icon_gap + text_width(m_fonts.secondary(), mode_name(m_mode));

	return Rect{m_bounds.right() - status_padding_right - width, m_bounds.bottom(), width, status_bar_height};
}

Rect Carousel::switcher_panel_rect() const
{
	const float row_height = m_fonts.body().line_height() + 10.0f;
	const float height = switcher_padding * 2.0f + row_height * switcher_row_count;
	const float width = switcher_width + track_column_width(m_fonts.secondary());

	return Rect{m_bounds.right() - width - switcher_margin, m_bounds.bottom() - height - switcher_margin, width,
				height};
}

Rect Carousel::switcher_row_rect(Rect t_panel, u32 t_row) const
{
	const float row_height = m_fonts.body().line_height() + 10.0f;

	return Rect{t_panel.x + switcher_padding, t_panel.y + switcher_padding + row_height * t_row,
				switcher_width - switcher_padding * 2.0f, row_height};
}

Rect Carousel::switcher_track_rect(Rect t_panel) const
{
	const float column = track_column_width(m_fonts.secondary());

	return Rect{t_panel.right() - column * 0.5f - switcher_track_width * 0.5f,
				t_panel.y + switcher_padding + switcher_indicator_height * 0.5f, switcher_track_width,
				t_panel.h - switcher_padding * 2.0f - switcher_indicator_height};
}

Rect Carousel::switcher_track_grab_rect(Rect t_panel) const
{
	const Rect track = switcher_track_rect(t_panel);

	return Rect{track.x - 10.0f, t_panel.y, track.w + 20.0f, t_panel.h};
}

bool Carousel::is_switcher_shown() const
{
	return m_switcher_shown > 0.01f;
}

bool Carousel::is_mouse_over_switcher(Vec2 t_mouse) const
{
	const Rect indicator = status_indicator_rect();
	if (indicator.contains(t_mouse)) return true;
	if (!is_switcher_shown()) return false;

	const Rect panel = switcher_panel_rect();
	const float left = std::min(panel.x, indicator.x);
	const Rect panel_and_indicator{left, panel.y, std::max(panel.right(), indicator.right()) - left,
								   indicator.bottom() - panel.y};

	return panel_and_indicator.contains(t_mouse);
}

void Carousel::set_zoom_stop(i32 t_stop)
{
	t_stop = std::clamp(t_stop, 0, zoom_stop_count - 1);

	if (t_stop != m_zoom_stop) {
		m_zoom_stop = t_stop;

		const ViewMode mode = mode_at_stop(t_stop);
		if (mode != m_mode) {
			m_previous_mode = m_mode;
			m_mode = mode;
			m_mode_transition = 1.0f;
			m_wrap_scroll = Scrollable{};
		}

		m_commands.push(Command{.type = CommandType::save_changes});
	}

	m_switcher_hold_seconds = switcher_hold_seconds;
}

i32 Carousel::focused_game() const
{
	return m_mode == ViewMode::carousel ? selected_game() : m_focused_game;
}

bool Carousel::is_focus_shown(u32 t_game) const
{
	return m_keyboard_focus_shown && m_mode != ViewMode::carousel && static_cast<i32>(t_game) == focused_game();
}

void Carousel::move_focus(i32 t_delta)
{
	if (game_count() == 0) return;

	m_keyboard_focus_shown = true;

	const i32 last = static_cast<i32>(game_count()) - 1;
	const i32 slot = std::clamp(static_cast<i32>(m_slot_of[focused_game()]) + t_delta, 0, last);
	m_focused_game = m_order[slot];

	if (m_mode == ViewMode::carousel) {
		m_target_scroll = static_cast<float>(slot);
		return;
	}

	const Rect card = slot_rect(m_mode, static_cast<u32>(slot));
	m_wrap_scroll.reveal(card.y, card.bottom(), m_bounds.y + grid_padding, m_bounds.bottom() - grid_padding,
						 wrap_scroll_geometry());
}

void Carousel::open_game(i32 t_game)
{
	m_commands.push(Command{.type = CommandType::open_game, .index = t_game});
}

void Carousel::start_press(i32 t_game, Vec2 t_point)
{
	m_long_press = LongPress{t_game, t_point, 0.0f};
	if (t_game < 0) return;

	m_reorder.game = t_game;
	m_reorder.lifted = false;
}

void Carousel::begin_reorder(u32 t_game, Vec2 t_point)
{
	const Rect card = shown_card(m_mode, t_game);

	m_card_drag.end();
	m_wrap_scroll.on_pointer_up();
	m_long_press.game = -1;

	m_reorder.active = true;
	m_reorder.lifted = true;
	m_reorder.game = static_cast<i32>(t_game);
	m_reorder.grab = Vec2{std::clamp((t_point.x - card.x) / card.w, 0.0f, 1.0f),
						  std::clamp((t_point.y - card.y) / card.h, 0.0f, 1.0f)};
	std::copy(std::begin(m_order), std::end(m_order), std::begin(m_reorder.start_order));

	m_card_offset[t_game] = Vec2{};
	m_card_velocity[t_game] = Vec2{};
}

void Carousel::end_reorder(bool t_cancel)
{
	if (!m_reorder.active) return;

	const auto game = static_cast<u32>(m_reorder.game);

	Vec2 centers[max_games];
	capture_centers(centers);
	m_reorder.active = false;

	if (t_cancel) {
		set_order({m_reorder.start_order, game_count()});
	}

	if (m_mode == ViewMode::carousel) {
		m_scroll = static_cast<float>(m_slot_of[game]);
		m_target_scroll = m_scroll;
	}

	restore_centers(centers);
	m_card_velocity[game] = Vec2{};

	if (!std::equal(m_order, m_order + game_count(), m_reorder.start_order)) {
		m_commands.push(Command{.type = CommandType::save_changes});
	}
}

void Carousel::move_to_slot(u32 t_game, u32 t_slot)
{
	Vec2 centers[max_games];
	capture_centers(centers);

	u8 order[max_games];
	std::copy(std::begin(m_order), std::end(m_order), std::begin(order));

	const u32 from = m_slot_of[t_game];
	if (from < t_slot) {
		std::rotate(order + from, order + from + 1, order + t_slot + 1);
	} else {
		std::rotate(order + t_slot, order + from, order + from + 1);
	}

	set_order({order, game_count()});
	restore_centers(centers);
}

void Carousel::capture_centers(Vec2 (&t_centers)[max_games]) const
{
	for (u32 game = 0; game < game_count(); game += 1) {
		t_centers[game] = shown_card(m_mode, game).center();
	}
}

void Carousel::restore_centers(const Vec2 (&t_centers)[max_games])
{
	for (u32 game = 0; game < game_count(); game += 1) {
		const Vec2 slot = slot_rect(m_mode, m_slot_of[game]).center();
		m_card_offset[game] = Vec2{t_centers[game].x - slot.x, t_centers[game].y - slot.y};
	}
}

void Carousel::retarget_reorder()
{
	const auto game = static_cast<u32>(m_reorder.game);
	const Vec2 center = dragged_rect().center();

	u32 best = m_slot_of[game];
	float best_distance = -1.0f;

	for (u32 slot = 0; slot < game_count(); slot += 1) {
		const Vec2 cell = slot_rect(m_mode, slot).center();
		const float dx = m_mode == ViewMode::list ? 0.0f : cell.x - center.x;
		const float dy = m_mode == ViewMode::carousel ? 0.0f : cell.y - center.y;
		const float distance = dx * dx + dy * dy;

		if (best_distance < 0.0f || distance < best_distance) {
			best = slot;
			best_distance = distance;
		}
	}

	if (best != m_slot_of[game]) {
		move_to_slot(game, best);
	}
}

void Carousel::update_reorder(float t_delta_seconds)
{
	if (m_long_press.game >= 0) {
		m_long_press.seconds += t_delta_seconds;

		const float charge = std::clamp(
			(m_long_press.seconds - press_delay_seconds) / (long_press_seconds - press_delay_seconds), 0.0f, 1.0f);
		m_lift = press_lift * charge * charge;

		if (m_long_press.seconds >= long_press_seconds) {
			begin_reorder(static_cast<u32>(m_long_press.game), m_mouse);
		}
	}

	const bool active = m_reorder.active;
	const bool pressing = m_long_press.game >= 0;
	const bool overview = active && m_mode == ViewMode::carousel;

	m_overview = animation::ease_toward(m_overview, overview ? 1.0f : 0.0f, overview_ease_rate, t_delta_seconds,
										animation::settled_pixels / std::max(m_bounds.w, 1.0f));
	if (!pressing) {
		m_lift = animation::ease_toward(m_lift, active ? 1.0f : 0.0f, lift_ease_rate, t_delta_seconds);
	}

	m_reorder_hint = animation::ease_toward(m_reorder_hint, active ? 1.0f : 0.0f, hint_ease_rate, t_delta_seconds);

	if (active && m_mode != ViewMode::carousel) {
		const float above = m_bounds.y + reorder_scroll_zone - m_mouse.y;
		const float below = m_mouse.y - (m_bounds.bottom() - reorder_scroll_zone);

		if (above > 0.0f) {
			m_wrap_scroll.scroll_by(-reorder_scroll_speed * std::min(above / reorder_scroll_zone, 1.0f) *
										t_delta_seconds,
									wrap_scroll_geometry());
		} else if (below > 0.0f) {
			m_wrap_scroll.scroll_by(reorder_scroll_speed * std::min(below / reorder_scroll_zone, 1.0f) *
										t_delta_seconds,
									wrap_scroll_geometry());
		}
	}

	if (active && (m_mode != ViewMode::carousel || m_overview > overview_retarget_threshold)) {
		retarget_reorder();
	}

	bool settled = true;

	for (u32 game = 0; game < game_count(); game += 1) {
		Vec2 &offset = m_card_offset[game];
		Vec2 &velocity = m_card_velocity[game];

		offset.x = animation::spring_toward(offset.x, velocity.x, 0.0f, reorder_stiffness, reorder_damping_ratio,
											t_delta_seconds, animation::settled_pixels);
		offset.y = animation::spring_toward(offset.y, velocity.y, 0.0f, reorder_stiffness, reorder_damping_ratio,
											t_delta_seconds, animation::settled_pixels);
		settled = settled && offset.x == 0.0f && offset.y == 0.0f;
	}

	if (!active && !pressing && m_reorder.game >= 0 && settled && m_lift == 0.0f) {
		m_reorder.game = -1;
		m_reorder.lifted = false;
	}
}

bool Carousel::switcher_pointer_down(Vec2 t_point)
{
	const Rect panel = switcher_panel_rect();
	if (!is_switcher_shown() || !panel.contains(t_point)) return false;

	m_switcher_owns_pointer = true;

	for (u32 row = 0; row < switcher_row_count; row += 1) {
		if (switcher_row_rect(panel, row).contains(t_point)) {
			set_zoom_stop(switcher_row_stops[row]);
			return true;
		}
	}

	if (switcher_track_grab_rect(panel).contains(t_point)) {
		m_switcher_drag.begin(t_point);
		set_zoom_stop(stop_at_track_position(switcher_track_rect(panel), t_point.y));
	}

	return true;
}

bool Carousel::switcher_pointer_move(Vec2 t_point)
{
	if (!m_switcher_drag.is_pressed()) return false;

	m_switcher_drag.update(t_point);
	set_zoom_stop(stop_at_track_position(switcher_track_rect(switcher_panel_rect()), t_point.y));

	return true;
}

bool Carousel::switcher_pointer_up()
{
	m_switcher_drag.end();

	return std::exchange(m_switcher_owns_pointer, false);
}

bool Carousel::on_pointer_down(Vec2 t_point)
{
	m_keyboard_focus_shown = false;

	if (switcher_pointer_down(t_point)) return true;
	if (m_reorder.active) return true;

	const i32 pressed = m_mode_transition <= 0.001f ? game_at(t_point) : -1;

	if (pressed >= 0 && is_control_held()) {
		begin_reorder(static_cast<u32>(pressed), t_point);
		return true;
	}

	if (m_mode != ViewMode::carousel) {
		if (m_wrap_scroll.on_pointer_down(t_point, wrap_scroll_geometry())) return true;

		start_press(pressed, t_point);
		return pressed >= 0;
	}

	start_press(pressed, t_point);
	m_card_drag.begin(t_point);
	m_drag_start_scroll = m_scroll;

	return true;
}

bool Carousel::on_pointer_move(Vec2 t_point)
{
	m_keyboard_focus_shown = false;

	if (switcher_pointer_move(t_point)) return true;
	if (m_reorder.active) return true;

	if (m_long_press.game >= 0) {
		const float dx = t_point.x - m_long_press.origin.x;
		const float dy = t_point.y - m_long_press.origin.y;

		if (dx * dx + dy * dy > long_press_slop * long_press_slop) {
			m_long_press.game = -1;
		}
	}

	if (m_mode != ViewMode::carousel) {
		m_wrap_scroll.on_pointer_move(t_point.y, wrap_scroll_geometry());
		return m_wrap_scroll.is_dragging();
	}

	if (!m_card_drag.is_pressed()) return false;

	m_card_drag.update(t_point);
	m_scroll = m_drag_start_scroll - m_card_drag.delta_x() / (drag_pixels_per_card * view_scale());
	m_target_scroll = m_scroll;

	return true;
}

bool Carousel::on_pointer_up(Vec2 t_point)
{
	m_keyboard_focus_shown = false;
	m_long_press.game = -1;

	if (switcher_pointer_up()) return true;
	if (std::exchange(m_release_ignored, false)) return true;

	if (m_reorder.active) {
		end_reorder(false);
		return true;
	}

	if (m_mode != ViewMode::carousel) {
		if (m_wrap_scroll.is_dragging()) {
			m_wrap_scroll.on_pointer_up();
			return true;
		}

		if (const i32 game = game_at(t_point); game >= 0) {
			open_game(game);
		}

		return true;
	}

	if (!m_card_drag.is_pressed()) return false;

	const bool dragged = m_card_drag.has_moved();
	m_card_drag.end();

	if (dragged) {
		m_target_scroll = clamp_scroll(std::round(m_scroll));
		return true;
	}

	const i32 game = game_at(t_point);
	if (game < 0) return true;

	if (game == selected_game()) {
		open_game(game);
	}

	m_target_scroll = static_cast<float>(m_slot_of[game]);

	return true;
}

bool Carousel::on_scroll(Vec2, float t_wheel_delta)
{
	m_keyboard_focus_shown = false;
	if (m_reorder.active) return true;

	if (is_control_held()) {
		if (t_wheel_delta != 0.0f) {
			set_zoom_stop(m_zoom_stop + (t_wheel_delta > 0.0f ? 1 : -1));
		}
	} else if (m_mode == ViewMode::carousel) {
		m_target_scroll = clamp_scroll(m_target_scroll + t_wheel_delta);
	} else {
		m_wrap_scroll.on_scroll(t_wheel_delta, wrap_scroll_geometry());
	}

	return true;
}

bool Carousel::on_key_down(u32 t_key)
{
	if (m_reorder.active) {
		if (t_key == VK_ESCAPE) {
			end_reorder(true);
			m_release_ignored = true;
		}

		return true;
	}

	if (game_count() == 0) return false;

	const auto columns = static_cast<i32>(grid_columns(m_bounds.w, m_zoom_percent, view_scale()));
	const bool horizontal = m_mode != ViewMode::list;
	const bool vertical = m_mode != ViewMode::carousel;
	const i32 row_step = m_mode == ViewMode::grid ? columns : 1;

	switch (t_key) {
		case VK_LEFT:
			if (!horizontal) return false;

			move_focus(-1);
			return true;

		case VK_RIGHT:
			if (!horizontal) return false;

			move_focus(1);
			return true;

		case VK_UP:
			if (!vertical) return false;

			move_focus(-row_step);
			return true;

		case VK_DOWN:
			if (!vertical) return false;

			move_focus(row_step);
			return true;

		case VK_RETURN:
			if (m_mode != ViewMode::carousel && !m_keyboard_focus_shown) {
				move_focus(0);
				return true;
			}

			open_game(focused_game());
			return true;

		default:
			return false;
	}
}

CursorKind Carousel::cursor() const
{
	if (m_reorder.active) return CursorKind::move;

	const bool dragging_cards = m_mode == ViewMode::carousel && m_card_drag.is_pressed();
	if (m_switcher_drag.is_pressed() || dragging_cards || m_wrap_scroll.is_dragging()) return CursorKind::drag;

	if (game_count() > 0 && status_indicator_rect().contains(m_mouse)) return CursorKind::hand;

	if (is_switcher_shown()) {
		const Rect panel = switcher_panel_rect();

		for (u32 row = 0; row < switcher_row_count; row += 1) {
			if (switcher_row_rect(panel, row).contains(m_mouse)) return CursorKind::hand;
		}

		if (switcher_track_grab_rect(panel).contains(m_mouse)) return CursorKind::hand;
	}

	if (const i32 game = game_at(m_mouse); game >= 0) {
		return is_control_held() ? CursorKind::move : CursorKind::hand;
	}

	const bool over_scrollbar =
		m_mode != ViewMode::carousel && m_wrap_scroll.is_over_track(m_mouse, wrap_scroll_geometry());

	return over_scrollbar ? CursorKind::hand : CursorKind::arrow;
}

void Carousel::update(float t_delta_seconds)
{
	m_scroll = animation::ease_toward(m_scroll, m_target_scroll, scroll_ease_rate, t_delta_seconds,
									  animation::settled_pixels / (drag_pixels_per_card * view_scale()));
	m_mode_transition = animation::ease_toward(m_mode_transition, 0.0f, mode_transition_ease_rate, t_delta_seconds);
	m_zoom_percent = animation::ease_toward(m_zoom_percent, stop_percent(m_zoom_stop), zoom_ease_rate, t_delta_seconds);
	m_wrap_scroll.update(t_delta_seconds);

	const i32 hovered_grid_card = m_mode == ViewMode::grid && !m_reorder.active ? game_at(m_mouse) : -1;

	for (u32 game = 0; game < game_count(); game += 1) {
		const bool raised = static_cast<i32>(game) == hovered_grid_card || is_focus_shown(game);
		m_card_hover[game] =
			animation::ease_toward(m_card_hover[game], raised ? 1.0f : 0.0f, grid_hover_ease_rate, t_delta_seconds);
	}

	if (is_mouse_over_switcher(m_mouse)) {
		m_switcher_hold_seconds = switcher_hold_seconds;
	} else {
		m_switcher_hold_seconds = std::max(0.0f, m_switcher_hold_seconds - t_delta_seconds);
	}

	const float switcher_target = m_switcher_hold_seconds > 0.0f ? 1.0f : 0.0f;
	m_switcher_shown = animation::ease_toward(m_switcher_shown, switcher_target, switcher_ease_rate, t_delta_seconds);

	update_reorder(t_delta_seconds);
}

void Carousel::draw_card(DrawList &t_draw_list, Rect t_rect, const Game &t_game, bool t_highlighted, bool t_centered,
						 u8 t_alpha) const
{
	const CardLook look = card_look(t_highlighted, t_centered);

	if (look.glow_size > 0.0f) {
		t_draw_list.add_banner_glow(t_rect, card_corner_radius, look.glow_size,
									faded(faded(t_game.accent, look.glow_alpha), t_alpha));
	}

	t_draw_list.add_rounded_rect(t_rect, rounded(card_corner_radius), faded(look.border, t_alpha));

	if (t_game.banner == nullptr) return;

	const Rect art = t_rect.inset(look.border_thickness);
	t_draw_list.add_image(art, t_game.banner, faded(color_image, t_alpha),
						  rounded(card_corner_radius - look.border_thickness),
						  cover_uv(art.w / art.h, t_game.banner->aspect()));
}

void Carousel::draw_carousel_mode(DrawList &t_draw_list, u8 t_alpha, float t_y_offset) const
{
	for (u32 game = 0; game < game_count(); game += 1) {
		if (static_cast<i32>(game) == m_detached_game || is_raised(ViewMode::carousel, game)) continue;

		Rect card = shown_card(ViewMode::carousel, game);
		card.y += t_y_offset;

		if (card.right() < m_bounds.x || card.x > m_bounds.right()) continue;

		const CardState state = card_state(ViewMode::carousel, game, card);
		draw_card(t_draw_list, card, m_library.game(game), state.highlighted, state.centered, t_alpha);
	}

	if (!m_reorder.lifted) {
		draw_raised(t_draw_list, ViewMode::carousel, t_alpha, t_y_offset);
	}

	const auto edge_alpha = static_cast<u8>(t_alpha * (1.0f - m_overview));
	const Color opaque = faded(theme().window, edge_alpha);
	const Color clear = faded(theme().window, 0);

	t_draw_list.add_gradient(Rect{m_bounds.x, m_bounds.y, edge_fade_width, m_bounds.h}, opaque, clear, opaque, clear);
	t_draw_list.add_gradient(Rect{m_bounds.right() - edge_fade_width, m_bounds.y, edge_fade_width, m_bounds.h}, clear,
							 opaque, clear, opaque);
}

void Carousel::draw_wrap_scroll(DrawList &t_draw_list, u8 t_alpha) const
{
	const ScrollGeometry geometry = wrap_scroll_geometry();

	m_wrap_scroll.draw_edge_fade(t_draw_list, m_bounds, geometry, faded(theme().window, t_alpha));
	m_wrap_scroll.draw(t_draw_list, geometry, m_mouse, t_alpha);
}

void Carousel::draw_grid_mode(DrawList &t_draw_list, u8 t_alpha, float t_y_offset) const
{
	t_draw_list.push_clip(m_bounds);

	for (u32 game = 0; game < game_count(); game += 1) {
		if (static_cast<i32>(game) == m_detached_game || is_raised(ViewMode::grid, game)) continue;

		Rect card = shown_card(ViewMode::grid, game);
		card.y += t_y_offset;

		if (!card.overlaps_vertically(m_bounds)) continue;

		const CardState state = card_state(ViewMode::grid, game, card);
		draw_card(t_draw_list, grown_grid_card(card, game), m_library.game(game), state.highlighted, false, t_alpha);
	}

	if (!m_reorder.lifted) {
		draw_raised(t_draw_list, ViewMode::grid, t_alpha, t_y_offset);
	}

	t_draw_list.pop_clip();

	draw_wrap_scroll(t_draw_list, t_alpha);
}

void Carousel::draw_list_row(DrawList &t_draw_list, Rect t_row, u32 t_game, bool t_highlighted, u8 t_alpha) const
{
	const Font &font = m_fonts.body();
	const Game &game = m_library.game(t_game);
	const Color image_tint = faded(color_image, t_alpha);

	t_draw_list.add_rounded_rect(t_row, rounded(list_corner_radius),
								 faded(t_highlighted ? theme().control : theme().popup, t_alpha));

	const Rect thumb = list_thumb(t_row);
	const bool detached = static_cast<i32>(t_game) == m_detached_game;

	if (!detached && game.icon != nullptr) {
		t_draw_list.add_image(thumb, game.icon, image_tint, rounded(list_corner_radius));
	} else if (!detached && game.banner != nullptr) {
		t_draw_list.add_image(thumb, game.banner, image_tint, rounded(list_corner_radius),
							  cover_uv(1.0f, game.banner->aspect()));
	}

	draw_text_truncated(t_draw_list, font, Vec2{thumb.right() + 16.0f, font.centered_baseline(t_row)}, game.title,
						t_row.right() - 16.0f - (thumb.right() + 16.0f), faded(theme().text, t_alpha));
}

void Carousel::draw_list_mode(DrawList &t_draw_list, u8 t_alpha, float t_y_offset) const
{
	t_draw_list.push_clip(m_bounds);

	for (u32 game = 0; game < game_count(); game += 1) {
		if (is_raised(ViewMode::list, game)) continue;

		Rect row = shown_card(ViewMode::list, game);
		row.y += t_y_offset;

		if (!row.overlaps_vertically(m_bounds)) continue;

		draw_list_row(t_draw_list, row, game, card_state(ViewMode::list, game, row).highlighted, t_alpha);
	}

	if (!m_reorder.lifted) {
		draw_raised(t_draw_list, ViewMode::list, t_alpha, t_y_offset);
	}

	t_draw_list.pop_clip();

	draw_wrap_scroll(t_draw_list, t_alpha);
}

void Carousel::draw_raised(DrawList &t_draw_list, ViewMode t_mode, u8 t_alpha, float t_y_offset) const
{
	if (m_reorder.game < 0 || t_mode != m_mode) return;

	const auto game = static_cast<u32>(m_reorder.game);
	const bool list = t_mode == ViewMode::list;
	const float radius = list ? list_corner_radius : card_corner_radius;

	if (!list && m_reorder.game == m_detached_game) return;

	if (m_reorder.active) {
		t_draw_list.add_rounded_rect(slot_rect(t_mode, m_slot_of[game]), rounded(radius),
									 faded(with_alpha(m_settings.accent, placeholder_alpha), t_alpha));
	}

	Rect card = shown_card(t_mode, game);
	card.y += t_y_offset;

	const CardState state = card_state(t_mode, game, card);

	if (t_mode == ViewMode::grid) {
		card = grown_grid_card(card, game);
	}

	const Rect lifted = scaled_from_center(card, lift_scale(game));

	controls::draw_panel_shadow(t_draw_list, lifted, radius, m_lift * (t_alpha / 255.0f));

	if (list) {
		draw_list_row(t_draw_list, lifted, game, state.highlighted, t_alpha);
	} else {
		draw_card(t_draw_list, lifted, m_library.game(game), state.highlighted, state.centered, t_alpha);
	}
}

void Carousel::draw_reorder_hint(DrawList &t_draw_list) const
{
	if (m_reorder_hint <= 0.01f) return;

	constexpr std::string_view place = "Release to place";
	constexpr std::string_view cancel = "Esc to cancel";

	const Font &font = m_fonts.secondary();
	const float width = text_width(font, place) + hint_dot_gap * 2.0f + hint_dot_size + text_width(font, cancel) +
						hint_padding_x * 2.0f;
	const float height = font.line_height() + hint_padding_y * 2.0f;
	const float rise = (1.0f - m_reorder_hint) * hint_rise;
	const Rect pill{snapped_to_pixel(m_bounds.center().x - width * 0.5f),
					snapped_to_pixel(m_bounds.bottom() - hint_margin - height + rise), width, height};
	const auto alpha = static_cast<u8>(255.0f * m_reorder_hint);
	const float baseline = font.centered_baseline(pill);

	controls::draw_popup_shadow(t_draw_list, pill, height * 0.5f, m_reorder_hint);
	t_draw_list.add_bordered_rect(pill, rounded(height * 0.5f), faded(theme().popup, alpha),
								  faded(theme().border, alpha), 1.0f);

	float x = pill.x + hint_padding_x;
	draw_text(t_draw_list, font, Vec2{x, baseline}, place, faded(theme().text, alpha));
	x += text_width(font, place) + hint_dot_gap;

	t_draw_list.add_rounded_rect(Rect{x, pill.center().y - hint_dot_size * 0.5f, hint_dot_size, hint_dot_size},
								 rounded(hint_dot_size * 0.5f), faded(theme().text_faint, alpha));
	x += hint_dot_size + hint_dot_gap;

	draw_text(t_draw_list, font, Vec2{x, baseline}, cancel, faded(theme().text_dim, alpha));
}

void Carousel::draw_mode(DrawList &t_draw_list, ViewMode t_mode, u8 t_alpha, float t_y_offset) const
{
	switch (t_mode) {
		case ViewMode::carousel:
			draw_carousel_mode(t_draw_list, t_alpha, t_y_offset);
			break;
		case ViewMode::grid:
			draw_grid_mode(t_draw_list, t_alpha, t_y_offset);
			break;
		case ViewMode::list:
			draw_list_mode(t_draw_list, t_alpha, t_y_offset);
			break;
	}
}

void Carousel::draw_status_bar(DrawList &t_draw_list) const
{
	if (game_count() == 0) return;

	const Font &font = m_fonts.secondary();
	const Rect indicator = status_indicator_rect();
	const Rect icon{indicator.x, indicator.y + (indicator.h - status_icon_size) * 0.5f, status_icon_size,
					status_icon_size};

	t_draw_list.add_image(icon, m_assets.get(mode_icon(m_mode)), theme().text_dim);
	draw_text(t_draw_list, font,
			  Vec2{icon.right() + status_icon_gap, font.centered_baseline(indicator) - baseline_nudge},
			  mode_name(m_mode), theme().text_dim);
}

void Carousel::draw_switcher_rows(DrawList &t_draw_list, Rect t_panel, u8 t_alpha) const
{
	const Font &font = m_fonts.body();

	for (u32 index = 0; index < switcher_row_count; index += 1) {
		const ViewMode mode = switcher_row_modes[index];
		const Rect row = switcher_row_rect(t_panel, index);
		const bool active = mode == m_mode;

		if (active) {
			t_draw_list.add_rounded_rect(row, rounded(6.0f), faded(theme().row_selected, t_alpha));
			t_draw_list.add_rounded_rect(Rect{row.x + 2.0f, row.y + 3.0f, 3.0f, row.h - 6.0f}, rounded(1.5f),
										 faded(m_settings.accent, t_alpha));
		}

		const Color content = faded(active ? theme().text : theme().text_dim, t_alpha);
		const float icon_center_y = row.center().y + font.ascent() * 0.15f;
		const Rect icon{row.x + switcher_content_inset, icon_center_y - switcher_icon_size * 0.5f, switcher_icon_size,
						switcher_icon_size};

		t_draw_list.add_image(icon, m_assets.get(mode_icon(mode)), content);
		draw_text(t_draw_list, font, Vec2{icon.right() + switcher_icon_gap, font.centered_baseline(row)},
				  mode_name(mode), content);
	}
}

void Carousel::draw_switcher_slider(DrawList &t_draw_list, Rect t_panel, u8 t_alpha) const
{
	const Font &font = m_fonts.secondary();
	const Rect track = switcher_track_rect(t_panel);

	t_draw_list.add_rounded_rect(track, rounded(track.w * 0.5f), faded(theme().track, t_alpha));

	for (i32 stop = 0; stop < zoom_stop_count; stop += 1) {
		const float tick_y = track.y + track.h * (1.0f - stop_percent(stop) / 100.0f);
		t_draw_list.add_rect(Rect{track.x - 3.0f, tick_y - 0.75f, track.w + 6.0f, 1.5f},
							 faded(with_alpha(theme().text_faint, switcher_tick_alpha), t_alpha));
	}

	char buffer[8];
	const int written = std::snprintf(buffer, sizeof(buffer), "%d%%", static_cast<int>(m_zoom_percent + 0.5f));
	const std::string_view percent{buffer, static_cast<usize>(std::max(written, 0))};

	const float indicator_width = text_width(font, percent) + switcher_indicator_padding * 2.0f;
	const float indicator_center_y = track.y + track.h * (1.0f - std::clamp(m_zoom_percent / 100.0f, 0.0f, 1.0f));
	const Rect indicator{track.center().x - indicator_width * 0.5f,
						 indicator_center_y - switcher_indicator_height * 0.5f, indicator_width,
						 switcher_indicator_height};

	t_draw_list.add_bordered_rect(indicator.inset(-1.0f), rounded(indicator.h * 0.5f + 1.0f),
								  faded(m_settings.accent, t_alpha), faded(outline_on(m_settings.accent), t_alpha),
								  1.0f);
	draw_text(t_draw_list, font,
			  Vec2{indicator.x + switcher_indicator_padding, font.centered_baseline(indicator) - 1.0f}, percent,
			  faded(foreground_on(m_settings.accent), t_alpha));
}

void Carousel::draw_switcher(DrawList &t_draw_list) const
{
	if (m_switcher_shown <= 0.001f) return;

	const auto alpha = static_cast<u8>(255.0f * m_switcher_shown);
	Rect panel = switcher_panel_rect();
	panel.y += snapped_to_pixel((1.0f - m_switcher_shown) * switcher_slide_distance);

	controls::draw_popup_shadow(t_draw_list, panel, switcher_radius, m_switcher_shown);
	t_draw_list.add_bordered_rect(panel, rounded(switcher_radius), faded(theme().popup, alpha),
								  faded(theme().border, alpha), 1.0f);

	draw_switcher_rows(t_draw_list, panel, alpha);
	draw_switcher_slider(t_draw_list, panel, alpha);
}

void Carousel::draw(DrawList &t_draw_list)
{
	PULSAR_PROFILE_SCOPE("Carousel.Draw");

	if (m_mode_transition > 0.001f) {
		const float slide = snapped_to_pixel(m_mode_transition * mode_slide_distance);

		draw_mode(t_draw_list, m_previous_mode, static_cast<u8>(255.0f * m_mode_transition), -slide);
		draw_mode(t_draw_list, m_mode, static_cast<u8>(255.0f * (1.0f - m_mode_transition)), slide);
	} else {
		draw_mode(t_draw_list, m_mode, 255, 0.0f);
	}

	if (m_reorder.lifted) {
		draw_raised(t_draw_list, m_mode, 255, 0.0f);
	}

	draw_reorder_hint(t_draw_list);

	if (game_count() > 0) {
		draw_switcher(t_draw_list);
	}
}
