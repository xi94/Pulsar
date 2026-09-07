#include "gfx/asset_manager.h"

#include <algorithm>
#include <atomic>
#include <print>
#include <thread>
#include <vector>

#include "embeds/banners/2XKO.hpp"
#include "embeds/banners/LeagueOfLegends.hpp"
#include "embeds/banners/Runeterra.hpp"
#include "embeds/banners/TeamfightTactics.hpp"
#include "embeds/banners/Valorant.hpp"
#include "embeds/icons/2XKOIcon.hpp"
#include "embeds/icons/AddIcon.hpp"
#include "embeds/icons/ArrowBack.hpp"
#include "embeds/icons/CarouselIcon.hpp"
#include "embeds/icons/Close.hpp"
#include "embeds/icons/EditIcon.hpp"
#include "embeds/icons/EyeHiddenIcon.hpp"
#include "embeds/icons/EyeVisible.hpp"
#include "embeds/icons/FolderIcon.hpp"
#include "embeds/icons/GridIcon.hpp"
#include "embeds/icons/LeagueIcon.hpp"
#include "embeds/icons/ListArrow.hpp"
#include "embeds/icons/ListIcon.hpp"
#include "embeds/icons/MenuIcon.hpp"
#include "embeds/icons/Minimize.hpp"
#include "embeds/icons/AppMark.hpp"
#include "embeds/icons/ResetIcon.hpp"
#include "embeds/icons/RuneterraIcon.hpp"
#include "embeds/icons/Settings.hpp"
#include "embeds/icons/TFTIcon.hpp"
#include "embeds/icons/UpdateIcon.hpp"
#include "embeds/icons/ValorantIcon.hpp"

#include "stb/stb_image.h"

