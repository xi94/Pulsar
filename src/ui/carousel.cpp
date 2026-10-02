#include "ui/carousel.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numbers>
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
constexpr float K_CARD_WIDTH            = 220.0f;
constexpr float K_CARD_HEIGHT           = 300.0f;
constexpr float K_CARD_SPACING          = 36.0f;
constexpr float K_CARD_CORNER_RADIUS    = 14.0f;
constexpr float K_DRAG_PIXELS_PER_CARD  = K_CARD_WIDTH + K_CARD_SPACING;
constexpr float K_SCROLL_EASE_RATE      = 12.0f;
constexpr float K_MODE_MORPH_SECONDS    = 0.34f;
constexpr float K_ZOOM_EASE_RATE        = 9.0f;
constexpr float K_EDGE_FADE_WIDTH       = 64.0f;
constexpr float K_CAROUSEL_EDGE_PADDING = 40.0f;
constexpr float K_SHELF_EASE_RATE       = 12.0f;

constexpr float K_OVERVIEW_PADDING            = 28.0f;
constexpr float K_OVERVIEW_HINT_ROOM          = 48.0f;
constexpr float K_OVERVIEW_GAP                = 18.0f;
constexpr float K_OVERVIEW_EASE_RATE          = 11.0f;
constexpr float K_OVERVIEW_RETARGET_THRESHOLD = 0.9f;
constexpr float K_LIFT_EASE_RATE              = 16.0f;
constexpr float K_LIFT_GROWTH                 = 0.06f;
constexpr float K_HINT_EASE_RATE              = 14.0f;
constexpr float K_REORDER_STIFFNESS           = 420.0f;
constexpr float K_REORDER_DAMPING_RATIO       = 0.86f;
constexpr float K_REORDER_SCROLL_ZONE         = 48.0f;
constexpr float K_REORDER_SCROLL_SPEED        = 720.0f;
constexpr float K_LONG_PRESS_SECONDS          = 0.5f;
constexpr float K_LONG_PRESS_SLOP             = 6.0f;
constexpr float K_PRESS_DELAY_SECONDS         = 0.08f;
constexpr float K_PRESS_LIFT                  = 0.8f;
constexpr u8    K_PLACEHOLDER_ALPHA           = 34;
constexpr float K_HINT_PADDING_X              = 14.0f;
constexpr float K_HINT_PADDING_Y              = 7.0f;
constexpr float K_HINT_MARGIN                 = 14.0f;
constexpr float K_HINT_RISE                   = 8.0f;
constexpr float K_HINT_DOT_SIZE               = 3.0f;
constexpr float K_HINT_DOT_GAP                = 9.0f;

constexpr Vec2  K_REFERENCE_VIEW_SIZE{1042.0f, 609.0f};
constexpr float K_MIN_VIEW_SCALE = 0.75f;
constexpr float K_MAX_VIEW_SCALE = 1.5f;

constexpr Vec2  K_GRID_CARD_MIN_SIZE{160.0f, 220.0f};
constexpr Vec2  K_GRID_CARD_MAX_SIZE{224.0f, 308.0f};
constexpr float K_GRID_GAP        = 24.0f;
constexpr float K_GRID_PADDING    = 24.0f;
constexpr float K_HOVER_GROWTH    = 0.06f;
constexpr float K_HOVER_EASE_RATE = 14.0f;

constexpr float K_LIST_THUMB_MIN_SIZE = 56.0f;
constexpr float K_LIST_THUMB_MAX_SIZE = 96.0f;
constexpr float K_LIST_ROW_PADDING_Y  = 14.0f;
constexpr float K_LIST_PADDING        = 16.0f;
constexpr float K_LIST_GAP            = 8.0f;
constexpr float K_LIST_CORNER_RADIUS  = 10.0f;

constexpr float K_ICON_ART_MIN_SIZE        = 48.0f;
constexpr float K_ICON_ART_MAX_SIZE        = 96.0f;
constexpr float K_ICON_ART_RADIUS_SHARE    = 0.18f;
constexpr float K_ICON_TILE_PADDING_X      = 20.0f;
constexpr float K_ICON_TILE_PADDING_TOP    = 12.0f;
constexpr float K_ICON_TILE_PADDING_BOTTOM = 10.0f;
constexpr float K_ICON_TILE_GAP            = 6.0f;
constexpr float K_ICON_TILE_RADIUS         = 8.0f;
constexpr float K_ICON_LABEL_GAP           = 8.0f;
constexpr float K_ICON_LABEL_INSET         = 6.0f;

constexpr float K_SWITCHER_HOLD_SECONDS      = 0.7f;
constexpr float K_SWITCHER_EASE_RATE         = 18.0f;
constexpr float K_SWITCHER_WIDTH             = 184.0f;
constexpr float K_SWITCHER_PADDING           = 6.0f;
constexpr float K_SWITCHER_RADIUS            = 10.0f;
constexpr float K_SWITCHER_MARGIN            = 16.0f;
constexpr float K_SWITCHER_SLIDE_DISTANCE    = 8.0f;
constexpr float K_SWITCHER_ICON_SIZE         = 24.0f;
constexpr float K_SWITCHER_ICON_GAP          = 10.0f;
constexpr float K_SWITCHER_CONTENT_INSET     = 12.0f;
constexpr float K_SWITCHER_ROW_RADIUS        = 6.0f;
constexpr float K_SWITCHER_HOVER_FILL        = 0.5f;
constexpr float K_SWITCHER_HOVER_BRIGHTENING = 0.5f;
constexpr float K_SIZE_DOT_SIZE              = 6.0f;
constexpr float K_SIZE_DOT_GAP               = 4.0f;
constexpr float K_SIZE_DOT_DIMMING           = 0.45f;
constexpr float K_SIZE_SLIDER_HEIGHT         = 30.0f;
constexpr float K_SIZE_SLIDER_FOLD_RATE      = 16.0f;
constexpr float K_SIZE_SLIDER_RISE           = 6.0f;
constexpr float K_SIZE_SLIDER_TRACK_HEIGHT   = 4.0f;
constexpr float K_SIZE_SLIDER_TICK_SIZE      = 4.0f;
constexpr float K_SIZE_SLIDER_THUMB_SIZE     = 12.0f;
constexpr float K_SIZE_SLIDER_THUMB_RING     = 2.0f;
constexpr float K_SIZE_SLIDER_SMALL_ICON     = 12.0f;
constexpr float K_SIZE_SLIDER_LARGE_ICON     = 18.0f;
constexpr float K_SIZE_SLIDER_ICON_GAP       = 8.0f;
constexpr float K_SIZE_SLIDER_ICON_REACH     = 4.0f;

constexpr float K_STATUS_ICON_SIZE     = 16.0f;
constexpr float K_STATUS_ICON_GAP      = 8.0f;
constexpr float K_STATUS_PADDING_RIGHT = 14.0f;
constexpr float K_BASELINE_NUDGE       = 2.0f;

constexpr Color K_COLOR_IMAGE{255, 255, 255, 255};
constexpr u8    K_CARD_BORDER_ALPHA             = 160;
constexpr u8    K_CARD_BORDER_HIGHLIGHTED_ALPHA = 235;

constexpr i32 K_ZOOM_STOP_COUNT  = 12;
constexpr i32 K_SHELF_STOP       = 1;
constexpr i32 K_SPREAD_STOP      = 2;
constexpr i32 K_GRID_FIRST_STOP  = 3;
constexpr i32 K_GRID_LAST_STOP   = 5;
constexpr i32 K_ICONS_FIRST_STOP = 6;
constexpr i32 K_ICONS_LAST_STOP  = 8;
constexpr i32 K_LIST_FIRST_STOP  = 9;
constexpr i32 K_LIST_LAST_STOP   = 11;

struct SwitcherRow {
	std::string_view name;
	Asset            icon;
	i32              first_stop;
	i32              last_stop;
};

constexpr SwitcherRow K_SWITCHER_ROWS[]{
	{"List", Asset::IconList, K_LIST_FIRST_STOP, K_LIST_LAST_STOP},
	{"Icons", Asset::IconIcons, K_ICONS_FIRST_STOP, K_ICONS_LAST_STOP},
	{"Grid", Asset::IconGrid, K_GRID_FIRST_STOP, K_GRID_LAST_STOP},
	{"Shelf", Asset::IconShelf, K_SHELF_STOP, K_SPREAD_STOP},
	{"Carousel", Asset::IconCarousel, 0, 0},
};
constexpr u32 K_SWITCHER_ROW_COUNT = static_cast<u32>(std::size(K_SWITCHER_ROWS));

[[nodiscard]] auto row_index_at_stop(i32 t_stop) -> u32
{
	for (u32 index = 0; index < K_SWITCHER_ROW_COUNT; index += 1) {
		if (t_stop >= K_SWITCHER_ROWS[index].first_stop && t_stop <= K_SWITCHER_ROWS[index].last_stop) return index;
	}

	return K_SWITCHER_ROW_COUNT - 1;
}

[[nodiscard]] auto row_at_stop(i32 t_stop) -> const SwitcherRow&
{
	return K_SWITCHER_ROWS[row_index_at_stop(t_stop)];
}

[[nodiscard]] auto has_sizes(const SwitcherRow& t_row) -> bool
{
	return t_row.last_stop > t_row.first_stop;
}

[[nodiscard]] auto is_shelf_stop(i32 t_stop) -> bool
{
	return t_stop == K_SHELF_STOP || t_stop == K_SPREAD_STOP;
}

[[nodiscard]] auto mode_at_stop(i32 t_stop) -> ViewMode
{
	if (t_stop <= K_SPREAD_STOP) return ViewMode::Carousel;
	if (t_stop <= K_GRID_LAST_STOP) return ViewMode::Grid;
	if (t_stop <= K_ICONS_LAST_STOP) return ViewMode::Icons;

	return ViewMode::List;
}

[[nodiscard]] auto uses_frame(ViewMode t_mode) -> bool
{
	return t_mode == ViewMode::List || t_mode == ViewMode::Icons;
}

[[nodiscard]] auto stop_percent(i32 t_stop) -> float
{
	return static_cast<float>(t_stop) / (K_ZOOM_STOP_COUNT - 1) * 100.0f;
}

[[nodiscard]] auto zoom_within(float t_percent, i32 t_first_stop, i32 t_last_stop) -> float
{
	const float first = stop_percent(t_first_stop);
	const float last  = stop_percent(t_last_stop);

	return std::clamp((t_percent - first) / (last - first), 0.0f, 1.0f);
}

