#pragma once

#include <span>

#include <d3d11.h>
#include <wrl/client.h>

#include "core/types.h"

class DrawList;
class Renderer;
class Window;
struct DrawCommand;

struct TextureLevel {
	const u8 *rgba_pixels;
	u32 width;
	u32 height;
};

class Texture {
  public:
	Texture(Renderer &t_renderer, std::span<const TextureLevel> t_levels);
	~Texture();

	Texture(const Texture &) = delete;
	Texture &operator=(const Texture &) = delete;

	bool is_valid() const;

	u32 slot() const
	{
		return m_slot;
	}

	u32 width() const
	{
		return m_width;
	}

	u32 height() const
	{
		return m_height;
	}

	float aspect() const
	{
		return m_height > 0 ? static_cast<float>(m_width) / static_cast<float>(m_height) : 1.0f;
	}

  private:
	Renderer &m_renderer;
	u32 m_slot;
	u32 m_width;
	u32 m_height;
};

class Renderer {
  public:
	static constexpr u32 invalid_texture_slot = 0xFFFFFFFFu;

	Renderer() = default;

	Renderer(const Renderer &) = delete;
	Renderer &operator=(const Renderer &) = delete;

	bool init(const Window &t_window);
	void resize(const Window &t_window);

	void set_effect_time(float t_seconds)
	{
		m_effect_time_seconds = t_seconds;
	}

	void render(const DrawList &t_draw_list, Color t_clear_color);

	u32 create_texture(std::span<const TextureLevel> t_levels);
	void destroy_texture(u32 t_slot);

  private:
	template <typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	struct TextureSlot {
		ComPtr<ID3D11Texture2D> texture;
		ComPtr<ID3D11ShaderResourceView> view;
	};

	static constexpr u32 max_textures = 64;
	static constexpr u32 initial_vertex_capacity = 1024;
	static constexpr u32 initial_index_capacity = 1536;
	static constexpr UINT msaa_sample_count = 4;

	bool create_render_target_view();
	bool create_shaders();
	bool create_constant_buffers();
	bool create_pipeline_states();
	bool create_vertex_buffer(u32 t_capacity);
	bool create_index_buffer(u32 t_capacity);
	void set_viewport(u32 t_width, u32 t_height, float t_logical_width, float t_logical_height);

	void upload_geometry(const DrawList &t_draw_list);
	void bind_shared_state();
	void apply_clip(const DrawCommand &t_command);
	void draw_command(const DrawCommand &t_command);

	ComPtr<ID3D11Device> m_device;
	ComPtr<ID3D11DeviceContext> m_context;
	ComPtr<IDXGISwapChain> m_swap_chain;
	ComPtr<ID3D11RenderTargetView> m_render_target_view;

	ComPtr<ID3D11VertexShader> m_vertex_shader;
	ComPtr<ID3D11PixelShader> m_solid_shader;
	ComPtr<ID3D11PixelShader> m_textured_shader;
	ComPtr<ID3D11PixelShader> m_banner_glow_shader;
	ComPtr<ID3D11PixelShader> m_color_picker_shader;
	ComPtr<ID3D11PixelShader> m_circular_progress_shader;
	ComPtr<ID3D11PixelShader> m_shadow_shader;
	ComPtr<ID3D11PixelShader> m_outline_countdown_shader;
	ComPtr<ID3D11InputLayout> m_input_layout;

	ComPtr<ID3D11Buffer> m_viewport_constants;
	ComPtr<ID3D11Buffer> m_banner_glow_constants;
	ComPtr<ID3D11Buffer> m_circular_progress_constants;
	ComPtr<ID3D11Buffer> m_shadow_constants;
	ComPtr<ID3D11Buffer> m_outline_countdown_constants;

	ComPtr<ID3D11BlendState> m_blend_state;
	ComPtr<ID3D11RasterizerState> m_rasterizer_state;
	ComPtr<ID3D11SamplerState> m_sampler_state;

	ComPtr<ID3D11Buffer> m_vertex_buffer;
	ComPtr<ID3D11Buffer> m_index_buffer;
	u32 m_vertex_capacity = 0;
	u32 m_index_capacity = 0;

	TextureSlot m_textures[max_textures];
	u32 m_free_texture_slots[max_textures]{};
	u32 m_free_texture_count = 0;
	u32 m_texture_high_water = 0;

	u32 m_physical_width = 0;
	u32 m_physical_height = 0;
	float m_logical_width = 0.0f;
	float m_logical_height = 0.0f;
	float m_effect_time_seconds = 0.0f;
};
