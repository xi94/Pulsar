#include "ui/unlock_screen.h"

#include <cstring>

#include <Windows.h>

#include "core/master_key.h"
#include "core/settings.h"
#include "gfx/asset_manager.h"
#include "platform/window.h"
#include "ui/controls.h"
#include "ui/draw_list.h"
#include "ui/text.h"

namespace {
constexpr float kCardWidth = 380.0f;
constexpr float kUnlockCardHeight = 210.0f;

// Title, a two-line description, two fields, a gap, the button, and a trailing error line.
constexpr float kSetupCardHeight = 366.0f;

constexpr float kCardRadius = 16.0f;
constexpr float kCardPadding = 28.0f;
constexpr float kFieldHeight = 40.0f;
constexpr float kButtonHeight = 40.0f;
constexpr float kGap = 14.0f;
constexpr float kRevealButtonSize = 22.0f;

// Where the first field sits below the card's title and description, which differ in height
// between the two modes.
constexpr float kUnlockFieldOffsetY = 74.0f;
constexpr float kSetupFieldOffsetY = 96.0f;

constexpr Color kColorBackdrop{12, 12, 14, 255};
constexpr Color kColorCard{26, 26, 30, 255};
constexpr Color kColorFieldBg{24, 24, 27, 255};
constexpr Color kColorFieldBorder{40, 40, 45, 255};
constexpr Color kColorTextBright{232, 232, 236, 255};
constexpr Color kColorTextDim{150, 150, 156, 255};
constexpr Color kColorError{220, 90, 80, 255};

// Always centred, unlike the modal and settings panel which scale against the window: a fixed
// small card is all this screen ever needs.
Rect CardRect(float windowW, float windowH, bool setupMode)
{
	const float h = setupMode ? kSetupCardHeight : kUnlockCardHeight;

	return Rect{(windowW - kCardWidth) * 0.5f, (windowH - h) * 0.5f, kCardWidth, h};
}

Rect FirstFieldRect(Rect card, bool setupMode)
{
	const float offsetY = setupMode ? kSetupFieldOffsetY : kUnlockFieldOffsetY;

	return Rect{card.X + kCardPadding, card.Y + offsetY, card.W - kCardPadding * 2.0f, kFieldHeight};
}

Rect ConfirmFieldRect(Rect card)
{
	const Rect password = FirstFieldRect(card, true);

	return Rect{password.X, password.Y + kFieldHeight + kGap, password.W, kFieldHeight};
}

Rect SubmitButtonRect(Rect card, bool setupMode)
{
	const Rect last = setupMode ? ConfirmFieldRect(card) : FirstFieldRect(card, false);

	return Rect{last.X, last.Y + last.H + kGap, last.W, kButtonHeight};
}

// Sits inside its own field's right edge, which is why hit-testing has to check it before the
// field itself.
Rect RevealButtonRect(Rect field)
{
	return Rect{field.X + field.W - kRevealButtonSize - 6.0f, field.Y + (field.H - kRevealButtonSize) * 0.5f,
				kRevealButtonSize, kRevealButtonSize};
}

// This screen rings a focused field in the accent rather than a neutral border, which is the one
// way its fields differ from every other panel's.
void DrawFieldChrome(CDrawList &drawList, Rect field, bool focused, Color accent)
{
	constexpr float kFieldCornerRadius = 6.0f;

	Controls::DrawFieldChrome(drawList, field, kFieldCornerRadius, focused ? accent : kColorFieldBorder, kColorFieldBg,
							  255);
}
} // namespace

CUnlockScreen::CUnlockScreen(const CFontManager &fonts, const CWindow &window, Settings *pSettings,
							 CMasterKey *pMasterKey, const CAssetManager &assets)
	: m_fonts(fonts)
	, m_window(window)
	, m_pSettings(pSettings)
	, m_pMasterKey(pMasterKey)
	, m_assets(assets)
{
}

void CUnlockScreen::ActivateForUnlock()
{
	m_bSetupMode = false;
	m_passwordInput.Init("");
	m_passwordInput.m_bFocused = true;
	m_bWrongPassword = false;
	m_bPasswordRevealed = false;
	m_bActive = true;
}

