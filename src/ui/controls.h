#pragma once

#include <string_view>

#include "core/types.h"
#include "gfx/asset_manager.h"

class CDrawList;
class CFont;
class CTexture;

// The small drawn controls every panel is built from: buttons, field chrome, hover badges and the
// two glyphs with no embedded icon. Extracted because the account modal, the settings panel and
// the unlock screen had each grown their own copy, and the copies had already drifted - two
// different X-glyph thicknesses, two different field corner radii.
//
// These are primitives, not policy: a caller passes the colours it wants rather than a state flag
// this layer interprets. Which colour means "focused" belongs to the panel, since the panels
// genuinely disagree - the unlock screen rings a focused field in the accent, the account modal
// in a neutral border - and hiding that behind a shared bool would only move the drift.

namespace Controls {

/// A no-op if the texture failed to load, so a missing asset draws nothing rather than garbage.
void DrawIcon(CDrawList &drawList, Rect rect, const CTexture *pTexture, Color tint);

/// The same, turned about the rect's own centre. Radians, clockwise in this project's y-down
/// space, so a rising angle reads as a forward turn.
void DrawIconRotated(CDrawList &drawList, Rect rect, const CTexture *pTexture, float radians, Color tint);

/// Hand-drawn, since no embedded icon exists for a close or remove affordance. The stroke scales
/// with the rect so it does not go spindly on a large button.
void DrawXGlyph(CDrawList &drawList, Rect rect, Color color);

void DrawEyeGlyph(CDrawList &drawList, const CAssetManager &assets, Rect rect, bool revealed, Color color);

/// A circular hover backing plus a soft lift ring, so the control reads as raised.
void DrawCircularHover(CDrawList &drawList, Rect rect, Color liftColor, Color fill, u8 alpha);

/// The two-rect fill-plus-border construction every text field and chip uses: an outer rounded
/// rect in the border colour with the fill inset on top.
void DrawFieldChrome(CDrawList &drawList, Rect rect, float cornerRadius, Color border, Color fill, u8 alpha);

/// The layered soft shadow under a floating panel: a few concentric rounded rects at falling
/// alpha, offset downward. `amount` scales the whole thing, so a panel can fade its shadow in with
/// its own open animation.
void DrawPanelShadow(CDrawList &drawList, Rect panel, float cornerRadius, float amount);

/// An accent-filled primary action. Always outlined: near the light/dark crossover the outline is
/// what keeps the button's shape distinct from the panel behind it. The label colour is chosen
/// against the fill, so an accent of any brightness stays readable.
void DrawAccentButton(CDrawList &drawList, const CFont &font, Rect rect, std::string_view label, Color accent,
					  bool enabled, bool hovered, Color disabledFill, Color disabledLabel, u8 alpha);

/// The unaccented sibling, for secondary and destructive actions that supply their own colours.
void DrawNeutralButton(CDrawList &drawList, const CFont &font, Rect rect, std::string_view label, Color liftColor,
					   Color restingFill, Color hoverFill, Color labelColor, bool hovered, u8 alpha);

} // namespace Controls
