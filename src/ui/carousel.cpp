#include "ui/carousel.h"

#include "core/profiler.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/animator.h"
#include "gfx/asset_manager.h"
#include "gfx/texture.h"
#include "platform/window.h"
#include "ui/draw_list.h"
#include "ui/layout.h"
#include "ui/text.h"

namespace {
constexpr float kCardWidth = 220.0f;
constexpr float kCardHeight = 300.0f;
constexpr float kCardSpacing = 36.0f;

// A nominal stride, used only to turn a drag's pixel delta into slot units - an input
// sensitivity, not the actual on-screen spacing, which CumulativeSlotOffset computes.
constexpr float kCardStride = kCardWidth + kCardSpacing;

constexpr float kEaseRate = 12.0f;
constexpr float kViewModeTransitionEaseRate = 16.0f;
constexpr float kViewModeSlidePixels = 18.0f;

// Slower than most easing here on purpose: List's row-icon growth reads best gradual.
constexpr float kZoomPercentEaseRate = 9.0f;

// The max preserves the base's aspect ratio exactly, so the card shape does not distort as it
// grows across Grid's three zoom stops.
constexpr float kGridCardWidthBase = 160.0f;
constexpr float kGridCardHeightBase = 220.0f;
constexpr float kGridCardWidthMax = 224.0f;
constexpr float kGridCardHeightMax = 308.0f;
constexpr float kGridGap = 24.0f;
constexpr float kGridPadding = 24.0f;

// Noticeable but not cartoonish, and well under half the gap on a side so a grown card cannot
// visually collide with its neighbours mid-transition.
constexpr float kGridHoverScaleAmount = 0.06f;
constexpr float kGridHoverScaleEaseRate = 14.0f;

constexpr float kListThumbSizeBase = 56.0f;
constexpr float kListThumbSizeMax = 96.0f;
constexpr float kListRowPaddingV = 14.0f;
constexpr float kListPadding = 16.0f;
constexpr float kListGap = 8.0f;

constexpr float kModeSwitcherHoldDuration = 0.7f;
constexpr float kModeSwitcherEaseRate = 18.0f;
constexpr float kModeSwitcherWidth = 150.0f;
constexpr float kModeSwitcherPadding = 6.0f;
constexpr float kModeSwitcherRadius = 10.0f;
constexpr float kModeSwitcherMargin = 16.0f;
constexpr float kModeSwitcherSlidePixels = 8.0f;
constexpr float kModeSwitcherRowIconSize = 24.0f;
constexpr float kModeSwitcherRowIconGap = 10.0f;
constexpr float kModeSwitcherRowContentInsetX = 12.0f;

// The vertical percentage slider down the panel's right edge. The live percentage text is
// itself the moving indicator, in a small pill. The bottom is 0% and the top is 100%.
constexpr float kModeSwitcherTrackColumnMin = 26.0f;
constexpr float kModeSwitcherTrackWidth = 4.0f;
constexpr float kModeSwitcherIndicatorHeight = 18.0f;
constexpr float kModeSwitcherIndicatorPaddingX = 7.0f;

constexpr float kStatusBarIconSize = 16.0f;
constexpr float kStatusBarIconGap = 8.0f;
constexpr float kStatusBarPadRight = 14.0f;

// Ascent alone puts the baseline too low, since it ignores how far descenders reach back up.
// The nudge closes the last couple of pixels; it is empirical, not font-exact.
constexpr float kBaselineVisualNudge = 2.0f;

constexpr Color kColorBackground{18, 18, 20, 255};
constexpr Color kColorWhite{255, 255, 255, 255};
constexpr Color kColorScrollThumb{160, 160, 168, 200};
constexpr Color kColorListRowBg{32, 32, 36, 220};
constexpr Color kColorListRowBgHover{42, 42, 47, 220};
constexpr Color kColorListText{232, 232, 236, 255};
constexpr Color kColorStatusBarIcon{175, 175, 182, 255};
constexpr Color kColorStatusBarText{158, 158, 166, 255};

constexpr Color kCardNeutralBorder{90, 90, 96, 160};
constexpr Color kCardHighlightBorder{255, 255, 255, 235};

constexpr Color kModeSwitcherBg{26, 26, 30, 255};
constexpr Color kModeSwitcherBorder{58, 58, 64, 255};
constexpr Color kModeSwitcherText{190, 190, 196, 255};
constexpr Color kModeSwitcherTextActive{240, 240, 244, 255};
constexpr Color kModeSwitcherTrackBg{58, 58, 64, 255};
constexpr Color kModeSwitcherTickColor{118, 118, 126, 190};

// The active row's highlight stays a plain neutral box: the accent belongs on the bar beside
// it and on the zoom readout, not on the surface behind the whole row. Tinting this too made
// the selected row the loudest thing on screen.
constexpr Color kModeSwitcherActiveRowFill{52, 52, 58, 255};

// Carousel at stop 0, then Grid and List across three stops each.
constexpr i32 kGridZoomStopFirst = 1;
constexpr i32 kGridZoomStopLast = 3;
constexpr i32 kListZoomStopFirst = 4;

// Listed in the same direction as the slider - bottom is 0% and Carousel, top is 100% and
// List - rather than in enum order. Shared by drawing and the row hit-test.
constexpr ECarouselViewMode kModeSwitcherRowMode[kCarouselViewModeCount]{
	ECarouselViewMode::List,
	ECarouselViewMode::Grid,
	ECarouselViewMode::Carousel,
};

// Each mode's base stop, never a grown one: clicking a row means "switch to this mode", not
// "jump to whatever zoom within it".
constexpr i32 kModeSwitcherRowStop[kCarouselViewModeCount]{
	kListZoomStopFirst,
	kGridZoomStopFirst,
	0,
};

// The one mapping every stop change goes through to detect when it crosses into another mode.
ECarouselViewMode ZoomStopViewMode(i32 stop)
{
	if (stop <= 0) return ECarouselViewMode::Carousel;

	if (stop <= kGridZoomStopLast) return ECarouselViewMode::Grid;

	return ECarouselViewMode::List;
}

// A stop's position on the continuous 0..100 scale, evenly spaced.
float ZoomStopPercent(i32 stop)
{
	return static_cast<float>(stop) / static_cast<float>(kCarouselZoomStopCount - 1) * 100.0f;
}

// 0 where the given band begins, 1 at its own maximum.
float ZoomTWithin(float zoomPercent, i32 firstStop, i32 lastStop)
{
	const float lo = ZoomStopPercent(firstStop);
	const float hi = ZoomStopPercent(lastStop);

	return std::clamp((zoomPercent - lo) / (hi - lo), 0.0f, 1.0f);
}

float GridZoomT(float zoomPercent)
{
	return ZoomTWithin(zoomPercent, kGridZoomStopFirst, kGridZoomStopLast);
}

float ListZoomT(float zoomPercent)
{
	return ZoomTWithin(zoomPercent, kListZoomStopFirst, kCarouselZoomStopCount - 1);
}

float GridCardWidthFor(float zoomPercent)
{
	return kGridCardWidthBase + (kGridCardWidthMax - kGridCardWidthBase) * GridZoomT(zoomPercent);
}

float GridCardHeightFor(float zoomPercent)
{
	return kGridCardHeightBase + (kGridCardHeightMax - kGridCardHeightBase) * GridZoomT(zoomPercent);
}

float ListThumbSizeFor(float zoomPercent)
{
	return kListThumbSizeBase + (kListThumbSizeMax - kListThumbSizeBase) * ListZoomT(zoomPercent);
}

float ListRowHeightFor(float zoomPercent)
{
	return ListThumbSizeFor(zoomPercent) + kListRowPaddingV * 2.0f;
}

u32 GridColumnsForWidth(float areaW, float zoomPercent)
{
	const float cardW = GridCardWidthFor(zoomPercent);
	const float usable = areaW - kGridPadding * 2.0f + kGridGap;

	return std::max<u32>(1, static_cast<u32>(usable / (cardW + kGridGap)));
}

// Falls off to a floor scale within two slots either side of centre, so the centred card reads
// clearly larger and its neighbours recede.
float CardScaleAtDistance(float distance)
{
	const float closeness = std::max(0.0f, 1.0f - std::min(distance, 2.0f) / 2.0f);

	return 0.90f + 0.28f * closeness;
}

// The horizontal offset from the centre slot, accounting for each card's actual scaled width
// rather than a fixed stride - which is what keeps the edge-to-edge gap constant regardless of
// distance from centre. The scale is piecewise-linear in distance, so a trapezoid rule is
// exact here rather than an approximation.
float CumulativeSlotOffset(float slot)
{
	constexpr i32 kIntegrationSteps = 24;

	const float sign = slot < 0.0f ? -1.0f : 1.0f;
	const float magnitude = std::fabs(slot);
	if (magnitude < 0.0001f) return 0.0f;

	const float step = magnitude / static_cast<float>(kIntegrationSteps);
	float integral = 0.0f;
	float previousWidth = kCardWidth * CardScaleAtDistance(0.0f);

	for (i32 i = 1; i <= kIntegrationSteps; i += 1) {
		const float width = kCardWidth * CardScaleAtDistance(step * static_cast<float>(i));
		integral += (previousWidth + width) * 0.5f * step;
		previousWidth = width;
	}

	return sign * (integral + magnitude * kCardSpacing);
}

// A card's visual state. The border itself is always plain white, never game-coloured - only
// the glow hugging the card uses the game's accent. `strong`, meaning Carousel's centred card,
// gets a slightly larger and brighter glow than a plain hover.
struct CardVisualState {
	Color BorderColor;
	float BorderThickness;
	float GlowSize; // pixels past the card the glow quad extends; 0 for none
	u8 GlowAlpha;
};

CardVisualState CardVisualStateFor(bool highlighted, bool strong)
{
	if (!highlighted) return CardVisualState{kCardNeutralBorder, 2.0f, 0.0f, 0};

	if (strong) return CardVisualState{kCardHighlightBorder, 2.5f, 18.0f, 255};

	return CardVisualState{kCardHighlightBorder, 2.0f, 14.0f, 225};
}

std::string_view ViewModeName(ECarouselViewMode mode)
{
	switch (mode) {
		case ECarouselViewMode::Carousel:
			return "Carousel";
		case ECarouselViewMode::Grid:
			return "Grid";
		case ECarouselViewMode::List:
			return "List";
	}

	return "";
}

// Tints the white-on-transparent source rather than needing separate active and inactive art,
// the same recolour-on-draw trick every other embedded icon here uses.
void DrawModeGlyph(CDrawList &drawList, const CAssetManager &assets, ECarouselViewMode mode, Rect box, Color color)
{
	EAsset asset = EAsset::IconCarousel;
	switch (mode) {
		case ECarouselViewMode::Carousel:
			asset = EAsset::IconCarousel;
			break;
		case ECarouselViewMode::Grid:
			asset = EAsset::IconGrid;
			break;
		case ECarouselViewMode::List:
			asset = EAsset::IconList;
			break;
	}

	const CTexture *pTexture = assets.Get(asset);
	if (pTexture != nullptr) {
		drawList.AddRectRoundedTextured(box.X, box.Y, box.W, box.H, kCornerRadiiNone, pTexture, color);
	}
}

// Centring a small icon on the raw geometric centre reads as sitting above the text, since a
// glyph's cap height sits above that centre rather than straddling it.
float IconCenterYFor(float areaCenterY, float ascentValue)
{
	return areaCenterY + ascentValue * 0.15f;
}

// %.*s rather than %s: the view mode's name is not guaranteed null-terminated. ASCII only,
// since the font only bakes 32..126 and anything else would simply be skipped.
std::string_view FormatStatusBarText(ECarouselViewMode mode, char *pBuffer, usize bufferSize)
{
	const std::string_view name = ViewModeName(mode);
	const int written =
		std::snprintf(pBuffer, bufferSize, "%.*s  -  Ctrl+Scroll to zoom", static_cast<int>(name.size()), name.data());

	return std::string_view{pBuffer, written > 0 ? static_cast<u64>(written) : 0};
}

// Widens the track column if the widest possible indicator text would not otherwise fit: a
// fixed column does not scale with the font size, and a large enough one would spill the pill
// past the panel's border.
float ModeSwitcherTrackColumnWidthFor(const CFont &secondary)
{
	const float indicatorW = TextWidth(secondary, "100%") + kModeSwitcherIndicatorPaddingX * 2.0f;

	return std::max(kModeSwitcherTrackColumnMin, indicatorW + 6.0f);
}

// The inverse of the thumb-position math, with the track's top as the 100% end.
i32 ZoomStopForTrackPosition(Rect track, float y)
{
	const float t = std::clamp(1.0f - (y - track.Y) / track.H, 0.0f, 1.0f);

	return static_cast<i32>(std::round(t * static_cast<float>(kCarouselZoomStopCount - 1)));
}
} // namespace

