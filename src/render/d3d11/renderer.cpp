#include "render/render_backend.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

#include <Windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include "core/debug_log.h"
#include "core/profiler.h"
#include "gfx/draw_list.h"
#include "os/system.h"
#include "os/window.h"
#include "render/d3d11/shaders.h"

using Microsoft::WRL::ComPtr;

namespace {
constexpr const char* K_LOG_CATEGORY = "gfx";

constexpr u32  K_INITIAL_VERTEX_CAPACITY = 1024;
constexpr u32  K_INITIAL_INDEX_CAPACITY  = 1536;
constexpr UINT K_MSAA_SAMPLE_COUNT       = 4;

constexpr const char* K_PIXEL_SHADER_ENTRY_POINTS[]{
	"ps_solid", "ps_textured", "ps_banner_glow", "ps_color_picker", "ps_shadow", "ps_outline_countdown", "ps_backdrop", "ps_backdrop_plain", "ps_backdrop_blur",
};

static_assert(std::size(K_PIXEL_SHADER_ENTRY_POINTS) == K_SHADER_KIND_COUNT);

[[nodiscard]] auto compile_shader(const char* t_entry_point, const char* t_target, ComPtr<ID3DBlob>* t_out_blob) -> bool
{
	UINT flags = 0;
#ifndef NDEBUG
	flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	ComPtr<ID3DBlob> errors;
	const HRESULT    result = D3DCompile(K_SHADER_SOURCE, std::strlen(K_SHADER_SOURCE), nullptr, nullptr, nullptr, t_entry_point, t_target, flags, 0,
	                                     t_out_blob->ReleaseAndGetAddressOf(), &errors);

	if (FAILED(result) && errors != nullptr) {
		OutputDebugStringA(static_cast<const char*>(errors->GetBufferPointer()));
	}

	return SUCCEEDED(result);
}

template <typename Constants>
[[nodiscard]] auto create_constant_buffer(ID3D11Device* t_device, ComPtr<ID3D11Buffer>* t_out_buffer) -> bool
{
	const D3D11_BUFFER_DESC desc{
		.ByteWidth = sizeof(Constants),
		.Usage     = D3D11_USAGE_DEFAULT,
		.BindFlags = D3D11_BIND_CONSTANT_BUFFER,
	};

	return SUCCEEDED(t_device->CreateBuffer(&desc, nullptr, t_out_buffer->ReleaseAndGetAddressOf()));
}

[[nodiscard]] auto create_dynamic_buffer(ID3D11Device* t_device, UINT t_byte_width, UINT t_bind_flags, ComPtr<ID3D11Buffer>* t_out_buffer) -> bool
{
	const D3D11_BUFFER_DESC desc{
		.ByteWidth      = t_byte_width,
		.Usage          = D3D11_USAGE_DYNAMIC,
		.BindFlags      = t_bind_flags,
		.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
	};

	return SUCCEEDED(t_device->CreateBuffer(&desc, nullptr, t_out_buffer->ReleaseAndGetAddressOf()));
}

[[nodiscard]] auto means_device_lost(HRESULT t_result) -> bool
{
	return t_result == DXGI_ERROR_DEVICE_REMOVED || t_result == DXGI_ERROR_DEVICE_RESET || t_result == DXGI_ERROR_DEVICE_HUNG ||
	       t_result == DXGI_ERROR_DRIVER_INTERNAL_ERROR;
}

auto upload(ID3D11DeviceContext* t_context, ID3D11Buffer* t_buffer, const void* t_data, usize t_bytes) -> void
{
	D3D11_MAPPED_SUBRESOURCE mapped{};
	if (FAILED(t_context->Map(t_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return;

	std::memcpy(mapped.pData, t_data, t_bytes);
	t_context->Unmap(t_buffer, 0);
}

struct D3D11Backend final : RenderBackend {
	struct TextureSlot {
		ComPtr<ID3D11Texture2D>          texture;
		ComPtr<ID3D11ShaderResourceView> view;
	};

	struct BlurTarget {
		ComPtr<ID3D11Texture2D>          texture;
		ComPtr<ID3D11RenderTargetView>   target;
		ComPtr<ID3D11ShaderResourceView> view;
		u32                              width  = 0;
		u32                              height = 0;
	};

	ComPtr<ID3D11Device>           device;
	ComPtr<ID3D11DeviceContext>    context;
	ComPtr<IDXGISwapChain>         swap_chain;
	ComPtr<ID3D11Texture2D>        back_buffer;
	ComPtr<ID3D11RenderTargetView> render_target_view;

	ComPtr<ID3D11VertexShader> vertex_shader;
	ComPtr<ID3D11PixelShader>  pixel_shaders[K_SHADER_KIND_COUNT];
	ComPtr<ID3D11InputLayout>  input_layout;
	ComPtr<ID3D11VertexShader> fullscreen_shader;
	ComPtr<ID3D11PixelShader>  downsample_shader;
	ComPtr<ID3D11PixelShader>  blur_shader;

	ComPtr<ID3D11Buffer> viewport_constants;
	ComPtr<ID3D11Buffer> banner_glow_constants;
	ComPtr<ID3D11Buffer> shadow_constants;
	ComPtr<ID3D11Buffer> outline_countdown_constants;
	ComPtr<ID3D11Buffer> backdrop_constants;
	ComPtr<ID3D11Buffer> blur_constants;

	ComPtr<ID3D11BlendState>      blend_state;
	ComPtr<ID3D11RasterizerState> rasterizer_state;
	ComPtr<ID3D11SamplerState>    sampler_state;

	ComPtr<ID3D11Buffer> vertex_buffer;
	ComPtr<ID3D11Buffer> index_buffer;
	u32                  vertex_capacity = 0;
	u32                  index_capacity  = 0;

	TextureSlot textures[Renderer::K_MAX_TEXTURES];

	// The frame so far, then halved level by level. Each level has a second target the blur passes bounce through.
	BlurTarget capture;
	BlurTarget levels[K_BLUR_LEVEL_COUNT][2];
	bool       blur_ready = false;

	// Set once the device is gone, after which nothing more is sent to it. The Renderer builds a new backend.
	bool device_lost = false;

	[[nodiscard]] auto init(const os::Window* t_window) -> bool override;
	auto resize(u32 t_physical_width, u32 t_physical_height) -> void override;
	auto render(const RenderFrame& t_frame) -> void override;

	[[nodiscard]] auto supports_backdrop_blur() const -> bool override
	{
		return blur_ready;
	}

	[[nodiscard]] auto is_device_lost() const -> bool override
	{
		return device_lost;
	}

	[[nodiscard]] auto create_texture(u32 t_slot, std::span<const TextureLevel> t_levels, bool t_updatable) -> bool override;
	auto update_texture(u32 t_slot, u32 t_x, u32 t_y, u32 t_width, u32 t_height, const u8* t_rgba_pixels) -> void override;
	auto destroy_texture(u32 t_slot) -> void override;

	auto create_render_target_view() -> bool;
	[[nodiscard]] auto create_shaders() -> bool;
	[[nodiscard]] auto create_constant_buffers() -> bool;
	[[nodiscard]] auto create_pipeline_states() -> bool;
	auto create_vertex_buffer(u32 t_capacity) -> bool;
	auto create_index_buffer(u32 t_capacity) -> bool;
	auto set_viewport(u32 t_width, u32 t_height) const -> void;
	[[nodiscard]] auto create_blur_target(u32 t_width, u32 t_height, bool t_renderable, BlurTarget* t_target) const -> bool;
	auto size_blur_targets(u32 t_width, u32 t_height) -> void;
	auto blur_frame(const RenderFrame& t_frame, BlurPlan t_plan) -> void;

	[[nodiscard]] auto upload_geometry(const DrawList* t_draw_list) -> bool;
	auto bind_shared_state(const RenderFrame& t_frame) const -> void;
	auto apply_clip(const RenderFrame& t_frame, const DrawCommand& t_command) const -> void;
	auto draw_command(const RenderFrame& t_frame, const DrawCommand& t_command) -> void;
};

auto D3D11Backend::init(const os::Window* t_window) -> bool
{
	DXGI_SWAP_CHAIN_DESC swap_chain_desc{
		.BufferDesc =
			{
				.Width       = t_window->physical_width(),
				.Height      = t_window->physical_height(),
				.RefreshRate = {.Numerator = 0, .Denominator = 1},
				.Format      = DXGI_FORMAT_R8G8B8A8_UNORM,
			},
		.SampleDesc   = {.Count = K_MSAA_SAMPLE_COUNT, .Quality = 0},
		.BufferUsage  = DXGI_USAGE_RENDER_TARGET_OUTPUT,
		.BufferCount  = 2,
		.OutputWindow = static_cast<HWND>(t_window->native_handle()),
		.Windowed     = TRUE,
		.SwapEffect   = DXGI_SWAP_EFFECT_DISCARD,
	};

	UINT device_flags = 0;
#ifndef NDEBUG
	if (os::environment_variable("PULSAR_D3D_DEBUG")) {
		device_flags |= D3D11_CREATE_DEVICE_DEBUG;
		debug_log::write(K_LOG_CATEGORY, "D3D11 debug layer enabled by PULSAR_D3D_DEBUG");
	}
#endif

	const D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;
	const auto              device_start  = std::chrono::steady_clock::now();

	if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, device_flags, &feature_level, 1, D3D11_SDK_VERSION, &swap_chain_desc,
	                                         &swap_chain, &device, nullptr, &context))) {
		return false;
	}

	const auto pipeline_start = std::chrono::steady_clock::now();

	const bool ready = create_render_target_view() && create_shaders() && create_constant_buffers() && create_pipeline_states() &&
	                   create_vertex_buffer(K_INITIAL_VERTEX_CAPACITY) && create_index_buffer(K_INITIAL_INDEX_CAPACITY);
	if (!ready) return false;

	debug_log::write(K_LOG_CATEGORY, "device %.1f ms, pipeline %.1f ms", std::chrono::duration<float, std::milli>(pipeline_start - device_start).count(),
	                 std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - pipeline_start).count());

