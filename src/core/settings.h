#pragma once

#include <string_view>

#include "core/crypto.h"
#include "core/types.h"

constexpr u32 K_MAX_GAME_ORDER = 16;
constexpr u32 K_MAX_GAME_TITLE = 48;
constexpr u32 K_PATH_CAPACITY  = 1024;

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

constexpr OptionLabel K_THEME_LABELS[]{
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

constexpr u32 K_THEME_COUNT = static_cast<u32>(std::size(K_THEME_LABELS));

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

constexpr OptionLabel K_BACKGROUND_LABELS[]{
	{"none", "None"},           {"dots", "Dots"},           {"grid", "Grid"},
	{"lines", "Lines"},         {"polka", "Polka"},         {"topography", "Topography"},
	{"starfield", "Starfield"}, {"scanlines", "Scanlines"}, {"crosshatch", "Crosshatch"},
};

constexpr u32 K_BACKGROUND_COUNT = static_cast<u32>(std::size(K_BACKGROUND_LABELS));

constexpr std::string_view K_GRAPHICS_API_IDS[]{"native", "opengl"};

constexpr float K_ANIMATION_SPEED_MIN     = 0.25f;
constexpr float K_ANIMATION_SPEED_MAX     = 3.0f;
constexpr float K_CORNER_ROUNDNESS_MIN    = 0.0f;
constexpr float K_CORNER_ROUNDNESS_MAX    = 1.5f;
constexpr float K_FONT_SIZE_MIN           = 10.0f;
constexpr float K_FONT_SIZE_MAX           = 24.0f;
constexpr float K_SECONDARY_FONT_SIZE_MIN = 8.0f;
constexpr float K_SECONDARY_FONT_SIZE_MAX = 18.0f;

struct Settings {
	u32 window_width  = 1042;
	u32 window_height = 675;

	bool            animations_enabled         = true;
	float           animation_speed            = 1.0f;
	float           corner_roundness           = 1.0f;
	BackgroundStyle background_style           = BackgroundStyle::None;
	bool            background_light           = true;
	bool            background_grain           = true;
	float           background_light_intensity = 0.5f;
	float           background_grain_intensity = 0.5f;
	bool            snow                       = false;
	float           background_intensity       = 0.5f;

	float font_size           = 13.0f;
	float secondary_font_size = 12.0f;
	// Braces, not `= "..."`: MSVC zeroes that form whenever a Settings is constant-initialized.
	char      font_name[260]{PULSAR_DEFAULT_FONT_FILE};
	ThemeKind theme = ThemeKind::Dark;
	Color     accent{203, 166, 247, 255};

	bool        show_notifications      = true;
	bool        hide_from_capture       = true;
	bool        block_overlay_injection = true;
	bool        close_to_tray           = false;
	GraphicsApi renderer                = GraphicsApi::Native;
	u32         auto_lock_minutes       = 0;
	char        riot_client_path[K_PATH_CAPACITY]{};

	i32  zoom_stop     = 0;
	i32  selected_game = 0;
	char game_order[K_MAX_GAME_ORDER][K_MAX_GAME_TITLE]{};
	u32  game_order_count = 0;

	char last_run_version[32]{};
	char release_notes_version[32]{};
	char release_notes[1024]{};

	bool            master_password_enabled = false;
	MasterKeyParams master_key;
};
