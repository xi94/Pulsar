#include "render/renderer.h"

#include <algorithm>
#include <cassert>
#include <cmath>

#include "core/debug_log.h"
#include "os/window.h"
#include "render/render_backend.h"

namespace {
constexpr const char* K_LOG_CATEGORY = "gfx";

[[nodiscard]] auto create_backend(GraphicsApi t_api) -> std::unique_ptr<RenderBackend>
{
	return t_api == GraphicsApi::OPENGL ? create_opengl_render_backend() : create_native_render_backend();
}
}

Texture::Texture(Renderer* t_renderer, std::span<const TextureLevel> t_levels, bool t_updatable)
	: m_renderer(t_renderer)
	, m_slot(t_renderer->create_texture(t_levels, t_updatable))
	, m_width(t_levels.front().width)
	, m_height(t_levels.front().height)
{
}

Texture::~Texture()
{
	if (is_valid()) {
		m_renderer->destroy_texture(m_slot);
	}
}

auto Texture::is_valid() const -> bool
{
	return m_slot != Renderer::K_INVALID_TEXTURE_SLOT;
}

auto Texture::update(u32 t_x, u32 t_y, u32 t_width, u32 t_height, const u8* t_rgba_pixels) -> void
{
	if (is_valid()) {
		m_renderer->update_texture(m_slot, t_x, t_y, t_width, t_height, t_rgba_pixels);
	}
}

auto Texture::restore(std::span<const TextureLevel> t_levels) -> void
{
	if (is_valid()) {
		m_renderer->restore_texture(m_slot, t_levels);
	}
}

auto graphics_api_name(GraphicsApi t_api) -> std::string_view
{
	return t_api == GraphicsApi::OPENGL ? "OpenGL" : native_render_backend_name();
}

auto pixel_scale(const RenderFrame& t_frame) -> float
{
	return t_frame.logical_width > 0.0f ? static_cast<float>(t_frame.physical_width) / t_frame.logical_width : 1.0f;
}

auto blur_level_extent(u32 t_frame_extent, u32 t_level) -> u32
{
	const u32 scale = 2u << t_level;

	return std::max((t_frame_extent + scale - 1) / scale, 1u);
}

// Glass blurs at the level where its spread is one or two texels wide, so a pass or two covers it, and reads the level back through
// a cubic filter that hides the texels. Halving and that filter already spread the image a little, so the passes only add the rest.
auto blur_plan(const RenderFrame& t_frame, const DrawCommand& t_command) -> BlurPlan
{
	constexpr float LEVEL_SPREAD    = 1.5f;
	constexpr float KERNEL_SPREAD   = 1.95f;
	constexpr float MAX_STEP        = 1.25f;
	constexpr float FILTER_VARIANCE = 0.58f;
	constexpr u32   MAX_ITERATIONS  = 6;

	const float spread       = t_command.box.edge_width * pixel_scale(t_frame);
	const auto  halvings     = static_cast<u32>(std::log2(std::max(spread / LEVEL_SPREAD, 2.0f)));
	const u32   level        = std::min(halvings, K_BLUR_LEVEL_COUNT) - 1;
	const float level_spread = spread / static_cast<float>(2u << level);
	const float variance     = std::max(level_spread * level_spread - FILTER_VARIANCE, 0.0f);
	const float pass_spread  = KERNEL_SPREAD * MAX_STEP;
	const u32   iterations   = std::min(static_cast<u32>(std::ceil(variance / (pass_spread * pass_spread))), MAX_ITERATIONS);
	const float step         = iterations > 0 ? std::sqrt(variance / static_cast<float>(iterations)) / KERNEL_SPREAD : 0.0f;

	return BlurPlan{.level = level, .iterations = iterations, .step = step};
}

