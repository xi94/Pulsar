#pragma once

#include <string_view>

#include "core/types.h"

class DrawList;
struct Fonts;

class Tooltip {
  public:
	auto request(std::string_view t_text, Rect t_anchor) -> void;
	auto update(float t_delta_seconds) -> void;
	auto draw(DrawList* t_draw_list, const Fonts* t_fonts, Rect t_bounds, u8 t_alpha) const -> void;
	auto reset() -> void;

  private:
	char  m_text[128]{};
	usize m_length = 0;
	Rect  m_anchor{};
	bool  m_requested_this_frame = false;
	float m_hover_seconds        = 0.0f;
	float m_visible_amount       = 0.0f;
};