void CUnlockScreen::ActivateForSetup()
{
	m_bSetupMode = true;
	m_passwordInput.Init("");
	m_confirmPasswordInput.Init("");
	m_passwordInput.m_bFocused = true;
	m_bPasswordRevealed = false;
	m_bPasswordMismatch = false;
	m_bSetupFailed = false;
	m_bActive = true;
}

void CUnlockScreen::Deactivate()
{
	m_bActive = false;
}

bool CUnlockScreen::ConsumeUnlockSucceeded()
{
	const bool bSucceeded = m_bUnlockSucceededThisFrame;
	m_bUnlockSucceededThisFrame = false;

	return bSucceeded;
}

bool CUnlockScreen::ConsumeSetupSucceeded()
{
	const bool bSucceeded = m_bSetupSucceededThisFrame;
	m_bSetupSucceededThisFrame = false;

	return bSucceeded;
}

bool CUnlockScreen::AttemptUnlock()
{
	const bool bUnlocked =
		m_pMasterKey->Unlock(m_passwordInput.GetValue(), m_pSettings->m_aMasterPasswordSalt,
							 m_pSettings->m_masterPasswordOpsLimit, m_pSettings->m_masterPasswordMemLimit,
							 m_pSettings->m_aMasterPasswordWrapNonce, m_pSettings->m_aMasterPasswordWrappedDek);

	m_passwordInput.SetValue("");
	m_bWrongPassword = !bUnlocked;
	m_bUnlockSucceededThisFrame = bUnlocked;

	// A failed attempt keeps focus for the retry.
	m_passwordInput.m_bFocused = !bUnlocked;

	return bUnlocked;
}

bool CUnlockScreen::AttemptSetup()
{
	if (m_passwordInput.GetValue().empty()) return false;

	if (m_passwordInput.GetValue() != m_confirmPasswordInput.GetValue()) {
		m_bPasswordMismatch = true;
		m_bSetupFailed = false;
		m_confirmPasswordInput.SetValue("");
		m_confirmPasswordInput.m_bFocused = true;

		return false;
	}

	if (!m_pMasterKey->Set(m_passwordInput.GetValue())) {
		// Argon2id or the wrap itself failed - rare, mainly an out-of-memory situation under
		// the moderate preset's cost, but a real outcome rather than a hypothetical.
		m_bSetupFailed = true;
		m_bPasswordMismatch = false;

		return false;
	}

	m_pSettings->m_bMasterPasswordEnabled = true;
	m_pSettings->m_masterPasswordOpsLimit = m_pMasterKey->m_opsLimit;
	m_pSettings->m_masterPasswordMemLimit = m_pMasterKey->m_memLimit;
	std::memcpy(m_pSettings->m_aMasterPasswordSalt, m_pMasterKey->m_aSalt, sizeof(m_pSettings->m_aMasterPasswordSalt));
	std::memcpy(m_pSettings->m_aMasterPasswordWrapNonce, m_pMasterKey->m_aWrapNonce,
				sizeof(m_pSettings->m_aMasterPasswordWrapNonce));
	std::memcpy(m_pSettings->m_aMasterPasswordWrappedDek, m_pMasterKey->m_aWrappedDek,
				sizeof(m_pSettings->m_aMasterPasswordWrappedDek));

	m_passwordInput.SetValue("");
	m_confirmPasswordInput.SetValue("");
	m_bPasswordMismatch = false;
	m_bSetupFailed = false;
	m_bSetupSucceededThisFrame = true;

	return true;
}

void CUnlockScreen::Update(float deltaSeconds)
{
	if (!m_bActive) return;

	m_passwordInput.Update(deltaSeconds);

	if (m_bSetupMode) {
		m_confirmPasswordInput.Update(deltaSeconds);
	}
}

bool CUnlockScreen::OnChar(u32 character)
{
	if (!m_bActive) return false;

	// A no-op on an unfocused field, so routing to both is safe - exactly one is ever focused.
	m_passwordInput.OnChar(character);

	if (m_bSetupMode) {
		m_confirmPasswordInput.OnChar(character);
	}

	return true;
}

