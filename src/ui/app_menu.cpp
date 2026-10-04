#include "ui/app_menu.h"

#include <algorithm>
#include <cstdio>

#include "core/animation.h"
#include "core/app_identity.h"
#include "gfx/assets.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "os/installation.h"
#include "os/window.h"
#include "ui/controls.h"
#include "ui/text.h"
#include "ui/theme.h"
#include "ui/window_layout.h"

namespace {
constexpr float K_OPEN_EASE_RATE  = 20.0f;
constexpr float K_HOVER_EASE_RATE = 22.0f;

constexpr float K_MENU_X          = 8.0f;
constexpr float K_MENU_WIDTH      = 236.0f;
constexpr float K_MENU_PADDING    = 4.0f;
constexpr float K_MENU_RADIUS     = 10.0f;
constexpr float K_SLIDE_DISTANCE  = 6.0f;
constexpr float K_ITEM_RADIUS     = 6.0f;
constexpr float K_CONTENT_X       = 8.0f;
constexpr float K_SEPARATOR_GAP   = 4.0f;
constexpr float K_SEPARATOR_BLOCK = K_SEPARATOR_GAP * 2.0f + 1.0f;
constexpr float K_ICON_SIZE       = 16.0f;
constexpr float K_ICON_TEXT_GAP   = 9.0f;
constexpr float K_FOOTER_PADDING  = 6.0f;

struct MenuItem {
	CommandType command;
	const char* label;
	Asset       icon;
	const char* shortcut;
	bool        starts_group;
	bool        needs_unlock;
	bool        needs_installer;
};

constexpr MenuItem K_MENU_ITEMS[]{
	{CommandType::CheckForUpdates, "Check for updates", Asset::IconUpdate, "", false, false, false},
	{CommandType::OpenSettings, "Settings", Asset::IconSettings, PULSAR_SHORTCUT_KEY "+,", true, true, false},
	{CommandType::OpenDataFolder, "Open data folder", Asset::IconFolderOpen, "", false, false, false},
	{CommandType::LockVault, "Lock now", Asset::IconLock, PULSAR_SHORTCUT_KEY "+L", true, true, false},
	{CommandType::OpenSetup, "Setup", Asset::IconApp, "", true, false, true},
};

constexpr u32 K_ITEM_COUNT = static_cast<u32>(std::size(K_MENU_ITEMS));

[[nodiscard]] auto item_height(const Fonts* t_fonts) -> float
{
	return std::max(28.0f, t_fonts->body.line_height() + 8.0f);
}

[[nodiscard]] auto footer_height(const Fonts* t_fonts) -> float
{
	return t_fonts->secondary.line_height() + K_FOOTER_PADDING * 2.0f;
}

[[nodiscard]] auto item_offset(const Fonts* t_fonts, std::span<const u32> t_items, u32 t_slot) -> float
{
	float offset = 0.0f;

	for (u32 slot = 1; slot <= t_slot; slot += 1) {
		offset += item_height(t_fonts) + (K_MENU_ITEMS[t_items[slot]].starts_group ? K_SEPARATOR_BLOCK : 0.0f);
	}

	return offset;
}

[[nodiscard]] auto item_rect(const Fonts* t_fonts, std::span<const u32> t_items, Rect t_menu, u32 t_slot) -> Rect
{
	return Rect{t_menu.x + K_MENU_PADDING, t_menu.y + K_MENU_PADDING + item_offset(t_fonts, t_items, t_slot), t_menu.w - K_MENU_PADDING * 2.0f,
	            item_height(t_fonts)};
}
}

AppMenu::AppMenu(const Fonts* t_fonts, const Assets* t_assets, CommandQueue* t_commands)
	: m_fonts(t_fonts)
	, m_assets(t_assets)
	, m_commands(t_commands)
{
	static_assert(K_ITEM_COUNT <= K_MAX_ITEMS);

	for (u32 item = 0; item < K_ITEM_COUNT; item += 1) {
		if (K_MENU_ITEMS[item].needs_installer && !os::installation::is_supported()) continue;

		m_items[m_item_count] = item;
		m_item_count += 1;
	}
}

auto AppMenu::open(bool t_unlocked, std::string_view t_update_status, float t_anchor_x) -> void
{
	m_unlocked      = t_unlocked;
	m_update_status = t_update_status;
	m_anchor_x      = t_anchor_x;
	m_open          = true;
}

auto AppMenu::close() -> void
{
	m_open = false;
}

auto AppMenu::items() const -> std::span<const u32>
{
	return {m_items, m_item_count};
}

auto AppMenu::menu() const -> Rect
{
	const float content = item_offset(m_fonts, items(), m_item_count - 1) + item_height(m_fonts);
	const float height  = K_MENU_PADDING * 2.0f + content + K_SEPARATOR_BLOCK + footer_height(m_fonts);
	const float slide   = snapped_to_pixel((1.0f - m_open_amount) * -K_SLIDE_DISTANCE);

	return Rect{m_anchor_x + K_MENU_X, K_TITLE_BAR_HEIGHT + 4.0f + slide, K_MENU_WIDTH, height};
}

