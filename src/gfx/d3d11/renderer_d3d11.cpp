#include "gfx/d3d11/renderer_d3d11.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

#include <d3dcompiler.h>

#include "core/debug_log.h"
#include "gfx/d3d11/shaders_d3d11.h"

namespace {
constexpr const char *kLogCategory = "gfx";

using Microsoft::WRL::ComPtr;
using rift::gfx::d3d11::kShaderSource;

// Every constant-buffer struct below must mirror its HLSL counterpart's field order exactly.
// The vertex stage's buffer lives at b0; the two pixel-stage buffers share b1, which is safe
// because no draw call ever binds both.

struct ShaderConstants {
	float ViewportWidth;
	float ViewportHeight;
	float Padding[2];
};

static_assert(sizeof(ShaderConstants) == 16);

struct BannerGlowConstants {
	float TimeSeconds;
	float QuadWidth;
	float QuadHeight;
	float CornerRadius;
	float RingWidth;
	float Padding[3];
};

static_assert(sizeof(BannerGlowConstants) == 32);

struct CircularProgressConstants {
	float QuadWidth;
	float QuadHeight;
	float OuterRadius;
	float InnerRadius;
	float StartAngle;
	float SweepAngle;
	float GlowStrength;
	float Padding;
};

static_assert(sizeof(CircularProgressConstants) == 32);

bool CompileShader(const char *pEntryPoint, const char *pTarget, ComPtr<ID3DBlob> &outBlob)
{
	UINT compileFlags = 0;
#ifndef NDEBUG
	compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	ComPtr<ID3DBlob> errorBlob;
	const HRESULT hr = D3DCompile(kShaderSource, std::strlen(kShaderSource), nullptr, nullptr, nullptr, pEntryPoint,
								  pTarget, compileFlags, 0, &outBlob, &errorBlob);

	if (FAILED(hr)) {
		if (errorBlob) {
			OutputDebugStringA(static_cast<const char *>(errorBlob->GetBufferPointer()));
		}

		return false;
	}

	return true;
}

bool CreateConstantBuffer(ID3D11Device &device, UINT byteWidth, ComPtr<ID3D11Buffer> &outBuffer)
{
	const D3D11_BUFFER_DESC desc{
		.ByteWidth = byteWidth,
		.Usage = D3D11_USAGE_DEFAULT,
		.BindFlags = D3D11_BIND_CONSTANT_BUFFER,
	};

	return SUCCEEDED(device.CreateBuffer(&desc, nullptr, &outBuffer));
}

void UploadToDynamicBuffer(ID3D11DeviceContext &context, ID3D11Buffer *pBuffer, const void *pData, usize bytes)
{
	D3D11_MAPPED_SUBRESOURCE mapped{};
	context.Map(pBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	std::memcpy(mapped.pData, pData, bytes);
	context.Unmap(pBuffer, 0);
}
} // namespace

// Only this backend knows the real layout, so it stays out of the header.
struct CRendererD3D11::TextureSlot {
	ComPtr<ID3D11Texture2D> Texture;
	ComPtr<ID3D11ShaderResourceView> ShaderResourceView;
	u32 Width = 0;
	u32 Height = 0;
};

CRendererD3D11::~CRendererD3D11()
{
	if (m_bInitialized) {
		Shutdown();
	}
}

bool CRendererD3D11::Init(const RendererConfig &config)
{
	assert(!m_bInitialized);
	assert(config.Window != nullptr);

	DXGI_SWAP_CHAIN_DESC swapChainDesc{
		.BufferDesc =
			{
				.Width = config.Width,
				.Height = config.Height,
				.RefreshRate = {.Numerator = 0, .Denominator = 1},
				.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
			},
		// MSAA straight on the backbuffer. DXGI_SWAP_EFFECT_DISCARD, unlike the flip effects,
		// supports it and resolves implicitly on Present, so no explicit resolve is needed. 4x
		// is mandatory hardware support at feature level 10.1 and up, so no capability query
		// is needed either.
		.SampleDesc = {.Count = kMsaaSampleCount, .Quality = 0},
		.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
		.BufferCount = 2,
		.OutputWindow = config.Window,
		.Windowed = TRUE,
		.SwapEffect = DXGI_SWAP_EFFECT_DISCARD,
	};

	// Opt-in rather than on for every Debug build: the D3D debug layer costs around a quarter of a
	// second of device creation, which was most of this app's entire startup time, and it is only
	// wanted when actually chasing a graphics bug. Set PULSAR_D3D_DEBUG to get it back.
	UINT deviceFlags = 0;
#ifndef NDEBUG
	if (GetEnvironmentVariableW(L"PULSAR_D3D_DEBUG", nullptr, 0) != 0) {
		deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
		DebugLog::Write(kLogCategory, "D3D11 debug layer enabled by PULSAR_D3D_DEBUG");
	}
#endif

	const D3D_FEATURE_LEVEL requestedFeatureLevel = D3D_FEATURE_LEVEL_11_0;
	D3D_FEATURE_LEVEL obtainedFeatureLevel;

	const auto deviceStart = std::chrono::steady_clock::now();
	const HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, deviceFlags,
													 &requestedFeatureLevel, 1, D3D11_SDK_VERSION, &swapChainDesc,
													 &m_pSwapChain, &m_pDevice, &obtainedFeatureLevel, &m_pContext);
	if (FAILED(hr)) return false;

