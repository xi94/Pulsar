#include "gfx/assets.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <print>
#include <ranges>

#include "embeds/banners/2XKO.hpp"
#include "embeds/banners/LeagueOfLegends.hpp"
#include "embeds/banners/Runeterra.hpp"
#include "embeds/banners/TeamfightTactics.hpp"
#include "embeds/banners/Valorant.hpp"
#include "embeds/icons/2XKOIcon.hpp"
#include "embeds/icons/AccountIcon.hpp"
#include "embeds/icons/AddIcon.hpp"
#include "embeds/icons/AppMark.hpp"
#include "embeds/icons/ArrowBack.hpp"
#include "embeds/icons/CarouselIcon.hpp"
#include "embeds/icons/CheckedIcon.hpp"
#include "embeds/icons/Close.hpp"
#include "embeds/icons/DownloadIcon.hpp"
#include "embeds/icons/EditIcon.hpp"
#include "embeds/icons/EyeHiddenIcon.hpp"
#include "embeds/icons/EyeVisible.hpp"
#include "embeds/icons/FavoriteIcon.hpp"
#include "embeds/icons/FileIcon.hpp"
#include "embeds/icons/FolderBlankIcon.hpp"
#include "embeds/icons/FolderOpenIcon.hpp"
#include "embeds/icons/GridIcon.hpp"
#include "embeds/icons/IconsIcon.hpp"
#include "embeds/icons/ImageIcon.hpp"
#include "embeds/icons/LeagueIcon.hpp"
#include "embeds/icons/ListArrow.hpp"
#include "embeds/icons/ListIcon.hpp"
#include "embeds/icons/LockIcon.hpp"
#include "embeds/icons/MenuIcon.hpp"
#include "embeds/icons/Minimize.hpp"
#include "embeds/icons/ResetIcon.hpp"
#include "embeds/icons/RuneterraIcon.hpp"
#include "embeds/icons/Settings.hpp"
#include "embeds/icons/ShelfIcon.hpp"
#include "embeds/icons/TFTIcon.hpp"
#include "embeds/icons/UpdateIcon.hpp"
#include "embeds/icons/UsernameIcon.hpp"
#include "embeds/icons/ValorantIcon.hpp"

#include "stb/stb_image.h"

namespace {
using namespace pulsar::embed;

struct EncodedAsset {
	Asset asset;
	std::span<const u8> bytes;
	const char *name;
};

void shrink_by_half(const u8 *t_source, u32 t_source_width, u32 t_source_height, u8 *t_target, u32 t_target_width, u32 t_target_height)
{
	for (u32 y = 0; y < t_target_height; y += 1) {
		const u32 rows[2]{std::min(y * 2, t_source_height - 1), std::min(y * 2 + 1, t_source_height - 1)};

		for (u32 x = 0; x < t_target_width; x += 1) {
			const u32 columns[2]{std::min(x * 2, t_source_width - 1), std::min(x * 2 + 1, t_source_width - 1)};

			u32 alpha_total = 0;
			u32 color_total[3]{};

			for (const u32 row : rows) {
				for (const u32 column : columns) {
					const u8 *texel = t_source + (static_cast<usize>(row) * t_source_width + column) * 4;
					alpha_total += texel[3];

					for (u32 channel = 0; channel < 3; channel += 1) {
						color_total[channel] += texel[channel] * texel[3];
					}
				}
			}

			u8 *target = t_target + (static_cast<usize>(y) * t_target_width + x) * 4;
			for (u32 channel = 0; channel < 3; channel += 1) {
				target[channel] = alpha_total > 0 ? static_cast<u8>(color_total[channel] / alpha_total) : 0;
			}
			target[3] = static_cast<u8>((alpha_total + 2) / 4);
		}
	}
}

constexpr EncodedAsset encoded_assets[]{
	{Asset::IconArrowBack, icon::arrow_back_icon, "ArrowBack"},
	{Asset::IconClose, icon::close_icon, "Close"},
	{Asset::IconMinimize, icon::minimize_icon, "Minimize"},
	{Asset::IconSettings, icon::settings_icon, "Settings"},
	{Asset::IconMenu, icon::menu_icon, "MenuIcon"},
	{Asset::IconAdd, icon::add_icon, "AddIcon"},
	{Asset::IconEdit, icon::edit_icon, "EditIcon"},
	{Asset::IconFolderOpen, icon::folder_open_icon, "FolderOpenIcon"},
	{Asset::IconGrid, icon::grid_icon, "GridIcon"},
	{Asset::IconList, icon::list_icon, "ListIcon"},
	{Asset::IconCarousel, icon::carousel_icon, "CarouselIcon"},
	{Asset::IconShelf, icon::shelf_icon, "ShelfIcon"},
	{Asset::IconIcons, icon::icons_icon, "IconsIcon"},
	{Asset::IconListArrow, icon::list_arrow, "ListArrow"},
	{Asset::IconEyeVisible, icon::eye_visible_icon, "EyeVisible"},
	{Asset::IconEyeHidden, icon::eye_hidden_icon, "EyeHiddenIcon"},
	{Asset::IconFavorite, icon::favorite_icon, "FavoriteIcon"},
	{Asset::IconUpdate, icon::update_icon, "UpdateIcon"},
	{Asset::IconReset, icon::reset_icon, "ResetIcon"},
	{Asset::IconAccount, icon::account_icon, "AccountIcon"},
	{Asset::IconImage, icon::image_icon, "ImageIcon"},
	{Asset::IconUsername, icon::username_icon, "UsernameIcon"},
	{Asset::IconLock, icon::lock_icon, "LockIcon"},
	{Asset::IconFolder, icon::folder_icon, "FolderBlankIcon"},
	{Asset::IconFile, icon::file_icon, "FileIcon"},
	{Asset::IconDownload, icon::download_icon, "DownloadIcon"},
	{Asset::IconCheck, icon::check_icon, "CheckedIcon"},
	{Asset::IconApp, icon::app_mark, "AppMark"},
	{Asset::IconLeagueOfLegends, icon::league_of_legends_icon, "LeagueIcon"},
	{Asset::IconValorant, icon::valorant_icon, "ValorantIcon"},
	{Asset::IconTwoXko, icon::two_xko_icon, "2XKOIcon"},
	{Asset::IconRuneterra, icon::runeterra_icon, "RuneterraIcon"},
	{Asset::IconTeamfightTactics, icon::teamfight_tactics_icon, "TFTIcon"},
	{Asset::BannerLeagueOfLegends, banner::league_of_legends, "LeagueOfLegends"},
	{Asset::BannerValorant, banner::valorant, "Valorant"},
	{Asset::BannerTwoXko, banner::two_xko, "2XKO"},
	{Asset::BannerRuneterra, banner::runeterra, "Runeterra"},
	{Asset::BannerTeamfightTactics, banner::teamfight_tactics, "TeamfightTactics"},
};

static_assert(std::size(encoded_assets) == asset_count);
static_assert(std::ranges::all_of(std::views::iota(usize{0}, asset_count),
								  [](usize t_index) { return encoded_assets[t_index].asset == static_cast<Asset>(t_index); }));
}

