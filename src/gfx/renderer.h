#pragma once

#include <Windows.h>

#include "core/types.h"

// The seam between UI code and the graphics backend. Exactly one implementation is compiled
// in, selected by the PULSAR_GFX_BACKEND CMake option - this interface exists for a clean
// boundary, not for runtime polymorphism.

/// Width and Height are physical pixels, the actual swapchain size. LogicalWidth and
/// LogicalHeight are the space every draw-list coordinate is authored in; the two differ on a
/// DPI-scaled monitor.
struct RendererConfig {
	HWND Window;
	u32 Width;
	u32 Height;
	float LogicalWidth;
	float LogicalHeight;
};

/// Floating-point RGBA, for renderer-internal values only. Not named Color, which
/// is the RGBA8 struct every widget uses - one name for both would mean a silent precision loss
/// at every call site.
struct ColorF {
	float R;
	float G;
	float B;
	float A;
};

/// Position is in logical pixels, origin top-left, y-down; the backend projects to NDC. Color
/// is RGBA8 packed low byte first. Solid draws ignore U/V; textured draws sample there and
/// multiply by Color, so a texture can be tinted or faded through vertex alpha.
struct Vertex2D {
	float X;
	float Y;
	float U;
	float V;
	u32 Color;
};

static_assert(sizeof(Vertex2D) == 20);

/// Set once per draw-list command by whoever submits it, in the same "set the pending
/// parameter, then submit" shape as SetEffectTime.
struct ClipRect {
	bool Enabled;
	Rect Bounds;
};

class IRenderer {
  public:
	virtual ~IRenderer() = default;

	/// Must succeed before any other call.
	virtual bool Init(const RendererConfig &config) = 0;
	virtual void Shutdown() = 0;

	/// Call after the client area changes size, including a DPI change, before the next frame.
	virtual void Resize(u32 width, u32 height, float logicalWidth, float logicalHeight) = 0;

	virtual void BeginFrame() = 0;
	virtual void Clear(ColorF color) = 0;
	virtual void EndFrame() = 0;

	/// One indexed triangle-list batch in logical-pixel screen space.
	virtual void Draw2D(const Vertex2D *pVertices, u32 vertexCount, const u32 *pIndices, u32 indexCount) = 0;

	/// Samples pTextureHandle at each vertex's U/V, tinted by vertex color.
	virtual void Draw2DTextured(void *pTextureHandle, const Vertex2D *pVertices, u32 vertexCount, const u32 *pIndices,
								u32 indexCount) = 0;

	/// The animated glow around a selected carousel card. quadWidth and quadHeight are the drawn
	/// quad's full size, cornerRadius is the card's rounding rather than the expanded quad's,
	/// and ringWidth is the glow margin - together they let the shader build a rounded-box
	/// distance field so the glow hugs the card's real outline.
	virtual void Draw2DBannerGlow(const Vertex2D *pVertices, u32 vertexCount, const u32 *pIndices, u32 indexCount,
								  float quadWidth, float quadHeight, float cornerRadius, float ringWidth) = 0;

	/// The colour picker's saturation/value square, as a real per-pixel HSV conversion: that
	/// gradient has a saturation-times-value cross term, and only affine functions survive
	/// triangle-linear interpolation. Hue travels in per-vertex, in the red channel.
	virtual void Draw2DColorPickerSv(const Vertex2D *pVertices, u32 vertexCount, const u32 *pIndices,
									 u32 indexCount) = 0;

	/// The account modal's login ring, per-pixel rather than tessellated so the band, comet
	/// tail, rounded caps and glow stay smooth at any size. The quad is the ring expanded by its
	/// glow margin, the same convention Draw2DBannerGlow uses. Angles are radians, and a sweep
	/// of at least a full turn draws a solid ring. glowStrength is 0..1 with any pulse already
	/// baked in by the caller.
	virtual void Draw2DCircularProgress(const Vertex2D *pVertices, u32 vertexCount, const u32 *pIndices, u32 indexCount,
										float quadWidth, float quadHeight, float outerRadius, float innerRadius,
										float startAngle, float sweepAngle, float glowStrength) = 0;

	/// Seconds since Init, read by every banner-glow draw until the next set.
	virtual void SetEffectTime(float timeSeconds) = 0;

	virtual void SetClipRect(ClipRect clip) = 0;

	/// Tightly-packed RGBA8, row-major. Only CTexture should call these; everything else owns a
	/// CTexture rather than a raw handle.
	virtual void *CreateTexture(const u8 *pRgbaPixels, u32 width, u32 height) = 0;
	virtual void DestroyTexture(void *pHandle) = 0;
};
