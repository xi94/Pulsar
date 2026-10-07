#pragma once

#include <functional>
#include <memory>
#include <span>
#include <string_view>

#include "core/types.h"

namespace os {

enum class TrayEventType : u8 {
	NONE,
	SHOW_WINDOW,
	EXIT,
	QUICK_LOGIN,
};

struct TrayEvent {
	TrayEventType type = TrayEventType::NONE;
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
	Tray();
	~Tray();

	Tray(const Tray&)                    = delete;
	auto operator=(const Tray&) -> Tray& = delete;

	auto create(std::string_view t_tooltip) -> bool;

	auto on_menu_open(std::function<void(TrayMenu*)> t_fill_menu) -> void;
	auto set_game_icon(u32 t_game, std::span<const u8> t_png) -> void;
	auto set_colors(const TrayColors& t_colors) -> void;
	auto set_locked(bool t_locked) -> void;

	[[nodiscard]] auto is_icon_visible() const -> bool;
	[[nodiscard]] auto take_event() -> TrayEvent;

  private:
	struct Native;

	std::unique_ptr<Native> m_native;
};

}
