#include "gfx/draw_list.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <numbers>

#include "core/profiler.h"

namespace {
constexpr u32 corner_segments = 14;
constexpr u32 points_per_corner = corner_segments + 1;
constexpr u32 rounded_point_count = points_per_corner * 4;
constexpr float degrees_to_radians = std::numbers::pi_v<float> / 180.0f;

float g_corner_roundness = 1.0f;

u32 pack(Color t_color)
{
	return static_cast<u32>(t_color.r) | (static_cast<u32>(t_color.g) << 8) | (static_cast<u32>(t_color.b) << 16) |
		   (static_cast<u32>(t_color.a) << 24);
}

const std::array<Vec2, rounded_point_count> &corner_arc_directions()
{
	static const std::array<Vec2, rounded_point_count> directions = [] {
		constexpr float corner_start_degrees[4]{180.0f, 270.0f, 0.0f, 90.0f};

		std::array<Vec2, rounded_point_count> arc{};
		for (u32 corner = 0; corner < 4; corner += 1) {
			for (u32 step = 0; step < points_per_corner; step += 1) {
				const float degrees = corner_start_degrees[corner] + 90.0f * step / corner_segments;
				const float radians = degrees * degrees_to_radians;

				arc[corner * points_per_corner + step] = Vec2{std::cos(radians), std::sin(radians)};
			}
		}

		return arc;
	}();

	return directions;
}

bool is_square(CornerRadii t_radii)
{
	return t_radii.top_left <= 0.0f && t_radii.top_right <= 0.0f && t_radii.bottom_right <= 0.0f &&
		   t_radii.bottom_left <= 0.0f;
}

Vec2 uv_at(Rect t_rect, UvRect t_uv, Vec2 t_point)
{
	return Vec2{t_uv.u0 + (t_uv.u1 - t_uv.u0) * (t_point.x - t_rect.x) / t_rect.w,
				t_uv.v0 + (t_uv.v1 - t_uv.v0) * (t_point.y - t_rect.y) / t_rect.h};
}
}

float scaled_radius(float t_radius)
{
	return t_radius * g_corner_roundness;
}

void set_corner_roundness(float t_scale)
{
	g_corner_roundness = std::max(0.0f, t_scale);
}

CornerRadii rounded(float t_radius)
{
	const float radius = scaled_radius(t_radius);

	return CornerRadii{radius, radius, radius, radius};
}

CornerRadii rounded(float t_top_left, float t_top_right, float t_bottom_right, float t_bottom_left)
{
	return CornerRadii{scaled_radius(t_top_left), scaled_radius(t_top_right), scaled_radius(t_bottom_right),
					   scaled_radius(t_bottom_left)};
}

UvRect cover_uv(float t_box_aspect, float t_texture_aspect)
{
	if (t_box_aspect <= 0.0f || t_texture_aspect <= 0.0f) return full_uv;

	if (t_texture_aspect > t_box_aspect) {
		const float cropped = (1.0f - t_box_aspect / t_texture_aspect) * 0.5f;
		return UvRect{cropped, 0.0f, 1.0f - cropped, 1.0f};
	}

	const float cropped = (1.0f - t_texture_aspect / t_box_aspect) * 0.5f;

	return UvRect{0.0f, cropped, 1.0f, 1.0f - cropped};
}

void DrawList::init(u32 t_vertex_capacity, u32 t_index_capacity)
{
	m_vertices = std::make_unique_for_overwrite<Vertex2D[]>(t_vertex_capacity);
	m_indices = std::make_unique_for_overwrite<u32[]>(t_index_capacity);
	m_vertex_capacity = t_vertex_capacity;
	m_index_capacity = t_index_capacity;

	clear();
}

void DrawList::clear()
{
	m_vertex_count = 0;
	m_index_count = 0;
	m_command_count = 0;
	m_has_open_command = false;
	m_clip_depth = 0;
}

void DrawList::finish()
{
	PULSAR_PROFILE_SCOPE("DrawList.Finish");

	close_command();
	m_has_open_command = false;
}

void DrawList::push_clip(Rect t_rect)
{
	assert(m_clip_depth < max_clip_depth);

	m_clip_stack[m_clip_depth] = m_clip_depth > 0 ? m_clip_stack[m_clip_depth - 1].intersect(t_rect) : t_rect;
	m_clip_depth += 1;
}

void DrawList::pop_clip()
{
	assert(m_clip_depth > 0);

	m_clip_depth -= 1;
}

void DrawList::target(ShaderKind t_shader, const Texture *t_texture, BannerGlowParams t_glow,
					  CircularProgressParams t_progress)
{
	const bool clipped = m_clip_depth > 0;
	const Rect clip = clipped ? m_clip_stack[m_clip_depth - 1] : Rect{};

	const bool continues_open_command =
		m_has_open_command && m_open.shader == t_shader && m_open.texture == t_texture && m_open.clipped == clipped &&
		m_open.clip == clip && (t_shader != ShaderKind::banner_glow || m_open.glow == t_glow) &&
		(t_shader != ShaderKind::circular_progress || m_open.progress == t_progress);
	if (continues_open_command) return;

	close_command();

	m_open = DrawCommand{
		.shader = t_shader,
		.texture = t_texture,
		.clipped = clipped,
		.clip = clip,
		.index_offset = m_index_count,
		.index_count = 0,
		.glow = t_glow,
		.progress = t_progress,
	};
	m_has_open_command = true;
}