	const auto pipelineStart = std::chrono::steady_clock::now();

	if (!CreateRenderTargetView() || !CreatePipeline() || !CreateVertexBuffer(kInitialVertexCapacity) ||
		!CreateIndexBuffer(kInitialIndexCapacity)) {
		return false;
	}

	// The two halves have very different fixes, so they are reported apart: device creation is the
	// driver and, in a Debug build, the D3D debug layer, while the pipeline is shader compilation.
	DebugLog::Write(kLogCategory, "device %.1f ms, pipeline %.1f ms",
					std::chrono::duration<float, std::milli>(pipelineStart - deviceStart).count(),
					std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - pipelineStart).count());

	m_pTexturePool = new TextureSlot[kMaxTextures];

	m_nWidth = config.Width;
	m_nHeight = config.Height;
	m_flLogicalWidth = config.LogicalWidth;
	m_flLogicalHeight = config.LogicalHeight;
	UpdateViewport();

	m_bInitialized = true;

	return true;
}

void CRendererD3D11::Shutdown()
{
	delete[] m_pTexturePool;
	m_pTexturePool = nullptr;

	// Assigning a default-constructed instance releases every ComPtr and returns every scalar
	// to its initializer, m_bInitialized included.
	*this = CRendererD3D11();
}

bool CRendererD3D11::CreateRenderTargetView()
{
	ComPtr<ID3D11Texture2D> backBuffer;
	if (FAILED(m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) return false;

	return SUCCEEDED(m_pDevice->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_pRenderTargetView));
}

void CRendererD3D11::UpdateViewport()
{
	const D3D11_VIEWPORT viewport{
		.TopLeftX = 0,
		.TopLeftY = 0,
		.Width = static_cast<float>(m_nWidth),
		.Height = static_cast<float>(m_nHeight),
		.MinDepth = 0.0f,
		.MaxDepth = 1.0f,
	};

	m_pContext->RSSetViewports(1, &viewport);
}

bool CRendererD3D11::CreatePipeline()
{
	return CreateShaders() && CreateConstantBuffers() && CreatePipelineStates();
}

