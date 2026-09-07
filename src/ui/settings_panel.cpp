#include "ui/settings_panel.h"

#include "core/profiler.h"

#include "core/str.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include <Windows.h>

#include "core/animator.h"
#include "gfx/asset_manager.h"
#include "gfx/font.h"
#include "platform/window.h"
#include "ui/controls.h"
#include "ui/draw_list.h"
#include "ui/layout.h"
#include "ui/text.h"

namespace {
constexpr float kPanelOpenEaseRate = 16.0f;
constexpr float kToggleEaseRate = 18.0f;

// Close to the toggle rate, so a row's control and its neighbours settle at the same pace.
constexpr float kValueEaseRate = 16.0f;

// Grows in lockstep with the font-size setting, so bigger text gets more room than it needs
// rather than being clipped.
constexpr float kPanelMaxWidthBase = 570.0f;

// A comfortable viewport, not "tall enough for every row" - the list scrolls once it overflows.
constexpr float kPanelMaxHeightBase = 480.0f;

// Real baked pixels, which already include the display-scale factor, matching the default
// nominal sizes in Settings.
constexpr float kReferenceBodyPixelHeight = 24.0f;
constexpr float kReferenceSecondaryPixelHeight = 20.0f;

constexpr float kPanelMargin = 48.0f;
constexpr float kPanelScaleMin = 0.94f;
constexpr float kPanelRadius = 16.0f;
constexpr float kPanelBorderThickness = 1.5f;

constexpr float kRowPaddingX = 26.0f;
constexpr float kCloseSize = 26.0f;

// Gaps rather than a fixed row height, so raising them spaces the list out without clipping.
constexpr float kRowLabelTopGap = 14.0f;
constexpr float kRowLabelLineGap = 6.0f;
constexpr float kRowLabelBottomGap = 16.0f;

// Inset from the row padding, so the highlight reads as a band rather than a full-bleed stripe.
constexpr float kRowHighlightInsetX = 12.0f;
constexpr float kRowHighlightRadius = 8.0f;

constexpr float kResetButtonSize = 28.0f;
constexpr float kResetButtonGap = 10.0f;
constexpr float kResetIconSize = 18.0f;
constexpr float kResetAppearRate = 20.0f;

// Slower than the fade, so the turn is still visible when the eye lands on it.
constexpr float kResetSpinRate = 6.0f;
constexpr float kTwoPi = 6.28318530717958647692f;

// The breathing room above a heading is what actually does the grouping work.
constexpr float kSectionHeaderTopGap = 22.0f;
constexpr float kSectionHeaderBottomGap = 8.0f;

constexpr float kSliderTrackWidth = 130.0f;
constexpr float kSliderTrackVisualHeight = 6.0f;
constexpr float kSliderThumbRadius = 8.0f;
constexpr float kSliderValueLabelGap = 8.0f;

// Fixed rather than measured, so the reset button left of it does not shuffle as digits change.
constexpr float kSliderValueLabelWidth = 48.0f;

constexpr float kAnimationSpeedMin = 0.25f;
constexpr float kAnimationSpeedMax = 3.0f;

// 1.0 is the design radius and 0 squares everything off. The ceiling is 1.5 because the
// rounded-rect builder clamps each radius to half the shorter side, and past roughly 1.5 most
// shapes are already at that limit.
constexpr float kCornerRoundnessMin = 0.0f;
constexpr float kCornerRoundnessMax = 1.5f;

// Nominal settings units, not baked pixels.
constexpr float kFontSizeMin = 10.0f;
constexpr float kFontSizeMax = 24.0f;

// Independently tunable, with a lower ceiling: secondary labels body content rather than
// rivalling it.
constexpr float kSecondaryFontSizeMin = 8.0f;
constexpr float kSecondaryFontSizeMax = 18.0f;

// Keeps a knob a distinct object rather than dissolving into whatever it sits on.
constexpr float kKnobRingThickness = 1.5f;

constexpr Color kColorBg{26, 26, 29, 255};
constexpr Color kColorBorder{70, 70, 76, 255};
constexpr Color kColorText{224, 224, 228, 255};
constexpr Color kColorTextDim{140, 140, 146, 255};
constexpr Color kColorSeparator{58, 58, 64, 255};
constexpr Color kColorControlBg{40, 40, 45, 255};
constexpr Color kColorToggleOff{70, 70, 76, 255};
constexpr Color kColorRowHover{36, 36, 41, 255};
constexpr Color kColorResetIdle{150, 150, 158, 255};
constexpr Color kColorResetHoverBg{64, 64, 72, 255};
constexpr Color kColorScrollThumb{120, 120, 128, 190};

// Every size derives from the active pFonts' glyph metrics, so a larger font grows rows to fit.
// Both faces matter: descriptions are set in the independently adjustable secondary face, and
// scaling off body alone leaves the panel too narrow for them.
float PanelMaxSizeScale(const CFontManager *pFonts)
{
	const float bodyScale = pFonts->GetBody().GetPixelHeight() / kReferenceBodyPixelHeight;
	const float secondaryScale = pFonts->GetSecondary().GetPixelHeight() / kReferenceSecondaryPixelHeight;

	return std::max(1.0f, std::max(bodyScale, secondaryScale));
}

float HeaderHeightFor(const CFontManager *pFonts)
{
	return pFonts->GetBody().GetLineHeight() + 20.0f;
}

float FooterHeightFor(const CFontManager *pFonts)
{
	return std::max(32.0f, pFonts->GetSecondary().GetLineHeight() + 16.0f);
}

// A row is exactly its title-plus-description stack.
float RowHeightFor(const CFontManager *pFonts)
{
	return kRowLabelTopGap + pFonts->GetBody().GetLineHeight() + kRowLabelLineGap +
		   pFonts->GetSecondary().GetLineHeight() + kRowLabelBottomGap;
}

float SectionHeaderHeightFor(const CFontManager *pFonts)
{
	return kSectionHeaderTopGap + pFonts->GetSecondary().GetLineHeight() + kSectionHeaderBottomGap;
}

// Body rather than secondary: every control this sizes holds a value the user reads or types.
float RowControlHeightFor(const CFontManager *pFonts)
{
	return std::max(34.0f, pFonts->GetBody().GetLineHeight() + 12.0f);
}

float RowTitleBaselineY(Rect row, const CFontManager *pFonts)
{
	return row.Y + kRowLabelTopGap + pFonts->GetBody().GetAscent();
}

float RowDescriptionBaselineY(Rect row, const CFontManager *pFonts)
{
	return row.Y + kRowLabelTopGap + pFonts->GetBody().GetLineHeight() + kRowLabelLineGap +
		   pFonts->GetSecondary().GetAscent();
}

// The midpoint between the two text baselines, not the row's box centre: the title line is
// usually taller, so centring on the box pulls controls visibly toward it.
float RowControlCenterY(Rect row, const CFontManager *pFonts)
{
	return (RowTitleBaselineY(row, pFonts) + RowDescriptionBaselineY(row, pFonts)) * 0.5f;
}

Rect PanelRect(float openAmount, float windowW, float windowH, const CFontManager *pFonts)
{
	const float sizeScale = PanelMaxSizeScale(pFonts);
	const float panelMaxWidth = kPanelMaxWidthBase * sizeScale;
	const float panelMaxHeight = kPanelMaxHeightBase * sizeScale;

	float w = std::min(panelMaxWidth, std::max(0.0f, windowW - kPanelMargin * 2.0f));
	float h = std::min(panelMaxHeight, std::max(0.0f, windowH - kPanelMargin * 2.0f));

	const float targetAspect = panelMaxWidth / panelMaxHeight;
	if (w / h > targetAspect) {
		w = h * targetAspect;
	} else {
		h = w / targetAspect;
	}

	const float openScale = kPanelScaleMin + (1.0f - kPanelScaleMin) * openAmount;
	w *= openScale;
	h *= openScale;

	return Rect{(windowW - w) * 0.5f, (windowH - h) * 0.5f, w, h};
}

// Sized off its own constant, so it stays a comfortable click target at any font size.
Rect CloseRect(Rect panel, const CFontManager *pFonts)
{
	const float headerHeight = HeaderHeightFor(pFonts);

	return Rect{panel.X + panel.W - 14.0f - kCloseSize, panel.Y + (headerHeight - kCloseSize) * 0.5f, kCloseSize,
				kCloseSize};
}

// A thin strip inset from the scroll region's right edge. Whether anything is drawn or
// hit-tested there is CScrollable's decision.
Rect ScrollbarTrackRect(Rect scrollRegion)
{
	constexpr float kTrackMargin = 4.0f;

	return Rect{scrollRegion.X + scrollRegion.W - kScrollbarWidth - kTrackMargin, scrollRegion.Y, kScrollbarWidth,
				scrollRegion.H};
}

/// The band a row's hover highlight actually paints, which is narrower than the row itself. Rows
/// span the full scroll region, so the scrollbar track sits inside the right-hand inset - testing
/// the row rect would light a row up while the pointer is on the scrollbar.
Rect RowHighlightRect(Rect row)
{
	return Rect{row.X + kRowHighlightInsetX, row.Y, row.W - kRowHighlightInsetX * 2.0f, row.H};
}

/// Cheap cull before drawing or hit-testing; the clip is what stops a partially visible row
/// painting outside the region.
bool RowInView(Rect row, Rect scrollRegion)
{
	return !(row.Y + row.H <= scrollRegion.Y || row.Y >= scrollRegion.Y + scrollRegion.H);
}

Rect FontFieldRect(Rect row, const CFontManager *pFonts)
{
	constexpr float kW = 190.0f;

	const float h = RowControlHeightFor(pFonts);

	return Rect{row.X + row.W - kRowPaddingX - kW, row.Y + (row.H - h) * 0.5f, kW, h};
}

// The stepper's bounding rect; the two button rects below carve out of this same rect, so
// drawing and hit-testing cannot disagree.
Rect StepperRect(Rect row, const CFontManager *pFonts)
{
	constexpr float kW = 108.0f;
	constexpr float kH = 28.0f;

	return Rect{row.X + row.W - kRowPaddingX - kW, RowControlCenterY(row, pFonts) - kH * 0.5f, kW, kH};
}

Rect StepperMinusRect(Rect stepper)
{
	return Rect{stepper.X, stepper.Y, stepper.H, stepper.H};
}

Rect StepperPlusRect(Rect stepper)
{
	return Rect{stepper.X + stepper.W - stepper.H, stepper.Y, stepper.H, stepper.H};
}

Rect ToggleRect(Rect row, const CFontManager *pFonts)
{
	constexpr float kW = 40.0f;
	constexpr float kH = 22.0f;

	return Rect{row.X + row.W - kRowPaddingX - kW, RowControlCenterY(row, pFonts) - kH * 0.5f, kW, kH};
}

// Also the anchor the colour picker positions itself against.
Rect SwatchRect(Rect row, const CFontManager *pFonts)
{
	constexpr float kW = 40.0f;
	constexpr float kH = 24.0f;

	return Rect{row.X + row.W - kRowPaddingX - kW, RowControlCenterY(row, pFonts) - kH * 0.5f, kW, kH};
}

Rect MasterPasswordButtonRect(Rect row, const CFontManager *pFonts)
{
	constexpr float kW = 140.0f;

	const float h = RowControlHeightFor(pFonts);

	return Rect{row.X + row.W - kRowPaddingX - kW, RowControlCenterY(row, pFonts) - h * 0.5f, kW, h};
}

// The draggable track only, at a fixed width regardless of font size. Taller than the visual
// bar itself, for an easier grab target.
Rect SliderTrackRect(Rect row, const CFontManager *pFonts)
{
	constexpr float kH = 22.0f;

	return Rect{row.X + row.W - kRowPaddingX - kSliderTrackWidth, RowControlCenterY(row, pFonts) - kH * 0.5f,
				kSliderTrackWidth, kH};
}

// The track plus the reserved space its readout occupies, so the reset button lands left of
// the readout rather than on top of it.
Rect SliderControlRect(Rect row, const CFontManager *pFonts)
{
	const Rect track = SliderTrackRect(row, pFonts);
	const float reserved = kSliderValueLabelGap + kSliderValueLabelWidth;

	return Rect{track.X - reserved, track.Y, track.W + reserved, track.H};
}

// Derived from the control rather than pinned to a fixed column, so it stays adjacent to the
// thing it restores whether that is a wide text field or a narrow toggle.
Rect ResetButtonRect(Rect control, Rect row, const CFontManager *pFonts)
{
	return Rect{control.X - kResetButtonGap - kResetButtonSize,
				RowControlCenterY(row, pFonts) - kResetButtonSize * 0.5f, kResetButtonSize, kResetButtonSize};
}

// Always leaves room for the reset button, which comes and goes as a value moves on and off its
// default. A label that re-flowed each time would be far more distracting.
float LabelRightEdge(Rect control, Rect row, const CFontManager *pFonts)
{
	constexpr float kLabelControlGap = 16.0f;

	return ResetButtonRect(control, row, pFonts).X - kLabelControlGap;
}

float AnimationSpeedToT(float speed)
{
	return std::clamp((speed - kAnimationSpeedMin) / (kAnimationSpeedMax - kAnimationSpeedMin), 0.0f, 1.0f);
}

float AnimationSpeedFromT(float t)
{
	return kAnimationSpeedMin + std::clamp(t, 0.0f, 1.0f) * (kAnimationSpeedMax - kAnimationSpeedMin);
}

float CornerRoundnessToT(float roundness)
{
	return std::clamp((roundness - kCornerRoundnessMin) / (kCornerRoundnessMax - kCornerRoundnessMin), 0.0f, 1.0f);
}

float CornerRoundnessFromT(float t)
{
	return kCornerRoundnessMin + std::clamp(t, 0.0f, 1.0f) * (kCornerRoundnessMax - kCornerRoundnessMin);
}

// The panel has no dedicated close-icon asset, unlike the title bar's.
// Ellipsized to where the label column ends, since at a large font size the text does reach the
// control. The caller passes that edge, since only it knows which control the row has.
void DrawRowLabel(CDrawList &drawList, const CFontManager *pFonts, Rect row, const char *pTitle,
				  const char *pDescription, float labelRightEdge, u8 alpha)
{
	const float labelX = row.X + kRowPaddingX;
	const float maxWidth = labelRightEdge - labelX;

	DrawTextEllipsized(drawList, pFonts->GetBody(), labelX, RowTitleBaselineY(row, pFonts), pTitle, maxWidth,
					   ColorScaleAlpha(kColorText, alpha));
	DrawTextEllipsized(drawList, pFonts->GetSecondary(), labelX, RowDescriptionBaselineY(row, pFonts), pDescription,
					   maxWidth, ColorScaleAlpha(kColorTextDim, alpha));
}

// The group's name between two rules. Bottom-aligned within its strip, so the first heading -
// whose strip is shorter - sits the same distance above its first row as every other one.
//
// The rule lands on the text's visual centre: the baseline minus half of ascent plus descent,
// since descent is negative. The difference of the two is a different quantity and sits high.
void DrawSectionHeader(CDrawList &drawList, const CFontManager *pFonts, Rect rect, const char *pTitle, u8 alpha)
{
	constexpr float kTextRuleGap = 10.0f;
	constexpr float kLeadRuleWidth = 16.0f;

	const CFont &secondary = pFonts->GetSecondary();
	const std::string_view title = pTitle;
	const float baselineY =
		rect.Y + rect.H - kSectionHeaderBottomGap - (secondary.GetLineHeight() - secondary.GetAscent());
	const float centerY = baselineY - (secondary.GetAscent() + secondary.GetDescent()) * 0.5f;

	// The stub gives every heading the same left edge as the rows beneath it.
	const float leadX = rect.X + kRowPaddingX;
	drawList.AddRectFilled(leadX, centerY, kLeadRuleWidth, 1.0f, ColorScaleAlpha(kColorSeparator, alpha));

	const float textX = leadX + kLeadRuleWidth + kTextRuleGap;
	DrawText(drawList, secondary, textX, baselineY, title, ColorScaleAlpha(kColorText, alpha));

	const float ruleX = textX + TextWidth(secondary, title) + kTextRuleGap;
	const float ruleRight = rect.X + rect.W - kRowPaddingX;

	if (ruleRight > ruleX) {
		drawList.AddRectFilled(ruleX, centerY, ruleRight - ruleX, 1.0f, ColorScaleAlpha(kColorSeparator, alpha));
	}
}

// Present only while its row is off its default, so the button appearing is the signal that
// there is something to restore. The spin winds back counter-clockwise.
void DrawResetButton(CDrawList &drawList, const CTexture *pIcon, Rect rect, float appearAmount, float spinAmount,
					 bool hovered, u8 alpha)
{
	if (appearAmount <= 0.01f || pIcon == nullptr) return;

	const auto fade = static_cast<u8>(static_cast<float>(alpha) * appearAmount);

	if (hovered) {
		drawList.AddRectRoundedFilled(rect.X, rect.Y, rect.W, rect.H, CDrawList::UniformRadii(7.0f),
									  ColorScaleAlpha(kColorResetHoverBg, fade));
	}

	drawList.AddRectTexturedRotated(
		rect.X + (rect.W - kResetIconSize) * 0.5f, rect.Y + (rect.H - kResetIconSize) * 0.5f, kResetIconSize,
		kResetIconSize, -spinAmount * kTwoPi, pIcon, ColorScaleAlpha(hovered ? kColorText : kColorResetIdle, fade));
}

// A ring outside with the fill inset within it, so the knob keeps the footprint it had.
void DrawKnob(CDrawList &drawList, float x, float y, float size, Color fill, u8 alpha)
{
	drawList.AddRectRoundedFilled(x, y, size, size, CDrawList::UniformRadii(size * 0.5f),
								  ColorScaleAlpha(ColorOutlineOn(fill), alpha));

	const float inner = size - kKnobRingThickness * 2.0f;
	drawList.AddRectRoundedFilled(x + kKnobRingThickness, y + kKnobRingThickness, inner, inner,
								  CDrawList::UniformRadii(inner * 0.5f), ColorScaleAlpha(fill, alpha));
}

// onAmount is already eased by the caller, so the switch animates rather than snapping.
void DrawToggle(CDrawList &drawList, Rect rect, float onAmount, Color accent, u8 alpha)
{
	const Color track = ColorLerp(kColorToggleOff, accent, onAmount);
	drawList.AddRectRoundedFilled(rect.X, rect.Y, rect.W, rect.H, CDrawList::UniformRadii(rect.H * 0.5f),
								  ColorScaleAlpha(track, alpha));

	// Contrasted against the track it sits on, which is itself mid-lerp toward the accent - a
	// white knob disappears entirely on a bright one.
	const float dotSize = rect.H - 6.0f;
	const float dotX = rect.X + 3.0f + (rect.W - rect.H) * onAmount;
	DrawKnob(drawList, dotX, rect.Y + 3.0f, dotSize, ColorForegroundOn(track), alpha);
}

// track is the hit rect, t is where the thumb sits, and valueText is the readout drawn in
// reserved space to its left. Both are passed in, since the two sliders this serves measure
// different things and neither range belongs in a drawing function.
void DrawSlider(CDrawList &drawList, const CFont &font, Rect track, float t, std::string_view valueText, Color accent,
				u8 alpha)
{
	const float trackCenterY = track.Y + track.H * 0.5f;
	const float textBaselineY = trackCenterY + (font.GetAscent() + font.GetDescent()) * 0.5f;

	DrawText(drawList, font, track.X - kSliderValueLabelGap - TextWidth(font, valueText), textBaselineY, valueText,
			 ColorScaleAlpha(kColorText, alpha));

	const float barY = track.Y + (track.H - kSliderTrackVisualHeight) * 0.5f;
	const CornerRadii barRadii = CDrawList::UniformRadii(kSliderTrackVisualHeight * 0.5f);
	drawList.AddRectRoundedFilled(track.X, barY, track.W, kSliderTrackVisualHeight, barRadii,
								  ColorScaleAlpha(kColorToggleOff, alpha));

	const float fillW = track.W * std::clamp(t, 0.0f, 1.0f);
	if (fillW > 0.0f) {
		drawList.AddRectRoundedFilled(track.X, barY, fillW, kSliderTrackVisualHeight, barRadii,
									  ColorScaleAlpha(accent, alpha));
	}

	// Contrasted against the accent, since the thumb rides the end of the filled part.
	DrawKnob(drawList, track.X + fillW - kSliderThumbRadius, trackCenterY - kSliderThumbRadius,
			 kSliderThumbRadius * 2.0f, ColorForegroundOn(accent), alpha);
}

// Shared by both font-size rows. Rounded rather than truncated, since this is an eased display
// copy that spends most of a reset animation between two integers.
void DrawStepper(CDrawList &drawList, const CFont &font, Rect rect, float value, u8 alpha)
{
	const Rect minus = StepperMinusRect(rect);
	const Rect plus = StepperPlusRect(rect);
	const Color glyphColor = ColorScaleAlpha(kColorText, alpha);

	drawList.AddRectRoundedFilled(minus.X, minus.Y, minus.W, minus.H, CDrawList::UniformRadii(6.0f),
								  ColorScaleAlpha(kColorControlBg, alpha));
	drawList.AddRectRoundedFilled(plus.X, plus.Y, plus.W, plus.H, CDrawList::UniformRadii(6.0f),
								  ColorScaleAlpha(kColorControlBg, alpha));

	const float mcx = minus.X + minus.W * 0.5f;
	const float mcy = minus.Y + minus.H * 0.5f;
	drawList.AddLine(mcx - 6.0f, mcy, mcx + 6.0f, mcy, 2.0f, glyphColor);

	const float pcx = plus.X + plus.W * 0.5f;
	const float pcy = plus.Y + plus.H * 0.5f;
	drawList.AddLine(pcx - 6.0f, pcy, pcx + 6.0f, pcy, 2.0f, glyphColor);
	drawList.AddLine(pcx, pcy - 6.0f, pcx, pcy + 6.0f, 2.0f, glyphColor);

	char buffer[8];
	const int written = std::snprintf(buffer, sizeof(buffer), "%d", static_cast<int>(std::lround(value)));
	const std::string_view text{buffer, written > 0 ? static_cast<u64>(written) : 0};

	const float middleX = minus.X + minus.W;
	const float middleW = plus.X - middleX;
	const float baselineY = rect.Y + rect.H * 0.5f + (font.GetAscent() + font.GetDescent()) * 0.5f;

	DrawText(drawList, font, middleX + (middleW - TextWidth(font, text)) * 0.5f, baselineY, text, glyphColor);
}

// The persisted name only moves once a bake actually succeeds; it never just mirrors whatever
// is currently typed, or a bad in-flight edit would be written to disk.
void SyncAppliedFontName(Settings *pSettings, std::string_view value)
{
	CopyTo(value, pSettings->m_szFontName, sizeof(pSettings->m_szFontName));
}
} // namespace

