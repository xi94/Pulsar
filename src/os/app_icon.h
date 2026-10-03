#pragma once

#include <vector>

#include "core/types.h"

namespace os {

[[nodiscard]] auto app_icon_pixels(u32 t_size) -> std::vector<u8>;

}