auto AppMenu::is_enabled(u32 t_item) const -> bool
{
	return !K_MENU_ITEMS[t_item].needs_unlock || m_unlocked;
}

auto AppMenu::update(float t_delta_seconds) -> void
{
	m_open_amount = animation::ease_toward(m_open_amount, m_open ? 1.0f : 0.0f, K_OPEN_EASE_RATE, t_delta_seconds);

	const Rect menu_area = menu();

	for (u32 slot = 0; slot < m_item_count; slot += 1) {
		const bool hovered = is_blocking() && is_enabled(m_items[slot]) && item_rect(m_fonts, items(), menu_area, slot).contains(m_mouse);
		m_item_hover[slot] = animation::ease_toward(m_item_hover[slot], hovered ? 1.0f : 0.0f, K_HOVER_EASE_RATE, t_delta_seconds);
	}
}

auto AppMenu::on_pointer_up(Vec2 t_point) -> bool
{
	if (!is_blocking()) return false;

	const Rect menu_area = menu();

	for (u32 slot = 0; slot < m_item_count; slot += 1) {
		if (is_enabled(m_items[slot]) && item_rect(m_fonts, items(), menu_area, slot).contains(t_point)) {
			m_commands->push(Command{.type = K_MENU_ITEMS[m_items[slot]].command});
			break;
		}
	}

	close();

	return true;
}

auto AppMenu::cursor() const -> CursorKind
{
	if (!is_blocking()) return CursorKind::Arrow;

	const Rect menu_area = menu();

	for (u32 slot = 0; slot < m_item_count; slot += 1) {
		if (is_enabled(m_items[slot]) && item_rect(m_fonts, items(), menu_area, slot).contains(m_mouse)) return CursorKind::Hand;
	}

	return CursorKind::Arrow;
}

auto AppMenu::draw(DrawList* t_draw_list) -> void
{
	if (m_open_amount <= 0.001f) return;

	const auto  alpha     = to_alpha(m_open_amount);
	const Rect  menu_area = menu();
	const Font& font      = m_fonts->body;
	const Font& hint_font = m_fonts->secondary;

	controls::draw_popup_shadow(t_draw_list, menu_area, K_MENU_RADIUS, m_open_amount);
	t_draw_list->add_bordered_rect(menu_area, rounded(K_MENU_RADIUS), faded(g_theme.popup, alpha), faded(g_theme.border, alpha), 1.0f);

	const auto separator_above = [&](float t_y) {
		t_draw_list->add_rect(Rect{menu_area.x + 1.0f, t_y - K_SEPARATOR_GAP - 1.0f, menu_area.w - 2.0f, 1.0f}, faded(g_theme.separator, alpha));
	};

	for (u32 slot = 0; slot < m_item_count; slot += 1) {
		const MenuItem& entry    = K_MENU_ITEMS[m_items[slot]];
		const Rect      item     = item_rect(m_fonts, items(), menu_area, slot);
		const bool      enabled  = is_enabled(m_items[slot]);
		const Color     backdrop = mix(g_theme.popup, hovered(g_theme.popup), m_item_hover[slot]);

		if (entry.starts_group) {
			separator_above(item.y);
		}

		if (m_item_hover[slot] > 0.001f) {
			t_draw_list->add_rounded_rect(item, rounded(K_ITEM_RADIUS), faded(backdrop, alpha));
		}

		const Color label      = faded(enabled ? g_theme.text : g_theme.text_faint, alpha);
		const Color icon_color = faded(enabled ? g_theme.text_dim : g_theme.text_faint, alpha);
		const Rect  icon{item.x + K_CONTENT_X, item.y + (item.h - K_ICON_SIZE) * 0.5f, K_ICON_SIZE, K_ICON_SIZE};

		t_draw_list->add_image(icon, m_assets->get(entry.icon), icon_color);

		draw_text(t_draw_list, font, Vec2{icon.right() + K_ICON_TEXT_GAP, font.centered_baseline(item)}, entry.label, label);

		if (*entry.shortcut != '\0') {
			controls::draw_shortcut(t_draw_list, hint_font, Vec2{item.right() - K_CONTENT_X, item.center().y}, entry.shortcut, faded(backdrop, alpha),
			                        static_cast<u8>(alpha * (enabled ? 1.0f : 0.5f)));
		}
	}

	const Rect last = item_rect(m_fonts, items(), menu_area, m_item_count - 1);
	const Rect footer{menu_area.x, last.bottom() + K_SEPARATOR_BLOCK, menu_area.w, footer_height(m_fonts)};
	separator_above(footer.y);

	char        version[48];
	const int   written  = std::snprintf(version, sizeof(version), "%s %s", K_APP_NAME, K_APP_VERSION);
	const float baseline = hint_font.centered_baseline(footer);
	const float inset    = K_MENU_PADDING + K_CONTENT_X;

	draw_text(t_draw_list, hint_font, Vec2{footer.x + inset, baseline}, std::string_view{version, static_cast<usize>(std::max(written, 0))},
	          faded(g_theme.text_faint, alpha));
	draw_text(t_draw_list, hint_font, Vec2{footer.right() - inset - text_width(hint_font, m_update_status), baseline}, m_update_status,
	          faded(g_theme.text_faint, alpha));
}
