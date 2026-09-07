#include "ui/text.h"

#include <algorithm>
#include <cmath>

#include "gfx/font.h"
#include "ui/draw_list.h"

#include "stb/stb_truetype.h"

namespace {
constexpr std::string_view kEllipsis{"...", 3};

bool IsDrawableGlyph(unsigned char c)
{
	return c >= CFont::kFirstChar && c < CFont::kFirstChar + CFont::kCharCount;
}

// stb accumulates the pen position and emits quads in the atlas's baked units, not the logical
// space the rest of this project draws in.
stbtt_aligned_quad NextGlyphQuad(const CFont &font, unsigned char c, float &penX, float &penY)
{
	const auto atlasSize = static_cast<int>(font.GetAtlasSize());

	stbtt_aligned_quad quad;
	stbtt_GetPackedQuad(font.GetPackedChars(), atlasSize, atlasSize, static_cast<int>(c - CFont::kFirstChar), &penX,
						&penY, &quad, 1);

	return quad;
}
} // namespace

float TextWidth(const CFont &font, std::string_view text)
{
	float penX = 0.0f;
	float penY = 0.0f;

	for (u64 i = 0; i < text.size(); i += 1) {
		const auto c = static_cast<unsigned char>(text.data()[i]);
		if (IsDrawableGlyph(c)) {
			NextGlyphQuad(font, c, penX, penY);
		}
	}

	// Divided back to logical pixels once, at the end, rather than per glyph.
	return penX / font.GetBakeScale();
}

void DrawText(CDrawList &list, const CFont &font, float x, float y, std::string_view text, Color color)
{
	if (font.GetAtlas() == nullptr) return;

	// Snapped to whole logical pixels: the atlas is a baked bitmap sampled bilinearly, so a
	// sub-pixel baseline shift changes which texels land under each glyph edge. EaseToward
	// approaches its target asymptotically, so without this the glyph edges keep reflowing at
	// the tail of every animation with nothing else moving to mask it.
	x = std::round(x);
	y = std::round(y);

	// Positions convert in and back out; UVs do not, since they are already atlas-relative.
	const float bakeScale = font.GetBakeScale();
	const float invScale = 1.0f / bakeScale;
	float penX = x * bakeScale;
	float penY = y * bakeScale;

	for (u64 i = 0; i < text.size(); i += 1) {
		const auto c = static_cast<unsigned char>(text.data()[i]);
		if (!IsDrawableGlyph(c)) continue;

		const stbtt_aligned_quad quad = NextGlyphQuad(font, c, penX, penY);

		list.AddRectTexturedUv(quad.x0 * invScale, quad.y0 * invScale, (quad.x1 - quad.x0) * invScale,
							   (quad.y1 - quad.y0) * invScale, quad.s0, quad.t0, quad.s1, quad.t1, font.GetAtlas(),
							   color);
	}
}

void DrawCenteredText(CDrawList &list, const CFont &font, float x, float y, float w, float h, std::string_view text,
					  Color color)
{
	const float textW = TextWidth(font, text);

	// Centres the glyphs' visual middle, not the ascent: the descent is negative, and ignoring
	// how far descenders hang below the baseline sits every label a couple of pixels low.
	const float baselineY = y + h * 0.5f + (font.GetAscent() + font.GetDescent()) * 0.5f;

	DrawText(list, font, x + (w - textW) * 0.5f, baselineY, text, color);
}

void DrawTextEllipsized(CDrawList &list, const CFont &font, float x, float y, std::string_view text, float maxWidth,
						Color color)
{
	if (maxWidth <= 0.0f) return;

	if (TextWidth(font, text) <= maxWidth) {
		DrawText(list, font, x, y, text, color);
		return;
	}

	// A lone ellipsis in a sliver of space says less than nothing does, and can still overhang
	// whatever is beside it.
	const float ellipsisWidth = TextWidth(font, kEllipsis);
	if (ellipsisWidth > maxWidth) return;

	// The longest prefix that still leaves room for the ellipsis. Linear from the front rather
	// than a binary search: these are short labels, and walking forward stays correct even if
	// TextWidth ever stops being a plain sum of advances.
	u64 fit = 0;
	while (fit < text.size() && TextWidth(font, std::string_view{text.data(), fit + 1}) + ellipsisWidth <= maxWidth) {
		fit += 1;
	}

	const std::string_view head{text.data(), fit};
	DrawText(list, font, x, y, head, color);
	DrawText(list, font, x + TextWidth(font, head), y, kEllipsis, color);
}