	set_viewport(t_window->physical_width(), t_window->physical_height());
	size_blur_targets(t_window->physical_width(), t_window->physical_height());

	return true;
}

auto D3D11Backend::resize(u32 t_physical_width, u32 t_physical_height) -> void
{
	if (device_lost) return;

	render_target_view.Reset();
	back_buffer.Reset();

	if (FAILED(swap_chain->ResizeBuffers(0, t_physical_width, t_physical_height, DXGI_FORMAT_UNKNOWN, 0)) || !create_render_target_view()) {
		device_lost = true;
		return;
	}

	set_viewport(t_physical_width, t_physical_height);
	size_blur_targets(t_physical_width, t_physical_height);
}

auto D3D11Backend::render(const RenderFrame& t_frame) -> void
{
	if (device_lost) return;

	ID3D11RenderTargetView* const render_target = render_target_view.Get();
	if (render_target == nullptr) {
		device_lost = true;
		return;
	}

	context->OMSetRenderTargets(1, &render_target, nullptr);

	const Color clear = t_frame.clear_color;
	const float clear_color[4]{clear.r / 255.0f, clear.g / 255.0f, clear.b / 255.0f, clear.a / 255.0f};
	context->ClearRenderTargetView(render_target, clear_color);

	{
		PULSAR_PROFILE_SCOPE("Render.Submit");

		if (!t_frame.draw_list->commands().empty()) {
			if (!upload_geometry(t_frame.draw_list)) {
				device_lost = true;
				return;
			}

			bind_shared_state(t_frame);

			// Glass needs a fresh blur of what is drawn before it, but a run of glass shapes that blur alike can share one.
			std::optional<BlurPlan> blurred;

			for (const DrawCommand& command : t_frame.draw_list->commands()) {
				if (command.shader != ShaderKind::BACKDROP_BLUR) {
					blurred.reset();
				} else if (!blur_ready) {
					continue;
				} else if (const BlurPlan plan = blur_plan(t_frame, command); blurred != plan) {
					blur_frame(t_frame, plan);
					blurred = plan;
				}

				draw_command(t_frame, command);
			}
		}
	}

	{
		PULSAR_PROFILE_SCOPE("Render.Present");
		if (means_device_lost(swap_chain->Present(1, 0))) {
			device_lost = true;
		}
	}
}

