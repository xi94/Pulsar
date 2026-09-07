#include "ui/update_overlay.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "core/settings.h"
#include "core/updater.h"
#include "core/app_identity.h"
#include "gfx/font_manager.h"
#include "platform/window.h"
#include "ui/controls.h"
#include "ui/layout.h"
#include "ui/draw_list.h"
#include "ui/text.h"

namespace {
constexpr float kCardWidth = 460.0f;

// Fixed: the notes box scrolls its own overflow rather than the card growing to fit it.
constexpr float kCardHeight = 300.0f;

constexpr float kCardRadius = 16.0f;
constexpr float kCardPadding = 28.0f;
constexpr float kButtonHeight = 40.0f;
constexpr float kGap = 14.0f;
constexpr float kProgressBarHeight = 10.0f;
constexpr float kCloseButtonSize = 28.0f;

constexpr float kNotesBoxPadding = 12.0f;
constexpr float kNotesScrollbarMargin = 14.0f; // reserved on the box's right edge; text never wraps into it
constexpr u32 kMaxNotesLines = 256;
constexpr u32 kMaxMessageLines = 4;

constexpr Color kColorCard{26, 26, 30, 255};
constexpr Color kColorCardBorder{54, 54, 62, 255};

/// A hairline along the card's top edge - the usual "catching light from above" cue for a raised
/// surface, which reads better than a drop shadow alone against an already-dark backdrop.
constexpr Color kColorCardTopEdge{92, 92, 104, 90};

constexpr Color kColorNotesBg{22, 22, 25, 255};
constexpr Color kColorNotesBorder{44, 44, 52, 255};
constexpr Color kColorTextBright{232, 232, 236, 255};
constexpr Color kColorTextDim{150, 150, 156, 255};
constexpr Color kColorError{220, 90, 80, 255};
constexpr Color kColorTrack{40, 40, 45, 255};
constexpr Color kColorNeutralButton{40, 40, 45, 255};
constexpr Color kColorNeutralButtonHover{56, 56, 62, 255};
constexpr Color kColorScrollThumb{120, 120, 128, 190};

// Semi-transparent, unlike the unlock screen's opaque backdrop: this can be dismissed, so
// whatever is behind it staying faintly visible reads as "on top of the app" rather than "a
// separate screen".
constexpr Color kColorBackdrop{8, 8, 10, 200};

Rect CloseButtonRect(Rect card)
{
	return Rect{card.X + card.W - kCloseButtonSize - 14.0f, card.Y + 14.0f, kCloseButtonSize, kCloseButtonSize};
}

Rect PrimaryButtonRect(Rect card)
{
	return Rect{card.X + kCardPadding, card.Y + card.H - kCardPadding - kButtonHeight, card.W - kCardPadding * 2.0f,
				kButtonHeight};
}

// Just past where the title and subtitle actually end, so the box's top cannot drift out of sync
// with them at a different font size.
float NotesBoxTopY(Rect card, const CFontManager &fonts)
{
	const CFont &body = fonts.GetBody();
	const CFont &secondary = fonts.GetSecondary();
	const float titleBaselineY = card.Y + kCardPadding + body.GetAscent();
	const float subtitleBaselineY = titleBaselineY + body.GetLineHeight() + 10.0f + secondary.GetAscent();

	return subtitleBaselineY + secondary.GetDescent() + kGap;
}

// From past the title down to just above the primary button, so how much scrolls is whatever
// is left over rather than a hand-tuned number that could drift from the button's position.
Rect NotesBoxRect(Rect card, const CFontManager &fonts)
{
	const float top = NotesBoxTopY(card, fonts);
	const float bottom = PrimaryButtonRect(card).Y - kGap;

	return Rect{card.X + kCardPadding, top, card.W - kCardPadding * 2.0f, bottom - top};
}

// Not while something is in flight: backing out is only disruptive in exactly that window.
bool StageIsDismissable(EUpdateStage stage)
{
	return stage != EUpdateStage::Downloading && stage != EUpdateStage::Verifying && stage != EUpdateStage::Installing;
}

bool StageHasPrimaryAction(EUpdateStage stage)
{
	switch (stage) {
		case EUpdateStage::Available:
		case EUpdateStage::Downloading:
		case EUpdateStage::Error:
		case EUpdateStage::Cancelled:
		case EUpdateStage::UpToDate:
		case EUpdateStage::CheckFailed:
			return true;

		default:
			return false;
	}
}

void FormatBytes(u64 bytes, char *pOut, usize outCapacity)
{
	if (bytes >= 1024ull * 1024ull) {
		std::snprintf(pOut, outCapacity, "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
	} else {
		std::snprintf(pOut, outCapacity, "%.0f KB", static_cast<double>(bytes) / 1024.0);
	}
}

void FormatSpeed(double bytesPerSecond, char *pOut, usize outCapacity)
{
	if (bytesPerSecond >= 1024.0 * 1024.0) {
		std::snprintf(pOut, outCapacity, "%.1f MB/s", bytesPerSecond / (1024.0 * 1024.0));
	} else {
		std::snprintf(pOut, outCapacity, "%.0f KB/s", bytesPerSecond / 1024.0);
	}
}
} // namespace

CUpdateOverlay::CUpdateOverlay(const CFontManager &fonts, const CWindow &window, Settings *pSettings,
							   CUpdater *pUpdater)
	: m_fonts(fonts)
	, m_window(window)
	, m_pSettings(pSettings)
	, m_pUpdater(pUpdater)
{
}

void CUpdateOverlay::Open()
{
	m_bActive = true;
}

void CUpdateOverlay::Close()
{
	m_bActive = false;
}

void CUpdateOverlay::Update(float deltaSeconds)
{
	m_notesScroll.Update(deltaSeconds);
}

Rect CUpdateOverlay::CardRect() const
{
	const auto windowW = static_cast<float>(m_window.GetWidth());
	const auto windowH = static_cast<float>(m_window.GetHeight());

	return Rect{(windowW - kCardWidth) * 0.5f, (windowH - kCardHeight) * 0.5f, kCardWidth, kCardHeight};
}

CUpdateOverlay::NotesLayout CUpdateOverlay::ComputeNotesLayout() const
{
	const Rect box = NotesBoxRect(CardRect(), m_fonts);
	const CFont &secondary = m_fonts.GetSecondary();

	std::string_view lines[kMaxNotesLines];
	const u32 lineCount =
		WrapText(secondary, m_pUpdater->GetManifest().szNotes, box.W - kNotesScrollbarMargin, lines, kMaxNotesLines);

	const Rect track{box.X + box.W - kScrollbarWidth, box.Y, kScrollbarWidth, box.H};

	return NotesLayout{box, track, static_cast<float>(lineCount) * secondary.GetLineHeight()};
}

bool CUpdateOverlay::OnPointerUp(float x, float y)
{
	if (!m_bActive) return false;

	// A scrollbar drag ending on this release should not also register as a click on whatever
	// is underneath: the drag ending is the interaction.
	if (m_notesScroll.IsDragging()) {
		m_notesScroll.OnPointerUp();
		return true;
	}

	const Rect card = CardRect();
	const EUpdateStage stage = m_pUpdater->GetStage();

	if (StageIsDismissable(stage) && RectContainsPoint(CloseButtonRect(card), x, y)) {
		Close();
		return true;
	}

	if (!RectContainsPoint(PrimaryButtonRect(card), x, y)) return true;

	switch (stage) {
		case EUpdateStage::Available:
			m_pUpdater->StartDownloadAsync();
			break;

		case EUpdateStage::Downloading:
			m_pUpdater->RequestCancel();
			break;

		case EUpdateStage::Error:
		case EUpdateStage::Cancelled:
			// A plain retry against the manifest already in hand; no fresh check needed.
			m_pUpdater->StartDownloadAsync();
			break;

		case EUpdateStage::UpToDate:
		case EUpdateStage::CheckFailed:
			// A fresh fetch this time - there is no download to retry yet.
			m_pUpdater->CheckForUpdateAsync(kAppVersion);
			break;

		default:
			break;
	}

	return true;
}

bool CUpdateOverlay::OnPointerDown(float x, float y)
{
	if (!m_bActive) return false;

	if (m_pUpdater->GetStage() == EUpdateStage::Available) {
		const NotesLayout notes = ComputeNotesLayout();
		m_notesScroll.OnPointerDown(x, y, notes.Track, notes.ContentHeight, notes.Box.H);
	}

	return IsBlocking();
}

bool CUpdateOverlay::OnPointerMove(float x, float y)
{
	if (!m_bActive) return false;

	if (m_notesScroll.IsDragging()) {
		const NotesLayout notes = ComputeNotesLayout();
		m_notesScroll.OnPointerMove(y, notes.Track, notes.ContentHeight, notes.Box.H);
	}

	return IsBlocking();
}

bool CUpdateOverlay::OnScroll(float x, float y, float wheelDelta)
{
	if (!m_bActive) return false;

	if (m_pUpdater->GetStage() == EUpdateStage::Available) {
		const NotesLayout notes = ComputeNotesLayout();

		if (RectContainsPoint(notes.Box, x, y)) {
			m_notesScroll.OnScroll(wheelDelta, notes.ContentHeight, notes.Box.H);
		}
	}

	return IsBlocking();
}

ECursorKind CUpdateOverlay::GetDesiredCursor() const
{
	if (!m_bActive) return ECursorKind::Arrow;

	const Rect card = CardRect();
	const EUpdateStage stage = m_pUpdater->GetStage();

	const bool overClose =
		StageIsDismissable(stage) && RectContainsPoint(CloseButtonRect(card), m_flMouseX, m_flMouseY);
	const bool overPrimary =
		StageHasPrimaryAction(stage) && RectContainsPoint(PrimaryButtonRect(card), m_flMouseX, m_flMouseY);

	return overClose || overPrimary ? ECursorKind::Hand : ECursorKind::Arrow;
}

void CUpdateOverlay::DrawCloseButton(CDrawList &drawList, Rect card) const
{
	const Rect close = CloseButtonRect(card);
	const bool hovered = RectContainsPoint(close, m_flMouseX, m_flMouseY);

	if (hovered) {
		drawList.AddRectRoundedFilled(close.X, close.Y, close.W, close.H, CDrawList::UniformRadii(close.W * 0.5f),
									  kColorNeutralButtonHover);
	}

	const float cx = close.X + close.W * 0.5f;
	const float cy = close.Y + close.H * 0.5f;
	const Color glyphColor = hovered ? kColorTextBright : kColorTextDim;

	drawList.AddLine(cx - 5.0f, cy - 5.0f, cx + 5.0f, cy + 5.0f, 1.5f, glyphColor);
	drawList.AddLine(cx - 5.0f, cy + 5.0f, cx + 5.0f, cy - 5.0f, 1.5f, glyphColor);
}

// The title column starts to the right of the header badge, so every stage's text lines up with
// every other stage's whatever it says.
void CUpdateOverlay::DrawCardTitle(CDrawList &drawList, Rect card, float &cursorY, std::string_view title) const
{
	const CFont &body = m_fonts.GetBody();

	DrawText(drawList, body, TextColumnX(card), cursorY, title, kColorTextBright);
	cursorY += body.GetLineHeight() + 10.0f + m_fonts.GetSecondary().GetAscent();
}

float CUpdateOverlay::TextColumnX(Rect card) const
{
	return card.X + kCardPadding;
}

float CUpdateOverlay::TextColumnWidth(Rect card) const
{
	return card.X + card.W - kCardPadding - TextColumnX(card);
}

void CUpdateOverlay::DrawPrimaryButton(CDrawList &drawList, Rect card, std::string_view label, bool accented) const
{
	const Rect button = PrimaryButtonRect(card);
	const Color accent = m_pSettings->m_clrAccent;

	if (accented) {
		drawList.AddRectRoundedBordered(button.X, button.Y, button.W, button.H, CDrawList::UniformRadii(8.0f), accent,
										ColorOutlineOn(accent), 1.0f);
		DrawCenteredText(drawList, m_fonts.GetBody(), button.X, button.Y, button.W, button.H, label,
						 ColorForegroundOn(accent));
		return;
	}

	const bool hovered = RectContainsPoint(button, m_flMouseX, m_flMouseY);
	drawList.AddRectRoundedFilled(button.X, button.Y, button.W, button.H, CDrawList::UniformRadii(8.0f),
								  hovered ? kColorNeutralButtonHover : kColorNeutralButton);
	DrawCenteredText(drawList, m_fonts.GetBody(), button.X, button.Y, button.W, button.H, label, kColorTextBright);
}

// Idle only shows up here for a frame at most: opening from the menu's Check row kicks a check
// first, which moves the stage on before the next draw.
void CUpdateOverlay::DrawCheckingStage(CDrawList &drawList, Rect card, float &cursorY)
{
	DrawCardTitle(drawList, card, cursorY, "Checking for Updates");
	DrawText(drawList, m_fonts.GetSecondary(), TextColumnX(card), cursorY, "This will only take a moment.",
			 kColorTextDim);
}

void CUpdateOverlay::DrawUpToDateStage(CDrawList &drawList, Rect card, float &cursorY)
{
	DrawCardTitle(drawList, card, cursorY, "You're Up to Date");

	char line[64];
	std::snprintf(line, sizeof(line), "%s %s is the latest version.", kAppName, kAppVersion);
	DrawText(drawList, m_fonts.GetSecondary(), TextColumnX(card), cursorY, line, kColorTextDim);

	DrawPrimaryButton(drawList, card, "Check Again", false);
}

void CUpdateOverlay::DrawCheckFailedStage(CDrawList &drawList, Rect card, float &cursorY)
{
	DrawCardTitle(drawList, card, cursorY, "Couldn't Check for Updates");
	DrawWrappedText(drawList, m_fonts.GetSecondary(), TextColumnX(card), cursorY, TextColumnWidth(card),
					m_pUpdater->GetErrorMessage(), kColorError, kMaxMessageLines);

	DrawPrimaryButton(drawList, card, "Try Again", true);
}

void CUpdateOverlay::DrawNotesBox(CDrawList &drawList)
{
	const NotesLayout notes = ComputeNotesLayout();
	const CFont &secondary = m_fonts.GetSecondary();

	drawList.AddRectRoundedBordered(notes.Box.X, notes.Box.Y, notes.Box.W, notes.Box.H, CDrawList::UniformRadii(8.0f),
									kColorNotesBg, kColorNotesBorder, 1.0f);

	std::string_view lines[kMaxNotesLines];
	const u32 lineCount = WrapText(secondary, m_pUpdater->GetManifest().szNotes, notes.Box.W - kNotesScrollbarMargin,
								   lines, kMaxNotesLines);
	const float lineHeight = secondary.GetLineHeight();

	drawList.PushClipRect(notes.Box);

	float y = notes.Box.Y + kNotesBoxPadding + secondary.GetAscent() - m_notesScroll.m_flScrollOffset;
	for (u32 i = 0; i < lineCount; i += 1) {
		if (y > notes.Box.Y - lineHeight && y < notes.Box.Y + notes.Box.H + lineHeight) {
			DrawText(drawList, secondary, notes.Box.X + kNotesBoxPadding, y, lines[i], kColorTextDim);
		}

		y += lineHeight;
	}

	drawList.PopClipRect();

	m_notesScroll.DrawEdgeFade(drawList, notes.Box, notes.ContentHeight, notes.Box.H, kColorNotesBg);
	m_notesScroll.Draw(drawList, notes.Track, notes.ContentHeight, notes.Box.H, kColorScrollThumb, m_flMouseX,
					   m_flMouseY);
}

void CUpdateOverlay::DrawAvailableStage(CDrawList &drawList, Rect card, float &cursorY)
{
	DrawCardTitle(drawList, card, cursorY, "Update Available");

	char line[64];
	std::snprintf(line, sizeof(line), "Version %s is ready to install.", m_pUpdater->GetManifest().szVersion);
	DrawText(drawList, m_fonts.GetSecondary(), TextColumnX(card), cursorY, line, kColorTextDim);

	DrawNotesBox(drawList);
	DrawPrimaryButton(drawList, card, "Download & Install", true);
}

void CUpdateOverlay::DrawManualUpgradeStage(CDrawList &drawList, Rect card, float &cursorY)
{
	const CFont &secondary = m_fonts.GetSecondary();

	DrawCardTitle(drawList, card, cursorY, "Manual Update Required");

	char line[64];
	std::snprintf(line, sizeof(line), "Version %s is out - please download it manually",
				  m_pUpdater->GetManifest().szVersion);
	DrawText(drawList, secondary, TextColumnX(card), cursorY, line, kColorTextDim);

	cursorY += secondary.GetLineHeight();
	DrawText(drawList, secondary, TextColumnX(card), cursorY, "from the GitHub releases page.", kColorTextDim);
}

void CUpdateOverlay::DrawProgressStage(CDrawList &drawList, Rect card, float &cursorY, EUpdateStage stage)
{
	const CFont &secondary = m_fonts.GetSecondary();
	const Color accent = m_pSettings->m_clrAccent;

	DrawCardTitle(drawList, card, cursorY, "Updating");

	const u64 downloaded = m_pUpdater->GetBytesDownloaded();
	const u64 total = m_pUpdater->GetTotalBytes();
	const float downloadFraction = total > 0 ? static_cast<float>(downloaded) / static_cast<float>(total) : 0.0f;
	const float progress = stage == EUpdateStage::Downloading ? downloadFraction : 1.0f;

	const Rect track{TextColumnX(card), cursorY + 8.0f, TextColumnWidth(card), kProgressBarHeight};
	drawList.AddRectRoundedFilled(track.X, track.Y, track.W, track.H, CDrawList::UniformRadii(track.H * 0.5f),
								  kColorTrack);

	// A sliver stays visible at 0%, so the bar is never literally invisible.
	const float fillWidth = track.W * std::max(progress, 0.02f);
	const CornerRadii fillRadii = CDrawList::UniformRadii(track.H * 0.5f);

	// A soft halo under the filled part, so the bar has some depth rather than reading as two flat
	// rectangles. Inset vertically, since a halo the full height of the bar just looks blurry.
	constexpr float kGlowExpand = 3.0f;
	drawList.AddRectRoundedFilled(track.X - kGlowExpand, track.Y - kGlowExpand * 0.5f, fillWidth + kGlowExpand * 2.0f,
								  track.H + kGlowExpand, CDrawList::UniformRadii(track.H * 0.5f + kGlowExpand),
								  ColorWithAlpha(accent, 40));
	drawList.AddRectRoundedFilled(track.X, track.Y, fillWidth, track.H, fillRadii, accent);

	cursorY = track.Y + track.H + 14.0f + secondary.GetAscent();

	char line[64];
	if (stage == EUpdateStage::Downloading) {
		char downloadedText[32];
		char totalText[32];
		char speedText[32];
		FormatBytes(downloaded, downloadedText, sizeof(downloadedText));
		FormatBytes(total, totalText, sizeof(totalText));
		FormatSpeed(m_pUpdater->GetBytesPerSecond(), speedText, sizeof(speedText));

		std::snprintf(line, sizeof(line), "%s / %s  -  %s", downloadedText, totalText, speedText);
	} else {
		std::snprintf(line, sizeof(line), stage == EUpdateStage::Verifying ? "Verifying..." : "Installing...");
	}

	DrawText(drawList, secondary, TextColumnX(card), cursorY, line, kColorTextDim);

	if (stage == EUpdateStage::Downloading) {
		DrawPrimaryButton(drawList, card, "Cancel", false);
	}
}

void CUpdateOverlay::DrawFailedStage(CDrawList &drawList, Rect card, float &cursorY, EUpdateStage stage)
{
	const bool errored = stage == EUpdateStage::Error;

	DrawCardTitle(drawList, card, cursorY, errored ? "Update Failed" : "Update Cancelled");

	if (errored) {
		DrawWrappedText(drawList, m_fonts.GetSecondary(), TextColumnX(card), cursorY, TextColumnWidth(card),
						m_pUpdater->GetErrorMessage(), kColorError, kMaxMessageLines);
	}

	DrawPrimaryButton(drawList, card, "Try Again", true);
}

void CUpdateOverlay::Draw(CDrawList &drawList)
{
	if (!m_bActive) return;

	const auto windowW = static_cast<float>(m_window.GetWidth());
	const auto windowH = static_cast<float>(m_window.GetHeight());
	drawList.AddRectFilled(0.0f, 0.0f, windowW, windowH, kColorBackdrop);

	const Rect card = CardRect();

	Controls::DrawPanelShadow(drawList, card, kCardRadius, 1.0f);
	drawList.AddRectRoundedBordered(card.X, card.Y, card.W, card.H, CDrawList::UniformRadii(kCardRadius), kColorCard,
									kColorCardBorder, 1.0f);

	// Inset by the corner radius so it stops exactly where the curve starts; at any other inset it
	// either leaves a gap at both ends or runs past the curve.
	const float edgeInset = CDrawList::ScaledRadius(kCardRadius);
	drawList.AddRectFilled(card.X + edgeInset, card.Y + 1.0f, card.W - edgeInset * 2.0f, 1.0f, kColorCardTopEdge);

	const EUpdateStage stage = m_pUpdater->GetStage();
	if (StageIsDismissable(stage)) {
		DrawCloseButton(drawList, card);
	}

	float cursorY = card.Y + kCardPadding + m_fonts.GetBody().GetAscent();

	switch (stage) {
		case EUpdateStage::Idle:
		case EUpdateStage::Checking:
			DrawCheckingStage(drawList, card, cursorY);
			break;

		case EUpdateStage::UpToDate:
			DrawUpToDateStage(drawList, card, cursorY);
			break;

		case EUpdateStage::CheckFailed:
			DrawCheckFailedStage(drawList, card, cursorY);
			break;

		case EUpdateStage::Available:
			DrawAvailableStage(drawList, card, cursorY);
			break;

		case EUpdateStage::ManualUpgradeRequired:
			DrawManualUpgradeStage(drawList, card, cursorY);
			break;

		case EUpdateStage::Downloading:
		case EUpdateStage::Verifying:
		case EUpdateStage::Installing:
			DrawProgressStage(drawList, card, cursorY, stage);
			break;

		case EUpdateStage::ReadyToRelaunch:
			DrawCardTitle(drawList, card, cursorY, "Restarting...");
			break;

		case EUpdateStage::Error:
		case EUpdateStage::Cancelled:
			DrawFailedStage(drawList, card, cursorY, stage);
			break;
	}
}
