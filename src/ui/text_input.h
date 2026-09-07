#pragma once

#include <string_view>
#include "core/types.h"

class CFont;
class CDrawList;

constexpr u32 kTextInputCapacity = 128;

/// A single-line text field: a fixed buffer, a cursor, a keyboard-driven selection, clipboard
/// support, and a blinking caret. Printable ASCII only - the cursor is a byte offset with no
/// multi-byte awareness.
///
/// Shift with the arrow, Home and End keys extends a selection; typing or deleting with one
/// active replaces it. Ctrl+A selects all and Ctrl+C, X and V use the system clipboard. Ctrl+E
/// is Emacs end-of-line, chosen over Emacs's Ctrl+A because select-all is the
/// far more widely expected binding for that key here.
///
/// A plain value type its owner forwards events to, not a CWidget - it never independently
/// needs a place in the stack's z-order. Only one should be focused at a time, and the owner
/// routes events to whichever that is; m_bFocused is public for exactly that reason, since
/// there is no invariant here for a setter to protect.
class CTextInput {
  public:
	/// Resets the whole object, not just the buffer - callers use this both for first-time setup
	/// and for re-seeding a field when its form reopens.
	void Init(std::string_view initialValue);

	std::string_view GetValue() const
	{
		return std::string_view{m_szBuffer, m_nLength};
	}

	/// Programs the contents from outside - seeding from a persisted setting, say - as opposed
	/// to the user-typed path.
	void SetValue(std::string_view value);

	bool HasSelection() const
	{
		return m_nSelectionAnchor >= 0 && static_cast<u32>(m_nSelectionAnchor) != m_nCursor;
	}

	/// Ignores non-printable characters, which arrive here too but are handled by OnKey.
	void OnChar(u32 character);

	/// keyCode is a Win32 virtual-key code. Anything not listed in this class's summary is a
	/// no-op, as is everything while unfocused.
	void OnKey(u32 keyCode);

	/// Advances the blink clock regardless of focus; Draw decides whether the caret shows at
	/// all, so an unfocused field's clock just idles.
	void Update(float deltaSeconds);

	/// masked renders every character as an asterisk while still measuring and positioning
	/// correctly, since the substitution happens before measurement rather than after.
	void Draw(CDrawList &drawList, const CFont &font, float x, float y, float w, float h, Color textColor,
			  Color caretColor, bool masked) const;

	bool m_bFocused = false;

  private:
	/// The selection as an ordered half-open range, whichever direction it was extended in.
	struct Range {
		u32 Start;
		u32 End;
	};

	Range SelectionRange() const;

	void EraseRange(u32 start, u32 count);

	/// Every edit that should type over a selection calls this first.
	void DeleteSelection();

	/// Inserts at the cursor, skipping non-printable characters and stopping at capacity.
	void InsertText(std::string_view text);

	void MoveCursorTo(u32 target, bool extendSelection);

	/// True if the key was one of the control-modified bindings, handled or not.
	bool HandleControlKey(u32 keyCode);
	void PasteFromClipboard();

	char m_szBuffer[kTextInputCapacity]{};
	u32 m_nLength = 0;
	u32 m_nCursor = 0;
	float m_flCaretBlinkSeconds = 0.0f;

	/// The selection's other end as a byte offset, or -1 for none. The cursor is always the
	/// live end - the one further input moves.
	i32 m_nSelectionAnchor = -1;
};
