#pragma once

#include <string_view>
#include "gfx/font_manager.h"
#include "ui/widget.h"

constexpr u32 kContextMenuMaxItems = 12;
constexpr u32 kContextMenuNoSelection = 0xFFFFFFFFu;

// An in-app right-click popup: structurally like the app menu, but built fresh from a
// caller-supplied item list each time it opens, since what it offers is contextual. No open
// animation - a right-click menu reads as instant everywhere else in Windows - and no
// submenus, since a flat list covers every real use here.
//
// Meant to sit above every other popup without being the title bar's alwaysTopmost special
// case: an owner achieves that simply by pushing it onto the stack last. Like the app menu,
// every input event is swallowed while open, not just clicks landing on a row.

/// Label is a non-owning view. Every current call site passes a literal; a caller wanting a
/// dynamic label would need its own storage for it.
struct ContextMenuItem {
	std::string_view Label;
	u32 Id;
};

class CContextMenu : public CWidget {
  public:
	explicit CContextMenu(const CFontManager &fonts);

	/// Opens at the right-click position, clamped so the menu never spills off the window edge.
	/// Items are copied in.
	void Open(float x, float y, const ContextMenuItem *pItems, u32 itemCount, float windowW, float windowH);

	void Close()
	{
		m_bOpen = false;
	}

	void Update(float deltaSeconds) override {}
	void Draw(CDrawList &drawList) override;

	bool OnPointerDown(float x, float y) override
	{
		return IsBlocking();
	}

	bool OnPointerMove(float x, float y) override
	{
		return IsBlocking();
	}

	/// Always closes, hit or not, and latches the clicked item's id for ConsumeSelection.
	bool OnPointerUp(float x, float y) override;

	bool OnScroll(float x, float y, float wheelDelta) override
	{
		return IsBlocking();
	}

	bool OnKeyDown(u32 keyCode) override
	{
		return IsBlocking();
	}

	bool OnChar(u32 character) override
	{
		return IsBlocking();
	}

	/// No animation, so this is a plain open check rather than an eased-amount threshold.
	bool IsBlocking() const override
	{
		return m_bOpen;
	}

	ECursorKind GetDesiredCursor() const override;

	/// Cleared on read; the no-selection sentinel if nothing was clicked, or the click missed
	/// every row, since the last Open.
	u32 ConsumeSelection();

  private:
	Rect MenuRect() const;
	Rect ItemRect(u32 index) const;

	const CFontManager &m_fonts;
	bool m_bOpen = false;

	/// The top-left corner, already clamped to the window by Open.
	float m_flAnchorX = 0.0f;
	float m_flAnchorY = 0.0f;

	ContextMenuItem m_aItems[kContextMenuMaxItems]{};
	u32 m_nItemCount = 0;
	u32 m_pendingSelection = kContextMenuNoSelection;
};
