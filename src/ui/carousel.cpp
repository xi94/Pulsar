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
constexpr float card_width = 220.0f;
constexpr float card_height = 300.0f;
constexpr float card_spacing = 36.0f;
constexpr float card_corner_radius = 14.0f;
constexpr float drag_pixels_per_card = card_width + card_spacing;
constexpr float scroll_ease_rate = 12.0f;
constexpr float mode_morph_seconds = 0.34f;
constexpr float zoom_ease_rate = 9.0f;
constexpr float edge_fade_width = 64.0f;
constexpr float carousel_edge_padding = 40.0f;
constexpr float shelf_ease_rate = 12.0f;

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
constexpr float hover_growth = 0.06f;
constexpr float hover_ease_rate = 14.0f;

constexpr float list_thumb_min_size = 56.0f;
constexpr float list_thumb_max_size = 96.0f;
constexpr float list_row_padding_y = 14.0f;
constexpr float list_padding = 16.0f;
constexpr float list_gap = 8.0f;
constexpr float list_corner_radius = 10.0f;

constexpr float icon_art_min_size = 48.0f;
constexpr float icon_art_max_size = 96.0f;
constexpr float icon_art_radius_share = 0.18f;
constexpr float icon_tile_padding_x = 20.0f;
constexpr float icon_tile_padding_top = 12.0f;
constexpr float icon_tile_padding_bottom = 10.0f;
constexpr float icon_tile_gap = 6.0f;
constexpr float icon_tile_radius = 8.0f;
constexpr float icon_label_gap = 8.0f;
constexpr float icon_label_inset = 6.0f;

constexpr float switcher_hold_seconds = 0.7f;
constexpr float switcher_ease_rate = 18.0f;
constexpr float switcher_width = 184.0f;
constexpr float switcher_padding = 6.0f;
constexpr float switcher_radius = 10.0f;
constexpr float switcher_margin = 16.0f;
constexpr float switcher_slide_distance = 8.0f;
constexpr float switcher_icon_size = 24.0f;
constexpr float switcher_icon_gap = 10.0f;
constexpr float switcher_content_inset = 12.0f;
constexpr float switcher_row_radius = 6.0f;
constexpr float switcher_hover_fill = 0.5f;
constexpr float switcher_hover_brightening = 0.5f;
constexpr float size_dot_size = 6.0f;
constexpr float size_dot_gap = 4.0f;
constexpr float size_dot_dimming = 0.45f;
constexpr float size_slider_height = 30.0f;
constexpr float size_slider_fold_rate = 16.0f;
constexpr float size_slider_rise = 6.0f;
constexpr float size_slider_track_height = 4.0f;
constexpr float size_slider_tick_size = 4.0f;
constexpr float size_slider_thumb_size = 12.0f;
constexpr float size_slider_thumb_ring = 2.0f;
constexpr float size_slider_small_icon = 12.0f;
constexpr float size_slider_large_icon = 18.0f;
constexpr float size_slider_icon_gap = 8.0f;
constexpr float size_slider_icon_reach = 4.0f;

constexpr float status_icon_size = 16.0f;
constexpr float status_icon_gap = 8.0f;
constexpr float status_padding_right = 14.0f;
constexpr float baseline_nudge = 2.0f;

constexpr Color color_image{255, 255, 255, 255};
constexpr u8 card_border_alpha = 160;
constexpr u8 card_border_highlighted_alpha = 235;

constexpr i32 zoom_stop_count = 12;
constexpr i32 shelf_stop = 1;
constexpr i32 spread_stop = 2;
constexpr i32 grid_first_stop = 3;
constexpr i32 grid_last_stop = 5;
constexpr i32 icons_first_stop = 6;
constexpr i32 icons_last_stop = 8;
constexpr i32 list_first_stop = 9;
constexpr i32 list_last_stop = 11;

struct SwitcherRow {
	std::string_view name;
	Asset icon;
	i32 first_stop;
	i32 last_stop;
};

constexpr SwitcherRow switcher_rows[]{
	{"List", Asset::icon_list, list_first_stop, list_last_stop},
	{"Icons", Asset::icon_icons, icons_first_stop, icons_last_stop},
	{"Grid", Asset::icon_grid, grid_first_stop, grid_last_stop},
	{"Shelf", Asset::icon_shelf, shelf_stop, spread_stop},
	{"Carousel", Asset::icon_carousel, 0, 0},
};
constexpr u32 switcher_row_count = static_cast<u32>(std::size(switcher_rows));

u32 row_index_at_stop(i32 t_stop)
{
	for (u32 index = 0; index < switcher_row_count; index += 1) {
		if (t_stop >= switcher_rows[index].first_stop && t_stop <= switcher_rows[index].last_stop) return index;
	}

	return switcher_row_count - 1;
}

const SwitcherRow &row_at_stop(i32 t_stop)
{
	return switcher_rows[row_index_at_stop(t_stop)];
}

bool has_sizes(const SwitcherRow &t_row)
{
	return t_row.last_stop > t_row.first_stop;
}

bool is_shelf_stop(i32 t_stop)
{
	return t_stop == shelf_stop || t_stop == spread_stop;
}

ViewMode mode_at_stop(i32 t_stop)
{
	if (t_stop <= spread_stop) return ViewMode::carousel;
	if (t_stop <= grid_last_stop) return ViewMode::grid;
	if (t_stop <= icons_last_stop) return ViewMode::icons;

	return ViewMode::list;
}

bool uses_frame(ViewMode t_mode)
{
	return t_mode == ViewMode::list || t_mode == ViewMode::icons;
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
	const float t = zoom_within(t_zoom_percent, list_first_stop, list_last_stop);

	return snapped_to_pixel((list_thumb_min_size + (list_thumb_max_size - list_thumb_min_size) * t) * t_view_scale);
}

