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

	auto operator==(const Rect&) const -> bool = default;

	[[nodiscard]] auto right() const -> float
	{
		return x + w;
	}

	[[nodiscard]] auto bottom() const -> float
	{
		return y + h;
	}

	[[nodiscard]] auto center() const -> Vec2
	{
		return Vec2{x + w * 0.5f, y + h * 0.5f};
	}

	[[nodiscard]] auto contains(Vec2 t_point) const -> bool
	{
		return t_point.x >= x && t_point.x < x + w && t_point.y >= y && t_point.y < y + h;
	}

	[[nodiscard]] auto overlaps_vertically(Rect t_other) const -> bool
	{
		return bottom() > t_other.y && y < t_other.bottom();
	}

	[[nodiscard]] auto inset(float t_horizontal, float t_vertical) const -> Rect
	{
		return Rect{x + t_horizontal, y + t_vertical, std::max(0.0f, w - t_horizontal * 2.0f), std::max(0.0f, h - t_vertical * 2.0f)};
	}

	[[nodiscard]] auto inset(float t_amount) const -> Rect
	{
		return inset(t_amount, t_amount);
	}

	[[nodiscard]] auto intersect(Rect t_other) const -> Rect
	{
		const float left = std::max(x, t_other.x);
		const float top  = std::max(y, t_other.y);

		return Rect{left, top, std::max(0.0f, std::min(right(), t_other.right()) - left), std::max(0.0f, std::min(bottom(), t_other.bottom()) - top)};
	}

	[[nodiscard]] auto moved(Vec2 t_offset) const -> Rect
	{
		return Rect{x + t_offset.x, y + t_offset.y, w, h};
	}

	[[nodiscard]] auto scaled_from_center(float t_scale) const -> Rect
	{
		return inset(w * (1.0f - t_scale) * 0.5f, h * (1.0f - t_scale) * 0.5f);
	}

	[[nodiscard]] auto centered(float t_width, float t_height) const -> Rect
	{
		return Rect{x + (w - t_width) * 0.5f, y + (h - t_height) * 0.5f, t_width, t_height};
	}

	auto split_top(float t_amount) -> Rect
	{
		const float taken = std::min(t_amount, h);
		const Rect  strip{x, y, w, taken};
		y += taken;
		h -= taken;

		return strip;
	}

	auto split_bottom(float t_amount) -> Rect
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

	auto operator==(const Color&) const -> bool = default;
};

struct CornerRadii {
	float top_left;
	float top_right;
	float bottom_right;
	float bottom_left;
};

constexpr CornerRadii K_SQUARE_CORNERS{0.0f, 0.0f, 0.0f, 0.0f};

[[nodiscard]] inline auto lerp(float t_from, float t_to, float t_amount) -> float
{
	return t_from + (t_to - t_from) * t_amount;
}

[[nodiscard]] inline auto lerp(Rect t_from, Rect t_to, float t_amount) -> Rect
{
	return Rect{lerp(t_from.x, t_to.x, t_amount), lerp(t_from.y, t_to.y, t_amount), lerp(t_from.w, t_to.w, t_amount), lerp(t_from.h, t_to.h, t_amount)};
}

[[nodiscard]] inline auto to_alpha(float t_amount) -> u8
{
	return static_cast<u8>(std::clamp(t_amount, 0.0f, 1.0f) * 255.0f + 0.5f);
}

[[nodiscard]] inline auto with_alpha(Color t_color, u8 t_alpha) -> Color
{
	return Color{t_color.r, t_color.g, t_color.b, t_alpha};
}

[[nodiscard]] inline auto faded(Color t_color, u8 t_alpha) -> Color
{
	return with_alpha(t_color, static_cast<u8>(t_color.a * t_alpha / 255));
}

[[nodiscard]] inline auto lightened(Color t_color, u8 t_amount) -> Color
{
	const auto brighten = [t_amount](u8 t_channel) { return static_cast<u8>(std::min(255, t_channel + t_amount)); };

	return Color{brighten(t_color.r), brighten(t_color.g), brighten(t_color.b), t_color.a};
}

[[nodiscard]] inline auto mix(Color t_from, Color t_to, float t_amount) -> Color
{
	const auto blend = [t_amount](u8 t_start, u8 t_end) {
		return static_cast<u8>(static_cast<float>(t_start) + (static_cast<float>(t_end) - t_start) * t_amount);
	};

	return Color{blend(t_from.r, t_to.r), blend(t_from.g, t_to.g), blend(t_from.b, t_to.b), 255};
}

[[nodiscard]] inline auto luminance(Color t_color) -> float
{
	const float r = t_color.r / 255.0f;
	const float g = t_color.g / 255.0f;
	const float b = t_color.b / 255.0f;

	return 0.2126f * r * r + 0.7152f * g * g + 0.0722f * b * b;
}

[[nodiscard]] inline auto foreground_on(Color t_background) -> Color
{
	constexpr Color DARK_TEXT{18, 18, 20, 255};
	constexpr Color LIGHT_TEXT{245, 245, 248, 255};
	constexpr float DEFAULT_ACCENT_STAYS_LIGHT = 0.22f;

	return luminance(t_background) > DEFAULT_ACCENT_STAYS_LIGHT ? DARK_TEXT : LIGHT_TEXT;
}

[[nodiscard]] inline auto outline_on(Color t_fill) -> Color
{
	return mix(t_fill, foreground_on(t_fill), 0.6f);
}

enum class CursorKind : u8 {
	Arrow,
	Hand,
	IBeam,
	Drag,
	Move,
};