CCarousel::CCarousel(const CFontManager &fonts, const CAssetManager &assets)
	: m_fonts(fonts)
	, m_assets(assets)
{
}

void CCarousel::AddBanner(std::string_view title, CTexture *pTexture, CTexture *pIcon, Color accent)
{
	assert(m_nBannerCount < kCarouselMaxBanners);

	Banner &banner = m_aBanners[m_nBannerCount];
	banner.Title = title;
	banner.pTexture = pTexture;
	banner.pIcon = pIcon;
	banner.Accent = accent;
	banner.AccountCount = 0;
	banner.TextureAspect = pTexture != nullptr && pTexture->GetHeight() > 0
							   ? static_cast<float>(pTexture->GetWidth()) / static_cast<float>(pTexture->GetHeight())
							   : 1.0f;

	m_nBannerCount += 1;
}

void CCarousel::AddAccount(u32 bannerIndex, std::string_view username, std::string_view note, std::string_view password)
{
	assert(bannerIndex < m_nBannerCount);

	Banner &banner = m_aBanners[bannerIndex];
	assert(banner.AccountCount < kCarouselMaxAccountsPerBanner);

	banner.Accounts[banner.AccountCount].Init(username, note, password);
	banner.AccountCount += 1;
}

void CCarousel::UpdateAccount(u32 bannerIndex, u32 accountIndex, std::string_view username, std::string_view note,
							  std::string_view password)
{
	assert(bannerIndex < m_nBannerCount);

	Banner &banner = m_aBanners[bannerIndex];
	assert(accountIndex < banner.AccountCount);

	banner.Accounts[accountIndex].Init(username, note, password);
}

