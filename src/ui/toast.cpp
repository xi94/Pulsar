#include "ui/toast.h"

#include <algorithm>
#include <cmath>

#include "core/animator.h"
#include "core/profiler.h"
#include "core/settings.h"
#include "core/str.h"
#include "platform/window.h"
#include "ui/controls.h"
#include "ui/draw_list.h"
#include "ui/text.h"

namespace {
constexpr float kMarginX = 14.0f;
constexpr float kMarginY = 12.0f;

constexpr float kPaddingX = 11.0f;
constexpr float kPaddingY = 8.0f;
constexpr float kCornerRadius = 7.0f;

constexpr float kIconSize = 15.0f;
constexpr float kIconGap = 8.0f;

/// One turn of a spinning icon. Slow enough to read as a hint that something is waiting rather
/// than as a busy indicator, which would suggest the app is doing work it is not.
constexpr float kIconSpinSeconds = 5.0f;
constexpr float kTwoPi = 6.28318530717958647692f;

/// A hairline along the bottom, inset to the same column as the text so it reads as part of the
/// card rather than as its border.
constexpr float kTimeoutBarHeight = 3.0f;
constexpr float kTimeoutBarGap = 7.0f;

/// Slack beyond what the text strictly needs, so a short message sits in a card rather than in a
/// box drawn tight around it. The floor stops a two-word message becoming a stub, and the ceiling
/// makes a long one wrap rather than stretch along the bottom of the window.
constexpr float kBreathingWidth = 26.0f;
constexpr float kMinCardWidth = 150.0f;
constexpr float kMaxCardWidth = 300.0f;

constexpr float kLifetimeSeconds = 4.0f;
constexpr float kPresenceEaseRate = 18.0f;

/// A card most of the way gone should not still be eating clicks.
constexpr float kInteractivePresence = 0.6f;

constexpr Color kColorCard{24, 24, 28, 244};
constexpr Color kColorBorder{52, 52, 60, 255};
constexpr Color kColorText{222, 222, 228, 255};
constexpr Color kColorTimeoutTrack{48, 48, 56, 255};

/// The halo around the filled part of the timeout bar: concentric rounded rects at falling alpha,
/// which is how the rest of this UI fakes a falloff without a shader.
constexpr u32 kGlowLayers = 2;
constexpr float kGlowExpand = 2.0f;
constexpr float kGlowAlpha = 54.0f;

/// The highlight travelling along the bar, in bar-widths per second.
constexpr float kSweepWidth = 46.0f;
constexpr float kSweepSpeed = 0.55f;
constexpr float kSweepAlpha = 120.0f;
} // namespace

CToastHost::CToastHost(const CFontManager &fonts, const CWindow &window, const CAssetManager &assets,
					   const Settings &settings)
	: m_fonts(fonts)
	, m_window(window)
	, m_assets(assets)
	, m_settings(settings)
{
}

float CToastHost::IconColumnWidth() const
{
	return m_icon == EAsset::Count ? 0.0f : kIconSize + kIconGap;
}

// Measured rather than fixed: a card four times longer than its own text reads as a placeholder.
// The clamp is what stops a long message running the width of the window - past the ceiling it
// wraps to a second line instead, which CardHeight then accounts for.
float CToastHost::CardWidth() const
{
	const float chrome = kPaddingX * 2.0f + IconColumnWidth();
	const float natural = chrome + TextWidth(m_fonts.GetSecondary(), m_szMessage) + kBreathingWidth;

	return std::clamp(natural, kMinCardWidth, kMaxCardWidth);
}

float CToastHost::CardHeight() const
{
	const CFont &font = m_fonts.GetSecondary();
	const float textWidth = CardWidth() - kPaddingX * 2.0f - IconColumnWidth();

	std::string_view lines[kMaxLines];
	const u32 lineCount = WrapText(font, m_szMessage, textWidth, lines, kMaxLines);

	return kPaddingY * 2.0f + static_cast<float>(std::max(1u, lineCount)) * font.GetLineHeight() + kTimeoutBarGap +
		   kTimeoutBarHeight;
}

Rect CToastHost::CardRect() const
{
	const float height = CardHeight();
	const float bottom = static_cast<float>(m_window.GetHeight()) - kStatusBarHeight - kMarginY;

	return Rect{kMarginX, bottom - height, CardWidth(), height};
}

Rect CToastHost::AnimatedCardRect() const
{
	Rect rect = CardRect();
	rect.X -= (1.0f - m_flPresenceAmount) * (rect.W + kMarginX);

	return rect;
}

