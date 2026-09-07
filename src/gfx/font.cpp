#include "gfx/font.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <print>
#include <vector>

#include "gfx/texture.h"

namespace {
// stb's raw coverage reads thin and dim for a light face at UI sizes. A mild gamma boost
// darkens partially-covered edge pixels toward fully-covered, closer to how the OS's text
// rendering reads, without touching glyph geometry.
constexpr float kAlphaGamma = 0.8f;

// A high font size on a high-DPI monitor can bake well over 100 texels per glyph, which does
// not fit 512x512. Coarse steps rather than an exact fit: retrying a failed pack is more
// complexity than a slightly oversized atlas costs in VRAM.
u32 AtlasSizeFor(float bakedPixelHeight)
{
	if (bakedPixelHeight <= 24.0f) return 512;

	if (bakedPixelHeight <= 48.0f) return 1024;

	return 2048;
}

bool ReadWholeFile(const char *pPath, std::vector<unsigned char> &outBytes)
{
	FILE *pFile = nullptr;
	if (fopen_s(&pFile, pPath, "rb") != 0 || pFile == nullptr) return false;

	std::fseek(pFile, 0, SEEK_END);
	const long fileSize = std::ftell(pFile);
	std::fseek(pFile, 0, SEEK_SET);

	if (fileSize <= 0) {
		std::fclose(pFile);
		return false;
	}

	outBytes.resize(static_cast<usize>(fileSize));
	const usize readCount = std::fread(outBytes.data(), 1, outBytes.size(), pFile);
	std::fclose(pFile);

	return readCount == outBytes.size();
}

std::vector<unsigned char> ExpandCoverageToRgba(const std::vector<unsigned char> &alpha)
{
	std::vector<unsigned char> rgba(alpha.size() * 4);

	for (usize i = 0; i < alpha.size(); i += 1) {
		const float coverage = static_cast<float>(alpha[i]) / 255.0f;
		const float boosted = std::pow(coverage, kAlphaGamma) * 255.0f;

		rgba[i * 4 + 0] = 255;
		rgba[i * 4 + 1] = 255;
		rgba[i * 4 + 2] = 255;
		rgba[i * 4 + 3] = static_cast<unsigned char>(std::min(255.0f, boosted));
	}

	return rgba;
}
} // namespace

// Defined here, where CTexture is complete - see the declarations in font.h.
CFont::CFont() = default;
CFont::~CFont() = default;
CFont::CFont(CFont &&) noexcept = default;
CFont &CFont::operator=(CFont &&) noexcept = default;

bool CFont::LoadFromFile(IRenderer *pRenderer, const char *pPath, float pixelHeight, float dpiScale)
{
	std::vector<unsigned char> fontData;
	if (!ReadWholeFile(pPath, fontData)) {
		std::println("Failed to read font file: {}", pPath);
		return false;
	}

	const float bakedPixelHeight = pixelHeight * dpiScale;
	const u32 atlasSize = AtlasSizeFor(bakedPixelHeight);

	std::vector<unsigned char> alphaPixels(static_cast<usize>(atlasSize) * atlasSize);

	stbtt_pack_context packContext;
	stbtt_PackBegin(&packContext, alphaPixels.data(), static_cast<int>(atlasSize), static_cast<int>(atlasSize), 0, 1,
					nullptr);

	// No oversampling. 2x2 rasterizes each glyph at double size and box-filters it back down,
	// which is a real low-pass blur - fine for text reused at many subpixel offsets, but at a
	// thin face's ~1.5px stroke it smears the stroke's core into a soft gradient with no sharp
	// centre left. This atlas is always sampled 1:1, so plain per-pixel coverage is what crisp
	// UI text actually wants.
	stbtt_PackSetOversampling(&packContext, 1, 1);

	const int packedOk =
		stbtt_PackFontRange(&packContext, fontData.data(), 0, bakedPixelHeight, static_cast<int>(kFirstChar),
							static_cast<int>(kCharCount), m_aPackedChars);
	stbtt_PackEnd(&packContext);

	if (!packedOk) {
		std::println("Failed to pack glyph atlas for font: {}", pPath);
		return false;
	}

	const std::vector<unsigned char> rgbaPixels = ExpandCoverageToRgba(alphaPixels);
	m_pAtlas = std::make_unique<CTexture>(pRenderer, rgbaPixels.data(), atlasSize, atlasSize);

	stbtt_fontinfo fontInfo;
	stbtt_InitFont(&fontInfo, fontData.data(), 0);

	int ascent = 0;
	int descent = 0;
	int lineGap = 0;
	stbtt_GetFontVMetrics(&fontInfo, &ascent, &descent, &lineGap);

	// Metrics come back in baked units; dividing by the scale puts them back in the logical
	// space every caller reasons about.
	const float scale = stbtt_ScaleForPixelHeight(&fontInfo, bakedPixelHeight) / dpiScale;

	m_nAtlasSize = atlasSize;
	m_flPixelHeight = pixelHeight;
	m_flBakeScale = dpiScale;
	m_flAscent = static_cast<float>(ascent) * scale;
	m_flDescent = static_cast<float>(descent) * scale;
	m_flLineGap = static_cast<float>(lineGap) * scale;

	return m_pAtlas->GetHandle() != nullptr;
}
