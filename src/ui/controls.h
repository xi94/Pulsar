#pragma once

#include <string_view>

#include "core/types.h"

struct Account;
class Assets;
class DrawList;
struct Font;
class Texture;
class TextInput;

namespace controls {

// Popups keep the corners they have at 40% whatever the Corner roundness setting says, because rounder ones look toy-like.
// Buttons and fields inside them still follow the setting.
constexpr float K_POPUP_ROUNDNESS = 0.4f;

enum class ButtonStyle : u8 {
	NEUTRAL,
	ACCENT,
	DANGER,
	GHOST,
	DANGER_CONFIRM,
};

// Each kind of glass surface blurs and tints by its own amount, and the Glass settings move them all together. Small surfaces over
// text stay light, menus and panels blur more so they read as one pane, the account popup's large sheet is the most opaque so a
// long list stays readable, and the window behind a modal blurs most.
enum class GlassSurface : u8 {
	SOFT,
	MENU,
	PANEL,
	SHEET,
	BACKDROP,
};

[[nodiscard]] auto confirm_red() -> Color;
[[nodiscard]] auto caution_color() -> Color;
[[nodiscard]] auto ink_on(Color t_background) -> Color;

enum class NoticeKind : u8 {
	CAPS_LOCK,
	ALERT,
};

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
auto draw_caps_lock(DrawList* t_draw_list, Rect t_rect, Color t_color) -> void;
auto draw_alert(DrawList* t_draw_list, Rect t_rect, Color t_color) -> void;
[[nodiscard]] auto notice_width(const Font& t_font, std::string_view t_text) -> float;
auto draw_notice(DrawList* t_draw_list, const Font& t_font, Vec2 t_top_left, NoticeKind t_kind, std::string_view t_text, Color t_color) -> void;
auto draw_eye(DrawList* t_draw_list, const Assets* t_assets, Rect t_rect, bool t_revealed, Color t_color) -> void;
auto draw_favorite(DrawList* t_draw_list, const Assets* t_assets, Rect t_rect, bool t_filled, Color t_color) -> void;

auto draw_lift(DrawList* t_draw_list, Rect t_rect, float t_radius, Color t_glow, u8 t_alpha) -> void;
auto draw_circular_hover(DrawList* t_draw_list, Rect t_rect, Color t_glow, Color t_fill, u8 t_alpha) -> void;
auto draw_panel_shadow(DrawList* t_draw_list, Rect t_panel, float t_radius, float t_amount) -> void;
auto draw_popup_shadow(DrawList* t_draw_list, Rect t_popup, float t_radius, float t_amount) -> void;

// Frosted glass: a blurred copy of what is behind, tinted with the popup colour. With glass off, or no blur support, it is plain popup
// colour.
auto set_glass(bool t_supported, bool t_enabled, float t_tint, float t_blur) -> void;
[[nodiscard]] auto glass_highlight(float t_amount = 1.0f) -> Color;
// Panels over the whole window blur it as well as dimming it, so they need less of the usual scrim.
auto draw_popup_backdrop(DrawList* t_draw_list, Rect t_rect, float t_amount) -> void;
auto draw_glass(DrawList* t_draw_list, Rect t_rect, CornerRadii t_radii, GlassSurface t_surface, u8 t_alpha, bool t_framed = true) -> void;
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

[[nodiscard]] auto region_chip_width(const Font& t_font, std::string_view t_region) -> float;
auto draw_region_chip(DrawList* t_draw_list, const Font& t_font, float t_x, float t_center_y, std::string_view t_region, u8 t_alpha) -> void;
auto draw_account_details(DrawList* t_draw_list, const Font& t_font, Vec2 t_baseline, float t_max_width, const Account& t_account, u8 t_alpha) -> void;
auto draw_dotted(DrawList*        t_draw_list,
                 const Font&      t_font,
                 Vec2             t_baseline,
                 std::string_view t_first,
                 std::string_view t_second,
                 float            t_max_width,
                 Color            t_color) -> void;

}
