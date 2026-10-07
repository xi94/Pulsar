#include "render/renderer.h"

#include <algorithm>
#include <cassert>

#include "core/debug_log.h"
#include "os/window.h"
#include "render/render_backend.h"

namespace {
constexpr const char* K_LOG_CATEGORY = "gfx";

[[nodiscard]] auto create_backend(GraphicsApi t_api) -> std::unique_ptr<RenderBackend>
{
	return t_api == GraphicsApi::OpenGl ? create_opengl_render_backend() : create_native_render_backend();
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

auto graphics_api_name(GraphicsApi t_api) -> std::string_view
{
	return t_api == GraphicsApi::OpenGl ? "OpenGL" : native_render_backend_name();
}

auto pixel_scale(const RenderFrame& t_frame) -> float
{
	return t_frame.logical_width > 0.0f ? static_cast<float>(t_frame.physical_width) / t_frame.logical_width : 1.0f;
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
		case ShaderKind::BannerGlow: {
			t_out->banner_glow = BannerGlowConstants{.time_seconds = t_frame.effect_time_seconds, .params = t_command.box};
			return sizeof(BannerGlowConstants);
		}

		case ShaderKind::Shadow: {
			t_out->shadow = ShadowConstants{.params = t_command.box};
			return sizeof(ShadowConstants);
		}

		case ShaderKind::OutlineCountdown: {
			t_out->outline_countdown = OutlineCountdownConstants{.params = t_command.outline};
			return sizeof(OutlineCountdownConstants);
		}

		case ShaderKind::Backdrop:
		case ShaderKind::BackdropPlain: {
			t_out->backdrop = backdrop_constants(t_frame);
			return sizeof(BackdropConstants);
		}

		case ShaderKind::Solid:
		case ShaderKind::Textured:
		case ShaderKind::ColorPicker:
		case ShaderKind::Count: {
			break;
		}
	}

	return 0;
}

Renderer::Renderer() = default;

Renderer::~Renderer() = default;

auto Renderer::init(const os::Window* t_window, GraphicsApi t_preferred_api) -> bool
{
	const GraphicsApi fallback_api = t_preferred_api == GraphicsApi::OpenGl ? GraphicsApi::Native : GraphicsApi::OpenGl;
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
	if (width == 0 || height == 0 || (width == m_physical_width && height == m_physical_height)) return;

	m_backend->resize(width, height);
	remember_size(t_window);
}

auto Renderer::render(const DrawList* t_draw_list, Color t_clear_color) -> void
{
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

auto Renderer::create_texture(std::span<const TextureLevel> t_levels, bool t_updatable) -> u32
{
	const u32 slot = allocate_texture_slot();
	if (slot == K_INVALID_TEXTURE_SLOT) return K_INVALID_TEXTURE_SLOT;

	if (!m_backend->create_texture(slot, t_levels, t_updatable)) {
		release_texture_slot(slot);
		return K_INVALID_TEXTURE_SLOT;
	}

	return slot;
}

auto Renderer::update_texture(u32 t_slot, u32 t_x, u32 t_y, u32 t_width, u32 t_height, const u8* t_rgba_pixels) -> void
{
	m_backend->update_texture(t_slot, t_x, t_y, t_width, t_height, t_rgba_pixels);
}

auto Renderer::destroy_texture(u32 t_slot) -> void
{
	m_backend->destroy_texture(t_slot);
	release_texture_slot(t_slot);
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
