#pragma once

#include <memory>
#include <thread>
#include <vector>

#include "gfx/texture.h"

class IRenderer;

/// Every compiled-in image, in the order asset_manager.cpp's table declares them.
enum class EAsset : u8 {
	IconArrowBack,
	IconClose,
	IconMinimize,
	IconSettings,
	IconMenu,
	IconAdd,
	IconEdit,
	IconFolder,

	/// The carousel's view-mode flyout rows.
	IconGrid,
	IconList,
	IconCarousel,

	/// The account form's "visible in N games" chip.
	IconListArrow,

	/// Password show/hide, on the account form and the unlock screen.
	IconEyeVisible,
	IconEyeHidden,

	IconUpdate,

	/// The settings panel's per-row restore button, drawn rotated so it can spin on click.
	IconReset,

	/// The app's own mark, beside the version in the status bar. Desaturated at embed time rather
	/// than tinted from the blue original: multiplying a colour by grey darkens it, it does not
	/// drain it, so a tintable icon has to start monochrome.
	IconApp,

	/// Per-game icons, drawn by List view and the game-select popup instead of a cover-fit crop.
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

/// The undecoded bytes of an embedded image, for a consumer that needs its own decode rather
/// than a GPU texture - the tray builds GDI bitmaps for its menu from these.
struct EmbeddedImageBytes {
	const u8 *pBytes;
	u64 Length;
};

/// One decoded image, still holding stb's own pixel buffer - the upload is what consumes and frees
/// it. At namespace scope because the decode workers hand these back before any texture exists.
struct DecodedImage {
	unsigned char *pPixels = nullptr; // null on failure
	u32 Width = 0;
	u32 Height = 0;
};

/// Decodes every embedded image into a GPU texture once at startup. Nothing else touches the
/// embed headers; everything downstream holds a CTexture owned here for the process lifetime.
class CAssetManager {
  public:
	~CAssetManager();

	/// Starts decoding every embedded image on worker threads. Call as early as possible: it needs
	/// no renderer, and the work it overlaps with is D3D device creation.
	void BeginDecode();

	/// Joins the decode and uploads the results. Requires an initialized renderer. False, having
	/// logged which asset failed, if any texture failed to decode or upload - a corrupt embed is a
	/// build problem, not something to silently render blank.
	bool FinishUpload(IRenderer *pRenderer);

	CTexture *Get(EAsset asset) const
	{
		return m_textures[static_cast<usize>(asset)].get();
	}

	static EmbeddedImageBytes GetSourceBytes(EAsset asset);

  private:
	std::unique_ptr<CTexture> m_textures[static_cast<usize>(EAsset::Count)];

	std::thread m_decodeWorker;
	std::vector<DecodedImage> m_decoded;
};
