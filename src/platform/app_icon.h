#pragma once

#include <Windows.h>

enum class AppIconSize : u8 {
	small_icon,
	large_icon,
};

HICON load_app_icon(AppIconSize t_size);
int app_icon_pixel_size(AppIconSize t_size);