[[nodiscard]] auto view_scale_for(Rect t_bounds) -> float
{
	const float fit = std::min(t_bounds.w / K_REFERENCE_VIEW_SIZE.x, t_bounds.h / K_REFERENCE_VIEW_SIZE.y);

	return std::clamp(fit, K_MIN_VIEW_SCALE, K_MAX_VIEW_SCALE);
}

[[nodiscard]] auto grid_card_size(float t_zoom_percent, float t_view_scale) -> Vec2
{
	const float t = zoom_within(t_zoom_percent, K_GRID_FIRST_STOP, K_GRID_LAST_STOP);

	return Vec2{(K_GRID_CARD_MIN_SIZE.x + (K_GRID_CARD_MAX_SIZE.x - K_GRID_CARD_MIN_SIZE.x) * t) * t_view_scale,
	            (K_GRID_CARD_MIN_SIZE.y + (K_GRID_CARD_MAX_SIZE.y - K_GRID_CARD_MIN_SIZE.y) * t) * t_view_scale};
}

[[nodiscard]] auto list_thumb_size(float t_zoom_percent, float t_view_scale) -> float
{
	const float t = zoom_within(t_zoom_percent, K_LIST_FIRST_STOP, K_LIST_LAST_STOP);

	return snapped_to_pixel((K_LIST_THUMB_MIN_SIZE + (K_LIST_THUMB_MAX_SIZE - K_LIST_THUMB_MIN_SIZE) * t) * t_view_scale);
}

[[nodiscard]] auto icon_art_size(float t_zoom_percent, float t_view_scale) -> float
{
	const float t = zoom_within(t_zoom_percent, K_ICONS_FIRST_STOP, K_ICONS_LAST_STOP);

	return snapped_to_pixel((K_ICON_ART_MIN_SIZE + (K_ICON_ART_MAX_SIZE - K_ICON_ART_MIN_SIZE) * t) * t_view_scale);
}

[[nodiscard]] auto art_radius(ViewMode t_mode, Rect t_art) -> float
{
	switch (t_mode) {
		case ViewMode::List:
			return K_LIST_CORNER_RADIUS;
		case ViewMode::Icons:
			return t_art.w * K_ICON_ART_RADIUS_SHARE;
		case ViewMode::Carousel:
		case ViewMode::Grid:
			break;
	}

	return K_CARD_CORNER_RADIUS;
}

auto draw_centered_label(DrawList*        t_draw_list,
                         const Font&      t_font,
                         float            t_center_x,
                         float            t_baseline,
                         std::string_view t_text,
                         float            t_max_width,
                         Color            t_color) -> void
{
	const float width = std::min(text_width(t_font, t_text), t_max_width);

	draw_text_truncated(t_draw_list, t_font, Vec2{snapped_to_pixel(t_center_x - width * 0.5f), t_baseline}, t_text, t_max_width, t_color);
}

[[nodiscard]] auto list_row_height(float t_zoom_percent, float t_view_scale) -> float
{
	return list_thumb_size(t_zoom_percent, t_view_scale) + K_LIST_ROW_PADDING_Y * 2.0f;
}

[[nodiscard]] auto grid_columns(float t_width, float t_zoom_percent, float t_view_scale) -> u32
{
	const float usable              = t_width - K_GRID_PADDING * 2.0f + K_GRID_GAP;
	const float card_width_with_gap = grid_card_size(t_zoom_percent, t_view_scale).x + K_GRID_GAP;

	return std::max<u32>(1, static_cast<u32>(usable / card_width_with_gap));
}

[[nodiscard]] auto card_scale(float t_slots_from_center) -> float
{
	const float closeness = std::max(0.0f, 1.0f - std::min(t_slots_from_center, 2.0f) / 2.0f);

	return 0.90f + 0.28f * closeness;
}

[[nodiscard]] auto center_offset_of_slot(float t_slot) -> float
{
	constexpr i32 INTEGRATION_STEPS = 24;

	const float distance = std::fabs(t_slot);
	if (distance < 0.0001f) return 0.0f;

	const float step           = distance / INTEGRATION_STEPS;
	float       covered        = 0.0f;
	float       previous_width = K_CARD_WIDTH * card_scale(0.0f);

	for (i32 i = 1; i <= INTEGRATION_STEPS; i += 1) {
		const float width = K_CARD_WIDTH * card_scale(step * i);
		covered += (previous_width + width) * 0.5f * step;
		previous_width = width;
	}

	return std::copysign(covered + distance * K_CARD_SPACING, t_slot);
}

struct CardLook {
	Color border;
	float border_thickness;
	float glow_size;
	u8    glow_alpha;
};

[[nodiscard]] auto card_look(bool t_highlighted, bool t_centered) -> CardLook
{
	if (!t_highlighted) return CardLook{with_alpha(g_theme.border, K_CARD_BORDER_ALPHA), 2.0f, 0.0f, 0};
	if (t_centered) return CardLook{with_alpha(g_theme.text, K_CARD_BORDER_HIGHLIGHTED_ALPHA), 2.5f, 18.0f, 255};

	return CardLook{with_alpha(g_theme.text, K_CARD_BORDER_HIGHLIGHTED_ALPHA), 2.0f, 14.0f, 225};
}

[[nodiscard]] auto blended_look(const CardLook& t_from, const CardLook& t_to, float t_amount) -> CardLook
{
	const Color from  = t_from.border_thickness > 0.0f ? t_from.border : with_alpha(t_to.border, 0);
	const Color to    = t_to.border_thickness > 0.0f ? t_to.border : with_alpha(t_from.border, 0);
	const auto  blend = [t_amount](float t_start, float t_end) { return t_start + (t_end - t_start) * t_amount; };

	return CardLook{with_alpha(mix(from, to, t_amount), static_cast<u8>(blend(from.a, to.a))), blend(t_from.border_thickness, t_to.border_thickness),
	                blend(t_from.glow_size, t_to.glow_size), static_cast<u8>(blend(t_from.glow_alpha, t_to.glow_alpha))};
}

auto draw_framed_art(DrawList* t_draw_list, Rect t_rect, const Game& t_game, const CardLook& t_look, float t_radius, float t_icon_share, u8 t_alpha) -> void
{
	if (t_look.glow_size > 0.0f && t_look.glow_alpha > 0) {
		t_draw_list->add_banner_glow(t_rect, t_radius, t_look.glow_size, faded(faded(t_game.accent, t_look.glow_alpha), t_alpha));
	}

	if (t_look.border_thickness > 0.0f) {
		t_draw_list->add_rounded_rect(t_rect, rounded(t_radius), faded(t_look.border, t_alpha));
	}

	const Rect        art        = t_rect.inset(t_look.border_thickness);
	const CornerRadii radii      = rounded(std::max(0.0f, t_radius - t_look.border_thickness));
	const float       icon_share = t_game.icon != nullptr ? t_icon_share : 0.0f;

	if (t_game.banner != nullptr && icon_share < 1.0f) {
		t_draw_list->add_image(art, t_game.banner, faded(K_COLOR_IMAGE, static_cast<u8>(t_alpha * (1.0f - icon_share))), radii,
		                       cover_uv(art.w / art.h, t_game.banner->aspect()));
	}

	if (icon_share > 0.0f) {
		t_draw_list->add_image(art, t_game.icon, faded(K_COLOR_IMAGE, static_cast<u8>(t_alpha * icon_share)), radii,
		                       cover_uv(art.w / art.h, t_game.icon->aspect()));
	}
}

[[nodiscard]] auto size_slider_center_y(Rect t_slider) -> float
{
	return t_slider.y + (t_slider.h - K_SWITCHER_PADDING) * 0.5f;
}

[[nodiscard]] auto size_slider_icon_rect(Rect t_slider, bool t_large) -> Rect
{
	const float size = t_large ? K_SIZE_SLIDER_LARGE_ICON : K_SIZE_SLIDER_SMALL_ICON;
	const float x    = t_large ? t_slider.right() - K_SWITCHER_CONTENT_INSET - size : t_slider.x + K_SWITCHER_CONTENT_INSET;

	return Rect{x, snapped_to_pixel(size_slider_center_y(t_slider) - size * 0.5f), size, size};
}

[[nodiscard]] auto size_slider_track_rect(Rect t_slider) -> Rect
{
	const float left  = size_slider_icon_rect(t_slider, false).right() + K_SIZE_SLIDER_ICON_GAP;
	const float right = size_slider_icon_rect(t_slider, true).x - K_SIZE_SLIDER_ICON_GAP;

	return Rect{left, snapped_to_pixel(size_slider_center_y(t_slider) - K_SIZE_SLIDER_TRACK_HEIGHT * 0.5f), std::max(0.0f, right - left),
	            K_SIZE_SLIDER_TRACK_HEIGHT};
}

[[nodiscard]] auto size_slider_grab_rect(Rect t_slider) -> Rect
{
	const Rect track = size_slider_track_rect(t_slider);

	return Rect{track.x - K_SIZE_SLIDER_THUMB_SIZE * 0.5f, t_slider.y, track.w + K_SIZE_SLIDER_THUMB_SIZE, t_slider.h};
}

[[nodiscard]] auto stop_at_track_x(const SwitcherRow& t_row, Rect t_track, float t_x) -> i32
{
	const float t = t_track.w > 0.0f ? std::clamp((t_x - t_track.x) / t_track.w, 0.0f, 1.0f) : 0.0f;

	return t_row.first_stop + static_cast<i32>(std::round(t * static_cast<float>(t_row.last_stop - t_row.first_stop)));
}
}

Carousel::Carousel(const Library* t_library, const Settings* t_settings, const Fonts* t_fonts, const Assets* t_assets, CommandQueue* t_commands)
	: m_library(t_library)
	, m_settings(t_settings)
	, m_fonts(t_fonts)
	, m_assets(t_assets)
	, m_commands(t_commands)
{
	for (u32 i = 0; i < K_MAX_GAMES; i += 1) {
		m_order[i]   = static_cast<u8>(i);
		m_slot_of[i] = static_cast<u8>(i);
	}
}

auto Carousel::restore(i32 t_zoom_stop, i32 t_selected_game) -> void
{
	m_zoom_stop       = std::clamp(t_zoom_stop, 0, K_ZOOM_STOP_COUNT - 1);
	m_zoom_percent    = stop_percent(m_zoom_stop);
	m_mode            = mode_at_stop(m_zoom_stop);
	m_previous_mode   = m_mode;
	m_shelf           = is_shelf_stop(m_zoom_stop) ? 1.0f : 0.0f;
	m_spread          = m_zoom_stop == K_SPREAD_STOP ? 1.0f : 0.0f;
	m_mode_transition = 0.0f;

	const i32 game  = std::clamp(t_selected_game, 0, std::max(0, static_cast<i32>(m_library->game_count) - 1));
	m_scroll        = clamp_scroll(m_library->game_count > 0 ? static_cast<float>(m_slot_of[game]) : 0.0f);
	m_target_scroll = m_scroll;
	m_focused_game  = selected_game();
}

