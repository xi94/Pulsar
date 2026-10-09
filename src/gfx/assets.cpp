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
#include "embeds/icons/CopyIcon.hpp"
#include "embeds/icons/DownloadIcon.hpp"
#include "embeds/icons/EditBoxIcon.hpp"
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
#include "embeds/icons/KeyIcon.hpp"
#include "embeds/icons/LeagueIcon.hpp"
#include "embeds/icons/LibraryIcon.hpp"
#include "embeds/icons/ListArrow.hpp"
#include "embeds/icons/LockIcon.hpp"
#include "embeds/icons/LogoIcon.hpp"
#include "embeds/icons/MenuIcon.hpp"
#include "embeds/icons/Minimize.hpp"
#include "embeds/icons/ResetIcon.hpp"
#include "embeds/icons/RuneterraIcon.hpp"
#include "embeds/icons/Settings.hpp"
#include "embeds/icons/ShelfIcon.hpp"
#include "embeds/icons/TFTIcon.hpp"
#include "embeds/icons/TrashIcon.hpp"
#include "embeds/icons/UpdateIcon.hpp"
#include "embeds/icons/ValorantIcon.hpp"

#include "stb/stb_image.h"

namespace {
using namespace pulsar::embed;

struct EncodedAsset {
	Asset               asset;
	std::span<const u8> bytes;
	const char*         name;
};

auto shrink_by_half(const u8* t_source, u32 t_source_width, u32 t_source_height, u8* t_target, u32 t_target_width, u32 t_target_height) -> void
{
	for (u32 y = 0; y < t_target_height; y += 1) {
		const u32 rows[2]{std::min(y * 2, t_source_height - 1), std::min(y * 2 + 1, t_source_height - 1)};

		for (u32 x = 0; x < t_target_width; x += 1) {
			const u32 columns[2]{std::min(x * 2, t_source_width - 1), std::min(x * 2 + 1, t_source_width - 1)};

			u32 alpha_total = 0;
			u32 color_total[3]{};

			for (const u32 row : rows) {
				for (const u32 column : columns) {
					const u8* texel = t_source + (static_cast<usize>(row) * t_source_width + column) * 4;
					alpha_total += texel[3];

					for (u32 channel = 0; channel < 3; channel += 1) {
						color_total[channel] += texel[channel] * texel[3];
					}
				}
			}

			u8* target = t_target + (static_cast<usize>(y) * t_target_width + x) * 4;
			for (u32 channel = 0; channel < 3; channel += 1) {
				target[channel] = alpha_total > 0 ? static_cast<u8>(color_total[channel] / alpha_total) : 0;
			}
			target[3] = static_cast<u8>((alpha_total + 2) / 4);
		}
	}
}

constexpr EncodedAsset K_ENCODED_ASSETS[]{
	{Asset::ICON_ARROW_BACK, icon::arrow_back_icon, "ArrowBack"},
	{Asset::ICON_CLOSE, icon::close_icon, "Close"},
	{Asset::ICON_MINIMIZE, icon::minimize_icon, "Minimize"},
	{Asset::ICON_SETTINGS, icon::settings_icon, "Settings"},
	{Asset::ICON_MENU, icon::menu_icon, "MenuIcon"},
	{Asset::ICON_ADD, icon::add_icon, "AddIcon"},
	{Asset::ICON_EDIT, icon::edit_icon, "EditIcon"},
	{Asset::ICON_FOLDER_OPEN, icon::folder_open_icon, "FolderOpenIcon"},
	{Asset::ICON_GRID, icon::grid_icon, "GridIcon"},
	{Asset::ICON_LIBRARY, icon::library_icon, "LibraryIcon"},
	{Asset::ICON_CAROUSEL, icon::carousel_icon, "CarouselIcon"},
	{Asset::ICON_SHELF, icon::shelf_icon, "ShelfIcon"},
	{Asset::ICON_ICONS, icon::icons_icon, "IconsIcon"},
	{Asset::ICON_LIST_ARROW, icon::list_arrow, "ListArrow"},
	{Asset::ICON_EYE_VISIBLE, icon::eye_visible_icon, "EyeVisible"},
	{Asset::ICON_EYE_HIDDEN, icon::eye_hidden_icon, "EyeHiddenIcon"},
	{Asset::ICON_FAVORITE, icon::favorite_icon, "FavoriteIcon"},
	{Asset::ICON_UPDATE, icon::update_icon, "UpdateIcon"},
	{Asset::ICON_RESET, icon::reset_icon, "ResetIcon"},
	{Asset::ICON_ACCOUNT, icon::account_icon, "AccountIcon"},
	{Asset::ICON_IMAGE, icon::image_icon, "ImageIcon"},
	{Asset::ICON_LOCK, icon::lock_icon, "LockIcon"},
	{Asset::ICON_FOLDER, icon::folder_icon, "FolderBlankIcon"},
	{Asset::ICON_FILE, icon::file_icon, "FileIcon"},
	{Asset::ICON_DOWNLOAD, icon::download_icon, "DownloadIcon"},
	{Asset::ICON_CHECK, icon::check_icon, "CheckedIcon"},
	{Asset::ICON_APP, icon::app_mark, "AppMark"},
	{Asset::ICON_LOGO, icon::logo_icon, "LogoIcon"},
	{Asset::ICON_LEAGUE_OF_LEGENDS, icon::league_of_legends_icon, "LeagueIcon"},
	{Asset::ICON_VALORANT, icon::valorant_icon, "ValorantIcon"},
	{Asset::ICON_TWO_XKO, icon::two_xko_icon, "2XKOIcon"},
	{Asset::ICON_RUNETERRA, icon::runeterra_icon, "RuneterraIcon"},
	{Asset::ICON_TEAMFIGHT_TACTICS, icon::teamfight_tactics_icon, "TFTIcon"},
	{Asset::ICON_EDIT_BOX, icon::edit_box_icon, "EditBoxIcon"},
	{Asset::ICON_COPY, icon::copy_icon, "CopyIcon"},
	{Asset::ICON_KEY, icon::key_icon, "KeyIcon"},
	{Asset::ICON_TRASH, icon::trash_icon, "TrashIcon"},
	{Asset::BANNER_LEAGUE_OF_LEGENDS, banner::league_of_legends, "LeagueOfLegends"},
	{Asset::BANNER_VALORANT, banner::valorant, "Valorant"},
	{Asset::BANNER_TWO_XKO, banner::two_xko, "2XKO"},
	{Asset::BANNER_RUNETERRA, banner::runeterra, "Runeterra"},
	{Asset::BANNER_TEAMFIGHT_TACTICS, banner::teamfight_tactics, "TeamfightTactics"},
};

static_assert(std::size(K_ENCODED_ASSETS) == K_ASSET_COUNT);
static_assert(std::ranges::all_of(std::views::iota(usize{0}, K_ASSET_COUNT),
                                  [](usize t_index) { return K_ENCODED_ASSETS[t_index].asset == static_cast<Asset>(t_index); }));
}

