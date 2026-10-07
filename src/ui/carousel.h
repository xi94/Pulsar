#pragma once

#include <span>

#include "core/library.h"
#include "ui/commands.h"
#include "ui/draggable.h"
#include "ui/library_view.h"
#include "ui/scrollable.h"
#include "ui/widget.h"

class Assets;
struct Fonts;
struct Game;
class LoginSession;
struct Settings;
class Toasts;

enum class ViewMode : u8 {
	CAROUSEL,
	GRID,
	ICONS,
	LIBRARY,
};

class Carousel : public Widget {
  public:
	Carousel(Library*        t_library,
	         const Settings* t_settings,
	         const Fonts*    t_fonts,
	         const Assets*   t_assets,
	         Toasts*         t_toasts,
	         LoginSession*   t_session,
	         CommandQueue*   t_commands);

	auto set_bounds(Rect t_bounds) -> void
	{
		m_bounds = t_bounds;
	}

	[[nodiscard]] auto zoom_stop() const -> i32
	{
		return m_zoom_stop;
	}

	[[nodiscard]] auto selected_game() const -> i32;

	[[nodiscard]] auto is_library() const -> bool
	{
		return m_mode == ViewMode::LIBRARY;
	}

	[[nodiscard]] auto library_view() -> LibraryView*
	{
		return &m_library_view;
	}

	auto restore(i32 t_zoom_stop, i32 t_selected_game) -> void;
	auto set_order(std::span<const u8> t_order) -> void;

	[[nodiscard]] auto order() const -> std::span<const u8>
	{
		return {m_order, m_library->game_count};
	}

	[[nodiscard]] auto art_source(u32 t_game) const -> ArtSource;

	auto set_detached_game(i32 t_game) -> void
	{
		m_detached_game = t_game;
	}

	auto update(float t_delta_seconds) -> void override;
	auto draw(DrawList* t_draw_list) -> void override;
	auto draw_status_bar(DrawList* t_draw_list) const -> void;

	auto on_pointer_down(Vec2 t_point) -> bool override;
	auto on_pointer_move(Vec2 t_point) -> bool override;
	auto on_pointer_up(Vec2 t_point) -> bool override;
	auto on_right_click(Vec2 t_point) -> bool override;
	auto on_scroll(Vec2 t_point, float t_wheel_delta) -> bool override;
	auto on_key_down(os::Key t_key) -> bool override;
	auto on_char(u32 t_character) -> bool override;

	[[nodiscard]] auto cursor() const -> CursorKind override;

  private:
	struct Reorder {
		i32  game   = -1;
		bool active = false;
		bool lifted = false;
		Vec2 grab{};
		u8   start_order[K_MAX_GAMES]{};
	};

	struct CardState {
		bool highlighted = false;
		bool centered    = false;
	};

	struct LongPress {
		i32   game = -1;
		Vec2  origin{};
		float seconds = 0.0f;
	};

	[[nodiscard]] auto clamp_scroll(float t_offset) const -> float;
	[[nodiscard]] auto wrap_content_height() const -> float;
	[[nodiscard]] auto wrap_scroll_geometry() const -> ScrollGeometry;