auto Carousel::set_order(std::span<const u8> t_order) -> void
{
	bool used[K_MAX_GAMES]{};
	u32  count = 0;

	for (const u8 game : t_order) {
		if (game >= m_library->game_count || used[game]) continue;

		used[game]     = true;
		m_order[count] = game;
		count += 1;
	}

	for (u32 game = 0; game < m_library->game_count; game += 1) {
		if (used[game]) continue;

		m_order[count] = static_cast<u8>(game);
		count += 1;
	}

	for (u32 slot = 0; slot < count; slot += 1) {
		m_slot_of[m_order[slot]] = static_cast<u8>(slot);
	}
}

auto Carousel::selected_game() const -> i32
{
	if (m_library->game_count == 0) return 0;

	return m_order[static_cast<u32>(clamp_scroll(std::round(m_target_scroll)))];
}

auto Carousel::clamp_scroll(float t_offset) const -> float
{
	return m_library->game_count == 0 ? 0.0f : std::clamp(t_offset, 0.0f, static_cast<float>(m_library->game_count - 1));
}

auto Carousel::view_scale() const -> float
{
	return view_scale_for(m_bounds);
}

auto Carousel::centered_carousel_slot(float t_offset) const -> Rect
{
	const float view     = view_scale();
	const float scale    = card_scale(std::fabs(t_offset)) * view;
	const float width    = K_CARD_WIDTH * scale;
	const float height   = K_CARD_HEIGHT * scale;
	const float center_x = m_bounds.center().x + center_offset_of_slot(t_offset) * view;

	return Rect{center_x - width * 0.5f, m_bounds.y + (m_bounds.h - height) * 0.5f, width, height};
}

auto Carousel::carousel_camera_shift() const -> float
{
	if (m_library->game_count == 0) return 0.0f;

	const Rect  first       = centered_carousel_slot(-m_scroll);
	const Rect  last        = centered_carousel_slot(static_cast<float>(m_library->game_count - 1) - m_scroll);
	const float padding     = K_CAROUSEL_EDGE_PADDING * view_scale();
	const float left_limit  = m_bounds.x + padding;
	const float right_limit = m_bounds.right() - padding;

	if (last.right() - first.x <= right_limit - left_limit) {
		return m_bounds.center().x - (first.x + last.right()) * 0.5f;
	}

	return std::clamp(0.0f, right_limit - last.right(), left_limit - first.x);
}

auto Carousel::carousel_slot(float t_offset) const -> Rect
{
	const Rect centered = centered_carousel_slot(t_offset);

	return m_shelf > 0.0f ? centered.moved(Vec2{carousel_camera_shift() * m_shelf, 0.0f}) : centered;
}

auto Carousel::overview_slot(u32 t_slot) const -> Rect
{
	const u32   count  = std::max<u32>(1, m_library->game_count);
	const float aspect = K_CARD_WIDTH / K_CARD_HEIGHT;
	const Rect  area{m_bounds.x + K_OVERVIEW_PADDING, m_bounds.y + K_OVERVIEW_PADDING, m_bounds.w - K_OVERVIEW_PADDING * 2.0f,
	                 m_bounds.h - K_OVERVIEW_PADDING * 2.0f - K_OVERVIEW_HINT_ROOM * m_reorder_hint};

	const float by_width  = (area.w - (count - 1) * K_OVERVIEW_GAP) / count;
	const float width     = std::min({by_width, area.h * aspect, K_CARD_WIDTH * view_scale()});
	const float height    = width / aspect;
	const float row_width = count * width + (count - 1) * K_OVERVIEW_GAP;

	return Rect{area.center().x - row_width * 0.5f + t_slot * (width + K_OVERVIEW_GAP), area.center().y - height * 0.5f, width, height};
}

auto Carousel::grid_slot(u32 t_slot) const -> Rect
{
	const Vec2 size    = grid_card_size(m_zoom_percent, view_scale());
	const u32  columns = grid_columns(m_bounds.w, m_zoom_percent, view_scale());
	const u32  column  = t_slot % columns;
	const u32  row     = t_slot / columns;

	const float total_width = columns * size.x + (columns - 1) * K_GRID_GAP;
	const float x           = m_bounds.x + (m_bounds.w - total_width) * 0.5f + column * (size.x + K_GRID_GAP);
	const float y           = m_bounds.y + K_GRID_PADDING + row * (size.y + K_GRID_GAP) - m_wrap_scroll.offset();

	return Rect{x, y, size.x, size.y};
}

auto Carousel::list_slot(u32 t_slot) const -> Rect
{
	const float height = list_row_height(m_zoom_percent, view_scale());
	const float y      = m_bounds.y + K_LIST_PADDING + t_slot * (height + K_LIST_GAP) - m_wrap_scroll.offset();

	return Rect{m_bounds.x + K_LIST_PADDING, y, m_bounds.w - K_LIST_PADDING * 2.0f, height};
}

auto Carousel::icon_tile_size() const -> Vec2
{
	const float art = icon_art_size(m_zoom_percent, view_scale());
	return Vec2{art + K_ICON_TILE_PADDING_X * 2.0f,
	            K_ICON_TILE_PADDING_TOP + art + K_ICON_LABEL_GAP + m_fonts->body.line_height() + K_ICON_TILE_PADDING_BOTTOM};
}

auto Carousel::icon_columns() const -> u32
{
	const float usable = m_bounds.w - K_GRID_PADDING * 2.0f + K_ICON_TILE_GAP;

	return std::max<u32>(1, static_cast<u32>(usable / (icon_tile_size().x + K_ICON_TILE_GAP)));
}

auto Carousel::icon_slot(u32 t_slot) const -> Rect
{
	const Vec2  tile    = icon_tile_size();
	const u32   columns = icon_columns();
	const float x       = m_bounds.x + K_GRID_PADDING + (t_slot % columns) * (tile.x + K_ICON_TILE_GAP);
	const float y       = m_bounds.y + K_GRID_PADDING + (t_slot / columns) * (tile.y + K_ICON_TILE_GAP) - m_wrap_scroll.offset();

	return Rect{x, y, tile.x, tile.y};
}

auto Carousel::icon_tile_art(Rect t_tile) const -> Rect
{
	const float art = std::max(0.0f, t_tile.w - K_ICON_TILE_PADDING_X * 2.0f);

	return Rect{t_tile.center().x - art * 0.5f, t_tile.y + K_ICON_TILE_PADDING_TOP, art, art};
}

auto Carousel::wrap_columns() const -> u32
{
	switch (m_mode) {
		case ViewMode::Grid:
			return grid_columns(m_bounds.w, m_zoom_percent, view_scale());
		case ViewMode::Icons:
			return icon_columns();
		case ViewMode::Carousel:
		case ViewMode::List:
			break;
	}

	return 0;
}

auto Carousel::slot_rect(ViewMode t_mode, u32 t_slot) const -> Rect
{
	switch (t_mode) {
		case ViewMode::Grid:
			return grid_slot(t_slot);
		case ViewMode::List:
			return list_slot(t_slot);
		case ViewMode::Icons:
			return icon_slot(t_slot);
		case ViewMode::Carousel:
			break;
	}

	const Rect carousel = carousel_slot(static_cast<float>(t_slot) - m_scroll);

	const float overview = std::max(m_overview, m_spread);

	return overview > 0.0f ? lerp(carousel, overview_slot(t_slot), overview) : carousel;
}

auto Carousel::shown_card(ViewMode t_mode, u32 t_game) const -> Rect
{
	if (m_reorder.active && static_cast<i32>(t_game) == m_reorder.game && t_mode == m_mode) return dragged_rect();

	return slot_rect(t_mode, m_slot_of[t_game]).moved(m_card_offset[t_game]);
}

auto Carousel::dragged_rect() const -> Rect
{
	const Rect slot = slot_rect(m_mode, m_slot_of[m_reorder.game]);

	return Rect{m_mouse.x - m_reorder.grab.x * slot.w, m_mouse.y - m_reorder.grab.y * slot.h, slot.w, slot.h};
}

auto Carousel::lift_scale(u32 t_game) const -> float
{
	return static_cast<i32>(t_game) == m_reorder.game ? 1.0f + K_LIFT_GROWTH * m_lift : 1.0f;
}

auto Carousel::is_raised(ViewMode t_mode, u32 t_game) const -> bool
{
	return t_mode == m_mode && static_cast<i32>(t_game) == m_reorder.game;
}

auto Carousel::card_state(ViewMode t_mode, u32 t_game, Rect t_card) const -> Carousel::CardState
{
	if (m_reorder.active && static_cast<i32>(t_game) == m_reorder.game) return CardState{true, false};

	const bool hovered = !m_reorder.active && t_card.contains(m_mouse);
	if (t_mode != ViewMode::Carousel) return CardState{hovered || is_focus_shown(t_game), false};

	const bool centered = std::fabs(static_cast<float>(m_slot_of[t_game]) - m_scroll) < 0.5f && m_overview < 0.5f && m_spread < 0.5f;

	return CardState{centered || hovered, centered};
}

auto Carousel::list_thumb(Rect t_row) const -> Rect
{
	const float size = list_thumb_size(m_zoom_percent, view_scale());

	return Rect{t_row.x + 12.0f, t_row.y + (t_row.h - size) * 0.5f, size, size};
}

auto Carousel::grown(Rect t_card, u32 t_game) const -> Rect
{
	const float growth = 1.0f + K_HOVER_GROWTH * m_card_hover[t_game];

	return t_card.inset(t_card.w * (1.0f - growth) * 0.5f, t_card.h * (1.0f - growth) * 0.5f);
}

auto Carousel::art_rect(ViewMode t_mode, u32 t_game) const -> Rect
{
	const Rect card = shown_card(t_mode, t_game);

	switch (t_mode) {
		case ViewMode::List:
			return list_thumb(card);
		case ViewMode::Icons:
			return grown(icon_tile_art(card), t_game);
		case ViewMode::Carousel:
		case ViewMode::Grid:
			break;
	}

	return grown(card, t_game);
}

auto Carousel::morph_art(u32 t_game) const -> Rect
{
	const Rect current = art_rect(m_mode, t_game);

	return m_mode_transition > 0.0f ? lerp(m_morph_from_art[t_game], current, mode_morph()) : current;
}

