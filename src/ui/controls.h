#pragma once

#include <string_view>

#include "core/types.h"

class Assets;
class DrawList;
class Font;
class Texture;

namespace controls {

struct ButtonColors {
	Color fill;
	Color hover_fill;
	Color label;
	Color lift;
};

void draw_icon(DrawList &t_draw_list, Rect t_rect, const Texture *t_icon, Color t_tint);
void draw_x(DrawList &t_draw_list, Rect t_rect, Color t_color);
void draw_eye(DrawList &t_draw_list, const Assets &t_assets, Rect t_rect, bool t_revealed, Color t_color);

void draw_lift(DrawList &t_draw_list, Rect t_rect, float t_radius, Color t_glow, u8 t_alpha);
void draw_circular_hover(DrawList &t_draw_list, Rect t_rect, Color t_glow, Color t_fill, u8 t_alpha);
void draw_panel_shadow(DrawList &t_draw_list, Rect t_panel, float t_radius, float t_amount);
void draw_field(DrawList &t_draw_list, Rect t_rect, float t_radius, Color t_border, Color t_fill, u8 t_alpha);

void draw_accent_button(DrawList &t_draw_list, const Font &t_font, Rect t_rect, std::string_view t_label,
						Color t_accent, bool t_enabled, bool t_hovered, Color t_disabled_fill, Color t_disabled_label,
						u8 t_alpha);
void draw_button(DrawList &t_draw_list, const Font &t_font, Rect t_rect, std::string_view t_label,
				 const ButtonColors &t_colors, bool t_hovered, u8 t_alpha);

}
