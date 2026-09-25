#include "ui/app_menu.h"

#include <span>

#include "core/animation.h"
#include "core/settings.h"
#include "gfx/assets.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "platform/window.h"
#include "ui/text.h"

namespace {
constexpr float open_ease_rate = 20.0f;
constexpr float hover_ease_rate = 22.0f;

constexpr float menu_x = 8.0f;
constexpr float menu_width = 226.0f;
constexpr float item_height = 34.0f;
constexpr float menu_padding = 6.0f;
constexpr float menu_radius = 12.0f;
constexpr float slide_distance = 6.0f;
constexpr float highlight_inset = 5.0f;
constexpr float content_x = 14.0f;
constexpr float separator_gap = 6.0f;
constexpr float separator_block = separator_gap * 2.0f + 1.0f;
constexpr float icon_size = 16.0f;
constexpr float icon_text_gap = 10.0f;
constexpr float baseline_nudge = 2.0f;

constexpr Color color_background{30, 30, 34, 255};
constexpr Color color_border{60, 60, 66, 255};
constexpr Color color_separator{52, 52, 58, 255};
constexpr Color color_text{220, 220, 224, 255};
constexpr Color color_text_disabled{100, 100, 106, 255};

struct MenuItem {
	CommandType command;
	const char *label;
	Asset icon;
	bool starts_group;
};

constexpr MenuItem menu_items[]{
	{CommandType::check_for_updates, "Check for Updates", Asset::icon_update, false},
	{CommandType::open_settings, "Settings", Asset::icon_settings, true},
	{CommandType::open_data_folder, "Open Data Folder", Asset::icon_folder, false},
};

constexpr u32 item_count = static_cast<u32>(std::size(menu_items));

float item_offset(u32 t_item)
{
	float offset = 0.0f;

	for (u32 i = 1; i <= t_item; i += 1) {
		offset += item_height + (menu_items[i].starts_group ? separator_block : 0.0f);
	}

	return offset;
}

Rect menu_rect(float t_open_amount)
{
	const float height = menu_padding * 2.0f + item_offset(item_count - 1) + item_height;
	const float slide = (1.0f - t_open_amount) * -slide_distance;

	return Rect{menu_x, title_bar_height + 4.0f + slide, menu_width, height};
}

Rect item_rect(Rect t_menu, u32 t_item)
{
	return Rect{t_menu.x, t_menu.y + menu_padding + item_offset(t_item), t_menu.w, item_height};
}
}

AppMenu::AppMenu(const Settings &t_settings, const Fonts &t_fonts, const Assets &t_assets, CommandQueue &t_commands)
	: m_settings(t_settings)
	, m_fonts(t_fonts)
	, m_assets(t_assets)
	, m_commands(t_commands)
{
	static_assert(item_count <= max_items);
}

void AppMenu::open(bool t_settings_available)
{
	m_settings_available = t_settings_available;
	m_open = true;
}

void AppMenu::close()
{
	m_open = false;
}

bool AppMenu::is_enabled(u32 t_item) const
{
	return menu_items[t_item].command != CommandType::open_settings || m_settings_available;
}

void AppMenu::update(float t_delta_seconds)
{
	m_open_amount = animation::ease_toward(m_open_amount, m_open ? 1.0f : 0.0f, open_ease_rate, t_delta_seconds);

	const Rect menu = menu_rect(m_open_amount);

	for (u32 i = 0; i < item_count; i += 1) {
		const bool hovered = is_blocking() && is_enabled(i) && item_rect(menu, i).contains(m_mouse);
		m_item_hover[i] =
			animation::ease_toward(m_item_hover[i], hovered ? 1.0f : 0.0f, hover_ease_rate, t_delta_seconds);
	}
}

bool AppMenu::on_pointer_up(Vec2 t_point)
{
	if (!is_blocking()) return false;

	const Rect menu = menu_rect(m_open_amount);

	for (u32 i = 0; i < item_count; i += 1) {
		if (is_enabled(i) && item_rect(menu, i).contains(t_point)) {
			m_commands.push(Command{.type = menu_items[i].command});
			break;
		}
	}

	close();

	return true;
}

CursorKind AppMenu::cursor() const
{
	if (!is_blocking()) return CursorKind::arrow;

	const Rect menu = menu_rect(m_open_amount);

	for (u32 i = 0; i < item_count; i += 1) {
		if (is_enabled(i) && item_rect(menu, i).contains(m_mouse)) return CursorKind::hand;
	}

	return CursorKind::arrow;
}

void AppMenu::draw(DrawList &t_draw_list)
{
	if (m_open_amount <= 0.001f) return;

	const auto alpha = static_cast<u8>(255.0f * m_open_amount);
	const Rect menu = menu_rect(m_open_amount);
	const Font &font = m_fonts.body();

	t_draw_list.add_bordered_rect(menu, rounded(menu_radius), with_alpha(color_background, alpha),
								  with_alpha(color_border, alpha), 1.0f);

	for (u32 i = 0; i < item_count; i += 1) {
		const Rect item = item_rect(menu, i);

		if (menu_items[i].starts_group) {
			const Rect separator{menu.x + content_x, item.y - separator_gap - 1.0f, menu.w - content_x * 2.0f, 1.0f};
			t_draw_list.add_rect(separator, with_alpha(color_separator, alpha));
		}

		if (m_item_hover[i] > 0.001f) {
			const Color hover_color = mix(color_background, m_settings.accent, 0.28f);
			const auto hover_alpha = static_cast<u8>(alpha * m_item_hover[i]);
			t_draw_list.add_rounded_rect(item.inset(highlight_inset, 0.0f), rounded(7.0f),
										 with_alpha(hover_color, hover_alpha));
		}

		const Color content_color = with_alpha(is_enabled(i) ? color_text : color_text_disabled, alpha);
		const Rect icon{item.x + content_x, item.y + (item.h - icon_size) * 0.5f, icon_size, icon_size};
		const float baseline = font.centered_baseline(item) - baseline_nudge;

		t_draw_list.add_image(icon, m_assets.get(menu_items[i].icon), content_color);
		draw_text(t_draw_list, font, Vec2{icon.right() + icon_text_gap, baseline}, menu_items[i].label, content_color);
	}
}
