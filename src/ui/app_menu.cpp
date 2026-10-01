#include "ui/app_menu.h"

#include <algorithm>
#include <cstdio>
#include <span>

#include "core/animation.h"
#include "core/app_identity.h"
#include "gfx/assets.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "platform/window.h"
#include "ui/controls.h"
#include "ui/text.h"
#include "ui/theme.h"

namespace {
constexpr float open_ease_rate = 20.0f;
constexpr float hover_ease_rate = 22.0f;

constexpr float menu_x = 8.0f;
constexpr float menu_width = 236.0f;
constexpr float menu_padding = 4.0f;
constexpr float menu_radius = 10.0f;
constexpr float slide_distance = 6.0f;
constexpr float item_radius = 6.0f;
constexpr float content_x = 8.0f;
constexpr float separator_gap = 4.0f;
constexpr float separator_block = separator_gap * 2.0f + 1.0f;
constexpr float icon_size = 16.0f;
constexpr float icon_text_gap = 9.0f;
constexpr float footer_padding = 6.0f;

struct MenuItem {
	CommandType command;
	const char *label;
	Asset icon;
	const char *shortcut;
	bool starts_group;
	bool needs_unlock;
};

constexpr MenuItem menu_items[]{
	{CommandType::CheckForUpdates, "Check for updates", Asset::IconUpdate, "", false, false},
	{CommandType::OpenSettings, "Settings", Asset::IconSettings, "Ctrl+,", true, true},
	{CommandType::OpenDataFolder, "Open data folder", Asset::IconFolderOpen, "", false, false},
	{CommandType::LockVault, "Lock now", Asset::IconLock, "Ctrl+L", true, true},
	{CommandType::OpenSetup, "Setup", Asset::IconApp, "", true, false},
};

constexpr u32 item_count = static_cast<u32>(std::size(menu_items));

float item_height(const Fonts &t_fonts)
{
	return std::max(28.0f, t_fonts.body.line_height() + 8.0f);
}

float footer_height(const Fonts &t_fonts)
{
	return t_fonts.secondary.line_height() + footer_padding * 2.0f;
}

float item_offset(const Fonts &t_fonts, u32 t_item)
{
	float offset = 0.0f;

	for (u32 i = 1; i <= t_item; i += 1) {
		offset += item_height(t_fonts) + (menu_items[i].starts_group ? separator_block : 0.0f);
	}

	return offset;
}

Rect menu_rect(const Fonts &t_fonts, float t_open_amount)
{
	const float items = item_offset(t_fonts, item_count - 1) + item_height(t_fonts);
	const float height = menu_padding * 2.0f + items + separator_block + footer_height(t_fonts);
	const float slide = snapped_to_pixel((1.0f - t_open_amount) * -slide_distance);

	return Rect{menu_x, title_bar_height + 4.0f + slide, menu_width, height};
}

Rect item_rect(const Fonts &t_fonts, Rect t_menu, u32 t_item)
{
	return Rect{t_menu.x + menu_padding, t_menu.y + menu_padding + item_offset(t_fonts, t_item),
				t_menu.w - menu_padding * 2.0f, item_height(t_fonts)};
}
}

AppMenu::AppMenu(const Fonts &t_fonts, const Assets &t_assets, CommandQueue &t_commands)
	: m_fonts(t_fonts)
	, m_assets(t_assets)
	, m_commands(t_commands)
{
	static_assert(item_count <= max_items);
}

void AppMenu::open(bool t_unlocked, std::string_view t_update_status)
{
	m_unlocked = t_unlocked;
	m_update_status = t_update_status;
	m_open = true;
}

void AppMenu::close()
{
	m_open = false;
}

bool AppMenu::is_enabled(u32 t_item) const
{
	return !menu_items[t_item].needs_unlock || m_unlocked;
}

void AppMenu::update(float t_delta_seconds)
{
	m_open_amount = animation::ease_toward(m_open_amount, m_open ? 1.0f : 0.0f, open_ease_rate, t_delta_seconds);

	const Rect menu = menu_rect(m_fonts, m_open_amount);

	for (u32 i = 0; i < item_count; i += 1) {
		const bool hovered = is_blocking() && is_enabled(i) && item_rect(m_fonts, menu, i).contains(m_mouse);
		m_item_hover[i] =
			animation::ease_toward(m_item_hover[i], hovered ? 1.0f : 0.0f, hover_ease_rate, t_delta_seconds);
	}
}

