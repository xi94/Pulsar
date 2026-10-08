#include "gfx/draw_list.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <numbers>

#include "core/profiler.h"

namespace {
constexpr u32   K_CORNER_SEGMENTS     = 14;
constexpr u32   K_POINTS_PER_CORNER   = K_CORNER_SEGMENTS + 1;
constexpr u32   K_ROUNDED_POINT_COUNT = K_POINTS_PER_CORNER * 4;
constexpr float K_DEGREES_TO_RADIANS  = std::numbers::pi_v<float> / 180.0f;

float g_user_roundness   = 1.0f;
float g_corner_roundness = 1.0f;
float g_pixel_scale      = 1.0f;

[[nodiscard]] auto pack(Color t_color) -> u32
{
	return static_cast<u32>(t_color.r) | (static_cast<u32>(t_color.g) << 8) | (static_cast<u32>(t_color.b) << 16) | (static_cast<u32>(t_color.a) << 24);
}

[[nodiscard]] auto corner_arc_directions() -> const std::array<Vec2, K_ROUNDED_POINT_COUNT>&
{
	static const std::array<Vec2, K_ROUNDED_POINT_COUNT> DIRECTIONS = [] {
		constexpr float CORNER_START_DEGREES[4]{180.0f, 270.0f, 0.0f, 90.0f};

		std::array<Vec2, K_ROUNDED_POINT_COUNT> arc{};
		for (u32 corner = 0; corner < 4; corner += 1) {
			for (u32 step = 0; step < K_POINTS_PER_CORNER; step += 1) {
				const float degrees = CORNER_START_DEGREES[corner] + 90.0f * step / K_CORNER_SEGMENTS;
				const float radians = degrees * K_DEGREES_TO_RADIANS;

				arc[corner * K_POINTS_PER_CORNER + step] = Vec2{std::cos(radians), std::sin(radians)};
			}
		}

		return arc;
	}();

	return DIRECTIONS;
}

[[nodiscard]] auto is_square(CornerRadii t_radii) -> bool
{
	return t_radii.top_left <= 0.0f && t_radii.top_right <= 0.0f && t_radii.bottom_right <= 0.0f && t_radii.bottom_left <= 0.0f;
}

[[nodiscard]] auto rounded_path_point(Rect t_path, float t_radius, float t_distance) -> Vec2
{
	constexpr float QUARTER_TURN = std::numbers::pi_v<float> * 0.5f;

	const float straight_width  = t_path.w - t_radius * 2.0f;
	const float straight_height = t_path.h - t_radius * 2.0f;
	const float arc_length      = QUARTER_TURN * t_radius;
	const Vec2  centers[4]{
		{t_path.right() - t_radius, t_path.y + t_radius},
		{t_path.right() - t_radius, t_path.bottom() - t_radius},
		{t_path.x + t_radius, t_path.bottom() - t_radius},
		{t_path.x + t_radius, t_path.y + t_radius},
	};
	const float edges[4]{straight_height, straight_width, straight_height, straight_width * 0.5f};

	float remaining = t_distance;
	if (remaining <= straight_width * 0.5f) return Vec2{t_path.center().x + remaining, t_path.y};

	remaining -= straight_width * 0.5f;

	for (u32 i = 0; i < 4; i += 1) {
		const float start_angle = QUARTER_TURN * (static_cast<float>(i) - 1.0f);

		if (remaining <= arc_length) {
			const float angle = start_angle + (t_radius > 0.0f ? remaining / t_radius : 0.0f);
			return Vec2{centers[i].x + std::cos(angle) * t_radius, centers[i].y + std::sin(angle) * t_radius};
		}

		remaining -= arc_length;

		const float end_angle = start_angle + QUARTER_TURN;
		const Vec2  normal{std::cos(end_angle), std::sin(end_angle)};
		const float along = std::min(remaining, edges[i]);

		if (remaining <= edges[i] || i == 3) {
			return Vec2{centers[i].x + normal.x * t_radius - normal.y * along, centers[i].y + normal.y * t_radius + normal.x * along};
		}

		remaining -= edges[i];
	}

	return Vec2{t_path.center().x, t_path.y};
}

[[nodiscard]] auto uses_rounded_box(ShaderKind t_shader) -> bool
{
	return t_shader == ShaderKind::BANNER_GLOW || t_shader == ShaderKind::SHADOW || t_shader == ShaderKind::BACKDROP_BLUR;
}

[[nodiscard]] auto uv_at(Rect t_rect, UvRect t_uv, Vec2 t_point) -> Vec2
{
	return Vec2{t_uv.u0 + (t_uv.u1 - t_uv.u0) * (t_point.x - t_rect.x) / t_rect.w, t_uv.v0 + (t_uv.v1 - t_uv.v0) * (t_point.y - t_rect.y) / t_rect.h};
}
}

