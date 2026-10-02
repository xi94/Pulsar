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

constexpr usize K_ASSET_COUNT = static_cast<usize>(Asset::Count);

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
