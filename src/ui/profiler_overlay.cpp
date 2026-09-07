#include "ui/profiler_overlay.h"

#ifdef PULSAR_PROFILING

#include <cstdio>

#include <Windows.h>

#include "ui/draw_list.h"
#include "ui/text.h"

namespace {
constexpr float kMargin = 12.0f;
constexpr float kPadding = 10.0f;
constexpr float kIndentPerDepth = 12.0f;
constexpr float kPanelWidth = 340.0f;
constexpr float kCornerRadius = 8.0f;

constexpr Color kColorPanel{14, 14, 16, 225};
constexpr Color kColorBorder{60, 60, 68, 255};
constexpr Color kColorHeading{236, 236, 240, 255};
constexpr Color kColorName{198, 198, 206, 255};
constexpr Color kColorNumbers{150, 190, 240, 255};

/// A frame over this is a visible hitch at 60Hz, so the heading turns red rather than making the
/// reader compare against a budget they have to remember.
constexpr float kFrameBudgetMs = 16.6f;
constexpr Color kColorOverBudget{235, 110, 110, 255};
} // namespace

CProfilerOverlay::CProfilerOverlay(CFontManager *pFonts)
	: m_pFonts(pFonts)
{
}

void CProfilerOverlay::Update(float deltaSeconds)
{
	(void)deltaSeconds;
}

bool CProfilerOverlay::OnKeyDown(u32 keyCode)
{
	if (keyCode == VK_F1) {
		m_bShown = !m_bShown;
		return true;
	}

	if (keyCode == VK_F2 && m_bShown) {
		CProfiler::Reset();
		return true;
	}

	return false;
}

void CProfilerOverlay::DrawRow(CDrawList &drawList, const CProfiler::ScopeStats &stats, float x, float baselineY) const
{
	const CFont &font = m_pFonts->GetSecondary();

	char numbers[48];
	std::snprintf(numbers, sizeof(numbers), "%6.2f  %6.2f  %3u", stats.AverageMs, stats.PeakMs, stats.Calls);

	const float numbersWidth = TextWidth(font, numbers);
	const float numbersX = x + kPanelWidth - kPadding * 2.0f - numbersWidth;

	// The name is cut at the numbers column rather than overrunning it: a deep scope name is
	// long, and a report whose columns do not line up is not a report.
	DrawTextEllipsized(drawList, font, x + static_cast<float>(stats.Depth) * kIndentPerDepth, baselineY, stats.pName,
					   numbersX - x - static_cast<float>(stats.Depth) * kIndentPerDepth - kPadding, kColorName);
	DrawText(drawList, font, numbersX, baselineY, numbers, kColorNumbers);
}

void CProfilerOverlay::Draw(CDrawList &drawList)
{
	if (!m_bShown) return;

	const CFont &font = m_pFonts->GetSecondary();
	const float lineHeight = font.GetLineHeight();
	const u32 scopeCount = CProfiler::GetScopeCount();

	// Heading, column header, then one row per scope.
	const float height = kPadding * 2.0f + static_cast<float>(scopeCount + 2) * lineHeight;
	const Rect panel{kMargin, kMargin, kPanelWidth, height};

	drawList.AddRectRoundedBordered(panel.X, panel.Y, panel.W, panel.H, CDrawList::UniformRadii(kCornerRadius),
									kColorPanel, kColorBorder, 1.0f);

	const float textX = panel.X + kPadding;
	float baselineY = panel.Y + kPadding + font.GetAscent();

	char heading[64];
	std::snprintf(heading, sizeof(heading), "Frame %.2f ms   F1 hide   F2 reset", CProfiler::GetFrameMs());
	DrawText(drawList, font, textX, baselineY, heading,
			 CProfiler::GetFrameMs() > kFrameBudgetMs ? kColorOverBudget : kColorHeading);

	baselineY += lineHeight;
	DrawText(drawList, font, textX, baselineY, "scope                     avg    peak  n", kColorName);

	for (u32 i = 0; i < scopeCount; i += 1) {
		baselineY += lineHeight;
		DrawRow(drawList, CProfiler::GetScope(i), textX, baselineY);
	}
}

#endif // PULSAR_PROFILING
