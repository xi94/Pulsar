#pragma once

#include <string>
#include <string_view>
#include <vector>

bool read_whole_file(const char *t_path, std::vector<u8> &t_out_bytes);
bool write_file_atomic(const std::string &t_path, std::string_view t_contents);
std::string backup_path_for(const std::string &t_path);

std::wstring app_data_subdirectory(const wchar_t *t_subfolder);
