#include "ui/hoverable.h"

#include "core/animator.h"
#include "ui/draw_list.h"

namespace {
constexpr float kHoverableEaseRate = 14.0f;
}

void CHoverable::Update(bool hovered, bool pressed, float deltaSeconds)
{
	m_flHoverAmount = CAnimator::EaseToward(m_flHoverAmount, hovered ? 1.0f : 0.0f, kHoverableEaseRate, deltaSeconds);
	m_flPressAmount = CAnimator::EaseToward(m_flPressAmount, pressed ? 1.0f : 0.0f, kHoverableEaseRate, deltaSeconds);
}

void CHoverable::DrawLift(CDrawList &drawList, Rect rect, float radius, Color accent, u8 alpha)
{
	constexpr int kLayers = 5;
	constexpr float kMaxExpand = 9.0f;
	constexpr float kBaseLayerAlpha = 34.0f;

	for (int i = kLayers; i >= 1; i -= 1) {
		const float t = static_cast<float>(i) / static_cast<float>(kLayers);
		const float expand = kMaxExpand * t;
		const float falloff = (1.0f - t) * (1.0f - t);
		const auto layerAlpha = static_cast<u8>(kBaseLayerAlpha * falloff * (static_cast<float>(alpha) / 255.0f));
		if (layerAlpha == 0) continue;

		drawList.AddRectRoundedFilled(rect.X - expand, rect.Y - expand, rect.W + expand * 2.0f, rect.H + expand * 2.0f,
									  CDrawList::UniformRadii(radius + expand), ColorWithAlpha(accent, layerAlpha));
	}
}
