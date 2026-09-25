#pragma once

#include "core/master_key.h"
#include "core/types.h"

struct Settings {
	u32 window_width = 1042;
	u32 window_height = 675;

	bool animations_enabled = true;
	float animation_speed = 1.0f;
	float corner_roundness = 1.0f;

	float font_size = 14.0f;
	float secondary_font_size = 12.0f;
	char font_name[64] = "segoeui.ttf";
	Color accent{108, 90, 220, 255};

	bool show_notifications = true;
	bool hide_accounts_from_capture = true;
	bool block_overlay_injection = true;
	bool close_to_tray = true;

	i32 zoom_stop = 0;
	i32 selected_game = 0;

	char last_run_version[32] = "";

	bool master_password_enabled = false;
	MasterKeyParams master_key;
};