// Compiled in parallel, then created serially. D3DCompile is thread-safe and each entry point is
// independent, so six compiles cost about as long as the slowest one rather than their sum - which
// is worth roughly 35ms of a Release startup. Creating the shader objects afterwards is device
// work and stays on this thread, like every other renderer call.
bool CRendererD3D11::CreateShaders()
{
	struct ShaderJob {
		const char *pEntryPoint;
		const char *pTarget;
		ComPtr<ID3DBlob> Blob;
		bool bCompiled = false;
	};

	ShaderJob jobs[]{
		{"vs_main", "vs_5_0"},
		{"ps_main", "ps_5_0"},
		{"ps_main_textured", "ps_5_0"},
		{"ps_banner_glow", "ps_5_0"},
		{"ps_color_picker_sv", "ps_5_0"},
		{"ps_circular_progress", "ps_5_0"},
	};

	constexpr usize kJobCount = sizeof(jobs) / sizeof(jobs[0]);

	std::vector<std::thread> workers;
	workers.reserve(kJobCount - 1);

	// The first job stays on this thread rather than getting a worker of its own: one fewer thread
	// creation, and this thread has nothing else to do until they all finish.
	for (usize i = 1; i < kJobCount; i += 1) {
		workers.emplace_back(
			[&job = jobs[i]]() { job.bCompiled = CompileShader(job.pEntryPoint, job.pTarget, job.Blob); });
	}

	jobs[0].bCompiled = CompileShader(jobs[0].pEntryPoint, jobs[0].pTarget, jobs[0].Blob);

	for (std::thread &worker : workers) {
		worker.join();
	}

	for (const ShaderJob &job : jobs) {
		if (!job.bCompiled) return false;
	}

	ComPtr<ID3D11PixelShader> *const pixelShaders[]{
		&m_pPixelShaderSolid,		  &m_pPixelShaderTextured,		   &m_pPixelShaderBannerGlow,
		&m_pPixelShaderColorPickerSv, &m_pPixelShaderCircularProgress,
	};

	if (FAILED(m_pDevice->CreateVertexShader(jobs[0].Blob->GetBufferPointer(), jobs[0].Blob->GetBufferSize(), nullptr,
											 &m_pVertexShader))) {
		return false;
	}

	for (usize i = 0; i < kJobCount - 1; i += 1) {
		const ComPtr<ID3DBlob> &blob = jobs[i + 1].Blob;

		if (FAILED(m_pDevice->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr,
												pixelShaders[i]->GetAddressOf()))) {
			return false;
		}
	}

	return CreateInputLayout(*jobs[0].Blob.Get());
}

bool CRendererD3D11::CreateInputLayout(ID3DBlob &vertexShaderBlob)
{
	const D3D11_INPUT_ELEMENT_DESC inputElements[]{
		{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex2D, X), D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex2D, U), D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(Vertex2D, Color), D3D11_INPUT_PER_VERTEX_DATA, 0},
	};

	return SUCCEEDED(m_pDevice->CreateInputLayout(inputElements, ARRAYSIZE(inputElements),
												  vertexShaderBlob.GetBufferPointer(), vertexShaderBlob.GetBufferSize(),
												  &m_pInputLayout));
}

bool CRendererD3D11::CreateConstantBuffers()
{
	return CreateConstantBuffer(*m_pDevice.Get(), sizeof(ShaderConstants), m_pConstantBuffer) &&
		   CreateConstantBuffer(*m_pDevice.Get(), sizeof(BannerGlowConstants), m_pBannerGlowConstantBuffer) &&
		   CreateConstantBuffer(*m_pDevice.Get(), sizeof(CircularProgressConstants), m_pCircularProgressConstantBuffer);
}

bool CRendererD3D11::CreatePipelineStates()
{
	D3D11_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0] = {
		.BlendEnable = TRUE,
		.SrcBlend = D3D11_BLEND_SRC_ALPHA,
		.DestBlend = D3D11_BLEND_INV_SRC_ALPHA,
		.BlendOp = D3D11_BLEND_OP_ADD,
		.SrcBlendAlpha = D3D11_BLEND_ONE,
		.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA,
		.BlendOpAlpha = D3D11_BLEND_OP_ADD,
		.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL,
	};

	if (FAILED(m_pDevice->CreateBlendState(&blendDesc, &m_pBlendState))) return false;

	// UI quads carry no meaningful winding, so culling is off rather than something to get
	// right. Scissoring is always on: Draw2DInternal sets the rect to the full viewport when
	// no clip is active, so one state covers both clipped and unclipped draws.
	const D3D11_RASTERIZER_DESC rasterizerDesc{
		.FillMode = D3D11_FILL_SOLID,
		.CullMode = D3D11_CULL_NONE,
		.ScissorEnable = TRUE,
	};

	if (FAILED(m_pDevice->CreateRasterizerState(&rasterizerDesc, &m_pRasterizerState))) return false;

	const D3D11_SAMPLER_DESC samplerDesc{
		.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR,
		.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP,
		.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP,
		.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP,
		.MaxAnisotropy = 1,
		.MaxLOD = D3D11_FLOAT32_MAX,
	};

	return SUCCEEDED(m_pDevice->CreateSamplerState(&samplerDesc, &m_pSamplerState));
}

