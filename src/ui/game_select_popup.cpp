#include "ui/game_select_popup.h"

#include <algorithm>
#include <bit>

#include "core/animator.h"
#include "ui/draw_list.h"
#include "ui/text.h"

namespace {
constexpr float kPopupWidth = 220.0f;
constexpr float kPopupPadding = 6.0f;
constexpr float kPopupRadius = 10.0f;
constexpr float kWindowMargin = 8.0f;
constexpr float kAnchorGap = 6.0f;
constexpr float kRowPaddingX = 10.0f;
constexpr float kOpenEaseRate = 20.0f;

constexpr Color kColorBg{30, 30, 34, 255};
constexpr Color kColorBorder{60, 60, 66, 255};
constexpr Color kColorHover{46, 46, 52, 255};
constexpr Color kColorText{220, 220, 224, 255};
constexpr Color kColorTextDisabled{130, 130, 136, 255};
constexpr Color kColorCheck{130, 200, 140, 255};
constexpr Color kColorCheckDisabled{110, 118, 112, 255};
constexpr Color kColorIconDisabled{150, 150, 150, 255};
constexpr Color kColorIconNormal{255, 255, 255, 255};

// Smaller than List view's dedicated thumbnail: this is a compact multi-select,
// not a place to show icons off.
float RowHeightFor(const CFontManager &fonts)
{
	return std::max(30.0f, fonts.GetSecondary().GetLineHeight() + 14.0f);
}

float IconSizeFor(const CFontManager &fonts)
{
	return fonts.GetSecondary().GetLineHeight() * 0.95f;
}

void DrawCheck(CDrawList &drawList, Rect row, float scale, Color color)
{
	const float cx = row.X + row.W - kRowPaddingX - 8.0f * scale;
	const float cy = row.Y + row.H * 0.5f;

	drawList.AddLine(cx - 7.0f * scale, cy, cx - 2.0f * scale, cy + 5.0f * scale, 2.0f, color);
	drawList.AddLine(cx - 2.0f * scale, cy + 5.0f * scale, cx + 7.0f * scale, cy - 6.0f * scale, 2.0f, color);
}
} // namespace

CGameSelectPopup::CGameSelectPopup(const CFontManager &fonts)
	: m_fonts(fonts)
{
}

// Below the anchor and right-aligned to it, so the box unfolds down and left over the form.
// Every clamp is relative to the bounds rect's edges.
//
// Only the resting height is clamped, never the animating one: clamping the growing height
// slides the box upward frame by frame, which reads as drifting rather than opening.
Rect CGameSelectPopup::PopupRect() const
{
	const u32 rowCount = std::max<u32>(1, m_nBannerCount);
	const float fullHeight = kPopupPadding * 2.0f + RowHeightFor(m_fonts) * static_cast<float>(rowCount);

	// Right edges flush, then clamped so a narrow column cannot push the box off its left side.
	// The popup is wider than the chip, so it always overhangs to the left.
	const float minX = m_bounds.X + kWindowMargin;
	const float maxX = std::max(minX, m_bounds.X + m_bounds.W - kWindowMargin - kPopupWidth);
	const float x = std::clamp(m_anchor.X + m_anchor.W - kPopupWidth, minX, maxX);

	const float minY = m_bounds.Y + kWindowMargin;
	const float maxY = std::max(minY, m_bounds.Y + m_bounds.H - kWindowMargin - fullHeight);
	const float y = std::clamp(m_anchor.Y + m_anchor.H + kAnchorGap, minY, maxY);

	return Rect{x, y, kPopupWidth, fullHeight * m_flOpenAmount};
}

Rect CGameSelectPopup::RowRect(u32 index) const
{
	const Rect popup = PopupRect();
	const float height = RowHeightFor(m_fonts);

	return Rect{popup.X, popup.Y + kPopupPadding + static_cast<float>(index) * height, popup.W, height};
}

bool CGameSelectPopup::IsLockedRow(u32 index) const
{
	return (m_uMask & (1u << index)) != 0 && std::popcount(m_uMask) == 1;
}

void CGameSelectPopup::Open(u16 initialMask, const Banner *pBanners, u32 bannerCount, Rect anchor, Rect bounds)
{
	m_uMask = initialMask;
	m_pBanners = pBanners;
	m_nBannerCount = bannerCount;
	m_anchor = anchor;
	m_bounds = bounds;
	m_bOpen = true;
}

