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
	LOCK,
	EXIT,
	QUICK_LOGIN,
	RETRY_LOGIN,
	SHOW_UPDATES,
};

// A quick login names its account by its game and its row among that game's accounts. Other events carry no account.
struct TrayEvent {
	TrayEventType type = TrayEventType::NONE;
	u32           game = 0;
	u32           row  = 0;
};

constexpr u32 K_TRAY_MAX_GAMES    = 16;
constexpr u32 K_TRAY_MAX_ACCOUNTS = 256;

struct TrayGame {
	char title[64];
	u32  game;
	u32  first_account;
	u32  account_count;
};

struct TrayAccount {
	char label[64];
	char region[8];
	u32  game;
	u32  row;
};

struct TrayColors {
	Color background;
	Color border;
	Color separator;
	Color scroll_thumb;
	Color shadow;
	Color text;
	Color text_dim;
	Color text_disabled;
	Color accent;
	Color accent_ink;
	Color success;
	Color error;

	auto operator==(const TrayColors&) const -> bool = default;
};

enum class TrayUpdate : u8 {
	NONE,
	AVAILABLE,
	INSTALLING,
};

enum class TrayLoginState : u8 {
	RUNNING,
	SUCCEEDED,
	FAILED,
};

struct TrayLogin {
	TrayLoginState state = TrayLoginState::RUNNING;
	char           account[64]{};
	char           status[160]{};
	u32            step       = 0;
	u32            step_count = 0;
	float          progress   = 0.0f;
};

struct TrayMenu {
	TrayGame    games[K_TRAY_MAX_GAMES];
	u32         game_count;
	TrayAccount accounts[K_TRAY_MAX_ACCOUNTS];
	u32         account_count;
	bool        locked;
	bool        can_lock;
	TrayUpdate  update;
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
	auto set_logo(std::span<const u8> t_png) -> void;
	auto set_colors(const TrayColors& t_colors) -> void;
	auto set_locked(bool t_locked) -> void;
	auto set_update(TrayUpdate t_update) -> void;

	// A finished login closes itself after a moment.
	auto show_login(const TrayLogin& t_login) -> void;
	auto hide_login() -> void;

	[[nodiscard]] auto is_icon_visible() const -> bool;
	[[nodiscard]] auto take_event() -> TrayEvent;

  private:
	struct Native;

	std::unique_ptr<Native> m_native;
};

}
