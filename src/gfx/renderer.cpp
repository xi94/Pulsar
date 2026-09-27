#include "gfx/renderer.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

#include <d3dcompiler.h>

#include "core/debug_log.h"
#include "core/profiler.h"
#include "gfx/draw_list.h"
#include "gfx/shaders.h"
#include "platform/window.h"

namespace {
constexpr const char *log_category = "gfx";

struct ViewportConstants {
	float width;
	float height;
	float padding[2];
};

struct BannerGlowConstants {
	float time_seconds;
	RoundedBoxParams params;
	float padding[3];
};

struct ShadowConstants {
	RoundedBoxParams params;
};

static_assert(sizeof(ViewportConstants) == 16);
static_assert(sizeof(BannerGlowConstants) == 32);
static_assert(sizeof(ShadowConstants) == 16);
static_assert(sizeof(CircularProgressParams) == 32);

struct CircularProgressConstants {
	CircularProgressParams params;
};

static_assert(sizeof(CircularProgressConstants) == 32);

struct OutlineCountdownConstants {
	OutlineCountdownParams params;
};

static_assert(sizeof(OutlineCountdownConstants) == 48);

bool compile_shader(const char *t_entry_point, const char *t_target, Microsoft::WRL::ComPtr<ID3DBlob> &t_out_blob)
{
	UINT flags = 0;
#ifndef NDEBUG
	flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	Microsoft::WRL::ComPtr<ID3DBlob> errors;
	const HRESULT result = D3DCompile(shader_source, std::strlen(shader_source), nullptr, nullptr, nullptr,
									  t_entry_point, t_target, flags, 0, &t_out_blob, &errors);

	if (FAILED(result) && errors != nullptr) {
		OutputDebugStringA(static_cast<const char *>(errors->GetBufferPointer()));
	}

	return SUCCEEDED(result);
}

template <typename Constants>
bool create_constant_buffer(ID3D11Device &t_device, Microsoft::WRL::ComPtr<ID3D11Buffer> &t_out_buffer)
{
	const D3D11_BUFFER_DESC desc{
		.ByteWidth = sizeof(Constants),
		.Usage = D3D11_USAGE_DEFAULT,
		.BindFlags = D3D11_BIND_CONSTANT_BUFFER,
	};

	return SUCCEEDED(t_device.CreateBuffer(&desc, nullptr, &t_out_buffer));
}

bool create_dynamic_buffer(ID3D11Device &t_device, UINT t_byte_width, UINT t_bind_flags,
						   Microsoft::WRL::ComPtr<ID3D11Buffer> &t_out_buffer)
{
	const D3D11_BUFFER_DESC desc{
		.ByteWidth = t_byte_width,
		.Usage = D3D11_USAGE_DYNAMIC,
		.BindFlags = t_bind_flags,
		.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
	};

	return SUCCEEDED(t_device.CreateBuffer(&desc, nullptr, &t_out_buffer));
}

void upload(ID3D11DeviceContext &t_context, ID3D11Buffer *t_buffer, const void *t_data, usize t_bytes)
{
	D3D11_MAPPED_SUBRESOURCE mapped{};
	if (FAILED(t_context.Map(t_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return;

	std::memcpy(mapped.pData, t_data, t_bytes);
	t_context.Unmap(t_buffer, 0);
}
}

Texture::Texture(Renderer &t_renderer, std::span<const TextureLevel> t_levels)
	: m_renderer(t_renderer)
	, m_slot(t_renderer.create_texture(t_levels))
	, m_width(t_levels.front().width)
	, m_height(t_levels.front().height)
{
}

Texture::~Texture()
{
	if (is_valid()) {
		m_renderer.destroy_texture(m_slot);
	}
}

bool Texture::is_valid() const
{
	return m_slot != Renderer::invalid_texture_slot;
}

bool Renderer::init(const Window &t_window)
{
	DXGI_SWAP_CHAIN_DESC swap_chain_desc{
		.BufferDesc =
			{
				.Width = t_window.physical_width(),
				.Height = t_window.physical_height(),
				.RefreshRate = {.Numerator = 0, .Denominator = 1},
				.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
			},
		.SampleDesc = {.Count = msaa_sample_count, .Quality = 0},
		.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
		.BufferCount = 2,
		.OutputWindow = t_window.handle(),
		.Windowed = TRUE,
		.SwapEffect = DXGI_SWAP_EFFECT_DISCARD,
	};

	UINT device_flags = 0;
#ifndef NDEBUG
	if (GetEnvironmentVariableW(L"PULSAR_D3D_DEBUG", nullptr, 0) != 0) {
		device_flags |= D3D11_CREATE_DEVICE_DEBUG;
		debug_log::write(log_category, "D3D11 debug layer enabled by PULSAR_D3D_DEBUG");
	}
#endif

	const D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;
	const auto device_start = std::chrono::steady_clock::now();

	if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, device_flags, &feature_level,
											 1, D3D11_SDK_VERSION, &swap_chain_desc, &m_swap_chain, &m_device, nullptr,
											 &m_context))) {
		return false;
	}

