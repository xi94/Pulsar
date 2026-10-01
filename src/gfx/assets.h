#pragma once

#include <memory>
#include <span>
#include <thread>
#include <vector>

#include "gfx/renderer.h"

enum class Asset : u8 {
	IconArrowBack,
	IconClose,
	IconMinimize,
	IconSettings,
	IconMenu,
	IconAdd,
	IconEdit,
	IconFolderOpen,
	IconGrid,
	IconList,
	IconCarousel,
	IconShelf,
	IconIcons,
	IconListArrow,
	IconEyeVisible,
	IconEyeHidden,
	IconFavorite,
	IconUpdate,
	IconReset,
	IconAccount,
	IconImage,
	IconUsername,
	IconLock,
	IconFolder,
	IconFile,
	IconDownload,
	IconCheck,
	IconApp,
	IconLeagueOfLegends,
	IconValorant,
	IconTwoXko,
	IconRuneterra,
	IconTeamfightTactics,
	BannerLeagueOfLegends,
	BannerValorant,
	BannerTwoXko,
	BannerRuneterra,
	BannerTeamfightTactics,
	Count,
};

constexpr usize asset_count = static_cast<usize>(Asset::Count);

class Assets {
  public:
	Assets() = default;
	~Assets();

	Assets(const Assets &) = delete;
	Assets &operator=(const Assets &) = delete;

	void begin_decode();
	bool finish_upload(Renderer *t_renderer);

	const Texture *get(Asset t_asset) const
	{
		return m_textures[static_cast<usize>(t_asset)].get();
	}

	static std::span<const u8> encoded_bytes(Asset t_asset);
	static std::unique_ptr<Texture> create_texture(Renderer *t_renderer, const u8 *t_rgba_pixels, u32 t_width, u32 t_height);

  private:
	struct MipLevel {
		u32 width;
		u32 height;
		usize offset;
	};

	struct DecodedImage {
		std::vector<u8> pixels;
		std::vector<MipLevel> levels;
	};

	static DecodedImage with_mipmaps(const u8 *t_rgba_pixels, u32 t_width, u32 t_height);
	static std::unique_ptr<Texture> upload(Renderer *t_renderer, const DecodedImage &t_image);

	std::unique_ptr<Texture> m_textures[asset_count];
	std::thread m_decoder;
	std::vector<DecodedImage> m_decoded;
};
