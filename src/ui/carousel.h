#pragma once

#include "core/library.h"
#include "ui/commands.h"
#include "ui/draggable.h"
#include "ui/scrollable.h"
#include "ui/widget.h"

class Assets;
class Fonts;
struct Game;
struct Settings;

enum class ViewMode : u8 {
	carousel,
	grid,
	list,
};

class Carousel : public Widget {
  public:
	Carousel(const Library &t_library, const Settings &t_settings, const Fonts &t_fonts, const Assets &t_assets,
			 CommandQueue &t_commands);

	void set_bounds(Rect t_bounds)
	{
		m_bounds = t_bounds;
	}

	i32 zoom_stop() const
	{
		return m_zoom_stop;
	}

	i32 selected_game() const;

	void restore(i32 t_zoom_stop, i32 t_selected_game);

	void update(float t_delta_seconds) override;
	void draw(DrawList &t_draw_list) override;
	void draw_status_bar(DrawList &t_draw_list) const;

	bool on_pointer_down(Vec2 t_point) override;
	bool on_pointer_move(Vec2 t_point) override;
	bool on_pointer_up(Vec2 t_point) override;
	bool on_scroll(Vec2 t_point, float t_wheel_delta) override;
	bool on_key_down(u32 t_key) override;

	CursorKind cursor() const override;

  private:
	u32 game_count() const
	{
		return m_library.game_count();
	}

	float clamp_scroll(float t_offset) const;
	float wrap_content_height() const;
	ScrollGeometry wrap_scroll_geometry() const;

	float view_scale() const;
	Rect carousel_card(u32 t_game) const;
	Rect grid_card(u32 t_game) const;
	Rect list_row(u32 t_game) const;
	Rect card_in_current_mode(u32 t_game) const;
	i32 game_at(Vec2 t_point) const;

	Rect status_indicator_rect() const;
	Rect switcher_panel_rect() const;
	Rect switcher_row_rect(Rect t_panel, u32 t_row) const;
	Rect switcher_track_rect(Rect t_panel) const;
	Rect switcher_track_grab_rect(Rect t_panel) const;
	bool is_switcher_shown() const;
	bool is_mouse_over_switcher(Vec2 t_mouse) const;

	void set_zoom_stop(i32 t_stop);
	i32 focused_game() const;
	bool is_focus_shown(u32 t_game) const;
	void move_focus(i32 t_delta);
	void open_game(i32 t_game);

	bool switcher_pointer_down(Vec2 t_point);
	bool switcher_pointer_move(Vec2 t_point);
	bool switcher_pointer_up();

	void draw_card(DrawList &t_draw_list, Rect t_rect, const Game &t_game, bool t_highlighted, bool t_centered,
				   u8 t_alpha) const;
	void draw_mode(DrawList &t_draw_list, ViewMode t_mode, u8 t_alpha, float t_y_offset) const;
	void draw_carousel_mode(DrawList &t_draw_list, u8 t_alpha, float t_y_offset) const;
	void draw_grid_mode(DrawList &t_draw_list, u8 t_alpha, float t_y_offset) const;
	void draw_list_mode(DrawList &t_draw_list, u8 t_alpha, float t_y_offset) const;
	void draw_wrap_scroll(DrawList &t_draw_list, u8 t_alpha) const;
	void draw_switcher(DrawList &t_draw_list) const;
	void draw_switcher_rows(DrawList &t_draw_list, Rect t_panel, u8 t_alpha) const;
	void draw_switcher_slider(DrawList &t_draw_list, Rect t_panel, u8 t_alpha) const;

	const Library &m_library;
	const Settings &m_settings;
	const Fonts &m_fonts;
	const Assets &m_assets;
	CommandQueue &m_commands;

	Rect m_bounds{};

	float m_scroll = 0.0f;
	float m_target_scroll = 0.0f;
	Draggable m_card_drag;
	float m_drag_start_scroll = 0.0f;

	i32 m_zoom_stop = 0;
	float m_zoom_percent = 0.0f;
	ViewMode m_mode = ViewMode::carousel;
	ViewMode m_previous_mode = ViewMode::carousel;
	float m_mode_transition = 0.0f;
	Scrollable m_wrap_scroll;

	float m_switcher_hold_seconds = 0.0f;
	float m_switcher_shown = 0.0f;
	Draggable m_switcher_drag;
	bool m_switcher_owns_pointer = false;

	float m_card_hover[max_games]{};
	i32 m_focused_game = 0;
	bool m_keyboard_focus_shown = false;
};
