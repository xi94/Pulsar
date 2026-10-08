#pragma once

#include <array>
#include <iterator>
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
	for (usize i = 0; i < K_REGION_COUNT; i += 1) {
		labels[i] = K_REGION_OPTIONS[i].label;
	}

	return labels;
}();

// Where a region code sits in K_REGION_OPTIONS. Codes that are not listed fall back to None.
[[nodiscard]] constexpr auto region_index(std::string_view t_code) -> u32
{
	for (u32 i = 0; i < K_REGION_COUNT; i += 1) {
		if (K_REGION_OPTIONS[i].code == t_code) return i;
	}

	return 0;
}
