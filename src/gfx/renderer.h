#pragma once

#include <span>

#include <d3d11.h>
#include <wrl/client.h>

#include "core/types.h"
#include "gfx/draw_list.h"

class Renderer;
class Window;

struct TextureLevel {
	const u8* rgba_pixels;
	u32       width;
	u32       height;
};

class Texture {
  public:
	Texture(Renderer* t_renderer, std::span<const TextureLevel> t_levels, bool t_updatable = false);
	~Texture();

	Texture(const Texture&)                    = delete;
	auto operator=(const Texture&) -> Texture& = delete;

	[[nodiscard]] auto is_valid() const -> bool;
	auto update(u32 t_x, u32 t_y, u32 t_width, u32 t_height, const u8* t_rgba_pixels) -> void;

	[[nodiscard]] auto slot() const -> u32
	{
		return m_slot;
	}

	[[nodiscard]] auto width() const -> u32
	{
		return m_width;
	}

	[[nodiscard]] auto height() const -> u32
	{
		return m_height;
	}

	[[nodiscard]] auto aspect() const -> float
	{
		return m_height > 0 ? static_cast<float>(m_width) / static_cast<float>(m_height) : 1.0f;
	}

  private:
	Renderer* m_renderer;
	u32       m_slot;
	u32       m_width;
	u32       m_height;
};

class Renderer {
  public:
	static constexpr u32 K_INVALID_TEXTURE_SLOT = 0xFFFFFFFFu;

	Renderer() = default;

	Renderer(const Renderer&)                    = delete;
	auto operator=(const Renderer&) -> Renderer& = delete;

	[[nodiscard]] auto init(const Window* t_window) -> bool;
	auto resize(const Window* t_window) -> void;

	auto set_effect_time(float t_seconds) -> void
	{
		m_effect_time_seconds = t_seconds;
	}

	auto set_backdrop(u32 t_style, float t_intensity, float t_light, float t_grain) -> void
	{
		m_backdrop_style     = t_style;
		m_backdrop_intensity = t_intensity;
		m_backdrop_light     = t_light;
		m_backdrop_grain     = t_grain;
	}

	auto render(const DrawList* t_draw_list, Color t_clear_color) -> void;

	[[nodiscard]] auto create_texture(std::span<const TextureLevel> t_levels, bool t_updatable) -> u32;
	auto update_texture(u32 t_slot, u32 t_x, u32 t_y, u32 t_width, u32 t_height, const u8* t_rgba_pixels) -> void;
	auto destroy_texture(u32 t_slot) -> void;

  private:
	template <typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	struct TextureSlot {
		ComPtr<ID3D11Texture2D>          texture;
		ComPtr<ID3D11ShaderResourceView> view;
	};

	static constexpr u32  K_MAX_TEXTURES            = 64;
	static constexpr u32  K_INITIAL_VERTEX_CAPACITY = 1024;
	static constexpr u32  K_INITIAL_INDEX_CAPACITY  = 1536;
	static constexpr UINT K_MSAA_SAMPLE_COUNT       = 4;

	auto create_render_target_view() -> bool;
	[[nodiscard]] auto create_shaders() -> bool;
	[[nodiscard]] auto create_constant_buffers() -> bool;
	[[nodiscard]] auto create_pipeline_states() -> bool;
	auto create_vertex_buffer(u32 t_capacity) -> bool;
	auto create_index_buffer(u32 t_capacity) -> bool;
	auto set_viewport(u32 t_width, u32 t_height, float t_logical_width, float t_logical_height) -> void;

	auto upload_geometry(const DrawList* t_draw_list) -> void;
	auto bind_shared_state() -> void;
	auto apply_clip(const DrawCommand& t_command) -> void;
	auto draw_command(const DrawCommand& t_command) -> void;

	ComPtr<ID3D11Device>           m_device;
	ComPtr<ID3D11DeviceContext>    m_context;
	ComPtr<IDXGISwapChain>         m_swap_chain;
	ComPtr<ID3D11RenderTargetView> m_render_target_view;

	ComPtr<ID3D11VertexShader> m_vertex_shader;
	ComPtr<ID3D11PixelShader>  m_pixel_shaders[K_SHADER_KIND_COUNT];
	ComPtr<ID3D11InputLayout>  m_input_layout;

	ComPtr<ID3D11Buffer> m_viewport_constants;
	ComPtr<ID3D11Buffer> m_banner_glow_constants;
	ComPtr<ID3D11Buffer> m_shadow_constants;
	ComPtr<ID3D11Buffer> m_outline_countdown_constants;
	ComPtr<ID3D11Buffer> m_backdrop_constants;

	ComPtr<ID3D11BlendState>      m_blend_state;
	ComPtr<ID3D11RasterizerState> m_rasterizer_state;
	ComPtr<ID3D11SamplerState>    m_sampler_state;

	ComPtr<ID3D11Buffer> m_vertex_buffer;
	ComPtr<ID3D11Buffer> m_index_buffer;
	u32                  m_vertex_capacity = 0;
	u32                  m_index_capacity  = 0;

	TextureSlot m_textures[K_MAX_TEXTURES];
	u32         m_free_texture_slots[K_MAX_TEXTURES]{};
	u32         m_free_texture_count = 0;
	u32         m_texture_high_water = 0;

	u32   m_physical_width      = 0;
	u32   m_physical_height     = 0;
	float m_logical_width       = 0.0f;
	float m_logical_height      = 0.0f;
	float m_effect_time_seconds = 0.0f;
	u32   m_backdrop_style      = 0;
	float m_backdrop_intensity  = 0.0f;
	float m_backdrop_light      = 0.0f;
	float m_backdrop_grain      = 0.0f;
};
