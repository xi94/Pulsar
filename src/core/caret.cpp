#include "core/caret.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ranges>
#include <span>
#include <utility>

#include "core/animation.h"

namespace {
constexpr float K_CARET_TYPING_RATE     = 50.0f;
constexpr float K_CARET_JUMP_RATE       = 27.0f;
constexpr float K_TRAIL_TAIL_RATE_SHORT = 14.0f;
constexpr float K_TRAIL_TAIL_RATE_LONG  = 3.5f;
constexpr float K_TRAIL_TAIL_SPREAD     = 0.45f;
constexpr float K_TRAIL_FAINT_ALPHA_MIN = 30.0f;
constexpr float K_TRAIL_FAINT_ALPHA_MAX = 80.0f;
constexpr float K_TRAIL_MIN_LENGTH      = 0.5f;
constexpr float K_TRAIL_MAX_STEP        = 0.1f;
constexpr auto  K_HOP_WINDOW            = std::chrono::milliseconds(250);

struct TrailPoint {
	Vec2  position;
	Color color;
};

struct CaretMark {
	Vec2                                  position;
	std::chrono::steady_clock::time_point released_at;
};

CaretStyle g_caret_style    = CaretStyle::BAR;
bool       g_caret_trail    = true;
float      g_trail_strength = 0.5f;
CaretMark  g_last_caret{};

// The outline around a set of points (Andrew's monotone chain), so the trail is one shape however the caret moves.
[[nodiscard]] auto convex_hull(std::span<TrailPoint> t_points, std::span<TrailPoint> t_out) -> u32
{
	std::ranges::sort(t_points, [](const TrailPoint& t_a, const TrailPoint& t_b) { return t_a.position.x < t_b.position.x || (t_a.position.x == t_b.position.x && t_a.position.y < t_b.position.y); });

	const auto turn = [](Vec2 t_origin, Vec2 t_a, Vec2 t_b) { return (t_a.x - t_origin.x) * (t_b.y - t_origin.y) - (t_a.y - t_origin.y) * (t_b.x - t_origin.x); };

	u32        count = 0;
	const auto add   = [&](const TrailPoint& t_point, u32 t_floor) {
		while (count >= t_floor && turn(t_out[count - 2].position, t_out[count - 1].position, t_point.position) <= 0.0f) {
			count -= 1;
		}

		t_out[count] = t_point;
		count += 1;
	};

	for (const TrailPoint& point : t_points) {
		add(point, 2);
	}

	const u32 lower_end = count + 1;
	for (const TrailPoint& point : t_points.first(t_points.size() - 1) | std::views::reverse) {
		add(point, lower_end);
	}

	return count - 1;
}
}

auto set_caret_style(CaretStyle t_style, bool t_trail, float t_trail_strength) -> void
{
	g_caret_style    = t_style;
	g_caret_trail    = t_trail;
	g_trail_strength = std::clamp(t_trail_strength, 0.0f, 1.0f);
}

auto caret_style() -> CaretStyle
{
	return g_caret_style;
}

auto CaretMotion::restart() -> void
{
	m_placed = false;
}

auto CaretMotion::let_go() -> void
{
	g_last_caret.released_at = std::chrono::steady_clock::now();
}

