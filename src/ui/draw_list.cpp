#include "ui/draw_list.h"

#include "core/profiler.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <utility>

#include "core/memory_arena.h"
#include "gfx/texture.h"

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegreesToRadians = kPi / 180.0f;

constexpr u32 kRoundedRectPointsPerCorner = CDrawList::kCornerSegments + 1;
constexpr u32 kRoundedRectPointCount = kRoundedRectPointsPerCorner * 4;

float g_flCornerRoundnessScale = 1.0f;

// These types are plain aggregates with no operator== of their own, and this is the only place
// that needs one: to answer "did anything change since the last draw call".
bool ClipRectsEqual(Rect a, Rect b)
{
	return a.X == b.X && a.Y == b.Y && a.W == b.W && a.H == b.H;
}

bool GlowParamsEqual(BannerGlowParams a, BannerGlowParams b)
{
	return a.QuadWidth == b.QuadWidth && a.QuadHeight == b.QuadHeight && a.CornerRadius == b.CornerRadius &&
		   a.RingWidth == b.RingWidth;
}

bool ProgressParamsEqual(CircularProgressParams a, CircularProgressParams b)
{
	return a.QuadWidth == b.QuadWidth && a.QuadHeight == b.QuadHeight && a.OuterRadius == b.OuterRadius &&
		   a.InnerRadius == b.InnerRadius && a.StartAngle == b.StartAngle && a.SweepAngle == b.SweepAngle &&
		   a.GlowStrength == b.GlowStrength;
}
} // namespace

u32 ColorPack(Color color)
{
	return static_cast<u32>(color.R) | (static_cast<u32>(color.G) << 8) | (static_cast<u32>(color.B) << 16) |
		   (static_cast<u32>(color.A) << 24);
}

Color ColorScaleAlpha(Color color, u8 alpha)
{
	return Color{color.R, color.G, color.B, static_cast<u8>((color.A * alpha) / 255)};
}

float CDrawList::ScaledRadius(float radius)
{
	return radius * g_flCornerRoundnessScale;
}

void CDrawList::SetCornerRoundnessScale(float scale)
{
	// Negative would flip corners inside out. The upper end needs no clamp: the point builder
	// already clamps each radius against the shape's size, so a large scale just turns a
	// small control into a pill.
	g_flCornerRoundnessScale = std::max(0.0f, scale);
}

CornerRadii CDrawList::UniformRadii(float radius)
{
	const float r = ScaledRadius(radius);

	return CornerRadii{r, r, r, r};
}

CornerRadii CDrawList::Radii(float topLeft, float topRight, float bottomRight, float bottomLeft)
{
	return CornerRadii{ScaledRadius(topLeft), ScaledRadius(topRight), ScaledRadius(bottomRight),
					   ScaledRadius(bottomLeft)};
}

void CDrawList::Init(CMemoryArena &arena, u32 vertexCapacity, u32 indexCapacity)
{
	m_pVertices = arena.AllocArray<Vertex2D>(vertexCapacity);
	m_pIndices = arena.AllocArray<u32>(indexCapacity);
	m_nVertexCapacity = vertexCapacity;
	m_nIndexCapacity = indexCapacity;

	Clear();
}

void CDrawList::Clear()
{
	m_nVertexCount = 0;
	m_nIndexCount = 0;
	m_nCommandCount = 0;
	m_nCommandStartIndex = 0;

	m_currentKind = EDrawCommandKind::Solid;
	m_pCurrentTexture = nullptr;
	m_bCurrentHasClip = false;
	m_bHasOpenCommand = false;
	m_currentGlow = BannerGlowParams{};
	m_currentProgress = CircularProgressParams{};

	// A stray unmatched push in a previous frame must never leak into the next.
	m_bActiveClipEnabled = false;
}

void CDrawList::PushClipRect(Rect rect)
{
	m_bActiveClipEnabled = true;
	m_activeClipRect = rect;
}

void CDrawList::PopClipRect()
{
	m_bActiveClipEnabled = false;
}

void CDrawList::CloseOpenCommand()
{
	if (!m_bHasOpenCommand || m_nIndexCount <= m_nCommandStartIndex) return;

	assert(m_nCommandCount < kMaxCommands);

	m_aCommands[m_nCommandCount] = DrawCommand{
		.Kind = m_currentKind,
		.pTexture = m_pCurrentTexture,
		.HasClip = m_bCurrentHasClip,
		.ClipRect = m_currentClipRect,
		.IndexOffset = m_nCommandStartIndex,
		.IndexCount = m_nIndexCount - m_nCommandStartIndex,
		.Glow = m_currentGlow,
		.Progress = m_currentProgress,
	};

	m_nCommandCount += 1;
}

