#include "ui/widget.h"

void CWidget::SetMouseGated(bool gated, float realX, float realY)
{
	if (gated) {
		m_flMouseX = -1.0f;
		m_flMouseY = -1.0f;
		m_bIsHovered = false;
		return;
	}

	m_flMouseX = realX;
	m_flMouseY = realY;
	m_bIsHovered = m_bVisible && RectContainsPoint(m_vecBounds, realX, realY);
}