Assets::~Assets()
{
	if (m_decoder.joinable()) {
		m_decoder.join();
	}
}

auto Assets::encoded_bytes(Asset t_asset) -> std::span<const u8>
{
	return K_ENCODED_ASSETS[static_cast<usize>(t_asset)].bytes;
}

auto Assets::create_texture(Renderer* t_renderer, const u8* t_rgba_pixels, u32 t_width, u32 t_height) -> std::unique_ptr<Texture>
{
	std::unique_ptr<Texture> texture = upload(t_renderer, with_mipmaps(t_rgba_pixels, t_width, t_height));

	return texture->is_valid() ? std::move(texture) : nullptr;
}

auto Assets::levels_of(const DecodedImage& t_image) -> std::vector<TextureLevel>
{
	std::vector<TextureLevel> levels;
	levels.reserve(t_image.levels.size());

	for (const MipLevel& level : t_image.levels) {
		levels.push_back(TextureLevel{t_image.pixels.data() + level.offset, level.width, level.height});
	}

	return levels;
}

auto Assets::upload(Renderer* t_renderer, const DecodedImage& t_image) -> std::unique_ptr<Texture>
{
	return std::make_unique<Texture>(t_renderer, levels_of(t_image));
}

auto Assets::with_mipmaps(const u8* t_rgba_pixels, u32 t_width, u32 t_height) -> Assets::DecodedImage
{
	DecodedImage image;
	usize        total_bytes = 0;

	for (u32 width = t_width, height = t_height;; width = std::max(1u, width / 2), height = std::max(1u, height / 2)) {
		image.levels.push_back(MipLevel{width, height, total_bytes});
		total_bytes += static_cast<usize>(width) * height * 4;

		if (width == 1 && height == 1) break;
	}

	image.pixels.resize(total_bytes);
	std::memcpy(image.pixels.data(), t_rgba_pixels, static_cast<usize>(t_width) * t_height * 4);

	for (usize i = 1; i < image.levels.size(); i += 1) {
		const MipLevel& source = image.levels[i - 1];
		const MipLevel& target = image.levels[i];

		shrink_by_half(image.pixels.data() + source.offset, source.width, source.height, image.pixels.data() + target.offset, target.width, target.height);
	}

	return image;
}

