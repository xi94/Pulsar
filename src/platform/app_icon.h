#pragma once

#include <vector>

#include <Windows.h>

#include "core/types.h"

enum class AppIconSize : u8 {
	SmallIcon,
	LargeIcon,
};

[[nodiscard]] auto load_app_icon(AppIconSize t_size) -> HICON;
[[nodiscard]] auto app_icon_pixel_size(AppIconSize t_size) -> int;
[[nodiscard]] auto app_icon_pixels(u32 t_size) -> std::vector<u8>;