bool CRendererD3D11::CreateVertexBuffer(u32 capacity)
{
	const D3D11_BUFFER_DESC desc{
		.ByteWidth = capacity * sizeof(Vertex2D),
		.Usage = D3D11_USAGE_DYNAMIC,
		.BindFlags = D3D11_BIND_VERTEX_BUFFER,
		.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
	};

	if (FAILED(m_pDevice->CreateBuffer(&desc, nullptr, &m_pVertexBuffer))) return false;

	m_nVertexBufferCapacity = capacity;

	return true;
}

bool CRendererD3D11::CreateIndexBuffer(u32 capacity)
{
	const D3D11_BUFFER_DESC desc{
		.ByteWidth = capacity * sizeof(u32),
		.Usage = D3D11_USAGE_DYNAMIC,
		.BindFlags = D3D11_BIND_INDEX_BUFFER,
		.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
	};

	if (FAILED(m_pDevice->CreateBuffer(&desc, nullptr, &m_pIndexBuffer))) return false;

	m_nIndexBufferCapacity = capacity;

	return true;
}

void CRendererD3D11::Resize(u32 width, u32 height, float logicalWidth, float logicalHeight)
{
	assert(m_bInitialized);

	if (width == 0 || height == 0) return;

	m_pRenderTargetView.Reset();

	const HRESULT hr = m_pSwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
	assert(SUCCEEDED(hr));
	(void)hr;

	const bool ok = CreateRenderTargetView();
	assert(ok);
	(void)ok;

	m_nWidth = width;
	m_nHeight = height;
	m_flLogicalWidth = logicalWidth;
	m_flLogicalHeight = logicalHeight;
	UpdateViewport();
}

void CRendererD3D11::BeginFrame()
{
	assert(m_bInitialized);

	ID3D11RenderTargetView *pRenderTargetView = m_pRenderTargetView.Get();
	m_pContext->OMSetRenderTargets(1, &pRenderTargetView, nullptr);
}

void CRendererD3D11::Clear(ColorF color)
{
	assert(m_bInitialized);

	const float rgba[4]{color.R, color.G, color.B, color.A};
	m_pContext->ClearRenderTargetView(m_pRenderTargetView.Get(), rgba);
}

void CRendererD3D11::EndFrame()
{
	assert(m_bInitialized);

	m_pSwapChain->Present(1, 0);
}

void CRendererD3D11::UploadBatch(const Vertex2D *pVertices, u32 vertexCount, const u32 *pIndices, u32 indexCount)
{
	if (vertexCount > m_nVertexBufferCapacity) {
		const bool ok = CreateVertexBuffer(vertexCount * 2);
		assert(ok);
		(void)ok;
	}

	if (indexCount > m_nIndexBufferCapacity) {
		const bool ok = CreateIndexBuffer(indexCount * 2);
		assert(ok);
		(void)ok;
	}

	UploadToDynamicBuffer(*m_pContext.Get(), m_pVertexBuffer.Get(), pVertices, vertexCount * sizeof(Vertex2D));
	UploadToDynamicBuffer(*m_pContext.Get(), m_pIndexBuffer.Get(), pIndices, indexCount * sizeof(u32));
}

void CRendererD3D11::ApplyScissorRect()
{
	// The scissor rect operates on the render target, which is always in physical pixels.
	const float scale = m_flLogicalWidth > 0.0f ? static_cast<float>(m_nWidth) / m_flLogicalWidth : 1.0f;

	D3D11_RECT scissor{
		.left = 0,
		.top = 0,
		.right = static_cast<LONG>(m_nWidth),
		.bottom = static_cast<LONG>(m_nHeight),
	};

	if (m_pendingClipRect.Enabled) {
		const Rect &clip = m_pendingClipRect.Bounds;
		scissor.left = std::clamp(static_cast<LONG>(clip.X * scale), 0L, static_cast<LONG>(m_nWidth));
		scissor.top = std::clamp(static_cast<LONG>(clip.Y * scale), 0L, static_cast<LONG>(m_nHeight));
		scissor.right =
			std::clamp(static_cast<LONG>((clip.X + clip.W) * scale), scissor.left, static_cast<LONG>(m_nWidth));
		scissor.bottom =
			std::clamp(static_cast<LONG>((clip.Y + clip.H) * scale), scissor.top, static_cast<LONG>(m_nHeight));
	}

	m_pContext->RSSetScissorRects(1, &scissor);
}