void CDrawList::SetTarget(const CTexture *pTexture, EDrawCommandKind kind, BannerGlowParams glow,
						  CircularProgressParams progress)
{
	const bool clipChanged = m_bCurrentHasClip != m_bActiveClipEnabled ||
							 (m_bActiveClipEnabled && !ClipRectsEqual(m_currentClipRect, m_activeClipRect));

	// One constant buffer per draw, so two glow or ring quads with different geometry cannot
	// share a command - the same reason a texture switch forces a new one.
	const bool glowChanged = kind == EDrawCommandKind::BannerGlow && !GlowParamsEqual(m_currentGlow, glow);
	const bool progressChanged =
		kind == EDrawCommandKind::CircularProgress && !ProgressParamsEqual(m_currentProgress, progress);

	if (m_bHasOpenCommand && m_pCurrentTexture == pTexture && m_currentKind == kind && !clipChanged && !glowChanged &&
		!progressChanged) {
		return;
	}

	CloseOpenCommand();

	m_pCurrentTexture = pTexture;
	m_currentKind = kind;
	m_bCurrentHasClip = m_bActiveClipEnabled;
	m_currentClipRect = m_activeClipRect;
	m_currentGlow = glow;
	m_currentProgress = progress;
	m_nCommandStartIndex = m_nIndexCount;
	m_bHasOpenCommand = true;
}

void CDrawList::SetTargetTexture(const CTexture *pTexture)
{
	SetTarget(pTexture, pTexture == nullptr ? EDrawCommandKind::Solid : EDrawCommandKind::Textured);
}

void CDrawList::Finish()
{
	PULSAR_PROFILE_SCOPE("DrawList.Finish");

	CloseOpenCommand();
	m_bHasOpenCommand = false;
}

// The two-triangle pattern every quad primitive shares, over four vertices appended in
// top-left, top-right, bottom-right, bottom-left order.
void CDrawList::EmitQuadIndices(u32 base)
{
	m_pIndices[m_nIndexCount + 0] = base + 0;
	m_pIndices[m_nIndexCount + 1] = base + 1;
	m_pIndices[m_nIndexCount + 2] = base + 2;
	m_pIndices[m_nIndexCount + 3] = base + 0;
	m_pIndices[m_nIndexCount + 4] = base + 2;
	m_pIndices[m_nIndexCount + 5] = base + 3;

	m_nIndexCount += 6;
}

// The centre-fan pattern the rounded draws share: `base` is the centre vertex and the
// perimeter points follow it in order.
void CDrawList::EmitFanIndices(u32 base, u32 pointCount)
{
	for (u32 i = 0; i < pointCount; i += 1) {
		const u32 next = (i + 1) % pointCount;

		m_pIndices[m_nIndexCount + 0] = base;
		m_pIndices[m_nIndexCount + 1] = base + 1 + i;
		m_pIndices[m_nIndexCount + 2] = base + 1 + next;
		m_nIndexCount += 3;
	}
}

// Perimeter points, clockwise, grouped by corner. Each arc starts where the previous edge ends
// and ends where the next begins, so consecutive points - across corners included - are always
// joined by a real segment with nothing implicit in between.
void CDrawList::BuildRoundedRectPoints(float x, float y, float w, float h, CornerRadii radii, float outX[],
									   float outY[]) const
{
	const float maxRadius = std::min(w, h) * 0.5f;
	const float radius[4]{
		std::min(radii.TopLeft, maxRadius),
		std::min(radii.TopRight, maxRadius),
		std::min(radii.BottomRight, maxRadius),
		std::min(radii.BottomLeft, maxRadius),
	};

	const float centerX[4]{x + radius[0], x + w - radius[1], x + w - radius[2], x + radius[3]};
	const float centerY[4]{y + radius[0], y + radius[1], y + h - radius[2], y + h - radius[3]};
	const float startAngleDeg[4]{180.0f, 270.0f, 0.0f, 90.0f};

	u32 pointIndex = 0;
	for (u32 corner = 0; corner < 4; corner += 1) {
		for (u32 step = 0; step < kRoundedRectPointsPerCorner; step += 1) {
			const float t = static_cast<float>(step) / static_cast<float>(kCornerSegments);
			const float angle = (startAngleDeg[corner] + 90.0f * t) * kDegreesToRadians;

			outX[pointIndex] = centerX[corner] + radius[corner] * std::cos(angle);
			outY[pointIndex] = centerY[corner] + radius[corner] * std::sin(angle);
			pointIndex += 1;
		}
	}
}