bool CToastHost::IsInteractive() const
{
	return m_bShowing && m_flPresenceAmount >= kInteractivePresence;
}

bool CToastHost::IsPointerOverCard(float x, float y) const
{
	return IsInteractive() && RectContainsPoint(AnimatedCardRect(), x, y);
}

void CToastHost::Show(const Notification &notification, float seconds, bool deadlineDriven)
{
	if (!m_settings.m_bShowNotifications) return;

	CopyTo(notification.Message, m_szMessage, sizeof(m_szMessage));
	m_icon = notification.Icon;
	m_bSpinIcon = notification.SpinIcon;
	m_action = notification.Action;
	m_flTotalSeconds = seconds;
	m_flRemainingSeconds = seconds;
	m_bShowing = true;
	m_bDeadlineDriven = deadlineDriven;
}

void CToastHost::Notify(const Notification &notification)
{
	Show(notification, kLifetimeSeconds, false);
}

void CToastHost::NotifyDeadline(std::string_view message, float seconds)
{
	Show(Notification{.Message = message}, seconds, true);
}

// Only clears a deadline notification. An ordinary one that replaced it in the meantime belongs to
// whoever raised it.
void CToastHost::DismissDeadline()
{
	if (m_bDeadlineDriven) {
		m_bShowing = false;
		m_bDeadlineDriven = false;
	}
}

EToastAction CToastHost::ConsumeAction()
{
	const EToastAction action = m_pendingAction;
	m_pendingAction = EToastAction::None;

	return action;
}

void CToastHost::Update(float deltaSeconds)
{
	PULSAR_PROFILE_SCOPE("Toast.Update");

	// Held while hovered, so something worth reading does not vanish mid-sentence. A deadline
	// notification is exempt: the deadline it mirrors does not pause either.
	if (m_bShowing && (m_bDeadlineDriven || !IsPointerOverCard(m_flMouseX, m_flMouseY))) {
		m_flRemainingSeconds -= deltaSeconds;

		if (m_flRemainingSeconds <= 0.0f) {
			m_flRemainingSeconds = 0.0f;
			m_bShowing = false;
		}
	}

	m_flPresenceAmount =
		CAnimator::EaseToward(m_flPresenceAmount, m_bShowing ? 1.0f : 0.0f, kPresenceEaseRate, deltaSeconds);

	// Free-running rather than tied to the countdown, so the sweep keeps its own steady pace while
	// the bar it travels along shortens underneath it.
	m_flElapsedSeconds += deltaSeconds;
}

// Both halves are consumed, or the widget underneath gets a press whose release never arrives and
// keeps thinking a drag is in progress.
bool CToastHost::OnPointerDown(float x, float y)
{
	return !m_bDeadlineDriven && IsPointerOverCard(x, y);
}

bool CToastHost::OnPointerUp(float x, float y)
{
	// A deadline notification is not dismissible by clicking it: the click that ends it is the one
	// on the button it is prompting about.
	if (m_bDeadlineDriven || !IsPointerOverCard(x, y)) return false;

	// Latched rather than performed here: what "open the updates view" means is the owner's
	// business, not this widget's.
	m_pendingAction = m_action;
	m_bShowing = false;

	return true;
}

ECursorKind CToastHost::GetDesiredCursor() const
{
	return IsPointerOverCard(m_flMouseX, m_flMouseY) ? ECursorKind::Hand : ECursorKind::Arrow;
}

void CToastHost::Draw(CDrawList &drawList)
{
	PULSAR_PROFILE_SCOPE("Toast.Draw");

	if (m_flPresenceAmount < 0.01f) return;

	const CFont &font = m_fonts.GetSecondary();
	const Rect card = AnimatedCardRect();
	const auto alpha = static_cast<u8>(std::clamp(m_flPresenceAmount, 0.0f, 1.0f) * 255.0f);
	const Color accent = m_settings.m_clrAccent;

	drawList.AddRectRoundedBordered(card.X, card.Y, card.W, card.H, CDrawList::UniformRadii(kCornerRadius),
									ColorScaleAlpha(kColorCard, alpha), ColorScaleAlpha(kColorBorder, alpha), 1.0f);

	// Centred on the first line rather than on the card, so the icon stays level with the text it
	// belongs to when the message wraps to two lines.
	if (m_icon != EAsset::Count) {
		const float firstLineCenterY = card.Y + kPaddingY + font.GetLineHeight() * 0.5f;
		const Rect icon{card.X + kPaddingX, firstLineCenterY - kIconSize * 0.5f, kIconSize, kIconSize};
		const Color tint = ColorScaleAlpha(accent, alpha);

		// Driven by the free-running clock rather than the countdown, so the turn keeps one steady
		// rate instead of finishing as the card expires.
		if (m_bSpinIcon) {
			const float radians = std::fmod(m_flElapsedSeconds, kIconSpinSeconds) / kIconSpinSeconds * kTwoPi;
			Controls::DrawIconRotated(drawList, icon, m_assets.Get(m_icon), radians, tint);
		} else {
			Controls::DrawIcon(drawList, icon, m_assets.Get(m_icon), tint);
		}
	}

	const float textX = card.X + kPaddingX + IconColumnWidth();
	const float textWidth = card.X + card.W - kPaddingX - textX;
	DrawWrappedText(drawList, font, textX, card.Y + kPaddingY + font.GetAscent(), textWidth, m_szMessage,
					ColorScaleAlpha(kColorText, alpha), kMaxLines);

	DrawTimeoutBar(drawList, card, alpha);
}

