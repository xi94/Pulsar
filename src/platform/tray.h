#pragma once

#include <Windows.h>

#include "core/types.h"
#include "gfx/asset_manager.h"

// A tray icon backed by a hidden message-only window, sharing the main window's thread and
// message pump - the frame loop just calls TakeEvent after pumping to see what happened.
//
// The context menu is owner-drawn to match the app's palette: every game with its icon and a
// submenu of the accounts visible under it, then a separator, then Show and Exit. It is
// rebuilt from the live carousel every time it opens, so account edits show up immediately
// with nothing to invalidate.

enum class ETrayEventType : u8 {
	None,
	ShowWindow,
	ExitRequested,
	QuickLogin, // GetPendingBannerIndex and GetPendingAccountIndex identify the account
};

constexpr u32 kTrayMaxGames = 16;
constexpr u32 kTrayMaxAccountItems = 256;

struct TrayGameItem {
	char Title[64];
	i32 BannerIndex;
	u32 FirstAccount; // index into TrayMenuModel::Accounts
	u32 AccountCount;
};

/// BannerIndex is the game this row is listed under and QueryIndex is its position within that
/// banner's visible-account query - not the owning banner and a raw account index. That pairing
/// is what makes a cross-visible account log into the game the user actually picked it under.
struct TrayAccountItem {
	/// The account's note when it has one, otherwise its username. The note is the name a
	/// person actually gave the account, so it identifies the row better than a login does, and
	/// the fallback means a row is never blank.
	char Label[64];
	i32 BannerIndex;
	i32 QueryIndex;
};

struct TrayMenuModel {
	TrayGameItem Games[kTrayMaxGames];
	u32 GameCount;
	TrayAccountItem Accounts[kTrayMaxAccountItems];
	u32 AccountCount;
};

/// Fills the model for one menu open. Called synchronously from inside the menu handler,
/// because TrackPopupMenu blocks the frame loop's thread for as long as the menu is open and
/// nothing polled once per frame could answer in time. Read-only.
using TrayMenuCallback = void (*)(void *pUserData, TrayMenuModel &outModel);

/// One owner-drawn row. Lives in CTray for the duration of one menu; the pointer is what the
/// menu item carries as its data.
struct TrayMenuEntry {
	wchar_t szLabel[96];
	HBITMAP hIcon;
	bool bIndent;
	bool bSubmenu;
	bool bSeparator;
	bool bDisabled;
};

constexpr u32 kTrayMaxMenuEntries = kTrayMaxAccountItems + kTrayMaxGames + 8;

class CTray {
  public:
	CTray() = default;
	~CTray();

	CTray(const CTray &) = delete;
	CTray &operator=(const CTray &) = delete;

	/// False only if the message-only window itself could not be created. A failed
	/// Shell_NotifyIcon is retried on a timer and again whenever Explorer restarts, so it is
	/// not fatal here.
	bool Create(const wchar_t *pTooltip);

	void SetMenuCallback(TrayMenuCallback callback, void *pUserData);

	/// Decodes an embedded PNG into the menu's icon column. Safe to skip; a game without one
	/// just draws no icon.
	void SetGameIcon(i32 bannerIndex, const u8 *pPngBytes, u64 length);

	/// Drives the menu's hover highlight. Call whenever the accent may have changed.
	void SetAccentColor(Color accent);

	bool IsIconVisible() const
	{
		return m_bIconAdded;
	}

	ETrayEventType TakeEvent();

	i32 GetPendingBannerIndex() const
	{
		return m_nPendingBannerIndex;
	}

	i32 GetPendingAccountIndex() const
	{
		return m_nPendingAccountIndex;
	}

  private:
	static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
	void HandleCommand(UINT commandId);

	bool AddIcon();
	void RemoveIcon();
	void RebuildBrushes();

	void ShowContextMenu();
	HMENU BuildMenu();
	HMENU BuildGameSubmenu(const TrayGameItem &game);

	/// The menu bitmap for one game, decoded on first use and cached from then on.
	HBITMAP GameIcon(i32 bannerIndex);

	/// Copies the row into m_entries and appends it, carrying the stored entry as the item's
	/// data. Null if the entry table is full.
	TrayMenuEntry *PushEntry(const TrayMenuEntry &entry);
	void AppendRow(HMENU target, UINT flags, UINT_PTR id, const TrayMenuEntry &entry);
	void AppendCommand(HMENU target, UINT_PTR id, const wchar_t *pLabel, bool bIndent);
	void AppendSubmenu(HMENU target, HMENU submenu, const wchar_t *pLabel, HBITMAP hIcon);
	void AppendPlaceholder(HMENU target, const wchar_t *pLabel, bool bIndent);
	void AppendSeparator(HMENU target);

	void OnMeasureItem(MEASUREITEMSTRUCT *pMeasure) const;
	void OnDrawItem(const DRAWITEMSTRUCT *pDraw) const;

	HWND m_hWnd = nullptr;
	HICON m_hIcon = nullptr;
	bool m_bIconAdded = false;
	u32 m_addAttempts = 0;
	wchar_t m_szTooltip[128]{};

	HFONT m_hMenuFont = nullptr;
	HBRUSH m_hBackBrush = nullptr;
	HBRUSH m_hHoverBrush = nullptr;
	Color m_accent{108, 90, 220, 255};
	/// The undecoded bytes, kept until a menu actually needs the bitmap - see SetGameIcon.
	EmbeddedImageBytes m_gameIconSources[kTrayMaxGames]{};
	HBITMAP m_gameIcons[kTrayMaxGames]{};

	ETrayEventType m_pendingEvent = ETrayEventType::None;
	i32 m_nPendingBannerIndex = -1;
	i32 m_nPendingAccountIndex = -1;

	TrayMenuModel m_model{};
	TrayMenuEntry m_entries[kTrayMaxMenuEntries]{};
	u32 m_entryCount = 0;

	TrayMenuCallback m_pMenuCallback = nullptr;
	void *m_pMenuCallbackUserData = nullptr;
};
