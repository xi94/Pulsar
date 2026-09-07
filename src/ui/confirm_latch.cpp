#include "ui/confirm_latch.h"

#include "core/animator.h"

namespace {
constexpr float kArmEaseRate = 18.0f;
}

bool CConfirmLatch::ClickArmedOrCommit(i32 target)
{
	if (m_nTarget == target) {
		Disarm();
		return true;
	}

	m_nTarget = target;
	m_flRemainingSeconds = kWindowSeconds;

	return false;
}

void CConfirmLatch::Disarm()
{
	m_nTarget = -1;
	m_flRemainingSeconds = 0.0f;
}

float CConfirmLatch::ArmedAmount(i32 target) const
{
	return IsArmed(target) ? m_flArmedAmount : 0.0f;
}

// The eased amount keeps running after the latch disarms, so an expired button fades back to its
// normal colour instead of snapping.
void CConfirmLatch::Update(float deltaSeconds)
{
	if (m_nTarget >= 0) {
		m_flRemainingSeconds -= deltaSeconds;

		if (m_flRemainingSeconds <= 0.0f) {
			Disarm();
		}
	}

	m_flArmedAmount = CAnimator::EaseToward(m_flArmedAmount, m_nTarget >= 0 ? 1.0f : 0.0f, kArmEaseRate, deltaSeconds);
}
