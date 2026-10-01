#pragma once

#include <string_view>

#include "core/types.h"

class Assets;
class DrawList;
class Font;
class Texture;

namespace controls {

enum class ButtonStyle : u8 {
	neutral,
	accent,
	danger,
	ghost,
	danger_confirm,
};

Color confirm_red();

void draw_icon(DrawList &t_draw_list, Rect t_rect, const Texture *t_icon, Color t_tint);
void draw_x(DrawList &t_draw_list, Rect t_rect, Color t_color);
void draw_check(DrawList &t_draw_list, Rect t_rect, Color t_color);
void draw_checkbox(DrawList &t_draw_list, Rect t_box, bool t_checked, bool t_enabled, Color t_accent);
void draw_chevron(DrawList &t_draw_list, Rect t_rect, bool t_points_up, Color t_color);
void draw_lock(DrawList &t_draw_list, Rect t_rect, Color t_color, Color t_backdrop, bool t_open = false);

float keycap_width(const Font &t_font, std::string_view t_label);
float keycap_height(const Font &t_font);
void draw_keycap_frame(DrawList &t_draw_list, Rect t_cap, Color t_backdrop, u8 t_alpha);
void draw_keycap(DrawList &t_draw_list, const Font &t_font, Rect t_cap, std::string_view t_label, Color t_backdrop,
				 u8 t_alpha);
void draw_mouse_keycap(DrawList &t_draw_list, Rect t_cap, Color t_backdrop, u8 t_alpha);
Color keycap_label_color();
float shortcut_width(const Font &t_font, std::string_view t_combo);
void draw_shortcut(DrawList &t_draw_list, const Font &t_font, Vec2 t_right_center, std::string_view t_combo,
				   Color t_backdrop, u8 t_alpha);
void draw_magnifier(DrawList &t_draw_list, Rect t_rect, Color t_color);
void draw_eye(DrawList &t_draw_list, const Assets &t_assets, Rect t_rect, bool t_revealed, Color t_color);
void draw_favorite(DrawList &t_draw_list, const Assets &t_assets, Rect t_rect, bool t_filled, Color t_color);

void draw_lift(DrawList &t_draw_list, Rect t_rect, float t_radius, Color t_glow, u8 t_alpha);
void draw_circular_hover(DrawList &t_draw_list, Rect t_rect, Color t_glow, Color t_fill, u8 t_alpha);
void draw_panel_shadow(DrawList &t_draw_list, Rect t_panel, float t_radius, float t_amount);
void draw_popup_shadow(DrawList &t_draw_list, Rect t_popup, float t_radius, float t_amount);
void draw_field(DrawList &t_draw_list, Rect t_rect, float t_radius, Color t_border, Color t_fill, u8 t_alpha);
void draw_circular_countdown(DrawList &t_draw_list, Rect t_circle, float t_remaining, Color t_color);

void draw_dropdown(DrawList &t_draw_list, const Font &t_font, Rect t_rect, std::string_view t_label, bool t_open,
				   bool t_hovered, Color t_accent, u8 t_alpha);
void draw_button(DrawList &t_draw_list, const Font &t_font, Rect t_rect, std::string_view t_label, ButtonStyle t_style,
				 Color t_accent, bool t_enabled, bool t_hovered, u8 t_alpha);

}
