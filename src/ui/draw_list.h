#pragma once

#include "core/types.h"
#include "gfx/renderer.h"

class CMemoryArena;
class CTexture;

// The CPU-side geometry builder for the whole UI. No OS controls are involved anywhere in this
// stack; every pixel is drawn here.
//
// Fixed-capacity and arena-backed: capacity is decided once at Init and never grows, so a frame
// never allocates. Draws batch by texture, shader kind and clip rect, and the caller submits one
// renderer call per command in order, preserving layering exactly as issued.

/// Packs to the byte order Vertex2D::Color expects.
u32 ColorPack(Color color);

/// Multiplies the colour's alpha by alpha/255, composing with whatever it already carried, which
/// is what fade animations want. ColorWithAlpha in core/types.h replaces the alpha instead.
Color ColorScaleAlpha(Color color, u8 alpha);

/// Which pixel shader a command is submitted with. BannerGlow has the same vertex shape as Solid
/// but its own shader, so it gets its own kind rather than overloading "no texture".
enum class EDrawCommandKind : u8 {
	Solid,
	Textured,
	BannerGlow,
	ColorPickerSv,
	CircularProgress,
};

/// The quad is the card expanded by its glow margin on every side; CornerRadius is the card's
/// rounding, not the expanded quad's. Two glows with different values cannot share one command.
struct BannerGlowParams {
	float QuadWidth;
	float QuadHeight;
	float CornerRadius;
	float RingWidth;
};

/// Same "quad bigger than the visible shape" convention as BannerGlowParams. Angles are radians,
/// clockwise and Y-down; a sweep of a full turn draws a solid ring. GlowStrength is 0..1 with any
/// pulse already baked in by the caller.
struct CircularProgressParams {
	float QuadWidth;
	float QuadHeight;
	float OuterRadius;
	float InnerRadius;
	float StartAngle;
	float SweepAngle;
	float GlowStrength;
	float Padding;
};

struct DrawCommand {
	EDrawCommandKind Kind;
	const CTexture *pTexture; // Textured only
	bool HasClip;
	Rect ClipRect; // logical pixels, HasClip only
	u32 IndexOffset;
	u32 IndexCount;
	BannerGlowParams Glow;			 // BannerGlow only
	CircularProgressParams Progress; // CircularProgress only
};

/// The UV sub-rectangle that cover-fits an image into a box: scaled up until it covers both axes
/// and cropped symmetrically on the overflowing one, rather than stretched.
struct UvRect {
	float U0;
	float V0;
	float U1;
	float V1;
};

class CDrawList {
  public:
	static constexpr u32 kMaxCommands = 256;

	/// Arc points per rounded corner. 14 is smooth at every radius this project uses while
	/// keeping a rounded rect under 60 vertices.
	static constexpr u32 kCornerSegments = 14;

	/// The only allocation a draw list ever makes; every frame afterwards reuses this storage.
	void Init(CMemoryArena &arena, u32 vertexCapacity, u32 indexCapacity);

	/// Resets to empty for a new frame. Frees nothing.
	void Clear();

	/// Clips every Add until the matching Pop. Not a stack: a nested Push replaces the active
	/// clip rather than intersecting with it, which covers every use here since no scrollable
	/// region nests inside another. Always pair the two.
	void PushClipRect(Rect rect);
	void PopClipRect();

	/// Closes the in-progress batch. Call once, after the frame's last Add and before submitting.
	void Finish();

	const DrawCommand *GetCommands() const
	{
		return m_aCommands;
	}

	u32 GetCommandCount() const
	{
		return m_nCommandCount;
	}

	const Vertex2D *GetVertices() const
	{
		return m_pVertices;
	}

	/// The whole frame's count, not a per-command one: every command's index range points into
	/// this one shared vertex buffer.
	u32 GetVertexCount() const
	{
		return m_nVertexCount;
	}

	const u32 *GetIndices() const
	{
		return m_pIndices;
	}

	/// The radius a request actually gets: the design radius scaled by the corner-roundness
	/// setting, which squares every corner at 0. Every rounded shape routes through these, so the
	/// setting has one place to hook into.
	static float ScaledRadius(float radius);
	static void SetCornerRoundnessScale(float scale);
	static CornerRadii UniformRadii(float radius);

	/// The per-corner sibling, for a shape rounded on only some corners. Hand-building a
	/// CornerRadii instead would silently opt out of the roundness setting.
	static CornerRadii Radii(float topLeft, float topRight, float bottomRight, float bottomLeft);

	/// The base solid fill. Every other flat-coloured shape here shares its two-triangle quad
	/// layout, just with different per-vertex colours, UVs or shader kind.
	void AddRectFilled(float x, float y, float w, float h, Color color);

	/// Four thin filled rects. There is no hollow-rect primitive: an outline is always built from
	/// filled geometry, either this or the nested-rounded-rect trick.
	void AddRectOutline(float x, float y, float w, float h, float thickness, Color color);

	/// A quad with an independent colour per corner. Exact for any one-dimensional ramp, but not
	/// for a genuinely bilinear function with a cross term - triangle interpolation only
	/// reconstructs affine functions. See AddRectColorPickerSv for that case.
	void AddRectGradientCorners(float x, float y, float w, float h, Color topLeft, Color topRight, Color bottomLeft,
								Color bottomRight);

