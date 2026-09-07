#pragma once

#include "core/crypto.h"
#include "core/types.h"

/// The persisted preferences record, separate from CSettingsPanel, which owns
/// only the transient UI state of the dialog that edits this.
struct Settings {
	u32 m_nWindowWidth = 1042;
	u32 m_nWindowHeight = 675;

	bool m_bAnimationsEnabled = true;
	float m_flAnimationSpeed = 1.0f;

	/// Multiplies every corner radius the UI asks for; 0 squares everything off, which is
	/// what replaced a separate on/off toggle.
	float m_flCornerRoundness = 1.0f;

	/// Nominal display units, not baked pixels - see CFont for why the two differ.
	float m_flFontPixelSize = 14.0f;
	float m_flSecondaryFontPixelSize = 12.0f;

	Color m_clrAccent{108, 90, 220, 255};
	char m_szFontName[64] = "segoeui.ttf";

	/// Keeps usernames, notes and revealed passwords out of screenshots and screen shares
	/// while the account modal is open. On by default: the kind of protection a user would
	/// want but would never think to go looking for.
	bool m_bExcludeAccountListFromCapture = true;

	/// Closing hides to the tray instead of quitting; the tray's Exit item is then what ends the
	/// app. Minimizing is unaffected and always goes to the taskbar.
	bool m_bCloseToTray = true;

	/// The version that last ran to completion. Compared against the running build at startup:
	/// different means this launch is the first after an update, which is the one moment worth
	/// telling the user about. Empty on a genuine first run, which says nothing.
	char m_szLastRunVersion[32] = "";

	/// Corner-of-the-screen confirmations for actions that otherwise complete silently - an
	/// account saved or deleted, a setting restored. On by default, since an action with no
	/// acknowledgement reads as one that did not happen.
	bool m_bShowNotifications = true;

	/// Refuses the DLL-injection route Discord's overlay and most keyloggers both use. On by
	/// default, and a setting rather than unconditional because the same policy disables
	/// third-party IMEs - see platform/overlay_guard.h. Read once at startup; changing it takes
	/// effect on the next launch.
	bool m_bBlockOverlayInjection = true;

	/// What CMasterKey::Set produced. None of it is secret on its own, and all of it has to
	/// be persisted verbatim - re-reading opsLimit/memLimit from today's defaults would
	/// invalidate a vault the moment those defaults are ever retuned.
	bool m_bMasterPasswordEnabled = false;
	u8 m_aMasterPasswordSalt[CCrypto::kSaltSize]{};
	u64 m_masterPasswordOpsLimit = 0;
	usize m_masterPasswordMemLimit = 0;
	u8 m_aMasterPasswordWrapNonce[CCrypto::kNonceSize]{};
	u8 m_aMasterPasswordWrappedDek[CCrypto::kKeySize + CCrypto::kTagSize]{};
};