void CGameSelectPopup::Close()
{
	m_bOpen = false;
}

void CGameSelectPopup::Update(float deltaSeconds)
{
	m_flOpenAmount = CAnimator::EaseToward(m_flOpenAmount, m_bOpen ? 1.0f : 0.0f, kOpenEaseRate, deltaSeconds);

	if (!m_bOpen && m_flOpenAmount < 0.002f) {
		m_flOpenAmount = 0.0f;
	}
}

bool CGameSelectPopup::OnPointerDown(float x, float y)
{
	if (!IsBlocking() || !RectContainsPoint(PopupRect(), x, y)) return false;

	for (u32 i = 0; i < m_nBannerCount; i += 1) {
		if (!RectContainsPoint(RowRect(i), x, y)) continue;

		if (!IsLockedRow(i)) {
			m_uMask ^= static_cast<u16>(1u << i);
		}

		break;
	}

	return true;
}

ECursorKind CGameSelectPopup::GetDesiredCursor() const
{
	if (!IsBlocking()) return ECursorKind::Arrow;

	for (u32 i = 0; i < m_nBannerCount; i += 1) {
		// A locked row's click is ignored, so it gets no hand either - matching Draw, which
		// skips its hover highlight.
		if (!IsLockedRow(i) && RectContainsPoint(RowRect(i), m_flMouseX, m_flMouseY)) return ECursorKind::Hand;
	}

	return ECursorKind::Arrow;
}

void CGameSelectPopup::Draw(CDrawList &drawList)
{
	if (!IsBlocking()) return;

	const Rect popup = PopupRect();
	drawList.AddRectRoundedFilled(popup.X, popup.Y, popup.W, popup.H, CDrawList::UniformRadii(kPopupRadius),
								  kColorBorder);
	drawList.AddRectRoundedFilled(popup.X + 1.0f, popup.Y + 1.0f, popup.W - 2.0f, popup.H - 2.0f,
								  CDrawList::UniformRadii(kPopupRadius - 1.0f), kColorBg);

	// The position clamp is what keeps this laid out correctly; the clip rect guarantees no row
	// paints past the rounded border even when many banners in a short bounds rect make the
	// clamp alone unable to fit everything.
	drawList.PushClipRect(popup);

	const CFont &secondary = m_fonts.GetSecondary();
	const float iconSize = IconSizeFor(m_fonts);
	const float baselineOffset = (secondary.GetAscent() + secondary.GetDescent()) * 0.5f;

	for (u32 i = 0; i < m_nBannerCount; i += 1) {
		const Rect row = RowRect(i);
		const Banner &banner = m_pBanners[i];
		const bool locked = IsLockedRow(i);

		if (!locked && RectContainsPoint(row, m_flMouseX, m_flMouseY)) {
			drawList.AddRectRoundedFilled(row.X + 4.0f, row.Y, row.W - 8.0f, row.H, CDrawList::UniformRadii(6.0f),
										  kColorHover);
		}

		const Rect iconRect{row.X + kRowPaddingX, row.Y + (row.H - iconSize) * 0.5f, iconSize, iconSize};

		if (banner.pIcon != nullptr) {
			drawList.AddRectRoundedTextured(iconRect.X, iconRect.Y, iconRect.W, iconRect.H,
											CDrawList::UniformRadii(4.0f), banner.pIcon,
											locked ? kColorIconDisabled : kColorIconNormal);
		} else {
			drawList.AddRectRoundedFilled(iconRect.X, iconRect.Y, iconRect.W, iconRect.H, CDrawList::UniformRadii(4.0f),
										  locked ? ColorScaleAlpha(banner.Accent, 140) : banner.Accent);
		}

		DrawText(drawList, secondary, iconRect.X + iconRect.W + 10.0f, row.Y + row.H * 0.5f + baselineOffset,
				 banner.Title, locked ? kColorTextDisabled : kColorText);

		if ((m_uMask & (1u << i)) != 0) {
			DrawCheck(drawList, row, iconSize / 20.0f, locked ? kColorCheckDisabled : kColorCheck);
		}
	}

	drawList.PopClipRect();
}
