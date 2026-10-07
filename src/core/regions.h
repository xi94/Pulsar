#pragma once

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
