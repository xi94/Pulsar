#include "ui/scrollable.h"

#include <algorithm>

#include "core/animator.h"
#include "ui/draw_list.h"

namespace {
constexpr float kScrollableEaseRate = 16.0f;
constexpr float kEdgeFadeHeight = 28.0f;

float MaxScroll(float contentHeight, float visibleHeight)
{
	return std::max(0.0f, contentHeight - visibleHeight);
}

float ThumbHeightFor(Rect track, float contentHeight, float visibleHeight)
{
	return std::max(kScrollbarMinThumbHeight, track.H * (visibleHeight / contentHeight));
}

// Shared by dragging and drawing so the two can never drift apart.
Rect ThumbRectFor(float scrollOffset, Rect track, float contentHeight, float visibleHeight)
{
	const float maxOffset = MaxScroll(contentHeight, visibleHeight);
	const float thumbHeight = ThumbHeightFor(track, contentHeight, visibleHeight);
	const float travel = track.H - thumbHeight;
	const float t = maxOffset > 0.0f ? std::clamp(scrollOffset / maxOffset, 0.0f, 1.0f) : 0.0f;

	return Rect{track.X, track.Y + travel * t, track.W, thumbHeight};
}

// A generous grab target around a thin visual element, shared by the click hit-test and the
// hover highlight so the widget never invites a click wider than what registers one.
Rect ThumbHitRectFor(float scrollOffset, Rect track, float contentHeight, float visibleHeight)
{
	const Rect thumb = ThumbRectFor(scrollOffset, track, contentHeight, visibleHeight);

	return Rect{thumb.X - 4.0f, thumb.Y, thumb.W + 8.0f, thumb.H};
}
} // namespace

void CScrollable::Update(float deltaSeconds)
{
	m_flScrollOffset =
		CAnimator::EaseToward(m_flScrollOffset, m_flTargetScrollOffset, kScrollableEaseRate, deltaSeconds);
}

bool CScrollable::IsVisible(float contentHeight, float visibleHeight)
{
	return contentHeight > visibleHeight + 0.5f;
}

void CScrollable::Draw(CDrawList &drawList, Rect track, float contentHeight, float visibleHeight, Color thumbColor,
					   float mouseX, float mouseY) const
{
	if (!IsVisible(contentHeight, visibleHeight)) return;

	const Rect thumb = ThumbRectFor(m_flScrollOffset, track, contentHeight, visibleHeight);
	const Rect hitRect = ThumbHitRectFor(m_flScrollOffset, track, contentHeight, visibleHeight);
	const bool hovered = m_bDragging || RectContainsPoint(hitRect, mouseX, mouseY);
	const Color color = hovered ? ColorLighten(thumbColor, 40) : thumbColor;

	drawList.AddRectRoundedFilled(thumb.X, thumb.Y, thumb.W, thumb.H, CDrawList::UniformRadii(thumb.W * 0.5f), color);
}

bool CScrollable::OnPointerDown(float x, float y, Rect track, float contentHeight, float visibleHeight)
{
	if (!IsVisible(contentHeight, visibleHeight)) return false;

	if (!RectContainsPoint(ThumbHitRectFor(m_flTargetScrollOffset, track, contentHeight, visibleHeight), x, y)) {
		return false;
	}

	m_bDragging = true;
	m_flDragStartPointerY = y;
	m_flDragStartScrollOffset = m_flTargetScrollOffset;

	return true;
}

void CScrollable::OnPointerMove(float y, Rect track, float contentHeight, float visibleHeight)
{
	if (!m_bDragging) return;

	const float travel = track.H - ThumbHeightFor(track, contentHeight, visibleHeight);
	const float maxOffset = MaxScroll(contentHeight, visibleHeight);
	if (travel <= 0.0f || maxOffset <= 0.0f) return;

	const float deltaPixels = y - m_flDragStartPointerY;
	m_flTargetScrollOffset =
		std::clamp(m_flDragStartScrollOffset + deltaPixels * (maxOffset / travel), 0.0f, maxOffset);

	// 1:1 tracking while dragging, same as every other drag in this project.
	m_flScrollOffset = m_flTargetScrollOffset;
}

void CScrollable::OnPointerUp()
{
	m_bDragging = false;
}

void CScrollable::OnScroll(float wheelDelta, float contentHeight, float visibleHeight)
{
	const float maxOffset = MaxScroll(contentHeight, visibleHeight);
	m_flTargetScrollOffset =
		std::clamp(m_flTargetScrollOffset - wheelDelta * kScrollbarWheelPixelsPerNotch, 0.0f, maxOffset);
}

void CScrollable::ScrollBy(float pixels, float contentHeight, float visibleHeight)
{
	const float maxOffset = MaxScroll(contentHeight, visibleHeight);
	m_flTargetScrollOffset = std::clamp(m_flTargetScrollOffset + pixels, 0.0f, maxOffset);
}

void CScrollable::DrawEdgeFade(CDrawList &drawList, Rect area, float contentHeight, float visibleHeight,
							   Color edgeColor) const
{
	if (!IsVisible(contentHeight, visibleHeight)) return;

	const float fadeHeight = std::min(kEdgeFadeHeight, area.H * 0.5f);
	const Color clear = ColorScaleAlpha(edgeColor, 0);
	const float maxOffset = MaxScroll(contentHeight, visibleHeight);

	// The overshoot strips close a rounding gap: the caller's clip rect and this fade's
	// geometry can land on slightly different physical pixels, leaving a sliver of unfaded
	// content right at the edge. A solid strip past the edge covers it either way, and the
	// gradient's slope still starts exactly on the boundary.
	constexpr float kOvershoot = 4.0f;

	drawList.PushClipRect(area);

	if (m_flScrollOffset > 0.5f) {
		drawList.AddRectFilled(area.X, area.Y - kOvershoot, area.W, kOvershoot, edgeColor);
		drawList.AddRectGradientCorners(area.X, area.Y, area.W, fadeHeight, edgeColor, edgeColor, clear, clear);
	}

	if (m_flScrollOffset < maxOffset - 0.5f) {
		drawList.AddRectGradientCorners(area.X, area.Y + area.H - fadeHeight, area.W, fadeHeight, clear, clear,
										edgeColor, edgeColor);
		drawList.AddRectFilled(area.X, area.Y + area.H, area.W, kOvershoot, edgeColor);
	}

	drawList.PopClipRect();
}
