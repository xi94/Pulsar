#pragma once

#include <memory>
#include <span>
#include <thread>
#include <vector>

#include "render/renderer.h"

enum class Asset : u8 {
	ICON_ARROW_BACK,
	ICON_CLOSE,
	ICON_MINIMIZE,
	ICON_SETTINGS,
	ICON_MENU,
	ICON_ADD,
	ICON_EDIT,
	ICON_FOLDER_OPEN,
	ICON_GRID,
	ICON_LIBRARY,
	ICON_CAROUSEL,
	ICON_SHELF,
	ICON_ICONS,
	ICON_LIST_ARROW,
	ICON_EYE_VISIBLE,
	ICON_EYE_HIDDEN,
	ICON_FAVORITE,
	ICON_UPDATE,
	ICON_RESET,
	ICON_ACCOUNT,
	ICON_IMAGE,
	ICON_LOCK,
	ICON_FOLDER,
	ICON_FILE,
	ICON_DOWNLOAD,
	ICON_CHECK,
	ICON_APP,
	ICON_LEAGUE_OF_LEGENDS,
	ICON_VALORANT,
	ICON_TWO_XKO,
	ICON_RUNETERRA,
	ICON_TEAMFIGHT_TACTICS,
	ICON_EDIT_BOX,
	ICON_COPY,
	ICON_KEY,
	ICON_TRASH,
	BANNER_LEAGUE_OF_LEGENDS,
	BANNER_VALORANT,
	BANNER_TWO_XKO,
	BANNER_RUNETERRA,
	BANNER_TEAMFIGHT_TACTICS,
	COUNT,
};

constexpr usize K_ASSET_COUNT = static_cast<usize>(Asset::COUNT);

class Assets {
  public:
	Assets() = default;
	~Assets();

	Assets(const Assets&)                    = delete;
	auto operator=(const Assets&) -> Assets& = delete;

	auto begin_decode() -> void;
	[[nodiscard]] auto finish_upload(Renderer* t_renderer) -> bool;

	[[nodiscard]] auto get(Asset t_asset) const -> const Texture*
	{
		return m_textures[static_cast<usize>(t_asset)].get();
	}

	[[nodiscard]] static auto encoded_bytes(Asset t_asset) -> std::span<const u8>;
	[[nodiscard]] static auto create_texture(Renderer* t_renderer, const u8* t_rgba_pixels, u32 t_width, u32 t_height) -> std::unique_ptr<Texture>;

  private:
	struct MipLevel {
		u32   width;
		u32   height;
		usize offset;
	};

	struct DecodedImage {
		std::vector<u8>       pixels;
		std::vector<MipLevel> levels;
	};

	[[nodiscard]] static auto with_mipmaps(const u8* t_rgba_pixels, u32 t_width, u32 t_height) -> DecodedImage;
	[[nodiscard]] static auto upload(Renderer* t_renderer, const DecodedImage& t_image) -> std::unique_ptr<Texture>;

	std::unique_ptr<Texture>  m_textures[K_ASSET_COUNT];
	std::thread               m_decoder;
	std::vector<DecodedImage> m_decoded;
};
