#include "ui/tooltip.h"

#include <algorithm>
#include <cstring>

#include "core/animator.h"
#include "gfx/font.h"
#include "gfx/font_manager.h"
#include "ui/draw_list.h"
#include "ui/text.h"

namespace {
constexpr float kFadeRate = 22.0f;
constexpr float kPaddingX = 10.0f;
constexpr float kPaddingY = 6.0f;
constexpr float kRadius = 6.0f;
constexpr float kAnchorGap = 8.0f;
constexpr float kEdgeMargin = 6.0f;

// The bubble rises the last few pixels as it fades in, so it reads as arriving rather than
// blinking on.
constexpr float kRisePixels = 4.0f;

constexpr Color kColorBg{18, 18, 21, 246};
constexpr Color kColorBorder{72, 72, 80, 255};
constexpr Color kColorText{228, 228, 232, 255};
} // namespace

void CTooltip::Request(std::string_view text, Rect anchor)
{
	const u64 length = std::min<u64>(text.size(), kMaxTextLength);

	// A different string restarts the delay, since it is a genuinely new thing to explain. The
	// same string on a new anchor does not.
	if (length != m_nTextLength || std::memcmp(m_szText, text.data(), length) != 0) {
		std::memcpy(m_szText, text.data(), length);
		m_szText[length] = '\0';
		m_nTextLength = length;

		if (m_flVisibleAmount <= 0.01f) {
			m_flHoverSeconds = 0.0f;
		}
	}

	m_anchor = anchor;
	m_bRequestedThisFrame = true;
}

void CTooltip::Update(float deltaSeconds)
{
	m_flHoverSeconds = m_bRequestedThisFrame ? m_flHoverSeconds + deltaSeconds : 0.0f;

	const bool shouldShow = m_bRequestedThisFrame && m_flHoverSeconds >= kTooltipDelaySeconds;
	const float target = shouldShow ? 1.0f : 0.0f;

	m_flVisibleAmount = CAnimator::EaseToward(m_flVisibleAmount, target, kFadeRate, deltaSeconds);
	if (target == 0.0f && m_flVisibleAmount < 0.002f) {
		m_flVisibleAmount = 0.0f;
	}

	// Cleared here rather than in Draw: this is the call every caller makes unconditionally, so
	// a caller that early-outs of its own Draw cannot strand the flag set.
	m_bRequestedThisFrame = false;
}

void CTooltip::Reset()
{
	m_flHoverSeconds = 0.0f;
	m_flVisibleAmount = 0.0f;
	m_bRequestedThisFrame = false;
	m_nTextLength = 0;
	m_szText[0] = '\0';
}

void CTooltip::Draw(CDrawList &drawList, const CFontManager *pFonts, Rect bounds, u8 alpha) const
{
	if (m_flVisibleAmount <= 0.001f || m_nTextLength == 0) return;

	const CFont &font = pFonts->GetSecondary();
	const std::string_view text{m_szText, m_nTextLength};

	const float w = TextWidth(font, text) + kPaddingX * 2.0f;
	const float h = font.GetLineHeight() + kPaddingY * 2.0f;
	const float rise = (1.0f - m_flVisibleAmount) * kRisePixels;

	// Above by default; a bubble painting off the top of the owning panel is worse than one on
	// the other side of its anchor.
	float y = m_anchor.Y - h - kAnchorGap + rise;
	if (y < bounds.Y + kEdgeMargin) {
		y = m_anchor.Y + m_anchor.H + kAnchorGap - rise;
	}

	const float minX = bounds.X + kEdgeMargin;
	const float maxX = std::max(minX, bounds.X + bounds.W - kEdgeMargin - w);
	const float x = std::clamp(m_anchor.X + (m_anchor.W - w) * 0.5f, minX, maxX);

	const auto fade = static_cast<u8>(static_cast<float>(alpha) * m_flVisibleAmount);
	const float baselineY = y + h * 0.5f + (font.GetAscent() + font.GetDescent()) * 0.5f;

	drawList.AddRectRoundedFilled(x, y, w, h, CDrawList::UniformRadii(kRadius), ColorScaleAlpha(kColorBorder, fade));
	drawList.AddRectRoundedFilled(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, CDrawList::UniformRadii(kRadius - 1.0f),
								  ColorScaleAlpha(kColorBg, fade));
	DrawText(drawList, font, x + kPaddingX, baselineY, text, ColorScaleAlpha(kColorText, fade));
}
