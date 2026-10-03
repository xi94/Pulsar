#include "ui/context_menu.h"

#include <algorithm>
#include <cmath>

#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "ui/controls.h"
#include "ui/text.h"
#include "ui/theme.h"

namespace {
constexpr float K_MIN_MENU_WIDTH = 180.0f;
constexpr float K_MENU_PADDING   = 4.0f;
constexpr float K_MENU_RADIUS    = 10.0f;
constexpr float K_ITEM_RADIUS    = 6.0f;
constexpr float K_WINDOW_MARGIN  = 8.0f;
constexpr float K_LABEL_INSET    = 10.0f;
constexpr float K_SHORTCUT_GAP   = 24.0f;

[[nodiscard]] auto item_height(const Fonts* t_fonts) -> float
{
	return std::max(28.0f, t_fonts->body.line_height() + 8.0f);
}
}

ContextMenu::ContextMenu(const Fonts* t_fonts, CommandQueue* t_commands)
	: m_fonts(t_fonts)
	, m_commands(t_commands)
{
}

auto ContextMenu::open(Vec2 t_position, std::span<const ContextMenuItem> t_items, Vec2 t_window_size) -> void
{
	m_item_count = static_cast<u32>(std::min<usize>(t_items.size(), K_MAX_ITEMS));
	std::copy_n(t_items.begin(), m_item_count, m_items);

	float content = 0.0f;
	for (const ContextMenuItem& item : std::span{m_items, m_item_count}) {
		const float shortcut = item.shortcut.empty() ? 0.0f : K_SHORTCUT_GAP + controls::shortcut_width(m_fonts->secondary, item.shortcut);
		content              = std::max(content, text_width(m_fonts->body, item.label) + shortcut);
	}

	m_width = std::ceil(std::max(K_MIN_MENU_WIDTH, content + (K_LABEL_INSET + K_MENU_PADDING) * 2.0f));

	const float height = menu_rect().h;
	m_position.x       = std::clamp(t_position.x, K_WINDOW_MARGIN, std::max(K_WINDOW_MARGIN, t_window_size.x - K_WINDOW_MARGIN - m_width));
	m_position.y       = std::clamp(t_position.y, K_WINDOW_MARGIN, std::max(K_WINDOW_MARGIN, t_window_size.y - K_WINDOW_MARGIN - height));
	m_open             = true;
}

auto ContextMenu::close() -> void
{
	m_open = false;
}

auto ContextMenu::menu_rect() const -> Rect
{
	return Rect{m_position.x, m_position.y, m_width, K_MENU_PADDING * 2.0f + item_height(m_fonts) * m_item_count};
}

auto ContextMenu::item_rect(u32 t_index) const -> Rect
{
	const float height = item_height(m_fonts);

	return Rect{m_position.x + K_MENU_PADDING, m_position.y + K_MENU_PADDING + height * t_index, m_width - K_MENU_PADDING * 2.0f, height};
}

auto ContextMenu::item_at(Vec2 t_point) const -> i32
{
	for (u32 i = 0; i < m_item_count; i += 1) {
		if (m_items[i].enabled && item_rect(i).contains(t_point)) return static_cast<i32>(i);
	}

	return -1;
}

auto ContextMenu::on_pointer_up(Vec2 t_point) -> bool
{
	if (!m_open) return false;

	const i32 chosen = item_at(t_point);
	if (chosen >= 0) {
		m_commands->push(m_items[chosen].command);
	}

	close();

	return true;
}

auto ContextMenu::on_right_click(Vec2) -> bool
{
	close();

	return false;
}

auto ContextMenu::on_key_down(os::Key t_key) -> bool
{
	if (!m_open) return false;

	if (t_key == os::Key::Escape) {
		close();
	}

	return true;
}

auto ContextMenu::cursor() const -> CursorKind
{
	return m_open && item_at(m_mouse) >= 0 ? CursorKind::Hand : CursorKind::Arrow;
}

auto ContextMenu::draw(DrawList* t_draw_list) -> void
{
	if (!m_open) return;

	const Font& font = m_fonts->body;

	controls::draw_popup_shadow(t_draw_list, menu_rect(), K_MENU_RADIUS, 1.0f);
	t_draw_list->add_bordered_rect(menu_rect(), rounded(K_MENU_RADIUS), g_theme.popup, g_theme.border, 1.0f);

	for (u32 i = 0; i < m_item_count; i += 1) {
		const Rect             row  = item_rect(i);
		const ContextMenuItem& item = m_items[i];

		const bool  hovered_row = item.enabled && row.contains(m_mouse);
		const Color backdrop    = hovered_row ? hovered(g_theme.popup) : g_theme.popup;

		if (hovered_row) {
			t_draw_list->add_rounded_rect(row, rounded(K_ITEM_RADIUS), backdrop);
		}

		draw_text(t_draw_list, font, Vec2{row.x + K_LABEL_INSET, font.centered_baseline(row)}, item.label, item.enabled ? g_theme.text : g_theme.text_faint);

		if (!item.shortcut.empty()) {
			controls::draw_shortcut(t_draw_list, m_fonts->secondary, Vec2{row.right() - K_LABEL_INSET, row.center().y}, item.shortcut, backdrop,
			                        item.enabled ? 255 : 128);
		}
	}
}