[[nodiscard]] auto scaled_radius(float t_radius) -> float
{
	return t_radius * g_corner_roundness;
}

auto set_corner_roundness(float t_scale) -> void
{
	g_user_roundness   = std::max(0.0f, t_scale);
	g_corner_roundness = g_user_roundness;
}

auto user_roundness() -> float
{
	return g_user_roundness;
}

RoundnessScope::RoundnessScope(float t_roundness)
	: m_previous(g_corner_roundness)
{
	g_corner_roundness = std::max(0.0f, t_roundness);
}

RoundnessScope::~RoundnessScope()
{
	g_corner_roundness = m_previous;
}

auto set_pixel_scale(float t_scale) -> void
{
	g_pixel_scale = t_scale;
}

[[nodiscard]] auto snapped_to_pixel(float t_value) -> float
{
	return std::round(t_value * g_pixel_scale) / g_pixel_scale;
}

[[nodiscard]] auto rounded(float t_radius) -> CornerRadii
{
	const float radius = scaled_radius(t_radius);

	return CornerRadii{radius, radius, radius, radius};
}

[[nodiscard]] auto rounded(float t_top_left, float t_top_right, float t_bottom_right, float t_bottom_left) -> CornerRadii
{
	return CornerRadii{scaled_radius(t_top_left), scaled_radius(t_top_right), scaled_radius(t_bottom_right), scaled_radius(t_bottom_left)};
}

[[nodiscard]] auto cover_uv(float t_box_aspect, float t_texture_aspect) -> UvRect
{
	if (t_box_aspect <= 0.0f || t_texture_aspect <= 0.0f) return K_FULL_UV;

	if (t_texture_aspect > t_box_aspect) {
		const float cropped = (1.0f - t_box_aspect / t_texture_aspect) * 0.5f;
		return UvRect{cropped, 0.0f, 1.0f - cropped, 1.0f};
	}

	const float cropped = (1.0f - t_texture_aspect / t_box_aspect) * 0.5f;

	return UvRect{0.0f, cropped, 1.0f, 1.0f - cropped};
}

auto DrawList::init(u32 t_vertex_capacity, u32 t_index_capacity) -> void
{
	m_vertices        = std::make_unique_for_overwrite<Vertex2D[]>(t_vertex_capacity);
	m_indices         = std::make_unique_for_overwrite<u32[]>(t_index_capacity);
	m_vertex_capacity = t_vertex_capacity;
	m_index_capacity  = t_index_capacity;

	clear();
}

auto DrawList::clear() -> void
{
	m_has_animated_effects = false;
	m_vertex_count         = 0;
	m_index_count          = 0;
	m_command_count        = 0;
	m_has_open_command     = false;
	m_clip_depth           = 0;
	m_transform            = Transform{};
	m_transform_depth      = 0;
}

auto DrawList::finish() -> void
{
	PULSAR_PROFILE_SCOPE("DrawList.Finish");

	close_command();
	m_has_open_command = false;
}