float icon_art_size(float t_zoom_percent, float t_view_scale)
{
	const float t = zoom_within(t_zoom_percent, icons_first_stop, icons_last_stop);

	return snapped_to_pixel((icon_art_min_size + (icon_art_max_size - icon_art_min_size) * t) * t_view_scale);
}

float art_radius(ViewMode t_mode, Rect t_art)
{
	switch (t_mode) {
		case ViewMode::list:
			return list_corner_radius;
		case ViewMode::icons:
			return t_art.w * icon_art_radius_share;
		case ViewMode::carousel:
		case ViewMode::grid:
			break;
	}

	return card_corner_radius;
}

void draw_centered_label(DrawList &t_draw_list, const Font &t_font, float t_center_x, float t_baseline,
						 std::string_view t_text, float t_max_width, Color t_color)
{
	const float width = std::min(text_width(t_font, t_text), t_max_width);

	draw_text_truncated(t_draw_list, t_font, Vec2{snapped_to_pixel(t_center_x - width * 0.5f), t_baseline}, t_text,
						t_max_width, t_color);
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

CardLook blended_look(const CardLook &t_from, const CardLook &t_to, float t_amount)
{
	const Color from = t_from.border_thickness > 0.0f ? t_from.border : with_alpha(t_to.border, 0);
	const Color to = t_to.border_thickness > 0.0f ? t_to.border : with_alpha(t_from.border, 0);
	const auto blend = [t_amount](float t_start, float t_end) { return t_start + (t_end - t_start) * t_amount; };

	return CardLook{with_alpha(mix(from, to, t_amount), static_cast<u8>(blend(from.a, to.a))),
					blend(t_from.border_thickness, t_to.border_thickness), blend(t_from.glow_size, t_to.glow_size),
					static_cast<u8>(blend(t_from.glow_alpha, t_to.glow_alpha))};
}

void draw_framed_art(DrawList &t_draw_list, Rect t_rect, const Game &t_game, const CardLook &t_look, float t_radius,
					 float t_icon_share, u8 t_alpha)
{
	if (t_look.glow_size > 0.0f && t_look.glow_alpha > 0) {
		t_draw_list.add_banner_glow(t_rect, t_radius, t_look.glow_size,
									faded(faded(t_game.accent, t_look.glow_alpha), t_alpha));
	}

	if (t_look.border_thickness > 0.0f) {
		t_draw_list.add_rounded_rect(t_rect, rounded(t_radius), faded(t_look.border, t_alpha));
	}

	const Rect art = t_rect.inset(t_look.border_thickness);
	const CornerRadii radii = rounded(std::max(0.0f, t_radius - t_look.border_thickness));
	const float icon_share = t_game.icon != nullptr ? t_icon_share : 0.0f;

	if (t_game.banner != nullptr && icon_share < 1.0f) {
		t_draw_list.add_image(art, t_game.banner, faded(color_image, static_cast<u8>(t_alpha * (1.0f - icon_share))),
							  radii, cover_uv(art.w / art.h, t_game.banner->aspect()));
	}

	if (icon_share > 0.0f) {
		t_draw_list.add_image(art, t_game.icon, faded(color_image, static_cast<u8>(t_alpha * icon_share)), radii,
							  cover_uv(art.w / art.h, t_game.icon->aspect()));
	}
}

float size_slider_center_y(Rect t_slider)
{
	return t_slider.y + (t_slider.h - switcher_padding) * 0.5f;
}

Rect size_slider_icon_rect(Rect t_slider, bool t_large)
{
	const float size = t_large ? size_slider_large_icon : size_slider_small_icon;
	const float x = t_large ? t_slider.right() - switcher_content_inset - size : t_slider.x + switcher_content_inset;

	return Rect{x, snapped_to_pixel(size_slider_center_y(t_slider) - size * 0.5f), size, size};
}

Rect size_slider_track_rect(Rect t_slider)
{
	const float left = size_slider_icon_rect(t_slider, false).right() + size_slider_icon_gap;
	const float right = size_slider_icon_rect(t_slider, true).x - size_slider_icon_gap;

	return Rect{left, snapped_to_pixel(size_slider_center_y(t_slider) - size_slider_track_height * 0.5f),
				std::max(0.0f, right - left), size_slider_track_height};
}

Rect size_slider_grab_rect(Rect t_slider)
{
	const Rect track = size_slider_track_rect(t_slider);

	return Rect{track.x - size_slider_thumb_size * 0.5f, t_slider.y, track.w + size_slider_thumb_size, t_slider.h};
}

i32 stop_at_track_x(const SwitcherRow &t_row, Rect t_track, float t_x)
{
	const float t = t_track.w > 0.0f ? std::clamp((t_x - t_track.x) / t_track.w, 0.0f, 1.0f) : 0.0f;

	return t_row.first_stop + static_cast<i32>(std::round(t * static_cast<float>(t_row.last_stop - t_row.first_stop)));
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

bool is_primary_button_held()
{
	return (GetKeyState(VK_LBUTTON) & 0x8000) != 0;
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
	m_shelf = is_shelf_stop(m_zoom_stop) ? 1.0f : 0.0f;
	m_spread = m_zoom_stop == spread_stop ? 1.0f : 0.0f;
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

Rect Carousel::centered_carousel_slot(float t_offset) const
{
	const float view = view_scale();
	const float scale = card_scale(std::fabs(t_offset)) * view;
	const float width = card_width * scale;
	const float height = card_height * scale;
	const float center_x = m_bounds.center().x + center_offset_of_slot(t_offset) * view;

	return Rect{center_x - width * 0.5f, m_bounds.y + (m_bounds.h - height) * 0.5f, width, height};
}

float Carousel::carousel_camera_shift() const
{
	if (game_count() == 0) return 0.0f;

	const Rect first = centered_carousel_slot(-m_scroll);
	const Rect last = centered_carousel_slot(static_cast<float>(game_count() - 1) - m_scroll);
	const float padding = carousel_edge_padding * view_scale();
	const float left_limit = m_bounds.x + padding;
	const float right_limit = m_bounds.right() - padding;

	if (last.right() - first.x <= right_limit - left_limit) {
		return m_bounds.center().x - (first.x + last.right()) * 0.5f;
	}

	return std::clamp(0.0f, right_limit - last.right(), left_limit - first.x);
}

Rect Carousel::carousel_slot(float t_offset) const
{
	const Rect centered = centered_carousel_slot(t_offset);

	return m_shelf > 0.0f ? translated(centered, Vec2{carousel_camera_shift() * m_shelf, 0.0f}) : centered;
}

Rect Carousel::overview_slot(u32 t_slot) const
{
	const u32 count = std::max<u32>(1, game_count());
	const float aspect = card_width / card_height;
	const Rect area{m_bounds.x + overview_padding, m_bounds.y + overview_padding, m_bounds.w - overview_padding * 2.0f,
					m_bounds.h - overview_padding * 2.0f - overview_hint_room * m_reorder_hint};

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

Vec2 Carousel::icon_tile_size() const
{
	const float art = icon_art_size(m_zoom_percent, view_scale());
	return Vec2{art + icon_tile_padding_x * 2.0f,
				icon_tile_padding_top + art + icon_label_gap + m_fonts.body().line_height() + icon_tile_padding_bottom};
}

u32 Carousel::icon_columns() const
{
	const float usable = m_bounds.w - grid_padding * 2.0f + icon_tile_gap;

	return std::max<u32>(1, static_cast<u32>(usable / (icon_tile_size().x + icon_tile_gap)));
}

Rect Carousel::icon_slot(u32 t_slot) const
{
	const Vec2 tile = icon_tile_size();
	const u32 columns = icon_columns();
	const float x = m_bounds.x + grid_padding + (t_slot % columns) * (tile.x + icon_tile_gap);
	const float y = m_bounds.y + grid_padding + (t_slot / columns) * (tile.y + icon_tile_gap) - m_wrap_scroll.offset();

	return Rect{x, y, tile.x, tile.y};
}

Rect Carousel::icon_tile_art(Rect t_tile) const
{
	const float art = std::max(0.0f, t_tile.w - icon_tile_padding_x * 2.0f);

	return Rect{t_tile.center().x - art * 0.5f, t_tile.y + icon_tile_padding_top, art, art};
}

u32 Carousel::wrap_columns() const
{
	switch (m_mode) {
		case ViewMode::grid:
			return grid_columns(m_bounds.w, m_zoom_percent, view_scale());
		case ViewMode::icons:
			return icon_columns();
		case ViewMode::carousel:
		case ViewMode::list:
			break;
	}

	return 0;
}

Rect Carousel::slot_rect(ViewMode t_mode, u32 t_slot) const
{
	switch (t_mode) {
		case ViewMode::grid:
			return grid_slot(t_slot);
		case ViewMode::list:
			return list_slot(t_slot);
		case ViewMode::icons:
			return icon_slot(t_slot);
		case ViewMode::carousel:
			break;
	}

	const Rect carousel = carousel_slot(static_cast<float>(t_slot) - m_scroll);

	const float overview = std::max(m_overview, m_spread);

	return overview > 0.0f ? lerp_rect(carousel, overview_slot(t_slot), overview) : carousel;
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

	const bool centered =
		std::fabs(static_cast<float>(m_slot_of[t_game]) - m_scroll) < 0.5f && m_overview < 0.5f && m_spread < 0.5f;

	return CardState{centered || hovered, centered};
}

Rect Carousel::list_thumb(Rect t_row) const
{
	const float size = list_thumb_size(m_zoom_percent, view_scale());

	return Rect{t_row.x + 12.0f, t_row.y + (t_row.h - size) * 0.5f, size, size};
}

Rect Carousel::grown(Rect t_card, u32 t_game) const
{
	const float growth = 1.0f + hover_growth * m_card_hover[t_game];

	return t_card.inset(t_card.w * (1.0f - growth) * 0.5f, t_card.h * (1.0f - growth) * 0.5f);
}

Rect Carousel::art_rect(ViewMode t_mode, u32 t_game) const
{
	const Rect card = shown_card(t_mode, t_game);

	switch (t_mode) {
		case ViewMode::list:
			return list_thumb(card);
		case ViewMode::icons:
			return grown(icon_tile_art(card), t_game);
		case ViewMode::carousel:
		case ViewMode::grid:
			break;
	}

	return grown(card, t_game);
}

Rect Carousel::morph_art(u32 t_game) const
{
	const Rect current = art_rect(m_mode, t_game);

	return m_mode_transition > 0.0f ? lerp_rect(m_morph_from_art[t_game], current, mode_morph()) : current;
}

float Carousel::mode_morph() const
{
	return 0.5f - 0.5f * std::cos((1.0f - m_mode_transition) * std::numbers::pi_v<float>);
}

ArtSource Carousel::art_source(u32 t_game) const
{
	const Rect card = shown_card(m_mode, t_game);
	const float scale = lift_scale(t_game);
	const Game &game = m_library.game(t_game);

	if (uses_frame(m_mode)) {
		const Rect lifted = scaled_from_center(card, scale);
		const Rect art = m_mode == ViewMode::list ? list_thumb(lifted) : grown(icon_tile_art(lifted), t_game);

		return ArtSource{.rect = art, .radius = art_radius(m_mode, art), .is_icon = game.icon != nullptr};
	}

	const CardState state = card_state(m_mode, t_game, card);
	const CardLook look = card_look(state.highlighted, state.centered);
	const Rect art = grown(card, t_game);

	return ArtSource{.rect = scaled_from_center(art, scale),
					 .radius = card_corner_radius,
					 .is_icon = false,
					 .border = look.border_thickness,
					 .border_color = look.border,
					 .glow = look.glow_size,
					 .glow_color = faded(game.accent, look.glow_alpha)};
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

	if (m_mode == ViewMode::icons) {
		const u32 rows = (game_count() + icon_columns() - 1) / icon_columns();

		return grid_padding * 2.0f + rows * icon_tile_size().y + (rows - 1) * icon_tile_gap;
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
	const float width =
		status_icon_size + status_icon_gap + text_width(m_fonts.secondary(), row_at_stop(m_zoom_stop).name);

	return Rect{m_bounds.right() - status_padding_right - width, m_bounds.bottom(), width, status_bar_height};
}

float Carousel::switcher_row_height() const
{
	return m_fonts.body().line_height() + 10.0f;
}

float Carousel::size_slider_shown_height() const
{
	return snapped_to_pixel(size_slider_height * m_size_slider_fold);
}

Rect Carousel::switcher_panel_rect() const
{
	const float height =
		switcher_padding * 2.0f + switcher_row_height() * switcher_row_count + size_slider_shown_height();

	return Rect{m_bounds.right() - switcher_width - switcher_margin, m_bounds.bottom() - height - switcher_margin,
				switcher_width, height};
}

Rect Carousel::switcher_row_rect(Rect t_panel, u32 t_row) const
{
	const float row_height = switcher_row_height();
	const float fold = t_row > row_index_at_stop(m_zoom_stop) ? size_slider_shown_height() : 0.0f;

	return Rect{t_panel.x + switcher_padding, t_panel.y + switcher_padding + row_height * t_row + fold,
				t_panel.w - switcher_padding * 2.0f, row_height};
}

Rect Carousel::switcher_active_pill(Rect t_panel) const
{
	const Rect row = switcher_row_rect(t_panel, row_index_at_stop(m_zoom_stop));

	return Rect{row.x, row.y, row.w, row.h + size_slider_shown_height()};
}

Rect Carousel::size_slider_rect(Rect t_panel) const
{
	const Rect row = switcher_row_rect(t_panel, row_index_at_stop(m_zoom_stop));

	return Rect{row.x, row.bottom(), row.w, size_slider_height};
}

bool Carousel::is_size_slider_open() const
{
	return m_size_slider_fold > 0.5f && has_sizes(row_at_stop(m_zoom_stop));
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
			capture_mode_morph();
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

void Carousel::drop_lost_press()
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
	if (m_long_press.game >= 0 || m_reorder.active) {
		animation::request_frame();
	}

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

void Carousel::capture_mode_morph()
{
	for (u32 game = 0; game < game_count(); game += 1) {
		m_morph_from_art[game] = morph_art(game);
		m_morph_from_frame[game] = shown_card(m_mode, game);
	}
}

void Carousel::smooth_grid_reflow()
{
	const u32 columns = wrap_columns();

	if (columns != 0 && m_grid_columns != 0 && columns != m_grid_columns) {
		for (u32 game = 0; game < game_count(); game += 1) {
			if (m_reorder.active && static_cast<i32>(game) == m_reorder.game) continue;

			const Vec2 slot = slot_rect(m_mode, m_slot_of[game]).center();
			m_card_offset[game] = Vec2{m_last_centers[game].x - slot.x, m_last_centers[game].y - slot.y};
		}
	}

	m_grid_columns = columns;
}

bool Carousel::switcher_pointer_down(Vec2 t_point)
{
	const Rect panel = switcher_panel_rect();
	if (!is_switcher_shown() || !panel.contains(t_point)) return false;

	m_switcher_owns_pointer = true;

	const u32 active = row_index_at_stop(m_zoom_stop);
	const SwitcherRow &entry = switcher_rows[active];

	if (is_size_slider_open()) {
		const Rect slider = size_slider_rect(panel);

		if (size_slider_icon_rect(slider, false).inset(-size_slider_icon_reach).contains(t_point)) {
			set_zoom_stop(std::max(entry.first_stop, m_zoom_stop - 1));
			return true;
		}

		if (size_slider_icon_rect(slider, true).inset(-size_slider_icon_reach).contains(t_point)) {
			set_zoom_stop(std::min(entry.last_stop, m_zoom_stop + 1));
			return true;
		}

		if (size_slider_grab_rect(slider).contains(t_point)) {
			m_switcher_drag.begin(t_point);
			set_zoom_stop(stop_at_track_x(entry, size_slider_track_rect(slider), t_point.x));
			return true;
		}
	}

	for (u32 row = 0; row < switcher_row_count; row += 1) {
		if (row != active && switcher_row_rect(panel, row).contains(t_point)) {
			set_zoom_stop(switcher_rows[row].first_stop);
			return true;
		}
	}

	return true;
}

bool Carousel::switcher_pointer_move(Vec2 t_point)
{
	if (!m_switcher_drag.is_pressed()) return false;

	m_switcher_drag.update(t_point);

	const Rect slider = size_slider_rect(switcher_panel_rect());
	set_zoom_stop(stop_at_track_x(row_at_stop(m_zoom_stop), size_slider_track_rect(slider), t_point.x));

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

	if (m_zoom_stop != spread_stop) {
		m_card_drag.begin(t_point);
		m_drag_start_scroll = m_scroll;
	}

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

	if (m_zoom_stop == spread_stop) {
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

	const bool horizontal = m_mode != ViewMode::list;
	const bool vertical = m_mode != ViewMode::carousel;
	const i32 row_step = std::max(1, static_cast<i32>(wrap_columns()));

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
		const u32 active = row_index_at_stop(m_zoom_stop);

		for (u32 row = 0; row < switcher_row_count; row += 1) {
			if (row != active && switcher_row_rect(panel, row).contains(m_mouse)) return CursorKind::hand;
		}

		if (is_size_slider_open()) {
			const Rect slider = size_slider_rect(panel);

			if (size_slider_grab_rect(slider).contains(m_mouse) ||
				size_slider_icon_rect(slider, false).inset(-size_slider_icon_reach).contains(m_mouse) ||
				size_slider_icon_rect(slider, true).inset(-size_slider_icon_reach).contains(m_mouse)) {
				return CursorKind::hand;
			}
		}
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
	if (!is_primary_button_held() || m_detached_game >= 0) {
		drop_lost_press();
	}

	m_scroll = animation::ease_toward(m_scroll, m_target_scroll, scroll_ease_rate, t_delta_seconds,
									  animation::settled_pixels / (drag_pixels_per_card * view_scale()));
	m_mode_transition = animation::step_toward(m_mode_transition, 0.0f, mode_morph_seconds, t_delta_seconds);
	m_zoom_percent = animation::ease_toward(m_zoom_percent, stop_percent(m_zoom_stop), zoom_ease_rate, t_delta_seconds);
	m_shelf =
		animation::ease_toward(m_shelf, is_shelf_stop(m_zoom_stop) ? 1.0f : 0.0f, shelf_ease_rate, t_delta_seconds);
	m_spread = animation::ease_toward(m_spread, m_zoom_stop == spread_stop ? 1.0f : 0.0f, overview_ease_rate,
									  t_delta_seconds, animation::settled_pixels / std::max(m_bounds.w, 1.0f));
	m_wrap_scroll.update(t_delta_seconds);
	smooth_grid_reflow();

	const i32 hovered_card = m_mode != ViewMode::list && !m_reorder.active ? game_at(m_mouse) : -1;

	for (u32 game = 0; game < game_count(); game += 1) {
		const bool raised = static_cast<i32>(game) == hovered_card || is_focus_shown(game);
		m_card_hover[game] =
			animation::ease_toward(m_card_hover[game], raised ? 1.0f : 0.0f, hover_ease_rate, t_delta_seconds);
	}

	if (is_mouse_over_switcher(m_mouse)) {
		m_switcher_hold_seconds = switcher_hold_seconds;
	} else {
		m_switcher_hold_seconds = std::max(0.0f, m_switcher_hold_seconds - t_delta_seconds);
	}

	if (m_switcher_hold_seconds > 0.0f) {
		animation::request_frame_after(m_switcher_hold_seconds);
	}

	const float switcher_target = m_switcher_hold_seconds > 0.0f ? 1.0f : 0.0f;
	m_switcher_shown = animation::ease_toward(m_switcher_shown, switcher_target, switcher_ease_rate, t_delta_seconds);

	const bool fold_open =
		is_switcher_shown() && has_sizes(row_at_stop(m_zoom_stop)) &&
		(m_switcher_drag.is_pressed() || switcher_active_pill(switcher_panel_rect()).contains(m_mouse));
	m_size_slider_fold = animation::ease_toward(m_size_slider_fold, fold_open ? 1.0f : 0.0f, size_slider_fold_rate,
												t_delta_seconds, animation::settled_pixels / size_slider_height);

	update_reorder(t_delta_seconds);

	for (u32 game = 0; game < game_count(); game += 1) {
		m_last_centers[game] = shown_card(m_mode, game).center();
	}
}

void Carousel::draw_card(DrawList &t_draw_list, Rect t_rect, const Game &t_game, bool t_highlighted, bool t_centered,
						 u8 t_alpha) const
{
	draw_framed_art(t_draw_list, t_rect, t_game, card_look(t_highlighted, t_centered), card_corner_radius, 0.0f,
					t_alpha);
}

void Carousel::draw_carousel_mode(DrawList &t_draw_list) const
{
	for (u32 game = 0; game < game_count(); game += 1) {
		if (static_cast<i32>(game) == m_detached_game || is_raised(ViewMode::carousel, game)) continue;

		const Rect card = shown_card(ViewMode::carousel, game);
		if (card.right() < m_bounds.x || card.x > m_bounds.right()) continue;

		const CardState state = card_state(ViewMode::carousel, game, card);
		draw_card(t_draw_list, grown(card, game), m_library.game(game), state.highlighted, state.centered, 255);
	}

	if (!m_reorder.lifted) {
		draw_raised(t_draw_list, ViewMode::carousel);
	}

	draw_carousel_edges(t_draw_list, 255);
}

void Carousel::draw_carousel_edges(DrawList &t_draw_list, u8 t_alpha) const
{
	if (game_count() == 0) return;

	const Rect first = carousel_slot(-m_scroll);
	const Rect last = carousel_slot(static_cast<float>(game_count() - 1) - m_scroll);
	const float fade = t_alpha * (1.0f - std::max(m_overview, m_spread));
	const float left_overflow = std::clamp((m_bounds.x - first.x) / edge_fade_width, 0.0f, 1.0f);
	const float right_overflow = std::clamp((last.right() - m_bounds.right()) / edge_fade_width, 0.0f, 1.0f);
	const float left = 1.0f + (left_overflow - 1.0f) * m_shelf;
	const float right = 1.0f + (right_overflow - 1.0f) * m_shelf;
	const Color clear = faded(theme().window, 0);

	if (left > 0.0f) {
		const Color opaque = faded(theme().window, static_cast<u8>(fade * left));
		fade_cards(t_draw_list, Rect{m_bounds.x, m_bounds.y, edge_fade_width, m_bounds.h}, opaque, clear, opaque,
				   clear);
	}

	if (right > 0.0f) {
		const Color opaque = faded(theme().window, static_cast<u8>(fade * right));
		fade_cards(t_draw_list, Rect{m_bounds.right() - edge_fade_width, m_bounds.y, edge_fade_width, m_bounds.h},
				   clear, opaque, clear, opaque);
	}
}

Rect Carousel::faded_rect(u32 t_game) const
{
	if (m_mode_transition > 0.0f) return morph_art(t_game);

	const Rect shown = shown_card(m_mode, t_game);

	return uses_frame(m_mode) ? shown : grown(shown, t_game);
}

void Carousel::fade_cards(DrawList &t_draw_list, Rect t_band, Color t_top_left, Color t_top_right, Color t_bottom_left,
						  Color t_bottom_right) const
{
	if (t_band.w <= 0.0f || t_band.h <= 0.0f) return;

	for (u32 game = 0; game < game_count(); game += 1) {
		const Rect card = faded_rect(game);
		const bool touches =
			card.right() > t_band.x && card.x < t_band.right() && card.bottom() > t_band.y && card.y < t_band.bottom();
		if (!touches) continue;

		t_draw_list.push_clip(card);
		t_draw_list.add_plain_backdrop(t_band, t_top_left, t_top_right, t_bottom_left, t_bottom_right);
		t_draw_list.pop_clip();
	}
}

void Carousel::draw_wrap_scroll(DrawList &t_draw_list, u8 t_alpha) const
{
	const ScrollGeometry geometry = wrap_scroll_geometry();
	const Scrollable::EdgeFades fades = m_wrap_scroll.edge_fades(m_bounds, geometry);
	const Color edge = faded(theme().window, t_alpha);
	const Color clear = faded(theme().window, 0);

	fade_cards(t_draw_list, fades.top, edge, edge, clear, clear);
	fade_cards(t_draw_list, fades.bottom, clear, clear, edge, edge);
	m_wrap_scroll.draw(t_draw_list, geometry, m_mouse, t_alpha);
}

void Carousel::draw_grid_mode(DrawList &t_draw_list) const
{
	t_draw_list.push_clip(m_bounds);

	for (u32 game = 0; game < game_count(); game += 1) {
		if (static_cast<i32>(game) == m_detached_game || is_raised(ViewMode::grid, game)) continue;

		const Rect card = shown_card(ViewMode::grid, game);
		if (!card.overlaps_vertically(m_bounds)) continue;

		const CardState state = card_state(ViewMode::grid, game, card);
		draw_card(t_draw_list, grown(card, game), m_library.game(game), state.highlighted, false, 255);
	}

	if (!m_reorder.lifted) {
		draw_raised(t_draw_list, ViewMode::grid);
	}

	t_draw_list.pop_clip();

	draw_wrap_scroll(t_draw_list, 255);
}

void Carousel::draw_list_row_frame(DrawList &t_draw_list, Rect t_row, u32 t_game, bool t_highlighted, u8 t_alpha) const
{
	const Font &font = m_fonts.body();
	const Rect thumb = list_thumb(t_row);

	t_draw_list.add_rounded_rect(t_row, rounded(list_corner_radius),
								 faded(t_highlighted ? theme().control : theme().popup, t_alpha));
	draw_text_truncated(t_draw_list, font, Vec2{thumb.right() + 16.0f, font.centered_baseline(t_row)},
						m_library.game(t_game).title, t_row.right() - 16.0f - (thumb.right() + 16.0f),
						faded(theme().text, t_alpha));
}

void Carousel::draw_list_row(DrawList &t_draw_list, Rect t_row, u32 t_game, bool t_highlighted, u8 t_alpha) const
{
	draw_list_row_frame(t_draw_list, t_row, t_game, t_highlighted, t_alpha);

	if (static_cast<i32>(t_game) == m_detached_game) return;

	draw_framed_art(t_draw_list, list_thumb(t_row), m_library.game(t_game), CardLook{}, list_corner_radius, 1.0f,
					t_alpha);
}

void Carousel::draw_icon_tile_frame(DrawList &t_draw_list, Rect t_tile, u32 t_game, bool t_highlighted,
									u8 t_alpha) const
{
	const Font &title_font = m_fonts.body();
	const Game &game = m_library.game(t_game);
	const Rect art = icon_tile_art(t_tile);
	const float label_width = t_tile.w - icon_label_inset * 2.0f;

	if (t_highlighted) {
		t_draw_list.add_rounded_rect(t_tile, rounded(icon_tile_radius), faded(theme().row_hover, t_alpha));
	}

	if (is_focus_shown(t_game)) {
		t_draw_list.add_bordered_rect(t_tile, rounded(icon_tile_radius), Color{}, faded(m_settings.accent, t_alpha),
									  1.0f);
	}

	const float title_baseline = art.bottom() + icon_label_gap + title_font.ascent();
	draw_centered_label(t_draw_list, title_font, t_tile.center().x, title_baseline, game.short_title, label_width,
						faded(theme().text, t_alpha));
}

void Carousel::draw_icon_tile(DrawList &t_draw_list, Rect t_tile, u32 t_game, bool t_highlighted, u8 t_alpha) const
{
	draw_icon_tile_frame(t_draw_list, t_tile, t_game, t_highlighted, t_alpha);

	if (static_cast<i32>(t_game) == m_detached_game) return;

	const Rect art = grown(icon_tile_art(t_tile), t_game);
	draw_framed_art(t_draw_list, art, m_library.game(t_game), CardLook{}, art_radius(ViewMode::icons, art), 1.0f,
					t_alpha);
}

void Carousel::draw_frame(DrawList &t_draw_list, ViewMode t_mode, Rect t_frame, u32 t_game, bool t_highlighted,
						  u8 t_alpha) const
{
	if (t_mode == ViewMode::icons) {
		draw_icon_tile_frame(t_draw_list, t_frame, t_game, t_highlighted, t_alpha);
	} else {
		draw_list_row_frame(t_draw_list, t_frame, t_game, t_highlighted, t_alpha);
	}
}

void Carousel::draw_icons_mode(DrawList &t_draw_list) const
{
	t_draw_list.push_clip(m_bounds);

	for (u32 game = 0; game < game_count(); game += 1) {
		if (is_raised(ViewMode::icons, game)) continue;

		const Rect tile = shown_card(ViewMode::icons, game);
		if (!tile.overlaps_vertically(m_bounds)) continue;

		draw_icon_tile(t_draw_list, tile, game, card_state(ViewMode::icons, game, tile).highlighted, 255);
	}

	if (!m_reorder.lifted) {
		draw_raised(t_draw_list, ViewMode::icons);
	}

	t_draw_list.pop_clip();

	draw_wrap_scroll(t_draw_list, 255);
}

void Carousel::draw_list_mode(DrawList &t_draw_list) const
{
	t_draw_list.push_clip(m_bounds);

	for (u32 game = 0; game < game_count(); game += 1) {
		if (is_raised(ViewMode::list, game)) continue;

		const Rect row = shown_card(ViewMode::list, game);
		if (!row.overlaps_vertically(m_bounds)) continue;

		draw_list_row(t_draw_list, row, game, card_state(ViewMode::list, game, row).highlighted, 255);
	}

	if (!m_reorder.lifted) {
		draw_raised(t_draw_list, ViewMode::list);
	}

	t_draw_list.pop_clip();

	draw_wrap_scroll(t_draw_list, 255);
}

void Carousel::draw_raised(DrawList &t_draw_list, ViewMode t_mode) const
{
	if (m_reorder.game < 0 || t_mode != m_mode) return;

	const auto game = static_cast<u32>(m_reorder.game);
	const bool framed = uses_frame(t_mode);
	const float radius = t_mode == ViewMode::list	 ? list_corner_radius
						 : t_mode == ViewMode::icons ? icon_tile_radius
													 : card_corner_radius;

	if (!framed && m_reorder.game == m_detached_game) return;

	if (m_reorder.active) {
		t_draw_list.add_rounded_rect(slot_rect(t_mode, m_slot_of[game]), rounded(radius),
									 with_alpha(m_settings.accent, placeholder_alpha));
	}

	Rect card = shown_card(t_mode, game);
	const CardState state = card_state(t_mode, game, card);

	if (!uses_frame(t_mode)) {
		card = grown(card, game);
	}

	const Rect lifted = scaled_from_center(card, lift_scale(game));

	controls::draw_panel_shadow(t_draw_list, lifted, radius, m_lift);

	if (t_mode == ViewMode::list) {
		draw_list_row(t_draw_list, lifted, game, state.highlighted, 255);
	} else if (t_mode == ViewMode::icons) {
		draw_icon_tile(t_draw_list, lifted, game, state.highlighted, 255);
	} else {
		draw_card(t_draw_list, lifted, m_library.game(game), state.highlighted, state.centered, 255);
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

void Carousel::draw_mode(DrawList &t_draw_list, ViewMode t_mode) const
{
	switch (t_mode) {
		case ViewMode::carousel:
			draw_carousel_mode(t_draw_list);
			break;
		case ViewMode::grid:
			draw_grid_mode(t_draw_list);
			break;
		case ViewMode::list:
			draw_list_mode(t_draw_list);
			break;
		case ViewMode::icons:
			draw_icons_mode(t_draw_list);
			break;
	}
}

void Carousel::draw_mode_morph(DrawList &t_draw_list) const
{
	const float amount = mode_morph();
	const auto incoming = static_cast<u8>(255.0f * amount);
	const auto outgoing = static_cast<u8>(255.0f * (1.0f - amount));
	const bool from_framed = uses_frame(m_previous_mode);
	const bool to_framed = uses_frame(m_mode);
	const float cardness_from = from_framed ? 0.0f : 1.0f;
	const float cardness = cardness_from + ((to_framed ? 0.0f : 1.0f) - cardness_from) * amount;

	const auto look_in = [this](ViewMode t_mode, u32 t_game, Rect t_card) {
		if (uses_frame(t_mode)) return CardLook{};

		const CardState state = card_state(t_mode, t_game, t_card);
		return card_look(state.highlighted, state.centered);
	};

	t_draw_list.push_clip(m_bounds);

	for (u32 game = 0; game < game_count(); game += 1) {
		if (from_framed) {
			draw_frame(t_draw_list, m_previous_mode, m_morph_from_frame[game], game, false, outgoing);
		}

		if (to_framed) {
			const Rect frame = shown_card(m_mode, game);
			draw_frame(t_draw_list, m_mode, frame, game, card_state(m_mode, game, frame).highlighted, incoming);
		}
	}

	for (u32 game = 0; game < game_count(); game += 1) {
		if (static_cast<i32>(game) == m_detached_game) continue;

		const Rect from = m_morph_from_art[game];
		const Rect to = art_rect(m_mode, game);
		const CardLook look =
			blended_look(look_in(m_previous_mode, game, from), look_in(m_mode, game, shown_card(m_mode, game)), amount);

		const float radius =
			art_radius(m_previous_mode, from) + (art_radius(m_mode, to) - art_radius(m_previous_mode, from)) * amount;

		draw_framed_art(t_draw_list, lerp_rect(from, to, amount), m_library.game(game), look, radius, 1.0f - cardness,
						255);
	}

	t_draw_list.pop_clip();

	if (m_previous_mode == ViewMode::carousel) {
		draw_carousel_edges(t_draw_list, outgoing);
	}

	if (m_mode == ViewMode::carousel) {
		draw_carousel_edges(t_draw_list, incoming);
	} else {
		draw_wrap_scroll(t_draw_list, incoming);
	}
}

void Carousel::draw_status_bar(DrawList &t_draw_list) const
{
	if (game_count() == 0) return;

	const Font &font = m_fonts.secondary();
	const Rect indicator = status_indicator_rect();
	const Rect icon{indicator.x, indicator.y + (indicator.h - status_icon_size) * 0.5f, status_icon_size,
					status_icon_size};

	const SwitcherRow &current = row_at_stop(m_zoom_stop);

	t_draw_list.add_image(icon, m_assets.get(current.icon), theme().text_dim);
	draw_text(t_draw_list, font,
			  Vec2{icon.right() + status_icon_gap, font.centered_baseline(indicator) - baseline_nudge}, current.name,
			  theme().text_dim);
}

void Carousel::draw_switcher_rows(DrawList &t_draw_list, Rect t_panel, u8 t_alpha) const
{
	const Theme &colors = theme();
	const Font &font = m_fonts.body();
	const u32 active = row_index_at_stop(m_zoom_stop);
	const Color active_fill = hovered(colors.popup);
	const bool pointer_live = !m_switcher_drag.is_pressed();

	t_draw_list.add_rounded_rect(switcher_active_pill(t_panel), rounded(switcher_row_radius),
								 faded(active_fill, t_alpha));

	for (u32 index = 0; index < switcher_row_count; index += 1) {
		const SwitcherRow &entry = switcher_rows[index];
		const Rect row = switcher_row_rect(t_panel, index);
		const bool is_active = index == active;
		const bool is_hovered = !is_active && pointer_live && row.contains(m_mouse);

		if (is_hovered) {
			t_draw_list.add_rounded_rect(row, rounded(switcher_row_radius),
										 faded(mix(colors.popup, active_fill, switcher_hover_fill), t_alpha));
		}

		Color content = colors.text_dim;
		if (is_active) {
			content = colors.text;
		} else if (is_hovered) {
			content = mix(colors.text_dim, colors.text, switcher_hover_brightening);
		}

		const float icon_center_y = row.center().y + font.ascent() * 0.15f;
		const Rect icon{row.x + switcher_content_inset, icon_center_y - switcher_icon_size * 0.5f, switcher_icon_size,
						switcher_icon_size};

		t_draw_list.add_image(icon, m_assets.get(entry.icon), faded(content, t_alpha));
		draw_text(t_draw_list, font, Vec2{icon.right() + switcher_icon_gap, font.centered_baseline(row)}, entry.name,
				  faded(content, t_alpha));

		if (!is_active || !has_sizes(entry) || m_size_slider_fold >= 0.999f) continue;

		const auto dots_alpha = static_cast<u8>(static_cast<float>(t_alpha) * (1.0f - m_size_slider_fold));
		const i32 sizes = entry.last_stop - entry.first_stop + 1;
		float x = row.right() - switcher_content_inset -
				  (static_cast<float>(sizes) * size_dot_size + static_cast<float>(sizes - 1) * size_dot_gap);

		for (i32 size = 0; size < sizes; size += 1) {
			const Rect dot{snapped_to_pixel(x), snapped_to_pixel(icon_center_y - size_dot_size * 0.5f), size_dot_size,
						   size_dot_size};
			const Color fill = entry.first_stop + size == m_zoom_stop
								   ? m_settings.accent
								   : mix(colors.text_faint, active_fill, size_dot_dimming);

			t_draw_list.add_rounded_rect(dot, rounded(size_dot_size * 0.5f), faded(fill, dots_alpha));
			x += size_dot_size + size_dot_gap;
		}
	}
}

void Carousel::draw_size_slider(DrawList &t_draw_list, Rect t_panel, u8 t_alpha) const
{
	const SwitcherRow &entry = row_at_stop(m_zoom_stop);
	if (m_size_slider_fold <= 0.001f || !has_sizes(entry)) return;

	const Theme &colors = theme();
	const Color backdrop = hovered(colors.popup);
	const auto alpha = static_cast<u8>(static_cast<float>(t_alpha) * m_size_slider_fold);
	const bool pointer_live = !m_switcher_drag.is_pressed();

	Rect slider = size_slider_rect(t_panel);
	const Rect shown{slider.x, slider.y, slider.w, size_slider_shown_height()};
	slider.y -= snapped_to_pixel((1.0f - m_size_slider_fold) * size_slider_rise);

	const Rect track = size_slider_track_rect(slider);
	const i32 sizes = entry.last_stop - entry.first_stop + 1;

	t_draw_list.push_clip(shown);

	for (const bool large : {false, true}) {
		const Rect icon = size_slider_icon_rect(slider, large);
		const bool icon_hovered = pointer_live && icon.inset(-size_slider_icon_reach).contains(m_mouse);

		t_draw_list.add_image(icon, m_assets.get(Asset::icon_image),
							  faded(icon_hovered ? colors.text : colors.text_faint, alpha));
	}

	t_draw_list.add_rounded_rect(track, rounded(track.h * 0.5f), faded(colors.track, alpha));

	for (i32 size = 0; size < sizes; size += 1) {
		const float x = track.x + track.w * static_cast<float>(size) / static_cast<float>(sizes - 1);
		const Rect tick{snapped_to_pixel(x - size_slider_tick_size * 0.5f),
						track.center().y - size_slider_tick_size * 0.5f, size_slider_tick_size, size_slider_tick_size};

		t_draw_list.add_rounded_rect(tick, rounded(size_slider_tick_size * 0.5f), faded(colors.text_faint, alpha));
	}

	const float thumb_x = track.x + track.w * zoom_within(m_zoom_percent, entry.first_stop, entry.last_stop);
	const Rect thumb{thumb_x - size_slider_thumb_size * 0.5f, track.center().y - size_slider_thumb_size * 0.5f,
					 size_slider_thumb_size, size_slider_thumb_size};

	t_draw_list.add_rounded_rect(thumb.inset(-size_slider_thumb_ring),
								 rounded(size_slider_thumb_size * 0.5f + size_slider_thumb_ring),
								 faded(backdrop, alpha));
	t_draw_list.add_rounded_rect(thumb, rounded(size_slider_thumb_size * 0.5f), faded(m_settings.accent, alpha));

	t_draw_list.pop_clip();
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
	draw_size_slider(t_draw_list, panel, alpha);
}

void Carousel::draw(DrawList &t_draw_list)
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

	if (game_count() > 0) {
		draw_switcher(t_draw_list);
	}
}