// Header and footer are carved off the border-inset rect; whatever remains is the clipped,
// scrollable strip the row stack lives in.
struct CSettingsPanel::PanelLayout {
	Rect Panel;
	Rect Inner;
	Rect Header;
	Rect Footer;
	Rect ScrollRegion;
};

// Every row's rect, built by walking one splitting cursor down the scroll region, so rows and
// headings can never overlap however many exist. ContentHeight is the sum of every strip,
// independent of the scroll offset.
struct CSettingsPanel::Rows {
	Rect SectionAppearance;
	Rect Font;
	Rect FontSize;
	Rect SecondaryFontSize;
	Rect Accent;
	Rect CornerRoundness;

	Rect SectionMotion;
	Rect Animations;
	Rect AnimationSpeed;

	Rect SectionPrivacy;
	Rect Notifications;
	Rect ExcludeFromCapture;
	Rect BlockOverlayInjection;
	Rect CloseToTray;

	Rect SectionSecurity;
	Rect MasterPassword;

	float ContentHeight;
};

Rect CSettingsPanel::ResetTargetRowRect(const Rows &rows, ESettingsResetTarget target)
{
	switch (target) {
		case ESettingsResetTarget::Font:
			return rows.Font;
		case ESettingsResetTarget::FontSize:
			return rows.FontSize;
		case ESettingsResetTarget::SecondaryFontSize:
			return rows.SecondaryFontSize;
		case ESettingsResetTarget::Accent:
			return rows.Accent;
		case ESettingsResetTarget::CornerRoundness:
			return rows.CornerRoundness;
		case ESettingsResetTarget::Animations:
			return rows.Animations;
		case ESettingsResetTarget::AnimationSpeed:
			return rows.AnimationSpeed;
		case ESettingsResetTarget::Notifications:
			return rows.Notifications;

		case ESettingsResetTarget::ExcludeFromCapture:
			return rows.ExcludeFromCapture;

		case ESettingsResetTarget::BlockOverlayInjection:
			return rows.BlockOverlayInjection;
		case ESettingsResetTarget::CloseToTray:
			return rows.CloseToTray;
		case ESettingsResetTarget::Count:
			break;
	}

	return Rect{};
}

