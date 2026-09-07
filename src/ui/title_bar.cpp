#include "ui/title_bar.h"

#include <Windows.h>

#include "core/updater.h"
#include "gfx/asset_manager.h"
#include "gfx/font_manager.h"
#include "platform/window.h"
#include "ui/draw_list.h"
#include "ui/text.h"

namespace {
constexpr Color kColorGlyph{214, 214, 218, 255};
constexpr Color kColorGlyphDim{150, 150, 156, 255};

// Close reads warm on hover, the near-universal convention; every other button gets a neutral
// lighten matching the app menu's row hover.
constexpr Color kColorHoverNeutral{255, 255, 255, 18};
constexpr Color kColorHoverClose{232, 17, 35, 255};

// A muted background plus a brighter matching foreground, rather than one flat colour for
// both: a solid bright fill this small would read as an alert badge instead of a calm status
// pill.
constexpr Color kColorUpdateGoodBg{32, 58, 44, 255};
constexpr Color kColorUpdateGoodFg{110, 220, 150, 255};
constexpr Color kColorUpdateBadBg{58, 34, 34, 255};
constexpr Color kColorUpdateBadFg{230, 120, 110, 255};

constexpr float kGlyphLineThickness = 1.5f;
constexpr float kIconSize = 16.0f;

constexpr float kPillVerticalInset = 7.0f;
constexpr float kPillSideInset = 4.0f;
constexpr float kPillPadding = 10.0f;
constexpr float kPillIconTextGap = 8.0f;

Rect IconRectFor(Rect button)
{
	return Rect{button.X + (button.W - kIconSize) * 0.5f, button.Y + (button.H - kIconSize) * 0.5f, kIconSize,
				kIconSize};
}

// The update button occupies its reserved slot only once there is something worth a click -
// silent through the ordinary idle, checking and up-to-date states, matching this app's "check
// quietly, only surface it when there is something to say" design.
bool IsUpdateButtonVisible(EUpdateStage stage)
{
	switch (stage) {
		case EUpdateStage::Available:
		case EUpdateStage::ManualUpgradeRequired:
		case EUpdateStage::Downloading:
		case EUpdateStage::Verifying:
		case EUpdateStage::Installing:
		case EUpdateStage::ReadyToRelaunch:
		case EUpdateStage::Error:
		case EUpdateStage::Cancelled:
			return true;

		default:
			return false;
	}
}

bool IsUpdateFailure(EUpdateStage stage)
{
	return stage == EUpdateStage::Error || stage == EUpdateStage::Cancelled;
}

bool IsUpdateNotable(EUpdateStage stage)
{
	return IsUpdateFailure(stage) || stage == EUpdateStage::Available || stage == EUpdateStage::ManualUpgradeRequired;
}

// "Update Available" is the one this bar exists to make impossible to miss; the other stages
// get a plainer label rather than reusing that wording for something that is no longer an
// available, untouched update.
std::string_view UpdateStatusLabel(EUpdateStage stage)
{
	switch (stage) {
		case EUpdateStage::Available:
		case EUpdateStage::ManualUpgradeRequired:
			return "Update Available";

		case EUpdateStage::Error:
		case EUpdateStage::Cancelled:
			return "Update Failed";

		default:
			return "Updating...";
	}
}
} // namespace

CTitleBar::CTitleBar(CWindow *pWindow, const CAssetManager &assets, const CUpdater &updater, const CFontManager &fonts)
	: m_pWindow(pWindow)
	, m_assets(assets)
	, m_updater(updater)
	, m_fonts(fonts)
{
}

void CTitleBar::Update(float deltaSeconds)
{
	m_vecBounds = Rect{0.0f, 0.0f, static_cast<float>(m_pWindow->GetWidth()), kTitleBarHeight};
	m_pWindow->SetUpdateButtonVisible(IsUpdateButtonVisible(m_updater.GetStage()));
}

