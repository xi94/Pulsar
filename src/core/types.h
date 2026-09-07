#pragma once

#include <algorithm>

// Screen-space geometry and color, in logical (DPI-independent) pixels with the origin at
// the top-left. Plain data with no behavior, so no C-prefix - the same treatment Source
// gives Vector and color32.

struct Rect {
	float X;
	float Y;
	float W;
	float H;
};

struct Vec2 {
	float X;
	float Y;
};

/// RGBA8, stored exactly as Vertex2D::Color wants it so no conversion happens between the
/// color a widget picks and the color the GPU rasterizes.
struct Color {
	u8 R;
	u8 G;
	u8 B;
	u8 A;
};

/// Per-corner rounding for a rounded rectangle; zero squares that corner off.
struct CornerRadii {
	float TopLeft;
	float TopRight;
	float BottomLeft;
	float BottomRight;
};

constexpr CornerRadii kCornerRadiiNone{0.0f, 0.0f, 0.0f, 0.0f};

inline CornerRadii CornerRadiiUniform(float radius)
{
	return CornerRadii{radius, radius, radius, radius};
}

inline bool RectContainsPoint(const Rect &rect, float x, float y)
{
	return x >= rect.X && x < rect.X + rect.W && y >= rect.Y && y < rect.Y + rect.H;
}

/// Empty (never negative) when the two don't overlap. CDrawList holds a single clip slot
/// rather than a stack, so nesting one clip inside another goes through this: intersect,
/// push the result, then re-push the outer rect instead of popping.
inline Rect RectIntersect(const Rect &a, const Rect &b)
{
	const float x0 = std::max(a.X, b.X);
	const float y0 = std::max(a.Y, b.Y);
	const float x1 = std::min(a.X + a.W, b.X + b.W);
	const float y1 = std::min(a.Y + a.H, b.Y + b.H);

	return Rect{x0, y0, std::max(0.0f, x1 - x0), std::max(0.0f, y1 - y0)};
}

inline Color ColorWithAlpha(Color color, u8 alpha)
{
	return Color{color.R, color.G, color.B, alpha};
}

inline Color ColorLighten(Color color, u8 amount)
{
	auto Clamp255 = [](int value) { return static_cast<u8>(value > 255 ? 255 : value); };

	return Color{Clamp255(color.R + amount), Clamp255(color.G + amount), Clamp255(color.B + amount), color.A};
}

inline Color ColorLerp(Color a, Color b, float t)
{
	auto Lerp8 = [t](u8 from, u8 to) {
		const float lerped = static_cast<float>(from) + (static_cast<float>(to) - static_cast<float>(from)) * t;
		return static_cast<u8>(lerped);
	};

	return Color{Lerp8(a.R, b.R), Lerp8(a.G, b.G), Lerp8(a.B, b.B), 255};
}

/// Perceived brightness, 0 (black) to 1 (white). Channels are linearized with a plain
/// square rather than sRGB's piecewise curve: the only consumer is the light/dark choice
/// below, and the two formulas disagree only within a hair of its crossover.
inline float ColorRelativeLuminance(Color color)
{
	const float r = static_cast<float>(color.R) / 255.0f;
	const float g = static_cast<float>(color.G) / 255.0f;
	const float b = static_cast<float>(color.B) / 255.0f;

	return 0.2126f * r * r + 0.7152f * g * g + 0.0722f * b * b;
}

/// The foreground that stays readable on `background` - every accent-filled surface draws
/// its content through this, since the accent color is the user's to choose.
///
/// The 0.22 crossover is tuned against this app's default accent rather than WCAG's 0.179:
/// the default sits at ~0.181, a hair the wrong side of the strict boundary, which flipped
/// the shipped look to black text on a purple that white reads perfectly well on.
inline Color ColorForegroundOn(Color background)
{
	constexpr Color kOnLight{18, 18, 20, 255};
	constexpr Color kOnDark{245, 245, 248, 255};

	return ColorRelativeLuminance(background) > 0.22f ? kOnLight : kOnDark;
}

/// The outline for a shape filled with `fill`: its contrasting foreground, pulled most of
/// the way back toward the fill so it separates the shape without drawing more attention
/// than the control itself.
inline Color ColorOutlineOn(Color fill)
{
	constexpr float kStrength = 0.6f;

	return ColorLerp(fill, ColorForegroundOn(fill), kStrength);
}

/// A one-shot "did a click land on something" result: a widget latches one on pointer-up
/// and a coordinating owner polls it once through a Consume* method.
///
/// The three outcomes are named rather than packed into an int's sentinel range (-2/-1/>=0),
/// which is what a past bug collapsed by turning "hit nothing specific" into "nothing
/// pending", firing every caller's != check on every frame.
enum class EPendingHitKind : u8 {
	None,
	Miss,  // landed inside the widget, but not on any item
	Index, // landed on PendingHit::Index
};

struct PendingHit {
	EPendingHitKind Kind = EPendingHitKind::None;
	i32 Index = -1;
};

inline PendingHit PendingHitMiss()
{
	return PendingHit{EPendingHitKind::Miss, -1};
}

inline PendingHit PendingHitIndex(i32 index)
{
	return PendingHit{EPendingHitKind::Index, index};
}

inline PendingHit PendingHitFromHitTest(i32 hitTestResult)
{
	return hitTestResult >= 0 ? PendingHitIndex(hitTestResult) : PendingHitMiss();
}

/// Arrow doubles as "no opinion", so CWidgetStack can keep walking down the stack without a
/// separate has-an-opinion bit.
enum class ECursorKind : u8 {
	Arrow,
	Hand,
	IBeam,
	Drag,
};
