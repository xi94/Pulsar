#pragma once

#include <string_view>
#include "core/updater.h"
#include "ui/scrollable.h"
#include "ui/widget.h"

class CFontManager;
class CWindow;
struct Settings;

/// The full-screen "an update is available" takeover, opened from the title bar's update pill.
/// Unlike the unlock screen it is always dismissible while nothing is actively downloading: an
/// update is a suggestion, not a security gate, so nothing here should trap the user.
///
/// Pure UI - every real action calls straight through to the updater, so there is no polled
/// action for a coordinating owner to react to. The one exception is the relaunch itself, which
/// only the owner can do.
class CUpdateOverlay : public CWidget {
  public:
	CUpdateOverlay(const CFontManager &fonts, const CWindow &window, Settings *pSettings, CUpdater *pUpdater);

	void Open();
	void Close();

	void Update(float deltaSeconds) override;
	void Draw(CDrawList &drawList) override;

	bool OnPointerUp(float x, float y) override;

	/// Real hit-testing rather than a blanket swallow: a click on the notes scrollbar starts a
	/// drag. Anything else still swallows, since nothing underneath a modal takeover should be
	/// reachable, dismissible or not.
	bool OnPointerDown(float x, float y) override;
	bool OnPointerMove(float x, float y) override;
	bool OnScroll(float x, float y, float wheelDelta) override;

	bool OnRightPointerUp(float x, float y) override
	{
		return IsBlocking();
	}

	ECursorKind GetDesiredCursor() const override;

	bool IsBlocking() const override
	{
		return m_bActive;
	}

  private:
	/// The notes box's geometry and wrapped height, which the scroll, drag and draw paths all
	/// need together.
	struct NotesLayout {
		Rect Box;
		Rect Track;
		float ContentHeight;
	};

	Rect CardRect() const;
	NotesLayout ComputeNotesLayout() const;

	void DrawCloseButton(CDrawList &drawList, Rect card) const;
	void DrawNotesBox(CDrawList &drawList);

	void DrawCheckingStage(CDrawList &drawList, Rect card, float &cursorY);
	void DrawUpToDateStage(CDrawList &drawList, Rect card, float &cursorY);
	void DrawCheckFailedStage(CDrawList &drawList, Rect card, float &cursorY);
	void DrawAvailableStage(CDrawList &drawList, Rect card, float &cursorY);
	void DrawManualUpgradeStage(CDrawList &drawList, Rect card, float &cursorY);
	void DrawProgressStage(CDrawList &drawList, Rect card, float &cursorY, EUpdateStage stage);
	void DrawFailedStage(CDrawList &drawList, Rect card, float &cursorY, EUpdateStage stage);

	/// Draws the title and advances the cursor past it to the subtitle's baseline.
	void DrawCardTitle(CDrawList &drawList, Rect card, float &cursorY, std::string_view title) const;

	/// Where every stage's text starts: right of the header badge, so the stages line up with each
	/// other rather than each measuring from the card edge.
	float TextColumnX(Rect card) const;
	float TextColumnWidth(Rect card) const;
	void DrawPrimaryButton(CDrawList &drawList, Rect card, std::string_view label, bool accented) const;

	const CFontManager &m_fonts;
	const CWindow &m_window;
	Settings *m_pSettings = nullptr;
	CUpdater *m_pUpdater = nullptr;

	bool m_bActive = false;

	/// Owned here rather than reset on every Open: reopening for the same still-pending update
	/// reasonably keeps wherever the notes were scrolled to.
	CScrollable m_notesScroll;
};
