#pragma once

#include "core/types.h"

constexpr float K_TITLE_BAR_HEIGHT        = 40.0f;
constexpr float K_TITLE_BAR_BUTTON_WIDTH  = 46.0f;
constexpr float K_UPDATE_BUTTON_WIDTH     = 170.0f;
constexpr float K_SEARCH_BUTTON_WIDTH     = 300.0f;
constexpr float K_SEARCH_BUTTON_MIN_WIDTH = 150.0f;
constexpr float K_SEARCH_BUTTON_SIDE_ROOM = 120.0f;
constexpr float K_SEARCH_BUTTON_MARGIN    = 16.0f;
constexpr float K_STATUS_BAR_HEIGHT       = 26.0f;
constexpr float K_MIN_WINDOW_WIDTH        = 640.0f;
constexpr float K_MIN_WINDOW_HEIGHT       = 440.0f;
constexpr Color K_TITLE_BAR_CLOSE_HOVER{232, 17, 35, 255};
constexpr Color K_TITLE_BAR_CLOSE_GLYPH_HOVER{255, 255, 255, 255};
constexpr u8    K_TITLE_BAR_HOVER_ALPHA = 18;

enum class TitleBarButton : u8 {
	NONE,
	MENU,
	SEARCH,
	UPDATE,
	MINIMIZE,
	MAXIMIZE,
	CLOSE,
};

struct TitleBarLayout {
	float width                 = 0.0f;
	float native_controls_width = 0.0f;
	bool  dialog                = false;
	bool  update_visible        = false;
	float update_width          = K_UPDATE_BUTTON_WIDTH;
	bool  search_visible        = false;

	[[nodiscard]] auto menu_on_right() const -> bool;
	[[nodiscard]] auto button_rect(TitleBarButton t_button) const -> Rect;
	[[nodiscard]] auto button_at(Vec2 t_point) const -> TitleBarButton;
};

[[nodiscard]] auto content_rect(Vec2 t_window_size) -> Rect;