	/// A real per-pixel HSV conversion rather than a geometric trick. The hue reaches the shader
	/// packed into the quad's vertex colour, so this needs no per-frame renderer state.
	void AddRectColorPickerSv(float x, float y, float w, float h, float hueDegrees);

	/// A thin filled quad along a segment - the only solid draw here that is not axis-aligned.
	void AddLine(float x0, float y0, float x1, float y1, float thickness, Color color);

	void AddRectRoundedFilled(float x, float y, float w, float h, CornerRadii radii, Color color);

	/// Fill plus border in one call: an outer rounded rect in the border colour with the fill
	/// inset on top. See ColorOutlineOn in core/types.h for where the border colour comes from.
	void AddRectRoundedBordered(float x, float y, float w, float h, CornerRadii radii, Color fill, Color border,
								float thickness);

	/// One quad, big enough to hold the ring plus its glow margin, submitted through the ring
	/// shader, which computes the track, the sweep, its rounded caps and the halo per pixel so the
	/// curve stays smooth at any size. The colour is the arc's tint and its alpha carries the
	/// caller's fade. Angles are degrees, and a sweep of 360 or more draws a full ring.
	void AddCircularProgress(float cx, float cy, float outerRadius, float innerRadius, float glowMargin,
							 float startAngleDeg, float sweepAngleDeg, float glowStrength, Color color);

	/// The animated beam hugging the outside of a card. The parameters describe the card itself,
	/// not the larger quad actually drawn: this expands it by glowSize internally and hands the
	/// shader enough to build a distance field from the card's real outline.
	void AddRectRoundedBannerGlow(float cardX, float cardY, float cardW, float cardH, float cardCornerRadius,
								  float glowSize, Color color);

	/// UVs map linearly across the rect's untrimmed bounds regardless of rounding, so the image
	/// does not distort at the corners.
	void AddRectRoundedTextured(float x, float y, float w, float h, CornerRadii radii, const CTexture *pTexture,
								Color tint);

	/// The same, mapping the bounds to a sub-rectangle of the texture - how banner art cover-fit
	/// crops to a card's aspect instead of stretching.
	void AddRectRoundedTexturedUv(float x, float y, float w, float h, CornerRadii radii, float u0, float v0, float u1,
								  float v1, const CTexture *pTexture, Color tint);

	/// Explicit UV corners rather than UVs derived from position, which is what glyph quads need:
	/// stb hands back atlas coordinates directly.
	void AddRectTexturedUv(float x, float y, float w, float h, float u0, float v0, float u1, float v1,
						   const CTexture *pTexture, Color tint);

	/// A textured quad rotated about its own centre, for spinning an icon in place. No rounding
	/// parameter, since a rotated corner fan would have to rotate with it and nothing needs that.
	void AddRectTexturedRotated(float x, float y, float w, float h, float radians, const CTexture *pTexture,
								Color tint);

	static UvRect ComputeCoverUv(float boxW, float boxH, float textureW, float textureH);

  private:
	/// Closes the in-progress command and opens one targeting this combination. A no-op if it
	/// already matches, so consecutive draws that change nothing stay batched together.
	void SetTarget(const CTexture *pTexture, EDrawCommandKind kind, BannerGlowParams glow = BannerGlowParams{},
				   CircularProgressParams progress = CircularProgressParams{});
	void SetTargetTexture(const CTexture *pTexture);
	void CloseOpenCommand();

	void EmitQuadIndices(u32 base);
	void EmitFanIndices(u32 base, u32 pointCount);
	void BuildRoundedRectPoints(float x, float y, float w, float h, CornerRadii radii, float outX[],
								float outY[]) const;

	/// Appends four vertices in top-left, top-right, bottom-right, bottom-left order and
	/// triangulates them.
	void PushQuad(const Vertex2D corners[4]);

	/// The shared body of every rounded draw. uvFor maps a position to that vertex's UV, which is
	/// the only thing the callers differ in.
	template <typename TUvFor>
	void PushRoundedFan(float x, float y, float w, float h, CornerRadii radii, u32 packedColor, TUvFor uvFor);

	Vertex2D *m_pVertices = nullptr;
	u32 *m_pIndices = nullptr;
	u32 m_nVertexCapacity = 0;
	u32 m_nIndexCapacity = 0;
	u32 m_nVertexCount = 0;
	u32 m_nIndexCount = 0;

	DrawCommand m_aCommands[kMaxCommands]{};
	u32 m_nCommandCount = 0;

	/// The in-progress command's state.
	EDrawCommandKind m_currentKind = EDrawCommandKind::Solid;
	const CTexture *m_pCurrentTexture = nullptr;
	bool m_bCurrentHasClip = false;
	Rect m_currentClipRect{};
	BannerGlowParams m_currentGlow{};
	CircularProgressParams m_currentProgress{};
	u32 m_nCommandStartIndex = 0;
	bool m_bHasOpenCommand = false;

	/// What the next Add should use, compared against the in-progress command's state to decide
	/// whether it has to close and a new one open, the same way a texture change does.
	bool m_bActiveClipEnabled = false;
	Rect m_activeClipRect{};
};
