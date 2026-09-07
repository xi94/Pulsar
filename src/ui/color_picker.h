#pragma once

#include "core/types.h"
#include "ui/draggable.h"
#include "ui/widget.h"

/// A popup colour picker: a saturation/value square with a hue strip below it and an alpha
/// slider to its right, opened by clicking the swatch it anchors to. GetCurrentColor reads live
/// while dragging, so a caller writes it straight into settings on every move. Closing is
/// dismissal, not a confirm step - there is no cancel.
///
/// The square goes through a real per-pixel shader rather than vertex interpolation, because
/// its colour has a genuine saturation-times-value cross term that two triangles cannot
/// reproduce. The hue and alpha strips are exact as flat gradient quads, since both are
/// genuinely one-dimensional.
///
/// Open snapshots the anchor and window size to lay out against, so a resize while the popup
/// happens to be open will not re-flow it - a deliberate simplification, since nothing in this
/// widget tree has a resize notification to hook into yet.
///
/// Dragging goes through CDraggable for its press and release bookkeeping, but never reads
/// HasMoved: a plain click with no movement should still pick a colour, so the value applies on
/// press either way.
class CColorPicker : public CWidget {
  public:
	/// Seeds from `initial` and lays out right-aligned under the anchor, flipping above it if it
	/// would not fit and clamping to stay on screen either way.
	void Open(Color initial, Rect anchor, float windowW, float windowH);
	void Close();

	void Update(float deltaSeconds) override {}
	void Draw(CDrawList &drawList) override;

	/// Starts a drag if the press landed on a control, applying the picked value at once.
	/// Consumes any click inside the popup, so one that missed every control still does not fall
	/// through to what is behind it.
	bool OnPointerDown(float x, float y) override;
	bool OnPointerMove(float x, float y) override;
	bool OnPointerUp(float x, float y) override;

	bool IsBlocking() const override
	{
		return m_bOpen;
	}

	bool IsDragging() const
	{
		return m_dragSv.IsPressed() || m_dragHue.IsPressed() || m_dragAlpha.IsPressed();
	}

	ECursorKind GetDesiredCursor() const override;

	Color GetCurrentColor() const;

  private:
	Rect PopupRect() const;
	void EndAllDrags();

	Rect m_anchor{};
	float m_flWindowW = 0.0f;
	float m_flWindowH = 0.0f;
	bool m_bOpen = false;

	float m_flHue = 0.0f;		 // 0..360
	float m_flSaturation = 0.0f; // 0..1
	float m_flValue = 0.0f;		 // 0..1
	u8 m_uAlpha = 255;

	CDraggable m_dragSv;
	CDraggable m_dragHue;
	CDraggable m_dragAlpha;
};
