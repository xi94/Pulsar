#pragma once

#include "platform/window.h" // ETitleBarButton, which the draw helpers below take
#include "ui/widget.h"

class CAssetManager;
class CUpdater;
class CFontManager;

/// The title bar's fill, exported so any other chrome strip shares the exact value rather than
/// an independently tuned constant that reads as a differently coloured bar beside it.
constexpr Color kTitleBarColor{24, 24, 27, 255};

/// Draws the custom title bar - menu button, draggable caption, and the pWindow glyphs - and
/// handles clicks on them. The geometry itself lives on CWindow, because WM_NCHITTEST needs it
/// before any frame is ever drawn, so visuals and hit-testing cannot drift apart.
///
/// The one widget pushed onto the stack as alwaysTopmost: it must render on top and receive
/// input regardless of what is blocking, since minimizing or closing should not require
/// dismissing a popup first.
///
/// The pWindow glyphs act directly, since they are pure OS operations nothing else cares about.
/// The menu and update buttons instead latch a flag a coordinating owner polls once per frame -
/// this widget has no business knowing the menu or overlay classes exist.
class CTitleBar : public CWidget {
  public:
	/// updater is read only, purely to decide whether the update pill is visible at all and what
	/// label and colour to draw it in. Starting a download is the overlay's job.
	CTitleBar(CWindow *pWindow, const CAssetManager &assets, const CUpdater &updater, const CFontManager &fonts);

	void Update(float deltaSeconds) override;
	void Draw(CDrawList &drawList) override;

	/// Consumes a press on any button without acting on it - actions fire on release, matching
	/// native title-bar behaviour where a press-then-drag-off does not trigger.
	///
	/// This override exists so a press can never fall through to what is underneath. As the
	/// alwaysTopmost widget, an unconsumed press on the menu button would reach the carousel
	/// with nothing blocking yet and start a card drag, whose matching release is consumed here
	/// and so never ends it. That exact asymmetry was a real reported bug: the carousel kept
	/// dragging on pure mouse movement after opening Settings from this bar.
	bool OnPointerDown(float x, float y) override;
	bool OnPointerUp(float x, float y) override;

	ECursorKind GetDesiredCursor() const override;

	/// One-shot: true for the frame a click landed, then clears itself.
	bool ConsumeMenuClicked();

	/// Same contract. Only ever latches while the pill is actually visible, so a stray click
	/// cannot fire it when there is nothing to show.
	bool ConsumeUpdateClicked();

  private:
	void DrawHoverBackground(CDrawList &drawList, ETitleBarButton button, ETitleBarButton hovered) const;
	void DrawUpdatePill(CDrawList &drawList) const;
	void DrawMaximizeGlyph(CDrawList &drawList, ETitleBarButton hovered) const;

	CWindow *m_pWindow = nullptr;
	const CAssetManager &m_assets;
	const CUpdater &m_updater;
	const CFontManager &m_fonts;

	bool m_bMenuClickedThisFrame = false;
	bool m_bUpdateClickedThisFrame = false;
};