// Full width at birth, empty at expiry, over a dim track so the remaining time reads as a
// proportion rather than as a bar of unknown length.
//
// The bar carries the same treatment the carousel gives a selected card: a soft halo bleeding out
// past its edges, and a highlight travelling along it. Built from plain geometry rather than the
// banner-glow shader, which is a distance field around a card-sized rounded rect and has nothing
// useful to say about a shape three pixels tall.
void CToastHost::DrawTimeoutBar(CDrawList &drawList, Rect card, u8 alpha) const
{
	const Color accent = m_settings.m_clrAccent;
	const float barY = card.Y + card.H - kPaddingY - kTimeoutBarHeight;
	const float barX = card.X + kPaddingX;
	const float trackWidth = card.W - kPaddingX * 2.0f;
	const float remaining = m_flTotalSeconds > 0.0f ? m_flRemainingSeconds / m_flTotalSeconds : 0.0f;
	const float fillWidth = trackWidth * std::clamp(remaining, 0.0f, 1.0f);
	const CornerRadii barRadii = CDrawList::UniformRadii(kTimeoutBarHeight * 0.5f);

	drawList.AddRectRoundedFilled(barX, barY, trackWidth, kTimeoutBarHeight, barRadii,
								  ColorScaleAlpha(kColorTimeoutTrack, alpha));

	if (fillWidth <= 0.0f) return;

	// Two halos rather than one, the outer wider and fainter, which is what turns a hard edge into
	// a falloff without a shader to compute one per pixel.
	for (u32 i = 0; i < kGlowLayers; i += 1) {
		const float expand = kGlowExpand * static_cast<float>(i + 1);
		const auto glowAlpha = static_cast<u8>(kGlowAlpha / static_cast<float>(i + 1) * m_flPresenceAmount);

		drawList.AddRectRoundedFilled(
			barX - expand, barY - expand, fillWidth + expand * 2.0f, kTimeoutBarHeight + expand * 2.0f,
			CDrawList::UniformRadii(kTimeoutBarHeight * 0.5f + expand), ColorWithAlpha(accent, glowAlpha));
	}

	drawList.AddRectRoundedFilled(barX, barY, fillWidth, kTimeoutBarHeight, barRadii, ColorScaleAlpha(accent, alpha));

	// A brighter band sweeping along the filled part, clipped to it so it never runs past the head
	// of the bar. Its own gradient fades in and out at the edges, so nothing pops as it wraps.
	drawList.PushClipRect(
		Rect{barX, barY - kGlowExpand * kGlowLayers, fillWidth, kTimeoutBarHeight + kGlowExpand * 2.0f * kGlowLayers});

	const float sweepTravel = fillWidth + kSweepWidth;
	const float sweepX = barX - kSweepWidth + std::fmod(m_flElapsedSeconds * kSweepSpeed, 1.0f) * sweepTravel;
	const Color sweepEdge = ColorWithAlpha(ColorLighten(accent, 70), 0);
	const Color sweepPeak = ColorWithAlpha(ColorLighten(accent, 70), static_cast<u8>(kSweepAlpha * m_flPresenceAmount));

	drawList.AddRectGradientCorners(sweepX, barY, kSweepWidth * 0.5f, kTimeoutBarHeight, sweepEdge, sweepPeak,
									sweepEdge, sweepPeak);
	drawList.AddRectGradientCorners(sweepX + kSweepWidth * 0.5f, barY, kSweepWidth * 0.5f, kTimeoutBarHeight, sweepPeak,
									sweepEdge, sweepPeak, sweepEdge);

	drawList.PopClipRect();
}
