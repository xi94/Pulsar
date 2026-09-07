#include "ui/controls.h"

#include <algorithm>

#include "gfx/font.h"
#include "ui/draw_list.h"
#include "ui/hoverable.h"
#include "ui/text.h"

namespace {
constexpr float kButtonCornerRadius = 8.0f;

/// Proportional to the rect rather than fixed, so the same glyph reads correctly on a 16px row
/// badge and on a 32px close button.
constexpr float kGlyphArmRatio = 0.24f;
constexpr float kGlyphThicknessRatio = 0.09f;
constexpr float kGlyphMinThickness = 2.0f;
} // namespace

void Controls::DrawIcon(CDrawList &drawList, Rect rect, const CTexture *pTexture, Color tint)
{
	if (pTexture != nullptr) {
		drawList.AddRectRoundedTextured(rect.X, rect.Y, rect.W, rect.H, kCornerRadiiNone, pTexture, tint);
	}
}

void Controls::DrawIconRotated(CDrawList &drawList, Rect rect, const CTexture *pTexture, float radians, Color tint)
{
	if (pTexture == nullptr) return;

	drawList.AddRectTexturedRotated(rect.X, rect.Y, rect.W, rect.H, radians, pTexture, tint);
}

void Controls::DrawXGlyph(CDrawList &drawList, Rect rect, Color color)
{
	const float cx = rect.X + rect.W * 0.5f;
	const float cy = rect.Y + rect.H * 0.5f;
	const float arm = std::min(rect.W, rect.H) * kGlyphArmRatio;
	const float thickness = std::max(kGlyphMinThickness, rect.H * kGlyphThicknessRatio);

	drawList.AddLine(cx - arm, cy - arm, cx + arm, cy + arm, thickness, color);
	drawList.AddLine(cx - arm, cy + arm, cx + arm, cy - arm, thickness, color);
}

void Controls::DrawEyeGlyph(CDrawList &drawList, const CAssetManager &assets, Rect rect, bool revealed, Color color)
{
	DrawIcon(drawList, rect, assets.Get(revealed ? EAsset::IconEyeVisible : EAsset::IconEyeHidden), color);
}

void Controls::DrawCircularHover(CDrawList &drawList, Rect rect, Color liftColor, Color fill, u8 alpha)
{
	CHoverable::DrawLift(drawList, rect, rect.W * 0.5f, liftColor, alpha);
	drawList.AddRectRoundedFilled(rect.X, rect.Y, rect.W, rect.H, CDrawList::UniformRadii(rect.W * 0.5f),
								  ColorScaleAlpha(fill, alpha));
}

void Controls::DrawFieldChrome(CDrawList &drawList, Rect rect, float cornerRadius, Color border, Color fill, u8 alpha)
{
	// The border is a slightly larger rounded rect behind the fill rather than a stroke, so the
	// inner radius has to shrink by the same inset or the corners read as two nested shapes.
	constexpr float kBorderThickness = 1.5f;

	drawList.AddRectRoundedFilled(rect.X, rect.Y, rect.W, rect.H, CDrawList::UniformRadii(cornerRadius),
								  ColorScaleAlpha(border, alpha));
	drawList.AddRectRoundedFilled(rect.X + kBorderThickness, rect.Y + kBorderThickness,
								  rect.W - kBorderThickness * 2.0f, rect.H - kBorderThickness * 2.0f,
								  CDrawList::UniformRadii(cornerRadius - kBorderThickness),
								  ColorScaleAlpha(fill, alpha));
}

void Controls::DrawPanelShadow(CDrawList &drawList, Rect panel, float cornerRadius, float amount)
{
	constexpr int kLayers = 4;
	constexpr float kMaxExpand = 20.0f;
	constexpr float kYOffset = 10.0f;
	constexpr float kLayerAlpha = 18.0f;

	for (int i = kLayers; i >= 1; i -= 1) {
		const float t = static_cast<float>(i) / static_cast<float>(kLayers);
		const float expand = kMaxExpand * t;
		const auto shadowAlpha = static_cast<u8>(kLayerAlpha * t * amount);

		drawList.AddRectRoundedFilled(panel.X - expand, panel.Y - expand + kYOffset, panel.W + expand * 2.0f,
									  panel.H + expand * 2.0f, CDrawList::UniformRadii(cornerRadius + expand * 0.4f),
									  Color{0, 0, 0, shadowAlpha});
	}
}

void Controls::DrawAccentButton(CDrawList &drawList, const CFont &font, Rect rect, std::string_view label, Color accent,
								bool enabled, bool hovered, Color disabledFill, Color disabledLabel, u8 alpha)
{
	const Color fill = !enabled ? disabledFill : (hovered ? ColorLighten(accent, 20) : accent);

	if (hovered) {
		CHoverable::DrawLift(drawList, rect, kButtonCornerRadius, accent, alpha);
	}

	drawList.AddRectRoundedBordered(rect.X, rect.Y, rect.W, rect.H, CDrawList::UniformRadii(kButtonCornerRadius),
									ColorScaleAlpha(fill, alpha), ColorScaleAlpha(ColorOutlineOn(fill), alpha), 1.0f);
	DrawCenteredText(drawList, font, rect.X, rect.Y, rect.W, rect.H, label,
					 ColorScaleAlpha(enabled ? ColorForegroundOn(fill) : disabledLabel, alpha));
}

void Controls::DrawNeutralButton(CDrawList &drawList, const CFont &font, Rect rect, std::string_view label,
								 Color liftColor, Color restingFill, Color hoverFill, Color labelColor, bool hovered,
								 u8 alpha)
{
	if (hovered) {
		CHoverable::DrawLift(drawList, rect, kButtonCornerRadius, liftColor, alpha);
	}

	drawList.AddRectRoundedFilled(rect.X, rect.Y, rect.W, rect.H, CDrawList::UniformRadii(kButtonCornerRadius),
								  ColorScaleAlpha(hovered ? hoverFill : restingFill, alpha));
	DrawCenteredText(drawList, font, rect.X, rect.Y, rect.W, rect.H, label, ColorScaleAlpha(labelColor, alpha));
}