bool CTitleBar::OnPointerDown(float x, float y)
{
	return m_pWindow->TitleBarHitTest(x, y) != ETitleBarButton::None;
}

bool CTitleBar::OnPointerUp(float x, float y)
{
	const HWND hWnd = m_pWindow->GetHandle();

	switch (m_pWindow->TitleBarHitTest(x, y)) {
		case ETitleBarButton::None:
			return false;

		case ETitleBarButton::Menu:
			m_bMenuClickedThisFrame = true;
			break;

		case ETitleBarButton::Update:
			m_bUpdateClickedThisFrame = true;
			break;

		case ETitleBarButton::Minimize:
			ShowWindow(hWnd, SW_MINIMIZE);
			break;

		case ETitleBarButton::Maximize:
			ShowWindow(hWnd, IsZoomed(hWnd) ? SW_RESTORE : SW_MAXIMIZE);
			break;

		case ETitleBarButton::Close:
			// The same WM_CLOSE the OS caption's button would post, so CWindow's normal
			// close handling fires exactly as it does for Alt+F4 or the taskbar's command.
			PostMessageW(hWnd, WM_CLOSE, 0, 0);
			break;
	}

	return true;
}

ECursorKind CTitleBar::GetDesiredCursor() const
{
	return m_pWindow->TitleBarHitTest(m_flMouseX, m_flMouseY) == ETitleBarButton::None ? ECursorKind::Arrow
																					   : ECursorKind::Hand;
}

bool CTitleBar::ConsumeMenuClicked()
{
	const bool clicked = m_bMenuClickedThisFrame;
	m_bMenuClickedThisFrame = false;

	return clicked;
}

bool CTitleBar::ConsumeUpdateClicked()
{
	const bool clicked = m_bUpdateClickedThisFrame;
	m_bUpdateClickedThisFrame = false;

	return clicked;
}

// Flush rather than rounded, the same shape as the native buttons this mirrors. The hit test
// is the same one WM_NCHITTEST uses, so hover can never disagree with what is clickable.
void CTitleBar::DrawHoverBackground(CDrawList &drawList, ETitleBarButton button, ETitleBarButton hovered) const
{
	if (hovered != button) return;

	const Rect rect = m_pWindow->GetTitleBarButtonRect(button);
	const Color color = button == ETitleBarButton::Close ? kColorHoverClose : kColorHoverNeutral;

	drawList.AddRectFilled(rect.X, rect.Y, rect.W, rect.H, color);
}

// A labelled, colour-coded pill rather than a bare icon - which is why this slot is wider than
// every other button. It never opens the overlay on its own: the background check stays silent
// by design, and this pill is that check's one visible effect.
void CTitleBar::DrawUpdatePill(CDrawList &drawList) const
{
	const EUpdateStage stage = m_updater.GetStage();
	if (!IsUpdateButtonVisible(stage)) return;

	const bool failure = IsUpdateFailure(stage);
	const bool notable = IsUpdateNotable(stage);
	const Color pillBg = failure ? kColorUpdateBadBg : (notable ? kColorUpdateGoodBg : kColorHoverNeutral);
	const Color pillFg = failure ? kColorUpdateBadFg : (notable ? kColorUpdateGoodFg : kColorGlyphDim);

	const Rect button = m_pWindow->GetTitleBarButtonRect(ETitleBarButton::Update);
	const Rect pill{button.X + kPillSideInset, button.Y + kPillVerticalInset, button.W - kPillSideInset * 2.0f,
					button.H - kPillVerticalInset * 2.0f};
	drawList.AddRectRoundedFilled(pill.X, pill.Y, pill.W, pill.H, CDrawList::UniformRadii(pill.H * 0.5f), pillBg);

	const Rect icon{pill.X + kPillPadding, pill.Y + (pill.H - kIconSize) * 0.5f, kIconSize, kIconSize};
	drawList.AddRectRoundedTextured(icon.X, icon.Y, icon.W, icon.H, kCornerRadiiNone, m_assets.Get(EAsset::IconUpdate),
									pillFg);

	const CFont &secondary = m_fonts.GetSecondary();
	const float baselineY = pill.Y + pill.H * 0.5f + (secondary.GetAscent() + secondary.GetDescent()) * 0.5f;
	DrawText(drawList, secondary, icon.X + kIconSize + kPillIconTextGap, baselineY, UpdateStatusLabel(stage), pillFg);
}

