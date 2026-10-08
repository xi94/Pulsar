#pragma once

#include <memory>
#include <span>
#include <string_view>

#include "core/types.h"
#include "gfx/draw_list.h"
#include "render/renderer.h"

namespace os {
class Window;
}

struct ViewportConstants {
	float width;
	float height;
	float padding[2];
};

struct BannerGlowConstants {
	float            time_seconds;
	RoundedBoxParams params;
	float            padding[3];
};

struct ShadowConstants {
	RoundedBoxParams params;
};

struct OutlineCountdownConstants {
	OutlineCountdownParams params;
};

struct BackdropConstants {
	float target_width;
	float target_height;
	float intensity;
	float style;
	float pixel_scale;
	float light;
	float grain;
	float padding;
};

// The blur passes read step as the distance between taps. The glass draw reads the frame size, and as step the texel size of the
// level it samples.
struct BlurConstants {
	float target_width;
	float target_height;
	float step_x;
	float step_y;
};

static_assert(sizeof(ViewportConstants) == 16);
static_assert(sizeof(BlurConstants) == 16);
static_assert(sizeof(BannerGlowConstants) == 32);
static_assert(sizeof(ShadowConstants) == 16);
static_assert(sizeof(OutlineCountdownConstants) == 48);
static_assert(sizeof(BackdropConstants) == 32);

union EffectConstants {
	BannerGlowConstants       banner_glow;
	ShadowConstants           shadow;
	OutlineCountdownConstants outline_countdown;
	BackdropConstants         backdrop;
	BlurConstants             blur;
};

struct RenderFrame {
	const DrawList* draw_list;
	Color           clear_color;
	u32             physical_width;
	u32             physical_height;
	float           logical_width;
	float           logical_height;
	float           effect_time_seconds;
	u32             backdrop_style;
	float           backdrop_intensity;
	float           backdrop_light;
	float           backdrop_grain;
};

struct ScissorRect {
	i32 left;
	i32 top;
	i32 right;
	i32 bottom;
};

constexpr u32 K_BLUR_LEVEL_COUNT = 4;

// Glass blurs at one level of a chain that halves the frame at each step, level 0 being half size.
struct BlurPlan {
	u32   level;
	u32   iterations;
	float step;

	auto operator==(const BlurPlan&) const -> bool = default;
};

[[nodiscard]] auto pixel_scale(const RenderFrame& t_frame) -> float;
[[nodiscard]] auto blur_level_extent(u32 t_frame_extent, u32 t_level) -> u32;
[[nodiscard]] auto blur_plan(const RenderFrame& t_frame, const DrawCommand& t_command) -> BlurPlan;
[[nodiscard]] auto scissor_for(const RenderFrame& t_frame, const DrawCommand& t_command) -> ScissorRect;
[[nodiscard]] auto viewport_constants(const RenderFrame& t_frame) -> ViewportConstants;
[[nodiscard]] auto backdrop_constants(const RenderFrame& t_frame) -> BackdropConstants;
[[nodiscard]] auto effect_constants(const RenderFrame& t_frame, const DrawCommand& t_command, EffectConstants* t_out) -> usize;

class RenderBackend {
  public:
	RenderBackend()          = default;
	virtual ~RenderBackend() = default;

	RenderBackend(const RenderBackend&)                    = delete;
	auto operator=(const RenderBackend&) -> RenderBackend& = delete;

	[[nodiscard]] virtual auto init(const os::Window* t_window) -> bool      = 0;
	virtual auto resize(u32 t_physical_width, u32 t_physical_height) -> void = 0;
	virtual auto render(const RenderFrame& t_frame) -> void                  = 0;

	[[nodiscard]] virtual auto supports_backdrop_blur() const -> bool
	{
		return false;
	}

	[[nodiscard]] virtual auto create_texture(u32 t_slot, std::span<const TextureLevel> t_levels, bool t_updatable) -> bool = 0;
	virtual auto update_texture(u32 t_slot, u32 t_x, u32 t_y, u32 t_width, u32 t_height, const u8* t_rgba_pixels) -> void   = 0;
	virtual auto destroy_texture(u32 t_slot) -> void                                                                        = 0;
};

[[nodiscard]] auto create_native_render_backend() -> std::unique_ptr<RenderBackend>;
[[nodiscard]] auto create_opengl_render_backend() -> std::unique_ptr<RenderBackend>;
[[nodiscard]] auto native_render_backend_name() -> std::string_view;