auto Assets::begin_decode() -> void
{
	m_decoder = std::thread([this]() {
		m_decoded.resize(K_ASSET_COUNT);
		std::atomic<usize> next_asset{0};

		const auto decode_remaining = [this, &next_asset]() {
			for (usize index = next_asset.fetch_add(1); index < K_ASSET_COUNT; index = next_asset.fetch_add(1)) {
				const EncodedAsset& source = K_ENCODED_ASSETS[index];

				int width    = 0;
				int height   = 0;
				int channels = 0;
				u8* pixels   = stbi_load_from_memory(source.bytes.data(), static_cast<int>(source.bytes.size()), &width, &height, &channels, 4);

				if (pixels == nullptr) {
					std::println("Failed to decode embedded asset '{}': {}", source.name, stbi_failure_reason());
					continue;
				}

				m_decoded[index] = with_mipmaps(pixels, static_cast<u32>(width), static_cast<u32>(height));
				stbi_image_free(pixels);
			}
		};

		const usize              worker_count = std::clamp<usize>(std::thread::hardware_concurrency(), 1, K_ASSET_COUNT);
		std::vector<std::thread> workers;
		workers.reserve(worker_count);

		for (usize i = 0; i < worker_count; i += 1) {
			workers.emplace_back(decode_remaining);
		}

		for (std::thread& worker : workers) {
			worker.join();
		}
	});
}

auto Assets::finish_upload(Renderer* t_renderer) -> bool
{
	if (m_decoder.joinable()) {
		m_decoder.join();
	}

	bool all_uploaded = true;

	for (usize i = 0; i < K_ASSET_COUNT; i += 1) {
		const DecodedImage& image = m_decoded[i];

		if (!image.levels.empty()) {
			m_textures[i] = upload(t_renderer, image);
		}

		all_uploaded = all_uploaded && m_textures[i] != nullptr && m_textures[i]->is_valid();
	}

	m_decoded = {};

	return all_uploaded;
}

// After the renderer rebuilds a lost device, every image is decoded again into the texture that already holds its slot, so pointers to
// those textures stay good.
auto Assets::restore(Renderer* t_renderer) -> void
{
	begin_decode();
	m_decoder.join();

	for (usize i = 0; i < K_ASSET_COUNT; i += 1) {
		if (m_decoded[i].levels.empty()) continue;

		if (m_textures[i] != nullptr) {
			m_textures[i]->restore(levels_of(m_decoded[i]));
		} else {
			m_textures[i] = upload(t_renderer, m_decoded[i]);
		}
	}

	m_decoded = {};
}