// No embedded icon for this one: a square outline, or two overlapping ones once maximized.
void CTitleBar::DrawMaximizeGlyph(CDrawList &drawList, ETitleBarButton hovered) const
{
	const Rect rect = m_pWindow->GetTitleBarButtonRect(ETitleBarButton::Maximize);
	const float centerX = rect.X + rect.W * 0.5f;
	const float centerY = rect.Y + rect.H * 0.5f;
	const Color outlineColor = hovered == ETitleBarButton::Maximize ? kColorGlyph : kColorGlyphDim;

	if (!IsZoomed(m_pWindow->GetHandle())) {
		constexpr float kSize = 10.0f;
		drawList.AddRectOutline(centerX - kSize * 0.5f, centerY - kSize * 0.5f, kSize, kSize, kGlyphLineThickness,
								outlineColor);
		return;
	}

	constexpr float kSize = 8.0f;
	constexpr float kOffset = 3.0f;
	drawList.AddRectOutline(centerX - kSize * 0.5f + kOffset, centerY - kSize * 0.5f - kOffset, kSize, kSize,
							kGlyphLineThickness, outlineColor);
	drawList.AddRectOutline(centerX - kSize * 0.5f - kOffset, centerY - kSize * 0.5f + kOffset, kSize, kSize,
							kGlyphLineThickness, outlineColor);
}

void CTitleBar::Draw(CDrawList &drawList)
{
	const float width = static_cast<float>(m_pWindow->GetWidth());
	drawList.AddRectFilled(0.0f, 0.0f, width, kTitleBarHeight, kTitleBarColor);

	const ETitleBarButton hovered = m_pWindow->TitleBarHitTest(m_flMouseX, m_flMouseY);

	// A hamburger: this opens the app menu rather than Settings directly, so the gear lives on
	// that menu's Settings row.
	DrawHoverBackground(drawList, ETitleBarButton::Menu, hovered);
	const Rect menuIcon = IconRectFor(m_pWindow->GetTitleBarButtonRect(ETitleBarButton::Menu));
	drawList.AddRectRoundedTextured(menuIcon.X, menuIcon.Y, menuIcon.W, menuIcon.H, kCornerRadiiNone,
									m_assets.Get(EAsset::IconMenu), kColorGlyph);

	DrawUpdatePill(drawList);

	DrawHoverBackground(drawList, ETitleBarButton::Minimize, hovered);
	const Rect minimizeIcon = IconRectFor(m_pWindow->GetTitleBarButtonRect(ETitleBarButton::Minimize));
	drawList.AddRectRoundedTextured(minimizeIcon.X, minimizeIcon.Y, minimizeIcon.W, minimizeIcon.H, kCornerRadiiNone,
									m_assets.Get(EAsset::IconMinimize),
									hovered == ETitleBarButton::Minimize ? kColorGlyph : kColorGlyphDim);

	DrawHoverBackground(drawList, ETitleBarButton::Maximize, hovered);
	DrawMaximizeGlyph(drawList, hovered);

	DrawHoverBackground(drawList, ETitleBarButton::Close, hovered);
	const Rect closeIcon = IconRectFor(m_pWindow->GetTitleBarButtonRect(ETitleBarButton::Close));
	drawList.AddRectRoundedTextured(closeIcon.X, closeIcon.Y, closeIcon.W, closeIcon.H, kCornerRadiiNone,
									m_assets.Get(EAsset::IconClose), kColorGlyph);
}