namespace {
using namespace pulsar::embed;

constexpr usize kAssetCount = static_cast<usize>(EAsset::Count);

struct AssetSource {
	const u8 *pBytes;
	u64 Length;
	const char *pName;
};

// Indexed by EAsset, so the enum and this table cannot drift.
constexpr AssetSource kAssetSources[kAssetCount]{
	{icon::arrow_back_icon.data(), icon::arrow_back_icon.size(), "ArrowBack"},
	{icon::close_icon.data(), icon::close_icon.size(), "Close"},
	{icon::minimize_icon.data(), icon::minimize_icon.size(), "Minimize"},
	{icon::settings_icon.data(), icon::settings_icon.size(), "Settings"},
	{icon::menu_icon.data(), icon::menu_icon.size(), "MenuIcon"},
	{icon::add_icon.data(), icon::add_icon.size(), "AddIcon"},
	{icon::edit_icon.data(), icon::edit_icon.size(), "EditIcon"},
	{icon::folder_icon.data(), icon::folder_icon.size(), "FolderIcon"},
	{icon::grid_icon.data(), icon::grid_icon.size(), "GridIcon"},
	{icon::list_icon.data(), icon::list_icon.size(), "ListIcon"},
	{icon::carousel_icon.data(), icon::carousel_icon.size(), "CarouselIcon"},
	{icon::list_arrow.data(), icon::list_arrow.size(), "ListArrow"},
	{icon::eye_visible_icon.data(), icon::eye_visible_icon.size(), "EyeVisible"},
	{icon::eye_hidden_icon.data(), icon::eye_hidden_icon.size(), "EyeHiddenIcon"},
	{icon::update_icon.data(), icon::update_icon.size(), "UpdateIcon"},
	{icon::reset_icon.data(), icon::reset_icon.size(), "ResetIcon"},
	{icon::app_mark.data(), icon::app_mark.size(), "AppMark"},
	{icon::league_of_legends_icon.data(), icon::league_of_legends_icon.size(), "LeagueIcon"},
	{icon::valorant_icon.data(), icon::valorant_icon.size(), "ValorantIcon"},
	{icon::two_xko_icon.data(), icon::two_xko_icon.size(), "2XKOIcon"},
	{icon::runeterra_icon.data(), icon::runeterra_icon.size(), "RuneterraIcon"},
	{icon::teamfight_tactics_icon.data(), icon::teamfight_tactics_icon.size(), "TFTIcon"},
	{banner::league_of_legends.data(), banner::league_of_legends.size(), "LeagueOfLegends"},
	{banner::valorant.data(), banner::valorant.size(), "Valorant"},
	{banner::two_xko.data(), banner::two_xko.size(), "2XKO"},
	{banner::runeterra.data(), banner::runeterra.size(), "Runeterra"},
	{banner::teamfight_tactics.data(), banner::teamfight_tactics.size(), "TeamfightTactics"},
};

// Pure CPU work against its own input and output, which is what makes fanning these out across
// threads safe. The one caveat is stbi_failure_reason, a plain global in this build, so two
// simultaneous failures could log each other's message - harmless for known-good embeds.
DecodedImage DecodeImage(const AssetSource &source)
{
	DecodedImage image;

	int width = 0;
	int height = 0;
	int sourceChannels = 0;
	image.pPixels =
		stbi_load_from_memory(source.pBytes, static_cast<int>(source.Length), &width, &height, &sourceChannels, 4);

	if (image.pPixels == nullptr) {
		std::println("Failed to decode embedded asset '{}': {}", source.pName, stbi_failure_reason());
		return image;
	}

	image.Width = static_cast<u32>(width);
	image.Height = static_cast<u32>(height);

	return image;
}

// Never fanned out: CreateTexture mutates a shared unsynchronized pool, so every
// upload stays serialized on the calling thread like every other pRenderer call.
std::unique_ptr<CTexture> UploadTexture(IRenderer *pRenderer, const DecodedImage &image)
{
	if (image.pPixels == nullptr) return nullptr;

	auto pTexture = std::make_unique<CTexture>(pRenderer, image.pPixels, image.Width, image.Height);
	stbi_image_free(image.pPixels);

	return pTexture;
}

// Essentially all of the decode cost lives here - five banner JPEGs plus twenty PNG icons.
//
// One worker per core pulling from a shared cursor, rather than one thread per asset: twenty-five
// thread creations cost more than the decodes they were meant to overlap, and the five banners
// dominate the total anyway, so what matters is that those five land on different cores.
std::vector<DecodedImage> DecodeAll()
{
	std::vector<DecodedImage> decoded(kAssetCount);
	std::atomic<usize> nextIndex{0};

	const unsigned int coreCount = std::max(1u, std::thread::hardware_concurrency());
	const usize workerCount = std::min<usize>(coreCount, kAssetCount);

	std::vector<std::thread> workers;
	workers.reserve(workerCount);

	for (usize w = 0; w < workerCount; w += 1) {
		workers.emplace_back([&decoded, &nextIndex]() {
			for (;;) {
				const usize index = nextIndex.fetch_add(1, std::memory_order_relaxed);
				if (index >= kAssetCount) return;

				decoded[index] = DecodeImage(kAssetSources[index]);
			}
		});
	}

	for (std::thread &worker : workers) {
		worker.join();
	}

	return decoded;
}
} // namespace

EmbeddedImageBytes CAssetManager::GetSourceBytes(EAsset asset)
{
	const AssetSource &source = kAssetSources[static_cast<usize>(asset)];

	return EmbeddedImageBytes{source.pBytes, source.Length};
}

// Started before the D3D device exists, because decoding needs no GPU and device creation is a
// quarter of a second of driver work with the CPU otherwise idle. The two overlap almost exactly,
// which turns the whole decode into free time.
void CAssetManager::BeginDecode()
{
	m_decodeWorker = std::thread([this]() { m_decoded = DecodeAll(); });
}

bool CAssetManager::FinishUpload(IRenderer *pRenderer)
{
	if (m_decodeWorker.joinable()) {
		m_decodeWorker.join();
	}

	bool allSucceeded = true;
	for (usize i = 0; i < kAssetCount; i += 1) {
		m_textures[i] = UploadTexture(pRenderer, m_decoded[i]);
		allSucceeded = allSucceeded && m_textures[i] != nullptr;
	}

	// The pixel buffers are gone now, consumed by the uploads above.
	m_decoded.clear();
	m_decoded.shrink_to_fit();

	return allSucceeded;
}

CAssetManager::~CAssetManager()
{
	if (m_decodeWorker.joinable()) {
		m_decodeWorker.join();
	}
}
