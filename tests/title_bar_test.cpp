#include "test.h"
#include "ui/window_layout.h"

namespace {
constexpr float K_TRAFFIC_LIGHTS_WIDTH = 72.0f;

constexpr TitleBarButton K_BUTTONS[]{
	TitleBarButton::MENU, TitleBarButton::SEARCH, TitleBarButton::UPDATE, TitleBarButton::MINIMIZE, TitleBarButton::MAXIMIZE, TitleBarButton::CLOSE,
};

[[nodiscard]] auto overlaps(Rect t_a, Rect t_b) -> bool
{
	return t_a.w > 0.0f && t_b.w > 0.0f && t_a.x < t_b.right() && t_b.x < t_a.right();
}

[[nodiscard]] auto windows_layout(float t_width, bool t_update_visible) -> TitleBarLayout
{
	return TitleBarLayout{.width = t_width, .update_visible = t_update_visible, .search_visible = true};
}

[[nodiscard]] auto macos_layout(float t_width, bool t_update_visible) -> TitleBarLayout
{
	return TitleBarLayout{.width = t_width, .native_controls_width = K_TRAFFIC_LIGHTS_WIDTH, .update_visible = t_update_visible, .search_visible = true};
}

[[nodiscard]] auto shown_rect(const TitleBarLayout& t_layout, TitleBarButton t_button) -> Rect
{
	const bool hidden = (t_button == TitleBarButton::UPDATE && !t_layout.update_visible) || (t_button == TitleBarButton::SEARCH && !t_layout.search_visible);

	return hidden ? Rect{} : t_layout.button_rect(t_button);
}

[[nodiscard]] auto nothing_overlaps(const TitleBarLayout& t_layout) -> bool
{
	const Rect native_controls{0.0f, 0.0f, t_layout.native_controls_width, K_TITLE_BAR_HEIGHT};

	for (const TitleBarButton first : K_BUTTONS) {
		const Rect area = shown_rect(t_layout, first);
		if (area.w > 0.0f && (area.x < 0.0f || area.right() > t_layout.width || overlaps(area, native_controls))) return false;

		for (const TitleBarButton second : K_BUTTONS) {
			if (first != second && overlaps(area, shown_rect(t_layout, second))) return false;
		}
	}

	return true;
}
}

TEST_CASE("Windows: the menu sits at the left edge and the window buttons at the right")
{
	const TitleBarLayout layout = windows_layout(1000.0f, false);

	CHECK(!layout.menu_on_right());
	CHECK(layout.button_rect(TitleBarButton::MENU).x == 0.0f);
	CHECK(layout.button_rect(TitleBarButton::CLOSE).right() == 1000.0f);
	CHECK(layout.button_rect(TitleBarButton::MAXIMIZE).right() == layout.button_rect(TitleBarButton::CLOSE).x);
}

TEST_CASE("macOS: the menu sits at the right edge, the status beside it, and no window buttons are drawn")
{
	const TitleBarLayout layout = macos_layout(1000.0f, true);
	const Rect           menu   = layout.button_rect(TitleBarButton::MENU);

	CHECK(layout.menu_on_right());
	CHECK(menu.right() == 1000.0f);
	CHECK(layout.button_rect(TitleBarButton::UPDATE).right() == menu.x);
	CHECK(layout.button_rect(TitleBarButton::CLOSE).w == 0.0f);
	CHECK(layout.button_rect(TitleBarButton::MINIMIZE).w == 0.0f);
}

TEST_CASE("nothing in the title bar overlaps at any width")
{
	for (float width = K_MIN_WINDOW_WIDTH; width <= 2000.0f; width += 37.0f) {
		for (const bool update_visible : {false, true}) {
			CHECK(nothing_overlaps(windows_layout(width, update_visible)));
			CHECK(nothing_overlaps(macos_layout(width, update_visible)));
		}
	}
}

TEST_CASE("the search pill is centred when the window is wide")
{
	for (const TitleBarLayout& layout : {windows_layout(1400.0f, false), macos_layout(1400.0f, false)}) {
		const Rect search = layout.button_rect(TitleBarButton::SEARCH);

		CHECK(search.w == K_SEARCH_BUTTON_WIDTH);
		CHECK(search.center().x > 699.0f);
		CHECK(search.center().x < 701.0f);
	}
}

TEST_CASE("clicks find the button under them")
{
	for (const TitleBarLayout& layout : {windows_layout(1000.0f, true), macos_layout(1000.0f, true)}) {
		for (const TitleBarButton button : {TitleBarButton::MENU, TitleBarButton::SEARCH, TitleBarButton::UPDATE}) {
			CHECK(layout.button_at(layout.button_rect(button).center()) == button);
		}
	}
}