void DrawList::close_command()
{
	if (!m_has_open_command || m_index_count == m_open.index_offset) return;

	assert(m_command_count < max_commands);

	m_open.index_count = m_index_count - m_open.index_offset;
	m_commands[m_command_count] = m_open;
	m_command_count += 1;

	m_open.index_offset = m_index_count;
}

void DrawList::push_quad(const Vertex2D (&t_corners)[4])
{
	assert(m_vertex_count + 4 <= m_vertex_capacity);
	assert(m_index_count + 6 <= m_index_capacity);

	const u32 base = m_vertex_count;
	std::copy(std::begin(t_corners), std::end(t_corners), m_vertices.get() + base);
	m_vertex_count += 4;

	constexpr u32 quad_indices[6]{0, 1, 2, 0, 2, 3};
	for (const u32 index : quad_indices) {
		m_indices[m_index_count] = base + index;
		m_index_count += 1;
	}
}

void DrawList::push_quad(Rect t_rect, UvRect t_uv, u32 t_color)
{
	const Vertex2D corners[4]{
		{t_rect.x, t_rect.y, t_uv.u0, t_uv.v0, t_color},
		{t_rect.right(), t_rect.y, t_uv.u1, t_uv.v0, t_color},
		{t_rect.right(), t_rect.bottom(), t_uv.u1, t_uv.v1, t_color},
		{t_rect.x, t_rect.bottom(), t_uv.u0, t_uv.v1, t_color},
	};

	push_quad(corners);
}

void DrawList::push_rounded(Rect t_rect, CornerRadii t_radii, UvRect t_uv, u32 t_color)
{
	if (is_square(t_radii)) {
		push_quad(t_rect, t_uv, t_color);
		return;
	}

	assert(m_vertex_count + rounded_point_count + 1 <= m_vertex_capacity);
	assert(m_index_count + rounded_point_count * 3 <= m_index_capacity);

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

	const u32 center_vertex = m_vertex_count;
	const Vec2 center = t_rect.center();
	const Vec2 center_uv = uv_at(t_rect, t_uv, center);
	m_vertices[center_vertex] = Vertex2D{center.x, center.y, center_uv.x, center_uv.y, t_color};

	const auto &directions = corner_arc_directions();
	for (u32 i = 0; i < rounded_point_count; i += 1) {
		const u32 corner = i / points_per_corner;
		const Vec2 point{corner_centers[corner].x + radius[corner] * directions[i].x,
						 corner_centers[corner].y + radius[corner] * directions[i].y};
		const Vec2 uv = uv_at(t_rect, t_uv, point);

		m_vertices[center_vertex + 1 + i] = Vertex2D{point.x, point.y, uv.x, uv.y, t_color};
	}

	m_vertex_count += rounded_point_count + 1;

	for (u32 i = 0; i < rounded_point_count; i += 1) {
		m_indices[m_index_count + 0] = center_vertex;
		m_indices[m_index_count + 1] = center_vertex + 1 + i;
		m_indices[m_index_count + 2] = center_vertex + 1 + (i + 1) % rounded_point_count;
		m_index_count += 3;
	}
}

void DrawList::add_rect(Rect t_rect, Color t_color)
{
	target(ShaderKind::solid);
	push_quad(t_rect, full_uv, pack(t_color));
}

void DrawList::add_rect_outline(Rect t_rect, float t_thickness, Color t_color)
{
	const float inner_height = t_rect.h - t_thickness * 2.0f;

	add_rect(Rect{t_rect.x, t_rect.y, t_rect.w, t_thickness}, t_color);
	add_rect(Rect{t_rect.x, t_rect.bottom() - t_thickness, t_rect.w, t_thickness}, t_color);
	add_rect(Rect{t_rect.x, t_rect.y + t_thickness, t_thickness, inner_height}, t_color);
	add_rect(Rect{t_rect.right() - t_thickness, t_rect.y + t_thickness, t_thickness, inner_height}, t_color);
}

void DrawList::add_gradient(Rect t_rect, Color t_top_left, Color t_top_right, Color t_bottom_left, Color t_bottom_right)
{
	target(ShaderKind::solid);

	const Vertex2D corners[4]{
		{t_rect.x, t_rect.y, 0.0f, 0.0f, pack(t_top_left)},
		{t_rect.right(), t_rect.y, 0.0f, 0.0f, pack(t_top_right)},
		{t_rect.right(), t_rect.bottom(), 0.0f, 0.0f, pack(t_bottom_right)},
		{t_rect.x, t_rect.bottom(), 0.0f, 0.0f, pack(t_bottom_left)},
	};

	push_quad(corners);
}

