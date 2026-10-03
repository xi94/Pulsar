#pragma once

#include <cstdio>
#include <span>
#include <string>
#include <string_view>

#include "core/types.h"

namespace os {

[[nodiscard]] auto user_data_folder() -> std::string;
[[nodiscard]] auto write_file_durably(const std::string& t_path, std::string_view t_contents) -> bool;
auto replace_file(const std::string& t_from, const std::string& t_to) -> bool;
[[nodiscard]] auto open_log_file(const std::string& t_path) -> std::FILE*;

[[nodiscard]] auto map_file(const std::string& t_path) -> std::span<const u8>;
auto unmap_file(std::span<const u8> t_mapping) -> void;

}