auto D3D11Backend::create_texture(u32 t_slot, std::span<const TextureLevel> t_levels, bool t_updatable) -> bool
{
	const D3D11_TEXTURE2D_DESC desc{
		.Width      = t_levels.front().width,
		.Height     = t_levels.front().height,
		.MipLevels  = static_cast<UINT>(t_levels.size()),
		.ArraySize  = 1,
		.Format     = DXGI_FORMAT_R8G8B8A8_UNORM,
		.SampleDesc = {.Count = 1, .Quality = 0},
		.Usage      = t_updatable ? D3D11_USAGE_DEFAULT : D3D11_USAGE_IMMUTABLE,
		.BindFlags  = D3D11_BIND_SHADER_RESOURCE,
	};

	std::vector<D3D11_SUBRESOURCE_DATA> pixels;
	pixels.reserve(t_levels.size());

	for (const TextureLevel& level : t_levels) {
		pixels.push_back(D3D11_SUBRESOURCE_DATA{.pSysMem = level.rgba_pixels, .SysMemPitch = level.width * 4});
	}

	TextureSlot* texture = &textures[t_slot];
	if (FAILED(device->CreateTexture2D(&desc, pixels.data(), &texture->texture)) ||
	    FAILED(device->CreateShaderResourceView(texture->texture.Get(), nullptr, &texture->view))) {
		*texture = TextureSlot{};
		return false;
	}

	return true;
}