	const auto pipeline_start = std::chrono::steady_clock::now();

	const bool ready = create_render_target_view() && create_shaders() && create_constant_buffers() &&
					   create_pipeline_states() && create_vertex_buffer(initial_vertex_capacity) &&
					   create_index_buffer(initial_index_capacity);
	if (!ready) return false;

	debug_log::write(
		log_category, "device %.1f ms, pipeline %.1f ms",
		std::chrono::duration<float, std::milli>(pipeline_start - device_start).count(),
		std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - pipeline_start).count());

	set_viewport(t_window.physical_width(), t_window.physical_height(), static_cast<float>(t_window.width()),
				 static_cast<float>(t_window.height()));

	return true;
}

void Renderer::resize(const Window &t_window)
{
	const u32 width = t_window.physical_width();
	const u32 height = t_window.physical_height();
	if (width == 0 || height == 0) return;

	m_render_target_view.Reset();
	m_swap_chain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
	create_render_target_view();

	set_viewport(width, height, static_cast<float>(t_window.width()), static_cast<float>(t_window.height()));
}

void Renderer::set_viewport(u32 t_width, u32 t_height, float t_logical_width, float t_logical_height)
{
	m_physical_width = t_width;
	m_physical_height = t_height;
	m_logical_width = t_logical_width;
	m_logical_height = t_logical_height;

	const D3D11_VIEWPORT viewport{
		.Width = static_cast<float>(t_width),
		.Height = static_cast<float>(t_height),
		.MaxDepth = 1.0f,
	};

	m_context->RSSetViewports(1, &viewport);
}

bool Renderer::create_render_target_view()
{
	ComPtr<ID3D11Texture2D> back_buffer;
	if (FAILED(m_swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer)))) return false;

	return SUCCEEDED(m_device->CreateRenderTargetView(back_buffer.Get(), nullptr, &m_render_target_view));
}