auto DrawList::push_clip(Rect t_rect) -> void
{
	assert(m_clip_depth < K_MAX_CLIP_DEPTH);

	const Rect clip            = scaled(t_rect);
	m_clip_stack[m_clip_depth] = m_clip_depth > 0 ? m_clip_stack[m_clip_depth - 1].intersect(clip) : clip;
	m_clip_depth += 1;
}

auto DrawList::set_probe(Vec2 t_point) -> void
{
	m_probe             = t_point;
	m_probe_cover_count = 0;
}

auto DrawList::visible_rect(Rect t_rect) const -> Rect
{
	const Rect screen = scaled(t_rect);

	return m_clip_depth > 0 ? screen.intersect(m_clip_stack[m_clip_depth - 1]) : screen;
}

auto DrawList::note_cover(Rect t_rect, Color t_color) -> void
{
	constexpr u8 COVERING_ALPHA = 96;

	if (t_color.a >= COVERING_ALPHA && visible_rect(t_rect).contains(m_probe)) {
		m_probe_cover_count += 1;
	}
}

auto DrawList::pop_clip() -> void
{
	assert(m_clip_depth > 0);

	m_clip_depth -= 1;
}

auto DrawList::push_scale(Vec2 t_origin, float t_factor) -> void
{
	assert(m_transform_depth < K_MAX_TRANSFORM_DEPTH);

	m_transform_stack[m_transform_depth] = m_transform;
	m_transform_depth += 1;

	const float pull = (1.0f - t_factor) * m_transform.factor;
	m_transform      = Transform{
		.offset = Vec2{m_transform.offset.x + t_origin.x * pull, m_transform.offset.y + t_origin.y * pull},
		.factor = m_transform.factor * t_factor,
	};
}

auto DrawList::pop_scale() -> void
{
	assert(m_transform_depth > 0);

	m_transform_depth -= 1;
	m_transform = m_transform_stack[m_transform_depth];
}

auto DrawList::scaled(Vec2 t_point) const -> Vec2
{
	return Vec2{t_point.x * m_transform.factor + m_transform.offset.x, t_point.y * m_transform.factor + m_transform.offset.y};
}

auto DrawList::scaled(Rect t_rect) const -> Rect
{
	const Vec2 top_left = scaled(Vec2{t_rect.x, t_rect.y});

	return Rect{top_left.x, top_left.y, t_rect.w * m_transform.factor, t_rect.h * m_transform.factor};
}

auto DrawList::target(ShaderKind t_shader, const Texture* t_texture, RoundedBoxParams t_box, OutlineCountdownParams t_outline) -> void
{
	const bool clipped = m_clip_depth > 0;
	const Rect clip    = clipped ? m_clip_stack[m_clip_depth - 1] : Rect{};

	const bool continues_open_command = m_has_open_command && m_open.shader == t_shader && m_open.texture == t_texture && m_open.clipped == clipped &&
	                                    m_open.clip == clip && (!uses_rounded_box(t_shader) || m_open.box == t_box) &&
	                                    (t_shader != ShaderKind::OUTLINE_COUNTDOWN || m_open.outline == t_outline);
	if (continues_open_command) return;

	close_command();

	m_open = DrawCommand{
		.shader       = t_shader,
		.texture      = t_texture,
		.clipped      = clipped,
		.clip         = clip,
		.index_offset = m_index_count,
		.index_count  = 0,
		.box          = t_box,
		.outline      = t_outline,
	};
	m_has_open_command = true;
}

auto DrawList::close_command() -> void
{
	if (!m_has_open_command || m_index_count == m_open.index_offset) return;

	assert(m_command_count < K_MAX_COMMANDS);

	m_open.index_count          = m_index_count - m_open.index_offset;
	m_commands[m_command_count] = m_open;
	m_command_count += 1;

	m_open.index_offset = m_index_count;
}

