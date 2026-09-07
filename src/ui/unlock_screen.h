#pragma once

#include "gfx/font_manager.h"
#include "ui/text_input.h"
#include "ui/widget.h"

class CAssetManager;
class CMasterKey;
class CWindow;
struct Settings;

/// The master-password gate. Every vault has one, so this widget covers both situations that
/// creates:
///
///   Unlock - a password exists but this session has not unlocked it yet. One field.
///   Setup  - no password exists yet, or the user asked to replace theirs from Settings. Two
///            fields with a shared reveal toggle, so a typo in something nothing can recover
///            is not silently locked in.
///
/// The whole app is blocked behind this while active: every other widget exists underneath but
/// input does not reach it. That falls out of the stack's gating cascade, with no
/// special-casing needed - only the title bar's window buttons still work, since it dispatches
/// above everything regardless.
///
/// Setup mode is not cancelable. A first run has nothing to cancel back to, and a
/// voluntary reset already closed Settings before this activated, so there is nowhere sensible
/// to return to either.
///
/// The owner activates the right mode from what CStorage::Load reported, then polls the two
/// Consume methods. Their follow-ups differ: an unlock needs a reload, to backfill the account
/// passwords left blank while locked, while a setup needs a full save, to re-encrypt every
/// password under the fresh key and persist the new parameters.
class CUnlockScreen : public CWidget {
  public:
	/// All held by reference for this widget's lifetime. settings is mutable because a
	/// successful setup writes the new key parameters into it directly.
	CUnlockScreen(const CFontManager &fonts, const CWindow &window, Settings *pSettings, CMasterKey *pMasterKey,
				  const CAssetManager &assets);

	void ActivateForUnlock();
	void ActivateForSetup();
	void Deactivate();

	void Update(float deltaSeconds) override;
	void Draw(CDrawList &drawList) override;

	/// Routes to whichever field is focused - there is always exactly one, in either mode.
	bool OnChar(u32 character) override;

	/// Return attempts the unlock or the submit, Tab swaps fields in setup mode, and everything
	/// else routes into the focused field. No Escape handling in either mode; see this class's
	/// note on why setup is not cancelable, and unlock has nothing to cancel back to.
	bool OnKeyDown(u32 keyCode) override;

	/// Focuses the clicked field, toggles the reveal, or attempts the submit. Consumes every
	/// click while active.
	bool OnPointerUp(float x, float y) override;

	/// These carry no affordance here but must still be swallowed rather than falling through to
	/// what is underneath - a wheel notch does not need real cursor coordinates to scroll
	/// something invisible.
	bool OnPointerDown(float x, float y) override
	{
		return IsBlocking();
	}

	bool OnScroll(float x, float y, float wheelDelta) override
	{
		return IsBlocking();
	}

	bool OnRightPointerUp(float x, float y) override
	{
		return IsBlocking();
	}

	bool IsBlocking() const override
	{
		return m_bActive;
	}

	ECursorKind GetDesiredCursor() const override;

	/// Both one-shot: true for the frame the attempt succeeded, then cleared.
	bool ConsumeUnlockSucceeded();
	bool ConsumeSetupSucceeded();

  private:
	/// Tries the typed password against the persisted parameters, clears the field either way,
	/// and leaves an inline error plus focus for a retry on failure.
	bool AttemptUnlock();

	/// Validates that both fields are non-empty and agree before ever deriving a key, so a
	/// genuine derivation failure is never confused with a mistyped confirmation.
	bool AttemptSetup();

	void DrawUnlockCard(CDrawList &drawList, Rect card);
	void DrawSetupCard(CDrawList &drawList, Rect card);
	void DrawPasswordField(CDrawList &drawList, Rect field, CTextInput &input);

	const CFontManager &m_fonts;
	const CWindow &m_window;
	Settings *m_pSettings = nullptr;
	CMasterKey *m_pMasterKey = nullptr;
	const CAssetManager &m_assets;

	bool m_bActive = false;
	bool m_bSetupMode = false;

	CTextInput m_passwordInput;
	bool m_bWrongPassword = false; // unlock mode's inline error, shown until the next attempt

	/// Setup's two fields share one flag, since they represent the same value being
	/// double-checked. Reset on every activation so one mode's state never carries into the next.
	bool m_bPasswordRevealed = false;

	CTextInput m_confirmPasswordInput;
	bool m_bPasswordMismatch = false; // the two fields disagreed on the last submit
	bool m_bSetupFailed = false;	  // key derivation itself failed - rare, but real

	bool m_bUnlockSucceededThisFrame = false;
	bool m_bSetupSucceededThisFrame = false;
};