void CCarousel::RemoveAccount(u32 bannerIndex, u32 accountIndex)
{
	assert(bannerIndex < m_nBannerCount);

	Banner &banner = m_aBanners[bannerIndex];
	assert(accountIndex < banner.AccountCount);

	for (u32 i = accountIndex; i + 1 < banner.AccountCount; i += 1) {
		banner.Accounts[i] = banner.Accounts[i + 1];
	}

	banner.AccountCount -= 1;
}

u32 CCarousel::GetVisibleAccounts(u32 bannerIndex, VisibleAccountRef *pOut) const
{
	if (bannerIndex >= m_nBannerCount) return 0;

	u32 count = 0;

	const Banner &own = m_aBanners[bannerIndex];
	for (u32 i = 0; i < own.AccountCount; i += 1) {
		pOut[count] = VisibleAccountRef{bannerIndex, i};
		count += 1;
	}

	for (u32 b = 0; b < m_nBannerCount; b += 1) {
		if (b == bannerIndex) continue;

		const Banner &other = m_aBanners[b];
		for (u32 i = 0; i < other.AccountCount; i += 1) {
			if ((other.Accounts[i].GetEffectiveVisibleMask(b) & (1u << bannerIndex)) != 0) {
				pOut[count] = VisibleAccountRef{b, i};
				count += 1;
			}
		}
	}

	assert(count <= kCarouselMaxVisibleAccounts);

	return count;
}

float CCarousel::ClampTarget(float value) const
{
	if (m_nBannerCount == 0) return 0.0f;

	return std::clamp(value, 0.0f, static_cast<float>(m_nBannerCount - 1));
}

Rect CCarousel::GetBannerRect(u32 index) const
{
	const float slot = static_cast<float>(index) - m_flScrollOffset;
	const float scale = CardScaleAtDistance(std::fabs(slot));
	const float w = kCardWidth * scale;
	const float h = kCardHeight * scale;
	const float cx = m_vecBounds.X + m_vecBounds.W * 0.5f + CumulativeSlotOffset(slot);

	return Rect{cx - w * 0.5f, m_vecBounds.Y + (m_vecBounds.H - h) * 0.5f, w, h};
}

Rect CCarousel::GridBannerRect(u32 index) const
{
	const float cardW = GridCardWidthFor(m_flZoomPercent);
	const float cardH = GridCardHeightFor(m_flZoomPercent);
	const u32 columns = GridColumnsForWidth(m_vecBounds.W, m_flZoomPercent);
	const u32 col = index % columns;
	const u32 row = index / columns;

	const float totalWidth = static_cast<float>(columns) * cardW + static_cast<float>(columns - 1) * kGridGap;
	const float startX = m_vecBounds.X + (m_vecBounds.W - totalWidth) * 0.5f;
	const float y =
		m_vecBounds.Y + kGridPadding + static_cast<float>(row) * (cardH + kGridGap) - m_wrapScroll.m_flScrollOffset;

	return Rect{startX + static_cast<float>(col) * (cardW + kGridGap), y, cardW, cardH};
}

float CCarousel::GridContentHeight() const
{
	if (m_nBannerCount == 0) return 0.0f;

	const u32 columns = GridColumnsForWidth(m_vecBounds.W, m_flZoomPercent);
	const u32 rows = (m_nBannerCount + columns - 1) / columns;
	const float cardH = GridCardHeightFor(m_flZoomPercent);

	return kGridPadding * 2.0f + static_cast<float>(rows) * cardH + static_cast<float>(rows - 1) * kGridGap;
}

Rect CCarousel::ListBannerRect(u32 index) const
{
	const float rowHeight = ListRowHeightFor(m_flZoomPercent);
	const float y = m_vecBounds.Y + kListPadding + static_cast<float>(index) * (rowHeight + kListGap) -
					m_wrapScroll.m_flScrollOffset;

	return Rect{m_vecBounds.X + kListPadding, y, m_vecBounds.W - kListPadding * 2.0f, rowHeight};
}

float CCarousel::ListContentHeight() const
{
	if (m_nBannerCount == 0) return 0.0f;

	const float rowHeight = ListRowHeightFor(m_flZoomPercent);

	return kListPadding * 2.0f + static_cast<float>(m_nBannerCount) * rowHeight +
		   static_cast<float>(m_nBannerCount - 1) * kListGap;
}

float CCarousel::WrapContentHeight(ECarouselViewMode mode) const
{
	switch (mode) {
		case ECarouselViewMode::Grid:
			return GridContentHeight();
		case ECarouselViewMode::List:
			return ListContentHeight();
		case ECarouselViewMode::Carousel:
			break;
	}

	return 0.0f;
}

Rect CCarousel::ScrollbarTrackRect() const
{
	return Rect{m_vecBounds.X + m_vecBounds.W - kScrollbarWidth - 8.0f, m_vecBounds.Y + 8.0f, kScrollbarWidth,
				m_vecBounds.H - 16.0f};
}

// The bounds already stop exactly where the status bar starts, since the owner sets both from
// the same window height, so the strip's rect falls straight out of that.
Rect CCarousel::StatusBarRect() const
{
	return Rect{m_vecBounds.X, m_vecBounds.Y + m_vecBounds.H, m_vecBounds.W, kStatusBarHeight};
}

// The one geometry function drawing and hit-testing share, so hovering the indicator can never
// disagree with where it actually is.
Rect CCarousel::StatusBarContentRect() const
{
	const Rect statusBar = StatusBarRect();

	char buffer[48];
	const std::string_view text = FormatStatusBarText(m_viewMode, buffer, sizeof(buffer));
	const float contentW = kStatusBarIconSize + kStatusBarIconGap + TextWidth(m_fonts.GetSecondary(), text);

	return Rect{statusBar.X + statusBar.W - kStatusBarPadRight - contentW, statusBar.Y, contentW, statusBar.H};
}

float CCarousel::ModeSwitcherTrackColumnWidth() const
{
	return ModeSwitcherTrackColumnWidthFor(m_fonts.GetSecondary());
}

Rect CCarousel::ModeSwitcherPanelRect() const
{
	const float rowsH = (m_fonts.GetBody().GetLineHeight() + 10.0f) * static_cast<float>(kCarouselViewModeCount);
	const float h = kModeSwitcherPadding * 2.0f + rowsH;
	const float w = kModeSwitcherWidth + ModeSwitcherTrackColumnWidth();

	return Rect{m_vecBounds.X + m_vecBounds.W - w - kModeSwitcherMargin,
				m_vecBounds.Y + m_vecBounds.H - h - kModeSwitcherMargin, w, h};
}

Rect CCarousel::ModeSwitcherRowRect(Rect panel, u32 index) const
{
	const float rowHeight = m_fonts.GetBody().GetLineHeight() + 10.0f;

	return Rect{panel.X + kModeSwitcherPadding, panel.Y + kModeSwitcherPadding + rowHeight * static_cast<float>(index),
				kModeSwitcherWidth - kModeSwitcherPadding * 2.0f, rowHeight};
}