Rect CSettingsPanel::ResetTargetControlRect(const Rows &rows, ESettingsResetTarget target, const CFontManager *pFonts)
{
	switch (target) {
		case ESettingsResetTarget::Font:
			return FontFieldRect(rows.Font, pFonts);
		case ESettingsResetTarget::FontSize:
			return StepperRect(rows.FontSize, pFonts);
		case ESettingsResetTarget::SecondaryFontSize:
			return StepperRect(rows.SecondaryFontSize, pFonts);
		case ESettingsResetTarget::Accent:
			return SwatchRect(rows.Accent, pFonts);
		case ESettingsResetTarget::CornerRoundness:
			return SliderControlRect(rows.CornerRoundness, pFonts);
		case ESettingsResetTarget::Animations:
			return ToggleRect(rows.Animations, pFonts);
		case ESettingsResetTarget::AnimationSpeed:
			return SliderControlRect(rows.AnimationSpeed, pFonts);
		case ESettingsResetTarget::Notifications:
			return ToggleRect(rows.Notifications, pFonts);

		case ESettingsResetTarget::ExcludeFromCapture:
			return ToggleRect(rows.ExcludeFromCapture, pFonts);

		case ESettingsResetTarget::BlockOverlayInjection:
			return ToggleRect(rows.BlockOverlayInjection, pFonts);
		case ESettingsResetTarget::CloseToTray:
			return ToggleRect(rows.CloseToTray, pFonts);
		case ESettingsResetTarget::Count:
			break;
	}

	return Rect{};
}

Rect CSettingsPanel::ResetTargetButtonRect(const Rows &rows, ESettingsResetTarget target, const CFontManager *pFonts)
{
	return ResetButtonRect(ResetTargetControlRect(rows, target, pFonts), ResetTargetRowRect(rows, target), pFonts);
}

CSettingsPanel::CSettingsPanel(CFontManager *pFonts, Settings *pSettings, const CWindow &window, IRenderer *pRenderer,
							   const CAssetManager &assets)
	: m_pFonts(pFonts)
	, m_pSettings(pSettings)
	, m_window(window)
	, m_pRenderer(pRenderer)
	, m_assets(assets)
{
	m_fontNameInput.Init(m_pSettings->m_szFontName);

	// Seeded from the real values rather than left at zero: these exist only to make a change
	// animate, so the first frame must already show the truth.
	m_flFontSizeDisplay = m_pSettings->m_flFontPixelSize;
	m_flSecondaryFontSizeDisplay = m_pSettings->m_flSecondaryFontPixelSize;
	m_flAnimationSpeedDisplay = m_pSettings->m_flAnimationSpeed;
	m_flCornerRoundnessDisplay = m_pSettings->m_flCornerRoundness;
	m_flAccentDisplayR = static_cast<float>(m_pSettings->m_clrAccent.R);
	m_flAccentDisplayG = static_cast<float>(m_pSettings->m_clrAccent.G);
	m_flAccentDisplayB = static_cast<float>(m_pSettings->m_clrAccent.B);
}

