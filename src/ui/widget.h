#pragma once

#include "core/types.h"

class CDrawList;

/// The base every UI element derives from. A widget owns its own state, advances its own
/// animations in Update, draws itself in Draw, and answers input through the On* virtuals -
/// so no frame loop has to know which widget wants which event.
///
/// Not everything needs to be one. Something is a CWidget only if it can independently be the
/// foreground blocking thing, or needs its own place in the stack's z-order. A text field, a
/// glyph icon or a colour swatch is a plain member its owner forwards calls to.
class CWidget {
  public:
	virtual ~CWidget() = default;

	/// Called top of the stack down, before Draw, with the mouse state SetMouseGated just set.
	virtual void Update(float deltaSeconds) = 0;

	virtual void Draw(CDrawList &drawList) = 0;

	/// Each returns whether this widget consumed the event, which stops the stack offering it
	/// to anything below. The defaults do nothing, so a widget that ignores an input kind needs
	/// no boilerplate to say so.
	virtual bool OnPointerDown(float x, float y)
	{
		return false;
	}

	virtual bool OnPointerMove(float x, float y)
	{
		return false;
	}

	virtual bool OnPointerUp(float x, float y)
	{
		return false;
	}

	/// Right-click-down is not tracked separately, so this is the only right-click entry point.
	/// A widget that wants a context menu usually cannot do the whole job itself - the shape is
	/// to hit-test here, latch the result as one-shot consumable state, and let a coordinating
	/// owner poll it and decide what to open.
	virtual bool OnRightPointerUp(float x, float y)
	{
		return false;
	}

	virtual bool OnScroll(float x, float y, float wheelDelta)
	{
		return false;
	}

	virtual bool OnKeyDown(u32 keyCode)
	{
		return false;
	}

	virtual bool OnChar(u32 character)
	{
		return false;
	}

	/// Does this widget want exclusive input right now - an open modal or popup? The stack uses
	/// this to decide whether widgets below see the real mouse position or a gated-away one.
	virtual bool IsBlocking() const
	{
		return false;
	}

	/// What cursor this widget's current hover or drag state calls for. The stack asks every
	/// widget in the same gated top-down order dispatch uses and takes the first answer that
	/// is not the default, so anything with no click affordance needs no override.
	virtual ECursorKind GetDesiredCursor() const
	{
		return ECursorKind::Arrow;
	}

	/// Called once per frame before Update, with either the real cursor position or a gated-away
	/// one. Keeps m_bIsHovered current so no subclass has to hit-test its own bounds by hand.
	void SetMouseGated(bool gated, float realX, float realY);

	Rect m_vecBounds{};
	bool m_bVisible = true;

  protected:
	bool m_bIsHovered = false;
	float m_flMouseX = -1.0f;
	float m_flMouseY = -1.0f;
};