void CRendererD3D11::Draw2DInternal(const Vertex2D *pVertices, u32 vertexCount, const u32 *pIndices, u32 indexCount,
									ID3D11PixelShader *pPixelShader, ID3D11ShaderResourceView *pShaderResourceView,
									ID3D11Buffer *pPixelExtraConstantBuffer)
{
	assert(m_bInitialized);

	if (vertexCount == 0 || indexCount == 0) return;

	UploadBatch(pVertices, vertexCount, pIndices, indexCount);

	const ShaderConstants constants{.ViewportWidth = m_flLogicalWidth, .ViewportHeight = m_flLogicalHeight};
	m_pContext->UpdateSubresource(m_pConstantBuffer.Get(), 0, nullptr, &constants, 0, 0);

	const UINT stride = sizeof(Vertex2D);
	const UINT offset = 0;
	ID3D11Buffer *pVertexBuffer = m_pVertexBuffer.Get();
	m_pContext->IASetVertexBuffers(0, 1, &pVertexBuffer, &stride, &offset);
	m_pContext->IASetIndexBuffer(m_pIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	m_pContext->IASetInputLayout(m_pInputLayout.Get());
	m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ID3D11Buffer *pConstantBuffer = m_pConstantBuffer.Get();
	m_pContext->VSSetShader(m_pVertexShader.Get(), nullptr, 0);
	m_pContext->VSSetConstantBuffers(0, 1, &pConstantBuffer);

	ID3D11SamplerState *pSampler = m_pSamplerState.Get();
	m_pContext->PSSetShader(pPixelShader, nullptr, 0);
	m_pContext->PSSetShaderResources(0, 1, &pShaderResourceView);
	m_pContext->PSSetSamplers(0, 1, &pSampler);

	if (pPixelExtraConstantBuffer != nullptr) {
		m_pContext->PSSetConstantBuffers(1, 1, &pPixelExtraConstantBuffer);
	}

	m_pContext->OMSetBlendState(m_pBlendState.Get(), nullptr, 0xFFFFFFFF);
	m_pContext->RSSetState(m_pRasterizerState.Get());
	ApplyScissorRect();

	m_pContext->DrawIndexed(indexCount, 0, 0);
}

void CRendererD3D11::Draw2D(const Vertex2D *pVertices, u32 vertexCount, const u32 *pIndices, u32 indexCount)
{
	Draw2DInternal(pVertices, vertexCount, pIndices, indexCount, m_pPixelShaderSolid.Get(), nullptr);
}

void CRendererD3D11::Draw2DTextured(void *pTextureHandle, const Vertex2D *pVertices, u32 vertexCount,
									const u32 *pIndices, u32 indexCount)
{
	assert(pTextureHandle != nullptr);

	auto *pSlot = static_cast<TextureSlot *>(pTextureHandle);
	Draw2DInternal(pVertices, vertexCount, pIndices, indexCount, m_pPixelShaderTextured.Get(),
				   pSlot->ShaderResourceView.Get());
}

void CRendererD3D11::Draw2DBannerGlow(const Vertex2D *pVertices, u32 vertexCount, const u32 *pIndices, u32 indexCount,
									  float quadWidth, float quadHeight, float cornerRadius, float ringWidth)
{
	assert(m_bInitialized);

	const BannerGlowConstants constants{
		.TimeSeconds = m_flEffectTimeSeconds,
		.QuadWidth = quadWidth,
		.QuadHeight = quadHeight,
		.CornerRadius = cornerRadius,
		.RingWidth = ringWidth,
	};

	m_pContext->UpdateSubresource(m_pBannerGlowConstantBuffer.Get(), 0, nullptr, &constants, 0, 0);
	Draw2DInternal(pVertices, vertexCount, pIndices, indexCount, m_pPixelShaderBannerGlow.Get(), nullptr,
				   m_pBannerGlowConstantBuffer.Get());
}

void CRendererD3D11::Draw2DColorPickerSv(const Vertex2D *pVertices, u32 vertexCount, const u32 *pIndices,
										 u32 indexCount)
{
	assert(m_bInitialized);

	// No constant buffer: hue travels per-vertex in the colour's red channel.
	Draw2DInternal(pVertices, vertexCount, pIndices, indexCount, m_pPixelShaderColorPickerSv.Get(), nullptr);
}

void CRendererD3D11::Draw2DCircularProgress(const Vertex2D *pVertices, u32 vertexCount, const u32 *pIndices,
											u32 indexCount, float quadWidth, float quadHeight, float outerRadius,
											float innerRadius, float startAngle, float sweepAngle, float glowStrength)
{
	assert(m_bInitialized);

	const CircularProgressConstants constants{
		.QuadWidth = quadWidth,
		.QuadHeight = quadHeight,
		.OuterRadius = outerRadius,
		.InnerRadius = innerRadius,
		.StartAngle = startAngle,
		.SweepAngle = sweepAngle,
		.GlowStrength = glowStrength,
	};

	m_pContext->UpdateSubresource(m_pCircularProgressConstantBuffer.Get(), 0, nullptr, &constants, 0, 0);
	Draw2DInternal(pVertices, vertexCount, pIndices, indexCount, m_pPixelShaderCircularProgress.Get(), nullptr,
				   m_pCircularProgressConstantBuffer.Get());
}

u32 CRendererD3D11::AcquireTextureSlot()
{
	if (m_nFreeTextureCount > 0) {
		m_nFreeTextureCount -= 1;
		return m_aFreeTextureIndices[m_nFreeTextureCount];
	}

	assert(m_nTexturePoolCount < kMaxTextures);
	const u32 index = m_nTexturePoolCount;
	m_nTexturePoolCount += 1;

	return index;
}

void CRendererD3D11::ReleaseTextureSlot(u32 index)
{
	assert(m_nFreeTextureCount < kMaxTextures);

	m_aFreeTextureIndices[m_nFreeTextureCount] = index;
	m_nFreeTextureCount += 1;
}

void *CRendererD3D11::CreateTexture(const u8 *pRgbaPixels, u32 width, u32 height)
{
	assert(m_bInitialized);

	const u32 index = AcquireTextureSlot();
	TextureSlot &slot = m_pTexturePool[index];

	const D3D11_TEXTURE2D_DESC textureDesc{
		.Width = width,
		.Height = height,
		.MipLevels = 1,
		.ArraySize = 1,
		.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
		.SampleDesc = {.Count = 1, .Quality = 0},
		.Usage = D3D11_USAGE_IMMUTABLE,
		.BindFlags = D3D11_BIND_SHADER_RESOURCE,
	};

	const D3D11_SUBRESOURCE_DATA initialData{
		.pSysMem = pRgbaPixels,
		.SysMemPitch = width * 4,
	};

	if (FAILED(m_pDevice->CreateTexture2D(&textureDesc, &initialData, &slot.Texture))) {
		ReleaseTextureSlot(index);
		return nullptr;
	}

	if (FAILED(m_pDevice->CreateShaderResourceView(slot.Texture.Get(), nullptr, &slot.ShaderResourceView))) {
		slot.Texture.Reset();
		ReleaseTextureSlot(index);
		return nullptr;
	}

	slot.Width = width;
	slot.Height = height;

	return &slot;
}

void CRendererD3D11::DestroyTexture(void *pHandle)
{
	assert(pHandle != nullptr);

	auto *pSlot = static_cast<TextureSlot *>(pHandle);
	pSlot->ShaderResourceView.Reset();
	pSlot->Texture.Reset();

	const auto index = static_cast<u32>(pSlot - m_pTexturePool);
	assert(index < m_nTexturePoolCount);

	ReleaseTextureSlot(index);
}
