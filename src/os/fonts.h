#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace os {

struct SystemFont {
	std::string name;
	std::string file;
};

[[nodiscard]] auto system_fonts() -> std::vector<SystemFont>;
[[nodiscard]] auto system_font_path(std::string_view t_file) -> std::string;
[[nodiscard]] auto fallback_font_paths() -> std::vector<std::string>;

}