void DrawList::add_line(Vec2 t_from, Vec2 t_to, float t_thickness, Color t_color)
{
	const float dx = t_to.x - t_from.x;
	const float dy = t_to.y - t_from.y;
	const float length = std::sqrt(dx * dx + dy * dy);
	if (length < 0.0001f) return;

	target(ShaderKind::solid);

	const float half_thickness = t_thickness * 0.5f;
	const float normal_x = -dy / length * half_thickness;
	const float normal_y = dx / length * half_thickness;
	const u32 color = pack(t_color);

	const Vertex2D corners[4]{
		{t_from.x + normal_x, t_from.y + normal_y, 0.0f, 0.0f, color},
		{t_to.x + normal_x, t_to.y + normal_y, 0.0f, 0.0f, color},
		{t_to.x - normal_x, t_to.y - normal_y, 0.0f, 0.0f, color},
		{t_from.x - normal_x, t_from.y - normal_y, 0.0f, 0.0f, color},
	};

	push_quad(corners);
}

void DrawList::add_rounded_rect(Rect t_rect, CornerRadii t_radii, Color t_color)
{
	target(ShaderKind::solid);
	push_rounded(t_rect, t_radii, full_uv, pack(t_color));
}

void DrawList::add_bordered_rect(Rect t_rect, CornerRadii t_radii, Color t_fill, Color t_border, float t_thickness)
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

void DrawList::add_image(Rect t_rect, const Texture *t_texture, Color t_tint, CornerRadii t_radii, UvRect t_uv)
{
	if (t_texture == nullptr) return;

	target(ShaderKind::textured, t_texture);
	push_rounded(t_rect, t_radii, t_uv, pack(t_tint));
}

void DrawList::add_rotated_image(Rect t_rect, float t_radians, const Texture *t_texture, Color t_tint)
{
	if (t_texture == nullptr) return;

	target(ShaderKind::textured, t_texture);

	const Vec2 center = t_rect.center();
	const float cos_angle = std::cos(t_radians);
	const float sin_angle = std::sin(t_radians);
	const float half_w = t_rect.w * 0.5f;
	const float half_h = t_rect.h * 0.5f;
	const u32 tint = pack(t_tint);

	const auto corner = [&](float t_offset_x, float t_offset_y, float t_u, float t_v) {
		return Vertex2D{center.x + t_offset_x * cos_angle - t_offset_y * sin_angle,
						center.y + t_offset_x * sin_angle + t_offset_y * cos_angle, t_u, t_v, tint};
	};

	const Vertex2D corners[4]{
		corner(-half_w, -half_h, 0.0f, 0.0f),
		corner(half_w, -half_h, 1.0f, 0.0f),
		corner(half_w, half_h, 1.0f, 1.0f),
		corner(-half_w, half_h, 0.0f, 1.0f),
	};

	push_quad(corners);
}

void DrawList::add_color_picker_square(Rect t_rect, float t_hue_degrees)
{
	target(ShaderKind::color_picker);

	const auto hue = static_cast<u8>(std::clamp(t_hue_degrees / 360.0f, 0.0f, 1.0f) * 255.0f);
	push_quad(t_rect, full_uv, pack(Color{hue, 0, 0, 255}));
}

void DrawList::add_banner_glow(Rect t_card, float t_card_radius, float t_glow_size, Color t_color)
{
	const Rect quad{t_card.x - t_glow_size, t_card.y - t_glow_size, t_card.w + t_glow_size * 2.0f,
					t_card.h + t_glow_size * 2.0f};

	const BannerGlowParams glow{
		.quad_width = quad.w,
		.quad_height = quad.h,
		.corner_radius = scaled_radius(t_card_radius),
		.ring_width = t_glow_size,
	};

	target(ShaderKind::banner_glow, nullptr, glow);
	push_rounded(quad, rounded(t_card_radius + t_glow_size), full_uv, pack(t_color));
}

void DrawList::add_circular_progress(Vec2 t_center, float t_outer_radius, float t_inner_radius, float t_glow_margin,
									 float t_start_degrees, float t_sweep_degrees, float t_glow_strength, Color t_color)
{
	const float half_size = t_outer_radius + t_glow_margin;
	const Rect quad{t_center.x - half_size, t_center.y - half_size, half_size * 2.0f, half_size * 2.0f};

	const CircularProgressParams progress{
		.quad_width = quad.w,
		.quad_height = quad.h,
		.outer_radius = t_outer_radius,
		.inner_radius = t_inner_radius,
		.start_angle = t_start_degrees * degrees_to_radians,
		.sweep_angle = t_sweep_degrees * degrees_to_radians,
		.glow_strength = t_glow_strength,
	};

	target(ShaderKind::circular_progress, nullptr, {}, progress);
	push_quad(quad, full_uv, pack(t_color));
}
