#pragma once

#include "core/banner.h"
#include "core/types.h"
#include "gfx/font_manager.h"
#include "ui/draggable.h"
#include "ui/scrollable.h"
#include "ui/widget.h"

class CAssetManager;

// The game picker, in three view modes:
//
//   Carousel - a horizontal strip where the centred card is the selection.
//   Grid     - a wrapping grid of cards, vertically scrollable.
//   List     - compact full-width rows with a large per-game icon and title.
//
// Ctrl+scroll moves between them along one continuous zoom scale. The wheel event carries no
// modifier state, so OnScroll cannot tell that gesture from a plain notch: the owner checks the
// key itself and calls AdjustZoomStop instead for that frame.

enum class ECarouselViewMode : u8 {
	Carousel,
	Grid,
	List,
};

constexpr u32 kCarouselViewModeCount = 3;

/// Carousel, then three stops each for Grid and List - base, grown and max - so both keep
/// growing rather than being dead ends. Seven in total, hence the percentages landing on sixths.
constexpr i32 kCarouselZoomStopCount = 7;

/// One account's real identity within a visible-account query: which banner owns it, and its
/// index there. An account visible under a second game is still stored once, under its real
/// owner, and this points back to that one copy.
struct VisibleAccountRef {
	u32 BannerIndex;
	u32 AccountIndex;
};

/// The true worst case: every account in every banner visible under one target banner.
constexpr u32 kCarouselMaxVisibleAccounts = kCarouselMaxBanners * kCarouselMaxAccountsPerBanner;

class CCarousel : public CWidget {
  public:
	CCarousel(const CFontManager &fonts, const CAssetManager &assets);

	/// Pushed in once per frame rather than held as a settings reference, since this widget is
	/// constructed before a settings object exists. Drives the view-mode switcher only.
	void SetAccentColor(Color accent)
	{
		m_clrAccent = accent;
	}

	/// The texture's aspect is derived once here, so cover-fit cropping never re-queries later.
	void AddBanner(std::string_view title, CTexture *pTexture, CTexture *pIcon, Color accent);

	/// Asserts rather than validates: every index is UI-driven, never user-typed.
	void AddAccount(u32 bannerIndex, std::string_view username, std::string_view note, std::string_view password);

	/// Overwrites in place rather than remove-then-add, so an edited row keeps its position.
	void UpdateAccount(u32 bannerIndex, u32 accountIndex, std::string_view username, std::string_view note,
					   std::string_view password);

	/// Shifts the following accounts down, leaving no gaps in the fixed array.
	void RemoveAccount(u32 bannerIndex, u32 accountIndex);

	/// Every account visible under this banner: its own first, in order, then every other
	/// banner's account that opted in. pOut needs room for kCarouselMaxVisibleAccounts.
	u32 GetVisibleAccounts(u32 bannerIndex, VisibleAccountRef *pOut) const;

	u32 GetBannerCount() const
	{
		return m_nBannerCount;
	}

	const Banner &GetBanner(u32 index) const
	{
		return m_aBanners[index];
	}

	Banner &GetBanner(u32 index)
	{
		return m_aBanners[index];
	}

	/// The banner Carousel mode is settling on - its target, not the mid-animation position.
	/// Grid and List have no persistent selection of their own.
	i32 GetSelectedIndex() const;

	/// One banner's on-screen rect in Carousel mode, shared by drawing and hit-testing so the
	/// two cannot drift. Grid and List geometry stays internal.
	Rect GetBannerRect(u32 index) const;

	/// The banner under the point in whichever mode is active, or -1. Overlapping Carousel
	/// cards resolve to the most centred candidate, matching what is visually on top.
	i32 HitTest(float x, float y) const;

	/// One zoom stop per wheel notch, clamped rather than wrapped at either end.
	void AdjustZoomStop(float wheelDelta);

	/// Snaps straight to a stop with no easing or crossfade, for applying a persisted value
	/// before the first frame.
	void ApplyZoomStop(i32 stop);

