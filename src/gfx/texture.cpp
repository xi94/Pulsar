#include "gfx/texture.h"

#include "gfx/renderer.h"

CTexture::CTexture(IRenderer *pRenderer, const u8 *pRgbaPixels, u32 width, u32 height)
	: m_pRenderer(pRenderer)
	, m_nWidth(width)
	, m_nHeight(height)
	, m_flAspect(height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0f)
{
	m_pHandle = m_pRenderer->CreateTexture(pRgbaPixels, width, height);
}

CTexture::~CTexture()
{
	if (m_pHandle != nullptr) {
		m_pRenderer->DestroyTexture(m_pHandle);
	}
}
