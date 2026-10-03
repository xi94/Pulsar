#pragma once

#include <string>

namespace os {

auto install_crash_handler() -> void;
[[nodiscard]] auto write_diagnostic_dump(const char* t_tag) -> std::string;

}