	[[nodiscard]] auto view_scale() const -> float;
	[[nodiscard]] auto centered_carousel_slot(float t_offset) const -> Rect;
	[[nodiscard]] auto carousel_camera_shift() const -> float;
	[[nodiscard]] auto carousel_slot(float t_offset) const -> Rect;
	[[nodiscard]] auto overview_slot(u32 t_slot) const -> Rect;
	[[nodiscard]] auto grid_slot(u32 t_slot) const -> Rect;
	[[nodiscard]] auto sidebar_column() const -> Rect;
	[[nodiscard]] auto library_pane() const -> Rect;
	[[nodiscard]] auto sidebar_row_height() const -> float;
	[[nodiscard]] auto library_slot(float t_position) const -> Rect;
	[[nodiscard]] auto sidebar_icon(Rect t_row) const -> Rect;
	[[nodiscard]] auto is_library_shown() const -> bool;
	[[nodiscard]] auto icon_tile_size() const -> Vec2;
	[[nodiscard]] auto icon_columns() const -> u32;
	[[nodiscard]] auto icon_slot(u32 t_slot) const -> Rect;
	[[nodiscard]] auto icon_tile_art(Rect t_tile) const -> Rect;
	[[nodiscard]] auto wrap_columns() const -> u32;
	[[nodiscard]] auto wrap_area() const -> Rect;
	[[nodiscard]] auto slot_rect(ViewMode t_mode, u32 t_slot) const -> Rect;
	[[nodiscard]] auto shown_card(ViewMode t_mode, u32 t_game) const -> Rect;
	[[nodiscard]] auto dragged_rect() const -> Rect;
	[[nodiscard]] auto grown(Rect t_card, u32 t_game) const -> Rect;
	[[nodiscard]] auto art_rect(ViewMode t_mode, u32 t_game) const -> Rect;
	[[nodiscard]] auto morph_art(u32 t_game) const -> Rect;
	[[nodiscard]] auto mode_morph() const -> float;
	[[nodiscard]] auto lift_scale(u32 t_game) const -> float;
	[[nodiscard]] auto is_raised(ViewMode t_mode, u32 t_game) const -> bool;
	[[nodiscard]] auto card_state(ViewMode t_mode, u32 t_game, Rect t_card) const -> CardState;
	[[nodiscard]] auto game_at(Vec2 t_point) const -> i32;

	[[nodiscard]] auto status_indicator_rect() const -> Rect;
	[[nodiscard]] auto switcher_row_height() const -> float;
	[[nodiscard]] auto size_slider_shown_height() const -> float;
	[[nodiscard]] auto switcher_panel_rect() const -> Rect;
	[[nodiscard]] auto switcher_row_rect(Rect t_panel, u32 t_row) const -> Rect;
	[[nodiscard]] auto switcher_active_pill(Rect t_panel) const -> Rect;
	[[nodiscard]] auto size_slider_rect(Rect t_panel) const -> Rect;
	[[nodiscard]] auto is_size_slider_open() const -> bool;
	[[nodiscard]] auto is_switcher_shown() const -> bool;
	[[nodiscard]] auto is_mouse_over_switcher(Vec2 t_mouse) const -> bool;

	auto set_zoom_stop(i32 t_stop) -> void;
	[[nodiscard]] auto focused_game() const -> i32;
	[[nodiscard]] auto is_focus_shown(u32 t_game) const -> bool;
	auto move_focus(i32 t_delta) -> void;
	auto open_game(i32 t_game) -> void;
	auto choose_game(i32 t_game) -> void;

	auto drop_lost_press() -> void;
	auto start_press(i32 t_game, Vec2 t_point) -> void;
	auto begin_reorder(u32 t_game, Vec2 t_point) -> void;
	auto end_reorder(bool t_cancel) -> void;
	auto move_to_slot(u32 t_game, u32 t_slot) -> void;
	auto capture_centers(Vec2 (&t_centers)[K_MAX_GAMES]) const -> void;
	auto restore_centers(const Vec2 (&t_centers)[K_MAX_GAMES]) -> void;
	auto retarget_reorder() -> void;
	auto update_reorder(float t_delta_seconds) -> void;
	auto capture_mode_morph() -> void;
	auto smooth_grid_reflow() -> void;

	[[nodiscard]] auto switcher_pointer_down(Vec2 t_point) -> bool;
	[[nodiscard]] auto switcher_pointer_move(Vec2 t_point) -> bool;
	[[nodiscard]] auto switcher_pointer_up() -> bool;