bool Renderer::create_shaders()
{
	struct CompileJob {
		const char *entry_point;
		const char *target;
		ComPtr<ID3DBlob> blob;
		bool compiled = false;
	};

	CompileJob jobs[]{
		{"vs_main", "vs_5_0"},		   {"ps_solid", "ps_5_0"},
		{"ps_textured", "ps_5_0"},	   {"ps_banner_glow", "ps_5_0"},
		{"ps_color_picker", "ps_5_0"}, {"ps_circular_progress", "ps_5_0"},
		{"ps_shadow", "ps_5_0"},	   {"ps_outline_countdown", "ps_5_0"},
	};

	std::vector<std::thread> compilers;
	for (CompileJob &job : jobs) {
		compilers.emplace_back([&job]() { job.compiled = compile_shader(job.entry_point, job.target, job.blob); });
	}

	for (std::thread &compiler : compilers) {
		compiler.join();
	}

	if (!std::ranges::all_of(jobs, &CompileJob::compiled)) return false;

	ID3DBlob &vertex_blob = *jobs[0].blob.Get();
	if (FAILED(m_device->CreateVertexShader(vertex_blob.GetBufferPointer(), vertex_blob.GetBufferSize(), nullptr,
											&m_vertex_shader))) {
		return false;
	}

	ComPtr<ID3D11PixelShader> *const pixel_shaders[]{
		&m_solid_shader,
		&m_textured_shader,
		&m_banner_glow_shader,
		&m_color_picker_shader,
		&m_circular_progress_shader,
		&m_shadow_shader,
		&m_outline_countdown_shader,
	};

	for (usize i = 0; i < std::size(pixel_shaders); i += 1) {
		ID3DBlob &blob = *jobs[i + 1].blob.Get();

		if (FAILED(m_device->CreatePixelShader(blob.GetBufferPointer(), blob.GetBufferSize(), nullptr,
											   pixel_shaders[i]->GetAddressOf()))) {
			return false;
		}
	}

	const D3D11_INPUT_ELEMENT_DESC vertex_layout[]{
		{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex2D, x), D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex2D, u), D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(Vertex2D, color), D3D11_INPUT_PER_VERTEX_DATA, 0},
	};

	return SUCCEEDED(m_device->CreateInputLayout(vertex_layout, ARRAYSIZE(vertex_layout),
												 vertex_blob.GetBufferPointer(), vertex_blob.GetBufferSize(),
												 &m_input_layout));
}

bool Renderer::create_constant_buffers()
{
	return create_constant_buffer<ViewportConstants>(*m_device.Get(), m_viewport_constants) &&
		   create_constant_buffer<BannerGlowConstants>(*m_device.Get(), m_banner_glow_constants) &&
		   create_constant_buffer<CircularProgressConstants>(*m_device.Get(), m_circular_progress_constants) &&
		   create_constant_buffer<ShadowConstants>(*m_device.Get(), m_shadow_constants) &&
		   create_constant_buffer<OutlineCountdownConstants>(*m_device.Get(), m_outline_countdown_constants);
}

bool Renderer::create_pipeline_states()
{
	D3D11_BLEND_DESC blend_desc{};
	blend_desc.RenderTarget[0] = {
		.BlendEnable = TRUE,
		.SrcBlend = D3D11_BLEND_SRC_ALPHA,
		.DestBlend = D3D11_BLEND_INV_SRC_ALPHA,
		.BlendOp = D3D11_BLEND_OP_ADD,
		.SrcBlendAlpha = D3D11_BLEND_ONE,
		.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA,
		.BlendOpAlpha = D3D11_BLEND_OP_ADD,
		.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL,
	};

	const D3D11_RASTERIZER_DESC rasterizer_desc{
		.FillMode = D3D11_FILL_SOLID,
		.CullMode = D3D11_CULL_NONE,
		.ScissorEnable = TRUE,
	};

	const D3D11_SAMPLER_DESC sampler_desc{
		.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR,
		.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP,
		.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP,
		.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP,
		.MaxAnisotropy = 1,
		.MaxLOD = D3D11_FLOAT32_MAX,
	};

	return SUCCEEDED(m_device->CreateBlendState(&blend_desc, &m_blend_state)) &&
		   SUCCEEDED(m_device->CreateRasterizerState(&rasterizer_desc, &m_rasterizer_state)) &&
		   SUCCEEDED(m_device->CreateSamplerState(&sampler_desc, &m_sampler_state));
}

bool Renderer::create_vertex_buffer(u32 t_capacity)
{
	if (!create_dynamic_buffer(*m_device.Get(), t_capacity * sizeof(Vertex2D), D3D11_BIND_VERTEX_BUFFER,
							   m_vertex_buffer)) {
		return false;
	}

	m_vertex_capacity = t_capacity;

	return true;
}

bool Renderer::create_index_buffer(u32 t_capacity)
{
	if (!create_dynamic_buffer(*m_device.Get(), t_capacity * sizeof(u32), D3D11_BIND_INDEX_BUFFER, m_index_buffer)) {
		return false;
	}

	m_index_capacity = t_capacity;

	return true;
}

