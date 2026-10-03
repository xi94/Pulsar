#include "ui/window_layout.h"

#include <algorithm>
#include <cmath>

auto TitleBarLayout::button_rect(TitleBarButton t_button) const -> Rect
{
	const float right = width;

	if (dialog) {
		if (t_button == TitleBarButton::Minimize) {
			return Rect{right - K_TITLE_BAR_BUTTON_WIDTH * 2.0f, 0.0f, K_TITLE_BAR_BUTTON_WIDTH, K_TITLE_BAR_HEIGHT};
		}

		if (t_button == TitleBarButton::Close) {
			return Rect{right - K_TITLE_BAR_BUTTON_WIDTH, 0.0f, K_TITLE_BAR_BUTTON_WIDTH, K_TITLE_BAR_HEIGHT};
		}

		return Rect{};
	}

	switch (t_button) {
		case TitleBarButton::Menu:
			return Rect{0.0f, 0.0f, K_TITLE_BAR_BUTTON_WIDTH, K_TITLE_BAR_HEIGHT};
		case TitleBarButton::Search: {
			const float side       = update_visible ? update_width + K_SEARCH_BUTTON_MARGIN : 0.0f;
			const float left       = K_TITLE_BAR_BUTTON_WIDTH + std::max(K_SEARCH_BUTTON_SIDE_ROOM, side);
			const float limit      = right - K_TITLE_BAR_BUTTON_WIDTH * 3.0f - K_SEARCH_BUTTON_MARGIN;
			const float pill_width = std::min(K_SEARCH_BUTTON_WIDTH, limit - left);
			if (pill_width < K_SEARCH_BUTTON_MIN_WIDTH) return Rect{};

			const float x = std::clamp((right - pill_width) * 0.5f, left, limit - pill_width);

			return Rect{std::floor(x), 0.0f, pill_width, K_TITLE_BAR_HEIGHT};
		}
		case TitleBarButton::Update:
			return Rect{K_TITLE_BAR_BUTTON_WIDTH, 0.0f, update_width, K_TITLE_BAR_HEIGHT};
		case TitleBarButton::Minimize:
			return Rect{right - K_TITLE_BAR_BUTTON_WIDTH * 3.0f, 0.0f, K_TITLE_BAR_BUTTON_WIDTH, K_TITLE_BAR_HEIGHT};
		case TitleBarButton::Maximize:
			return Rect{right - K_TITLE_BAR_BUTTON_WIDTH * 2.0f, 0.0f, K_TITLE_BAR_BUTTON_WIDTH, K_TITLE_BAR_HEIGHT};
		case TitleBarButton::Close:
			return Rect{right - K_TITLE_BAR_BUTTON_WIDTH, 0.0f, K_TITLE_BAR_BUTTON_WIDTH, K_TITLE_BAR_HEIGHT};
		case TitleBarButton::None:
			break;
	}

	return Rect{};
}

auto TitleBarLayout::button_at(Vec2 t_point) const -> TitleBarButton
{
	constexpr TitleBarButton BUTTONS[]{
		TitleBarButton::Menu, TitleBarButton::Search, TitleBarButton::Update, TitleBarButton::Minimize, TitleBarButton::Maximize, TitleBarButton::Close,
	};

	for (const TitleBarButton button : BUTTONS) {
		if (button == TitleBarButton::Update && !update_visible) continue;
		if (button == TitleBarButton::Search && !search_visible) continue;

		if (button_rect(button).contains(t_point)) return button;
	}

	return TitleBarButton::None;
}

auto content_rect(Vec2 t_window_size) -> Rect
{
	return Rect{0.0f, K_TITLE_BAR_HEIGHT, t_window_size.x, std::max(0.0f, t_window_size.y - K_TITLE_BAR_HEIGHT - K_STATUS_BAR_HEIGHT)};
}