Rect CCarousel::ModeSwitcherTrackRect(Rect panel) const
{
	const float trackColumn = ModeSwitcherTrackColumnWidth();

	return Rect{
		panel.X + panel.W - trackColumn * 0.5f - kModeSwitcherTrackWidth * 0.5f,
		panel.Y + kModeSwitcherPadding + kModeSwitcherIndicatorHeight * 0.5f,
		kModeSwitcherTrackWidth,
		panel.H - kModeSwitcherPadding * 2.0f - kModeSwitcherIndicatorHeight,
	};
}

Rect CCarousel::ModeSwitcherTrackGrabRect(Rect panel) const
{
	const Rect track = ModeSwitcherTrackRect(panel);

	return Rect{track.X - 10.0f, panel.Y, track.W + 20.0f, panel.H};
}

Rect CCarousel::ModeSwitcherHoverRect() const
{
	const Rect panel = ModeSwitcherPanelRect();
	const Rect indicator = StatusBarContentRect();

	const float left = std::min(panel.X, indicator.X);
	const float right = std::max(panel.X + panel.W, indicator.X + indicator.W);

	return Rect{left, panel.Y, right - left, indicator.Y + indicator.H - panel.Y};
}

bool CCarousel::IsMouseOverModeSwitcher(float mouseX, float mouseY) const
{
	// The status-bar indicator alone is what opens the flyout from cold: hovering where the
	// not-yet-visible panel would appear should not summon it out of nowhere. Once it is
	// opening, the broader union keeps it open across the gap between the two.
	if (RectContainsPoint(StatusBarContentRect(), mouseX, mouseY)) return true;

	return IsModeSwitcherVisible() && RectContainsPoint(ModeSwitcherHoverRect(), mouseX, mouseY);
}

void CCarousel::SetZoomStop(i32 stop)
{
	stop = std::clamp(stop, 0, kCarouselZoomStopCount - 1);

	if (stop != m_nZoomStop) {
		m_nZoomStop = stop;

		const ECarouselViewMode nextMode = ZoomStopViewMode(stop);
		if (nextMode != m_viewMode) {
			m_transitionFromMode = m_viewMode;
			m_viewMode = nextMode;
			m_flTransitionAmount = 1.0f;

			// The new mode's content height has nothing to do with the old one's position.
			m_wrapScroll = CScrollable{};
		}
	}

	m_flModeSwitcherHoldSeconds = kModeSwitcherHoldDuration;
}

void CCarousel::AdjustZoomStop(float wheelDelta)
{
	if (wheelDelta == 0.0f) return;

	// Scrolling up moves forward through the stops, starting from Carousel.
	SetZoomStop(m_nZoomStop + (wheelDelta > 0.0f ? 1 : -1));
}

void CCarousel::ApplyZoomStop(i32 stop)
{
	stop = std::clamp(stop, 0, kCarouselZoomStopCount - 1);

	m_nZoomStop = stop;
	m_flZoomPercent = ZoomStopPercent(stop);
	m_viewMode = ZoomStopViewMode(stop);
	m_transitionFromMode = m_viewMode;
	m_flTransitionAmount = 0.0f;
	m_nFocusedIndex = GetSelectedIndex();
}

void CCarousel::ApplySelectedIndex(i32 index)
{
	const float target = ClampTarget(static_cast<float>(index));

	m_flScrollOffset = target;
	m_flTargetScrollOffset = target;
}

bool CCarousel::ModeSwitcherOnPointerDown(float x, float y)
{
	if (!IsModeSwitcherVisible()) return false;

	const Rect panel = ModeSwitcherPanelRect();
	if (!RectContainsPoint(panel, x, y)) return false;

	m_bModeSwitcherPointerCaptured = true;

	for (u32 i = 0; i < kCarouselViewModeCount; i += 1) {
		if (RectContainsPoint(ModeSwitcherRowRect(panel, i), x, y)) {
			SetZoomStop(kModeSwitcherRowStop[i]);
			return true;
		}
	}

	if (RectContainsPoint(ModeSwitcherTrackGrabRect(panel), x, y)) {
		m_modeSwitcherDrag.Begin(x, y);
		SetZoomStop(ZoomStopForTrackPosition(ModeSwitcherTrackRect(panel), y));
		return true;
	}

	// Somewhere inside the panel's padding - swallow rather than fall through.
	return true;
}

bool CCarousel::ModeSwitcherOnPointerMove(float x, float y)
{
	if (!m_modeSwitcherDrag.IsPressed()) return false;

	m_modeSwitcherDrag.Update(x, y);
	SetZoomStop(ZoomStopForTrackPosition(ModeSwitcherTrackRect(ModeSwitcherPanelRect()), y));

	return true;
}

bool CCarousel::ModeSwitcherOnPointerUp()
{
	const bool wasCaptured = m_bModeSwitcherPointerCaptured;

	m_bModeSwitcherPointerCaptured = false;
	m_modeSwitcherDrag.End();

	return wasCaptured;
}

bool CCarousel::OnPointerDown(float x, float y)
{
	m_bKeyboardFocusVisible = false;

	if (ModeSwitcherOnPointerDown(x, y)) return true;

	// Card dragging is Carousel mode's thing; the only draggable surface in the other two
	// is the scrollbar thumb.
	if (m_viewMode != ECarouselViewMode::Carousel) {
		return m_wrapScroll.OnPointerDown(x, y, ScrollbarTrackRect(), WrapContentHeight(m_viewMode), m_vecBounds.H);
	}

	m_cardDrag.Begin(x, y);
	m_flDragStartScrollOffset = m_flScrollOffset;

	return true;
}

bool CCarousel::OnPointerMove(float x, float y)
{
	// Any real pointer movement retires the keyboard focus - see m_bKeyboardFocusVisible.
	m_bKeyboardFocusVisible = false;

	if (ModeSwitcherOnPointerMove(x, y)) return true;

	if (m_viewMode != ECarouselViewMode::Carousel) {
		m_wrapScroll.OnPointerMove(y, ScrollbarTrackRect(), WrapContentHeight(m_viewMode), m_vecBounds.H);
		return m_wrapScroll.IsDragging();
	}

	if (!m_cardDrag.IsPressed()) return false;

	m_cardDrag.Update(x, y);

	// 1:1 tracking while dragging; easing only takes over after release.
	const float newOffset = m_flDragStartScrollOffset - m_cardDrag.DeltaX() / kCardStride;
	m_flScrollOffset = newOffset;
	m_flTargetScrollOffset = newOffset;

	return true;
}