auto DrawList::push_quad(const Vertex2D (&t_corners)[4]) -> void
{
	assert(m_vertex_count + 4 <= m_vertex_capacity);
	assert(m_index_count + 6 <= m_index_capacity);

	const u32 base = m_vertex_count;
	for (u32 i = 0; i < 4; i += 1) {
		const Vec2 position  = scaled(Vec2{t_corners[i].x, t_corners[i].y});
		m_vertices[base + i] = Vertex2D{position.x, position.y, t_corners[i].u, t_corners[i].v, t_corners[i].color};
	}
	m_vertex_count += 4;

	constexpr u32 QUAD_INDICES[6]{0, 1, 2, 0, 2, 3};
	for (const u32 index : QUAD_INDICES) {
		m_indices[m_index_count] = base + index;
		m_index_count += 1;
	}
}

auto DrawList::push_quad(Rect t_rect, UvRect t_uv, u32 t_color) -> void
{
	const Vertex2D corners[4]{
		{t_rect.x, t_rect.y, t_uv.u0, t_uv.v0, t_color},
		{t_rect.right(), t_rect.y, t_uv.u1, t_uv.v0, t_color},
		{t_rect.right(), t_rect.bottom(), t_uv.u1, t_uv.v1, t_color},
		{t_rect.x, t_rect.bottom(), t_uv.u0, t_uv.v1, t_color},
	};

	push_quad(corners);
}

auto DrawList::push_rounded(Rect t_rect, CornerRadii t_radii, UvRect t_uv, u32 t_color) -> void
{
	if (is_square(t_radii)) {
		push_quad(t_rect, t_uv, t_color);
		return;
	}

	assert(m_vertex_count + K_ROUNDED_POINT_COUNT + 1 <= m_vertex_capacity);
	assert(m_index_count + K_ROUNDED_POINT_COUNT * 3 <= m_index_capacity);

	const float max_radius = std::min(t_rect.w, t_rect.h) * 0.5f;
	const float radius[4]{
		std::min(t_radii.top_left, max_radius),
		std::min(t_radii.top_right, max_radius),
		std::min(t_radii.bottom_right, max_radius),
		std::min(t_radii.bottom_left, max_radius),
	};

	const Vec2 corner_centers[4]{
		{t_rect.x + radius[0], t_rect.y + radius[0]},
		{t_rect.right() - radius[1], t_rect.y + radius[1]},
		{t_rect.right() - radius[2], t_rect.bottom() - radius[2]},
		{t_rect.x + radius[3], t_rect.bottom() - radius[3]},
	};

	const u32  center_vertex  = m_vertex_count;
	const Vec2 center         = t_rect.center();
	const Vec2 center_uv      = uv_at(t_rect, t_uv, center);
	const Vec2 scaled_center  = scaled(center);
	m_vertices[center_vertex] = Vertex2D{scaled_center.x, scaled_center.y, center_uv.x, center_uv.y, t_color};

	const auto& directions = corner_arc_directions();
	for (u32 i = 0; i < K_ROUNDED_POINT_COUNT; i += 1) {
		const u32  corner = i / K_POINTS_PER_CORNER;
		const Vec2 point{corner_centers[corner].x + radius[corner] * directions[i].x, corner_centers[corner].y + radius[corner] * directions[i].y};
		const Vec2 uv = uv_at(t_rect, t_uv, point);

		const Vec2 scaled_point           = scaled(point);
		m_vertices[center_vertex + 1 + i] = Vertex2D{scaled_point.x, scaled_point.y, uv.x, uv.y, t_color};
	}

	m_vertex_count += K_ROUNDED_POINT_COUNT + 1;

	for (u32 i = 0; i < K_ROUNDED_POINT_COUNT; i += 1) {
		m_indices[m_index_count + 0] = center_vertex;
		m_indices[m_index_count + 1] = center_vertex + 1 + i;
		m_indices[m_index_count + 2] = center_vertex + 1 + (i + 1) % K_ROUNDED_POINT_COUNT;
		m_index_count += 3;
	}
}

