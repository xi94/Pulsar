#pragma once

#include <string_view>

#include "core/types.h"

class Assets;
class DrawList;
struct Font;
class Texture;
class TextInput;

namespace controls {

enum class ButtonStyle : u8 {
	Neutral,
	Accent,
	Danger,
	Ghost,
	DangerConfirm,
};

[[nodiscard]] auto confirm_red() -> Color;

auto draw_x(DrawList* t_draw_list, Rect t_rect, Color t_color) -> void;
auto draw_check(DrawList* t_draw_list, const Assets* t_assets, Rect t_rect, Color t_color) -> void;
auto draw_chevron(DrawList* t_draw_list, Rect t_rect, bool t_points_up, Color t_color) -> void;
auto draw_lock(DrawList* t_draw_list, Rect t_rect, Color t_color, Color t_backdrop, bool t_open = false) -> void;

[[nodiscard]] auto keycap_width(const Font& t_font, std::string_view t_label) -> float;
[[nodiscard]] auto keycap_height(const Font& t_font) -> float;
auto draw_keycap_frame(DrawList* t_draw_list, Rect t_cap, Color t_backdrop, u8 t_alpha) -> void;
auto draw_keycap(DrawList* t_draw_list, const Font& t_font, Rect t_cap, std::string_view t_label, Color t_backdrop, u8 t_alpha) -> void;
auto draw_mouse_keycap(DrawList* t_draw_list, Rect t_cap, Color t_backdrop, u8 t_alpha) -> void;
[[nodiscard]] auto keycap_label_color() -> Color;
[[nodiscard]] auto shortcut_width(const Font& t_font, std::string_view t_combo) -> float;
auto draw_shortcut(DrawList* t_draw_list, const Font& t_font, Vec2 t_right_center, std::string_view t_combo, Color t_backdrop, u8 t_alpha) -> void;
auto draw_magnifier(DrawList* t_draw_list, Rect t_rect, Color t_color) -> void;
auto draw_eye(DrawList* t_draw_list, const Assets* t_assets, Rect t_rect, bool t_revealed, Color t_color) -> void;
auto draw_favorite(DrawList* t_draw_list, const Assets* t_assets, Rect t_rect, bool t_filled, Color t_color) -> void;

auto draw_lift(DrawList* t_draw_list, Rect t_rect, float t_radius, Color t_glow, u8 t_alpha) -> void;
auto draw_circular_hover(DrawList* t_draw_list, Rect t_rect, Color t_glow, Color t_fill, u8 t_alpha) -> void;
auto draw_panel_shadow(DrawList* t_draw_list, Rect t_panel, float t_radius, float t_amount) -> void;
auto draw_popup_shadow(DrawList* t_draw_list, Rect t_popup, float t_radius, float t_amount) -> void;
auto draw_field(DrawList* t_draw_list, Rect t_rect, float t_radius, Color t_border, Color t_fill, u8 t_alpha) -> void;
auto draw_circular_countdown(DrawList* t_draw_list, Rect t_circle, float t_remaining, Color t_color) -> void;

[[nodiscard]] auto search_text_rect(Rect t_search, float t_inset) -> Rect;
[[nodiscard]] auto search_clear_rect(Rect t_search) -> Rect;
auto draw_search_field(DrawList* t_draw_list, const Font& t_font, Rect t_search, float t_inset, TextInput* t_input, Vec2 t_mouse, Color t_accent, u8 t_alpha)
	-> void;
auto draw_button(DrawList*        t_draw_list,
                 const Font&      t_font,
                 Rect             t_rect,
                 std::string_view t_label,
                 ButtonStyle      t_style,
                 Color            t_accent,
                 bool             t_enabled,
                 bool             t_hovered,
                 u8               t_alpha) -> void;

}
