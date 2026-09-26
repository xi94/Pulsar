#pragma once

#include <string_view>

#include "core/library.h"
#include "core/login_attempt.h"
#include "ui/commands.h"
#include "ui/confirm_latch.h"
#include "ui/game_select_popup.h"
#include "ui/scrollable.h"
#include "ui/text_input.h"
#include "ui/widget.h"

class Assets;
class Fonts;
class Toasts;
class Window;
struct Settings;

enum class EditField : u8 {
	note,
	username,
	password,
	count,
};

class AccountModal : public Widget {
  public:
	AccountModal(Library &t_library, const Settings &t_settings, const Fonts &t_fonts, const Assets &t_assets,
				 const Window &t_window, Toasts &t_toasts, CommandQueue &t_commands);

	void open(i32 t_game);
	void close();
	void quick_login(i32 t_game, i32 t_row);

	bool can_quick_login(i32 t_game, i32 t_row) const;
	const Account *account_at_row(i32 t_row) const;

	void update(float t_delta_seconds) override;
	void draw(DrawList &t_draw_list) override;

	bool on_pointer_down(Vec2 t_point) override;
	bool on_pointer_move(Vec2 t_point) override;
	bool on_pointer_up(Vec2 t_point) override;
	bool on_right_click(Vec2 t_point) override;
	bool on_scroll(Vec2 t_point, float t_wheel_delta) override;
	bool on_key_down(u32 t_key) override;
	bool on_char(u32 t_character) override;

	bool is_blocking() const override
	{
		return m_open_amount > 0.01f;
	}

	CursorKind cursor() const override;

  private:
	static constexpr u32 field_count = static_cast<u32>(EditField::count);

	enum class Mode : u8 {
		account_list,
		login_progress,
		edit_account,
	};

	struct Layout {
		Rect panel;
		Rect inner;
		Rect art_column;
		Rect main_column;
		Rect footer;
	};

	struct AccountRows {
		VisibleAccounts accounts;
		float row_height;
		Rect region;
		ScrollGeometry scroll;
	};

	bool has_game() const;
	Vec2 floating_panel_size() const;
	bool is_docked() const;
	Rect panel_rect() const;
	Layout layout() const;
	AccountRows account_rows(const Layout &t_layout) const;

	Rect row_rect(const Layout &t_layout, const AccountRows &t_rows, u32 t_row) const;
	i32 row_at(const Layout &t_layout, const AccountRows &t_rows, Vec2 t_point) const;
	Rect remove_button_rect(Rect t_row) const;
	Rect confirm_delete_rect(Rect t_row) const;
	Rect edit_button_rect(Rect t_row) const;
	Rect add_button_rect(Rect t_main) const;
	Rect primary_button_rect(Rect t_footer) const;
	Rect cancel_button_rect(Rect t_primary) const;
	Rect delete_button_rect(Rect t_footer) const;
	Rect form_region(Rect t_main) const;
	ScrollGeometry form_scroll(Rect t_main) const;
	Rect field_block_rect(Rect t_main, u32 t_field) const;
	Rect field_input_rect(Rect t_main, u32 t_field) const;
	Rect field_text_rect(Rect t_main, u32 t_field) const;
	Rect reveal_button_rect(Rect t_main) const;
	Rect visibility_chip_rect(Rect t_main) const;

	TextInput &field(EditField t_field)
	{
		return m_fields[static_cast<u32>(t_field)];
	}

	const TextInput &field(EditField t_field) const
	{
		return m_fields[static_cast<u32>(t_field)];
	}

	i32 focused_field() const;
	void focus_field(i32 t_field);
	i32 field_at(Rect t_main, Vec2 t_point) const;
	bool is_reveal_hit(Rect t_main, Vec2 t_point) const;
	void reveal_field(i32 t_field);

	void start_adding();
	void start_editing(u32 t_row);
	bool can_save() const;
	void save_edit();
	void delete_edited_account();
	void remove_row(u32 t_row);
	void confirm_row_delete(u32 t_row);

	void request_login(i32 t_game, i32 t_row);
	void start_login(i32 t_game, i32 t_row);
	bool has_queued_login() const
	{
		return m_queued_login_game >= 0;
	}

	bool handle_list_key(u32 t_key);
	void handle_list_click(const Layout &t_layout, Vec2 t_point);
	void handle_edit_click(const Layout &t_layout, Vec2 t_point);

	void notify(std::string_view t_message);
	void notify_delete_armed();

	CursorKind list_cursor(const Layout &t_layout) const;
	CursorKind edit_cursor(const Layout &t_layout) const;

	void draw_chrome(DrawList &t_draw_list, const Layout &t_layout, u8 t_alpha) const;
	void draw_section_title(DrawList &t_draw_list, Rect t_main, std::string_view t_title, u8 t_alpha) const;
	void draw_account_list(DrawList &t_draw_list, const Layout &t_layout, u8 t_alpha) const;
	void draw_account_row(DrawList &t_draw_list, Rect t_main, Rect t_row, const Account &t_account, bool t_selected,
						  float t_delete_armed, u8 t_alpha) const;
	void draw_login_progress(DrawList &t_draw_list, Rect t_main, u8 t_alpha) const;
	void draw_edit_form(DrawList &t_draw_list, Rect t_main, u8 t_alpha);
	void draw_visibility_chip(DrawList &t_draw_list, Rect t_main, u8 t_alpha) const;
	void draw_footer(DrawList &t_draw_list, Rect t_footer, u8 t_alpha) const;
	void draw_edit_footer(DrawList &t_draw_list, Rect t_footer, u8 t_alpha) const;

	Library &m_library;
	const Settings &m_settings;
	const Fonts &m_fonts;
	const Assets &m_assets;
	const Window &m_window;
	Toasts &m_toasts;
	CommandQueue &m_commands;

	bool m_open = false;
	float m_open_amount = 0.0f;
	i32 m_game = -1;
	i32 m_selected_row = -1;
	Mode m_mode = Mode::account_list;
	Scrollable m_rows_scroll;
	Scrollable m_form_scroll;
	ConfirmLatch m_row_delete;
	ConfirmLatch m_form_delete;

	LoginAttempt m_login;
	i32 m_queued_login_game = -1;
	i32 m_queued_login_row = -1;
	float m_login_seconds = 0.0f;

	TextInput m_fields[field_count];
	i32 m_edited_row = -1;
	GameSelectPopup m_visible_games;
};