bool CCarousel::OnPointerUp(float x, float y)
{
	m_bKeyboardFocusVisible = false;

	if (ModeSwitcherOnPointerUp()) return true;

	if (m_viewMode != ECarouselViewMode::Carousel) {
		// A release ending a scrollbar drag must not also read as a card click.
		if (m_wrapScroll.IsDragging()) {
			m_wrapScroll.OnPointerUp();
			return true;
		}

		// No drag or selection state to update here; the owner opens the modal on any hit.
		m_pendingClickBanner = PendingHitFromHitTest(HitTest(x, y));

		return true;
	}

	if (!m_cardDrag.IsPressed()) return false;

	const bool dragMoved = m_cardDrag.HasMoved();
	m_cardDrag.End();

	if (dragMoved) {
		m_flTargetScrollOffset = ClampTarget(std::round(m_flScrollOffset));
		return true;
	}

	// A click rather than a drag: only change the selection if it landed on a banner.
	const i32 hitIndex = HitTest(x, y);
	if (hitIndex >= 0) {
		m_flTargetScrollOffset = ClampTarget(static_cast<float>(hitIndex));
	}

	m_pendingClickBanner = PendingHitFromHitTest(hitIndex);

	return true;
}

PendingHit CCarousel::ConsumePendingClick()
{
	const PendingHit pending = m_pendingClickBanner;
	m_pendingClickBanner = PendingHit{};

	return pending;
}

PendingHit CCarousel::ConsumePendingActivate()
{
	const PendingHit pending = m_pendingActivateBanner;
	m_pendingActivateBanner = PendingHit{};

	return pending;
}

// In Carousel mode the focus and the centred card are the same thing, so moving one moves the
// other. Grid and List have no notion of a centred card, so there the focus is only a ring, and
// the view scrolls to keep it on screen.
i32 CCarousel::FocusedIndex() const
{
	return m_viewMode == ECarouselViewMode::Carousel ? GetSelectedIndex() : m_nFocusedIndex;
}

bool CCarousel::IsFocusHighlighted(u32 index) const
{
	return m_bKeyboardFocusVisible && m_viewMode != ECarouselViewMode::Carousel &&
		   static_cast<i32>(index) == FocusedIndex();
}

void CCarousel::MoveFocus(i32 delta)
{
	if (m_nBannerCount == 0) return;

	// Set here rather than in OnKeyDown, so it only turns on for a key that actually moved the
	// focus - a key the carousel declines leaves it as it was.
	m_bKeyboardFocusVisible = true;

	const auto lastIndex = static_cast<i32>(m_nBannerCount) - 1;
	m_nFocusedIndex = std::clamp(FocusedIndex() + delta, 0, lastIndex);

	// In Carousel mode the centred card is the focus, so moving one is moving the other and there
	// is nothing separate to remember - FocusedIndex reads it back out of the scroll target.
	if (m_viewMode == ECarouselViewMode::Carousel) {
		m_flTargetScrollOffset = ClampTarget(static_cast<float>(m_nFocusedIndex));
		return;
	}

	ScrollFocusIntoView();
}

void CCarousel::ScrollFocusIntoView()
{
	const Rect card =
		m_viewMode == ECarouselViewMode::Grid ? GridBannerRect(m_nFocusedIndex) : ListBannerRect(m_nFocusedIndex);

	// Measured against where the view is heading, not where it currently is: holding an arrow key
	// would otherwise correct against a still-moving offset and overshoot a little on every press.
	const float settleShift = m_wrapScroll.m_flScrollOffset - m_wrapScroll.m_flTargetScrollOffset;
	const float cardTop = card.Y + settleShift;
	const float cardBottom = cardTop + card.H;

	const float above = m_vecBounds.Y + kGridPadding - cardTop;
	const float below = cardBottom - (m_vecBounds.Y + m_vecBounds.H - kGridPadding);

	if (above > 0.0f) {
		m_wrapScroll.ScrollBy(-above, WrapContentHeight(m_viewMode), m_vecBounds.H);
	} else if (below > 0.0f) {
		m_wrapScroll.ScrollBy(below, WrapContentHeight(m_viewMode), m_vecBounds.H);
	}
}

// Deliberately silent on anything else: this widget sits at the bottom of the stack, so consuming
// a key it has no use for would take it from whatever is above.
bool CCarousel::OnKeyDown(u32 keyCode)
{
	if (!m_bVisible || m_nBannerCount == 0) return false;

	// Which arrows move the focus follows what is on screen: a horizontal strip reads along, a
	// grid reads both ways, and a list reads down.
	const u32 columns = GridColumnsForWidth(m_vecBounds.W, m_flZoomPercent);

	switch (keyCode) {
		case VK_LEFT:
			if (m_viewMode == ECarouselViewMode::List) return false;

			MoveFocus(-1);
			return true;

		case VK_RIGHT:
			if (m_viewMode == ECarouselViewMode::List) return false;

			MoveFocus(1);
			return true;

		case VK_UP:
			if (m_viewMode == ECarouselViewMode::Carousel) return false;

			MoveFocus(m_viewMode == ECarouselViewMode::Grid ? -static_cast<i32>(columns) : -1);
			return true;

		case VK_DOWN:
			if (m_viewMode == ECarouselViewMode::Carousel) return false;

			MoveFocus(m_viewMode == ECarouselViewMode::Grid ? static_cast<i32>(columns) : 1);
			return true;

		case VK_RETURN:
			// Grid and List only open on Enter once the focus is actually on screen. Before that
			// the first press reveals it, so Enter can never open a card the user could not see
			// was focused - Carousel is exempt, since its centred card is always visibly the one.
			if (m_viewMode != ECarouselViewMode::Carousel && !m_bKeyboardFocusVisible) {
				m_bKeyboardFocusVisible = true;
				ScrollFocusIntoView();
				return true;
			}

			m_pendingActivateBanner = PendingHitIndex(FocusedIndex());
			return true;

		default:
			return false;
	}
}

bool CCarousel::OnScroll(float x, float y, float wheelDelta)
{
	m_bKeyboardFocusVisible = false;

	// Scrolling up moves toward later cards, the opposite of the vertical modes below. A wheel
	// notch here reads as "advance the strip", not as "move the viewport up".
	if (m_viewMode == ECarouselViewMode::Carousel) {
		m_flTargetScrollOffset = ClampTarget(m_flTargetScrollOffset + wheelDelta);
	} else {
		m_wrapScroll.OnScroll(wheelDelta, WrapContentHeight(m_viewMode), m_vecBounds.H);
	}

	return true;
}

i32 CCarousel::HitTest(float x, float y) const
{
	if (m_viewMode != ECarouselViewMode::Carousel) {
		for (u32 i = 0; i < m_nBannerCount; i += 1) {
			const Rect rect = m_viewMode == ECarouselViewMode::Grid ? GridBannerRect(i) : ListBannerRect(i);

			// The second test rejects a row scrolled out of the visible area.
			if (RectContainsPoint(rect, x, y) && y >= m_vecBounds.Y && y < m_vecBounds.Y + m_vecBounds.H) {
				return static_cast<i32>(i);
			}
		}

		return -1;
	}

	// Carousel cards overlap, so the most centred candidate wins - matching what is visually
	// on top.
	i32 bestIndex = -1;
	float bestDistance = 0.0f;

	for (u32 i = 0; i < m_nBannerCount; i += 1) {
		if (!RectContainsPoint(GetBannerRect(i), x, y)) continue;

		const float distance = std::fabs(static_cast<float>(i) - m_flScrollOffset);
		if (bestIndex < 0 || distance < bestDistance) {
			bestIndex = static_cast<i32>(i);
			bestDistance = distance;
		}
	}

	return bestIndex;
}

