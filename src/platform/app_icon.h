#pragma once

#include <vector>

#include <Windows.h>

#include "core/types.h"

enum class AppIconSize : u8 {
	small_icon,
	large_icon,
};

HICON load_app_icon(AppIconSize t_size);
int app_icon_pixel_size(AppIconSize t_size);
std::vector<u8> app_icon_pixels(u32 t_size);
