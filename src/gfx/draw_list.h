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
	u32 color;
};

enum class ShaderKind : u8 {
	solid,
	textured,
	banner_glow,
	color_picker,
	circular_progress,
	shadow,
	outline_countdown,
};

struct RoundedBoxParams {
	float quad_width;
	float quad_height;
	float corner_radius;
	float edge_width;

	bool operator==(const RoundedBoxParams &) const = default;
};

struct CircularProgressParams {
	float quad_width;
	float quad_height;
	float outer_radius;
	float inner_radius;
	float start_angle;
	float sweep_angle;
	float glow_strength;
	Color track;

	bool operator==(const CircularProgressParams &) const = default;
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

	bool operator==(const OutlineCountdownParams &) const = default;
};

struct DrawCommand {
	ShaderKind shader;
	const Texture *texture;
	bool clipped;
	Rect clip;
	u32 index_offset;
	u32 index_count;
	RoundedBoxParams box;
	CircularProgressParams progress;
	OutlineCountdownParams outline;
};

struct UvRect {
	float u0;
	float v0;
	float u1;
	float v1;
};

constexpr UvRect full_uv{0.0f, 0.0f, 1.0f, 1.0f};

float scaled_radius(float t_radius);
void set_corner_roundness(float t_scale);

void set_pixel_scale(float t_scale);
float snapped_to_pixel(float t_value);

CornerRadii rounded(float t_radius);
CornerRadii rounded(float t_top_left, float t_top_right, float t_bottom_right, float t_bottom_left);

UvRect cover_uv(float t_box_aspect, float t_texture_aspect);

class DrawList {
  public:
	void init(u32 t_vertex_capacity, u32 t_index_capacity);
	void clear();
	void finish();

	void push_clip(Rect t_rect);
	void pop_clip();

	void push_scale(Vec2 t_origin, float t_factor);
	void pop_scale();

	std::span<const DrawCommand> commands() const
	{
		return {m_commands, m_command_count};
	}

	std::span<const Vertex2D> vertices() const
	{
		return {m_vertices.get(), m_vertex_count};
	}

	std::span<const u32> indices() const
	{
		return {m_indices.get(), m_index_count};
	}

	void add_rect(Rect t_rect, Color t_color);
	void add_triangle(Vec2 t_a, Vec2 t_b, Vec2 t_c, Color t_color);
	void add_rect_outline(Rect t_rect, float t_thickness, Color t_color);
	void add_gradient(Rect t_rect, Color t_top_left, Color t_top_right, Color t_bottom_left, Color t_bottom_right);
	void add_line(Vec2 t_from, Vec2 t_to, float t_thickness, Color t_color);

	void add_rounded_rect(Rect t_rect, CornerRadii t_radii, Color t_color);
	void add_bordered_rect(Rect t_rect, CornerRadii t_radii, Color t_fill, Color t_border, float t_thickness);

	void add_image(Rect t_rect, const Texture *t_texture, Color t_tint, CornerRadii t_radii = square_corners,
				   UvRect t_uv = full_uv);
	void add_rotated_image(Rect t_rect, float t_radians, const Texture *t_texture, Color t_tint);

	void add_color_picker_square(Rect t_rect, float t_hue_degrees);
	void add_banner_glow(Rect t_card, float t_card_radius, float t_glow_size, Color t_color);
	void add_shadow(Rect t_rect, float t_corner_radius, float t_blur, Color t_color);
	void add_circular_progress(Vec2 t_center, float t_outer_radius, float t_inner_radius, float t_glow_margin,
							   float t_start_degrees, float t_sweep_degrees, float t_glow_strength, Color t_color,
							   Color t_track);
	void add_outline_countdown(Rect t_path, float t_corner_radius, float t_remaining, float t_thickness, Color t_color);

  private:
	static constexpr u32 max_commands = 256;
	static constexpr u32 max_clip_depth = 8;

	void target(ShaderKind t_shader, const Texture *t_texture = nullptr, RoundedBoxParams t_box = {},
				CircularProgressParams t_progress = {}, OutlineCountdownParams t_outline = {});
	void close_command();

	Vec2 scaled(Vec2 t_point) const;
	Rect scaled(Rect t_rect) const;

	void push_quad(Rect t_rect, UvRect t_uv, u32 t_color);
	void push_quad(const Vertex2D (&t_corners)[4]);
	void push_rounded(Rect t_rect, CornerRadii t_radii, UvRect t_uv, u32 t_color);

	std::unique_ptr<Vertex2D[]> m_vertices;
	std::unique_ptr<u32[]> m_indices;
	u32 m_vertex_capacity = 0;
	u32 m_index_capacity = 0;
	u32 m_vertex_count = 0;
	u32 m_index_count = 0;

	DrawCommand m_commands[max_commands]{};
	u32 m_command_count = 0;
	DrawCommand m_open{};
	bool m_has_open_command = false;

	Rect m_clip_stack[max_clip_depth]{};
	u32 m_clip_depth = 0;

	struct Scale {
		Vec2 origin{};
		float factor = 1.0f;
	};

	Scale m_scale;
};