void CDrawList::PushQuad(const Vertex2D corners[4])
{
	assert(m_nVertexCount + 4 <= m_nVertexCapacity);
	assert(m_nIndexCount + 6 <= m_nIndexCapacity);

	const u32 base = m_nVertexCount;
	for (u32 i = 0; i < 4; i += 1) {
		m_pVertices[base + i] = corners[i];
	}

	m_nVertexCount += 4;

	EmitQuadIndices(base);
}

// The shared body of every rounded draw: build the perimeter, emit a centre vertex plus one
// per point, and fan them. UvFor maps a position to that vertex's UV, which is the only thing
// the three callers actually differ in.
template <typename TUvFor>
void CDrawList::PushRoundedFan(float x, float y, float w, float h, CornerRadii radii, u32 packedColor, TUvFor uvFor)
{
	float pointX[kRoundedRectPointCount];
	float pointY[kRoundedRectPointCount];
	BuildRoundedRectPoints(x, y, w, h, radii, pointX, pointY);

	assert(m_nVertexCount + kRoundedRectPointCount + 1 <= m_nVertexCapacity);
	assert(m_nIndexCount + kRoundedRectPointCount * 3 <= m_nIndexCapacity);

	const u32 base = m_nVertexCount;
	const float centerX = x + w * 0.5f;
	const float centerY = y + h * 0.5f;

	const auto [centerU, centerV] = uvFor(centerX, centerY);
	m_pVertices[base] = {.X = centerX, .Y = centerY, .U = centerU, .V = centerV, .Color = packedColor};

	for (u32 i = 0; i < kRoundedRectPointCount; i += 1) {
		const auto [u, v] = uvFor(pointX[i], pointY[i]);
		m_pVertices[base + 1 + i] = {.X = pointX[i], .Y = pointY[i], .U = u, .V = v, .Color = packedColor};
	}

	m_nVertexCount += kRoundedRectPointCount + 1;

	EmitFanIndices(base, kRoundedRectPointCount);
}

void CDrawList::AddRectFilled(float x, float y, float w, float h, Color color)
{
	SetTargetTexture(nullptr);

	const u32 packed = ColorPack(color);
	const Vertex2D corners[4]{
		{.X = x, .Y = y, .U = 0.0f, .V = 0.0f, .Color = packed},
		{.X = x + w, .Y = y, .U = 0.0f, .V = 0.0f, .Color = packed},
		{.X = x + w, .Y = y + h, .U = 0.0f, .V = 0.0f, .Color = packed},
		{.X = x, .Y = y + h, .U = 0.0f, .V = 0.0f, .Color = packed},
	};

	PushQuad(corners);
}

void CDrawList::AddRectOutline(float x, float y, float w, float h, float thickness, Color color)
{
	AddRectFilled(x, y, w, thickness, color);
	AddRectFilled(x, y + h - thickness, w, thickness, color);
	AddRectFilled(x, y + thickness, thickness, h - 2.0f * thickness, color);
	AddRectFilled(x + w - thickness, y + thickness, thickness, h - 2.0f * thickness, color);
}

void CDrawList::AddRectGradientCorners(float x, float y, float w, float h, Color topLeft, Color topRight,
									   Color bottomLeft, Color bottomRight)
{
	SetTargetTexture(nullptr);

	const Vertex2D corners[4]{
		{.X = x, .Y = y, .U = 0.0f, .V = 0.0f, .Color = ColorPack(topLeft)},
		{.X = x + w, .Y = y, .U = 0.0f, .V = 0.0f, .Color = ColorPack(topRight)},
		{.X = x + w, .Y = y + h, .U = 0.0f, .V = 0.0f, .Color = ColorPack(bottomRight)},
		{.X = x, .Y = y + h, .U = 0.0f, .V = 0.0f, .Color = ColorPack(bottomLeft)},
	};

	PushQuad(corners);
}