auto DrawList::push_rounded_ring(Rect t_rect, CornerRadii t_radii, float t_thickness, u32 t_color) -> void
{
	assert(m_vertex_count + K_ROUNDED_POINT_COUNT * 2 <= m_vertex_capacity);
	assert(m_index_count + K_ROUNDED_POINT_COUNT * 6 <= m_index_capacity);

	const Rect  inner      = t_rect.inset(t_thickness);
	const float outer_most = std::min(t_rect.w, t_rect.h) * 0.5f;
	const float inner_most = std::max(0.0f, std::min(inner.w, inner.h) * 0.5f);
	const float corners[4]{t_radii.top_left, t_radii.top_right, t_radii.bottom_right, t_radii.bottom_left};
	const auto& directions = corner_arc_directions();
	const u32   first      = m_vertex_count;

	for (const bool outer : {true, false}) {
		const Rect box = outer ? t_rect : inner;

		for (u32 i = 0; i < K_ROUNDED_POINT_COUNT; i += 1) {
			const u32   corner = i / K_POINTS_PER_CORNER;
			const float radius = outer ? std::min(corners[corner], outer_most) : std::clamp(corners[corner] - t_thickness, 0.0f, inner_most);
			const Vec2  center{corner == 0 || corner == 3 ? box.x + radius : box.right() - radius, corner < 2 ? box.y + radius : box.bottom() - radius};
			const Vec2  point = scaled(Vec2{center.x + radius * directions[i].x, center.y + radius * directions[i].y});

			m_vertices[m_vertex_count] = Vertex2D{point.x, point.y, 0.0f, 0.0f, t_color};
			m_vertex_count += 1;
		}
	}

	for (u32 i = 0; i < K_ROUNDED_POINT_COUNT; i += 1) {
		const u32 next = (i + 1) % K_ROUNDED_POINT_COUNT;
		const u32 quad[6]{
			first + i, first + next, first + K_ROUNDED_POINT_COUNT + i, first + next, first + K_ROUNDED_POINT_COUNT + next, first + K_ROUNDED_POINT_COUNT + i};

		for (const u32 index : quad) {
			m_indices[m_index_count] = index;
			m_index_count += 1;
		}
	}
}

auto DrawList::add_rect(Rect t_rect, Color t_color) -> void
{
	note_cover(t_rect, t_color);
	target(ShaderKind::SOLID);
	push_quad(t_rect, K_FULL_UV, pack(t_color));
}

auto DrawList::add_triangle(Vec2 t_a, Vec2 t_b, Vec2 t_c, Color t_color) -> void
{
	target(ShaderKind::SOLID);

	assert(m_vertex_count + 3 <= m_vertex_capacity);
	assert(m_index_count + 3 <= m_index_capacity);

	const u32  base  = m_vertex_count;
	const u32  color = pack(t_color);
	const Vec2 corners[3]{scaled(t_a), scaled(t_b), scaled(t_c)};

	for (u32 i = 0; i < 3; i += 1) {
		m_vertices[base + i]         = Vertex2D{corners[i].x, corners[i].y, 0.0f, 0.0f, color};
		m_indices[m_index_count + i] = base + i;
	}

	m_vertex_count += 3;
	m_index_count += 3;
}

auto DrawList::add_rect_outline(Rect t_rect, float t_thickness, Color t_color) -> void
{
	const float inner_height = t_rect.h - t_thickness * 2.0f;

	add_rect(Rect{t_rect.x, t_rect.y, t_rect.w, t_thickness}, t_color);
	add_rect(Rect{t_rect.x, t_rect.bottom() - t_thickness, t_rect.w, t_thickness}, t_color);
	add_rect(Rect{t_rect.x, t_rect.y + t_thickness, t_thickness, inner_height}, t_color);
	add_rect(Rect{t_rect.right() - t_thickness, t_rect.y + t_thickness, t_thickness, inner_height}, t_color);
}

