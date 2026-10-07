#pragma once

#include <string_view>

#include "core/library.h"
#include "gfx/assets.h"

struct GameInfo {
	std::string_view title;
	std::string_view short_title;
	std::string_view launch_product;
	Asset            banner;
	Asset            icon;
	Color            accent;
};

constexpr GameInfo K_GAME_INFOS[]{
	{"League of Legends", "LoL", "league_of_legends", Asset::BANNER_LEAGUE_OF_LEGENDS, Asset::ICON_LEAGUE_OF_LEGENDS, {210, 175, 55, 255}},
	{"Teamfight Tactics", "TFT", "league_of_legends", Asset::BANNER_TEAMFIGHT_TACTICS, Asset::ICON_TEAMFIGHT_TACTICS, {70, 140, 190, 255}},
	{"Valorant", "Valorant", "valorant", Asset::BANNER_VALORANT, Asset::ICON_VALORANT, {210, 55, 60, 255}},
	{"2XKO", "2XKO", "lion", Asset::BANNER_TWO_XKO, Asset::ICON_TWO_XKO, {45, 205, 210, 255}},
	{"Legends of Runeterra", "LoR", "bacon", Asset::BANNER_RUNETERRA, Asset::ICON_RUNETERRA, {140, 90, 200, 255}},
};

static_assert(std::size(K_GAME_INFOS) <= K_MAX_GAMES);