auto Carousel::mode_morph() const -> float
{
	return 0.5f - 0.5f * std::cos((1.0f - m_mode_transition) * std::numbers::pi_v<float>);
}

auto Carousel::art_source(u32 t_game) const -> ArtSource
{
	const Rect  card  = shown_card(m_mode, t_game);
	const float scale = lift_scale(t_game);
	const Game& game  = m_library->games[t_game];

	if (uses_frame(m_mode)) {
		const Rect lifted = card.scaled_from_center(scale);
		const Rect art    = m_mode == ViewMode::List ? list_thumb(lifted) : grown(icon_tile_art(lifted), t_game);

		return ArtSource{.rect = art, .radius = art_radius(m_mode, art), .is_icon = game.icon != nullptr};
	}

	const CardState state = card_state(m_mode, t_game, card);
	const CardLook  look  = card_look(state.highlighted, state.centered);
	const Rect      art   = grown(card, t_game);

	return ArtSource{.rect         = art.scaled_from_center(scale),
	                 .radius       = K_CARD_CORNER_RADIUS,
	                 .is_icon      = false,
	                 .border       = look.border_thickness,
	                 .border_color = look.border,
	                 .glow         = look.glow_size,
	                 .glow_color   = faded(game.accent, look.glow_alpha)};
}

auto Carousel::wrap_content_height() const -> float
{
	if (m_library->game_count == 0) return 0.0f;

	if (m_mode == ViewMode::Grid) {
		const u32 columns = grid_columns(m_bounds.w, m_zoom_percent, view_scale());
		const u32 rows    = (m_library->game_count + columns - 1) / columns;

		return K_GRID_PADDING * 2.0f + rows * grid_card_size(m_zoom_percent, view_scale()).y + (rows - 1) * K_GRID_GAP;
	}

	if (m_mode == ViewMode::List) {
		return K_LIST_PADDING * 2.0f + m_library->game_count * list_row_height(m_zoom_percent, view_scale()) + (m_library->game_count - 1) * K_LIST_GAP;
	}

	if (m_mode == ViewMode::Icons) {
		const u32 rows = (m_library->game_count + icon_columns() - 1) / icon_columns();

		return K_GRID_PADDING * 2.0f + rows * icon_tile_size().y + (rows - 1) * K_ICON_TILE_GAP;
	}

	return 0.0f;
}

auto Carousel::wrap_scroll_geometry() const -> ScrollGeometry
{
	const Rect track{m_bounds.right() - K_SCROLLBAR_WIDTH - 8.0f, m_bounds.y + 8.0f, K_SCROLLBAR_WIDTH, m_bounds.h - 16.0f};

	return ScrollGeometry{track, wrap_content_height(), m_bounds.h};
}

auto Carousel::game_at(Vec2 t_point) const -> i32
{
	if (m_mode != ViewMode::Carousel && (t_point.y < m_bounds.y || t_point.y >= m_bounds.bottom())) return -1;

	i32   closest          = -1;
	float closest_distance = 0.0f;

	for (u32 game = 0; game < m_library->game_count; game += 1) {
		if (!shown_card(m_mode, game).contains(t_point)) continue;

		const float distance = std::fabs(static_cast<float>(m_slot_of[game]) - m_scroll);
		if (closest < 0 || distance < closest_distance) {
			closest          = static_cast<i32>(game);
			closest_distance = distance;
		}
	}

	return closest;
}

auto Carousel::status_indicator_rect() const -> Rect
{
	const float width = K_STATUS_ICON_SIZE + K_STATUS_ICON_GAP + text_width(m_fonts->secondary, row_at_stop(m_zoom_stop).name);

	return Rect{m_bounds.right() - K_STATUS_PADDING_RIGHT - width, m_bounds.bottom(), width, K_STATUS_BAR_HEIGHT};
}

auto Carousel::switcher_row_height() const -> float
{
	return m_fonts->body.line_height() + 10.0f;
}

auto Carousel::size_slider_shown_height() const -> float
{
	return snapped_to_pixel(K_SIZE_SLIDER_HEIGHT * m_size_slider_fold);
}

auto Carousel::switcher_panel_rect() const -> Rect
{
	const float height = K_SWITCHER_PADDING * 2.0f + switcher_row_height() * K_SWITCHER_ROW_COUNT + size_slider_shown_height();

	return Rect{m_bounds.right() - K_SWITCHER_WIDTH - K_SWITCHER_MARGIN, m_bounds.bottom() - height - K_SWITCHER_MARGIN, K_SWITCHER_WIDTH, height};
}

auto Carousel::switcher_row_rect(Rect t_panel, u32 t_row) const -> Rect
{
	const float row_height = switcher_row_height();
	const float fold       = t_row > row_index_at_stop(m_zoom_stop) ? size_slider_shown_height() : 0.0f;

	return Rect{t_panel.x + K_SWITCHER_PADDING, t_panel.y + K_SWITCHER_PADDING + row_height * t_row + fold, t_panel.w - K_SWITCHER_PADDING * 2.0f, row_height};
}

auto Carousel::switcher_active_pill(Rect t_panel) const -> Rect
{
	const Rect row = switcher_row_rect(t_panel, row_index_at_stop(m_zoom_stop));

	return Rect{row.x, row.y, row.w, row.h + size_slider_shown_height()};
}

auto Carousel::size_slider_rect(Rect t_panel) const -> Rect
{
	const Rect row = switcher_row_rect(t_panel, row_index_at_stop(m_zoom_stop));

	return Rect{row.x, row.bottom(), row.w, K_SIZE_SLIDER_HEIGHT};
}

auto Carousel::is_size_slider_open() const -> bool
{
	return m_size_slider_fold > 0.5f && has_sizes(row_at_stop(m_zoom_stop));
}

auto Carousel::is_switcher_shown() const -> bool
{
	return m_switcher_shown > 0.01f;
}

auto Carousel::is_mouse_over_switcher(Vec2 t_mouse) const -> bool
{
	const Rect indicator = status_indicator_rect();
	if (indicator.contains(t_mouse)) return true;
	if (!is_switcher_shown()) return false;

	const Rect  panel = switcher_panel_rect();
	const float left  = std::min(panel.x, indicator.x);
	const Rect  panel_and_indicator{left, panel.y, std::max(panel.right(), indicator.right()) - left, indicator.bottom() - panel.y};

	return panel_and_indicator.contains(t_mouse);
}

auto Carousel::set_zoom_stop(i32 t_stop) -> void
{
	t_stop = std::clamp(t_stop, 0, K_ZOOM_STOP_COUNT - 1);

	if (t_stop != m_zoom_stop) {
		m_zoom_stop = t_stop;

		const ViewMode mode = mode_at_stop(t_stop);
		if (mode != m_mode) {
			capture_mode_morph();
			m_previous_mode   = m_mode;
			m_mode            = mode;
			m_mode_transition = 1.0f;
			m_wrap_scroll     = Scrollable{};
		}

		m_commands->push(Command{.type = CommandType::SaveChanges});
	}

	m_switcher_hold_seconds = K_SWITCHER_HOLD_SECONDS;
}

auto Carousel::focused_game() const -> i32
{
	return m_mode == ViewMode::Carousel ? selected_game() : m_focused_game;
}

auto Carousel::is_focus_shown(u32 t_game) const -> bool
{
	return m_keyboard_focus_shown && m_mode != ViewMode::Carousel && static_cast<i32>(t_game) == focused_game();
}

auto Carousel::move_focus(i32 t_delta) -> void
{
	if (m_library->game_count == 0) return;

	m_keyboard_focus_shown = true;

	const i32 last = static_cast<i32>(m_library->game_count) - 1;
	const i32 slot = std::clamp(static_cast<i32>(m_slot_of[focused_game()]) + t_delta, 0, last);
	m_focused_game = m_order[slot];

	if (m_mode == ViewMode::Carousel) {
		m_target_scroll = static_cast<float>(slot);
		return;
	}

	const Rect card = slot_rect(m_mode, static_cast<u32>(slot));
	m_wrap_scroll.reveal(card.y, card.bottom(), m_bounds.y + K_GRID_PADDING, m_bounds.bottom() - K_GRID_PADDING, wrap_scroll_geometry());
}

auto Carousel::open_game(i32 t_game) -> void
{
	m_commands->push(Command{.type = CommandType::OpenGame, .index = t_game});
}

auto Carousel::drop_lost_press() -> void
{
	m_long_press.game = -1;

	if (m_reorder.active) {
		end_reorder(false);
	}

	if (m_card_drag.is_pressed()) {
		m_card_drag.end();
		m_target_scroll = clamp_scroll(std::round(m_scroll));
	}
}

auto Carousel::start_press(i32 t_game, Vec2 t_point) -> void
{
	m_long_press = LongPress{t_game, t_point, 0.0f};
	if (t_game < 0) return;

	m_reorder.game   = t_game;
	m_reorder.lifted = false;
}

auto Carousel::begin_reorder(u32 t_game, Vec2 t_point) -> void
{
	const Rect card = shown_card(m_mode, t_game);

	m_card_drag.end();
	m_wrap_scroll.on_pointer_up();
	m_long_press.game = -1;

	m_reorder.active = true;
	m_reorder.lifted = true;
	m_reorder.game   = static_cast<i32>(t_game);
	m_reorder.grab   = Vec2{std::clamp((t_point.x - card.x) / card.w, 0.0f, 1.0f), std::clamp((t_point.y - card.y) / card.h, 0.0f, 1.0f)};
	std::copy(std::begin(m_order), std::end(m_order), std::begin(m_reorder.start_order));

	m_card_offset[t_game]   = Vec2{};
	m_card_velocity[t_game] = Vec2{};
}

auto Carousel::end_reorder(bool t_cancel) -> void
{
	if (!m_reorder.active) return;

	const auto game = static_cast<u32>(m_reorder.game);

	Vec2 centers[K_MAX_GAMES];
	capture_centers(centers);
	m_reorder.active = false;

	if (t_cancel) {
		set_order({m_reorder.start_order, m_library->game_count});
	}

	if (m_mode == ViewMode::Carousel) {
		m_scroll        = static_cast<float>(m_slot_of[game]);
		m_target_scroll = m_scroll;
	}

	restore_centers(centers);
	m_card_velocity[game] = Vec2{};

	if (!std::equal(m_order, m_order + m_library->game_count, m_reorder.start_order)) {
		m_commands->push(Command{.type = CommandType::SaveChanges});
	}
}

