#include "ui/color_picker.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "ui/draw_list.h"

namespace {
constexpr float kPopupPadding = 12.0f;
constexpr float kPopupRadius = 12.0f;
constexpr float kSvSize = 180.0f;
constexpr float kStripGap = 10.0f;
constexpr float kAlphaStripWidth = 20.0f;
constexpr float kHueStripHeight = 16.0f;
constexpr float kHandleRadius = 6.0f;
constexpr float kWindowMargin = 8.0f;
constexpr float kAnchorGap = 8.0f;

constexpr Color kColorBg{30, 30, 34, 255};
constexpr Color kColorBorder{70, 70, 76, 255};
constexpr Color kColorMarker{255, 255, 255, 255};

// The hue circle in RGB space is piecewise-linear through exactly these points, so linear
// gradients between them are mathematically exact rather than an approximation.
constexpr Color kHueStops[]{
	{255, 0, 0, 255}, {255, 255, 0, 255}, {0, 255, 0, 255}, {0, 255, 255, 255},
	{0, 0, 255, 255}, {255, 0, 255, 255}, {255, 0, 0, 255},
};

constexpr u32 kHueStopCount = sizeof(kHueStops) / sizeof(kHueStops[0]);

struct Hsv {
	float Hue;
	float Saturation;
	float Value;
};

Color HsvToRgb(float hue, float saturation, float value)
{
	const float c = value * saturation;
	const float hPrime = std::fmod(hue, 360.0f) / 60.0f;
	const float x = c * (1.0f - std::fabs(std::fmod(hPrime, 2.0f) - 1.0f));

	float r = 0.0f;
	float g = 0.0f;
	float b = 0.0f;

	if (hPrime < 1.0f) {
		r = c;
		g = x;
	} else if (hPrime < 2.0f) {
		r = x;
		g = c;
	} else if (hPrime < 3.0f) {
		g = c;
		b = x;
	} else if (hPrime < 4.0f) {
		g = x;
		b = c;
	} else if (hPrime < 5.0f) {
		r = x;
		b = c;
	} else {
		r = c;
		b = x;
	}

	const float m = value - c;
	const auto toByte = [](float channel) { return static_cast<u8>(std::clamp(channel * 255.0f, 0.0f, 255.0f)); };

	return Color{toByte(r + m), toByte(g + m), toByte(b + m), 255};
}

Hsv RgbToHsv(Color color)
{
	const float r = static_cast<float>(color.R) / 255.0f;
	const float g = static_cast<float>(color.G) / 255.0f;
	const float b = static_cast<float>(color.B) / 255.0f;

	const float maxC = std::max({r, g, b});
	const float minC = std::min({r, g, b});
	const float delta = maxC - minC;

	float hue = 0.0f;
	if (delta > 0.0001f) {
		if (maxC == r) {
			hue = 60.0f * std::fmod((g - b) / delta, 6.0f);
		} else if (maxC == g) {
			hue = 60.0f * ((b - r) / delta + 2.0f);
		} else {
			hue = 60.0f * ((r - g) / delta + 4.0f);
		}
	}

	if (hue < 0.0f) {
		hue += 360.0f;
	}

	return Hsv{hue, maxC > 0.0001f ? delta / maxC : 0.0f, maxC};
}

std::pair<float, float> PopupSize()
{
	return {kPopupPadding * 2.0f + kSvSize + kStripGap + kAlphaStripWidth,
			kPopupPadding * 2.0f + kSvSize + kStripGap + kHueStripHeight};
}

Rect SvRect(Rect popup)
{
	return Rect{popup.X + kPopupPadding, popup.Y + kPopupPadding, kSvSize, kSvSize};
}

Rect HueRect(Rect popup)
{
	const Rect sv = SvRect(popup);

	return Rect{sv.X, sv.Y + sv.H + kStripGap, kSvSize, kHueStripHeight};
}

Rect AlphaRect(Rect popup)
{
	const Rect sv = SvRect(popup);

	return Rect{sv.X + sv.W + kStripGap, sv.Y, kAlphaStripWidth, kSvSize};
}

// A ring around the picked colour rather than a plain dot, which would disappear against a
// same-coloured background. The ring contrasts against the colour it points at instead of
// being a fixed white - white on the square's white corner is as invisible as no ring.
void DrawHandleRing(CDrawList &drawList, float cx, float cy, Color fill)
{
	drawList.AddRectRoundedFilled(cx - kHandleRadius, cy - kHandleRadius, kHandleRadius * 2.0f, kHandleRadius * 2.0f,
								  CDrawList::UniformRadii(kHandleRadius), ColorForegroundOn(fill));

	const float inner = kHandleRadius - 2.5f;
	drawList.AddRectRoundedFilled(cx - inner, cy - inner, inner * 2.0f, inner * 2.0f, CDrawList::UniformRadii(inner),
								  fill);
}
} // namespace