CSettingsPanel::PanelLayout CSettingsPanel::Layout() const
{
	const auto windowW = static_cast<float>(m_window.GetWidth());
	const auto windowH = static_cast<float>(m_window.GetHeight());

	PanelLayout layout{};
	layout.Panel = PanelRect(m_flOpenAmount, windowW, windowH, m_pFonts);
	layout.Inner = RectInset(layout.Panel, kPanelBorderThickness);

	Rect cursor = layout.Inner;
	layout.Header = RectSplitTop(cursor, HeaderHeightFor(m_pFonts));
	layout.Footer = RectSplitBottom(cursor, FooterHeightFor(m_pFonts));
	layout.ScrollRegion = cursor;

	return layout;
}

// Rows are grouped under section headings rather than listed in one undifferentiated stack:
// what a setting affects is the only ordering a reader can navigate by. The first heading gets
// a smaller top gap, since it has no preceding group to separate from.
CSettingsPanel::Rows CSettingsPanel::RowsFor(const PanelLayout &layout) const
{
	const float rowHeight = RowHeightFor(m_pFonts);
	const float sectionHeight = SectionHeaderHeightFor(m_pFonts);

	Rect cursor{layout.ScrollRegion.X, layout.ScrollRegion.Y - m_rowsScroll.m_flScrollOffset, layout.ScrollRegion.W,
				1.0e6f};
	const float startY = cursor.Y;

	Rows rows{};
	rows.SectionAppearance = RectSplitTop(cursor, sectionHeight - kSectionHeaderTopGap * 0.5f);
	rows.Font = RectSplitTop(cursor, rowHeight);
	rows.FontSize = RectSplitTop(cursor, rowHeight);
	rows.SecondaryFontSize = RectSplitTop(cursor, rowHeight);
	rows.Accent = RectSplitTop(cursor, rowHeight);
	rows.CornerRoundness = RectSplitTop(cursor, rowHeight);
	rows.Notifications = RectSplitTop(cursor, rowHeight);

	rows.SectionMotion = RectSplitTop(cursor, sectionHeight);
	rows.Animations = RectSplitTop(cursor, rowHeight);
	rows.AnimationSpeed = RectSplitTop(cursor, rowHeight);

	rows.SectionPrivacy = RectSplitTop(cursor, sectionHeight);
	rows.ExcludeFromCapture = RectSplitTop(cursor, rowHeight);
	rows.BlockOverlayInjection = RectSplitTop(cursor, rowHeight);
	rows.CloseToTray = RectSplitTop(cursor, rowHeight);

	rows.SectionSecurity = RectSplitTop(cursor, sectionHeight);
	rows.MasterPassword = RectSplitTop(cursor, rowHeight);

	rows.ContentHeight = cursor.Y - startY;

	return rows;
}

void CSettingsPanel::ApplyFontSettings()
{
	if (m_pFonts->ApplyBody(m_pRenderer, m_fontNameInput.GetValue(), m_pSettings->m_flFontPixelSize,
							m_pSettings->m_flSecondaryFontPixelSize, m_window.GetDpiScale())) {
		SyncAppliedFontName(m_pSettings, m_fontNameInput.GetValue());
	}
}

bool CSettingsPanel::IsRowControlHit(const PanelLayout &layout, Rect row, Rect control, float x, float y) const
{
	return RowInView(row, layout.ScrollRegion) && RectContainsPoint(control, x, y);
}

void CSettingsPanel::Open()
{
	m_bOpen = true;
}

void CSettingsPanel::Close()
{
	m_bOpen = false;
	m_fontNameInput.m_bFocused = false;
	m_colorPicker.Close();

	// Nothing left on screen for a bubble to point at.
	m_tooltip.Reset();
}

bool CSettingsPanel::IsTargetAtDefault(ESettingsResetTarget target) const
{
	// Straight off a default-constructed record: Settings' member initializers are the one
	// place a default is stated, and a copy here would drift.
	const Settings defaults;
	constexpr float kEpsilon = 0.001f;

	switch (target) {
		case ESettingsResetTarget::Font:
			// Both the applied name and whatever is typed: an un-applied edit sitting in the
			// box is exactly what a reset is for.
			return std::strcmp(m_pSettings->m_szFontName, defaults.m_szFontName) == 0 &&
				   m_fontNameInput.GetValue() == std::string_view{defaults.m_szFontName};

		case ESettingsResetTarget::FontSize:
			return std::fabs(m_pSettings->m_flFontPixelSize - defaults.m_flFontPixelSize) < kEpsilon;

		case ESettingsResetTarget::SecondaryFontSize:
			return std::fabs(m_pSettings->m_flSecondaryFontPixelSize - defaults.m_flSecondaryFontPixelSize) < kEpsilon;

		case ESettingsResetTarget::Accent:
			return m_pSettings->m_clrAccent.R == defaults.m_clrAccent.R &&
				   m_pSettings->m_clrAccent.G == defaults.m_clrAccent.G &&
				   m_pSettings->m_clrAccent.B == defaults.m_clrAccent.B &&
				   m_pSettings->m_clrAccent.A == defaults.m_clrAccent.A;

		case ESettingsResetTarget::CornerRoundness:
			return std::fabs(m_pSettings->m_flCornerRoundness - defaults.m_flCornerRoundness) < kEpsilon;

		case ESettingsResetTarget::Animations:
			return m_pSettings->m_bAnimationsEnabled == defaults.m_bAnimationsEnabled;

		case ESettingsResetTarget::AnimationSpeed:
			return std::fabs(m_pSettings->m_flAnimationSpeed - defaults.m_flAnimationSpeed) < kEpsilon;

		case ESettingsResetTarget::Notifications:
			return m_pSettings->m_bShowNotifications == defaults.m_bShowNotifications;

		case ESettingsResetTarget::ExcludeFromCapture:
			return m_pSettings->m_bExcludeAccountListFromCapture == defaults.m_bExcludeAccountListFromCapture;

		case ESettingsResetTarget::BlockOverlayInjection:
			return m_pSettings->m_bBlockOverlayInjection == defaults.m_bBlockOverlayInjection;

		case ESettingsResetTarget::CloseToTray:
			return m_pSettings->m_bCloseToTray == defaults.m_bCloseToTray;

		case ESettingsResetTarget::Count:
			break;
	}

	return true;
}

void CSettingsPanel::ResetTargetToDefault(ESettingsResetTarget target)
{
	const Settings defaults;

	switch (target) {
		case ESettingsResetTarget::Font:
			// The default face can be missing on a machine too, so it goes through the same
			// apply-then-sync rule.
			m_fontNameInput.SetValue(defaults.m_szFontName);
			ApplyFontSettings();
			break;

		case ESettingsResetTarget::FontSize:
			m_pSettings->m_flFontPixelSize = defaults.m_flFontPixelSize;
			ApplyFontSettings();
			break;

		case ESettingsResetTarget::SecondaryFontSize:
			m_pSettings->m_flSecondaryFontPixelSize = defaults.m_flSecondaryFontPixelSize;
			ApplyFontSettings();
			break;

		case ESettingsResetTarget::Accent:
			m_pSettings->m_clrAccent = defaults.m_clrAccent;
			// The picker would be showing the colour that just stopped being current.
			m_colorPicker.Close();
			break;

		case ESettingsResetTarget::CornerRoundness:
			// No immediate scale change: Update drives the real scale from the eased copy, so
			// leaving this alone is what animates the reset instead of snapping.
			m_pSettings->m_flCornerRoundness = defaults.m_flCornerRoundness;
			break;

		case ESettingsResetTarget::Animations:
			m_pSettings->m_bAnimationsEnabled = defaults.m_bAnimationsEnabled;
			CAnimator::SetEnabled(m_pSettings->m_bAnimationsEnabled);
			break;

		case ESettingsResetTarget::AnimationSpeed:
			m_pSettings->m_flAnimationSpeed = defaults.m_flAnimationSpeed;
			CAnimator::SetSpeed(m_pSettings->m_flAnimationSpeed);
			break;

		case ESettingsResetTarget::Notifications:
			m_pSettings->m_bShowNotifications = defaults.m_bShowNotifications;
			break;

		case ESettingsResetTarget::ExcludeFromCapture:
			m_pSettings->m_bExcludeAccountListFromCapture = defaults.m_bExcludeAccountListFromCapture;
			break;

		case ESettingsResetTarget::BlockOverlayInjection:
			m_pSettings->m_bBlockOverlayInjection = defaults.m_bBlockOverlayInjection;
			break;

		case ESettingsResetTarget::CloseToTray:
			m_pSettings->m_bCloseToTray = defaults.m_bCloseToTray;
			break;

		case ESettingsResetTarget::Count:
			return;
	}

	m_aResetSpinAmount[static_cast<u64>(target)] = 1.0f;
}

