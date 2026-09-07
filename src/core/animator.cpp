#include "core/animator.h"

#include <cmath>

bool CAnimator::s_bEnabled = true;
float CAnimator::s_flSpeed = 1.0f;

float CAnimator::EaseToward(float value, float target, float rate, float deltaSeconds)
{
	if (!s_bEnabled) return target;

	const float t = 1.0f - std::exp(-rate * s_flSpeed * deltaSeconds);
	return value + (target - value) * t;
}
