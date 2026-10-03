#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "core/types.h"

[[nodiscard]] auto to_path(std::string_view t_utf8) -> std::filesystem::path;
[[nodiscard]] auto from_path(const std::filesystem::path& t_path) -> std::string;
[[nodiscard]] auto joined_path(std::string_view t_folder, std::string_view t_name) -> std::string;

[[nodiscard]] auto read_whole_file(const std::string& t_path, std::vector<u8>* t_out_bytes) -> bool;
[[nodiscard]] auto write_file_atomic(const std::string& t_path, std::string_view t_contents) -> bool;
[[nodiscard]] auto backup_path_for(const std::string& t_path) -> std::string;

[[nodiscard]] auto app_data_subdirectory(const char* t_subfolder) -> std::string;