void CSettingsPanel::Update(float deltaSeconds)
{
	m_flOpenAmount = CAnimator::EaseToward(m_flOpenAmount, m_bOpen ? 1.0f : 0.0f, kPanelOpenEaseRate, deltaSeconds);
	if (!m_bOpen && m_flOpenAmount < 0.002f) {
		m_flOpenAmount = 0.0f;
	}

	// These snap instantly when animations are switched off, since EaseToward respects that
	// flag itself.
	m_flAnimationsToggleAmount = CAnimator::EaseToward(
		m_flAnimationsToggleAmount, m_pSettings->m_bAnimationsEnabled ? 1.0f : 0.0f, kToggleEaseRate, deltaSeconds);
	m_flCloseToTrayToggleAmount = CAnimator::EaseToward(
		m_flCloseToTrayToggleAmount, m_pSettings->m_bCloseToTray ? 1.0f : 0.0f, kToggleEaseRate, deltaSeconds);
	m_flNotificationsToggleAmount = CAnimator::EaseToward(
		m_flNotificationsToggleAmount, m_pSettings->m_bShowNotifications ? 1.0f : 0.0f, kToggleEaseRate, deltaSeconds);

	m_flBlockOverlayInjectionToggleAmount =
		CAnimator::EaseToward(m_flBlockOverlayInjectionToggleAmount,
							  m_pSettings->m_bBlockOverlayInjection ? 1.0f : 0.0f, kToggleEaseRate, deltaSeconds);

	m_flExcludeFromCaptureToggleAmount = CAnimator::EaseToward(
		m_flExcludeFromCaptureToggleAmount, m_pSettings->m_bExcludeAccountListFromCapture ? 1.0f : 0.0f,
		kToggleEaseRate, deltaSeconds);

	m_flFontSizeDisplay =
		CAnimator::EaseToward(m_flFontSizeDisplay, m_pSettings->m_flFontPixelSize, kValueEaseRate, deltaSeconds);
	m_flSecondaryFontSizeDisplay = CAnimator::EaseToward(
		m_flSecondaryFontSizeDisplay, m_pSettings->m_flSecondaryFontPixelSize, kValueEaseRate, deltaSeconds);

	// A slider being dragged is the one exception: easing there would make the thumb trail the
	// cursor, which reads as lag rather than as animation.
	m_flAnimationSpeedDisplay = m_animationSpeedDrag.IsPressed()
									? m_pSettings->m_flAnimationSpeed
									: CAnimator::EaseToward(m_flAnimationSpeedDisplay, m_pSettings->m_flAnimationSpeed,
															kValueEaseRate, deltaSeconds);
	m_flCornerRoundnessDisplay =
		m_cornerRoundnessDrag.IsPressed()
			? m_pSettings->m_flCornerRoundness
			: CAnimator::EaseToward(m_flCornerRoundnessDisplay, m_pSettings->m_flCornerRoundness, kValueEaseRate,
									deltaSeconds);

	// The one display copy that also drives the real thing, so a reset travels back as if the
	// slider were dragged there. Runs whether the panel is open or not.
	CDrawList::SetCornerRoundnessScale(m_flCornerRoundnessDisplay);

	m_flAccentDisplayR = CAnimator::EaseToward(m_flAccentDisplayR, static_cast<float>(m_pSettings->m_clrAccent.R),
											   kValueEaseRate, deltaSeconds);
	m_flAccentDisplayG = CAnimator::EaseToward(m_flAccentDisplayG, static_cast<float>(m_pSettings->m_clrAccent.G),
											   kValueEaseRate, deltaSeconds);
	m_flAccentDisplayB = CAnimator::EaseToward(m_flAccentDisplayB, static_cast<float>(m_pSettings->m_clrAccent.B),
											   kValueEaseRate, deltaSeconds);

	UpdateResetButtons(deltaSeconds);

	m_fontNameInput.Update(deltaSeconds);
	m_rowsScroll.Update(deltaSeconds);
	m_tooltip.Update(deltaSeconds);
}

void CSettingsPanel::UpdateResetButtons(float deltaSeconds)
{
	const PanelLayout layout = Layout();
	const Rows rows = RowsFor(layout);

	// A closed panel has no hover to speak of, and the picker's popup covers rows the cursor
	// would otherwise look like it is over.
	const bool pointerLive =
		IsBlocking() && !m_colorPicker.IsBlocking() && RectContainsPoint(layout.ScrollRegion, m_flMouseX, m_flMouseY);

	for (u64 i = 0; i < static_cast<u64>(ESettingsResetTarget::Count); i += 1) {
		const auto target = static_cast<ESettingsResetTarget>(i);
		const bool atDefault = IsTargetAtDefault(target);

		// Shown only while it would do something: its presence is the signal that there is
		// something to restore.
		const float appearTarget = IsBlocking() && !atDefault ? 1.0f : 0.0f;
		m_aResetAppearAmount[i] =
			CAnimator::EaseToward(m_aResetAppearAmount[i], appearTarget, kResetAppearRate, deltaSeconds);
		m_aResetSpinAmount[i] = CAnimator::EaseToward(m_aResetSpinAmount[i], 0.0f, kResetSpinRate, deltaSeconds);

		if (m_aResetSpinAmount[i] < 0.002f) {
			m_aResetSpinAmount[i] = 0.0f;
		}

		if (!pointerLive || atDefault || !RowInView(ResetTargetRowRect(rows, target), layout.ScrollRegion)) {
			continue;
		}

		const Rect button = ResetTargetButtonRect(rows, target, m_pFonts);
		if (RectContainsPoint(button, m_flMouseX, m_flMouseY)) {
			m_tooltip.Request("Restore default setting.", button);
		}
	}
}

void CSettingsPanel::ApplyAnimationSpeedFromPointer(Rect track, float x)
{
	m_pSettings->m_flAnimationSpeed = AnimationSpeedFromT((x - track.X) / track.W);
	CAnimator::SetSpeed(m_pSettings->m_flAnimationSpeed);
}

void CSettingsPanel::ApplyCornerRoundnessFromPointer(Rect track, float x)
{
	m_pSettings->m_flCornerRoundness = CornerRoundnessFromT((x - track.X) / track.W);

	// Applied live while dragging - the whole UI rounds off under the cursor.
	CDrawList::SetCornerRoundnessScale(m_pSettings->m_flCornerRoundness);
}

bool CSettingsPanel::OnPointerDown(float x, float y)
{
	if (!IsBlocking()) return false;

	const PanelLayout layout = Layout();
	const Rows rows = RowsFor(layout);

	if (m_rowsScroll.OnPointerDown(x, y, ScrollbarTrackRect(layout.ScrollRegion), rows.ContentHeight,
								   layout.ScrollRegion.H)) {
		return true;
	}

	if (m_colorPicker.OnPointerDown(x, y)) {
		m_pSettings->m_clrAccent = m_colorPicker.GetCurrentColor();
		return true;
	}

	const Rect speedTrack = SliderTrackRect(rows.AnimationSpeed, m_pFonts);
	if (IsRowControlHit(layout, rows.AnimationSpeed, speedTrack, x, y)) {
		m_animationSpeedDrag.Begin(x, y);
		ApplyAnimationSpeedFromPointer(speedTrack, x);
		return true;
	}

	const Rect roundnessTrack = SliderTrackRect(rows.CornerRoundness, m_pFonts);
	if (IsRowControlHit(layout, rows.CornerRoundness, roundnessTrack, x, y)) {
		m_cornerRoundnessDrag.Begin(x, y);
		ApplyCornerRoundnessFromPointer(roundnessTrack, x);
		return true;
	}

	// Swallow every other press while open.
	return true;
}

bool CSettingsPanel::OnPointerMove(float x, float y)
{
	if (!IsBlocking()) return false;

	const PanelLayout layout = Layout();
	const Rows rows = RowsFor(layout);

	// A no-op unless a thumb drag is actually in progress - the function guards itself.
	m_rowsScroll.OnPointerMove(y, ScrollbarTrackRect(layout.ScrollRegion), rows.ContentHeight, layout.ScrollRegion.H);

	if (m_colorPicker.IsDragging()) {
		m_colorPicker.OnPointerMove(x, y);
		m_pSettings->m_clrAccent = m_colorPicker.GetCurrentColor();
	}

	if (m_animationSpeedDrag.IsPressed()) {
		m_animationSpeedDrag.Update(x, y);
		ApplyAnimationSpeedFromPointer(SliderTrackRect(rows.AnimationSpeed, m_pFonts), x);
	}

	if (m_cornerRoundnessDrag.IsPressed()) {
		m_cornerRoundnessDrag.Update(x, y);
		ApplyCornerRoundnessFromPointer(SliderTrackRect(rows.CornerRoundness, m_pFonts), x);
	}

	return true;
}

bool CSettingsPanel::OnPointerUp(float x, float y)
{
	if (!IsBlocking()) return false;

	const bool wasDraggingScrollbar = m_rowsScroll.IsDragging();
	m_rowsScroll.OnPointerUp();

	const bool wasDraggingColor = m_colorPicker.IsDragging();
	m_colorPicker.OnPointerUp(x, y);

	const bool wasDraggingSlider = m_animationSpeedDrag.IsPressed() || m_cornerRoundnessDrag.IsPressed();
	m_animationSpeedDrag.End();
	m_cornerRoundnessDrag.End();

	if (wasDraggingScrollbar || wasDraggingColor || wasDraggingSlider) return true;

	return HandleClick(x, y);
}

