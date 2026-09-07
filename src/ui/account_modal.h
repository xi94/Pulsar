#pragma once

#include "core/login_attempt.h"
#include "core/types.h"
#include "gfx/font_manager.h"
#include "ui/carousel.h"
#include "ui/confirm_latch.h"
#include "ui/game_select_popup.h"
#include "ui/scrollable.h"
#include "ui/text_input.h"
#include "ui/toast.h"
#include "ui/widget.h"

class CAssetManager;
class CWindow;
struct Settings;

// The account list popup: opens when a carousel banner is clicked, animates in, and blocks
// everything behind it while open. The panel scales with the window, capped so it does not
// balloon on a large monitor.
//
// Three modes share one panel; only the inner content crossfades, so the panel's position and
// size do not re-animate on a mode switch.
//
// This class reads and mutates account data entirely through CCarousel's public surface rather
// than a raw banner array, since the carousel owns that storage and its mutation logic is
// already correct.
//
// Every index this class stores is a *query index* - a position in CCarousel's visible-account
// query, not an index into one banner's array, because an account can be visible under more
// than one game. Every mutating path resolves one back to its real owning banner through
// ResolveVisibleAccount before touching storage. That resolver is the mechanism that fixed a
// real cross-banner data-corruption bug, so nothing here may bypass it.

enum class EAccountModalMode : u8 {
	AccountList,
	LoginProgress,
	EditAccount, // adding when the edit index is negative, editing an existing row otherwise
};

class CAccountModal : public CWidget {
  public:
	CAccountModal(CCarousel *pCarousel, INotificationSink *pNotifications, const CFontManager &fonts,
				  const CWindow &window, const Settings &settings, const CAssetManager &assets);

	/// No account is pre-selected; the user picks a row explicitly.
	void Open(i32 bannerIndex);

	/// Starts the close animation without forgetting which banner it was showing, so the panel
	/// still has something to draw while it fades out.
	void Close();

	/// Opens pre-selected to one account and starts a login immediately - clicking a row and
	/// then Login, collapsed into one call for the tray's quick login. Does not
	/// show the window: a login started from the tray runs uninterrupted, and restoring later
	/// lands straight on its progress view.
	void OpenForQuickLogin(i32 bannerIndex, i32 accountIndex);

	void StartAddAccount();

	/// queryIndex is resolved internally, so this works even when the row being edited is
	/// visible here but owned by a different banner. Also the entry point for the row's
	/// right-click Edit action.
	void StartEditAccount(u32 queryIndex);

	/// Resolves the row to its real owning banner and removes it there, adjusting the selection
	/// the same way the row's remove button does. A no-op if it does not resolve.
	void RemoveAccountRow(u32 queryIndex);

	/// The two fields worth copying, as pointers into the account's null-terminated buffers.
	/// False if the row does not resolve, in which case neither output is touched. Exists so an
	/// owner can serve the row's right-click menu without reimplementing the query-index
	/// mapping. Both pointers borrow the carousel's storage, so use them before anything can
	/// mutate the account list.
	bool GetAccountCopyFields(u32 queryIndex, const char *&outUsername, const char *&outPassword) const;

	void Update(float deltaSeconds) override;
	void Draw(CDrawList &drawList) override;

	/// Always consumed while blocking - the standard "a modal owns all input while open"
	/// contract every popup here follows.
	bool OnPointerDown(float x, float y) override;
	bool OnPointerMove(float x, float y) override;
	bool OnPointerUp(float x, float y) override;

	/// Latches which row was right-clicked. This class cannot open a context menu itself, so an
	/// owner polls the result and builds the actual menu.
	bool OnRightPointerUp(float x, float y) override;

	/// Consumes regardless of mode, so a wheel notch over an open modal never reaches the
	/// carousel underneath.
	bool OnScroll(float x, float y, float wheelDelta) override;

	/// Escape backs out one level: the edit form returns to the list, and in the list it clears
	/// the selection first, only closing on a second press with nothing selected.
	bool OnKeyDown(u32 keyCode) override;
	bool OnChar(u32 character) override;

