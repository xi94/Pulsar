#pragma once

#include <functional>
#include <span>

#include <Windows.h>

#include "core/types.h"

enum class TrayEventType : u8 {
	None,
	ShowWindow,
	Exit,
	QuickLogin,
};

struct TrayEvent {
	TrayEventType type = TrayEventType::None;
	i32 game = -1;
	i32 row = -1;
};

constexpr u32 tray_max_games = 16;
constexpr u32 tray_max_accounts = 256;

struct TrayGame {
	char title[64];
	i32 game;
	u32 first_account;
	u32 account_count;
};

struct TrayAccount {
	char label[64];
	i32 game;
	i32 row;
};

struct TrayColors {
	Color background;
	Color hover;
	Color text;
	Color text_disabled;
	Color separator;

	bool operator==(const TrayColors &) const = default;
};

struct TrayMenu {
	TrayGame games[tray_max_games];
	u32 game_count;
	TrayAccount accounts[tray_max_accounts];
	u32 account_count;
	bool locked;
};

class Tray {
  public:
	Tray() = default;
	~Tray();

	Tray(const Tray &) = delete;
	Tray &operator=(const Tray &) = delete;

	bool create(const wchar_t *t_tooltip);

	void on_menu_open(std::function<void(TrayMenu &)> t_fill_menu);
	void set_game_icon(u32 t_game, std::span<const u8> t_png);
	void set_colors(const TrayColors &t_colors);
	void set_locked(bool t_locked);

	bool is_icon_visible() const
	{
		return m_icon_added;
	}

	TrayEvent take_event();

  private:
	struct MenuRow {
		wchar_t label[96];
		HBITMAP icon;
		bool indented;
		bool submenu;
		bool separator;
		bool disabled;
	};

	static constexpr u32 max_menu_rows = tray_max_accounts + tray_max_games + 8;

	static LRESULT CALLBACK window_proc(HWND t_window, UINT t_message, WPARAM t_wparam, LPARAM t_lparam);

	LRESULT handle_message(UINT t_message, WPARAM t_wparam, LPARAM t_lparam);
	void handle_command(UINT t_command);

	bool add_icon();
	void remove_icon();
	void update_icon();
	HICON shown_icon() const;
	void fill_tooltip(wchar_t (&t_tooltip)[128]) const;
	void rebuild_brushes();

	void show_menu();
	HMENU build_menu();
	HMENU build_game_submenu(const TrayGame &t_game);
	HBITMAP game_icon(i32 t_game);

	void append_row(HMENU t_menu, UINT t_flags, UINT_PTR t_id, const MenuRow &t_row);

	void measure_row(MEASUREITEMSTRUCT &t_measure) const;
	void draw_row(const DRAWITEMSTRUCT &t_draw) const;

	HWND m_window = nullptr;
	HICON m_icon = nullptr;
	HICON m_locked_icon = nullptr;
	bool m_locked = false;
	bool m_icon_added = false;
	u32 m_add_attempts = 0;
	wchar_t m_tooltip[128]{};

	HFONT m_menu_font = nullptr;
	bool m_owns_menu_font = false;
	HBRUSH m_background_brush = nullptr;
	HBRUSH m_hover_brush = nullptr;
	TrayColors m_colors{
		.background = {32, 32, 36, 255},
		.hover = {68, 60, 124, 255},
		.text = {232, 232, 236, 255},
		.text_disabled = {108, 108, 116, 255},
		.separator = {50, 50, 56, 255},
	};

	std::span<const u8> m_game_icon_sources[tray_max_games]{};
	HBITMAP m_game_icons[tray_max_games]{};

	TrayEvent m_pending_event{};

	std::function<void(TrayMenu &)> m_fill_menu;
	TrayMenu m_menu{};
	MenuRow m_rows[max_menu_rows]{};
	u32 m_row_count = 0;
};
