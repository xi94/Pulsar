#pragma once

#include <algorithm>

struct Vec2 {
	float x;
	float y;
};

struct Rect {
	float x;
	float y;
	float w;
	float h;

	bool operator==(const Rect &) const = default;

	float right() const
	{
		return x + w;
	}

	float bottom() const
	{
		return y + h;
	}

	Vec2 center() const
	{
		return Vec2{x + w * 0.5f, y + h * 0.5f};
	}

	bool contains(Vec2 t_point) const
	{
		return t_point.x >= x && t_point.x < x + w && t_point.y >= y && t_point.y < y + h;
	}

	bool overlaps_vertically(Rect t_other) const
	{
		return bottom() > t_other.y && y < t_other.bottom();
	}

	Rect inset(float t_horizontal, float t_vertical) const
	{
		return Rect{x + t_horizontal, y + t_vertical, std::max(0.0f, w - t_horizontal * 2.0f),
					std::max(0.0f, h - t_vertical * 2.0f)};
	}

	Rect inset(float t_amount) const
	{
		return inset(t_amount, t_amount);
	}

	Rect intersect(Rect t_other) const
	{
		const float left = std::max(x, t_other.x);
		const float top = std::max(y, t_other.y);

		return Rect{left, top, std::max(0.0f, std::min(right(), t_other.right()) - left),
					std::max(0.0f, std::min(bottom(), t_other.bottom()) - top)};
	}

	Rect centered(float t_width, float t_height) const
	{
		return Rect{x + (w - t_width) * 0.5f, y + (h - t_height) * 0.5f, t_width, t_height};
	}

	Rect split_top(float t_amount)
	{
		const float taken = std::min(t_amount, h);
		const Rect strip{x, y, w, taken};
		y += taken;
		h -= taken;

		return strip;
	}

	Rect split_bottom(float t_amount)
	{
		const float taken = std::min(t_amount, h);
		h -= taken;

		return Rect{x, y + h, w, taken};
	}
};

struct Color {
	u8 r;
	u8 g;
	u8 b;
	u8 a;

	bool operator==(const Color &) const = default;
};

struct CornerRadii {
	float top_left;
	float top_right;
	float bottom_right;
	float bottom_left;
};

constexpr CornerRadii square_corners{0.0f, 0.0f, 0.0f, 0.0f};

inline Color with_alpha(Color t_color, u8 t_alpha)
{
	return Color{t_color.r, t_color.g, t_color.b, t_alpha};
}

inline Color faded(Color t_color, u8 t_alpha)
{
	return with_alpha(t_color, static_cast<u8>(t_color.a * t_alpha / 255));
}

inline Color lightened(Color t_color, u8 t_amount)
{
	const auto brighten = [t_amount](u8 t_channel) { return static_cast<u8>(std::min(255, t_channel + t_amount)); };

	return Color{brighten(t_color.r), brighten(t_color.g), brighten(t_color.b), t_color.a};
}

inline Color mix(Color t_from, Color t_to, float t_amount)
{
	const auto blend = [t_amount](u8 t_start, u8 t_end) {
		return static_cast<u8>(static_cast<float>(t_start) + (static_cast<float>(t_end) - t_start) * t_amount);
	};

	return Color{blend(t_from.r, t_to.r), blend(t_from.g, t_to.g), blend(t_from.b, t_to.b), 255};
}

inline float luminance(Color t_color)
{
	const float r = t_color.r / 255.0f;
	const float g = t_color.g / 255.0f;
	const float b = t_color.b / 255.0f;

	return 0.2126f * r * r + 0.7152f * g * g + 0.0722f * b * b;
}

inline Color foreground_on(Color t_background)
{
	constexpr Color dark_text{18, 18, 20, 255};
	constexpr Color light_text{245, 245, 248, 255};
	constexpr float default_accent_stays_light = 0.22f;

	return luminance(t_background) > default_accent_stays_light ? dark_text : light_text;
}

inline Color outline_on(Color t_fill)
{
	return mix(t_fill, foreground_on(t_fill), 0.6f);
}

enum class CursorKind : u8 {
	arrow,
	hand,
	ibeam,
	drag,
	move,
};