// The caret glides to its new spot: quickly for a move the size of typing, so it keeps up with the text, and more slowly for longer jumps. Its tail follows behind, and the stretch between them is drawn as a fading ribbon, the smear kitty and Neovide leave behind their cursors. A field that just took focus starts at the caret of the field that let go of focus, so the caret flies over.
auto CaretMotion::follow(Vec2 t_target, Vec2 t_origin, float t_line_height, float t_seconds, bool t_focused) -> void
{
	const float seconds = std::min(t_seconds, K_TRAIL_MAX_STEP);

	if (!m_placed) {
		const bool hops = g_caret_trail && t_focused && std::chrono::steady_clock::now() - g_last_caret.released_at < K_HOP_WINDOW;
		m_head          = hops ? Vec2{g_last_caret.position.x - t_origin.x, g_last_caret.position.y - t_origin.y} : t_target;
		m_tail          = m_head;
		m_target        = t_target;
		m_rate          = K_CARET_JUMP_RATE;
		m_placed        = true;
	}

	if (t_target.x != m_target.x || t_target.y != m_target.y) {
		const bool typing = t_target.y == m_head.y && std::fabs(t_target.x - m_head.x) <= t_line_height;
		m_rate            = typing ? K_CARET_TYPING_RATE : K_CARET_JUMP_RATE;
		m_target          = t_target;
	}

	m_head.x = animation::ease_toward(m_head.x, t_target.x, m_rate, seconds, animation::K_SETTLED_PIXELS);
	m_head.y = animation::ease_toward(m_head.y, t_target.y, m_rate, seconds, animation::K_SETTLED_PIXELS);

	if (g_caret_trail) {
		const float rate = K_TRAIL_TAIL_RATE_SHORT * std::pow(K_TRAIL_TAIL_RATE_LONG / K_TRAIL_TAIL_RATE_SHORT, g_trail_strength);

		m_tail.x = animation::ease_toward(m_tail.x, t_target.x, rate, seconds, animation::K_SETTLED_PIXELS);
		m_tail.y = animation::ease_toward(m_tail.y, t_target.y, rate, seconds, animation::K_SETTLED_PIXELS);
	} else {
		m_tail = m_head;
	}

	if (t_focused) {
		g_last_caret.position = Vec2{t_origin.x + m_head.x, t_origin.y + m_head.y};
	}
}

auto CaretMotion::is_settled() const -> bool
{
	return m_head.x == m_target.x && m_head.y == m_target.y && m_tail.x == m_target.x && m_tail.y == m_target.y;
}

// The trail is the outline around the caret and a shrunken copy of it where the tail is: the area the caret sweeps through. The caret is that outline's front edge, so nothing pokes out of it whichever way the caret moves.
auto CaretMotion::trail(Vec2 t_origin, Vec2 t_size, Color t_color) const -> CaretTrail
{
	const Vec2 head{t_origin.x + m_head.x, t_origin.y + m_head.y};
	const Vec2 head_center{head.x + t_size.x * 0.5f, head.y + t_size.y * 0.5f};
	const Vec2 tail{t_origin.x + m_tail.x + t_size.x * 0.5f, t_origin.y + m_tail.y + t_size.y * 0.5f};

	if (std::hypot(head_center.x - tail.x, head_center.y - tail.y) <= K_TRAIL_MIN_LENGTH) return CaretTrail{};

	const auto  alpha = static_cast<u8>(K_TRAIL_FAINT_ALPHA_MIN + (K_TRAIL_FAINT_ALPHA_MAX - K_TRAIL_FAINT_ALPHA_MIN) * g_trail_strength);
	const Color faint = faded(t_color, alpha);
	const Vec2  tail_half{t_size.x * 0.5f * K_TRAIL_TAIL_SPREAD, t_size.y * 0.5f * K_TRAIL_TAIL_SPREAD};

	TrailPoint corners[K_CARET_TRAIL_CORNERS]{
		{{head.x, head.y}, t_color},
		{{head.x + t_size.x, head.y}, t_color},
		{{head.x + t_size.x, head.y + t_size.y}, t_color},
		{{head.x, head.y + t_size.y}, t_color},
		{{tail.x - tail_half.x, tail.y - tail_half.y}, faint},
		{{tail.x + tail_half.x, tail.y - tail_half.y}, faint},
		{{tail.x + tail_half.x, tail.y + tail_half.y}, faint},
		{{tail.x - tail_half.x, tail.y + tail_half.y}, faint},
	};

	TrailPoint outline[K_CARET_TRAIL_CORNERS * 2]{};
	CaretTrail shape;
	shape.count = convex_hull(corners, outline);

	for (const u32 i : std::views::iota(0u, shape.count)) {
		shape.points[i] = outline[i].position;
		shape.colors[i] = outline[i].color;
	}

	return shape;
}