// The one geometry function every hit-test and draw shares, so the popup's position and its
// clickable area can never drift apart.
Rect CColorPicker::PopupRect() const
{
	const auto [w, h] = PopupSize();

	// Right-aligned under the swatch, flipped above if it would not fit below.
	const float x = std::clamp(m_anchor.X + m_anchor.W - w, kWindowMargin,
							   std::max(kWindowMargin, m_flWindowW - kWindowMargin - w));

	float y = m_anchor.Y + m_anchor.H + kAnchorGap;
	if (y + h > m_flWindowH - kWindowMargin) {
		y = m_anchor.Y - kAnchorGap - h;
	}

	y = std::clamp(y, kWindowMargin, std::max(kWindowMargin, m_flWindowH - kWindowMargin - h));

	return Rect{x, y, w, h};
}

void CColorPicker::EndAllDrags()
{
	m_dragSv.End();
	m_dragHue.End();
	m_dragAlpha.End();
}

void CColorPicker::Open(Color initial, Rect anchor, float windowW, float windowH)
{
	const Hsv hsv = RgbToHsv(initial);
	m_flHue = hsv.Hue;
	m_flSaturation = hsv.Saturation;
	m_flValue = hsv.Value;
	m_uAlpha = initial.A;

	m_anchor = anchor;
	m_flWindowW = windowW;
	m_flWindowH = windowH;
	m_bOpen = true;

	EndAllDrags();
}

void CColorPicker::Close()
{
	m_bOpen = false;
	EndAllDrags();
}

Color CColorPicker::GetCurrentColor() const
{
	return ColorWithAlpha(HsvToRgb(m_flHue, m_flSaturation, m_flValue), m_uAlpha);
}

bool CColorPicker::OnPointerDown(float x, float y)
{
	if (!m_bOpen) return false;

	const Rect popup = PopupRect();
	if (!RectContainsPoint(popup, x, y)) return false;

	if (RectContainsPoint(SvRect(popup), x, y)) {
		m_dragSv.Begin(x, y);
	} else if (RectContainsPoint(HueRect(popup), x, y)) {
		m_dragHue.Begin(x, y);
	} else if (RectContainsPoint(AlphaRect(popup), x, y)) {
		m_dragAlpha.Begin(x, y);
	}

	// Applying through the move path means a plain click picks a colour immediately, without
	// duplicating the clamping here.
	OnPointerMove(x, y);

	return true;
}