	/// Independent of the view mode: Grid and List never touch the carousel's offset, so
	/// this is exactly the banner Carousel mode resumes on when next entered.
	void ApplySelectedIndex(i32 index);

	i32 GetZoomStop() const
	{
		return m_nZoomStop;
	}

	ECarouselViewMode GetViewMode() const
	{
		return m_viewMode;
	}

	/// Every geometry computation derives from m_vecBounds, which the owner sets once per frame.
	void Update(float deltaSeconds) override;
	void Draw(CDrawList &drawList) override;

	/// The current mode's icon and name in the app's shared bottom strip, and the mode
	/// switcher's open trigger. The owner draws that strip's background.
	void DrawStatusBarContent(CDrawList &drawList) const;

	bool OnPointerDown(float x, float y) override;
	bool OnPointerMove(float x, float y) override;
	bool OnPointerUp(float x, float y) override;
	bool OnScroll(float x, float y, float wheelDelta) override;

	/// The banner a genuine left-click just resolved to - not a drag, not a switcher click, and
	/// not the release that ends a scrollbar drag. An explicit signal rather than something the
	/// owner re-derives from HitTest, which cannot tell a card click from a drag release.
	PendingHit ConsumePendingClick();

	/// The banner Enter was pressed on, which always means open it. Separate from the click signal
	/// above because a click carries the "only an already-centred card opens" rule and a keypress
	/// does not - the focus ring already said which card Enter would act on.
	PendingHit ConsumePendingActivate();

	/// Arrow keys move the focus ring, Enter activates it. Which arrows do what follows the layout
	/// on screen: a row in Carousel, a grid in Grid, a column in List.
	bool OnKeyDown(u32 keyCode) override;

	/// Whether the pointer is in the mode switcher's hover region.
	bool IsMouseOverModeSwitcher(float mouseX, float mouseY) const;

	/// Mirrors the pointer handlers' per-mode priority order, so this never shows a cursor for
	/// an interaction that would not actually happen on that click.
	ECursorKind GetDesiredCursor() const override;

  private:
	/// Pure functions of the current state and bounds; methods only because they read members.
	Rect GridBannerRect(u32 index) const;
	Rect ListBannerRect(u32 index) const;
	float GridContentHeight() const;
	float ListContentHeight() const;
	float WrapContentHeight(ECarouselViewMode mode) const;
	Rect ScrollbarTrackRect() const;
	float ClampTarget(float value) const;

	Rect StatusBarRect() const;
	Rect StatusBarContentRect() const;

	float ModeSwitcherTrackColumnWidth() const;
	Rect ModeSwitcherPanelRect() const;
	Rect ModeSwitcherRowRect(Rect panel, u32 index) const;
	Rect ModeSwitcherTrackRect(Rect panel) const;

	/// A generous grab area around the thin visual track.
	Rect ModeSwitcherTrackGrabRect(Rect panel) const;

	/// The union of the panel and the status-bar indicator that opens it, spanning the gap
	/// between them, so moving up from the indicator never reads as leaving.
	Rect ModeSwitcherHoverRect() const;

	bool IsModeSwitcherVisible() const
	{
		return m_flModeSwitcherVisibleAmount > 0.01f;
	}

	/// Which card the keyboard is on. Carousel mode derives it from the scroll target rather than
	/// storing it, so dragging or clicking a card leaves the arrow keys carrying on from there
	/// instead of from wherever they were last used.
	i32 FocusedIndex() const;

	/// Whether this card should be drawn as the keyboard focus, which looks exactly like hover -
	/// the two mean the same thing to the reader, so they should not look different.
	bool IsFocusHighlighted(u32 index) const;

	/// Clamps to the banner range, re-centres in Carousel mode and scrolls into view in the other
	/// two, so the focused card is always the one on screen.
	void MoveFocus(i32 delta);

	/// Nudges the wrapping scroll just far enough that the focused card is fully visible. A no-op
	/// when it already is, so holding an arrow key does not drag the view along with it.
	void ScrollFocusIntoView();

