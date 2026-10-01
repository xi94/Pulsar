#pragma once

#include <string_view>

#include "core/library.h"
#include "gfx/assets.h"

struct GameInfo {
	std::string_view title;
	std::string_view short_title;
	std::string_view launch_product;
	Asset banner;
	Asset icon;
	Color accent;
};

constexpr GameInfo game_infos[]{
	{"League of Legends",
	 "LoL",
	 "league_of_legends",
	 Asset::BannerLeagueOfLegends,
	 Asset::IconLeagueOfLegends,
	 {210, 175, 55, 255}},
	{"Teamfight Tactics",
	 "TFT",
	 "league_of_legends",
	 Asset::BannerTeamfightTactics,
	 Asset::IconTeamfightTactics,
	 {70, 140, 190, 255}},
	{"Valorant", "Valorant", "valorant", Asset::BannerValorant, Asset::IconValorant, {210, 55, 60, 255}},
	{"2XKO", "2XKO", "lion", Asset::BannerTwoXko, Asset::IconTwoXko, {45, 205, 210, 255}},
	{"Legends of Runeterra", "LoR", "bacon", Asset::BannerRuneterra, Asset::IconRuneterra, {140, 90, 200, 255}},
};

static_assert(std::size(game_infos) <= max_games);
