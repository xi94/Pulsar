#pragma once

#include "core/types.h"

class CDrawList;

/// Eased hover and press amounts, composed once per clickable region a widget owns rather than
/// hand-rolled per widget - so "does this thing show hover feedback" stops being a question you
/// can only answer by reading its Draw method.
///
/// It holds no opinion about what it is attached to: Update takes the raw booleans its owner
/// already knows and eases two 0..1 amounts, which a Draw can read as a brighten, a lift, or a
/// press-scale. DrawLift is the one shared visual it owns outright, because that exact
/// five-layer glow was independently duplicated at nearly a dozen call sites.
class CHoverable {
  public:
	void Update(bool hovered, bool pressed, float deltaSeconds);

	/// A soft accent-tinted glow behind rect, for the shared "raised on hover" cue. alpha scales
	/// the whole effect, for a widget that is itself fading.
	static void DrawLift(CDrawList &drawList, Rect rect, float radius, Color accent, u8 alpha);

	float m_flHoverAmount = 0.0f;
	float m_flPressAmount = 0.0f;
};