	/// Applies a clamped stop, starting the crossfade only when the implied mode actually
	/// changes. Shared by the wheel, the switcher's rows, and its track drag.
	void SetZoomStop(i32 stop);

	/// Run before falling through to card and scrollbar handling.
	bool ModeSwitcherOnPointerDown(float x, float y);
	bool ModeSwitcherOnPointerMove(float x, float y);
	bool ModeSwitcherOnPointerUp();

	void DrawBannerCard(CDrawList &drawList, Rect rect, const Banner &banner, bool highlighted, bool strong,
						u8 alpha) const;
	void DrawCarouselMode(CDrawList &drawList, u8 alpha, float yOffset, float mouseX, float mouseY) const;
	void DrawGridMode(CDrawList &drawList, u8 alpha, float yOffset, float mouseX, float mouseY) const;
	void DrawListMode(CDrawList &drawList, u8 alpha, float yOffset, float mouseX, float mouseY) const;
	void DrawMode(CDrawList &drawList, ECarouselViewMode mode, u8 alpha, float yOffset, float mouseX,
				  float mouseY) const;
	void DrawWrapChrome(CDrawList &drawList, float contentHeight, u8 alpha, float mouseX, float mouseY) const;
	void DrawModeSwitcher(CDrawList &drawList) const;
	void DrawModeSwitcherRows(CDrawList &drawList, Rect panel, u8 alpha) const;
	void DrawModeSwitcherSlider(CDrawList &drawList, Rect panel, u8 alpha) const;

	const CFontManager &m_fonts;
	const CAssetManager &m_assets;
	Color m_clrAccent{108, 90, 220, 255};

	Banner m_aBanners[kCarouselMaxBanners]{};
	u32 m_nBannerCount = 0;

	/// Carousel mode only, in card-slot units.
	float m_flScrollOffset = 0.0f;
	float m_flTargetScrollOffset = 0.0f;

	CDraggable m_cardDrag;
	float m_flDragStartScrollOffset = 0.0f;

	/// The percent eases toward the target stop every frame and drives both the switcher's
	/// slider and the card size within a mode, so growing across several notches reads as motion
	/// rather than snapping between fixed sizes.
	i32 m_nZoomStop = 0;
	float m_flZoomPercent = 0.0f;

	/// While the transition amount is above zero the previous mode is still drawn, fading out as
	/// the current one fades in.
	ECarouselViewMode m_viewMode = ECarouselViewMode::Carousel;
	ECarouselViewMode m_transitionFromMode = ECarouselViewMode::Carousel;
	float m_flTransitionAmount = 0.0f;

	/// Reset on every mode switch, since Grid's and List's content heights are unrelated.
	CScrollable m_wrapScroll;

	/// The hold timer counts down whenever the pointer leaves the switcher's hover region and
	/// nothing has just changed; the visible amount eases toward 1 while it lasts.
	float m_flModeSwitcherHoldSeconds = 0.0f;
	float m_flModeSwitcherVisibleAmount = 0.0f;

	CDraggable m_modeSwitcherDrag;

	/// Wider than the drag state on purpose: a row click resolves entirely on the down and
	/// starts no drag, but its release still has to be swallowed.
	bool m_bModeSwitcherPointerCaptured = false;

	/// Grid mode's per-card hover pop. An array rather than one hovered index, so the outgoing
	/// and incoming cards can ease simultaneously instead of snapping.
	float m_aGridHoverScale[kCarouselMaxBanners]{};

	/// Only meaningful in Grid and List - see FocusedIndex.
	i32 m_nFocusedIndex = 0;

	/// Whether the focus is worth showing. False until an arrow key is actually pressed, and false
	/// again the moment the mouse moves: a focus marker on a card nobody navigated to reads as a
	/// selection the user did not make.
	bool m_bKeyboardFocusVisible = false;

	PendingHit m_pendingClickBanner;
	PendingHit m_pendingActivateBanner;
};