auto D3D11Backend::update_texture(u32 t_slot, u32 t_x, u32 t_y, u32 t_width, u32 t_height, const u8* t_rgba_pixels) -> void
{
	ID3D11Texture2D* const texture = textures[t_slot].texture.Get();
	if (device_lost || texture == nullptr) return;

	const D3D11_BOX region{.left = t_x, .top = t_y, .front = 0, .right = t_x + t_width, .bottom = t_y + t_height, .back = 1};

	context->UpdateSubresource(texture, 0, &region, t_rgba_pixels, t_width * 4, 0);
}

auto D3D11Backend::destroy_texture(u32 t_slot) -> void
{
	textures[t_slot] = TextureSlot{};
}

auto D3D11Backend::set_viewport(u32 t_width, u32 t_height) const -> void
{
	const D3D11_VIEWPORT viewport{
		.Width    = static_cast<float>(t_width),
		.Height   = static_cast<float>(t_height),
		.MaxDepth = 1.0f,
	};

	context->RSSetViewports(1, &viewport);
}

auto D3D11Backend::create_blur_target(u32 t_width, u32 t_height, bool t_renderable, BlurTarget* t_target) const -> bool
{
	*t_target = BlurTarget{.width = std::max<u32>(t_width, 1), .height = std::max<u32>(t_height, 1)};

	const D3D11_TEXTURE2D_DESC desc{
		.Width      = t_target->width,
		.Height     = t_target->height,
		.MipLevels  = 1,
		.ArraySize  = 1,
		.Format     = DXGI_FORMAT_R8G8B8A8_UNORM,
		.SampleDesc = {.Count = 1, .Quality = 0},
		.Usage      = D3D11_USAGE_DEFAULT,
		.BindFlags  = D3D11_BIND_SHADER_RESOURCE | (t_renderable ? D3D11_BIND_RENDER_TARGET : 0u),
	};

	if (FAILED(device->CreateTexture2D(&desc, nullptr, &t_target->texture))) return false;
	if (FAILED(device->CreateShaderResourceView(t_target->texture.Get(), nullptr, &t_target->view))) return false;

	return !t_renderable || SUCCEEDED(device->CreateRenderTargetView(t_target->texture.Get(), nullptr, &t_target->target));
}

auto D3D11Backend::size_blur_targets(u32 t_width, u32 t_height) -> void
{
	blur_ready = create_blur_target(t_width, t_height, false, &capture);

	for (u32 level = 0; level < K_BLUR_LEVEL_COUNT; level += 1) {
		const u32 width  = blur_level_extent(t_width, level);
		const u32 height = blur_level_extent(t_height, level);

		for (BlurTarget& target : levels[level]) {
			blur_ready = blur_ready && create_blur_target(width, height, true, &target);
		}
	}

	if (!blur_ready) {
		debug_log::write(K_LOG_CATEGORY, "glass blur targets could not be made - glass draws without blur");
	}
}

