#include "ui/window_layout.h"

#include <algorithm>
#include <cmath>

auto TitleBarLayout::menu_on_right() const -> bool
{
	return native_controls_width > 0.0f;
}

auto TitleBarLayout::button_rect(TitleBarButton t_button) const -> Rect
{
	const bool  window_button = t_button == TitleBarButton::MINIMIZE || t_button == TitleBarButton::MAXIMIZE || t_button == TitleBarButton::CLOSE;
	const float right         = width;
	const float leading       = native_controls_width;
	const float trailing      = menu_on_right() ? 0.0f : K_TITLE_BAR_BUTTON_WIDTH * 3.0f;

	if (menu_on_right() && window_button) return Rect{};

	if (dialog) {
		if (t_button == TitleBarButton::MINIMIZE) {
			return Rect{right - K_TITLE_BAR_BUTTON_WIDTH * 2.0f, 0.0f, K_TITLE_BAR_BUTTON_WIDTH, K_TITLE_BAR_HEIGHT};
		}

		if (t_button == TitleBarButton::CLOSE) {
			return Rect{right - K_TITLE_BAR_BUTTON_WIDTH, 0.0f, K_TITLE_BAR_BUTTON_WIDTH, K_TITLE_BAR_HEIGHT};
		}

		return Rect{};
	}

	switch (t_button) {
		using enum TitleBarButton;

		case MENU: {
			const float x = menu_on_right() ? right - K_TITLE_BAR_BUTTON_WIDTH : leading;

			return Rect{x, 0.0f, K_TITLE_BAR_BUTTON_WIDTH, K_TITLE_BAR_HEIGHT};
		}

		case SEARCH: {
			const float status     = update_visible ? update_width + K_SEARCH_BUTTON_MARGIN : 0.0f;
			const float menu_side  = K_TITLE_BAR_BUTTON_WIDTH + std::max(K_SEARCH_BUTTON_SIDE_ROOM, status);
			const float left       = menu_on_right() ? leading + identity_width + K_SEARCH_BUTTON_MARGIN : leading + menu_side;
			const float limit      = menu_on_right() ? right - menu_side : right - trailing - K_SEARCH_BUTTON_MARGIN;
			const float pill_width = std::min(K_SEARCH_BUTTON_WIDTH, limit - left);
			if (pill_width < K_SEARCH_BUTTON_MIN_WIDTH) return Rect{};

			const float x = std::clamp((right - pill_width) * 0.5f, left, limit - pill_width);

			return Rect{std::floor(x), 0.0f, pill_width, K_TITLE_BAR_HEIGHT};
		}

		case UPDATE: {
			const float x = menu_on_right() ? right - K_TITLE_BAR_BUTTON_WIDTH - update_width : leading + K_TITLE_BAR_BUTTON_WIDTH;

			return Rect{x, 0.0f, update_width, K_TITLE_BAR_HEIGHT};
		}

		case MINIMIZE: {
			return Rect{right - K_TITLE_BAR_BUTTON_WIDTH * 3.0f, 0.0f, K_TITLE_BAR_BUTTON_WIDTH, K_TITLE_BAR_HEIGHT};
		}

		case MAXIMIZE: {
			return Rect{right - K_TITLE_BAR_BUTTON_WIDTH * 2.0f, 0.0f, K_TITLE_BAR_BUTTON_WIDTH, K_TITLE_BAR_HEIGHT};
		}

		case CLOSE: {
			return Rect{right - K_TITLE_BAR_BUTTON_WIDTH, 0.0f, K_TITLE_BAR_BUTTON_WIDTH, K_TITLE_BAR_HEIGHT};
		}

		case NONE: {
			break;
		}
	}

	return Rect{};
}

auto TitleBarLayout::button_at(Vec2 t_point) const -> TitleBarButton
{
	constexpr TitleBarButton BUTTONS[]{
		TitleBarButton::MENU, TitleBarButton::SEARCH, TitleBarButton::UPDATE, TitleBarButton::MINIMIZE, TitleBarButton::MAXIMIZE, TitleBarButton::CLOSE,
	};

	for (const TitleBarButton button : BUTTONS) {
		if (button == TitleBarButton::UPDATE && !update_visible) continue;
		if (button == TitleBarButton::SEARCH && !search_visible) continue;

		if (button_rect(button).contains(t_point)) return button;
	}

	return TitleBarButton::NONE;
}

auto content_rect(Vec2 t_window_size) -> Rect
{
	return Rect{0.0f, K_TITLE_BAR_HEIGHT, t_window_size.x, std::max(0.0f, t_window_size.y - K_TITLE_BAR_HEIGHT - K_STATUS_BAR_HEIGHT)};
}