ECursorKind CSettingsPanel::GetDesiredCursor() const
{
	if (!IsBlocking()) return ECursorKind::Arrow;

	if (m_rowsScroll.IsDragging() || m_colorPicker.IsDragging() || m_animationSpeedDrag.IsPressed() ||
		m_cornerRoundnessDrag.IsPressed()) {
		return ECursorKind::Drag;
	}

	// The picker is a plain member rather than a stack entry, so its cursor is forwarded here.
	if (m_colorPicker.IsBlocking()) {
		const ECursorKind pickerCursor = m_colorPicker.GetDesiredCursor();
		if (pickerCursor != ECursorKind::Arrow) return pickerCursor;
	}

	const PanelLayout layout = Layout();

	if (RectContainsPoint(CloseRect(layout.Panel, m_pFonts), m_flMouseX, m_flMouseY)) return ECursorKind::Hand;

	if (!RectContainsPoint(layout.ScrollRegion, m_flMouseX, m_flMouseY)) return ECursorKind::Arrow;

	const Rows rows = RowsFor(layout);

	// Same order as the click dispatch: a reset button sits inside its row, so it answers first.
	for (u64 i = 0; i < static_cast<u64>(ESettingsResetTarget::Count); i += 1) {
		const auto target = static_cast<ESettingsResetTarget>(i);
		if (IsTargetAtDefault(target)) continue;

		if (IsRowControlHit(layout, ResetTargetRowRect(rows, target), ResetTargetButtonRect(rows, target, m_pFonts),
							m_flMouseX, m_flMouseY)) {
			return ECursorKind::Hand;
		}
	}

	if (IsRowControlHit(layout, rows.Font, FontFieldRect(rows.Font, m_pFonts), m_flMouseX, m_flMouseY)) {
		return ECursorKind::IBeam;
	}

	const Rect fontSizeStepper = StepperRect(rows.FontSize, m_pFonts);
	const Rect secondaryStepper = StepperRect(rows.SecondaryFontSize, m_pFonts);

	// Only the buttons are clickable, not the number between them.
	const struct {
		Rect Row;
		Rect Control;
	} handTargets[]{
		{rows.FontSize, StepperMinusRect(fontSizeStepper)},
		{rows.FontSize, StepperPlusRect(fontSizeStepper)},
		{rows.SecondaryFontSize, StepperMinusRect(secondaryStepper)},
		{rows.SecondaryFontSize, StepperPlusRect(secondaryStepper)},
		{rows.Animations, ToggleRect(rows.Animations, m_pFonts)},
		{rows.Notifications, ToggleRect(rows.Notifications, m_pFonts)},
		{rows.ExcludeFromCapture, ToggleRect(rows.ExcludeFromCapture, m_pFonts)},
		{rows.BlockOverlayInjection, ToggleRect(rows.BlockOverlayInjection, m_pFonts)},
		{rows.CloseToTray, ToggleRect(rows.CloseToTray, m_pFonts)},
		{rows.AnimationSpeed, SliderTrackRect(rows.AnimationSpeed, m_pFonts)},
		{rows.CornerRoundness, SliderTrackRect(rows.CornerRoundness, m_pFonts)},
		{rows.Accent, SwatchRect(rows.Accent, m_pFonts)},
		{rows.MasterPassword, MasterPasswordButtonRect(rows.MasterPassword, m_pFonts)},
	};

	for (const auto &target : handTargets) {
		if (IsRowControlHit(layout, target.Row, target.Control, m_flMouseX, m_flMouseY)) return ECursorKind::Hand;
	}

	if (CScrollable::IsVisible(rows.ContentHeight, layout.ScrollRegion.H) &&
		RectContainsPoint(ScrollbarTrackRect(layout.ScrollRegion), m_flMouseX, m_flMouseY)) {
		return ECursorKind::Hand;
	}

	return ECursorKind::Arrow;
}

bool CSettingsPanel::HandleResetButtonClick(const PanelLayout &layout, const Rows &rows, float x, float y)
{
	if (m_colorPicker.IsBlocking()) return false;

	for (u64 i = 0; i < static_cast<u64>(ESettingsResetTarget::Count); i += 1) {
		const auto target = static_cast<ESettingsResetTarget>(i);
		if (IsTargetAtDefault(target)) continue;

		if (IsRowControlHit(layout, ResetTargetRowRect(rows, target), ResetTargetButtonRect(rows, target, m_pFonts), x,
							y)) {
			ResetTargetToDefault(target);
			return true;
		}
	}

	return false;
}

bool CSettingsPanel::HandleAccentSwatchClick(const PanelLayout &layout, const Rows &rows, float x, float y)
{
	const Rect swatch = SwatchRect(rows.Accent, m_pFonts);

	if (IsRowControlHit(layout, rows.Accent, swatch, x, y)) {
		if (m_colorPicker.IsBlocking()) {
			m_colorPicker.Close();
		} else {
			m_colorPicker.Open(m_pSettings->m_clrAccent, swatch, static_cast<float>(m_window.GetWidth()),
							   static_cast<float>(m_window.GetHeight()));
		}

		return true;
	}

	// Click-anywhere-else closes; the colour was already applied live while dragging. A click
	// inside the popup was consumed by the picker itself.
	if (m_colorPicker.IsBlocking()) {
		m_colorPicker.Close();
		return true;
	}

	return false;
}

void CSettingsPanel::HandleStepperClick(Rect stepper, float &value, float minValue, float maxValue, float x, float y)
{
	if (RectContainsPoint(StepperMinusRect(stepper), x, y)) {
		value = std::max(minValue, value - 1.0f);
		ApplyFontSettings();
	} else if (RectContainsPoint(StepperPlusRect(stepper), x, y)) {
		value = std::min(maxValue, value + 1.0f);
		ApplyFontSettings();
	}
}

bool CSettingsPanel::HandleClick(float x, float y)
{
	const PanelLayout layout = Layout();

	if (RectContainsPoint(CloseRect(layout.Panel, m_pFonts), x, y) || !RectContainsPoint(layout.Panel, x, y)) {
		Close();
		return true;
	}

	const Rows rows = RowsFor(layout);

	// A click in the header or footer can never hit a row.
	if (!RectContainsPoint(layout.ScrollRegion, x, y)) return true;

	if (HandleResetButtonClick(layout, rows, x, y)) return true;

	const bool clickedFontField = IsRowControlHit(layout, rows.Font, FontFieldRect(rows.Font, m_pFonts), x, y);
	m_fontNameInput.m_bFocused = clickedFontField;

	if (clickedFontField) {
		// Click-to-position is not implemented; the end is a reasonable default.
		m_fontNameInput.SetValue(m_fontNameInput.GetValue());
	}

	if (RowInView(rows.FontSize, layout.ScrollRegion)) {
		HandleStepperClick(StepperRect(rows.FontSize, m_pFonts), m_pSettings->m_flFontPixelSize, kFontSizeMin,
						   kFontSizeMax, x, y);
	}

	if (RowInView(rows.SecondaryFontSize, layout.ScrollRegion)) {
		HandleStepperClick(StepperRect(rows.SecondaryFontSize, m_pFonts), m_pSettings->m_flSecondaryFontPixelSize,
						   kSecondaryFontSizeMin, kSecondaryFontSizeMax, x, y);
	}

	if (IsRowControlHit(layout, rows.Animations, ToggleRect(rows.Animations, m_pFonts), x, y)) {
		m_pSettings->m_bAnimationsEnabled = !m_pSettings->m_bAnimationsEnabled;
		CAnimator::SetEnabled(m_pSettings->m_bAnimationsEnabled);
	}

	if (IsRowControlHit(layout, rows.Notifications, ToggleRect(rows.Notifications, m_pFonts), x, y)) {
		m_pSettings->m_bShowNotifications = !m_pSettings->m_bShowNotifications;
	}

	if (IsRowControlHit(layout, rows.ExcludeFromCapture, ToggleRect(rows.ExcludeFromCapture, m_pFonts), x, y)) {
		m_pSettings->m_bExcludeAccountListFromCapture = !m_pSettings->m_bExcludeAccountListFromCapture;
	}

	// Only read at startup, so the toggle records an intent for the next launch rather than
	// doing anything now. The row's own description is what says so.
	if (IsRowControlHit(layout, rows.BlockOverlayInjection, ToggleRect(rows.BlockOverlayInjection, m_pFonts), x, y)) {
		m_pSettings->m_bBlockOverlayInjection = !m_pSettings->m_bBlockOverlayInjection;
	}

	if (IsRowControlHit(layout, rows.CloseToTray, ToggleRect(rows.CloseToTray, m_pFonts), x, y)) {
		m_pSettings->m_bCloseToTray = !m_pSettings->m_bCloseToTray;
	}

	// The sliders are dragged rather than clicked - see OnPointerDown and OnPointerMove.

	if (HandleAccentSwatchClick(layout, rows, x, y)) return true;

	if (IsRowControlHit(layout, rows.MasterPassword, MasterPasswordButtonRect(rows.MasterPassword, m_pFonts), x, y)) {
		// Closed here, not just latched: the owner is about to take the whole app over with the
		// setup screen.
		m_bResetPasswordRequestedThisFrame = true;
		Close();
	}

	return true;
}