// Resolves what is drawn so far, halves it down to the plan's level and blurs it there, then puts the frame's own state back.
auto D3D11Backend::blur_frame(const RenderFrame& t_frame, BlurPlan t_plan) -> void
{
	PULSAR_PROFILE_SCOPE("Render.Blur");

	if constexpr (K_MSAA_SAMPLE_COUNT > 1) {
		context->ResolveSubresource(capture.texture.Get(), 0, back_buffer.Get(), 0, DXGI_FORMAT_R8G8B8A8_UNORM);
	} else {
		context->CopyResource(capture.texture.Get(), back_buffer.Get());
	}

	ID3D11ShaderResourceView* const no_view  = nullptr;
	ID3D11Buffer* const             settings = blur_constants.Get();

	context->IASetInputLayout(nullptr);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->VSSetShader(fullscreen_shader.Get(), nullptr, 0);
	context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
	context->PSSetConstantBuffers(1, 1, &settings);

	const auto pass = [&](const BlurTarget& t_target, const BlurTarget& t_source, ID3D11PixelShader* t_shader, float t_step_x, float t_step_y) {
		ID3D11RenderTargetView* const   target = t_target.target.Get();
		ID3D11ShaderResourceView* const source = t_source.view.Get();
		const D3D11_RECT                scissor{0, 0, static_cast<LONG>(t_target.width), static_cast<LONG>(t_target.height)};
		const BlurConstants             constants{.step_x = t_step_x, .step_y = t_step_y};

		context->PSSetShaderResources(0, 1, &no_view);
		context->OMSetRenderTargets(1, &target, nullptr);
		set_viewport(t_target.width, t_target.height);
		context->RSSetScissorRects(1, &scissor);
		context->UpdateSubresource(blur_constants.Get(), 0, nullptr, &constants, 0, 0);
		context->PSSetShader(t_shader, nullptr, 0);
		context->PSSetShaderResources(0, 1, &source);
		context->Draw(3, 0);
	};

	const BlurTarget* source = &capture;

	for (u32 level = 0; level <= t_plan.level; level += 1) {
		pass(levels[level][0], *source, downsample_shader.Get(), 1.0f / static_cast<float>(source->width), 1.0f / static_cast<float>(source->height));
		source = &levels[level][0];
	}

	const BlurTarget* blurred = levels[t_plan.level];
	const float       step_x  = t_plan.step / static_cast<float>(blurred[0].width);
	const float       step_y  = t_plan.step / static_cast<float>(blurred[0].height);

	for (u32 i = 0; i < t_plan.iterations; i += 1) {
		pass(blurred[1], blurred[0], blur_shader.Get(), step_x, 0.0f);
		pass(blurred[0], blurred[1], blur_shader.Get(), 0.0f, step_y);
	}

	context->PSSetShaderResources(0, 1, &no_view);

	ID3D11RenderTargetView* const frame_target = render_target_view.Get();
	context->OMSetRenderTargets(1, &frame_target, nullptr);
	set_viewport(t_frame.physical_width, t_frame.physical_height);
	bind_shared_state(t_frame);
}

auto D3D11Backend::create_render_target_view() -> bool
{
	if (FAILED(swap_chain->GetBuffer(0, IID_PPV_ARGS(back_buffer.ReleaseAndGetAddressOf())))) return false;

	return SUCCEEDED(device->CreateRenderTargetView(back_buffer.Get(), nullptr, &render_target_view));
}

auto D3D11Backend::create_shaders() -> bool
{
	struct CompileJob {
		const char*      entry_point;
		const char*      target;
		ComPtr<ID3DBlob> blob;
		bool             compiled = false;
	};

	CompileJob vertex_job{"vs_main", "vs_5_0"};
	CompileJob fullscreen_job{"vs_fullscreen", "vs_5_0"};
	CompileJob downsample_job{"ps_downsample", "ps_5_0"};
	CompileJob blur_job{"ps_blur", "ps_5_0"};
	CompileJob pixel_jobs[K_SHADER_KIND_COUNT];
	for (u32 i = 0; i < K_SHADER_KIND_COUNT; i += 1) {
		pixel_jobs[i] = CompileJob{K_PIXEL_SHADER_ENTRY_POINTS[i], "ps_5_0"};
	}

	std::vector<std::thread> compilers;
	const auto               compile = [&compilers](CompileJob* t_job) {
		compilers.emplace_back([t_job]() { t_job->compiled = compile_shader(t_job->entry_point, t_job->target, &t_job->blob); });
	};

	compile(&vertex_job);
	compile(&fullscreen_job);
	compile(&downsample_job);
	compile(&blur_job);
	for (CompileJob& job : pixel_jobs) {
		compile(&job);
	}

	for (std::thread& compiler : compilers) {
		compiler.join();
	}

	if (!vertex_job.compiled || !std::ranges::all_of(pixel_jobs, &CompileJob::compiled)) return false;
	if (!fullscreen_job.compiled || !downsample_job.compiled || !blur_job.compiled) return false;

	const auto bytecode = [](const CompileJob& t_job) { return std::pair{t_job.blob->GetBufferPointer(), t_job.blob->GetBufferSize()}; };
	const auto [fullscreen_code, fullscreen_size] = bytecode(fullscreen_job);
	const auto [downsample_code, downsample_size] = bytecode(downsample_job);
	const auto [blur_code, blur_size]             = bytecode(blur_job);

	if (FAILED(device->CreateVertexShader(fullscreen_code, fullscreen_size, nullptr, &fullscreen_shader)) ||
	    FAILED(device->CreatePixelShader(downsample_code, downsample_size, nullptr, &downsample_shader)) ||
	    FAILED(device->CreatePixelShader(blur_code, blur_size, nullptr, &blur_shader))) {
		return false;
	}

	ID3DBlob* vertex_blob = vertex_job.blob.Get();
	if (FAILED(device->CreateVertexShader(vertex_blob->GetBufferPointer(), vertex_blob->GetBufferSize(), nullptr, &vertex_shader))) {
		return false;
	}

	for (u32 i = 0; i < K_SHADER_KIND_COUNT; i += 1) {
		ID3DBlob* blob = pixel_jobs[i].blob.Get();

		if (FAILED(device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &pixel_shaders[i]))) {
			return false;
		}
	}

	const D3D11_INPUT_ELEMENT_DESC vertex_layout[]{
		{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex2D, x), D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex2D, u), D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(Vertex2D, color), D3D11_INPUT_PER_VERTEX_DATA, 0},
	};

	return SUCCEEDED(
		device->CreateInputLayout(vertex_layout, ARRAYSIZE(vertex_layout), vertex_blob->GetBufferPointer(), vertex_blob->GetBufferSize(), &input_layout));
}