Assets::~Assets()
{
	if (m_decoder.joinable()) {
		m_decoder.join();
	}
}

std::span<const u8> Assets::encoded_bytes(Asset t_asset)
{
	return encoded_assets[static_cast<usize>(t_asset)].bytes;
}

std::unique_ptr<Texture> Assets::create_texture(Renderer *t_renderer, const u8 *t_rgba_pixels, u32 t_width, u32 t_height)
{
	std::unique_ptr<Texture> texture = upload(t_renderer, with_mipmaps(t_rgba_pixels, t_width, t_height));

	return texture->is_valid() ? std::move(texture) : nullptr;
}

std::unique_ptr<Texture> Assets::upload(Renderer *t_renderer, const DecodedImage &t_image)
{
	std::vector<TextureLevel> levels;
	levels.reserve(t_image.levels.size());

	for (const MipLevel &level : t_image.levels) {
		levels.push_back(TextureLevel{t_image.pixels.data() + level.offset, level.width, level.height});
	}

	return std::make_unique<Texture>(t_renderer, levels);
}

Assets::DecodedImage Assets::with_mipmaps(const u8 *t_rgba_pixels, u32 t_width, u32 t_height)
{
	DecodedImage image;
	usize total_bytes = 0;

	for (u32 width = t_width, height = t_height;; width = std::max(1u, width / 2), height = std::max(1u, height / 2)) {
		image.levels.push_back(MipLevel{width, height, total_bytes});
		total_bytes += static_cast<usize>(width) * height * 4;

		if (width == 1 && height == 1) break;
	}

	image.pixels.resize(total_bytes);
	std::memcpy(image.pixels.data(), t_rgba_pixels, static_cast<usize>(t_width) * t_height * 4);

	for (usize i = 1; i < image.levels.size(); i += 1) {
		const MipLevel &source = image.levels[i - 1];
		const MipLevel &target = image.levels[i];

		shrink_by_half(image.pixels.data() + source.offset, source.width, source.height, image.pixels.data() + target.offset, target.width, target.height);
	}

	return image;
}

void Assets::begin_decode()
{
	m_decoder = std::thread([this]() {
		m_decoded.resize(asset_count);
		std::atomic<usize> next_asset{0};

		const auto decode_remaining = [this, &next_asset]() {
			for (usize index = next_asset.fetch_add(1); index < asset_count; index = next_asset.fetch_add(1)) {
				const EncodedAsset &source = encoded_assets[index];

				int width = 0;
				int height = 0;
				int channels = 0;
				u8 *pixels = stbi_load_from_memory(source.bytes.data(), static_cast<int>(source.bytes.size()), &width, &height, &channels, 4);

				if (pixels == nullptr) {
					std::println("Failed to decode embedded asset '{}': {}", source.name, stbi_failure_reason());
					continue;
				}

				m_decoded[index] = with_mipmaps(pixels, static_cast<u32>(width), static_cast<u32>(height));
				stbi_image_free(pixels);
			}
		};

		const usize worker_count = std::clamp<usize>(std::thread::hardware_concurrency(), 1, asset_count);
		std::vector<std::thread> workers;
		workers.reserve(worker_count);

		for (usize i = 0; i < worker_count; i += 1) {
			workers.emplace_back(decode_remaining);
		}

		for (std::thread &worker : workers) {
			worker.join();
		}
	});
}

bool Assets::finish_upload(Renderer *t_renderer)
{
	if (m_decoder.joinable()) {
		m_decoder.join();
	}

	bool all_uploaded = true;

	for (usize i = 0; i < asset_count; i += 1) {
		const DecodedImage &image = m_decoded[i];

		if (!image.levels.empty()) {
			m_textures[i] = upload(t_renderer, image);
		}

		all_uploaded = all_uploaded && m_textures[i] != nullptr && m_textures[i]->is_valid();
	}

	m_decoded = {};

	return all_uploaded;
}
