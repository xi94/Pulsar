#pragma once

#include <memory>
#include <span>

#include "core/types.h"

class Texture;

struct Vertex2D {
	float x;
	float y;
	float u;
	float v;
	u32   color;
};

enum class ShaderKind : u8 {
	SOLID,
	TEXTURED,
	BANNER_GLOW,
	COLOR_PICKER,
	SHADOW,
	OUTLINE_COUNTDOWN,
	BACKDROP,
	BACKDROP_PLAIN,
	COUNT,
};

constexpr u32 K_SHADER_KIND_COUNT = static_cast<u32>(ShaderKind::COUNT);

struct RoundedBoxParams {
	float quad_width;
	float quad_height;
	float corner_radius;
	float edge_width;

	auto operator==(const RoundedBoxParams&) const -> bool = default;
};

struct OutlineCountdownParams {
	float quad_width;
	float quad_height;
	float half_width;
	float half_height;
	float corner_radius;
	float thickness;
	float glow_radius;
	float lit_length;
	float end_x;
	float end_y;
	float padding[2];

	auto operator==(const OutlineCountdownParams&) const -> bool = default;
};

struct DrawCommand {
	ShaderKind             shader;
	const Texture*         texture;
	bool                   clipped;
	Rect                   clip;
	u32                    index_offset;
	u32                    index_count;
	RoundedBoxParams       box;
	OutlineCountdownParams outline;
};

struct UvRect {
	float u0;
	float v0;
	float u1;
	float v1;
};

constexpr UvRect K_FULL_UV{0.0f, 0.0f, 1.0f, 1.0f};

[[nodiscard]] auto scaled_radius(float t_radius) -> float;
auto set_corner_roundness(float t_scale) -> void;

auto set_pixel_scale(float t_scale) -> void;
[[nodiscard]] auto snapped_to_pixel(float t_value) -> float;

[[nodiscard]] auto rounded(float t_radius) -> CornerRadii;
[[nodiscard]] auto rounded(float t_top_left, float t_top_right, float t_bottom_right, float t_bottom_left) -> CornerRadii;

[[nodiscard]] auto cover_uv(float t_box_aspect, float t_texture_aspect) -> UvRect;

class DrawList {
  public:
	auto init(u32 t_vertex_capacity, u32 t_index_capacity) -> void;
	auto clear() -> void;
	auto finish() -> void;

	auto push_clip(Rect t_rect) -> void;
	auto pop_clip() -> void;

	auto set_probe(Vec2 t_point) -> void;
	[[nodiscard]] auto visible_rect(Rect t_rect) const -> Rect;

	[[nodiscard]] auto has_animated_effects() const -> bool
	{
		return m_has_animated_effects;
	}

	[[nodiscard]] auto probe_cover_count() const -> u32
	{
		return m_probe_cover_count;
	}

	auto push_scale(Vec2 t_origin, float t_factor) -> void;
	auto pop_scale() -> void;

	[[nodiscard]] auto commands() const -> std::span<const DrawCommand>
	{
		return {m_commands, m_command_count};
	}

	[[nodiscard]] auto vertices() const -> std::span<const Vertex2D>
	{
		return {m_vertices.get(), m_vertex_count};
	}

	[[nodiscard]] auto indices() const -> std::span<const u32>
	{
		return {m_indices.get(), m_index_count};
	}

	auto add_rect(Rect t_rect, Color t_color) -> void;
	auto add_triangle(Vec2 t_a, Vec2 t_b, Vec2 t_c, Color t_color) -> void;
	auto add_rect_outline(Rect t_rect, float t_thickness, Color t_color) -> void;
	auto add_gradient(Rect t_rect, Color t_top_left, Color t_top_right, Color t_bottom_left, Color t_bottom_right) -> void;
	auto add_quad(const Vec2 (&t_corners)[4], const Color (&t_colors)[4]) -> void;
	auto add_backdrop(Rect t_rect, Color t_top_left, Color t_top_right, Color t_bottom_left, Color t_bottom_right) -> void;
	auto add_plain_backdrop(Rect t_rect, Color t_top_left, Color t_top_right, Color t_bottom_left, Color t_bottom_right) -> void;
	auto add_pattern_swatch(Rect t_rect, CornerRadii t_radii, Color t_color, u32 t_style) -> void;
	auto add_line(Vec2 t_from, Vec2 t_to, float t_thickness, Color t_color) -> void;

	auto add_rounded_rect(Rect t_rect, CornerRadii t_radii, Color t_color) -> void;
	auto add_bordered_rect(Rect t_rect, CornerRadii t_radii, Color t_fill, Color t_border, float t_thickness) -> void;

	auto add_image(Rect t_rect, const Texture* t_texture, Color t_tint, CornerRadii t_radii = K_SQUARE_CORNERS, UvRect t_uv = K_FULL_UV) -> void;
	auto add_rotated_image(Rect t_rect, float t_radians, const Texture* t_texture, Color t_tint) -> void;

	auto add_color_picker_square(Rect t_rect, float t_hue_degrees) -> void;
	auto add_banner_glow(Rect t_card, float t_card_radius, float t_glow_size, Color t_color) -> void;
	auto add_shadow(Rect t_rect, float t_corner_radius, float t_blur, Color t_color) -> void;
	auto add_outline_countdown(Rect t_path, float t_corner_radius, float t_remaining, float t_thickness, Color t_color) -> void;

  private:
	auto note_cover(Rect t_rect, Color t_color) -> void;

	static constexpr u32 K_MAX_COMMANDS   = 256;
	static constexpr u32 K_MAX_CLIP_DEPTH = 8;

	auto target(ShaderKind t_shader, const Texture* t_texture = nullptr, RoundedBoxParams t_box = {}, OutlineCountdownParams t_outline = {}) -> void;
	auto close_command() -> void;

	[[nodiscard]] auto scaled(Vec2 t_point) const -> Vec2;
	[[nodiscard]] auto scaled(Rect t_rect) const -> Rect;

	auto push_backdrop(ShaderKind t_shader, Rect t_rect, Color t_top_left, Color t_top_right, Color t_bottom_left, Color t_bottom_right) -> void;
	auto push_quad(Rect t_rect, UvRect t_uv, u32 t_color) -> void;
	auto push_quad(const Vertex2D (&t_corners)[4]) -> void;
	auto push_rounded(Rect t_rect, CornerRadii t_radii, UvRect t_uv, u32 t_color) -> void;

	std::unique_ptr<Vertex2D[]> m_vertices;
	std::unique_ptr<u32[]>      m_indices;
	u32                         m_vertex_capacity = 0;
	u32                         m_index_capacity  = 0;
	u32                         m_vertex_count    = 0;
	u32                         m_index_count     = 0;

	DrawCommand m_commands[K_MAX_COMMANDS]{};
	u32         m_command_count = 0;
	DrawCommand m_open{};
	bool        m_has_open_command = false;

	Rect m_clip_stack[K_MAX_CLIP_DEPTH]{};
	u32  m_clip_depth = 0;

	Vec2 m_probe{-1.0f, -1.0f};
	u32  m_probe_cover_count    = 0;
	bool m_has_animated_effects = false;

	struct Scale {
		Vec2  origin{};
		float factor = 1.0f;
	};

	Scale m_scale;
};
