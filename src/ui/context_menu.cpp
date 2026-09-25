#include "ui/context_menu.h"

#include <algorithm>

#include <Windows.h>

#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "ui/text.h"

namespace {
constexpr float menu_width = 200.0f;
constexpr float item_height = 32.0f;
constexpr float menu_padding = 6.0f;
constexpr float menu_radius = 10.0f;
constexpr float window_margin = 8.0f;
constexpr float label_inset = 14.0f;

constexpr Color color_background{30, 30, 34, 255};
constexpr Color color_border{60, 60, 66, 255};
constexpr Color color_hover{54, 46, 78, 255};
constexpr Color color_text{220, 220, 224, 255};
constexpr Color color_text_disabled{110, 110, 116, 255};
}

ContextMenu::ContextMenu(const Fonts &t_fonts, CommandQueue &t_commands)
	: m_fonts(t_fonts)
	, m_commands(t_commands)
{
}

void ContextMenu::open(Vec2 t_position, std::span<const ContextMenuItem> t_items, Vec2 t_window_size)
{
	m_item_count = static_cast<u32>(std::min<usize>(t_items.size(), max_items));
	std::copy_n(t_items.begin(), m_item_count, m_items);

	const float height = menu_rect().h;
	m_position.x =
		std::clamp(t_position.x, window_margin, std::max(window_margin, t_window_size.x - window_margin - menu_width));
	m_position.y =
		std::clamp(t_position.y, window_margin, std::max(window_margin, t_window_size.y - window_margin - height));
	m_open = true;
}

void ContextMenu::close()
{
	m_open = false;
}

Rect ContextMenu::menu_rect() const
{
	return Rect{m_position.x, m_position.y, menu_width, menu_padding * 2.0f + item_height * m_item_count};
}

Rect ContextMenu::item_rect(u32 t_index) const
{
	return Rect{m_position.x, m_position.y + menu_padding + item_height * t_index, menu_width, item_height};
}

i32 ContextMenu::item_at(Vec2 t_point) const
{
	for (u32 i = 0; i < m_item_count; i += 1) {
		if (m_items[i].enabled && item_rect(i).contains(t_point)) return static_cast<i32>(i);
	}

	return -1;
}

bool ContextMenu::on_pointer_up(Vec2 t_point)
{
	if (!m_open) return false;

	const i32 chosen = item_at(t_point);
	if (chosen >= 0) {
		m_commands.push(m_items[chosen].command);
	}

	close();

	return true;
}

bool ContextMenu::on_right_click(Vec2)
{
	close();

	return false;
}

bool ContextMenu::on_key_down(u32 t_key)
{
	if (!m_open) return false;

	if (t_key == VK_ESCAPE) {
		close();
	}

	return true;
}

CursorKind ContextMenu::cursor() const
{
	return m_open && item_at(m_mouse) >= 0 ? CursorKind::hand : CursorKind::arrow;
}

void ContextMenu::draw(DrawList &t_draw_list)
{
	if (!m_open) return;

	t_draw_list.add_bordered_rect(menu_rect(), rounded(menu_radius), color_background, color_border, 1.0f);

	const Font &font = m_fonts.body();

	for (u32 i = 0; i < m_item_count; i += 1) {
		const Rect row = item_rect(i);
		const ContextMenuItem &item = m_items[i];

		if (item.enabled && row.contains(m_mouse)) {
			t_draw_list.add_rounded_rect(row.inset(4.0f, 0.0f), rounded(6.0f), color_hover);
		}

		draw_text(t_draw_list, font, Vec2{row.x + label_inset, font.centered_baseline(row)}, item.label,
				  item.enabled ? color_text : color_text_disabled);
	}
}
