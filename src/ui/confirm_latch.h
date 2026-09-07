#pragma once

#include "core/types.h"

/// A destructive button's two-step confirm: the first click arms it, a second click within the
/// window commits, and anything else disarms it. The armed state is what the button paints red -
/// no dialog, no extra layout, and no way to delete something with one stray click.
///
/// A latch belongs to a target, not to a button, so arming a different row silently moves the arm
/// rather than confirming the wrong thing.
class CConfirmLatch {
  public:
	/// Long enough to read the colour change and act on it, short enough that a button left armed
	/// and forgotten disarms before it can be hit by accident.
	static constexpr float kWindowSeconds = 2.5f;

	/// True when this click is the confirming one, which is the caller's cue to do the deed. A
	/// first click on any target, or a click on a different target, arms instead and returns
	/// false. `target` identifies the thing being deleted; negative values are reserved for
	/// "nothing armed".
	bool ClickArmedOrCommit(i32 target);

	void Disarm();

	/// Eased 0..1, for cross-fading a button toward its armed colour rather than snapping to it.
	float ArmedAmount(i32 target) const;

	void Update(float deltaSeconds);

	bool IsArmed(i32 target) const
	{
		return m_nTarget >= 0 && m_nTarget == target;
	}

	bool IsArmedAny() const
	{
		return m_nTarget >= 0;
	}

  private:
	i32 m_nTarget = -1;
	float m_flRemainingSeconds = 0.0f;
	float m_flArmedAmount = 0.0f;
};