bool CUnlockScreen::OnKeyDown(u32 keyCode)
{
	if (!m_bActive) return false;

	if (keyCode == VK_RETURN) {
		if (m_bSetupMode) {
			AttemptSetup();
		} else {
			AttemptUnlock();
		}

		return true;
	}

	// Setup only - unlock has one field with nothing to cycle to. Plain Tab swaps regardless of
	// Shift, since with exactly two fields forward and backward land in the same place.
	if (m_bSetupMode && keyCode == VK_TAB) {
		const bool wasOnPassword = m_passwordInput.m_bFocused;
		m_passwordInput.m_bFocused = !wasOnPassword;
		m_confirmPasswordInput.m_bFocused = wasOnPassword;
		(wasOnPassword ? m_confirmPasswordInput : m_passwordInput).OnKey(VK_END);

		return true;
	}

	m_passwordInput.OnKey(keyCode);

	if (m_bSetupMode) {
		m_confirmPasswordInput.OnKey(keyCode);
	}

	return true;
}

bool CUnlockScreen::OnPointerUp(float x, float y)
{
	if (!m_bActive) return false;

	const Rect card =
		CardRect(static_cast<float>(m_window.GetWidth()), static_cast<float>(m_window.GetHeight()), m_bSetupMode);
	const Rect passwordField = FirstFieldRect(card, m_bSetupMode);

	const bool clickedPassword = RectContainsPoint(passwordField, x, y);
	const bool clickedConfirm = m_bSetupMode && RectContainsPoint(ConfirmFieldRect(card), x, y);

	if (clickedPassword || clickedConfirm) {
		m_passwordInput.m_bFocused = clickedPassword;
		m_confirmPasswordInput.m_bFocused = clickedConfirm;
		// Positions the cursor at the end of the possibly seeded value.
		(clickedPassword ? m_passwordInput : m_confirmPasswordInput).OnKey(VK_END);
	}

	// Both fields share one reveal state, but each draws and hit-tests its own button.
	const bool clickedReveal = RectContainsPoint(RevealButtonRect(passwordField), x, y) ||
							   (m_bSetupMode && RectContainsPoint(RevealButtonRect(ConfirmFieldRect(card)), x, y));
	if (clickedReveal) {
		m_bPasswordRevealed = !m_bPasswordRevealed;
	}

	if (RectContainsPoint(SubmitButtonRect(card, m_bSetupMode), x, y)) {
		if (m_bSetupMode) {
			AttemptSetup();
		} else {
			AttemptUnlock();
		}
	}

	return true;
}

ECursorKind CUnlockScreen::GetDesiredCursor() const
{
	if (!m_bActive) return ECursorKind::Arrow;

	const Rect card =
		CardRect(static_cast<float>(m_window.GetWidth()), static_cast<float>(m_window.GetHeight()), m_bSetupMode);
	const Rect passwordField = FirstFieldRect(card, m_bSetupMode);
	const Rect confirmField = ConfirmFieldRect(card);

	// Reveal buttons sit inside their field's right edge, so they have to be checked first -
	// the field would always win that overlap and the button would only ever show an I-beam.
	const bool overButton = RectContainsPoint(RevealButtonRect(passwordField), m_flMouseX, m_flMouseY) ||
							RectContainsPoint(SubmitButtonRect(card, m_bSetupMode), m_flMouseX, m_flMouseY) ||
							(m_bSetupMode && RectContainsPoint(RevealButtonRect(confirmField), m_flMouseX, m_flMouseY));
	if (overButton) return ECursorKind::Hand;

	const bool overField = RectContainsPoint(passwordField, m_flMouseX, m_flMouseY) ||
						   (m_bSetupMode && RectContainsPoint(confirmField, m_flMouseX, m_flMouseY));

	return overField ? ECursorKind::IBeam : ECursorKind::Arrow;
}

void CUnlockScreen::DrawPasswordField(CDrawList &drawList, Rect field, CTextInput &input)
{
	const Color accent = m_pSettings->m_clrAccent;

	DrawFieldChrome(drawList, field, input.m_bFocused, accent);
	input.Draw(drawList, m_fonts.GetBody(), field.X, field.Y, field.W, field.H, kColorTextBright, accent,
			   !m_bPasswordRevealed);

	const Rect reveal = RevealButtonRect(field);
	const bool hovered = RectContainsPoint(reveal, m_flMouseX, m_flMouseY);
	Controls::DrawEyeGlyph(drawList, m_assets, reveal, m_bPasswordRevealed, hovered ? kColorTextBright : kColorTextDim);
}

