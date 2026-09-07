#pragma once

#include "core/types.h"

// The rectangle vocabulary widgets compose layouts from: split a rect into strips, inset it
// for padding, centre a fixed-size box in it, or clamp it against size constraints so a
// widget makes an explicit shrink decision instead of overflowing silently.
//
// Every widget reasons about a single Rect it was handed rather than four loose floats each
// call site keeps in sync by convention.

/// Negative amounts grow the rect.
Rect RectInset(Rect rect, float amount);
Rect RectInset(Rect rect, float left, float top, float right, float bottom);

/// Carves a strip off the named edge and returns it, shrinking `rect` in place to the
/// remainder. `amount` is clamped to the rect's extent, so a too-small area degrades to "the
/// strip takes everything" rather than producing a negative size.
Rect RectSplitTop(Rect &rect, float amount);
Rect RectSplitBottom(Rect &rect, float amount);
Rect RectSplitLeft(Rect &rect, float amount);
Rect RectSplitRight(Rect &rect, float amount);

Rect RectCenterIn(Rect outer, float w, float h);

/// 0 means no preference for the maximums.
struct LayoutConstraints {
	float MinW;
	float MinH;
	float MaxW;
	float MaxH;
};

/// Clamps the size, keeping the rect centred on its original centre point.
Rect RectClamp(Rect rect, LayoutConstraints constraints);
