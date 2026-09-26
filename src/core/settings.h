#pragma once

#include <string_view>

#include "core/master_key.h"
#include "core/types.h"

enum class ThemeKind : u8 {
	dark,
	light,
	forest,
	ocean,
	pink,
	blossom,
	gruvbox,
	catppuccin_mocha,
	catppuccin_latte,
	nord,
	dracula,
	tokyo_night,
	rose_pine,
};

struct ThemeLabel {
	std::string_view id;
	std::string_view name;
};

constexpr ThemeLabel theme_labels[]{
	{"dark", "Dark"},
	{"light", "Light"},
	{"forest", "Forest"},
	{"ocean", "Ocean"},
	{"pink", "Pink"},
	{"blossom", "Blossom"},
	{"gruvbox", "Gruvbox"},
	{"catppuccin_mocha", "Catppuccin Mocha"},
	{"catppuccin_latte", "Catppuccin Latte"},
	{"nord", "Nord"},
	{"dracula", "Dracula"},
	{"tokyo_night", "Tokyo Night"},
	{"rose_pine", "Rose Pine"},
};

constexpr u32 theme_count = static_cast<u32>(std::size(theme_labels));

struct Settings {
	u32 window_width = 1042;
	u32 window_height = 675;

	bool animations_enabled = true;
	float animation_speed = 1.0f;
	float corner_roundness = 1.0f;

	float font_size = 14.0f;
	float secondary_font_size = 12.0f;
	// Braces, not `= "..."`: MSVC zeroes that form whenever a Settings is constant-initialized.
	char font_name[260]{"segoeui.ttf"};
	ThemeKind theme = ThemeKind::dark;
	Color accent{108, 90, 220, 255};

	bool show_notifications = true;
	bool hide_accounts_from_capture = true;
	bool block_overlay_injection = true;
	bool close_to_tray = false;

	i32 zoom_stop = 0;
	i32 selected_game = 0;

	char last_run_version[32]{};

	bool master_password_enabled = false;
	MasterKeyParams master_key;
};
