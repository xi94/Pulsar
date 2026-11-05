#pragma once

#include <string>

namespace os {

auto install_crash_handler() -> void;
auto set_crash_cleanup(void (*t_cleanup)()) -> void;
[[nodiscard]] auto write_diagnostic_dump(const char* t_tag) -> std::string;

}
