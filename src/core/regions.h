#pragma once

#include <algorithm>
#include <array>
#include <iterator>
#include <ranges>
#include <string_view>

#include "core/types.h"

struct RegionOption {
	std::string_view code;
	std::string_view label;
};

constexpr RegionOption K_REGION_OPTIONS[]{
	{"", "None"},
	{"NA", "North America (NA)"},
	{"EUW", "Europe West (EUW)"},
	{"EUNE", "Europe Nordic & East (EUNE)"},
	{"KR", "Korea (KR)"},
	{"JP", "Japan (JP)"},
	{"BR", "Brazil (BR)"},
	{"LAN", "Latin America North (LAN)"},
	{"LAS", "Latin America South (LAS)"},
	{"OCE", "Oceania (OCE)"},
	{"TR", "Turkiye (TR)"},
	{"RU", "Russia (RU)"},
	{"ME", "Middle East (ME)"},
	{"PH", "Philippines (PH)"},
	{"SG", "Singapore (SG)"},
	{"TH", "Thailand (TH)"},
	{"TW", "Taiwan (TW)"},
	{"VN", "Vietnam (VN)"},
};

constexpr usize K_REGION_COUNT = std::size(K_REGION_OPTIONS);

constexpr auto K_REGION_LABELS = [] {
	std::array<std::string_view, K_REGION_COUNT> labels{};
	std::ranges::transform(K_REGION_OPTIONS, labels.begin(), &RegionOption::label);

	return labels;
}();

[[nodiscard]] constexpr auto region_index(std::string_view t_code) -> u32
{
	for (const u32 i : std::views::iota(0u, K_REGION_COUNT)) {
		if (K_REGION_OPTIONS[i].code == t_code) return i;
	}

	return 0;
}