void CDrawList::AddRectColorPickerSv(float x, float y, float w, float h, float hueDegrees)
{
	SetTarget(nullptr, EDrawCommandKind::ColorPickerSv);

	// The vertex colour is not a tint here: its red channel carries the hue, which the shader
	// decodes and combines with this quad's UV - saturation across, value down - to compute the
	// real colour per pixel.
	const auto hueByte = static_cast<u8>(std::clamp(hueDegrees / 360.0f, 0.0f, 1.0f) * 255.0f);
	const u32 packed = ColorPack(Color{hueByte, 0, 0, 255});

	const Vertex2D corners[4]{
		{.X = x, .Y = y, .U = 0.0f, .V = 0.0f, .Color = packed},
		{.X = x + w, .Y = y, .U = 1.0f, .V = 0.0f, .Color = packed},
		{.X = x + w, .Y = y + h, .U = 1.0f, .V = 1.0f, .Color = packed},
		{.X = x, .Y = y + h, .U = 0.0f, .V = 1.0f, .Color = packed},
	};

	PushQuad(corners);
}

void CDrawList::AddLine(float x0, float y0, float x1, float y1, float thickness, Color color)
{
	const float dx = x1 - x0;
	const float dy = y1 - y0;
	const float length = std::sqrt(dx * dx + dy * dy);
	if (length < 0.0001f) return;

	SetTargetTexture(nullptr);

	const float half = thickness * 0.5f;
	const float nx = -dy / length * half;
	const float ny = dx / length * half;
	const u32 packed = ColorPack(color);

	const Vertex2D corners[4]{
		{.X = x0 + nx, .Y = y0 + ny, .U = 0.0f, .V = 0.0f, .Color = packed},
		{.X = x1 + nx, .Y = y1 + ny, .U = 0.0f, .V = 0.0f, .Color = packed},
		{.X = x1 - nx, .Y = y1 - ny, .U = 0.0f, .V = 0.0f, .Color = packed},
		{.X = x0 - nx, .Y = y0 - ny, .U = 0.0f, .V = 0.0f, .Color = packed},
	};

	PushQuad(corners);
}

void CDrawList::AddRectRoundedFilled(float x, float y, float w, float h, CornerRadii radii, Color color)
{
	SetTargetTexture(nullptr);

	PushRoundedFan(x, y, w, h, radii, ColorPack(color),
				   [](float, float) { return std::pair<float, float>{0.0f, 0.0f}; });
}

void CDrawList::AddRectRoundedBordered(float x, float y, float w, float h, CornerRadii radii, Color fill, Color border,
									   float thickness)
{
	AddRectRoundedFilled(x, y, w, h, radii, border);

	// The inner radii shrink with the inset, so the border keeps a constant thickness around
	// the curve instead of bunching up at the corners.
	const CornerRadii innerRadii{
		std::max(0.0f, radii.TopLeft - thickness),
		std::max(0.0f, radii.TopRight - thickness),
		std::max(0.0f, radii.BottomRight - thickness),
		std::max(0.0f, radii.BottomLeft - thickness),
	};

	AddRectRoundedFilled(x + thickness, y + thickness, w - thickness * 2.0f, h - thickness * 2.0f, innerRadii, fill);
}

void CDrawList::AddCircularProgress(float cx, float cy, float outerRadius, float innerRadius, float glowMargin,
									float startAngleDeg, float sweepAngleDeg, float glowStrength, Color color)
{
	const float half = outerRadius + glowMargin;
	const float quadSize = half * 2.0f;

	const CircularProgressParams progress{
		.QuadWidth = quadSize,
		.QuadHeight = quadSize,
		.OuterRadius = outerRadius,
		.InnerRadius = innerRadius,
		.StartAngle = startAngleDeg * kDegreesToRadians,
		.SweepAngle = sweepAngleDeg * kDegreesToRadians,
		.GlowStrength = glowStrength,
	};

	SetTarget(nullptr, EDrawCommandKind::CircularProgress, BannerGlowParams{}, progress);

	// UV spans the quad's bounds, and the shader reconstructs angle and radius from it -
	// the same convention the banner glow's distance field uses.
	const u32 packed = ColorPack(color);
	const Vertex2D corners[4]{
		{.X = cx - half, .Y = cy - half, .U = 0.0f, .V = 0.0f, .Color = packed},
		{.X = cx + half, .Y = cy - half, .U = 1.0f, .V = 0.0f, .Color = packed},
		{.X = cx + half, .Y = cy + half, .U = 1.0f, .V = 1.0f, .Color = packed},
		{.X = cx - half, .Y = cy + half, .U = 0.0f, .V = 1.0f, .Color = packed},
	};

	PushQuad(corners);
}