namespace {
// The hard-break half of WrapText, kept separate so a writer's explicit newlines stay decoupled
// from the soft, width-driven wrapping below.
u32 SplitLines(std::string_view text, std::string_view *pOutLines, u32 maxLines)
{
	u32 count = 0;
	u64 lineStart = 0;

	for (u64 i = 0; i <= text.size() && count < maxLines; i += 1) {
		if (i == text.size() || text.data()[i] == '\n') {
			pOutLines[count] = std::string_view{text.data() + lineStart, i - lineStart};
			count += 1;
			lineStart = i + 1;
		}
	}

	return count;
}

// Extends the current line word by word for as long as it still fits.
u32 WrapParagraph(const CFont &font, std::string_view paragraph, float maxWidth, std::string_view *pOutLines,
				  u32 maxLines)
{
	if (paragraph.empty()) {
		if (maxLines == 0) return 0;

		pOutLines[0] = paragraph;

		return 1;
	}

	u32 lineCount = 0;
	u64 lineStart = 0;

	while (lineStart < paragraph.size() && lineCount < maxLines) {
		u64 lineEnd = lineStart;
		u64 scan = lineStart;

		for (;;) {
			while (scan < paragraph.size() && paragraph.data()[scan] == ' ') {
				scan += 1;
			}

			const u64 wordStart = scan;
			while (scan < paragraph.size() && paragraph.data()[scan] != ' ') {
				scan += 1;
			}

			if (wordStart == scan) {
				break; // trailing spaces consumed the rest of the paragraph
			}

			const std::string_view candidate{paragraph.data() + lineStart, scan - lineStart};
			if (TextWidth(font, candidate) > maxWidth && lineEnd > lineStart) {
				scan = wordStart; // this word does not fit; leave it for the next line
				break;
			}

			lineEnd = scan;
		}

		if (lineEnd == lineStart) {
			// One word is already wider than maxWidth on its own - take it whole, so this loop
			// always makes forward progress instead of spinning on it.
			u64 wordEnd = lineStart;
			while (wordEnd < paragraph.size() && paragraph.data()[wordEnd] != ' ') {
				wordEnd += 1;
			}

			lineEnd = wordEnd > lineStart ? wordEnd : lineStart + 1;
		}

		pOutLines[lineCount] = std::string_view{paragraph.data() + lineStart, lineEnd - lineStart};
		lineCount += 1;

		lineStart = lineEnd;
		while (lineStart < paragraph.size() && paragraph.data()[lineStart] == ' ') {
			lineStart += 1;
		}
	}

	return lineCount;
}
} // namespace

u32 WrapText(const CFont &font, std::string_view text, float maxWidth, std::string_view *pOutLines, u32 maxLines)
{
	constexpr u32 kMaxParagraphs = 16;

	std::string_view paragraphs[kMaxParagraphs];
	const u32 paragraphCount = SplitLines(text, paragraphs, kMaxParagraphs);

	u32 lineCount = 0;
	for (u32 i = 0; i < paragraphCount && lineCount < maxLines; i += 1) {
		lineCount += WrapParagraph(font, paragraphs[i], maxWidth, pOutLines + lineCount, maxLines - lineCount);
	}

	return lineCount;
}

float DrawWrappedText(CDrawList &list, const CFont &font, float x, float y, float maxWidth, std::string_view text,
					  Color color, u32 maxLines)
{
	constexpr u32 kMaxDrawnLines = 8;

	std::string_view lines[kMaxDrawnLines];
	const u32 lineCount = WrapText(font, text, maxWidth, lines, std::min(maxLines, kMaxDrawnLines));

	float cursorY = y;
	for (u32 i = 0; i < lineCount; i += 1) {
		DrawText(list, font, x, cursorY, lines[i], color);
		cursorY += font.GetLineHeight();
	}

	return cursorY;
}
