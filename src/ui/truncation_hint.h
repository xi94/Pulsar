#pragma once

#include <string_view>

#include "core/types.h"

class DrawList;
class Fonts;

class TruncationHint {
  public:
	explicit TruncationHint(const Fonts &t_fonts);

	void capture(const DrawList &t_draw_list);
	void update(float t_delta_seconds, bool t_suppressed);
	void draw(DrawList &t_draw_list, Rect t_bounds) const;

  private:
	const Fonts &m_fonts;

	char m_text[512]{};
	usize m_length = 0;
	Rect m_anchor{};
	bool m_requested = false;
	float m_hover_seconds = 0.0f;
	float m_visible_amount = 0.0f;
};