ECursorKind CCarousel::GetDesiredCursor() const
{
	const bool draggingCards = m_viewMode == ECarouselViewMode::Carousel && m_cardDrag.IsPressed();
	const bool draggingScrollbar = m_viewMode != ECarouselViewMode::Carousel && m_wrapScroll.IsDragging();

	if (m_modeSwitcherDrag.IsPressed() || draggingCards || draggingScrollbar) return ECursorKind::Drag;

	if (m_nBannerCount > 0 && RectContainsPoint(StatusBarContentRect(), m_flMouseX, m_flMouseY)) {
		return ECursorKind::Hand;
	}

	if (IsModeSwitcherVisible()) {
		const Rect panel = ModeSwitcherPanelRect();

		if (RectContainsPoint(panel, m_flMouseX, m_flMouseY)) {
			for (u32 i = 0; i < kCarouselViewModeCount; i += 1) {
				if (RectContainsPoint(ModeSwitcherRowRect(panel, i), m_flMouseX, m_flMouseY)) {
					return ECursorKind::Hand;
				}
			}

			if (RectContainsPoint(ModeSwitcherTrackGrabRect(panel), m_flMouseX, m_flMouseY)) {
				return ECursorKind::Hand;
			}
		}
	}

	if (HitTest(m_flMouseX, m_flMouseY) >= 0) return ECursorKind::Hand;

	if (m_viewMode != ECarouselViewMode::Carousel) {
		const float contentHeight = WrapContentHeight(m_viewMode);

		if (CScrollable::IsVisible(contentHeight, m_vecBounds.H) &&
			RectContainsPoint(ScrollbarTrackRect(), m_flMouseX, m_flMouseY)) {
			return ECursorKind::Hand;
		}
	}

	return ECursorKind::Arrow;
}

void CCarousel::Update(float deltaSeconds)
{
	m_flScrollOffset = CAnimator::EaseToward(m_flScrollOffset, m_flTargetScrollOffset, kEaseRate, deltaSeconds);
	m_flTransitionAmount = CAnimator::EaseToward(m_flTransitionAmount, 0.0f, kViewModeTransitionEaseRate, deltaSeconds);
	m_flZoomPercent =
		CAnimator::EaseToward(m_flZoomPercent, ZoomStopPercent(m_nZoomStop), kZoomPercentEaseRate, deltaSeconds);

	if (m_flTransitionAmount < 0.002f) {
		m_flTransitionAmount = 0.0f;
	}

	m_wrapScroll.Update(deltaSeconds);

	// Only one card ever hit-tests as hovered, but every card's scale eases independently, so
	// an outgoing card shrinks back while the incoming one grows.
	const i32 gridHoveredIndex = m_viewMode == ECarouselViewMode::Grid ? HitTest(m_flMouseX, m_flMouseY) : -1;

	for (u32 i = 0; i < m_nBannerCount; i += 1) {
		// The keyboard focus pops the same way hovering does, or arrowing onto a card would light it
		// up without the growth that every hovered card has.
		const bool focused = static_cast<i32>(i) == gridHoveredIndex || IsFocusHighlighted(i);
		const float target = focused ? 1.0f : 0.0f;
		m_aGridHoverScale[i] =
			CAnimator::EaseToward(m_aGridHoverScale[i], target, kGridHoverScaleEaseRate, deltaSeconds);
	}

	if (IsMouseOverModeSwitcher(m_flMouseX, m_flMouseY)) {
		m_flModeSwitcherHoldSeconds = kModeSwitcherHoldDuration;
	} else {
		m_flModeSwitcherHoldSeconds = std::max(0.0f, m_flModeSwitcherHoldSeconds - deltaSeconds);
	}

	const float switcherTarget = m_flModeSwitcherHoldSeconds > 0.0f ? 1.0f : 0.0f;
	m_flModeSwitcherVisibleAmount =
		CAnimator::EaseToward(m_flModeSwitcherVisibleAmount, switcherTarget, kModeSwitcherEaseRate, deltaSeconds);

	if (m_flModeSwitcherVisibleAmount < 0.002f) {
		m_flModeSwitcherVisibleAmount = 0.0f;
	}
}

i32 CCarousel::GetSelectedIndex() const
{
	return static_cast<i32>(ClampTarget(std::round(m_flTargetScrollOffset)));
}

// A rounded stroke is not a primitive here, so the border is a full-size rounded rect with the
// smaller-radius art drawn on top, inset by the border thickness. The glow quad extends past
// the card on every side; the shader builds its distance field from the card's rect.
void CCarousel::DrawBannerCard(CDrawList &drawList, Rect rect, const Banner &banner, bool highlighted, bool strong,
							   u8 alpha) const
{
	const CardVisualState state = CardVisualStateFor(highlighted, strong);

	if (state.GlowSize > 0.0f && state.GlowAlpha != 0) {
		const Color glowColor = ColorScaleAlpha(ColorScaleAlpha(banner.Accent, state.GlowAlpha), alpha);
		drawList.AddRectRoundedBannerGlow(rect.X, rect.Y, rect.W, rect.H, kCarouselCardCornerRadius, state.GlowSize,
										  glowColor);
	}

	drawList.AddRectRoundedFilled(rect.X, rect.Y, rect.W, rect.H, CDrawList::UniformRadii(kCarouselCardCornerRadius),
								  ColorScaleAlpha(state.BorderColor, alpha));

	if (banner.pTexture == nullptr) return;

	const float innerW = rect.W - state.BorderThickness * 2.0f;
	const float innerH = rect.H - state.BorderThickness * 2.0f;
	const CornerRadii innerRadii = CDrawList::UniformRadii(kCarouselCardCornerRadius - state.BorderThickness);
	const UvRect uv = CDrawList::ComputeCoverUv(innerW, innerH, banner.TextureAspect, 1.0f);

	drawList.AddRectRoundedTexturedUv(rect.X + state.BorderThickness, rect.Y + state.BorderThickness, innerW, innerH,
									  innerRadii, uv.U0, uv.V0, uv.U1, uv.V1, banner.pTexture,
									  ColorScaleAlpha(kColorWhite, alpha));
}

void CCarousel::DrawCarouselMode(CDrawList &drawList, u8 alpha, float yOffset, float mouseX, float mouseY) const
{
	const float areaX = m_vecBounds.X;
	const float areaW = m_vecBounds.W;

	for (u32 i = 0; i < m_nBannerCount; i += 1) {
		Rect rect = GetBannerRect(i);
		rect.Y += yOffset;

		if (rect.X + rect.W < areaX || rect.X > areaX + areaW) continue;

		const bool isSelected = std::fabs(static_cast<float>(i) - m_flScrollOffset) < 0.5f;
		const bool isHovered = !isSelected && RectContainsPoint(rect, mouseX, mouseY);

		DrawBannerCard(drawList, rect, m_aBanners[i], isSelected || isHovered, isSelected, alpha);
	}

	// The bounds span the full window width with no side margin, so a card mid-scroll touches
	// the window edge exactly. A gradient from the clear colour to transparent reads as an
	// intentional vignette rather than a card bleeding into the border.
	constexpr float kEdgeFadeWidth = 64.0f;
	const Color fadeOpaque = ColorScaleAlpha(kColorBackground, alpha);
	const Color fadeClear = ColorScaleAlpha(kColorBackground, 0);

	drawList.AddRectGradientCorners(areaX, m_vecBounds.Y, kEdgeFadeWidth, m_vecBounds.H, fadeOpaque, fadeClear,
									fadeOpaque, fadeClear);
	drawList.AddRectGradientCorners(areaX + areaW - kEdgeFadeWidth, m_vecBounds.Y, kEdgeFadeWidth, m_vecBounds.H,
									fadeClear, fadeOpaque, fadeClear, fadeOpaque);
}