auto scissor_for(const RenderFrame& t_frame, const DrawCommand& t_command) -> ScissorRect
{
	ScissorRect scissor{0, 0, static_cast<i32>(t_frame.physical_width), static_cast<i32>(t_frame.physical_height)};
	if (!t_command.clipped) return scissor;

	const float scale = pixel_scale(t_frame);
	const Rect& clip  = t_command.clip;

	scissor.left   = std::clamp(static_cast<i32>(clip.x * scale), 0, scissor.right);
	scissor.top    = std::clamp(static_cast<i32>(clip.y * scale), 0, scissor.bottom);
	scissor.right  = std::clamp(static_cast<i32>(clip.right() * scale), scissor.left, scissor.right);
	scissor.bottom = std::clamp(static_cast<i32>(clip.bottom() * scale), scissor.top, scissor.bottom);

	return scissor;
}

auto viewport_constants(const RenderFrame& t_frame) -> ViewportConstants
{
	return ViewportConstants{.width = t_frame.logical_width, .height = t_frame.logical_height};
}

auto backdrop_constants(const RenderFrame& t_frame) -> BackdropConstants
{
	return BackdropConstants{
		.target_width  = static_cast<float>(t_frame.physical_width),
		.target_height = static_cast<float>(t_frame.physical_height),
		.intensity     = t_frame.backdrop_intensity,
		.style         = static_cast<float>(t_frame.backdrop_style),
		.pixel_scale   = pixel_scale(t_frame),
		.light         = t_frame.backdrop_light,
		.grain         = t_frame.backdrop_grain,
	};
}

auto effect_constants(const RenderFrame& t_frame, const DrawCommand& t_command, EffectConstants* t_out) -> usize
{
	switch (t_command.shader) {
		using enum ShaderKind;

		case BANNER_GLOW: {
			t_out->banner_glow = BannerGlowConstants{.time_seconds = t_frame.effect_time_seconds, .params = t_command.box};
			return sizeof(BannerGlowConstants);
		}

		case SHADOW: {
			t_out->shadow = ShadowConstants{.params = t_command.box};
			return sizeof(ShadowConstants);
		}

		case OUTLINE_COUNTDOWN: {
			t_out->outline_countdown = OutlineCountdownConstants{.params = t_command.outline};
			return sizeof(OutlineCountdownConstants);
		}

		case BACKDROP:
		case BACKDROP_PLAIN: {
			t_out->backdrop = backdrop_constants(t_frame);
			return sizeof(BackdropConstants);
		}

		case BACKDROP_BLUR: {
			const u32 level = blur_plan(t_frame, t_command).level;
			t_out->blur     = BlurConstants{
				.target_width  = static_cast<float>(t_frame.physical_width),
				.target_height = static_cast<float>(t_frame.physical_height),
				.step_x        = 1.0f / static_cast<float>(blur_level_extent(t_frame.physical_width, level)),
				.step_y        = 1.0f / static_cast<float>(blur_level_extent(t_frame.physical_height, level)),
			};
			return sizeof(BlurConstants);
		}

		case SOLID:
		case TEXTURED:
		case COLOR_PICKER:
		case COUNT: {
			break;
		}
	}

	return 0;
}

Renderer::Renderer() = default;

Renderer::~Renderer() = default;

auto Renderer::init(const os::Window* t_window, GraphicsApi t_preferred_api) -> bool
{
	const GraphicsApi fallback_api = t_preferred_api == GraphicsApi::OPENGL ? GraphicsApi::NATIVE : GraphicsApi::OPENGL;
	m_requested_api                = t_preferred_api;

	for (const GraphicsApi api : {t_preferred_api, fallback_api}) {
		const std::string_view name = graphics_api_name(api);
		m_backend                   = create_backend(api);

		if (m_backend->init(t_window)) {
			m_api = api;
			remember_size(t_window);
			debug_log::write(K_LOG_CATEGORY, "rendering with %.*s", static_cast<int>(name.size()), name.data());

			return true;
		}

		debug_log::write(K_LOG_CATEGORY, "%.*s could not start", static_cast<int>(name.size()), name.data());
		m_backend.reset();
	}

	return false;
}

auto Renderer::resize(const os::Window* t_window) -> void
{
	const u32 width  = t_window->physical_width();
	const u32 height = t_window->physical_height();
	if (m_backend == nullptr || width == 0 || height == 0 || (width == m_physical_width && height == m_physical_height)) return;

	m_backend->resize(width, height);
	remember_size(t_window);
}