	bool IsBlocking() const override
	{
		return m_flOpenAmount > 0.01f;
	}

	ECursorKind GetDesiredCursor() const override;

	/// Cleared on read; Miss means the right-click landed in the modal but on no row.
	PendingHit ConsumePendingRightClickRow();

  private:
	/// The panel's rects, computed once per call and shared by every hit-test and draw rather
	/// than each re-deriving it.
	struct Layout {
		Rect Panel;
		Rect Inner;
		Rect Content;
		Rect Left;
		Rect Right;
		Rect Footer;
	};

	/// One snapshot of the account list: which accounts are visible, where each row sits, and
	/// the scroll geometry around them. Gathered once per call by every path that needs rows, so
	/// hit-testing, scrolling and drawing can never disagree about where a row is.
	struct AccountListView {
		VisibleAccountRef Refs[kCarouselMaxVisibleAccounts];
		float Tops[kCarouselMaxVisibleAccounts];
		u32 Count = 0;
		float RowHeight = 0.0f;
		float ContentHeight = 0.0f;
		Rect ScrollRegion{};
		Rect ScrollbarTrack{};
	};

	Layout ComputeLayout() const;
	Rect PanelRect() const;

	/// False when no valid banner is open, in which case the view is left empty.
	bool BuildAccountListView(const Layout &layout, AccountListView &out) const;

	Rect RowRectAt(const Layout &layout, const AccountListView &view, u32 index) const;
	bool IsRowVisible(const AccountListView &view, Rect row) const;

	/// The row under the point, or -1. Only rows actually on screen can be hit.
	i32 HitTestRow(const Layout &layout, const AccountListView &view, float x, float y) const;

	const Account &AccountAt(const AccountListView &view, u32 index) const;

	Rect AccountsScrollRegionRect(Rect right) const;
	Rect AccountsScrollbarTrackRect(Rect scrollRegion) const;
	Rect CloseBadgeRect(Rect left) const;
	Rect PrimaryButtonRect(Rect footer) const;
	Rect RowRemoveButtonRect(Rect row) const;

	/// The remove badge grown into a labelled pill, which is both what an armed row draws and what
	/// it hit-tests.
	Rect RowConfirmDeleteRect(Rect row) const;
	Rect RowEditButtonRect(Rect row) const;
	Rect AddAccountButtonRect(Rect right) const;

	/// The three stacked field blocks, in the order they are drawn: note, username, password.
	Rect EditFieldBlockRect(Rect right, u32 index) const;
	Rect EditFieldInputRect(Rect block) const;
	Rect EditFieldInputRectAt(Rect right, u32 index) const;

	Rect VisibilityChipRect(Rect right) const;
	Rect EditCancelButtonRect(Rect save) const;
	Rect EditDeleteButtonRect(Rect footer) const;
	Rect PasswordRevealButtonRect(Rect passwordField) const;

	/// Whether the edit form's Save is live: a username and a password are both required, since
	/// an account missing either cannot log in, while the note stays optional - it labels the
	/// row for the reader rather than being a credential. The one place that rule is written,
	/// since the click handler, the cursor and the button's painting must all agree.
	bool CanSaveEditedAccount() const;

	/// See this class's note on query indices for why every mutation goes through this.
	bool ResolveVisibleAccount(u32 bannerIndex, u32 queryIndex, VisibleAccountRef &out) const;

	/// The launch target comes from bannerIndex - the game the user was looking at - rather than
	/// the banner the account record happens to live under. One Riot account is shared across
	/// every game, and cross-visibility means those are genuinely different questions; a
	/// cross-visible account launched the wrong game until this used the viewed banner.
	void StartLoginFor(u32 bannerIndex, u32 queryIndex);

	/// The one entry point for "log this account in now". Starts immediately when nothing is in
	/// flight; otherwise cancels what is running and queues this for Update to start once the
	/// old worker lets go - the attempt refuses to start over a live one, and joining here could
	/// park the render thread on a wedged call.
	void RequestLogin(i32 bannerIndex, i32 queryIndex);

	/// The progress view reads this so the cancelled attempt's terminal state does not flash on
	/// the way through.
	bool HasPendingLogin() const
	{
		return m_nPendingLoginBannerIndex >= 0;
	}

