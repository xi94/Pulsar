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
	i32           game = -1;
	i32           row  = -1;
};

constexpr u32 K_TRAY_MAX_GAMES    = 16;
constexpr u32 K_TRAY_MAX_ACCOUNTS = 256;

struct TrayGame {
	char title[64];
	i32  game;
	u32  first_account;
	u32  account_count;
};

struct TrayAccount {
	char label[64];
	i32  game;
	i32  row;
};

struct TrayColors {
	Color background;
	Color hover;
	Color text;
	Color text_disabled;
	Color separator;

	auto operator==(const TrayColors&) const -> bool = default;
};

struct TrayMenu {
	TrayGame    games[K_TRAY_MAX_GAMES];
	u32         game_count;
	TrayAccount accounts[K_TRAY_MAX_ACCOUNTS];
	u32         account_count;
	bool        locked;
};

class Tray {
  public:
	Tray() = default;
	~Tray();

	Tray(const Tray&)                    = delete;
	auto operator=(const Tray&) -> Tray& = delete;

	auto create(const wchar_t* t_tooltip) -> bool;

	auto on_menu_open(std::function<void(TrayMenu*)> t_fill_menu) -> void;
	auto set_game_icon(u32 t_game, std::span<const u8> t_png) -> void;
	auto set_colors(const TrayColors& t_colors) -> void;
	auto set_locked(bool t_locked) -> void;

	[[nodiscard]] auto is_icon_visible() const -> bool
	{
		return m_icon_added;
	}

	[[nodiscard]] auto take_event() -> TrayEvent;

  private:
	struct MenuRow {
		wchar_t label[96];
		HBITMAP icon;
		bool    indented;
		bool    submenu;
		bool    separator;
		bool    disabled;
	};

	static constexpr u32 K_MAX_MENU_ROWS = K_TRAY_MAX_ACCOUNTS + K_TRAY_MAX_GAMES + 8;

	static auto CALLBACK window_proc(HWND t_window, UINT t_message, WPARAM t_wparam, LPARAM t_lparam) -> LRESULT;

	[[nodiscard]] auto handle_message(UINT t_message, WPARAM t_wparam, LPARAM t_lparam) -> LRESULT;
	auto handle_command(UINT t_command) -> void;

	auto add_icon() -> bool;
	auto remove_icon() -> void;
	auto update_icon() -> void;
	[[nodiscard]] auto shown_icon() const -> HICON;
	auto fill_tooltip(wchar_t (&t_tooltip)[128]) const -> void;
	auto rebuild_brushes() -> void;

	auto show_menu() -> void;
	[[nodiscard]] auto build_menu() -> HMENU;
	[[nodiscard]] auto build_game_submenu(const TrayGame& t_game) -> HMENU;
	[[nodiscard]] auto game_icon(i32 t_game) -> HBITMAP;

	auto append_row(HMENU t_menu, UINT t_flags, UINT_PTR t_id, const MenuRow& t_row) -> void;

	auto measure_row(MEASUREITEMSTRUCT* t_measure) const -> void;
	auto draw_row(const DRAWITEMSTRUCT* t_draw) const -> void;

	HWND    m_window       = nullptr;
	HICON   m_icon         = nullptr;
	HICON   m_locked_icon  = nullptr;
	bool    m_locked       = false;
	bool    m_icon_added   = false;
	u32     m_add_attempts = 0;
	wchar_t m_tooltip[128]{};

	HFONT      m_menu_font        = nullptr;
	bool       m_owns_menu_font   = false;
	HBRUSH     m_background_brush = nullptr;
	HBRUSH     m_hover_brush      = nullptr;
	TrayColors m_colors{
		.background    = {32, 32, 36, 255},
		.hover         = {68, 60, 124, 255},
		.text          = {232, 232, 236, 255},
		.text_disabled = {108, 108, 116, 255},
		.separator     = {50, 50, 56, 255},
	};

	std::span<const u8> m_game_icon_sources[K_TRAY_MAX_GAMES]{};
	HBITMAP             m_game_icons[K_TRAY_MAX_GAMES]{};

	TrayEvent m_pending_event{};

	std::function<void(TrayMenu*)> m_fill_menu;
	TrayMenu                       m_menu{};
	MenuRow                        m_rows[K_MAX_MENU_ROWS]{};
	u32                            m_row_count = 0;
};