void CCarousel::DrawWrapChrome(CDrawList &drawList, float contentHeight, u8 alpha, float mouseX, float mouseY) const
{
	m_wrapScroll.DrawEdgeFade(drawList, m_vecBounds, contentHeight, m_vecBounds.H,
							  ColorScaleAlpha(kColorBackground, alpha));
	m_wrapScroll.Draw(drawList, ScrollbarTrackRect(), contentHeight, m_vecBounds.H,
					  ColorScaleAlpha(kColorScrollThumb, alpha), mouseX, mouseY);
}

void CCarousel::DrawGridMode(CDrawList &drawList, u8 alpha, float yOffset, float mouseX, float mouseY) const
{
	const Rect region = m_vecBounds;

	drawList.PushClipRect(region);

	for (u32 i = 0; i < m_nBannerCount; i += 1) {
		Rect rect = GridBannerRect(i);
		rect.Y += yOffset;

		if (rect.Y + rect.H < region.Y || rect.Y > region.Y + region.H) continue;

		// No persistent selected look here: the hovered card is highlighted, and a keyboard focus is
		// drawn as one, since to the reader they mean the same thing.
		const bool isHovered = RectContainsPoint(rect, mouseX, mouseY) || IsFocusHighlighted(i);

		// Grows around the card's centre.
		const float hoverScale = 1.0f + kGridHoverScaleAmount * m_aGridHoverScale[i];
		if (hoverScale != 1.0f) {
			const float cx = rect.X + rect.W * 0.5f;
			const float cy = rect.Y + rect.H * 0.5f;
			rect.W *= hoverScale;
			rect.H *= hoverScale;
			rect.X = cx - rect.W * 0.5f;
			rect.Y = cy - rect.H * 0.5f;
		}

		DrawBannerCard(drawList, rect, m_aBanners[i], isHovered, false, alpha);
	}

	drawList.PopClipRect();

	DrawWrapChrome(drawList, GridContentHeight(), alpha, mouseX, mouseY);
}

void CCarousel::DrawListMode(CDrawList &drawList, u8 alpha, float yOffset, float mouseX, float mouseY) const
{
	const Rect region = m_vecBounds;
	const CFont &body = m_fonts.GetBody();
	const float thumbSize = ListThumbSizeFor(m_flZoomPercent);
	const Color imageTint = ColorScaleAlpha(kColorWhite, alpha);

	drawList.PushClipRect(region);

	for (u32 i = 0; i < m_nBannerCount; i += 1) {
		Rect rect = ListBannerRect(i);
		rect.Y += yOffset;

		if (rect.Y + rect.H < region.Y || rect.Y > region.Y + region.H) continue;

		const Banner &banner = m_aBanners[i];
		const bool isHovered = RectContainsPoint(rect, mouseX, mouseY) || IsFocusHighlighted(i);
		const Color rowBg = isHovered ? kColorListRowBgHover : kColorListRowBg;

		drawList.AddRectRoundedFilled(rect.X, rect.Y, rect.W, rect.H, CDrawList::UniformRadii(10.0f),
									  ColorScaleAlpha(rowBg, alpha));

		const Rect thumb{rect.X + 12.0f, rect.Y + (rect.H - thumbSize) * 0.5f, thumbSize, thumbSize};

		if (banner.pIcon != nullptr) {
			// Already square, so a plain 0..1 UV rather than a cover-fit crop.
			drawList.AddRectRoundedTextured(thumb.X, thumb.Y, thumb.W, thumb.H, CDrawList::UniformRadii(10.0f),
											banner.pIcon, imageTint);
		} else if (banner.pTexture != nullptr) {
			// Falls back to a crop of the banner art for a game added without its own icon.
			const UvRect uv = CDrawList::ComputeCoverUv(thumb.W, thumb.H, banner.TextureAspect, 1.0f);
			drawList.AddRectRoundedTexturedUv(thumb.X, thumb.Y, thumb.W, thumb.H, CDrawList::UniformRadii(10.0f), uv.U0,
											  uv.V0, uv.U1, uv.V1, banner.pTexture, imageTint);
		}

		const float baselineY = rect.Y + rect.H * 0.5f + (body.GetAscent() + body.GetDescent()) * 0.5f;
		DrawText(drawList, body, thumb.X + thumb.W + 16.0f, baselineY, banner.Title,
				 ColorScaleAlpha(kColorListText, alpha));
	}

	drawList.PopClipRect();

	DrawWrapChrome(drawList, ListContentHeight(), alpha, mouseX, mouseY);
}

void CCarousel::DrawMode(CDrawList &drawList, ECarouselViewMode mode, u8 alpha, float yOffset, float mouseX,
						 float mouseY) const
{
	switch (mode) {
		case ECarouselViewMode::Carousel:
			DrawCarouselMode(drawList, alpha, yOffset, mouseX, mouseY);
			break;
		case ECarouselViewMode::Grid:
			DrawGridMode(drawList, alpha, yOffset, mouseX, mouseY);
			break;
		case ECarouselViewMode::List:
			DrawListMode(drawList, alpha, yOffset, mouseX, mouseY);
			break;
	}
}

// No background pill of its own: the status bar underneath is the only backing this needs.
void CCarousel::DrawStatusBarContent(CDrawList &drawList) const
{
	if (m_nBannerCount == 0) return;

	char buffer[48];
	const std::string_view text = FormatStatusBarText(m_viewMode, buffer, sizeof(buffer));

	const CFont &secondary = m_fonts.GetSecondary();
	const Rect content = StatusBarContentRect();
	const Rect iconBox{content.X, content.Y + (content.H - kStatusBarIconSize) * 0.5f, kStatusBarIconSize,
					   kStatusBarIconSize};

	DrawModeGlyph(drawList, m_assets, m_viewMode, iconBox, kColorStatusBarIcon);

	const float baselineY =
		content.Y + content.H * 0.5f + (secondary.GetAscent() + secondary.GetDescent()) * 0.5f - kBaselineVisualNudge;
	DrawText(drawList, secondary, iconBox.X + kStatusBarIconSize + kStatusBarIconGap, baselineY, text,
			 kColorStatusBarText);
}

