#pragma once

#include <string_view>

#include "core/crypto.h"
#include "core/types.h"

constexpr u32 max_game_order = 16;
constexpr u32 max_game_title = 48;

enum class ThemeKind : u8 {
	Dark,
	Light,
	Forest,
	Ocean,
	Pink,
	Blossom,
	Gruvbox,
	CatppuccinMocha,
	CatppuccinLatte,
	Nord,
	Dracula,
	TokyoNight,
	RosePine,
};

struct OptionLabel {
	std::string_view id;
	std::string_view name;
};

constexpr OptionLabel theme_labels[]{
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

enum class BackgroundStyle : u8 {
	None,
	Dots,
	Grid,
	Lines,
	Polka,
	Topography,
	Starfield,
	Scanlines,
	Crosshatch,
};

constexpr OptionLabel background_labels[]{
	{"none", "None"},			{"dots", "Dots"},			{"grid", "Grid"},
	{"lines", "Lines"},			{"polka", "Polka"},			{"topography", "Topography"},
	{"starfield", "Starfield"}, {"scanlines", "Scanlines"}, {"crosshatch", "Crosshatch"},
};

constexpr u32 background_count = static_cast<u32>(std::size(background_labels));

constexpr float animation_speed_min = 0.25f;
constexpr float animation_speed_max = 3.0f;
constexpr float corner_roundness_min = 0.0f;
constexpr float corner_roundness_max = 1.5f;
constexpr float font_size_min = 10.0f;
constexpr float font_size_max = 24.0f;
constexpr float secondary_font_size_min = 8.0f;
constexpr float secondary_font_size_max = 18.0f;

struct Settings {
	u32 window_width = 1042;
	u32 window_height = 675;

	bool animations_enabled = true;
	float animation_speed = 1.0f;
	float corner_roundness = 1.0f;
	BackgroundStyle background_style = BackgroundStyle::None;
	bool background_light = true;
	bool background_grain = true;
	float background_light_intensity = 0.5f;
	float background_grain_intensity = 0.5f;
	bool snow = false;
	float background_intensity = 0.5f;

	float font_size = 13.0f;
	float secondary_font_size = 12.0f;
	// Braces, not `= "..."`: MSVC zeroes that form whenever a Settings is constant-initialized.
	char font_name[260]{"segoeui.ttf"};
	ThemeKind theme = ThemeKind::Dark;
	Color accent{203, 166, 247, 255};

	bool show_notifications = true;
	bool hide_from_capture = true;
	bool block_overlay_injection = true;
	bool close_to_tray = false;
	u32 auto_lock_minutes = 0;

	i32 zoom_stop = 0;
	i32 selected_game = 0;
	char game_order[max_game_order][max_game_title]{};
	u32 game_order_count = 0;

	char last_run_version[32]{};
	char release_notes_version[32]{};
	char release_notes[1024]{};

	bool master_password_enabled = false;
	MasterKeyParams master_key;
};