auto DrawList::add_gradient(Rect t_rect, Color t_top_left, Color t_top_right, Color t_bottom_left, Color t_bottom_right) -> void
{
	target(ShaderKind::SOLID);

	const Vertex2D corners[4]{
		{t_rect.x, t_rect.y, 0.0f, 0.0f, pack(t_top_left)},
		{t_rect.right(), t_rect.y, 0.0f, 0.0f, pack(t_top_right)},
		{t_rect.right(), t_rect.bottom(), 0.0f, 0.0f, pack(t_bottom_right)},
		{t_rect.x, t_rect.bottom(), 0.0f, 0.0f, pack(t_bottom_left)},
	};

	push_quad(corners);
}

auto DrawList::add_convex_polygon(std::span<const Vec2> t_points, std::span<const Color> t_colors) -> void
{
	const auto count = static_cast<u32>(t_points.size());
	if (count < 3) return;

	target(ShaderKind::SOLID);

	assert(m_vertex_count + count <= m_vertex_capacity);
	assert(m_index_count + (count - 2) * 3 <= m_index_capacity);

	const u32 base = m_vertex_count;
	for (u32 i = 0; i < count; i += 1) {
		const Vec2 position  = scaled(t_points[i]);
		m_vertices[base + i] = Vertex2D{position.x, position.y, 0.0f, 0.0f, pack(t_colors[i])};
	}

	m_vertex_count += count;

	for (u32 i = 1; i + 1 < count; i += 1) {
		m_indices[m_index_count + 0] = base;
		m_indices[m_index_count + 1] = base + i;
		m_indices[m_index_count + 2] = base + i + 1;
		m_index_count += 3;
	}
}

auto DrawList::add_backdrop(Rect t_rect, Color t_top_left, Color t_top_right, Color t_bottom_left, Color t_bottom_right) -> void
{
	push_backdrop(ShaderKind::BACKDROP, t_rect, t_top_left, t_top_right, t_bottom_left, t_bottom_right);
}

auto DrawList::add_plain_backdrop(Rect t_rect, Color t_top_left, Color t_top_right, Color t_bottom_left, Color t_bottom_right) -> void
{
	push_backdrop(ShaderKind::BACKDROP_PLAIN, t_rect, t_top_left, t_top_right, t_bottom_left, t_bottom_right);
}

auto DrawList::add_pattern_swatch(Rect t_rect, CornerRadii t_radii, Color t_color, u32 t_style) -> void
{
	constexpr float SWATCH_CODE = 8192.0f;

	const Vec2  center = scaled(t_rect.center());
	const float u      = static_cast<float>(t_style + 1) * SWATCH_CODE + std::round(center.x * g_pixel_scale);
	const float v      = std::round(center.y * g_pixel_scale);

	note_cover(t_rect, t_color);
	target(ShaderKind::BACKDROP);
	push_rounded(t_rect, t_radii, UvRect{u, v, u, v}, pack(t_color));
}

auto DrawList::push_backdrop(ShaderKind t_shader, Rect t_rect, Color t_top_left, Color t_top_right, Color t_bottom_left, Color t_bottom_right) -> void
{
	target(t_shader);

	const Vertex2D corners[4]{
		{t_rect.x, t_rect.y, 0.0f, 0.0f, pack(t_top_left)},
		{t_rect.right(), t_rect.y, 0.0f, 0.0f, pack(t_top_right)},
		{t_rect.right(), t_rect.bottom(), 0.0f, 0.0f, pack(t_bottom_right)},
		{t_rect.x, t_rect.bottom(), 0.0f, 0.0f, pack(t_bottom_left)},
	};

	push_quad(corners);

	const u8 weakest = std::min({t_top_left.a, t_top_right.a, t_bottom_left.a, t_bottom_right.a});
	note_cover(t_rect, with_alpha(t_top_left, weakest));
}