	auto draw_card(DrawList* t_draw_list, Rect t_rect, const Game& t_game, bool t_highlighted, bool t_centered, u8 t_alpha) const -> void;
	auto draw_sidebar_row_frame(DrawList* t_draw_list, Rect t_row, u32 t_game, bool t_highlighted, u8 t_alpha) const -> void;
	auto draw_sidebar_row(DrawList* t_draw_list, Rect t_row, u32 t_game, bool t_highlighted, u8 t_alpha) const -> void;
	auto draw_sidebar_chrome(DrawList* t_draw_list, Rect t_selection, u8 t_alpha) const -> void;
	auto draw_icon_tile_frame(DrawList* t_draw_list, Rect t_tile, u32 t_game, bool t_highlighted, u8 t_alpha) const -> void;
	auto draw_icon_tile(DrawList* t_draw_list, Rect t_tile, u32 t_game, bool t_highlighted, u8 t_alpha) const -> void;
	auto draw_frame(DrawList* t_draw_list, ViewMode t_mode, Rect t_frame, u32 t_game, bool t_highlighted, u8 t_alpha) const -> void;
	auto draw_icons_mode(DrawList* t_draw_list) const -> void;
	auto draw_mode(DrawList* t_draw_list, ViewMode t_mode) const -> void;
	auto draw_mode_morph(DrawList* t_draw_list) const -> void;
	auto draw_carousel_mode(DrawList* t_draw_list) const -> void;
	auto draw_carousel_edges(DrawList* t_draw_list, u8 t_alpha) const -> void;
	[[nodiscard]] auto faded_rect(u32 t_game) const -> Rect;
	auto fade_cards(DrawList* t_draw_list, Rect t_band, Color t_top_left, Color t_top_right, Color t_bottom_left, Color t_bottom_right) const -> void;
	auto draw_grid_mode(DrawList* t_draw_list) const -> void;
	auto draw_library_mode(DrawList* t_draw_list) const -> void;
	auto draw_wrap_scroll(DrawList* t_draw_list, u8 t_alpha) const -> void;
	auto draw_raised(DrawList* t_draw_list, ViewMode t_mode) const -> void;
	auto draw_reorder_hint(DrawList* t_draw_list) const -> void;
	auto draw_switcher(DrawList* t_draw_list) const -> void;
	auto draw_switcher_rows(DrawList* t_draw_list, Rect t_panel, u8 t_alpha) const -> void;
	auto draw_size_slider(DrawList* t_draw_list, Rect t_panel, u8 t_alpha) const -> void;

	const Library*  m_library;
	const Settings* m_settings;
	const Fonts*    m_fonts;
	const Assets*   m_assets;
	CommandQueue*   m_commands;

	Rect m_bounds{};

	float     m_scroll        = 0.0f;
	float     m_target_scroll = 0.0f;
	Draggable m_card_drag;
	float     m_drag_start_scroll = 0.0f;

	i32        m_zoom_stop       = 0;
	float      m_zoom_percent    = 0.0f;
	ViewMode   m_mode            = ViewMode::CAROUSEL;
	ViewMode   m_previous_mode   = ViewMode::CAROUSEL;
	float      m_mode_transition = 0.0f;
	Rect       m_morph_from_art[K_MAX_GAMES]{};
	Rect       m_morph_from_frame[K_MAX_GAMES]{};
	Vec2       m_last_centers[K_MAX_GAMES]{};
	u32        m_grid_columns = 0;
	float      m_shelf        = 0.0f;
	float      m_spread       = 0.0f;
	Scrollable m_wrap_scroll;

	float     m_switcher_hold_seconds = 0.0f;
	float     m_switcher_shown        = 0.0f;
	float     m_size_slider_fold      = 0.0f;
	Draggable m_switcher_drag;
	bool      m_switcher_owns_pointer = false;

	float m_card_hover[K_MAX_GAMES]{};
	i32   m_focused_game         = 0;
	bool  m_keyboard_focus_shown = false;
	i32   m_detached_game        = -1;

	u8        m_order[K_MAX_GAMES]{};
	u8        m_slot_of[K_MAX_GAMES]{};
	Vec2      m_card_offset[K_MAX_GAMES]{};
	Vec2      m_card_velocity[K_MAX_GAMES]{};
	Reorder   m_reorder;
	LongPress m_long_press;
	bool      m_release_ignored = false;
	float     m_overview        = 0.0f;
	float     m_lift            = 0.0f;
	float     m_reorder_hint    = 0.0f;

	LibraryView m_library_view;
};
