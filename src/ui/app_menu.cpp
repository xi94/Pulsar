#include "ui/app_menu.h"

#include <algorithm>

#include "core/animator.h"
#include "core/settings.h"
#include <string_view>
#include "gfx/asset_manager.h"
#include "platform/window.h"
#include "ui/draw_list.h"
#include "ui/text.h"

namespace {
constexpr float kMenuOpenEaseRate = 20.0f;
constexpr float kItemHoverEaseRate = 22.0f;

constexpr float kMenuX = 8.0f;
constexpr float kMenuWidth = 226.0f;
constexpr float kMenuItemHeight = 34.0f;
constexpr float kMenuPadding = 6.0f;
constexpr float kMenuRadius = 12.0f;
constexpr float kMenuSlidePixels = 6.0f;

// One number for the hover pill's inset and the header content's, so the pill, the text and
// the separator all line up on the same left edge instead of three hand-tuned values drifting.
constexpr float kMenuInsetX = 5.0f;
constexpr float kMenuContentX = 14.0f;
constexpr float kSeparatorGap = 6.0f;

constexpr float kMenuIconSize = 16.0f;
constexpr float kMenuIconTextGap = 10.0f;

constexpr Color kColorBg{30, 30, 34, 255};
constexpr Color kColorBorder{60, 60, 66, 255};
constexpr Color kColorSeparator{52, 52, 58, 255};
constexpr Color kColorText{220, 220, 224, 255};
constexpr Color kColorTextDisabled{100, 100, 106, 255};

// Geometry, hit-testing, hover and drawing all walk this one list, so a new item is one entry
// rather than a fourth near-identical branch. StartsGroup puts a separator above the item;
// grouping is by subject, since Check for Updates acts on this build while the other two are
// about this install's configuration.
struct MenuItem {
	EAppMenuAction Action;
	const char *pLabel;
	bool StartsGroup;
};

constexpr MenuItem kMenuItems[]{
	{EAppMenuAction::CheckForUpdates, "Check for Updates", false},
	{EAppMenuAction::OpenSettings, "Settings", true},
	{EAppMenuAction::OpenDataFolder, "Open Data Folder", false},
};

constexpr u64 kMenuItemCount = sizeof(kMenuItems) / sizeof(kMenuItems[0]);
static_assert(kMenuItemCount <= CAppMenu::kMaxItems, "grow CAppMenu::kMaxItems to match kMenuItems");

// The hairline plus the air on either side of it - one number, so the height math and the
// per-item walk cannot disagree.
constexpr float kGroupSeparatorBlock = kSeparatorGap * 2.0f + 1.0f;

// Exhaustive on purpose: adding an action without an icon is a compile error here rather than
// a blank square at runtime.
const CTexture *IconForItem(const CAssetManager &assets, u64 index)
{
	switch (kMenuItems[index].Action) {
		case EAppMenuAction::OpenSettings:
			return assets.Get(EAsset::IconSettings);
		case EAppMenuAction::OpenDataFolder:
			return assets.Get(EAsset::IconFolder);
		case EAppMenuAction::CheckForUpdates:
			return assets.Get(EAsset::IconUpdate);
		case EAppMenuAction::None:
			break;
	}

	return nullptr;
}

// The only place the item stride is expressed: walks the table so every separator above the
// index is counted exactly once.
float ItemOffsetY(u64 index)
{
	float offset = 0.0f;

	for (u64 i = 0; i < index; i += 1) {
		offset += kMenuItemHeight;

		if (i + 1 < kMenuItemCount && kMenuItems[i + 1].StartsGroup) {
			offset += kGroupSeparatorBlock;
		}
	}

	return offset;
}

Rect MenuRect(float openAmount)
{
	const float h = kMenuPadding * 2.0f + ItemOffsetY(kMenuItemCount - 1) + kMenuItemHeight;
	const float slide = (1.0f - openAmount) * -kMenuSlidePixels;

	return Rect{kMenuX, kTitleBarHeight + 4.0f + slide, kMenuWidth, h};
}

Rect ItemRect(Rect menu, u64 index)
{
	return Rect{menu.X, menu.Y + kMenuPadding + ItemOffsetY(index), menu.W, kMenuItemHeight};
}

Rect GroupSeparatorRect(Rect menu, u64 index)
{
	const Rect item = ItemRect(menu, index);

	return Rect{menu.X + kMenuContentX, item.Y - kSeparatorGap - 1.0f, menu.W - kMenuContentX * 2.0f, 1.0f};
}

// Inset from the popup's edges so the highlight reads as a chip inside the menu rather than a
// full-width band cutting it in half.
Rect ItemHighlightRect(Rect item)
{
	return Rect{item.X + kMenuInsetX, item.Y, item.W - kMenuInsetX * 2.0f, item.H};
}

// Puts the glyphs' visual centre - not just the ascent - on the row's centre, level with the
// icon. The nudge closes the last couple of pixels the metric math alone does not; it is
// empirical rather than font-exact.
float RowBaselineY(Rect row, const CFont &font)
{
	constexpr float kBaselineVisualNudge = 2.0f;

	return row.Y + row.H * 0.5f + (font.GetAscent() + font.GetDescent()) * 0.5f - kBaselineVisualNudge;
}
} // namespace

CAppMenu::CAppMenu(const CFontManager &fonts, const CAssetManager &assets, const Settings &settings,
				   const bool &appLocked)
	: m_fonts(fonts)
	, m_assets(assets)
	, m_settings(settings)
	, m_appLocked(appLocked)
{
}

