#pragma once

class IRenderer;

/// RAII ownership of one GPU texture: construction uploads, destruction frees. The free-function
/// pair this replaced had to be balanced by hand, and once was not - a missing free list meant
/// destroyed slots were never reused.
class CTexture {
  public:
	/// pRgbaPixels is width*height*4 bytes, row-major with no padding. The pRenderer must outlive
	/// this texture, which holds for every real use.
	CTexture(IRenderer *pRenderer, const u8 *pRgbaPixels, u32 width, u32 height);
	~CTexture();

	/// A bitwise copy would leave two owners freeing the same backend slot.
	CTexture(const CTexture &) = delete;
	CTexture &operator=(const CTexture &) = delete;

	u32 GetWidth() const
	{
		return m_nWidth;
	}

	u32 GetHeight() const
	{
		return m_nHeight;
	}

	/// Lets a caller cover-fit an image into a differently-proportioned rect instead of
	/// stretching it.
	float GetAspect() const
	{
		return m_flAspect;
	}

	/// Only the code that submits draw-list commands to the renderer should need this.
	void *GetHandle() const
	{
		return m_pHandle;
	}

  private:
	IRenderer *m_pRenderer = nullptr;
	void *m_pHandle;
	u32 m_nWidth;
	u32 m_nHeight;
	float m_flAspect;
};