auto D3D11Backend::create_constant_buffers() -> bool
{
	return create_constant_buffer<ViewportConstants>(device.Get(), &viewport_constants) &&
	       create_constant_buffer<BannerGlowConstants>(device.Get(), &banner_glow_constants) &&
	       create_constant_buffer<ShadowConstants>(device.Get(), &shadow_constants) &&
	       create_constant_buffer<OutlineCountdownConstants>(device.Get(), &outline_countdown_constants) &&
	       create_constant_buffer<BackdropConstants>(device.Get(), &backdrop_constants) && create_constant_buffer<BlurConstants>(device.Get(), &blur_constants);
}

auto D3D11Backend::create_pipeline_states() -> bool
{
	D3D11_BLEND_DESC blend_desc{};
	blend_desc.RenderTarget[0] = {
		.BlendEnable           = TRUE,
		.SrcBlend              = D3D11_BLEND_SRC_ALPHA,
		.DestBlend             = D3D11_BLEND_INV_SRC_ALPHA,
		.BlendOp               = D3D11_BLEND_OP_ADD,
		.SrcBlendAlpha         = D3D11_BLEND_ONE,
		.DestBlendAlpha        = D3D11_BLEND_INV_SRC_ALPHA,
		.BlendOpAlpha          = D3D11_BLEND_OP_ADD,
		.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL,
	};

	const D3D11_RASTERIZER_DESC rasterizer_desc{
		.FillMode      = D3D11_FILL_SOLID,
		.CullMode      = D3D11_CULL_NONE,
		.ScissorEnable = TRUE,
	};

	const D3D11_SAMPLER_DESC sampler_desc{
		.Filter        = D3D11_FILTER_MIN_MAG_MIP_LINEAR,
		.AddressU      = D3D11_TEXTURE_ADDRESS_CLAMP,
		.AddressV      = D3D11_TEXTURE_ADDRESS_CLAMP,
		.AddressW      = D3D11_TEXTURE_ADDRESS_CLAMP,
		.MaxAnisotropy = 1,
		.MaxLOD        = D3D11_FLOAT32_MAX,
	};

	return SUCCEEDED(device->CreateBlendState(&blend_desc, &blend_state)) && SUCCEEDED(device->CreateRasterizerState(&rasterizer_desc, &rasterizer_state)) &&
	       SUCCEEDED(device->CreateSamplerState(&sampler_desc, &sampler_state));
}

auto D3D11Backend::create_vertex_buffer(u32 t_capacity) -> bool
{
	if (!create_dynamic_buffer(device.Get(), t_capacity * sizeof(Vertex2D), D3D11_BIND_VERTEX_BUFFER, &vertex_buffer)) {
		return false;
	}

	vertex_capacity = t_capacity;

	return true;
}

