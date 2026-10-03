#pragma once

#include <compare>
#include <optional>
#include <string_view>

#include "core/types.h"

struct Version {
	u32 major = 0;
	u32 minor = 0;
	u32 patch = 0;

	auto operator<=>(const Version&) const = default;
};

[[nodiscard]] inline auto parse_version(std::string_view t_text) -> std::optional<Version>
{
	const auto parse_component = [t_text](usize* t_index, u32* t_out_value) {
		const usize start = *t_index;
		*t_out_value      = 0;

		while (*t_index < t_text.size() && t_text[*t_index] >= '0' && t_text[*t_index] <= '9') {
			*t_out_value = *t_out_value * 10 + static_cast<u32>(t_text[*t_index] - '0');
			*t_index += 1;
		}

		if (*t_index < t_text.size() && t_text[*t_index] == '.') {
			*t_index += 1;
		}

		return *t_index > start;
	};

	Version version{};
	usize   index = 0;
	if (!parse_component(&index, &version.major)) return std::nullopt;

	parse_component(&index, &version.minor);
	parse_component(&index, &version.patch);

	return version;
}
