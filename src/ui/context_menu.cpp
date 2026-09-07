#include "ui/context_menu.h"

#include <algorithm>

#include "ui/draw_list.h"
#include "ui/text.h"

namespace {
constexpr float kMenuWidth = 200.0f;
constexpr float kMenuItemHeight = 32.0f;
constexpr float kMenuPadding = 6.0f;
constexpr float kMenuRadius = 10.0f;
constexpr float kWindowMargin = 8.0f;

constexpr Color kColorBg{30, 30, 34, 255};
constexpr Color kColorBorder{60, 60, 66, 255};
constexpr Color kColorHover{54, 46, 78, 255};
constexpr Color kColorText{220, 220, 224, 255};

float MenuHeightFor(u32 itemCount)
{
	return kMenuPadding * 2.0f + kMenuItemHeight * static_cast<float>(itemCount);
}
} // namespace

CContextMenu::CContextMenu(const CFontManager &fonts)
	: m_fonts(fonts)
{
}

Rect CContextMenu::MenuRect() const
{
	return Rect{m_flAnchorX, m_flAnchorY, kMenuWidth, MenuHeightFor(m_nItemCount)};
}

// Shared by hit-testing and drawing, so hover and clicks cannot drift apart.
Rect CContextMenu::ItemRect(u32 index) const
{
	const Rect menu = MenuRect();

	return Rect{menu.X, menu.Y + kMenuPadding + static_cast<float>(index) * kMenuItemHeight, menu.W, kMenuItemHeight};
}

void CContextMenu::Open(float x, float y, const ContextMenuItem *pItems, u32 itemCount, float windowW, float windowH)
{
	m_nItemCount = std::min(itemCount, kContextMenuMaxItems);
	for (u32 i = 0; i < m_nItemCount; i += 1) {
		m_aItems[i] = pItems[i];
	}

	const float h = MenuHeightFor(m_nItemCount);
	m_flAnchorX = std::clamp(x, kWindowMargin, std::max(kWindowMargin, windowW - kWindowMargin - kMenuWidth));
	m_flAnchorY = std::clamp(y, kWindowMargin, std::max(kWindowMargin, windowH - kWindowMargin - h));
	m_bOpen = true;
}

bool CContextMenu::OnPointerUp(float x, float y)
{
	if (!m_bOpen) return false;

	m_pendingSelection = kContextMenuNoSelection;

	for (u32 i = 0; i < m_nItemCount; i += 1) {
		if (RectContainsPoint(ItemRect(i), x, y)) {
			m_pendingSelection = m_aItems[i].Id;
			break;
		}
	}

	Close();

	return true;
}

ECursorKind CContextMenu::GetDesiredCursor() const
{
	if (!m_bOpen) return ECursorKind::Arrow;

	for (u32 i = 0; i < m_nItemCount; i += 1) {
		if (RectContainsPoint(ItemRect(i), m_flMouseX, m_flMouseY)) return ECursorKind::Hand;
	}

	return ECursorKind::Arrow;
}

u32 CContextMenu::ConsumeSelection()
{
	const u32 selection = m_pendingSelection;
	m_pendingSelection = kContextMenuNoSelection;

	return selection;
}

void CContextMenu::Draw(CDrawList &drawList)
{
	if (!m_bOpen) return;

	const Rect rect = MenuRect();
	drawList.AddRectRoundedFilled(rect.X, rect.Y, rect.W, rect.H, CDrawList::UniformRadii(kMenuRadius), kColorBorder);
	drawList.AddRectRoundedFilled(rect.X + 1.0f, rect.Y + 1.0f, rect.W - 2.0f, rect.H - 2.0f,
								  CDrawList::UniformRadii(kMenuRadius - 1.0f), kColorBg);

	const CFont &body = m_fonts.GetBody();
	const float baselineOffset = (body.GetAscent() + body.GetDescent()) * 0.5f;

	for (u32 i = 0; i < m_nItemCount; i += 1) {
		const Rect row = ItemRect(i);

		if (RectContainsPoint(row, m_flMouseX, m_flMouseY)) {
			drawList.AddRectRoundedFilled(row.X + 4.0f, row.Y, row.W - 8.0f, row.H, CDrawList::UniformRadii(6.0f),
										  kColorHover);
		}

		DrawText(drawList, body, row.X + 14.0f, row.Y + row.H * 0.5f + baselineOffset, m_aItems[i].Label, kColorText);
	}
}
