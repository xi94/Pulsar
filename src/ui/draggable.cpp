#include "ui/draggable.h"

#include <cmath>

namespace {
constexpr float kDragThresholdPixels = 4.0f;
}

void CDraggable::Begin(float x, float y)
{
	m_bPressed = true;
	m_bHasMoved = false;
	m_flStartX = x;
	m_flStartY = y;
	m_flCurrentX = x;
	m_flCurrentY = y;
}

void CDraggable::Update(float x, float y)
{
	if (!m_bPressed) return;

	m_flCurrentX = x;
	m_flCurrentY = y;

	const float deltaX = m_flCurrentX - m_flStartX;
	const float deltaY = m_flCurrentY - m_flStartY;
	if (std::sqrt(deltaX * deltaX + deltaY * deltaY) > kDragThresholdPixels) {
		m_bHasMoved = true;
	}
}

void CDraggable::End()
{
	m_bPressed = false;
	m_bHasMoved = false;
}
