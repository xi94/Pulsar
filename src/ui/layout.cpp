#include "ui/layout.h"

#include <algorithm>

Rect RectInset(Rect rect, float amount)
{
	return RectInset(rect, amount, amount, amount, amount);
}

Rect RectInset(Rect rect, float left, float top, float right, float bottom)
{
	return Rect{
		.X = rect.X + left,
		.Y = rect.Y + top,
		.W = std::max(0.0f, rect.W - left - right),
		.H = std::max(0.0f, rect.H - top - bottom),
	};
}

Rect RectSplitTop(Rect &rect, float amount)
{
	const float split = std::min(amount, rect.H);
	const Rect strip{.X = rect.X, .Y = rect.Y, .W = rect.W, .H = split};
	rect.Y += split;
	rect.H -= split;
	return strip;
}

Rect RectSplitBottom(Rect &rect, float amount)
{
	const float split = std::min(amount, rect.H);
	rect.H -= split;
	return Rect{.X = rect.X, .Y = rect.Y + rect.H, .W = rect.W, .H = split};
}

Rect RectSplitLeft(Rect &rect, float amount)
{
	const float split = std::min(amount, rect.W);
	const Rect strip{.X = rect.X, .Y = rect.Y, .W = split, .H = rect.H};
	rect.X += split;
	rect.W -= split;
	return strip;
}

Rect RectSplitRight(Rect &rect, float amount)
{
	const float split = std::min(amount, rect.W);
	rect.W -= split;
	return Rect{.X = rect.X + rect.W, .Y = rect.Y, .W = split, .H = rect.H};
}

Rect RectCenterIn(Rect outer, float w, float h)
{
	return Rect{.X = outer.X + (outer.W - w) * 0.5f, .Y = outer.Y + (outer.H - h) * 0.5f, .W = w, .H = h};
}

Rect RectClamp(Rect rect, LayoutConstraints constraints)
{
	const float cx = rect.X + rect.W * 0.5f;
	const float cy = rect.Y + rect.H * 0.5f;

	float w = std::max(rect.W, constraints.MinW);
	if (constraints.MaxW > 0.0f) {
		w = std::min(w, constraints.MaxW);
	}
	float h = std::max(rect.H, constraints.MinH);
	if (constraints.MaxH > 0.0f) {
		h = std::min(h, constraints.MaxH);
	}

	return Rect{.X = cx - w * 0.5f, .Y = cy - h * 0.5f, .W = w, .H = h};
}
