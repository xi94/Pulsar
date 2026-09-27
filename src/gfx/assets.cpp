#include "gfx/assets.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <print>

#include "embeds/banners/2XKO.hpp"
#include "embeds/banners/LeagueOfLegends.hpp"
#include "embeds/banners/Runeterra.hpp"
#include "embeds/banners/TeamfightTactics.hpp"
#include "embeds/banners/Valorant.hpp"
#include "embeds/icons/2XKOIcon.hpp"
#include "embeds/icons/AddIcon.hpp"
#include "embeds/icons/AppMark.hpp"
#include "embeds/icons/ArrowBack.hpp"
#include "embeds/icons/CarouselIcon.hpp"
#include "embeds/icons/Close.hpp"
#include "embeds/icons/EditIcon.hpp"
#include "embeds/icons/EyeHiddenIcon.hpp"
#include "embeds/icons/EyeVisible.hpp"
#include "embeds/icons/FavoriteIcon.hpp"
#include "embeds/icons/FolderIcon.hpp"
#include "embeds/icons/GridIcon.hpp"
#include "embeds/icons/LeagueIcon.hpp"
#include "embeds/icons/ListArrow.hpp"
#include "embeds/icons/ListIcon.hpp"
#include "embeds/icons/MenuIcon.hpp"
#include "embeds/icons/Minimize.hpp"
#include "embeds/icons/ResetIcon.hpp"
#include "embeds/icons/RuneterraIcon.hpp"
#include "embeds/icons/Settings.hpp"
#include "embeds/icons/TFTIcon.hpp"
#include "embeds/icons/UpdateIcon.hpp"
#include "embeds/icons/ValorantIcon.hpp"

#include "stb/stb_image.h"

namespace {
using namespace pulsar::embed;

struct EncodedAsset {
	std::span<const u8> bytes;
	const char *name;
};

void shrink_by_half(const u8 *t_source, u32 t_source_width, u32 t_source_height, u8 *t_target, u32 t_target_width,
					u32 t_target_height)
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

const EncodedAsset encoded_assets[asset_count]{
	{icon::arrow_back_icon, "ArrowBack"},
	{icon::close_icon, "Close"},
	{icon::minimize_icon, "Minimize"},
	{icon::settings_icon, "Settings"},
	{icon::menu_icon, "MenuIcon"},
	{icon::add_icon, "AddIcon"},
	{icon::edit_icon, "EditIcon"},
	{icon::folder_icon, "FolderIcon"},
	{icon::grid_icon, "GridIcon"},
	{icon::list_icon, "ListIcon"},
	{icon::carousel_icon, "CarouselIcon"},
	{icon::list_arrow, "ListArrow"},
	{icon::eye_visible_icon, "EyeVisible"},
	{icon::eye_hidden_icon, "EyeHiddenIcon"},
	{icon::favorite_icon, "FavoriteIcon"},
	{icon::update_icon, "UpdateIcon"},
	{icon::reset_icon, "ResetIcon"},
	{icon::app_mark, "AppMark"},
	{icon::league_of_legends_icon, "LeagueIcon"},
	{icon::valorant_icon, "ValorantIcon"},
	{icon::two_xko_icon, "2XKOIcon"},
	{icon::runeterra_icon, "RuneterraIcon"},
	{icon::teamfight_tactics_icon, "TFTIcon"},
	{banner::league_of_legends, "LeagueOfLegends"},
	{banner::valorant, "Valorant"},
	{banner::two_xko, "2XKO"},
	{banner::runeterra, "Runeterra"},
	{banner::teamfight_tactics, "TeamfightTactics"},
};
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

		shrink_by_half(image.pixels.data() + source.offset, source.width, source.height,
					   image.pixels.data() + target.offset, target.width, target.height);
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
				u8 *pixels = stbi_load_from_memory(source.bytes.data(), static_cast<int>(source.bytes.size()), &width,
												   &height, &channels, 4);

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

bool Assets::finish_upload(Renderer &t_renderer)
{
	if (m_decoder.joinable()) {
		m_decoder.join();
	}

	bool all_uploaded = true;

	for (usize i = 0; i < asset_count; i += 1) {
		const DecodedImage &image = m_decoded[i];

		if (!image.levels.empty()) {
			std::vector<TextureLevel> levels;
			levels.reserve(image.levels.size());

			for (const MipLevel &level : image.levels) {
				levels.push_back(TextureLevel{image.pixels.data() + level.offset, level.width, level.height});
			}

			m_textures[i] = std::make_unique<Texture>(t_renderer, levels);
		}

		all_uploaded = all_uploaded && m_textures[i] != nullptr && m_textures[i]->is_valid();
	}

	m_decoded = {};

	return all_uploaded;
}
