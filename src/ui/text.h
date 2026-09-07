#pragma once

#include <string_view>
#include "core/types.h"

class CFont;
class CDrawList;

// Text drawing from a baked atlas, kept separate from CDrawList so that class stays
// font-agnostic - it only knows about the generic explicit-UV textured quad these build on.

/// Logical-pixel width, regardless of what DPI scale the font was baked at.
float TextWidth(const CFont &font, std::string_view text);

/// (x, y) is the baseline start, not the top-left, matching stb's convention - so glyphs with
/// descenders sit correctly relative to the line.
void DrawText(CDrawList &list, const CFont &font, float x, float y, std::string_view text, Color color);

/// Centres text within the box - shared rather than reimplemented per widget, which is how the
/// original's modal and settings panel drifted out of sync.
void DrawCenteredText(CDrawList &list, const CFont &font, float x, float y, float w, float h, std::string_view text,
					  Color color);

/// DrawText, but never wider than maxWidth: what does not fit is cut and given an ellipsis.
/// This project's panels lay out in real glyph metrics, so a label that fits comfortably at the
/// default size can run straight through the control to its right once the user raises it, and
/// silently painting over a control is worse than an honest cut. A clip rect is not an option:
/// CDrawList holds a single active rect, so a per-label clip inside a scroll region would
/// clobber the region's.
void DrawTextEllipsized(CDrawList &list, const CFont &font, float x, float y, std::string_view text, float maxWidth,
						Color color);

/// Greedy word wrap into caller-provided line views, splitting on literal newlines first so a
/// writer's hard breaks always survive, then wrapping each paragraph to maxWidth. A single
/// word wider than maxWidth is taken whole rather than broken mid-word. Returns the line count,
/// capped at maxLines.
u32 WrapText(const CFont &font, std::string_view text, float maxWidth, std::string_view *pOutLines, u32 maxLines);

/// Wraps and draws in one call, for short unscrolled labels. Returns the y just past the last
/// line, so a caller can stack something beneath it. maxLines is clamped to a small internal
/// cap; anything longer wants its own scroll region rather than this.
float DrawWrappedText(CDrawList &list, const CFont &font, float x, float y, float maxWidth, std::string_view text,
					  Color color, u32 maxLines);