auto DrawList::add_line(Vec2 t_from, Vec2 t_to, float t_thickness, Color t_color) -> void
{
	const float dx     = t_to.x - t_from.x;
	const float dy     = t_to.y - t_from.y;
	const float length = std::sqrt(dx * dx + dy * dy);
	if (length < 0.0001f) return;

	target(ShaderKind::SOLID);

	const float half_thickness = t_thickness * 0.5f;
	const float normal_x       = -dy / length * half_thickness;
	const float normal_y       = dx / length * half_thickness;
	const u32   color          = pack(t_color);

	const Vertex2D corners[4]{
		{t_from.x + normal_x, t_from.y + normal_y, 0.0f, 0.0f, color},
		{t_to.x + normal_x, t_to.y + normal_y, 0.0f, 0.0f, color},
		{t_to.x - normal_x, t_to.y - normal_y, 0.0f, 0.0f, color},
		{t_from.x - normal_x, t_from.y - normal_y, 0.0f, 0.0f, color},
	};

	push_quad(corners);
}

auto DrawList::add_rounded_rect(Rect t_rect, CornerRadii t_radii, Color t_color) -> void
{
	note_cover(t_rect, t_color);
	target(ShaderKind::SOLID);
	push_rounded(t_rect, t_radii, K_FULL_UV, pack(t_color));
}

auto DrawList::add_rounded_outline(Rect t_rect, CornerRadii t_radii, float t_thickness, Color t_color) -> void
{
	if (t_color.a == 0 || t_thickness <= 0.0f) return;

	target(ShaderKind::SOLID);
	push_rounded_ring(t_rect, t_radii, t_thickness, pack(t_color));
}

auto DrawList::add_blurred_backdrop(Rect t_rect, CornerRadii t_radii, float t_spread, u8 t_alpha) -> void
{
	if (t_alpha == 0 || t_spread <= 0.0f) return;

	const Color tint{255, 255, 255, t_alpha};

	note_cover(t_rect, tint);
	target(ShaderKind::BACKDROP_BLUR, nullptr, RoundedBoxParams{.edge_width = t_spread});
	push_rounded(t_rect, t_radii, K_FULL_UV, pack(tint));
}

auto DrawList::add_bordered_rect(Rect t_rect, CornerRadii t_radii, Color t_fill, Color t_border, float t_thickness) -> void
{
	add_rounded_rect(t_rect, t_radii, t_border);

	const CornerRadii inner_radii{
		std::max(0.0f, t_radii.top_left - t_thickness),
		std::max(0.0f, t_radii.top_right - t_thickness),
		std::max(0.0f, t_radii.bottom_right - t_thickness),
		std::max(0.0f, t_radii.bottom_left - t_thickness),
	};

	add_rounded_rect(t_rect.inset(t_thickness), inner_radii, t_fill);
}

auto DrawList::add_image(Rect t_rect, const Texture* t_texture, Color t_tint, CornerRadii t_radii, UvRect t_uv) -> void
{
	if (t_texture == nullptr) return;

	target(ShaderKind::TEXTURED, t_texture);
	push_rounded(t_rect, t_radii, t_uv, pack(t_tint));
}

auto DrawList::add_rotated_image(Rect t_rect, float t_radians, const Texture* t_texture, Color t_tint) -> void
{
	if (t_texture == nullptr) return;

	target(ShaderKind::TEXTURED, t_texture);

	const Vec2  center    = t_rect.center();
	const float cos_angle = std::cos(t_radians);
	const float sin_angle = std::sin(t_radians);
	const float half_w    = t_rect.w * 0.5f;
	const float half_h    = t_rect.h * 0.5f;
	const u32   tint      = pack(t_tint);

	const auto corner = [&](float t_offset_x, float t_offset_y, float t_u, float t_v) {
		return Vertex2D{center.x + t_offset_x * cos_angle - t_offset_y * sin_angle, center.y + t_offset_x * sin_angle + t_offset_y * cos_angle, t_u, t_v, tint};
	};

	const Vertex2D corners[4]{
		corner(-half_w, -half_h, 0.0f, 0.0f),
		corner(half_w, -half_h, 1.0f, 0.0f),
		corner(half_w, half_h, 1.0f, 1.0f),
		corner(-half_w, half_h, 0.0f, 1.0f),
	};

	push_quad(corners);
}