auto Carousel::move_to_slot(u32 t_game, u32 t_slot) -> void
{
	Vec2 centers[K_MAX_GAMES];
	capture_centers(centers);

	u8 order[K_MAX_GAMES];
	std::copy(std::begin(m_order), std::end(m_order), std::begin(order));

	const u32 from = m_slot_of[t_game];
	if (from < t_slot) {
		std::rotate(order + from, order + from + 1, order + t_slot + 1);
	} else {
		std::rotate(order + t_slot, order + from, order + from + 1);
	}

	set_order({order, m_library->game_count});
	restore_centers(centers);
}

auto Carousel::capture_centers(Vec2 (&t_centers)[K_MAX_GAMES]) const -> void
{
	for (u32 game = 0; game < m_library->game_count; game += 1) {
		t_centers[game] = shown_card(m_mode, game).center();
	}
}

auto Carousel::restore_centers(const Vec2 (&t_centers)[K_MAX_GAMES]) -> void
{
	for (u32 game = 0; game < m_library->game_count; game += 1) {
		const Vec2 slot     = slot_rect(m_mode, m_slot_of[game]).center();
		m_card_offset[game] = Vec2{t_centers[game].x - slot.x, t_centers[game].y - slot.y};
	}
}

auto Carousel::retarget_reorder() -> void
{
	const auto game   = static_cast<u32>(m_reorder.game);
	const Vec2 center = dragged_rect().center();

	u32   best          = m_slot_of[game];
	float best_distance = -1.0f;

	for (u32 slot = 0; slot < m_library->game_count; slot += 1) {
		const Vec2  cell     = slot_rect(m_mode, slot).center();
		const float dx       = m_mode == ViewMode::List ? 0.0f : cell.x - center.x;
		const float dy       = m_mode == ViewMode::Carousel ? 0.0f : cell.y - center.y;
		const float distance = dx * dx + dy * dy;

		if (best_distance < 0.0f || distance < best_distance) {
			best          = slot;
			best_distance = distance;
		}
	}

	if (best != m_slot_of[game]) {
		move_to_slot(game, best);
	}
}

auto Carousel::update_reorder(float t_delta_seconds) -> void
{
	if (m_long_press.game >= 0 || m_reorder.active) {
		animation::request_frame();
	}

	if (m_long_press.game >= 0) {
		m_long_press.seconds += t_delta_seconds;

		const float charge = std::clamp((m_long_press.seconds - K_PRESS_DELAY_SECONDS) / (K_LONG_PRESS_SECONDS - K_PRESS_DELAY_SECONDS), 0.0f, 1.0f);
		m_lift             = K_PRESS_LIFT * charge * charge;

		if (m_long_press.seconds >= K_LONG_PRESS_SECONDS) {
			begin_reorder(static_cast<u32>(m_long_press.game), m_mouse);
		}
	}

	const bool active   = m_reorder.active;
	const bool pressing = m_long_press.game >= 0;
	const bool overview = active && m_mode == ViewMode::Carousel;

	m_overview = animation::ease_toward(m_overview, overview ? 1.0f : 0.0f, K_OVERVIEW_EASE_RATE, t_delta_seconds,
	                                    animation::K_SETTLED_PIXELS / std::max(m_bounds.w, 1.0f));
	if (!pressing) {
		m_lift = animation::ease_toward(m_lift, active ? 1.0f : 0.0f, K_LIFT_EASE_RATE, t_delta_seconds);
	}

	m_reorder_hint = animation::ease_toward(m_reorder_hint, active ? 1.0f : 0.0f, K_HINT_EASE_RATE, t_delta_seconds);

	if (active && m_mode != ViewMode::Carousel) {
		const float above = m_bounds.y + K_REORDER_SCROLL_ZONE - m_mouse.y;
		const float below = m_mouse.y - (m_bounds.bottom() - K_REORDER_SCROLL_ZONE);

		if (above > 0.0f) {
			m_wrap_scroll.scroll_by(-K_REORDER_SCROLL_SPEED * std::min(above / K_REORDER_SCROLL_ZONE, 1.0f) * t_delta_seconds, wrap_scroll_geometry());
		} else if (below > 0.0f) {
			m_wrap_scroll.scroll_by(K_REORDER_SCROLL_SPEED * std::min(below / K_REORDER_SCROLL_ZONE, 1.0f) * t_delta_seconds, wrap_scroll_geometry());
		}
	}

	if (active && (m_mode != ViewMode::Carousel || m_overview > K_OVERVIEW_RETARGET_THRESHOLD)) {
		retarget_reorder();
	}

	bool settled = true;

	for (u32 game = 0; game < m_library->game_count; game += 1) {
		Vec2* offset   = &m_card_offset[game];
		Vec2* velocity = &m_card_velocity[game];

		offset->x =
			animation::spring_toward(offset->x, &velocity->x, 0.0f, K_REORDER_STIFFNESS, K_REORDER_DAMPING_RATIO, t_delta_seconds, animation::K_SETTLED_PIXELS);
		offset->y =
			animation::spring_toward(offset->y, &velocity->y, 0.0f, K_REORDER_STIFFNESS, K_REORDER_DAMPING_RATIO, t_delta_seconds, animation::K_SETTLED_PIXELS);
		settled = settled && offset->x == 0.0f && offset->y == 0.0f;
	}

	if (!active && !pressing && m_reorder.game >= 0 && settled && m_lift == 0.0f) {
		m_reorder.game   = -1;
		m_reorder.lifted = false;
	}
}

auto Carousel::capture_mode_morph() -> void
{
	for (u32 game = 0; game < m_library->game_count; game += 1) {
		m_morph_from_art[game]   = morph_art(game);
		m_morph_from_frame[game] = shown_card(m_mode, game);
	}
}

auto Carousel::smooth_grid_reflow() -> void
{
	const u32 columns = wrap_columns();

	if (columns != 0 && m_grid_columns != 0 && columns != m_grid_columns) {
		for (u32 game = 0; game < m_library->game_count; game += 1) {
			if (m_reorder.active && static_cast<i32>(game) == m_reorder.game) continue;

			const Vec2 slot     = slot_rect(m_mode, m_slot_of[game]).center();
			m_card_offset[game] = Vec2{m_last_centers[game].x - slot.x, m_last_centers[game].y - slot.y};
		}
	}

	m_grid_columns = columns;
}

auto Carousel::switcher_pointer_down(Vec2 t_point) -> bool
{
	const Rect panel = switcher_panel_rect();
	if (!is_switcher_shown() || !panel.contains(t_point)) return false;

	m_switcher_owns_pointer = true;

	const u32          active = row_index_at_stop(m_zoom_stop);
	const SwitcherRow& entry  = K_SWITCHER_ROWS[active];

	if (is_size_slider_open()) {
		const Rect slider = size_slider_rect(panel);

		if (size_slider_icon_rect(slider, false).inset(-K_SIZE_SLIDER_ICON_REACH).contains(t_point)) {
			set_zoom_stop(std::max(entry.first_stop, m_zoom_stop - 1));
			return true;
		}

		if (size_slider_icon_rect(slider, true).inset(-K_SIZE_SLIDER_ICON_REACH).contains(t_point)) {
			set_zoom_stop(std::min(entry.last_stop, m_zoom_stop + 1));
			return true;
		}

		if (size_slider_grab_rect(slider).contains(t_point)) {
			m_switcher_drag.begin(t_point);
			set_zoom_stop(stop_at_track_x(entry, size_slider_track_rect(slider), t_point.x));
			return true;
		}
	}

	for (u32 row = 0; row < K_SWITCHER_ROW_COUNT; row += 1) {
		if (row != active && switcher_row_rect(panel, row).contains(t_point)) {
			set_zoom_stop(K_SWITCHER_ROWS[row].first_stop);
			return true;
		}
	}

	return true;
}

auto Carousel::switcher_pointer_move(Vec2 t_point) -> bool
{
	if (!m_switcher_drag.is_pressed()) return false;

	m_switcher_drag.update(t_point);

	const Rect slider = size_slider_rect(switcher_panel_rect());
	set_zoom_stop(stop_at_track_x(row_at_stop(m_zoom_stop), size_slider_track_rect(slider), t_point.x));

	return true;
}

auto Carousel::switcher_pointer_up() -> bool
{
	m_switcher_drag.end();

	return std::exchange(m_switcher_owns_pointer, false);
}

auto Carousel::on_pointer_down(Vec2 t_point) -> bool
{
	m_keyboard_focus_shown = false;

	if (switcher_pointer_down(t_point)) return true;
	if (m_reorder.active) return true;

	const i32 pressed = m_mode_transition <= 0.001f ? game_at(t_point) : -1;

	if (pressed >= 0 && is_key_down(VK_CONTROL)) {
		begin_reorder(static_cast<u32>(pressed), t_point);
		return true;
	}

	if (m_mode != ViewMode::Carousel) {
		if (m_wrap_scroll.on_pointer_down(t_point, wrap_scroll_geometry())) return true;

		start_press(pressed, t_point);
		return pressed >= 0;
	}

	start_press(pressed, t_point);

	if (m_zoom_stop != K_SPREAD_STOP) {
		m_card_drag.begin(t_point);
		m_drag_start_scroll = m_scroll;
	}

	return true;
}

auto Carousel::on_pointer_move(Vec2 t_point) -> bool
{
	m_keyboard_focus_shown = false;

	if (switcher_pointer_move(t_point)) return true;
	if (m_reorder.active) return true;

	if (m_long_press.game >= 0) {
		const float dx = t_point.x - m_long_press.origin.x;
		const float dy = t_point.y - m_long_press.origin.y;

		if (dx * dx + dy * dy > K_LONG_PRESS_SLOP * K_LONG_PRESS_SLOP) {
			m_long_press.game = -1;
		}
	}

	if (m_mode != ViewMode::Carousel) {
		m_wrap_scroll.on_pointer_move(t_point.y, wrap_scroll_geometry());
		return m_wrap_scroll.is_dragging();
	}

	if (!m_card_drag.is_pressed()) return false;

	m_card_drag.update(t_point);
	m_scroll        = m_drag_start_scroll - m_card_drag.delta_x() / (K_DRAG_PIXELS_PER_CARD * view_scale());
	m_target_scroll = m_scroll;

	return true;
}

