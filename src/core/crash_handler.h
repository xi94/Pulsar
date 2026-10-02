#pragma once

#include <string>

auto install_crash_handler() -> void;

[[nodiscard]] auto write_diagnostic_dump(const wchar_t* t_tag) -> std::wstring;