bool AppMenu::on_pointer_up(Vec2 t_point)
{
	if (!is_blocking()) return false;

	const Rect menu = menu_rect(m_fonts, m_open_amount);

	for (u32 i = 0; i < item_count; i += 1) {
		if (is_enabled(i) && item_rect(m_fonts, menu, i).contains(t_point)) {
			m_commands.push(Command{.type = menu_items[i].command});
			break;
		}
	}

	close();

	return true;
}

CursorKind AppMenu::cursor() const
{
	if (!is_blocking()) return CursorKind::Arrow;

	const Rect menu = menu_rect(m_fonts, m_open_amount);

	for (u32 i = 0; i < item_count; i += 1) {
		if (is_enabled(i) && item_rect(m_fonts, menu, i).contains(m_mouse)) return CursorKind::Hand;
	}

	return CursorKind::Arrow;
}

void AppMenu::draw(DrawList &t_draw_list)
{
	if (m_open_amount <= 0.001f) return;

	const Theme &colors = theme();
	const auto alpha = to_alpha(m_open_amount);
	const Rect menu = menu_rect(m_fonts, m_open_amount);
	const Font &font = m_fonts.body;
	const Font &hint_font = m_fonts.secondary;

	controls::draw_popup_shadow(t_draw_list, menu, menu_radius, m_open_amount);
	t_draw_list.add_bordered_rect(menu, rounded(menu_radius), faded(colors.popup, alpha), faded(colors.border, alpha),
								  1.0f);

	const auto separator_above = [&](float t_y) {
		t_draw_list.add_rect(Rect{menu.x + 1.0f, t_y - separator_gap - 1.0f, menu.w - 2.0f, 1.0f},
							 faded(colors.separator, alpha));
	};

	for (u32 i = 0; i < item_count; i += 1) {
		const MenuItem &entry = menu_items[i];
		const Rect item = item_rect(m_fonts, menu, i);
		const bool enabled = is_enabled(i);
		const Color backdrop = mix(colors.popup, hovered(colors.popup), m_item_hover[i]);

		if (entry.starts_group) {
			separator_above(item.y);
		}

		if (m_item_hover[i] > 0.001f) {
			t_draw_list.add_rounded_rect(item, rounded(item_radius), faded(backdrop, alpha));
		}

		const Color label = faded(enabled ? colors.text : colors.text_faint, alpha);
		const Color icon_color = faded(enabled ? colors.text_dim : colors.text_faint, alpha);
		const Rect icon{item.x + content_x, item.y + (item.h - icon_size) * 0.5f, icon_size, icon_size};

		t_draw_list.add_image(icon, m_assets.get(entry.icon), icon_color);

		draw_text(t_draw_list, font, Vec2{icon.right() + icon_text_gap, font.centered_baseline(item)}, entry.label,
				  label);

		if (*entry.shortcut != '\0') {
			controls::draw_shortcut(t_draw_list, hint_font, Vec2{item.right() - content_x, item.center().y},
									entry.shortcut, faded(backdrop, alpha),
									static_cast<u8>(alpha * (enabled ? 1.0f : 0.5f)));
		}
	}

	const Rect last = item_rect(m_fonts, menu, item_count - 1);
	const Rect footer{menu.x, last.bottom() + separator_block, menu.w, footer_height(m_fonts)};
	separator_above(footer.y);

	char version[48];
	const int written = std::snprintf(version, sizeof(version), "%s %s", app_name, app_version);
	const float baseline = hint_font.centered_baseline(footer);
	const float inset = menu_padding + content_x;

	draw_text(t_draw_list, hint_font, Vec2{footer.x + inset, baseline},
			  std::string_view{version, static_cast<usize>(std::max(written, 0))}, faded(colors.text_faint, alpha));
	draw_text(t_draw_list, hint_font, Vec2{footer.right() - inset - text_width(hint_font, m_update_status), baseline},
			  m_update_status, faded(colors.text_faint, alpha));
}