void CCarousel::DrawModeSwitcherRows(CDrawList &drawList, Rect panel, u8 alpha) const
{
	const CFont &body = m_fonts.GetBody();

	for (u32 i = 0; i < kCarouselViewModeCount; i += 1) {
		const ECarouselViewMode mode = kModeSwitcherRowMode[i];
		const Rect row = ModeSwitcherRowRect(panel, i);
		const bool active = mode == m_viewMode;

		// A fixed inset for the icon and text column, well clear of the active row's bar.
		const float contentX = row.X + kModeSwitcherRowContentInsetX;

		if (active) {
			drawList.AddRectRoundedFilled(row.X, row.Y, row.W, row.H, CDrawList::UniformRadii(6.0f),
										  ColorScaleAlpha(kModeSwitcherActiveRowFill, alpha));
			// The bar is the one thing in this row that follows the accent.
			drawList.AddRectRoundedFilled(row.X + 2.0f, row.Y + 3.0f, 3.0f, row.H - 6.0f, CDrawList::UniformRadii(1.5f),
										  ColorScaleAlpha(m_clrAccent, alpha));
		}

		const Color rowContent = ColorScaleAlpha(active ? kModeSwitcherTextActive : kModeSwitcherText, alpha);
		const Rect iconBox{
			contentX,
			IconCenterYFor(row.Y + row.H * 0.5f, body.GetAscent()) - kModeSwitcherRowIconSize * 0.5f,
			kModeSwitcherRowIconSize,
			kModeSwitcherRowIconSize,
		};

		DrawModeGlyph(drawList, m_assets, mode, iconBox, rowContent);

		const float baselineY = row.Y + row.H * 0.5f + (body.GetAscent() + body.GetDescent()) * 0.5f;
		DrawText(drawList, body, contentX + kModeSwitcherRowIconSize + kModeSwitcherRowIconGap, baselineY,
				 ViewModeName(mode), rowContent);
	}
}

// Ticks at every zoom stop, not just the three named modes, so the discrete steps underneath
// the continuous indicator position stay visible.
void CCarousel::DrawModeSwitcherSlider(CDrawList &drawList, Rect panel, u8 alpha) const
{
	const CFont &secondary = m_fonts.GetSecondary();
	const Rect track = ModeSwitcherTrackRect(panel);

	drawList.AddRectRoundedFilled(track.X, track.Y, track.W, track.H, CDrawList::UniformRadii(track.W * 0.5f),
								  ColorScaleAlpha(kModeSwitcherTrackBg, alpha));

	for (i32 stop = 0; stop < kCarouselZoomStopCount; stop += 1) {
		const float tickY = track.Y + track.H * (1.0f - ZoomStopPercent(stop) / 100.0f);
		drawList.AddRectFilled(track.X - 3.0f, tickY - 0.75f, track.W + 6.0f, 1.5f,
							   ColorScaleAlpha(kModeSwitcherTickColor, alpha));
	}

	char percentBuffer[8];
	const int written =
		std::snprintf(percentBuffer, sizeof(percentBuffer), "%d%%", static_cast<int>(m_flZoomPercent + 0.5f));
	const std::string_view percentText{percentBuffer, written > 0 ? static_cast<u64>(written) : 0};

	const float indicatorW = TextWidth(secondary, percentText) + kModeSwitcherIndicatorPaddingX * 2.0f;
	const float t = std::clamp(m_flZoomPercent / 100.0f, 0.0f, 1.0f);
	const float indicatorCy = track.Y + track.H * (1.0f - t);

	const Rect indicator{
		track.X + track.W * 0.5f - indicatorW * 0.5f,
		indicatorCy - kModeSwitcherIndicatorHeight * 0.5f,
		indicatorW,
		kModeSwitcherIndicatorHeight,
	};

	// A ring plus the fill, so the pill reads as a real control sitting on the track rather
	// than a flat patch of colour. Both follow the accent, and the readout contrasts against
	// that fill rather than being a fixed near-black that only worked against one purple.
	drawList.AddRectRoundedBordered(indicator.X - 1.0f, indicator.Y - 1.0f, indicator.W + 2.0f, indicator.H + 2.0f,
									CDrawList::UniformRadii(indicator.H * 0.5f + 1.0f),
									ColorScaleAlpha(m_clrAccent, alpha),
									ColorScaleAlpha(ColorOutlineOn(m_clrAccent), alpha), 1.0f);

	const float baselineY =
		indicator.Y + indicator.H * 0.5f + (secondary.GetAscent() + secondary.GetDescent()) * 0.5f - 1.0f;
	DrawText(drawList, secondary, indicator.X + kModeSwitcherIndicatorPaddingX, baselineY, percentText,
			 ColorScaleAlpha(ColorForegroundOn(m_clrAccent), alpha));
}

void CCarousel::DrawModeSwitcher(CDrawList &drawList) const
{
	if (m_flModeSwitcherVisibleAmount <= 0.001f) return;

	const auto alpha = static_cast<u8>(255.0f * m_flModeSwitcherVisibleAmount);
	const Rect panel = ModeSwitcherPanelRect();
	const float slide = (1.0f - m_flModeSwitcherVisibleAmount) * kModeSwitcherSlidePixels;
	const Rect shifted{panel.X, panel.Y + slide, panel.W, panel.H};

	// Three layered, progressively larger and dimmer rounded rects offset downward, so the
	// flyout reads as lifted off the carousel rather than pasted flat onto it.
	for (i32 i = 3; i >= 1; i -= 1) {
		const float t = static_cast<float>(i) / 3.0f;
		const float offset = 7.0f * t;
		const auto layerAlpha = static_cast<u8>(30.0f * (1.0f - t * 0.6f) * (static_cast<float>(alpha) / 255.0f));

		if (layerAlpha == 0) continue;

		drawList.AddRectRoundedFilled(shifted.X - offset * 0.3f, shifted.Y + offset, shifted.W + offset * 0.6f,
									  shifted.H + offset, CDrawList::UniformRadii(kModeSwitcherRadius + offset * 0.3f),
									  Color{0, 0, 0, layerAlpha});
	}

	drawList.AddRectRoundedFilled(shifted.X, shifted.Y, shifted.W, shifted.H,
								  CDrawList::UniformRadii(kModeSwitcherRadius),
								  ColorScaleAlpha(kModeSwitcherBorder, alpha));
	drawList.AddRectRoundedFilled(shifted.X + 1.0f, shifted.Y + 1.0f, shifted.W - 2.0f, shifted.H - 2.0f,
								  CDrawList::UniformRadii(kModeSwitcherRadius - 1.0f),
								  ColorScaleAlpha(kModeSwitcherBg, alpha));

	DrawModeSwitcherRows(drawList, shifted, alpha);
	DrawModeSwitcherSlider(drawList, shifted, alpha);
}

void CCarousel::Draw(CDrawList &drawList)
{
	PULSAR_PROFILE_SCOPE("Carousel.Draw");

	if (m_flTransitionAmount > 0.001f) {
		const auto outgoingAlpha = static_cast<u8>(255.0f * m_flTransitionAmount);
		const auto incomingAlpha = static_cast<u8>(255.0f * (1.0f - m_flTransitionAmount));
		const float slide = m_flTransitionAmount * kViewModeSlidePixels;

		DrawMode(drawList, m_transitionFromMode, outgoingAlpha, -slide, m_flMouseX, m_flMouseY);
		DrawMode(drawList, m_viewMode, incomingAlpha, slide, m_flMouseX, m_flMouseY);
	} else {
		DrawMode(drawList, m_viewMode, 255, 0.0f, m_flMouseX, m_flMouseY);
	}

	if (m_nBannerCount > 0) {
		DrawModeSwitcher(drawList);
	}
}