bool CColorPicker::OnPointerMove(float x, float y)
{
	if (!IsDragging()) return false;

	const Rect popup = PopupRect();

	if (m_dragSv.IsPressed()) {
		const Rect sv = SvRect(popup);
		m_dragSv.Update(x, y);
		m_flSaturation = std::clamp((x - sv.X) / sv.W, 0.0f, 1.0f);
		m_flValue = std::clamp(1.0f - (y - sv.Y) / sv.H, 0.0f, 1.0f);
	}

	if (m_dragHue.IsPressed()) {
		const Rect hue = HueRect(popup);
		m_dragHue.Update(x, y);
		m_flHue = std::clamp((x - hue.X) / hue.W, 0.0f, 1.0f) * 360.0f;
	}

	if (m_dragAlpha.IsPressed()) {
		const Rect alpha = AlphaRect(popup);
		m_dragAlpha.Update(x, y);
		m_uAlpha = static_cast<u8>(std::clamp(1.0f - (y - alpha.Y) / alpha.H, 0.0f, 1.0f) * 255.0f);
	}

	return true;
}

bool CColorPicker::OnPointerUp(float x, float y)
{
	const bool wasDragging = IsDragging();
	EndAllDrags();

	return wasDragging;
}

ECursorKind CColorPicker::GetDesiredCursor() const
{
	if (!m_bOpen) return ECursorKind::Arrow;

	if (IsDragging()) return ECursorKind::Drag;

	const Rect popup = PopupRect();
	const bool overControl = RectContainsPoint(SvRect(popup), m_flMouseX, m_flMouseY) ||
							 RectContainsPoint(HueRect(popup), m_flMouseX, m_flMouseY) ||
							 RectContainsPoint(AlphaRect(popup), m_flMouseX, m_flMouseY);

	return overControl ? ECursorKind::Hand : ECursorKind::Arrow;
}

void CColorPicker::Draw(CDrawList &drawList)
{
	if (!m_bOpen) return;

	const Rect popup = PopupRect();
	drawList.AddRectRoundedFilled(popup.X - 1.0f, popup.Y - 1.0f, popup.W + 2.0f, popup.H + 2.0f,
								  CDrawList::UniformRadii(kPopupRadius), kColorBorder);
	drawList.AddRectRoundedFilled(popup.X, popup.Y, popup.W, popup.H, CDrawList::UniformRadii(kPopupRadius - 1.0f),
								  kColorBg);

	const Color picked = HsvToRgb(m_flHue, m_flSaturation, m_flValue);

	const Rect sv = SvRect(popup);
	drawList.AddRectColorPickerSv(sv.X, sv.Y, sv.W, sv.H, m_flHue);
	DrawHandleRing(drawList, sv.X + m_flSaturation * sv.W, sv.Y + (1.0f - m_flValue) * sv.H, picked);

	const Rect hue = HueRect(popup);
	const float hueSegmentWidth = hue.W / static_cast<float>(kHueStopCount - 1);
	for (u32 i = 0; i + 1 < kHueStopCount; i += 1) {
		drawList.AddRectGradientCorners(hue.X + static_cast<float>(i) * hueSegmentWidth, hue.Y, hueSegmentWidth, hue.H,
										kHueStops[i], kHueStops[i + 1], kHueStops[i], kHueStops[i + 1]);
	}

	const float hueMarkerX = hue.X + (m_flHue / 360.0f) * hue.W;
	drawList.AddRectFilled(hueMarkerX - 1.5f, hue.Y - 2.0f, 3.0f, hue.H + 4.0f, kColorMarker);

	// One exact vertical gradient; the renderer's existing blend state does the real per-pixel
	// alpha against the background beneath.
	const Rect alpha = AlphaRect(popup);
	const Color opaque = ColorWithAlpha(picked, 255);
	const Color transparent = ColorWithAlpha(picked, 0);
	drawList.AddRectGradientCorners(alpha.X, alpha.Y, alpha.W, alpha.H, opaque, opaque, transparent, transparent);

	const float alphaMarkerY = alpha.Y + (1.0f - static_cast<float>(m_uAlpha) / 255.0f) * alpha.H;
	drawList.AddRectFilled(alpha.X - 2.0f, alphaMarkerY - 1.5f, alpha.W + 4.0f, 3.0f, kColorMarker);
}
