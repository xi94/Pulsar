#pragma once

#include "core/banner.h"
#include "gfx/font_manager.h"
#include "ui/widget.h"

/// The account form's "visible in which games" multi-select: one row per banner, an icon on the
/// left and a check on the right when that bit is set. The mask is edited live and persists
/// across close and reopen within an edit session; the owner reads it back at save time, so
/// closing is dismissal rather than a confirm step.
///
/// A row can never be unchecked down to zero games - an account visible in no game is
/// unreachable from the carousel entirely - so the sole remaining checked row is drawn greyed
/// out and its clicks are silently ignored until another row is checked.
///
/// Row and icon sizes derive from the font's line height rather than fixed constants, so this
/// grows with the font-size setting like every other list here.
///
/// Not a stack member but a value its owner forwards calls to, so nothing calls SetMouseGated
/// on it automatically - the owner must do that once per frame, or every hover check here
/// silently never fires.
///
/// It hangs directly below the chip that opens it, right-aligned so it grows leftward over the
/// form. There is no flip-above fallback: the chip sits in the form's header, so
/// below always has the whole form to unfold into, and a popup that sometimes jumped above its
/// own chip was a direct bug report. The open amount grows its height from the top down while a
/// clip rect hides the rows it has not reached yet.
class CGameSelectPopup : public CWidget {
  public:
	explicit CGameSelectPopup(const CFontManager &fonts);

	/// The caller's banner array does not need to outlive the call, but the icons and accents it
	/// points at are read live by Draw.
	///
	/// bounds is the region the popup must stay fully inside - the caller's content column,
	/// not its whole panel. Clamping against the full window let the popup spill past the
	/// modal's bottom border whenever the panel was smaller than the window.
	void Open(u16 initialMask, const Banner *pBanners, u32 bannerCount, Rect anchor, Rect bounds);
	void Close();

	u16 GetMask() const
	{
		return m_uMask;
	}

	void Update(float deltaSeconds) override;
	void Draw(CDrawList &drawList) override;

	/// Toggles the clicked row's bit, except on the locked row. Returns whether the point was
	/// anywhere inside the popup - unlike the other menus here this does not self-close on a
	/// miss, since it applies its result live and there is nothing to lose by leaving the close
	/// decision to the owner.
	bool OnPointerDown(float x, float y) override;

	/// True through the fold-in too, not just while open: a closing popup still wants input
	/// routed to it until the animation finishes.
	bool IsBlocking() const override
	{
		return m_flOpenAmount > 0.01f;
	}

	/// Like SetMouseGated, the owner must consult this explicitly - the stack does not.
	ECursorKind GetDesiredCursor() const override;

  private:
	Rect PopupRect() const;
	Rect RowRect(u32 index) const;

	/// The sole remaining checked row cannot be unchecked. Shared by the click path, to refuse
	/// it, and the draw path, to render it disabled, so the two cannot disagree about which row
	/// that is.
	bool IsLockedRow(u32 index) const;

	const CFontManager &m_fonts;
	bool m_bOpen = false;
	float m_flOpenAmount = 0.0f;
	u16 m_uMask = 0;

	const Banner *m_pBanners = nullptr;
	u32 m_nBannerCount = 0;

	Rect m_anchor{};
	Rect m_bounds{};
};
