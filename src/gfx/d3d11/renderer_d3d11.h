#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include "gfx/renderer.h"

/// The D3D11 backend: 4x MSAA on the swapchain, a fixed texture pool with a free list, five
/// pixel shaders (see shaders_d3d11.h), and scissor-rect clipping.
class CRendererD3D11 final : public IRenderer {
  public:
	/// Shuts down if nothing already has. Every CTexture still alive at teardown is a
	/// widget-owned member destructing in the normal reverse-declaration order, and each one
	/// calls back into the renderer to release its GPU resource - so "the renderer outlives
	/// every texture it issued" has to be the default rather than something each call site gets
	/// right. Getting that ordering wrong by hand was a real access violation.
	~CRendererD3D11() override;

	bool Init(const RendererConfig &config) override;
	void Shutdown() override;
	void Resize(u32 width, u32 height, float logicalWidth, float logicalHeight) override;

	void BeginFrame() override;
	void Clear(ColorF color) override;
	void EndFrame() override;

	void Draw2D(const Vertex2D *pVertices, u32 vertexCount, const u32 *pIndices, u32 indexCount) override;
	void Draw2DTextured(void *pTextureHandle, const Vertex2D *pVertices, u32 vertexCount, const u32 *pIndices,
						u32 indexCount) override;
	void Draw2DBannerGlow(const Vertex2D *pVertices, u32 vertexCount, const u32 *pIndices, u32 indexCount,
						  float quadWidth, float quadHeight, float cornerRadius, float ringWidth) override;
	void Draw2DColorPickerSv(const Vertex2D *pVertices, u32 vertexCount, const u32 *pIndices, u32 indexCount) override;
	void Draw2DCircularProgress(const Vertex2D *pVertices, u32 vertexCount, const u32 *pIndices, u32 indexCount,
								float quadWidth, float quadHeight, float outerRadius, float innerRadius,
								float startAngle, float sweepAngle, float glowStrength) override;

	void SetEffectTime(float timeSeconds) override
	{
		m_flEffectTimeSeconds = timeSeconds;
	}

	void SetClipRect(ClipRect clip) override
	{
		m_pendingClipRect = clip;
	}

	void *CreateTexture(const u8 *pRgbaPixels, u32 width, u32 height) override;
	void DestroyTexture(void *pHandle) override;

  private:
	template <typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	/// Defined only in the .cpp, so its layout stays opaque above IRenderer. Handed out as a
	/// void* into m_pTexturePool, a fixed array, so handles stay valid for the process lifetime.
	struct TextureSlot;

	static constexpr u32 kMaxTextures = 32;
	static constexpr u32 kInitialVertexCapacity = 1024;
	static constexpr u32 kInitialIndexCapacity = 1536;
	static constexpr UINT kMsaaSampleCount = 4;

	bool CreateRenderTargetView();
	void UpdateViewport();

	bool CreatePipeline();
	bool CreateShaders();
	bool CreateInputLayout(ID3DBlob &vertexShaderBlob);
	bool CreateConstantBuffers();
	bool CreatePipelineStates();

	bool CreateVertexBuffer(u32 capacity);
	bool CreateIndexBuffer(u32 capacity);

	void UploadBatch(const Vertex2D *pVertices, u32 vertexCount, const u32 *pIndices, u32 indexCount);
	void ApplyScissorRect();

	/// The one real draw path every entry point above funnels through.
	void Draw2DInternal(const Vertex2D *pVertices, u32 vertexCount, const u32 *pIndices, u32 indexCount,
						ID3D11PixelShader *pPixelShader, ID3D11ShaderResourceView *pShaderResourceView,
						ID3D11Buffer *pPixelExtraConstantBuffer = nullptr);

	u32 AcquireTextureSlot();
	void ReleaseTextureSlot(u32 index);

	ComPtr<ID3D11Device> m_pDevice;
	ComPtr<ID3D11DeviceContext> m_pContext;
	ComPtr<IDXGISwapChain> m_pSwapChain;
	ComPtr<ID3D11RenderTargetView> m_pRenderTargetView;

	ComPtr<ID3D11VertexShader> m_pVertexShader;
	ComPtr<ID3D11PixelShader> m_pPixelShaderSolid;
	ComPtr<ID3D11PixelShader> m_pPixelShaderTextured;
	ComPtr<ID3D11PixelShader> m_pPixelShaderBannerGlow;
	ComPtr<ID3D11PixelShader> m_pPixelShaderColorPickerSv;
	ComPtr<ID3D11PixelShader> m_pPixelShaderCircularProgress;

	ComPtr<ID3D11InputLayout> m_pInputLayout;
	ComPtr<ID3D11Buffer> m_pConstantBuffer;
	ComPtr<ID3D11Buffer> m_pBannerGlowConstantBuffer;
	ComPtr<ID3D11Buffer> m_pCircularProgressConstantBuffer;

	ComPtr<ID3D11BlendState> m_pBlendState;
	ComPtr<ID3D11RasterizerState> m_pRasterizerState;
	ComPtr<ID3D11SamplerState> m_pSamplerState;

	ComPtr<ID3D11Buffer> m_pVertexBuffer;
	ComPtr<ID3D11Buffer> m_pIndexBuffer;
	u32 m_nVertexBufferCapacity = 0;
	u32 m_nIndexBufferCapacity = 0;

	TextureSlot *m_pTexturePool = nullptr;

	/// High-water mark: slots below this have been allocated at least once.
	u32 m_nTexturePoolCount = 0;

	/// Reused before the high-water mark grows. Without this, create/destroy churn - a font
	/// atlas re-baking on every size change - would permanently consume a slot per call and
	/// eventually hit kMaxTextures with nothing simultaneously alive.
	u32 m_aFreeTextureIndices[kMaxTextures]{};
	u32 m_nFreeTextureCount = 0;

	/// Physical pixels: the swapchain, backbuffer and viewport size.
	u32 m_nWidth = 0;
	u32 m_nHeight = 0;

	float m_flLogicalWidth = 0.0f;
	float m_flLogicalHeight = 0.0f;
	float m_flEffectTimeSeconds = 0.0f;
	bool m_bInitialized = false;

	ClipRect m_pendingClipRect{}; // applied by the next Draw2DInternal
};
