#pragma once

#include <memory>

#include <string_view>

#include "stb/stb_truetype.h"

class IRenderer;
class CTexture;

/// A baked glyph atlas: one GPU texture plus stb's packed metrics for ASCII 32..126. Alpha
/// coverage is expanded to RGBA8 so it samples through the same tinted-texture pipeline every
/// other textured draw uses.
///
/// DPI-aware by construction rather than by scale factors sprinkled at call sites. The atlas is
/// baked at pixelHeight * dpiScale real texels, so glyphs stay crisp, but every metric this
/// class exposes is in the same logical-pixel space as the rest of the UI - callers never need
/// to know what scale a font was baked at. GetBakeScale, GetPackedChars and GetAtlasSize are the
/// exception, for ui/text.cpp's glyph walk, which is the one place that works in baked units.
///
/// Re-baking in place is supported and used by both the font-size setting and a live DPI change:
/// LoadFromFile always bakes into a fresh texture and drops the previous one.
class CFont {
  public:
	/// Declared out-of-line rather than defaulted inline: an implicitly generated destructor,
	/// move, or even constructor-unwind path would need CTexture's complete type at every call
	/// site, not just here where it is only forward-declared.
	CFont();
	~CFont();
	CFont(CFont &&) noexcept;
	CFont &operator=(CFont &&) noexcept;

	CFont(const CFont &) = delete;
	CFont &operator=(const CFont &) = delete;

	static constexpr u32 kFirstChar = 32;
	static constexpr u32 kCharCount = 95; // ASCII 32..126

	/// pPath goes straight to fopen, so it is a real null-terminated path rather than a
	/// std::string_view. pixelHeight is the logical size every caller reasons about.
	bool LoadFromFile(IRenderer *pRenderer, const char *pPath, float pixelHeight, float dpiScale);

	float GetPixelHeight() const
	{
		return m_flPixelHeight;
	}

	float GetAscent() const
	{
		return m_flAscent;
	}

	float GetDescent() const
	{
		return m_flDescent;
	}

	float GetLineGap() const
	{
		return m_flLineGap;
	}

	/// Baseline to baseline for consecutive lines. Stacked text uses this rather than a fixed
	/// constant so it grows with the font instead of overlapping at larger sizes.
	float GetLineHeight() const
	{
		return m_flAscent - m_flDescent + m_flLineGap;
	}

	const CTexture *GetAtlas() const
	{
		return m_pAtlas.get();
	}

	const stbtt_packedchar *GetPackedChars() const
	{
		return m_aPackedChars;
	}

	u32 GetAtlasSize() const
	{
		return m_nAtlasSize;
	}

	/// Atlas texels per logical pixel.
	float GetBakeScale() const
	{
		return m_flBakeScale;
	}

  private:
	std::unique_ptr<CTexture> m_pAtlas;
	stbtt_packedchar m_aPackedChars[kCharCount]{};
	u32 m_nAtlasSize = 0;
	float m_flBakeScale = 1.0f;

	/// Everything below is logical pixels.
	float m_flPixelHeight = 0.0f;
	float m_flAscent = 0.0f;
	float m_flDescent = 0.0f;
	float m_flLineGap = 0.0f;
};
