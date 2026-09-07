#pragma once

#include "core/account.h"
#include <string_view>
#include "core/types.h"

class CTexture;

constexpr u32 kCarouselMaxBanners = 16;
constexpr u32 kCarouselMaxAccountsPerBanner = 32;
constexpr float kCarouselCardCornerRadius = 14.0f;

/// One game in the carousel plus the accounts filed under it. CStorage round-trips only the
/// account list - the artwork and accent are compiled in, not user data.
struct Banner {
	std::string_view Title;
	Color Accent{};

	CTexture *pTexture = nullptr;
	float TextureAspect = 1.0f; // width/height at load time, so cover-fit never re-queries the renderer

	/// Square per-game icon for List view's thumbnail and CGameSelectPopup's rows. May be
	/// null, in which case call sites fall back to pTexture or Accent.
	CTexture *pIcon = nullptr;

	Account Accounts[kCarouselMaxAccountsPerBanner];
	u32 AccountCount = 0;
};