void Renderer::upload_geometry(const DrawList &t_draw_list)
{
	const auto vertices = t_draw_list.vertices();
	const auto indices = t_draw_list.indices();

	if (vertices.size() > m_vertex_capacity) {
		create_vertex_buffer(static_cast<u32>(vertices.size()) * 2);
	}

	if (indices.size() > m_index_capacity) {
		create_index_buffer(static_cast<u32>(indices.size()) * 2);
	}

	upload(*m_context.Get(), m_vertex_buffer.Get(), vertices.data(), vertices.size_bytes());
	upload(*m_context.Get(), m_index_buffer.Get(), indices.data(), indices.size_bytes());
}

void Renderer::bind_shared_state()
{
	const ViewportConstants viewport{.width = m_logical_width, .height = m_logical_height};
	m_context->UpdateSubresource(m_viewport_constants.Get(), 0, nullptr, &viewport, 0, 0);

	const UINT stride = sizeof(Vertex2D);
	const UINT offset = 0;
	ID3D11Buffer *const vertex_buffer = m_vertex_buffer.Get();
	ID3D11Buffer *const viewport_constants = m_viewport_constants.Get();
	ID3D11SamplerState *const sampler = m_sampler_state.Get();

	m_context->IASetVertexBuffers(0, 1, &vertex_buffer, &stride, &offset);
	m_context->IASetIndexBuffer(m_index_buffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	m_context->IASetInputLayout(m_input_layout.Get());
	m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_context->VSSetShader(m_vertex_shader.Get(), nullptr, 0);
	m_context->VSSetConstantBuffers(0, 1, &viewport_constants);
	m_context->PSSetSamplers(0, 1, &sampler);
	m_context->OMSetBlendState(m_blend_state.Get(), nullptr, 0xFFFFFFFF);
	m_context->RSSetState(m_rasterizer_state.Get());
}

void Renderer::apply_clip(const DrawCommand &t_command)
{
	D3D11_RECT scissor{0, 0, static_cast<LONG>(m_physical_width), static_cast<LONG>(m_physical_height)};

	if (t_command.clipped) {
		const float scale = m_logical_width > 0.0f ? static_cast<float>(m_physical_width) / m_logical_width : 1.0f;
		const Rect &clip = t_command.clip;

		scissor.left = std::clamp(static_cast<LONG>(clip.x * scale), 0L, scissor.right);
		scissor.top = std::clamp(static_cast<LONG>(clip.y * scale), 0L, scissor.bottom);
		scissor.right = std::clamp(static_cast<LONG>(clip.right() * scale), scissor.left, scissor.right);
		scissor.bottom = std::clamp(static_cast<LONG>(clip.bottom() * scale), scissor.top, scissor.bottom);
	}

	m_context->RSSetScissorRects(1, &scissor);
}

void Renderer::draw_command(const DrawCommand &t_command)
{
	ID3D11PixelShader *shader = m_solid_shader.Get();
	ID3D11ShaderResourceView *image = nullptr;
	ID3D11Buffer *extra_constants = nullptr;

	switch (t_command.shader) {
		case ShaderKind::solid:
			break;

		case ShaderKind::textured:
			if (!t_command.texture->is_valid()) return;

			shader = m_textured_shader.Get();
			image = m_textures[t_command.texture->slot()].view.Get();
			break;

		case ShaderKind::banner_glow: {
			const BannerGlowConstants constants{.time_seconds = m_effect_time_seconds, .params = t_command.box};
			m_context->UpdateSubresource(m_banner_glow_constants.Get(), 0, nullptr, &constants, 0, 0);
			shader = m_banner_glow_shader.Get();
			extra_constants = m_banner_glow_constants.Get();
			break;
		}

		case ShaderKind::color_picker:
			shader = m_color_picker_shader.Get();
			break;

		case ShaderKind::circular_progress: {
			const CircularProgressConstants constants{.params = t_command.progress};
			m_context->UpdateSubresource(m_circular_progress_constants.Get(), 0, nullptr, &constants, 0, 0);
			shader = m_circular_progress_shader.Get();
			extra_constants = m_circular_progress_constants.Get();
			break;
		}

		case ShaderKind::shadow: {
			const ShadowConstants constants{.params = t_command.box};
			m_context->UpdateSubresource(m_shadow_constants.Get(), 0, nullptr, &constants, 0, 0);
			shader = m_shadow_shader.Get();
			extra_constants = m_shadow_constants.Get();
			break;
		}

		case ShaderKind::outline_countdown: {
			const OutlineCountdownConstants constants{.params = t_command.outline};
			m_context->UpdateSubresource(m_outline_countdown_constants.Get(), 0, nullptr, &constants, 0, 0);
			shader = m_outline_countdown_shader.Get();
			extra_constants = m_outline_countdown_constants.Get();
			break;
		}
	}

	m_context->PSSetShader(shader, nullptr, 0);
	m_context->PSSetShaderResources(0, 1, &image);

	if (extra_constants != nullptr) {
		m_context->PSSetConstantBuffers(1, 1, &extra_constants);
	}

	apply_clip(t_command);
	m_context->DrawIndexed(t_command.index_count, t_command.index_offset, 0);
}

void Renderer::render(const DrawList &t_draw_list, Color t_clear_color)
{
	ID3D11RenderTargetView *const render_target = m_render_target_view.Get();
	m_context->OMSetRenderTargets(1, &render_target, nullptr);

	const float clear_color[4]{t_clear_color.r / 255.0f, t_clear_color.g / 255.0f, t_clear_color.b / 255.0f,
							   t_clear_color.a / 255.0f};
	m_context->ClearRenderTargetView(render_target, clear_color);

	{
		PULSAR_PROFILE_SCOPE("Render.Submit");

		if (!t_draw_list.commands().empty()) {
			upload_geometry(t_draw_list);
			bind_shared_state();

			for (const DrawCommand &command : t_draw_list.commands()) {
				draw_command(command);
			}
		}
	}

	{
		PULSAR_PROFILE_SCOPE("Render.Present");
		m_swap_chain->Present(1, 0);
	}
}

u32 Renderer::create_texture(std::span<const TextureLevel> t_levels)
{
	u32 slot = invalid_texture_slot;
	if (m_free_texture_count > 0) {
		m_free_texture_count -= 1;
		slot = m_free_texture_slots[m_free_texture_count];
	} else {
		assert(m_texture_high_water < max_textures);
		slot = m_texture_high_water;
		m_texture_high_water += 1;
	}

	const D3D11_TEXTURE2D_DESC desc{
		.Width = t_levels.front().width,
		.Height = t_levels.front().height,
		.MipLevels = static_cast<UINT>(t_levels.size()),
		.ArraySize = 1,
		.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
		.SampleDesc = {.Count = 1, .Quality = 0},
		.Usage = D3D11_USAGE_IMMUTABLE,
		.BindFlags = D3D11_BIND_SHADER_RESOURCE,
	};

	std::vector<D3D11_SUBRESOURCE_DATA> pixels;
	pixels.reserve(t_levels.size());

	for (const TextureLevel &level : t_levels) {
		pixels.push_back(D3D11_SUBRESOURCE_DATA{.pSysMem = level.rgba_pixels, .SysMemPitch = level.width * 4});
	}

	TextureSlot &texture = m_textures[slot];
	if (FAILED(m_device->CreateTexture2D(&desc, pixels.data(), &texture.texture)) ||
		FAILED(m_device->CreateShaderResourceView(texture.texture.Get(), nullptr, &texture.view))) {
		destroy_texture(slot);
		return invalid_texture_slot;
	}

	return slot;
}

void Renderer::destroy_texture(u32 t_slot)
{
	assert(t_slot < m_texture_high_water && m_free_texture_count < max_textures);

	m_textures[t_slot] = TextureSlot{};
	m_free_texture_slots[m_free_texture_count] = t_slot;
	m_free_texture_count += 1;
}