void CUnlockScreen::Draw(CDrawList &drawList)
{
	if (!m_bActive) return;

	const auto windowW = static_cast<float>(m_window.GetWidth());
	const auto windowH = static_cast<float>(m_window.GetHeight());

	// Stops short of the status bar rather than covering the whole window: that strip is drawn
	// before this widget, and a backdrop reaching all the way down painted over it entirely.
	drawList.AddRectFilled(0.0f, 0.0f, windowW, windowH - kStatusBarHeight, kColorBackdrop);

	const Rect card = CardRect(windowW, windowH, m_bSetupMode);
	drawList.AddRectRoundedFilled(card.X, card.Y, card.W, card.H, CDrawList::UniformRadii(kCardRadius), kColorCard);

	if (m_bSetupMode) {
		DrawSetupCard(drawList, card);
	} else {
		DrawUnlockCard(drawList, card);
	}
}

void CUnlockScreen::DrawUnlockCard(CDrawList &drawList, Rect card)
{
	const CFont &body = m_fonts.GetBody();
	const CFont &secondary = m_fonts.GetSecondary();
	const Color accent = m_pSettings->m_clrAccent;
	const float textX = card.X + kCardPadding;

	DrawText(drawList, body, textX, card.Y + kCardPadding + body.GetAscent(), "Master Password", kColorTextBright);
	DrawText(drawList, secondary, textX, card.Y + kCardPadding + body.GetLineHeight() + 4.0f + secondary.GetAscent(),
			 "Enter your master password to continue.", kColorTextDim);

	DrawPasswordField(drawList, FirstFieldRect(card, false), m_passwordInput);

	const Rect button = SubmitButtonRect(card, false);
	drawList.AddRectRoundedBordered(button.X, button.Y, button.W, button.H, CDrawList::UniformRadii(8.0f), accent,
									ColorOutlineOn(accent), 1.0f);
	DrawCenteredText(drawList, body, button.X, button.Y, button.W, button.H, "Unlock", ColorForegroundOn(accent));

	if (m_bWrongPassword) {
		DrawText(drawList, secondary, textX, button.Y + button.H + kGap + secondary.GetAscent(), "Incorrect password.",
				 kColorError);
	}
}

void CUnlockScreen::DrawSetupCard(CDrawList &drawList, Rect card)
{
	const CFont &body = m_fonts.GetBody();
	const CFont &secondary = m_fonts.GetSecondary();
	const Color accent = m_pSettings->m_clrAccent;
	const float textX = card.X + kCardPadding;

	DrawText(drawList, body, textX, card.Y + kCardPadding + body.GetAscent(), "Create a Master Password",
			 kColorTextBright);

	const float descriptionY = card.Y + kCardPadding + body.GetLineHeight() + 4.0f + secondary.GetAscent();
	DrawText(drawList, secondary, textX, descriptionY, "This encrypts your saved account passwords. Choose",
			 kColorTextDim);
	DrawText(drawList, secondary, textX, descriptionY + secondary.GetLineHeight(),
			 "something memorable - it can't be recovered if lost.", kColorTextDim);

	DrawPasswordField(drawList, FirstFieldRect(card, true), m_passwordInput);
	DrawPasswordField(drawList, ConfirmFieldRect(card), m_confirmPasswordInput);

	const Rect button = SubmitButtonRect(card, true);
	drawList.AddRectRoundedBordered(button.X, button.Y, button.W, button.H, CDrawList::UniformRadii(8.0f), accent,
									ColorOutlineOn(accent), 1.0f);
	DrawCenteredText(drawList, body, button.X, button.Y, button.W, button.H, "Create Password",
					 ColorForegroundOn(accent));

	if (m_bPasswordMismatch || m_bSetupFailed) {
		const std::string_view message =
			m_bPasswordMismatch ? "Passwords don't match." : "Something went wrong - try again.";
		DrawText(drawList, secondary, textX, button.Y + button.H + kGap + secondary.GetAscent(), message, kColorError);
	}
}