void CDrawList::AddRectRoundedBannerGlow(float cardX, float cardY, float cardW, float cardH, float cardCornerRadius,
										 float glowSize, Color color)
{
	const float x = cardX - glowSize;
	const float y = cardY - glowSize;
	const float w = cardW + glowSize * 2.0f;
	const float h = cardH + glowSize * 2.0f;

	// The shader gets the card's radius separately rather than through CornerRadii, so it has
	// to be scaled here too or its outline would not match the geometry.
	const BannerGlowParams glow{
		.QuadWidth = w,
		.QuadHeight = h,
		.CornerRadius = ScaledRadius(cardCornerRadius),
		.RingWidth = glowSize,
	};

	SetTarget(nullptr, EDrawCommandKind::BannerGlow, glow);

	// UV spans the quad's bounds, which the shader reads as position within the card rather
	// than as a texture coordinate.
	PushRoundedFan(x, y, w, h, UniformRadii(cardCornerRadius + glowSize), ColorPack(color),
				   [x, y, w, h](float px, float py) { return std::pair<float, float>{(px - x) / w, (py - y) / h}; });
}

void CDrawList::AddRectRoundedTexturedUv(float x, float y, float w, float h, CornerRadii radii, float u0, float v0,
										 float u1, float v1, const CTexture *pTexture, Color tint)
{
	SetTargetTexture(pTexture);

	PushRoundedFan(x, y, w, h, radii, ColorPack(tint), [x, y, w, h, u0, v0, u1, v1](float px, float py) {
		return std::pair<float, float>{u0 + (u1 - u0) * (px - x) / w, v0 + (v1 - v0) * (py - y) / h};
	});
}

void CDrawList::AddRectRoundedTextured(float x, float y, float w, float h, CornerRadii radii, const CTexture *pTexture,
									   Color tint)
{
	AddRectRoundedTexturedUv(x, y, w, h, radii, 0.0f, 0.0f, 1.0f, 1.0f, pTexture, tint);
}

void CDrawList::AddRectTexturedUv(float x, float y, float w, float h, float u0, float v0, float u1, float v1,
								  const CTexture *pTexture, Color tint)
{
	SetTargetTexture(pTexture);

	const u32 packed = ColorPack(tint);
	const Vertex2D corners[4]{
		{.X = x, .Y = y, .U = u0, .V = v0, .Color = packed},
		{.X = x + w, .Y = y, .U = u1, .V = v0, .Color = packed},
		{.X = x + w, .Y = y + h, .U = u1, .V = v1, .Color = packed},
		{.X = x, .Y = y + h, .U = u0, .V = v1, .Color = packed},
	};

	PushQuad(corners);
}

void CDrawList::AddRectTexturedRotated(float x, float y, float w, float h, float radians, const CTexture *pTexture,
									   Color tint)
{
	SetTargetTexture(pTexture);

	const float cx = x + w * 0.5f;
	const float cy = y + h * 0.5f;
	const float cosAngle = std::cos(radians);
	const float sinAngle = std::sin(radians);
	const float hw = w * 0.5f;
	const float hh = h * 0.5f;

	// Corner offsets from the centre, rotated - same winding as every other quad here, so the
	// shared index emitter needs no special case.
	const float offsetsX[4]{-hw, hw, hw, -hw};
	const float offsetsY[4]{-hh, -hh, hh, hh};
	const float uvs[4][2]{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};

	const u32 packed = ColorPack(tint);
	Vertex2D corners[4];
	for (u32 i = 0; i < 4; i += 1) {
		corners[i] = {
			.X = cx + offsetsX[i] * cosAngle - offsetsY[i] * sinAngle,
			.Y = cy + offsetsX[i] * sinAngle + offsetsY[i] * cosAngle,
			.U = uvs[i][0],
			.V = uvs[i][1],
			.Color = packed,
		};
	}

	PushQuad(corners);
}

UvRect CDrawList::ComputeCoverUv(float boxW, float boxH, float textureW, float textureH)
{
	if (boxW <= 0.0f || boxH <= 0.0f || textureW <= 0.0f || textureH <= 0.0f) return UvRect{0.0f, 0.0f, 1.0f, 1.0f};

	const float boxAspect = boxW / boxH;
	const float textureAspect = textureW / textureH;

	if (textureAspect > boxAspect) {
		const float visibleWidthFraction = boxAspect / textureAspect;
		const float u0 = (1.0f - visibleWidthFraction) * 0.5f;

		return UvRect{u0, 0.0f, 1.0f - u0, 1.0f};
	}

	if (textureAspect < boxAspect) {
		const float visibleHeightFraction = textureAspect / boxAspect;
		const float v0 = (1.0f - visibleHeightFraction) * 0.5f;

		return UvRect{0.0f, v0, 1.0f, 1.0f - v0};
	}

	return UvRect{0.0f, 0.0f, 1.0f, 1.0f};
}
