#pragma once

#include "core/types.h"

class CDrawList;

constexpr float kScrollbarWidth = 8.0f;
constexpr float kScrollbarMinThumbHeight = 24.0f;
constexpr float kScrollbarWheelPixelsPerNotch = 48.0f;

/// A reusable component rather than a base class: a widget has a CScrollable rather than is
/// one, since the carousel needs one only in two of its three view modes while the account
/// modal always needs exactly one.
///
/// It holds no opinion about what it is scrolling - every method takes the content and visible
/// heights as parameters rather than storing them.
class CScrollable {
  public:
	/// Eases the offset toward its target; call once per frame before Draw.
	void Update(float deltaSeconds);

	/// `track` is the full vertical strip the thumb travels within. Draws and hit-tests nothing
	/// when there is nothing to scroll - not a greyed-out bar, nothing at all - with IsVisible
	/// as the single source of truth so drawing and hit-testing cannot disagree.
	void Draw(CDrawList &drawList, Rect track, float contentHeight, float visibleHeight, Color thumbColor, float mouseX,
			  float mouseY) const;

	/// True, and starts a drag, only if the click landed on the thumb and there is something to
	/// scroll - so a caller can fall through to whatever is behind the track otherwise.
	bool OnPointerDown(float x, float y, Rect track, float contentHeight, float visibleHeight);
	void OnPointerMove(float y, Rect track, float contentHeight, float visibleHeight);
	void OnPointerUp();

	void OnScroll(float wheelDelta, float contentHeight, float visibleHeight);

	/// Moves the target by a pixel amount, clamped - for a caller bringing something into view
	/// rather than responding to a wheel notch.
	void ScrollBy(float pixels, float contentHeight, float visibleHeight);

	static bool IsVisible(float contentHeight, float visibleHeight);

	/// For a consumer whose own release handling needs to tell "this ended a scrollbar drag"
	/// apart from "this is a plain click behind the track" - OnPointerUp just clears the flag
	/// rather than reporting what it was.
	bool IsDragging() const
	{
		return m_bDragging;
	}

	/// Fades the area's content into `edgeColor` near its top and bottom. Content scrolled
	/// against a clip rect is otherwise cut off mid-row with no cue that there is more of it.
	/// The top only fades once something is actually scrolled out of view above, and the bottom
	/// only while there is still more below.
	void DrawEdgeFade(CDrawList &drawList, Rect area, float contentHeight, float visibleHeight, Color edgeColor) const;

	float m_flScrollOffset = 0.0f;		 // animated, pixels
	float m_flTargetScrollOffset = 0.0f; // where the offset eases toward

  private:
	bool m_bDragging = false;
	float m_flDragStartPointerY = 0.0f;
	float m_flDragStartScrollOffset = 0.0f;
};