auto Carousel::on_pointer_up(Vec2 t_point) -> bool
{
	m_keyboard_focus_shown = false;
	m_long_press.game      = -1;

	if (switcher_pointer_up()) return true;
	if (std::exchange(m_release_ignored, false)) return true;

	if (m_reorder.active) {
		end_reorder(false);
		return true;
	}

	if (m_mode != ViewMode::Carousel) {
		if (m_wrap_scroll.is_dragging()) {
			m_wrap_scroll.on_pointer_up();
			return true;
		}

		if (const i32 game = game_at(t_point); game >= 0) {
			open_game(game);
		}

		return true;
	}

	if (m_zoom_stop == K_SPREAD_STOP) {
		if (const i32 game = game_at(t_point); game >= 0) {
			m_target_scroll = static_cast<float>(m_slot_of[game]);
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

auto Carousel::on_scroll(Vec2, float t_wheel_delta) -> bool
{
	m_keyboard_focus_shown = false;
	if (m_reorder.active) return true;

	if (is_key_down(VK_CONTROL)) {
		if (t_wheel_delta != 0.0f) {
			set_zoom_stop(m_zoom_stop + (t_wheel_delta > 0.0f ? 1 : -1));
		}
	} else if (m_mode == ViewMode::Carousel) {
		m_target_scroll = clamp_scroll(m_target_scroll + t_wheel_delta);
	} else {
		m_wrap_scroll.on_scroll(t_wheel_delta, wrap_scroll_geometry());
	}

	return true;
}

auto Carousel::on_key_down(u32 t_key) -> bool
{
	if (m_reorder.active) {
		if (t_key == VK_ESCAPE) {
			end_reorder(true);
			m_release_ignored = true;
		}

		return true;
	}

	if (m_library->game_count == 0) return false;

	const bool horizontal = m_mode != ViewMode::List;
	const bool vertical   = m_mode != ViewMode::Carousel;
	const i32  row_step   = std::max(1, static_cast<i32>(wrap_columns()));

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
			if (m_mode != ViewMode::Carousel && !m_keyboard_focus_shown) {
				move_focus(0);
				return true;
			}

			open_game(focused_game());
			return true;

		default:
			return false;
	}
}

auto Carousel::cursor() const -> CursorKind
{
	if (m_reorder.active) return CursorKind::Move;

	const bool dragging_cards = m_mode == ViewMode::Carousel && m_card_drag.is_pressed();
	if (m_switcher_drag.is_pressed() || dragging_cards || m_wrap_scroll.is_dragging()) return CursorKind::Drag;

	if (m_library->game_count > 0 && status_indicator_rect().contains(m_mouse)) return CursorKind::Hand;

	if (is_switcher_shown()) {
		const Rect panel  = switcher_panel_rect();
		const u32  active = row_index_at_stop(m_zoom_stop);

		for (u32 row = 0; row < K_SWITCHER_ROW_COUNT; row += 1) {
			if (row != active && switcher_row_rect(panel, row).contains(m_mouse)) return CursorKind::Hand;
		}

		if (is_size_slider_open()) {
			const Rect slider = size_slider_rect(panel);

			if (size_slider_grab_rect(slider).contains(m_mouse) || size_slider_icon_rect(slider, false).inset(-K_SIZE_SLIDER_ICON_REACH).contains(m_mouse) ||
			    size_slider_icon_rect(slider, true).inset(-K_SIZE_SLIDER_ICON_REACH).contains(m_mouse)) {
				return CursorKind::Hand;
			}
		}
	}

	if (const i32 game = game_at(m_mouse); game >= 0) {
		return is_key_down(VK_CONTROL) ? CursorKind::Move : CursorKind::Hand;
	}

	const bool over_scrollbar = m_mode != ViewMode::Carousel && m_wrap_scroll.is_over_track(m_mouse, wrap_scroll_geometry());

	return over_scrollbar ? CursorKind::Hand : CursorKind::Arrow;
}

auto Carousel::update(float t_delta_seconds) -> void
{
	if (!is_key_down(VK_LBUTTON) || m_detached_game >= 0) {
		drop_lost_press();
	}

	m_scroll          = animation::ease_toward(m_scroll, m_target_scroll, K_SCROLL_EASE_RATE, t_delta_seconds,
	                                           animation::K_SETTLED_PIXELS / (K_DRAG_PIXELS_PER_CARD * view_scale()));
	m_mode_transition = animation::step_toward(m_mode_transition, 0.0f, K_MODE_MORPH_SECONDS, t_delta_seconds);
	m_zoom_percent    = animation::ease_toward(m_zoom_percent, stop_percent(m_zoom_stop), K_ZOOM_EASE_RATE, t_delta_seconds);
	m_shelf           = animation::ease_toward(m_shelf, is_shelf_stop(m_zoom_stop) ? 1.0f : 0.0f, K_SHELF_EASE_RATE, t_delta_seconds);
	m_spread          = animation::ease_toward(m_spread, m_zoom_stop == K_SPREAD_STOP ? 1.0f : 0.0f, K_OVERVIEW_EASE_RATE, t_delta_seconds,
	                                           animation::K_SETTLED_PIXELS / std::max(m_bounds.w, 1.0f));
	m_wrap_scroll.update(t_delta_seconds);
	smooth_grid_reflow();

	const i32 hovered_card = m_mode != ViewMode::List && !m_reorder.active ? game_at(m_mouse) : -1;

	for (u32 game = 0; game < m_library->game_count; game += 1) {
		const bool raised  = static_cast<i32>(game) == hovered_card || is_focus_shown(game);
		m_card_hover[game] = animation::ease_toward(m_card_hover[game], raised ? 1.0f : 0.0f, K_HOVER_EASE_RATE, t_delta_seconds);
	}

	if (is_mouse_over_switcher(m_mouse)) {
		m_switcher_hold_seconds = K_SWITCHER_HOLD_SECONDS;
	} else {
		m_switcher_hold_seconds = std::max(0.0f, m_switcher_hold_seconds - t_delta_seconds);
	}

	if (m_switcher_hold_seconds > 0.0f) {
		animation::request_frame_after(m_switcher_hold_seconds);
	}

	const float switcher_target = m_switcher_hold_seconds > 0.0f ? 1.0f : 0.0f;
	m_switcher_shown            = animation::ease_toward(m_switcher_shown, switcher_target, K_SWITCHER_EASE_RATE, t_delta_seconds);

	const bool fold_open = is_switcher_shown() && has_sizes(row_at_stop(m_zoom_stop)) &&
	                       (m_switcher_drag.is_pressed() || switcher_active_pill(switcher_panel_rect()).contains(m_mouse));
	m_size_slider_fold   = animation::ease_toward(m_size_slider_fold, fold_open ? 1.0f : 0.0f, K_SIZE_SLIDER_FOLD_RATE, t_delta_seconds,
	                                              animation::K_SETTLED_PIXELS / K_SIZE_SLIDER_HEIGHT);

	update_reorder(t_delta_seconds);

	for (u32 game = 0; game < m_library->game_count; game += 1) {
		m_last_centers[game] = shown_card(m_mode, game).center();
	}
}

auto Carousel::draw_card(DrawList* t_draw_list, Rect t_rect, const Game& t_game, bool t_highlighted, bool t_centered, u8 t_alpha) const -> void
{
	draw_framed_art(t_draw_list, t_rect, t_game, card_look(t_highlighted, t_centered), K_CARD_CORNER_RADIUS, 0.0f, t_alpha);
}

auto Carousel::draw_carousel_mode(DrawList* t_draw_list) const -> void
{
	for (u32 game = 0; game < m_library->game_count; game += 1) {
		if (static_cast<i32>(game) == m_detached_game || is_raised(ViewMode::Carousel, game)) continue;

		const Rect card = shown_card(ViewMode::Carousel, game);
		if (card.right() < m_bounds.x || card.x > m_bounds.right()) continue;

		const CardState state = card_state(ViewMode::Carousel, game, card);
		draw_card(t_draw_list, grown(card, game), m_library->games[game], state.highlighted, state.centered, 255);
	}

	if (!m_reorder.lifted) {
		draw_raised(t_draw_list, ViewMode::Carousel);
	}

	draw_carousel_edges(t_draw_list, 255);
}

auto Carousel::draw_carousel_edges(DrawList* t_draw_list, u8 t_alpha) const -> void
{
	if (m_library->game_count == 0) return;

	const Rect  first          = carousel_slot(-m_scroll);
	const Rect  last           = carousel_slot(static_cast<float>(m_library->game_count - 1) - m_scroll);
	const float fade           = t_alpha * (1.0f - std::max(m_overview, m_spread));
	const float left_overflow  = std::clamp((m_bounds.x - first.x) / K_EDGE_FADE_WIDTH, 0.0f, 1.0f);
	const float right_overflow = std::clamp((last.right() - m_bounds.right()) / K_EDGE_FADE_WIDTH, 0.0f, 1.0f);
	const float left           = 1.0f + (left_overflow - 1.0f) * m_shelf;
	const float right          = 1.0f + (right_overflow - 1.0f) * m_shelf;
	const Color clear          = faded(g_theme.window, 0);

	if (left > 0.0f) {
		const Color opaque = faded(g_theme.window, static_cast<u8>(fade * left));
		fade_cards(t_draw_list, Rect{m_bounds.x, m_bounds.y, K_EDGE_FADE_WIDTH, m_bounds.h}, opaque, clear, opaque, clear);
	}

	if (right > 0.0f) {
		const Color opaque = faded(g_theme.window, static_cast<u8>(fade * right));
		fade_cards(t_draw_list, Rect{m_bounds.right() - K_EDGE_FADE_WIDTH, m_bounds.y, K_EDGE_FADE_WIDTH, m_bounds.h}, clear, opaque, clear, opaque);
	}
}

auto Carousel::faded_rect(u32 t_game) const -> Rect
{
	if (m_mode_transition > 0.0f) return morph_art(t_game);

	const Rect shown = shown_card(m_mode, t_game);

	return uses_frame(m_mode) ? shown : grown(shown, t_game);
}

auto Carousel::fade_cards(DrawList* t_draw_list, Rect t_band, Color t_top_left, Color t_top_right, Color t_bottom_left, Color t_bottom_right) const -> void
{
	if (t_band.w <= 0.0f || t_band.h <= 0.0f) return;

	for (u32 game = 0; game < m_library->game_count; game += 1) {
		const Rect card    = faded_rect(game);
		const bool touches = card.right() > t_band.x && card.x < t_band.right() && card.bottom() > t_band.y && card.y < t_band.bottom();
		if (!touches) continue;

		t_draw_list->push_clip(card);
		t_draw_list->add_plain_backdrop(t_band, t_top_left, t_top_right, t_bottom_left, t_bottom_right);
		t_draw_list->pop_clip();
	}
}

auto Carousel::draw_wrap_scroll(DrawList* t_draw_list, u8 t_alpha) const -> void
{
	const ScrollGeometry        geometry = wrap_scroll_geometry();
	const Scrollable::EdgeFades fades    = m_wrap_scroll.edge_fades(m_bounds, geometry);
	const Color                 edge     = faded(g_theme.window, t_alpha);
	const Color                 clear    = faded(g_theme.window, 0);

	fade_cards(t_draw_list, fades.top, edge, edge, clear, clear);
	fade_cards(t_draw_list, fades.bottom, clear, clear, edge, edge);
	m_wrap_scroll.draw(t_draw_list, geometry, m_mouse, t_alpha);
}

auto Carousel::draw_grid_mode(DrawList* t_draw_list) const -> void
{
	t_draw_list->push_clip(m_bounds);

	for (u32 game = 0; game < m_library->game_count; game += 1) {
		if (static_cast<i32>(game) == m_detached_game || is_raised(ViewMode::Grid, game)) continue;

		const Rect card = shown_card(ViewMode::Grid, game);
		if (!card.overlaps_vertically(m_bounds)) continue;

		const CardState state = card_state(ViewMode::Grid, game, card);
		draw_card(t_draw_list, grown(card, game), m_library->games[game], state.highlighted, false, 255);
	}

	if (!m_reorder.lifted) {
		draw_raised(t_draw_list, ViewMode::Grid);
	}

	t_draw_list->pop_clip();

	draw_wrap_scroll(t_draw_list, 255);
}

auto Carousel::draw_list_row_frame(DrawList* t_draw_list, Rect t_row, u32 t_game, bool t_highlighted, u8 t_alpha) const -> void
{
	const Font& font  = m_fonts->body;
	const Rect  thumb = list_thumb(t_row);

	t_draw_list->add_rounded_rect(t_row, rounded(K_LIST_CORNER_RADIUS), faded(t_highlighted ? g_theme.control : g_theme.popup, t_alpha));
	draw_text_truncated(t_draw_list, font, Vec2{thumb.right() + 16.0f, font.centered_baseline(t_row)}, m_library->games[t_game].title,
	                    t_row.right() - 16.0f - (thumb.right() + 16.0f), faded(g_theme.text, t_alpha));
}

auto Carousel::draw_list_row(DrawList* t_draw_list, Rect t_row, u32 t_game, bool t_highlighted, u8 t_alpha) const -> void
{
	draw_list_row_frame(t_draw_list, t_row, t_game, t_highlighted, t_alpha);

	if (static_cast<i32>(t_game) == m_detached_game) return;

	draw_framed_art(t_draw_list, list_thumb(t_row), m_library->games[t_game], CardLook{}, K_LIST_CORNER_RADIUS, 1.0f, t_alpha);
}

auto Carousel::draw_icon_tile_frame(DrawList* t_draw_list, Rect t_tile, u32 t_game, bool t_highlighted, u8 t_alpha) const -> void
{
	const Font& title_font  = m_fonts->body;
	const Game& game        = m_library->games[t_game];
	const Rect  art         = icon_tile_art(t_tile);
	const float label_width = t_tile.w - K_ICON_LABEL_INSET * 2.0f;

	if (t_highlighted) {
		t_draw_list->add_rounded_rect(t_tile, rounded(K_ICON_TILE_RADIUS), faded(g_theme.row_hover, t_alpha));
	}

	if (is_focus_shown(t_game)) {
		t_draw_list->add_bordered_rect(t_tile, rounded(K_ICON_TILE_RADIUS), Color{}, faded(m_settings->accent, t_alpha), 1.0f);
	}

	const float title_baseline = art.bottom() + K_ICON_LABEL_GAP + title_font.ascent;
	draw_centered_label(t_draw_list, title_font, t_tile.center().x, title_baseline, game.short_title, label_width, faded(g_theme.text, t_alpha));
}

auto Carousel::draw_icon_tile(DrawList* t_draw_list, Rect t_tile, u32 t_game, bool t_highlighted, u8 t_alpha) const -> void
{
	draw_icon_tile_frame(t_draw_list, t_tile, t_game, t_highlighted, t_alpha);

	if (static_cast<i32>(t_game) == m_detached_game) return;

	const Rect art = grown(icon_tile_art(t_tile), t_game);
	draw_framed_art(t_draw_list, art, m_library->games[t_game], CardLook{}, art_radius(ViewMode::Icons, art), 1.0f, t_alpha);
}

auto Carousel::draw_frame(DrawList* t_draw_list, ViewMode t_mode, Rect t_frame, u32 t_game, bool t_highlighted, u8 t_alpha) const -> void
{
	if (t_mode == ViewMode::Icons) {
		draw_icon_tile_frame(t_draw_list, t_frame, t_game, t_highlighted, t_alpha);
	} else {
		draw_list_row_frame(t_draw_list, t_frame, t_game, t_highlighted, t_alpha);
	}
}

auto Carousel::draw_icons_mode(DrawList* t_draw_list) const -> void
{
	t_draw_list->push_clip(m_bounds);

	for (u32 game = 0; game < m_library->game_count; game += 1) {
		if (is_raised(ViewMode::Icons, game)) continue;

		const Rect tile = shown_card(ViewMode::Icons, game);
		if (!tile.overlaps_vertically(m_bounds)) continue;

		draw_icon_tile(t_draw_list, tile, game, card_state(ViewMode::Icons, game, tile).highlighted, 255);
	}

	if (!m_reorder.lifted) {
		draw_raised(t_draw_list, ViewMode::Icons);
	}

	t_draw_list->pop_clip();

	draw_wrap_scroll(t_draw_list, 255);
}

auto Carousel::draw_list_mode(DrawList* t_draw_list) const -> void
{
	t_draw_list->push_clip(m_bounds);

	for (u32 game = 0; game < m_library->game_count; game += 1) {
		if (is_raised(ViewMode::List, game)) continue;

		const Rect row = shown_card(ViewMode::List, game);
		if (!row.overlaps_vertically(m_bounds)) continue;

		draw_list_row(t_draw_list, row, game, card_state(ViewMode::List, game, row).highlighted, 255);
	}

	if (!m_reorder.lifted) {
		draw_raised(t_draw_list, ViewMode::List);
	}

	t_draw_list->pop_clip();

	draw_wrap_scroll(t_draw_list, 255);
}

auto Carousel::draw_raised(DrawList* t_draw_list, ViewMode t_mode) const -> void
{
	if (m_reorder.game < 0 || t_mode != m_mode) return;

	const auto  game   = static_cast<u32>(m_reorder.game);
	const bool  framed = uses_frame(t_mode);
	const float radius = t_mode == ViewMode::List ? K_LIST_CORNER_RADIUS : t_mode == ViewMode::Icons ? K_ICON_TILE_RADIUS : K_CARD_CORNER_RADIUS;

	if (!framed && m_reorder.game == m_detached_game) return;

	if (m_reorder.active) {
		t_draw_list->add_rounded_rect(slot_rect(t_mode, m_slot_of[game]), rounded(radius), with_alpha(m_settings->accent, K_PLACEHOLDER_ALPHA));
	}

	Rect            card  = shown_card(t_mode, game);
	const CardState state = card_state(t_mode, game, card);

	if (!uses_frame(t_mode)) {
		card = grown(card, game);
	}

	const Rect lifted = card.scaled_from_center(lift_scale(game));

	controls::draw_panel_shadow(t_draw_list, lifted, radius, m_lift);

	if (t_mode == ViewMode::List) {
		draw_list_row(t_draw_list, lifted, game, state.highlighted, 255);
	} else if (t_mode == ViewMode::Icons) {
		draw_icon_tile(t_draw_list, lifted, game, state.highlighted, 255);
	} else {
		draw_card(t_draw_list, lifted, m_library->games[game], state.highlighted, state.centered, 255);
	}
}

auto Carousel::draw_reorder_hint(DrawList* t_draw_list) const -> void
{
	if (m_reorder_hint <= 0.01f) return;

	constexpr std::string_view PLACE  = "Release to place";
	constexpr std::string_view CANCEL = "Esc to cancel";

	const Font& font   = m_fonts->secondary;
	const float width  = text_width(font, PLACE) + K_HINT_DOT_GAP * 2.0f + K_HINT_DOT_SIZE + text_width(font, CANCEL) + K_HINT_PADDING_X * 2.0f;
	const float height = font.line_height() + K_HINT_PADDING_Y * 2.0f;
	const float rise   = (1.0f - m_reorder_hint) * K_HINT_RISE;
	const Rect  pill{snapped_to_pixel(m_bounds.center().x - width * 0.5f), snapped_to_pixel(m_bounds.bottom() - K_HINT_MARGIN - height + rise), width, height};
	const auto  alpha    = to_alpha(m_reorder_hint);
	const float baseline = font.centered_baseline(pill);

	controls::draw_popup_shadow(t_draw_list, pill, height * 0.5f, m_reorder_hint);
	t_draw_list->add_bordered_rect(pill, rounded(height * 0.5f), faded(g_theme.popup, alpha), faded(g_theme.border, alpha), 1.0f);

	float x = pill.x + K_HINT_PADDING_X;
	draw_text(t_draw_list, font, Vec2{x, baseline}, PLACE, faded(g_theme.text, alpha));
	x += text_width(font, PLACE) + K_HINT_DOT_GAP;

	t_draw_list->add_rounded_rect(Rect{x, pill.center().y - K_HINT_DOT_SIZE * 0.5f, K_HINT_DOT_SIZE, K_HINT_DOT_SIZE}, rounded(K_HINT_DOT_SIZE * 0.5f),
	                              faded(g_theme.text_faint, alpha));
	x += K_HINT_DOT_SIZE + K_HINT_DOT_GAP;

	draw_text(t_draw_list, font, Vec2{x, baseline}, CANCEL, faded(g_theme.text_dim, alpha));
}

auto Carousel::draw_mode(DrawList* t_draw_list, ViewMode t_mode) const -> void
{
	switch (t_mode) {
		case ViewMode::Carousel:
			draw_carousel_mode(t_draw_list);
			break;
		case ViewMode::Grid:
			draw_grid_mode(t_draw_list);
			break;
		case ViewMode::List:
			draw_list_mode(t_draw_list);
			break;
		case ViewMode::Icons:
			draw_icons_mode(t_draw_list);
			break;
	}
}

auto Carousel::draw_mode_morph(DrawList* t_draw_list) const -> void
{
	const float amount        = mode_morph();
	const auto  incoming      = to_alpha(amount);
	const auto  outgoing      = to_alpha(1.0f - amount);
	const bool  from_framed   = uses_frame(m_previous_mode);
	const bool  to_framed     = uses_frame(m_mode);
	const float cardness_from = from_framed ? 0.0f : 1.0f;
	const float cardness      = cardness_from + ((to_framed ? 0.0f : 1.0f) - cardness_from) * amount;

	const auto look_in = [this](ViewMode t_mode, u32 t_game, Rect t_card) {
		if (uses_frame(t_mode)) return CardLook{};

		const CardState state = card_state(t_mode, t_game, t_card);
		return card_look(state.highlighted, state.centered);
	};

	t_draw_list->push_clip(m_bounds);

	for (u32 game = 0; game < m_library->game_count; game += 1) {
		if (from_framed) {
			draw_frame(t_draw_list, m_previous_mode, m_morph_from_frame[game], game, false, outgoing);
		}

		if (to_framed) {
			const Rect frame = shown_card(m_mode, game);
			draw_frame(t_draw_list, m_mode, frame, game, card_state(m_mode, game, frame).highlighted, incoming);
		}
	}

	for (u32 game = 0; game < m_library->game_count; game += 1) {
		if (static_cast<i32>(game) == m_detached_game) continue;

		const Rect     from = m_morph_from_art[game];
		const Rect     to   = art_rect(m_mode, game);
		const CardLook look = blended_look(look_in(m_previous_mode, game, from), look_in(m_mode, game, shown_card(m_mode, game)), amount);

		const float radius = art_radius(m_previous_mode, from) + (art_radius(m_mode, to) - art_radius(m_previous_mode, from)) * amount;

		draw_framed_art(t_draw_list, lerp(from, to, amount), m_library->games[game], look, radius, 1.0f - cardness, 255);
	}

	t_draw_list->pop_clip();

	if (m_previous_mode == ViewMode::Carousel) {
		draw_carousel_edges(t_draw_list, outgoing);
	}

	if (m_mode == ViewMode::Carousel) {
		draw_carousel_edges(t_draw_list, incoming);
	} else {
		draw_wrap_scroll(t_draw_list, incoming);
	}
}

auto Carousel::draw_status_bar(DrawList* t_draw_list) const -> void
{
	if (m_library->game_count == 0) return;

	const Font& font      = m_fonts->secondary;
	const Rect  indicator = status_indicator_rect();
	const Rect  icon{indicator.x, indicator.y + (indicator.h - K_STATUS_ICON_SIZE) * 0.5f, K_STATUS_ICON_SIZE, K_STATUS_ICON_SIZE};

	const SwitcherRow& current = row_at_stop(m_zoom_stop);

	t_draw_list->add_image(icon, m_assets->get(current.icon), g_theme.text_dim);
	draw_text(t_draw_list, font, Vec2{icon.right() + K_STATUS_ICON_GAP, font.centered_baseline(indicator) - K_BASELINE_NUDGE}, current.name, g_theme.text_dim);
}

auto Carousel::draw_switcher_rows(DrawList* t_draw_list, Rect t_panel, u8 t_alpha) const -> void
{
	const Font& font         = m_fonts->body;
	const u32   active       = row_index_at_stop(m_zoom_stop);
	const Color active_fill  = hovered(g_theme.popup);
	const bool  pointer_live = !m_switcher_drag.is_pressed();

	t_draw_list->add_rounded_rect(switcher_active_pill(t_panel), rounded(K_SWITCHER_ROW_RADIUS), faded(active_fill, t_alpha));

	for (u32 index = 0; index < K_SWITCHER_ROW_COUNT; index += 1) {
		const SwitcherRow& entry      = K_SWITCHER_ROWS[index];
		const Rect         row        = switcher_row_rect(t_panel, index);
		const bool         is_active  = index == active;
		const bool         is_hovered = !is_active && pointer_live && row.contains(m_mouse);

		if (is_hovered) {
			t_draw_list->add_rounded_rect(row, rounded(K_SWITCHER_ROW_RADIUS), faded(mix(g_theme.popup, active_fill, K_SWITCHER_HOVER_FILL), t_alpha));
		}

		Color content = g_theme.text_dim;
		if (is_active) {
			content = g_theme.text;
		} else if (is_hovered) {
			content = mix(g_theme.text_dim, g_theme.text, K_SWITCHER_HOVER_BRIGHTENING);
		}

		const float icon_center_y = row.center().y + font.ascent * 0.15f;
		const Rect  icon{row.x + K_SWITCHER_CONTENT_INSET, icon_center_y - K_SWITCHER_ICON_SIZE * 0.5f, K_SWITCHER_ICON_SIZE, K_SWITCHER_ICON_SIZE};

		t_draw_list->add_image(icon, m_assets->get(entry.icon), faded(content, t_alpha));
		draw_text(t_draw_list, font, Vec2{icon.right() + K_SWITCHER_ICON_GAP, font.centered_baseline(row)}, entry.name, faded(content, t_alpha));

		if (!is_active || !has_sizes(entry) || m_size_slider_fold >= 0.999f) continue;

		const auto dots_alpha = static_cast<u8>(static_cast<float>(t_alpha) * (1.0f - m_size_slider_fold));
		const i32  sizes      = entry.last_stop - entry.first_stop + 1;
		float      x = row.right() - K_SWITCHER_CONTENT_INSET - (static_cast<float>(sizes) * K_SIZE_DOT_SIZE + static_cast<float>(sizes - 1) * K_SIZE_DOT_GAP);

		for (i32 size = 0; size < sizes; size += 1) {
			const Rect  dot{snapped_to_pixel(x), snapped_to_pixel(icon_center_y - K_SIZE_DOT_SIZE * 0.5f), K_SIZE_DOT_SIZE, K_SIZE_DOT_SIZE};
			const Color fill = entry.first_stop + size == m_zoom_stop ? m_settings->accent : mix(g_theme.text_faint, active_fill, K_SIZE_DOT_DIMMING);

			t_draw_list->add_rounded_rect(dot, rounded(K_SIZE_DOT_SIZE * 0.5f), faded(fill, dots_alpha));
			x += K_SIZE_DOT_SIZE + K_SIZE_DOT_GAP;
		}
	}
}

auto Carousel::draw_size_slider(DrawList* t_draw_list, Rect t_panel, u8 t_alpha) const -> void
{
	const SwitcherRow& entry = row_at_stop(m_zoom_stop);
	if (m_size_slider_fold <= 0.001f || !has_sizes(entry)) return;

	const Color backdrop     = hovered(g_theme.popup);
	const auto  alpha        = static_cast<u8>(static_cast<float>(t_alpha) * m_size_slider_fold);
	const bool  pointer_live = !m_switcher_drag.is_pressed();

	Rect       slider = size_slider_rect(t_panel);
	const Rect shown{slider.x, slider.y, slider.w, size_slider_shown_height()};
	slider.y -= snapped_to_pixel((1.0f - m_size_slider_fold) * K_SIZE_SLIDER_RISE);

	const Rect track = size_slider_track_rect(slider);
	const i32  sizes = entry.last_stop - entry.first_stop + 1;

	t_draw_list->push_clip(shown);

	for (const bool large : {false, true}) {
		const Rect icon         = size_slider_icon_rect(slider, large);
		const bool icon_hovered = pointer_live && icon.inset(-K_SIZE_SLIDER_ICON_REACH).contains(m_mouse);

		t_draw_list->add_image(icon, m_assets->get(Asset::IconImage), faded(icon_hovered ? g_theme.text : g_theme.text_faint, alpha));
	}

	t_draw_list->add_rounded_rect(track, rounded(track.h * 0.5f), faded(g_theme.track, alpha));

	for (i32 size = 0; size < sizes; size += 1) {
		const float x = track.x + track.w * static_cast<float>(size) / static_cast<float>(sizes - 1);
		const Rect  tick{snapped_to_pixel(x - K_SIZE_SLIDER_TICK_SIZE * 0.5f), track.center().y - K_SIZE_SLIDER_TICK_SIZE * 0.5f, K_SIZE_SLIDER_TICK_SIZE,
		                 K_SIZE_SLIDER_TICK_SIZE};

		t_draw_list->add_rounded_rect(tick, rounded(K_SIZE_SLIDER_TICK_SIZE * 0.5f), faded(g_theme.text_faint, alpha));
	}

	const float thumb_x = track.x + track.w * zoom_within(m_zoom_percent, entry.first_stop, entry.last_stop);
	const Rect  thumb{thumb_x - K_SIZE_SLIDER_THUMB_SIZE * 0.5f, track.center().y - K_SIZE_SLIDER_THUMB_SIZE * 0.5f, K_SIZE_SLIDER_THUMB_SIZE,
	                  K_SIZE_SLIDER_THUMB_SIZE};

	t_draw_list->add_rounded_rect(thumb.inset(-K_SIZE_SLIDER_THUMB_RING), rounded(K_SIZE_SLIDER_THUMB_SIZE * 0.5f + K_SIZE_SLIDER_THUMB_RING),
	                              faded(backdrop, alpha));
	t_draw_list->add_rounded_rect(thumb, rounded(K_SIZE_SLIDER_THUMB_SIZE * 0.5f), faded(m_settings->accent, alpha));

	t_draw_list->pop_clip();
}

auto Carousel::draw_switcher(DrawList* t_draw_list) const -> void
{
	if (m_switcher_shown <= 0.001f) return;

	const auto alpha = to_alpha(m_switcher_shown);
	Rect       panel = switcher_panel_rect();
	panel.y += snapped_to_pixel((1.0f - m_switcher_shown) * K_SWITCHER_SLIDE_DISTANCE);

	controls::draw_popup_shadow(t_draw_list, panel, K_SWITCHER_RADIUS, m_switcher_shown);
	t_draw_list->add_bordered_rect(panel, rounded(K_SWITCHER_RADIUS), faded(g_theme.popup, alpha), faded(g_theme.border, alpha), 1.0f);

	draw_switcher_rows(t_draw_list, panel, alpha);
	draw_size_slider(t_draw_list, panel, alpha);
}

auto Carousel::draw(DrawList* t_draw_list) -> void
{
	PULSAR_PROFILE_SCOPE("Carousel.Draw");

	if (m_mode_transition > 0.0f) {
		draw_mode_morph(t_draw_list);
	} else {
		draw_mode(t_draw_list, m_mode);
	}

	if (m_reorder.lifted) {
		draw_raised(t_draw_list, m_mode);
	}

	draw_reorder_hint(t_draw_list);

	if (m_library->game_count > 0) {
		draw_switcher(t_draw_list);
	}
}
