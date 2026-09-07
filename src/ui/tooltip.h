#pragma once

#include <string_view>
#include "core/types.h"

class CDrawList;
class CFontManager;

// A hover tooltip: a small rounded bubble pointing at whatever the cursor is resting on. A
// component rather than a CWidget - it takes no input, blocks nothing, and just draws on top
// of whatever its owner already drew.
//
// The contract is one call per frame from whichever hover branch already knows what is under
// the cursor:
//
//     if (RectContainsPoint(button, mouseX, mouseY)) {
//         m_tooltip.Request("Restore default setting.", button);
//     }
//     m_tooltip.Update(deltaSeconds);                   // no Request this frame fades it out
//     m_tooltip.Draw(drawList, pFonts, bounds, alpha);   // last, so it layers over everything
//
// Request only states what would be shown; the delay and the fade are this class's business,
// so no caller reimplements "do not flash a tooltip the instant the cursor crosses a button".
// A frame with no Request starts the fade out, so a caller can never strand a stale bubble by
// forgetting a Hide call on some exit path.
//
// Call Request from Update rather than Draw, so the fade advances the same frame the hover
// starts and Draw stays a pure paint of what Update settled on.

/// Long enough that sweeping across a row of buttons does not strobe one tooltip per button,
/// short enough that pausing on something feels answered.
constexpr float kTooltipDelaySeconds = 0.35f;

class CTooltip {
  public:
	/// anchor is the described thing's rect: the bubble centres on it and sits above,
	/// flipping below only when there is no room. Retargeting while already visible moves it
	/// without re-waiting, so sliding along a row reads as one tooltip following the cursor.
	void Request(std::string_view text, Rect anchor);

	/// Call once per frame, after every Request branch has had its chance.
	void Update(float deltaSeconds);

	/// bounds is the region the bubble must stay inside. alpha scales it on top of its own
	/// fade, so a tooltip inside a panel that is itself fading does not pop in at full opacity.
	/// Draw last: this does no clipping and layers over everything.
	void Draw(CDrawList &drawList, const CFontManager *pFonts, Rect bounds, u8 alpha = 255) const;

	/// Kills a visible bubble immediately, for a caller whose whole surface just went away and
	/// would otherwise leave it hanging over what is underneath.
	void Reset();

  private:
	/// Owns its text rather than borrowing the caller's pointer: a literal is safe today, but a
	/// formatted stack buffer would dangle by the time Draw runs, and avoiding exactly that
	/// trap is what this component is for.
	static constexpr u64 kMaxTextLength = 127;

	char m_szText[kMaxTextLength + 1]{};
	u64 m_nTextLength = 0;

	Rect m_anchor{};
	bool m_bRequestedThisFrame = false;
	float m_flHoverSeconds = 0.0f;
	float m_flVisibleAmount = 0.0f;
};
