#pragma once

#include <memory>
#include <span>
#include <thread>
#include <vector>

#include "gfx/renderer.h"

enum class Asset : u8 {
	icon_arrow_back,
	icon_close,
	icon_minimize,
	icon_settings,
	icon_menu,
	icon_add,
	icon_edit,
	icon_folder,
	icon_grid,
	icon_list,
	icon_carousel,
	icon_list_arrow,
	icon_eye_visible,
	icon_eye_hidden,
	icon_update,
	icon_reset,
	icon_app,
	icon_league_of_legends,
	icon_valorant,
	icon_two_xko,
	icon_runeterra,
	icon_teamfight_tactics,
	banner_league_of_legends,
	banner_valorant,
	banner_two_xko,
	banner_runeterra,
	banner_teamfight_tactics,
	count,
};

constexpr usize asset_count = static_cast<usize>(Asset::count);

class Assets {
  public:
	Assets() = default;
	~Assets();

	Assets(const Assets &) = delete;
	Assets &operator=(const Assets &) = delete;

	void begin_decode();
	bool finish_upload(Renderer &t_renderer);

	const Texture *get(Asset t_asset) const
	{
		return m_textures[static_cast<usize>(t_asset)].get();
	}

	static std::span<const u8> encoded_bytes(Asset t_asset);

  private:
	struct DecodedImage {
		u8 *pixels = nullptr;
		u32 width = 0;
		u32 height = 0;
	};

	std::unique_ptr<Texture> m_textures[asset_count];
	std::thread m_decoder;
	std::vector<DecodedImage> m_decoded;
};