auto DrawList::add_color_picker_square(Rect t_rect, float t_hue_degrees) -> void
{
	target(ShaderKind::COLOR_PICKER);

	const auto hue = static_cast<u8>(std::clamp(t_hue_degrees / 360.0f, 0.0f, 1.0f) * 255.0f);
	push_quad(t_rect, K_FULL_UV, pack(Color{hue, 0, 0, 255}));
}

auto DrawList::add_banner_glow(Rect t_card, float t_card_radius, float t_glow_size, Color t_color) -> void
{
	m_has_animated_effects = true;

	const Rect quad{t_card.x - t_glow_size, t_card.y - t_glow_size, t_card.w + t_glow_size * 2.0f, t_card.h + t_glow_size * 2.0f};

	const RoundedBoxParams glow{
		.quad_width    = quad.w,
		.quad_height   = quad.h,
		.corner_radius = scaled_radius(t_card_radius),
		.edge_width    = t_glow_size,
	};

	target(ShaderKind::BANNER_GLOW, nullptr, glow);
	push_rounded(quad, rounded(t_card_radius + t_glow_size), K_FULL_UV, pack(t_color));
}

auto DrawList::add_shadow(Rect t_rect, float t_corner_radius, float t_blur, Color t_color) -> void
{
	const Rect             quad = t_rect.inset(-t_blur);
	const RoundedBoxParams shadow{
		.quad_width    = quad.w,
		.quad_height   = quad.h,
		.corner_radius = std::min(scaled_radius(t_corner_radius), std::min(t_rect.w, t_rect.h) * 0.5f),
		.edge_width    = t_blur,
	};

	target(ShaderKind::SHADOW, nullptr, shadow);
	push_quad(quad, K_FULL_UV, pack(t_color));
}

auto DrawList::add_outline_countdown(Rect t_path, float t_corner_radius, float t_remaining, float t_thickness, Color t_color) -> void
{
	constexpr float GLOW_RADIUS = 1.8f;
	constexpr float GLOW_MARGIN = 8.0f;

	const float remaining = std::clamp(t_remaining, 0.0f, 1.0f);
	if (remaining <= 0.0f) return;

	const float radius     = std::clamp(t_corner_radius, 0.0f, std::min(t_path.w, t_path.h) * 0.5f);
	const float perimeter  = (t_path.w - radius * 2.0f) * 2.0f + (t_path.h - radius * 2.0f) * 2.0f + std::numbers::pi_v<float> * 2.0f * radius;
	const float lit_length = perimeter * remaining;
	const Vec2  end        = rounded_path_point(t_path, radius, lit_length);
	const Vec2  center     = t_path.center();
	const Rect  quad       = t_path.inset(-GLOW_MARGIN);

	const OutlineCountdownParams outline{
		.quad_width    = quad.w,
		.quad_height   = quad.h,
		.half_width    = t_path.w * 0.5f,
		.half_height   = t_path.h * 0.5f,
		.corner_radius = radius,
		.thickness     = t_thickness,
		.glow_radius   = GLOW_RADIUS,
		.lit_length    = lit_length,
		.end_x         = end.x - center.x,
		.end_y         = end.y - center.y,
		.padding       = {},
	};

	target(ShaderKind::OUTLINE_COUNTDOWN, nullptr, {}, outline);
	push_quad(quad, K_FULL_UV, pack(t_color));
}