auto D3D11Backend::create_index_buffer(u32 t_capacity) -> bool
{
	if (!create_dynamic_buffer(device.Get(), t_capacity * sizeof(u32), D3D11_BIND_INDEX_BUFFER, &index_buffer)) {
		return false;
	}

	index_capacity = t_capacity;

	return true;
}

auto D3D11Backend::upload_geometry(const DrawList* t_draw_list) -> bool
{
	const auto vertices = t_draw_list->vertices();
	const auto indices  = t_draw_list->indices();

	if (vertices.size() > vertex_capacity && !create_vertex_buffer(static_cast<u32>(vertices.size()) * 2)) return false;
	if (indices.size() > index_capacity && !create_index_buffer(static_cast<u32>(indices.size()) * 2)) return false;

	upload(context.Get(), vertex_buffer.Get(), vertices.data(), vertices.size_bytes());
	upload(context.Get(), index_buffer.Get(), indices.data(), indices.size_bytes());

	return true;
}

auto D3D11Backend::bind_shared_state(const RenderFrame& t_frame) const -> void
{
	const ViewportConstants viewport = ::viewport_constants(t_frame);
	context->UpdateSubresource(viewport_constants.Get(), 0, nullptr, &viewport, 0, 0);

	const UINT                stride    = sizeof(Vertex2D);
	const UINT                offset    = 0;
	ID3D11Buffer* const       vertices  = vertex_buffer.Get();
	ID3D11Buffer* const       constants = viewport_constants.Get();
	ID3D11SamplerState* const sampler   = sampler_state.Get();

	context->IASetVertexBuffers(0, 1, &vertices, &stride, &offset);
	context->IASetIndexBuffer(index_buffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	context->IASetInputLayout(input_layout.Get());
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->VSSetShader(vertex_shader.Get(), nullptr, 0);
	context->VSSetConstantBuffers(0, 1, &constants);
	context->PSSetSamplers(0, 1, &sampler);
	context->OMSetBlendState(blend_state.Get(), nullptr, 0xFFFFFFFF);
	context->RSSetState(rasterizer_state.Get());
}

auto D3D11Backend::apply_clip(const RenderFrame& t_frame, const DrawCommand& t_command) const -> void
{
	const ScissorRect clip = scissor_for(t_frame, t_command);
	const D3D11_RECT  scissor{clip.left, clip.top, clip.right, clip.bottom};

	context->RSSetScissorRects(1, &scissor);
}

auto D3D11Backend::draw_command(const RenderFrame& t_frame, const DrawCommand& t_command) -> void
{
	ID3D11ShaderResourceView* image           = nullptr;
	ID3D11Buffer*             extra_constants = nullptr;
	EffectConstants           effect{};
	const usize               effect_size = effect_constants(t_frame, t_command, &effect);

	switch (t_command.shader) {
		using enum ShaderKind;

		case SOLID:
		case COLOR_PICKER:
		case COUNT: {
			break;
		}

		case TEXTURED: {
			if (!t_command.texture->is_valid()) return;

			image = textures[t_command.texture->slot()].view.Get();
			break;
		}

		case BANNER_GLOW: {
			extra_constants = banner_glow_constants.Get();
			break;
		}

		case SHADOW: {
			extra_constants = shadow_constants.Get();
			break;
		}

		case OUTLINE_COUNTDOWN: {
			extra_constants = outline_countdown_constants.Get();
			break;
		}

		case BACKDROP:
		case BACKDROP_PLAIN: {
			extra_constants = backdrop_constants.Get();
			break;
		}

		case BACKDROP_BLUR: {
			image           = levels[blur_plan(t_frame, t_command).level][0].view.Get();
			extra_constants = blur_constants.Get();
			break;
		}
	}

	if (extra_constants != nullptr && effect_size > 0) {
		context->UpdateSubresource(extra_constants, 0, nullptr, &effect, 0, 0);
	}

	context->PSSetShader(pixel_shaders[static_cast<u32>(t_command.shader)].Get(), nullptr, 0);
	context->PSSetShaderResources(0, 1, &image);

	if (extra_constants != nullptr) {
		context->PSSetConstantBuffers(1, 1, &extra_constants);
	}

	apply_clip(t_frame, t_command);
	context->DrawIndexed(t_command.index_count, t_command.index_offset, 0);
}
}

auto create_native_render_backend() -> std::unique_ptr<RenderBackend>
{
	return std::make_unique<D3D11Backend>();
}

auto native_render_backend_name() -> std::string_view
{
	return "Direct3D 11";
}