	bool HasValidBanner() const;

	/// Arrow keys, Enter and Delete over the account list. False when the key is not one of those.
	bool HandleAccountListKey(u32 keyCode);
	void ScrollSelectedRowIntoView(const AccountListView &view);

	bool HandleAccountListClick(const Layout &layout, float x, float y);
	bool HandleLoginProgressClick(const Layout &layout, float x, float y);
	bool HandleEditAccountClick(const Layout &layout, float x, float y);
	bool IsDeleteArmed() const;
	void NotifyDeleteArmed() const;
	void Notify(std::string_view message) const;

	void SaveEditedAccount();
	void DeleteEditedAccount();

	ECursorKind AccountListCursor(const Layout &layout) const;
	ECursorKind EditAccountCursor(const Layout &layout) const;

	void DrawPanelChrome(CDrawList &drawList, const Layout &layout, const Banner &banner, u8 alpha) const;
	void DrawAccountList(CDrawList &drawList, const Layout &layout, u8 alpha) const;
	void DrawAccountRow(CDrawList &drawList, Rect right, Rect row, const Account &account, bool isSelected,
						float deleteArmedAmount, u8 alpha) const;
	void DrawLoginProgress(CDrawList &drawList, Rect right, u8 alpha) const;
	void DrawEditAccount(CDrawList &drawList, Rect right, u8 alpha);
	void DrawEditField(CDrawList &drawList, Rect block, const char *pLabel, const CTextInput &input, bool masked,
					   u8 alpha) const;
	void DrawVisibilityChip(CDrawList &drawList, Rect right, u8 alpha) const;
	void DrawFooter(CDrawList &drawList, Rect footer, u8 alpha);
	void DrawEditFooter(CDrawList &drawList, Rect footer, u8 alpha);
	void DrawLoginProgressFooter(CDrawList &drawList, Rect footer, u8 alpha);
	void DrawAccountListFooter(CDrawList &drawList, Rect footer, u8 alpha);
	void DrawSectionTitle(CDrawList &drawList, Rect right, std::string_view title, u8 alpha) const;
	void DrawAddAccountButton(CDrawList &drawList, Rect right, u8 alpha) const;

	// Pointers for what this mutates, references for what it only reads - so the declaration
	// alone says which of its dependencies this class can change.
	CCarousel *m_pCarousel = nullptr;
	INotificationSink *m_pNotifications = nullptr;

	const CFontManager &m_fonts;
	const CWindow &m_window;
	const Settings &m_settings;
	const CAssetManager &m_assets;

	/// Two-step confirms for the two places an account can be deleted. Separate latches rather
	/// than one shared with a sentinel target, since a row index and "the edit form" are not the
	/// same kind of thing and would have to be told apart anyway.
	CConfirmLatch m_rowDeleteConfirm;
	CConfirmLatch m_editDeleteConfirm;

	bool m_bIsOpen = false;
	float m_flOpenAmount = 0.0f;
	i32 m_nBannerIndex = -1;

	/// Which row is highlighted and would log in, as a query index; -1 for none.
	i32 m_nSelectedAccountIndex = -1;

	EAccountModalMode m_mode = EAccountModalMode::AccountList;
	CLoginAttempt m_login;

	/// A login requested while another was still running; -1 for none.
	i32 m_nPendingLoginBannerIndex = -1;
	i32 m_nPendingLoginQueryIndex = -1;
	float m_flLoginElapsedSeconds = 0.0f;

	/// Only one field is focused at a time; the owner routes input to it.
	CTextInput m_editUsername;
	CTextInput m_editNote;
	CTextInput m_editPassword;

	/// -1 while adding; otherwise the query index being edited.
	i32 m_nEditAccountIndex = -1;
	bool m_bEditPasswordRevealed = false;

	/// Which other games this account is visible in. The chip opens this, and its working mask
	/// is written into the account on save.
	CGameSelectPopup m_gameSelect;

	CScrollable m_accountsScroll;

	PendingHit m_pendingRightClickRow;
};