auto Renderer::render(const DrawList* t_draw_list, Color t_clear_color) -> void
{
	if (m_backend == nullptr) return;

	m_backend->render(RenderFrame{
		.draw_list           = t_draw_list,
		.clear_color         = t_clear_color,
		.physical_width      = m_physical_width,
		.physical_height     = m_physical_height,
		.logical_width       = m_logical_width,
		.logical_height      = m_logical_height,
		.effect_time_seconds = m_effect_time_seconds,
		.backdrop_style      = m_backdrop_style,
		.backdrop_intensity  = m_backdrop_intensity,
		.backdrop_light      = m_backdrop_light,
		.backdrop_grain      = m_backdrop_grain,
	});
}

auto Renderer::supports_backdrop_blur() const -> bool
{
	return m_backend != nullptr && m_backend->supports_backdrop_blur();
}

auto Renderer::is_device_lost() const -> bool
{
	return m_backend == nullptr || m_backend->is_device_lost();
}

// Builds a new device after the old one was lost. Every texture slot stays with the Texture that holds it, but the new device starts empty,
// so their owners upload their pixels again once this succeeds.
auto Renderer::recover(const os::Window* t_window) -> bool
{
	m_backend.reset();

	std::unique_ptr<RenderBackend> backend = create_backend(m_api);
	if (!backend->init(t_window)) return false;

	m_backend = std::move(backend);
	remember_size(t_window);

	const std::string_view name = graphics_api_name(m_api);
	debug_log::write(K_LOG_CATEGORY, "rebuilt the %.*s device after it was lost", static_cast<int>(name.size()), name.data());

	return true;
}

auto Renderer::create_texture(std::span<const TextureLevel> t_levels, bool t_updatable) -> u32
{
	const u32 slot = allocate_texture_slot();
	if (slot == K_INVALID_TEXTURE_SLOT) return K_INVALID_TEXTURE_SLOT;

	if (m_backend == nullptr || !m_backend->create_texture(slot, t_levels, t_updatable)) {
		release_texture_slot(slot);
		return K_INVALID_TEXTURE_SLOT;
	}

	return slot;
}

auto Renderer::update_texture(u32 t_slot, u32 t_x, u32 t_y, u32 t_width, u32 t_height, const u8* t_rgba_pixels) -> void
{
	if (m_backend != nullptr) {
		m_backend->update_texture(t_slot, t_x, t_y, t_width, t_height, t_rgba_pixels);
	}
}

auto Renderer::destroy_texture(u32 t_slot) -> void
{
	if (m_backend != nullptr) {
		m_backend->destroy_texture(t_slot);
	}

	release_texture_slot(t_slot);
}

auto Renderer::restore_texture(u32 t_slot, std::span<const TextureLevel> t_levels) -> void
{
	if (m_backend != nullptr) {
		static_cast<void>(m_backend->create_texture(t_slot, t_levels, false));
	}
}

auto Renderer::allocate_texture_slot() -> u32
{
	if (m_free_texture_count > 0) {
		m_free_texture_count -= 1;
		return m_free_texture_slots[m_free_texture_count];
	}

	assert(m_texture_high_water < K_MAX_TEXTURES);
	if (m_texture_high_water >= K_MAX_TEXTURES) return K_INVALID_TEXTURE_SLOT;

	const u32 slot = m_texture_high_water;
	m_texture_high_water += 1;

	return slot;
}

auto Renderer::release_texture_slot(u32 t_slot) -> void
{
	assert(t_slot < m_texture_high_water && m_free_texture_count < K_MAX_TEXTURES);

	m_free_texture_slots[m_free_texture_count] = t_slot;
	m_free_texture_count += 1;
}

auto Renderer::remember_size(const os::Window* t_window) -> void
{
	m_physical_width  = t_window->physical_width();
	m_physical_height = t_window->physical_height();
	m_logical_width   = static_cast<float>(t_window->width());
	m_logical_height  = static_cast<float>(t_window->height());
}