bool CSettingsPanel::OnScroll(float x, float y, float wheelDelta)
{
	if (!IsBlocking()) return false;

	const PanelLayout layout = Layout();
	const Rows rows = RowsFor(layout);

	m_rowsScroll.OnScroll(wheelDelta, rows.ContentHeight, layout.ScrollRegion.H);

	return true;
}

bool CSettingsPanel::OnChar(u32 character)
{
	if (!IsBlocking()) return false;

	// A no-op on an unfocused field, so this is safe to call unconditionally.
	m_fontNameInput.OnChar(character);

	return true;
}

bool CSettingsPanel::OnKeyDown(u32 keyCode)
{
	if (!IsBlocking()) return false;

	if (keyCode == VK_ESCAPE) {
		Close();
		return true;
	}

	if (!m_fontNameInput.m_bFocused) return true;

	if (keyCode == VK_RETURN) {
		ApplyFontSettings();
	} else {
		m_fontNameInput.OnKey(keyCode);
	}

	return true;
}

bool CSettingsPanel::ConsumeResetPasswordRequested()
{
	const bool requested = m_bResetPasswordRequestedThisFrame;
	m_bResetPasswordRequestedThisFrame = false;

	return requested;
}

void CSettingsPanel::DrawChrome(CDrawList &drawList, const PanelLayout &layout, u8 alpha) const
{
	const auto windowW = static_cast<float>(m_window.GetWidth());
	const auto windowH = static_cast<float>(m_window.GetHeight());
	const CFont &body = m_pFonts->GetBody();
	const CFont &secondary = m_pFonts->GetSecondary();

	drawList.AddRectFilled(0.0f, 0.0f, windowW, windowH, Color{0, 0, 0, static_cast<u8>(140.0f * m_flOpenAmount)});

	drawList.AddRectRoundedFilled(layout.Panel.X, layout.Panel.Y, layout.Panel.W, layout.Panel.H,
								  CDrawList::UniformRadii(kPanelRadius), ColorScaleAlpha(kColorBorder, alpha));
	drawList.AddRectRoundedFilled(layout.Inner.X, layout.Inner.Y, layout.Inner.W, layout.Inner.H,
								  CDrawList::UniformRadii(kPanelRadius - kPanelBorderThickness),
								  ColorScaleAlpha(kColorBg, alpha));

	const float headerBaselineY =
		layout.Header.Y + layout.Header.H * 0.5f + (body.GetAscent() + body.GetDescent()) * 0.5f;
	DrawText(drawList, body, layout.Header.X + kRowPaddingX, headerBaselineY, "Settings",
			 ColorScaleAlpha(kColorText, alpha));

	const Rect close = CloseRect(layout.Panel, m_pFonts);
	const bool hoverClose = RectContainsPoint(close, m_flMouseX, m_flMouseY);
	Controls::DrawXGlyph(drawList, close, ColorScaleAlpha(hoverClose ? kColorText : kColorTextDim, alpha));

	drawList.AddRectFilled(layout.Header.X + kRowPaddingX, layout.Header.Y + layout.Header.H,
						   layout.Header.W - kRowPaddingX * 2.0f, 1.0f, ColorScaleAlpha(kColorSeparator, alpha));

	drawList.AddRectFilled(layout.Footer.X + kRowPaddingX, layout.Footer.Y, layout.Footer.W - kRowPaddingX * 2.0f, 1.0f,
						   ColorScaleAlpha(kColorSeparator, alpha));

	const float footerBaselineY =
		layout.Footer.Y + layout.Footer.H * 0.5f + (secondary.GetAscent() + secondary.GetDescent()) * 0.5f;
	DrawText(drawList, secondary, layout.Footer.X + kRowPaddingX, footerBaselineY, "Escape to dismiss",
			 ColorScaleAlpha(kColorTextDim, alpha));
}

// Painted before any row content, so every label and control lands on top of it. Not eased: a
// highlight that fades behind a moving cursor trails the thing it marks.
void CSettingsPanel::DrawRowHoverHighlight(CDrawList &drawList, const PanelLayout &layout, const Rows &rows,
										   u8 alpha) const
{
	if (!IsBlocking() || m_colorPicker.IsBlocking() ||
		!RectContainsPoint(layout.ScrollRegion, m_flMouseX, m_flMouseY)) {
		return;
	}

	const Rect hoverableRows[]{
		rows.Font,
		rows.FontSize,
		rows.SecondaryFontSize,
		rows.Accent,
		rows.CornerRoundness,
		rows.Notifications,
		rows.Animations,
		rows.AnimationSpeed,
		rows.ExcludeFromCapture,
		rows.BlockOverlayInjection,
		rows.CloseToTray,
		rows.MasterPassword,
	};

	for (const Rect &row : hoverableRows) {
		const Rect highlight = RowHighlightRect(row);
		if (!RowInView(row, layout.ScrollRegion) || !RectContainsPoint(highlight, m_flMouseX, m_flMouseY)) continue;

		drawList.AddRectRoundedFilled(highlight.X, highlight.Y, highlight.W, highlight.H,
									  CDrawList::UniformRadii(kRowHighlightRadius),
									  ColorScaleAlpha(kColorRowHover, alpha));
		break;
	}
}

void CSettingsPanel::DrawSectionHeaders(CDrawList &drawList, const PanelLayout &layout, const Rows &rows,
										u8 alpha) const
{
	const struct {
		Rect Strip;
		const char *pTitle;
	} sections[]{
		{rows.SectionAppearance, "Appearance"},
		{rows.SectionMotion, "Motion"},
		{rows.SectionPrivacy, "Privacy"},
		{rows.SectionSecurity, "Security"},
	};

	for (const auto &section : sections) {
		if (RowInView(section.Strip, layout.ScrollRegion)) {
			DrawSectionHeader(drawList, m_pFonts, section.Strip, section.pTitle, alpha);
		}
	}
}

void CSettingsPanel::DrawAppearanceRows(CDrawList &drawList, const PanelLayout &layout, const Rows &rows, u8 alpha)
{
	const CFont &body = m_pFonts->GetBody();
	const Color accent = m_pSettings->m_clrAccent;

	if (RowInView(rows.Font, layout.ScrollRegion)) {
		const Rect field = FontFieldRect(rows.Font, m_pFonts);
		DrawRowLabel(drawList, m_pFonts, rows.Font, "Font", "The system font file applied to the UI.",
					 LabelRightEdge(field, rows.Font, m_pFonts), alpha);

		// A border ring rather than a tinted fill, which read as ambiguous about which field
		// was active.
		const bool focused = m_fontNameInput.m_bFocused;
		drawList.AddRectRoundedFilled(field.X, field.Y, field.W, field.H, CDrawList::UniformRadii(6.0f),
									  ColorScaleAlpha(focused ? accent : kColorControlBg, alpha));
		drawList.AddRectRoundedFilled(field.X + 1.5f, field.Y + 1.5f, field.W - 3.0f, field.H - 3.0f,
									  CDrawList::UniformRadii(5.0f),
									  ColorScaleAlpha(focused ? ColorLerp(kColorBg, accent, 0.25f) : kColorBg, alpha));

		m_fontNameInput.Draw(drawList, body, field.X, field.Y, field.W, field.H, ColorScaleAlpha(kColorText, alpha),
							 ColorScaleAlpha(accent, alpha), false);
	}

	if (RowInView(rows.FontSize, layout.ScrollRegion)) {
		const Rect stepper = StepperRect(rows.FontSize, m_pFonts);
		DrawRowLabel(drawList, m_pFonts, rows.FontSize, "Font Size", "Determines the scale of the whole UI.",
					 LabelRightEdge(stepper, rows.FontSize, m_pFonts), alpha);
		DrawStepper(drawList, body, stepper, m_flFontSizeDisplay, alpha);
	}

	if (RowInView(rows.SecondaryFontSize, layout.ScrollRegion)) {
		const Rect stepper = StepperRect(rows.SecondaryFontSize, m_pFonts);
		DrawRowLabel(drawList, m_pFonts, rows.SecondaryFontSize, "Secondary Font Size",
					 "Scale of labels/hints and other small text.",
					 LabelRightEdge(stepper, rows.SecondaryFontSize, m_pFonts), alpha);
		DrawStepper(drawList, body, stepper, m_flSecondaryFontSizeDisplay, alpha);
	}

	if (RowInView(rows.Accent, layout.ScrollRegion)) {
		const Rect swatch = SwatchRect(rows.Accent, m_pFonts);
		DrawRowLabel(drawList, m_pFonts, rows.Accent, "Accent Color",
					 "Click to pick the selection/button accent color.", LabelRightEdge(swatch, rows.Accent, m_pFonts),
					 alpha);

		// The eased copy, so restoring the default travels there instead of cutting.
		const Color swatchColor{
			static_cast<u8>(std::lround(m_flAccentDisplayR)),
			static_cast<u8>(std::lround(m_flAccentDisplayG)),
			static_cast<u8>(std::lround(m_flAccentDisplayB)),
			accent.A,
		};

		drawList.AddRectRoundedFilled(swatch.X, swatch.Y, swatch.W, swatch.H, CDrawList::UniformRadii(6.0f),
									  ColorScaleAlpha(swatchColor, alpha));
	}

	if (RowInView(rows.CornerRoundness, layout.ScrollRegion)) {
		DrawRowLabel(
			drawList, m_pFonts, rows.CornerRoundness, "Corner Roundness", "How round those corners actually are.",
			LabelRightEdge(SliderControlRect(rows.CornerRoundness, m_pFonts), rows.CornerRoundness, m_pFonts), alpha);

		// A percentage rather than a multiplier: this scales a length nobody knows in pixels.
		char buffer[8];
		const int written = std::snprintf(buffer, sizeof(buffer), "%.0f%%", m_flCornerRoundnessDisplay * 100.0f);
		const std::string_view valueText{buffer, written > 0 ? static_cast<u64>(written) : 0};

		DrawSlider(drawList, body, SliderTrackRect(rows.CornerRoundness, m_pFonts),
				   CornerRoundnessToT(m_flCornerRoundnessDisplay), valueText, accent, alpha);
	}

	if (RowInView(rows.Notifications, layout.ScrollRegion)) {
		const Rect toggle = ToggleRect(rows.Notifications, m_pFonts);
		DrawRowLabel(drawList, m_pFonts, rows.Notifications, "Notifications",
					 "Confirms saves, deletions and resets in the bottom-left corner.",
					 LabelRightEdge(toggle, rows.Notifications, m_pFonts), alpha);
		DrawToggle(drawList, toggle, m_flNotificationsToggleAmount, accent, alpha);
	}
}

