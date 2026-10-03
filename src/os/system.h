#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "core/types.h"

namespace os {

struct LocalTime {
	u32 hour;
	u32 minute;
	u32 second;
	u32 millisecond;
};

[[nodiscard]] auto local_time() -> LocalTime;
[[nodiscard]] auto current_thread_id() -> u64;
auto write_to_debugger(const char* t_text) -> void;
[[nodiscard]] auto environment_variable(const char* t_name) -> std::optional<std::string>;

[[nodiscard]] auto find_ignoring_case(std::string_view t_text, std::string_view t_query) -> usize;

}
