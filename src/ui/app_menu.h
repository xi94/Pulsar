#pragma once

#include "gfx/font_manager.h"
#include "ui/widget.h"

class CAssetManager;
struct Settings;

// The rounded popup anchored under the title bar's Menu button, drawn rather than handed to a
// native TrackPopupMenu. Two groups split by a hairline: Check for Updates, which acts on this
// build, then Settings and Open Data Folder, which are both about this install's
// configuration.
//
// No Exit item - the title bar's close button already does that - and no version strip: a
// number nobody can act on does not earn a permanent line in a menu.
//
// Rows come from a table in the .cpp rather than a hand-written rect, hover and draw trio
// each. This menu grew from three items to four, and each of those three had its own
// copy-pasted hover branch - which is exactly how a fifth ends up subtly different.
//
// While open, every input event is swallowed regardless of kind or where it lands, which is
// why every override below returns IsBlocking unconditionally.

enum class EAppMenuAction : u8 {
	None,
	OpenSettings,
	OpenDataFolder,

	/// The owner kicks a fresh check if one is not already due, then opens the update overlay
	/// either way - so this row always shows something rather than being a silent no-op. The
	/// row's label stays static; the dynamic feedback lives on the title bar's update pill.
	CheckForUpdates,
};

class CAppMenu : public CWidget {
  public:
	/// settings is read only, so the hover highlight follows the user's accent instead of a
	/// hardcoded purple. appLocked is a live reference too, purely to grey out and disable the
	/// Settings row while the vault is locked: this menu is reachable on the master-password
	/// screen, but the settings panel behind it is not, and a click that silently does nothing
	/// with no visual hint is worse than a disabled row.
	CAppMenu(const CFontManager &fonts, const CAssetManager &assets, const Settings &settings, const bool &appLocked);

	void Open();
	void Close();

	/// Runs every frame whether open or not, so a closing menu animates out instead of vanishing
	/// and the hover highlight grows under the cursor rather than snapping between rows.
	void Update(float deltaSeconds) override;
	void Draw(CDrawList &drawList) override;

	bool OnPointerDown(float x, float y) override
	{
		return IsBlocking();
	}

	bool OnPointerMove(float x, float y) override
	{
		return IsBlocking();
	}

	/// Any click while blocking dismisses the menu and latches whichever item, if any, was hit.
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

	bool IsBlocking() const override
	{
		return m_flOpenAmount > 0.01f;
	}

	ECursorKind GetDesiredCursor() const override;

	/// Cleared on read; an owner polls this once per frame.
	EAppMenuAction ConsumeAction();

	/// Public only so the item table's static_assert can check itself against it.
	static constexpr u64 kMaxItems = 8;

  private:
	/// Only ever false for Settings while locked, but asked as a per-item question so the draw,
	/// click and cursor paths all answer it the same way.
	bool IsItemEnabled(u64 index) const;

	const CFontManager &m_fonts;
	const CAssetManager &m_assets;
	const Settings &m_settings;
	const bool &m_appLocked;

	bool m_bOpen = false;
	float m_flOpenAmount = 0.0f;

	/// Indexed the same as the item table. Sized off the cap above rather than that table's
	/// length, which this header cannot see - the static_assert beside it keeps the two honest.
	float m_aItemHoverAmount[kMaxItems]{};

	EAppMenuAction m_pendingAction = EAppMenuAction::None;
};