void CAppMenu::Open()
{
	m_bOpen = true;
}

// Nothing transient to reset: the hover amounts ease back down on their own.
void CAppMenu::Close()
{
	m_bOpen = false;
}

bool CAppMenu::IsItemEnabled(u64 index) const
{
	if (kMenuItems[index].Action == EAppMenuAction::OpenSettings) return !m_appLocked;

	return true;
}

void CAppMenu::Update(float deltaSeconds)
{
	m_flOpenAmount = CAnimator::EaseToward(m_flOpenAmount, m_bOpen ? 1.0f : 0.0f, kMenuOpenEaseRate, deltaSeconds);
	if (!m_bOpen && m_flOpenAmount < 0.002f) {
		m_flOpenAmount = 0.0f;
	}

	const Rect menu = MenuRect(m_flOpenAmount);

	for (u64 i = 0; i < kMenuItemCount; i += 1) {
		// A row that is currently a no-op gets no hover highlight either.
		const bool hovered =
			IsBlocking() && IsItemEnabled(i) && RectContainsPoint(ItemRect(menu, i), m_flMouseX, m_flMouseY);

		m_aItemHoverAmount[i] =
			CAnimator::EaseToward(m_aItemHoverAmount[i], hovered ? 1.0f : 0.0f, kItemHoverEaseRate, deltaSeconds);
	}
}

bool CAppMenu::OnPointerUp(float x, float y)
{
	if (!IsBlocking()) return false;

	const Rect menu = MenuRect(m_flOpenAmount);

	for (u64 i = 0; i < kMenuItemCount; i += 1) {
		if (!RectContainsPoint(ItemRect(menu, i), x, y)) continue;

		if (IsItemEnabled(i)) {
			m_pendingAction = kMenuItems[i].Action;
		}

		break;
	}

	Close();

	return true;
}

ECursorKind CAppMenu::GetDesiredCursor() const
{
	if (!IsBlocking()) return ECursorKind::Arrow;

	const Rect menu = MenuRect(m_flOpenAmount);

	for (u64 i = 0; i < kMenuItemCount; i += 1) {
		if (IsItemEnabled(i) && RectContainsPoint(ItemRect(menu, i), m_flMouseX, m_flMouseY)) {
			return ECursorKind::Hand;
		}
	}

	return ECursorKind::Arrow;
}

EAppMenuAction CAppMenu::ConsumeAction()
{
	const EAppMenuAction action = m_pendingAction;
	m_pendingAction = EAppMenuAction::None;

	return action;
}

void CAppMenu::Draw(CDrawList &drawList)
{
	if (m_flOpenAmount <= 0.001f) return;

	const auto alpha = static_cast<u8>(255.0f * m_flOpenAmount);
	const Rect rect = MenuRect(m_flOpenAmount);
	const CFont &body = m_fonts.GetBody();

	drawList.AddRectRoundedFilled(rect.X, rect.Y, rect.W, rect.H, CDrawList::UniformRadii(kMenuRadius),
								  ColorWithAlpha(kColorBorder, alpha));
	drawList.AddRectRoundedFilled(rect.X + 1.0f, rect.Y + 1.0f, rect.W - 2.0f, rect.H - 2.0f,
								  CDrawList::UniformRadii(kMenuRadius - 1.0f), ColorWithAlpha(kColorBg, alpha));

	for (u64 i = 1; i < kMenuItemCount; i += 1) {
		if (!kMenuItems[i].StartsGroup) continue;

		const Rect separator = GroupSeparatorRect(rect, i);
		drawList.AddRectFilled(separator.X, separator.Y, separator.W, separator.H,
							   ColorWithAlpha(kColorSeparator, alpha));
	}

	for (u64 i = 0; i < kMenuItemCount; i += 1) {
		const Rect item = ItemRect(rect, i);
		const float hover = m_aItemHoverAmount[i];

		if (hover > 0.001f) {
			// The user's accent mixed most of the way back toward the background: a
			// full-strength fill behind body text would be louder than anything else on
			// screen, and this menu is not the app's focal point.
			const Color hoverColor = ColorLerp(kColorBg, m_settings.m_clrAccent, 0.28f);
			const auto hoverAlpha = static_cast<u8>(static_cast<float>(alpha) * hover);
			const Rect highlight = ItemHighlightRect(item);

			drawList.AddRectRoundedFilled(highlight.X, highlight.Y, highlight.W, highlight.H,
										  CDrawList::UniformRadii(7.0f), ColorWithAlpha(hoverColor, hoverAlpha));
		}

		const Color textColor = IsItemEnabled(i) ? kColorText : kColorTextDisabled;

		// Every row shares the same left padding, so icons - and the text shifted over to make
		// room - always land at the same x.
		const Rect icon{item.X + kMenuContentX, item.Y + (item.H - kMenuIconSize) * 0.5f, kMenuIconSize, kMenuIconSize};
		drawList.AddRectRoundedTextured(icon.X, icon.Y, icon.W, icon.H, kCornerRadiiNone, IconForItem(m_assets, i),
										ColorWithAlpha(textColor, alpha));

		DrawText(drawList, body, icon.X + kMenuIconSize + kMenuIconTextGap, RowBaselineY(item, body),
				 kMenuItems[i].pLabel, ColorWithAlpha(textColor, alpha));
	}
}
