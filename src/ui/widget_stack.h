#pragma once

#include <memory>
#include <vector>

#include "ui/widget.h"

class CDrawList;

/// One generalized stack: push order is z-order bottom to top, and both the mouse-gating
/// cascade and input dispatch derive from that single order rather than from two hand-
/// maintained copies of it that can drift apart.
///
/// The title bar is the one deliberate exception. It must always render on top and always
/// receive input regardless of what is blocking, since minimizing or closing should not
/// require dismissing a popup first - Push's alwaysTopmost flag marks a widget as implicitly
/// above every normal entry, for both drawing and dispatch, independent of when it was pushed.
class CWidgetStack {
  public:
	/// Takes ownership and returns a non-owning pointer for the caller to keep, so it can call
	/// widget-specific methods the base does not expose. Leave alwaysTopmost false for every
	/// ordinary widget.
	CWidget *Push(std::unique_ptr<CWidget> widget, bool alwaysTopmost = false);

	/// A no-op for a widget this stack does not own.
	void Remove(CWidget *pWidget);

	/// Must be called before Update, so the gating cascade has something to gate.
	void SetRealMousePosition(float x, float y);

	/// Top-down: gates each widget's mouse position by whether anything above it is blocking,
	/// then updates it. Adding an overlay needs nothing here beyond one more Push.
	void Update(float deltaSeconds);

	/// Bottom-up, so the topmost widget paints over everything below it.
	void Draw(CDrawList &drawList);

	/// Top-down; the first widget that consumes an event stops it propagating. This is the one
	/// place "who gets this input" is decided.
	bool DispatchPointerDown(float x, float y);
	bool DispatchPointerMove(float x, float y);
	bool DispatchPointerUp(float x, float y);
	bool DispatchRightPointerUp(float x, float y);
	bool DispatchScroll(float x, float y, float wheelDelta);
	bool DispatchKeyDown(u32 keyCode);
	bool DispatchChar(u32 character);

	/// The first visible widget whose answer is not the Arrow default, in the same order and
	/// gating as dispatch. A widget below a blocking one was already gated to a hidden mouse
	/// position by Update, so its answer is already Arrow without this knowing about blocking.
	ECursorKind GetDesiredCursor() const;

	/// Public only so widget_stack.cpp's file-local iteration helper can name it; not part of
	/// this class's contract.
	struct Entry {
		std::unique_ptr<CWidget> m_pWidget;
		bool m_bAlwaysTopmost = false;
	};

  private:
	/// Visual top to bottom: always-topmost entries first, most recently pushed first, then
	/// normal entries in reverse push order. Every method walks this one order, or its reverse
	/// for drawing, so what blocks what and what paints over what can never disagree.
	std::vector<Entry> m_aWidgets;

	float m_flRealMouseX = -1.0f;
	float m_flRealMouseY = -1.0f;
};