void CSettingsPanel::DrawMotionRows(CDrawList &drawList, const PanelLayout &layout, const Rows &rows, u8 alpha) const
{
	const CFont &body = m_pFonts->GetBody();
	const Color accent = m_pSettings->m_clrAccent;

	if (RowInView(rows.Animations, layout.ScrollRegion)) {
		const Rect toggle = ToggleRect(rows.Animations, m_pFonts);
		DrawRowLabel(drawList, m_pFonts, rows.Animations, "Animations",
					 "Applies animations to popups, scrolling, and the caret.",
					 LabelRightEdge(toggle, rows.Animations, m_pFonts), alpha);
		DrawToggle(drawList, toggle, m_flAnimationsToggleAmount, accent, alpha);
	}

	if (RowInView(rows.AnimationSpeed, layout.ScrollRegion)) {
		DrawRowLabel(drawList, m_pFonts, rows.AnimationSpeed, "Animation Speed",
					 "How fast popups, scrolling, and toggles animate.",
					 LabelRightEdge(SliderControlRect(rows.AnimationSpeed, m_pFonts), rows.AnimationSpeed, m_pFonts),
					 alpha);

		char buffer[8];
		const int written = std::snprintf(buffer, sizeof(buffer), "%.2fx", m_flAnimationSpeedDisplay);
		const std::string_view valueText{buffer, written > 0 ? static_cast<u64>(written) : 0};

		DrawSlider(drawList, body, SliderTrackRect(rows.AnimationSpeed, m_pFonts),
				   AnimationSpeedToT(m_flAnimationSpeedDisplay), valueText, accent, alpha);
	}
}

void CSettingsPanel::DrawPrivacyRows(CDrawList &drawList, const PanelLayout &layout, const Rows &rows, u8 alpha) const
{
	const Color accent = m_pSettings->m_clrAccent;

	if (RowInView(rows.ExcludeFromCapture, layout.ScrollRegion)) {
		const Rect toggle = ToggleRect(rows.ExcludeFromCapture, m_pFonts);
		DrawRowLabel(drawList, m_pFonts, rows.ExcludeFromCapture, "Hide From Screen Capture",
					 "Excludes the account list from screenshots and screen sharing.",
					 LabelRightEdge(toggle, rows.ExcludeFromCapture, m_pFonts), alpha);
		DrawToggle(drawList, toggle, m_flExcludeFromCaptureToggleAmount, accent, alpha);
	}

	if (RowInView(rows.BlockOverlayInjection, layout.ScrollRegion)) {
		const Rect toggle = ToggleRect(rows.BlockOverlayInjection, m_pFonts);
		DrawRowLabel(drawList, m_pFonts, rows.BlockOverlayInjection, "Block Overlay Injection",
					 "Keeps Discord's overlay and hook-based keyloggers out. Disables IMEs; restart to apply.",
					 LabelRightEdge(toggle, rows.BlockOverlayInjection, m_pFonts), alpha);
		DrawToggle(drawList, toggle, m_flBlockOverlayInjectionToggleAmount, accent, alpha);
	}

	if (RowInView(rows.CloseToTray, layout.ScrollRegion)) {
		const Rect toggle = ToggleRect(rows.CloseToTray, m_pFonts);
		DrawRowLabel(drawList, m_pFonts, rows.CloseToTray, "Close To Tray",
					 "Closing hides the app to the system tray instead of quitting.",
					 LabelRightEdge(toggle, rows.CloseToTray, m_pFonts), alpha);
		DrawToggle(drawList, toggle, m_flCloseToTrayToggleAmount, accent, alpha);
	}
}

void CSettingsPanel::DrawSecurityRows(CDrawList &drawList, const PanelLayout &layout, const Rows &rows, u8 alpha) const
{
	if (!RowInView(rows.MasterPassword, layout.ScrollRegion)) return;

	const Rect button = MasterPasswordButtonRect(rows.MasterPassword, m_pFonts);
	DrawRowLabel(drawList, m_pFonts, rows.MasterPassword, "Master Password", "Encrypts your saved account passwords.",
				 LabelRightEdge(button, rows.MasterPassword, m_pFonts), alpha);

	const bool hovered = RectContainsPoint(button, m_flMouseX, m_flMouseY);
	drawList.AddRectRoundedFilled(
		button.X, button.Y, button.W, button.H, CDrawList::UniformRadii(6.0f),
		ColorScaleAlpha(hovered ? ColorLighten(kColorControlBg, 10) : kColorControlBg, alpha));
	DrawCenteredText(drawList, m_pFonts->GetBody(), button.X, button.Y, button.W, button.H, "Reset Password",
					 ColorScaleAlpha(kColorText, alpha));
}

// Drawn after every row's content but still inside the clip: a button belongs to its row and
// should scroll out of view with it.
void CSettingsPanel::DrawResetButtons(CDrawList &drawList, const PanelLayout &layout, const Rows &rows, u8 alpha) const
{
	const bool pointerLive =
		!m_colorPicker.IsBlocking() && RectContainsPoint(layout.ScrollRegion, m_flMouseX, m_flMouseY);

	for (u64 i = 0; i < static_cast<u64>(ESettingsResetTarget::Count); i += 1) {
		const auto target = static_cast<ESettingsResetTarget>(i);
		if (!RowInView(ResetTargetRowRect(rows, target), layout.ScrollRegion)) continue;

		const Rect button = ResetTargetButtonRect(rows, target, m_pFonts);
		const bool hovered = pointerLive && RectContainsPoint(button, m_flMouseX, m_flMouseY);

		DrawResetButton(drawList, m_assets.Get(EAsset::IconReset), button, m_aResetAppearAmount[i],
						m_aResetSpinAmount[i], hovered, alpha);
	}
}

void CSettingsPanel::Draw(CDrawList &drawList)
{
	PULSAR_PROFILE_SCOPE("SettingsPanel.Draw");

	if (m_flOpenAmount <= 0.001f) return;

	const auto alpha = static_cast<u8>(255.0f * m_flOpenAmount);
	const PanelLayout layout = Layout();
	const Rows rows = RowsFor(layout);

	DrawChrome(drawList, layout, alpha);

	// Real GPU-side clipping, so a row scrolled halfway behind the header genuinely cannot
	// paint outside the region.
	drawList.PushClipRect(layout.ScrollRegion);

	DrawRowHoverHighlight(drawList, layout, rows, alpha);
	DrawSectionHeaders(drawList, layout, rows, alpha);
	DrawAppearanceRows(drawList, layout, rows, alpha);
	DrawMotionRows(drawList, layout, rows, alpha);
	DrawPrivacyRows(drawList, layout, rows, alpha);
	DrawSecurityRows(drawList, layout, rows, alpha);
	DrawResetButtons(drawList, layout, rows, alpha);

	drawList.PopClipRect();

	m_rowsScroll.DrawEdgeFade(drawList, layout.ScrollRegion, rows.ContentHeight, layout.ScrollRegion.H,
							  ColorScaleAlpha(kColorBg, alpha));
	m_rowsScroll.Draw(drawList, ScrollbarTrackRect(layout.ScrollRegion), rows.ContentHeight, layout.ScrollRegion.H,
					  ColorScaleAlpha(kColorScrollThumb, alpha), m_flMouseX, m_flMouseY);

	// Drawn last so it layers over every row, including the footer. It anchors to the Accent
	// row's on-screen position, so there is nothing sensible to show if that row is scrolled
	// out of view.
	if (RowInView(rows.Accent, layout.ScrollRegion)) {
		m_colorPicker.Draw(drawList);
	}

	// Last, over even the picker. Clamped to the panel rather than the window, so a bubble never
	// floats off the dialog it belongs to.
	m_tooltip.Draw(drawList, m_pFonts, layout.Panel, alpha);
}
