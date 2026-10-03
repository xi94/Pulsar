#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "core/types.h"

namespace os {

[[nodiscard]] auto install_update(const std::vector<u8>& t_new_build, std::string_view t_running_version, std::string* t_out_error) -> bool;
[[nodiscard]] auto handed_off_to_repaired_copy() -> bool;

}
