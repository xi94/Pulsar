#pragma once

#include <string>
#include <string_view>
#include <vector>

[[nodiscard]] auto read_whole_file(const char* t_path, std::vector<u8>* t_out_bytes) -> bool;
[[nodiscard]] auto write_file_atomic(const std::string& t_path, std::string_view t_contents) -> bool;
[[nodiscard]] auto backup_path_for(const std::string& t_path) -> std::string;

[[nodiscard]] auto local_app_data_folder() -> std::wstring;
[[nodiscard]] auto app_data_subdirectory(const wchar_t* t_subfolder) -> std::wstring;
