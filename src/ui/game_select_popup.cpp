#include "ui/game_select_popup.h"

#include <algorithm>
#include <bit>

#include "core/animation.h"
#include "core/library.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "ui/text.h"

namespace {
constexpr float popup_width = 220.0f;
constexpr float popup_padding = 6.0f;
constexpr float popup_radius = 10.0f;
constexpr float bounds_margin = 8.0f;
constexpr float anchor_gap = 6.0f;
constexpr float row_padding_x = 10.0f;
constexpr float open_ease_rate = 20.0f;

constexpr Color color_background{30, 30, 34, 255};
constexpr Color color_border{60, 60, 66, 255};
constexpr Color color_hover{46, 46, 52, 255};
constexpr Color color_text{220, 220, 224, 255};
constexpr Color color_text_disabled{130, 130, 136, 255};
constexpr Color color_check{130, 200, 140, 255};
constexpr Color color_check_disabled{110, 118, 112, 255};
constexpr Color color_icon{255, 255, 255, 255};
constexpr Color color_icon_disabled{150, 150, 150, 255};

float row_height_for(const Fonts &t_fonts)
{
	return std::max(30.0f, t_fonts.secondary().line_height() + 14.0f);
}

float icon_size_for(const Fonts &t_fonts)
{
	return t_fonts.secondary().line_height() * 0.95f;
}

void draw_check(DrawList &t_draw_list, Rect t_row, float t_scale, Color t_color)
{
	const float x = t_row.right() - row_padding_x - 8.0f * t_scale;
	const float y = t_row.center().y;

	t_draw_list.add_line({x - 7.0f * t_scale, y}, {x - 2.0f * t_scale, y + 5.0f * t_scale}, 2.0f, t_color);
	t_draw_list.add_line({x - 2.0f * t_scale, y + 5.0f * t_scale}, {x + 7.0f * t_scale, y - 6.0f * t_scale}, 2.0f,
						 t_color);
}

bool has_bit(u16 t_mask, u32 t_bit)
{
	return (t_mask & (1u << t_bit)) != 0;
}
}

GameSelectPopup::GameSelectPopup(const Library &t_library, const Fonts &t_fonts)
	: m_library(t_library)
	, m_fonts(t_fonts)
{
}

void GameSelectPopup::open(Rect t_anchor, Rect t_bounds)
{
	m_anchor = t_anchor;
	m_bounds = t_bounds;
	m_open = true;
}

void GameSelectPopup::close()
{
	m_open = false;
}

void GameSelectPopup::update(float t_delta_seconds)
{
	m_open_amount = animation::ease_toward(m_open_amount, m_open ? 1.0f : 0.0f, open_ease_rate, t_delta_seconds);
}

Rect GameSelectPopup::popup_rect() const
{
	const u32 row_count = std::max<u32>(1, m_library.game_count());
	const float full_height = popup_padding * 2.0f + row_height_for(m_fonts) * row_count;

	const float min_x = m_bounds.x + bounds_margin;
	const float max_x = std::max(min_x, m_bounds.right() - bounds_margin - popup_width);
	const float min_y = m_bounds.y + bounds_margin;
	const float max_y = std::max(min_y, m_bounds.bottom() - bounds_margin - full_height);

	return Rect{std::clamp(m_anchor.right() - popup_width, min_x, max_x),
				std::clamp(m_anchor.bottom() + anchor_gap, min_y, max_y), popup_width, full_height * m_open_amount};
}

Rect GameSelectPopup::row_rect(u32 t_game) const
{
	const Rect popup = popup_rect();
	const float height = row_height_for(m_fonts);

	return Rect{popup.x, popup.y + popup_padding + t_game * height, popup.w, height};
}

bool GameSelectPopup::is_last_checked(u32 t_game) const
{
	return has_bit(m_mask, t_game) && std::popcount(m_mask) == 1;
}

bool GameSelectPopup::on_pointer_down(Vec2 t_point)
{
	if (!is_open() || !popup_rect().contains(t_point)) return false;

	for (u32 game = 0; game < m_library.game_count(); game += 1) {
		if (!row_rect(game).contains(t_point)) continue;

		if (!is_last_checked(game)) {
			m_mask ^= static_cast<u16>(1u << game);
		}

		break;
	}

	return true;
}

CursorKind GameSelectPopup::cursor(Vec2 t_mouse) const
{
	if (!is_open()) return CursorKind::arrow;

	for (u32 game = 0; game < m_library.game_count(); game += 1) {
		if (!is_last_checked(game) && row_rect(game).contains(t_mouse)) return CursorKind::hand;
	}

	return CursorKind::arrow;
}

void GameSelectPopup::draw(DrawList &t_draw_list, Vec2 t_mouse) const
{
	if (!is_open()) return;

	const Rect popup = popup_rect();
	t_draw_list.add_bordered_rect(popup, rounded(popup_radius), color_background, color_border, 1.0f);
	t_draw_list.push_clip(popup);

	const Font &font = m_fonts.secondary();
	const float icon_size = icon_size_for(m_fonts);

	for (u32 index = 0; index < m_library.game_count(); index += 1) {
		const Game &game = m_library.game(index);
		const Rect row = row_rect(index);
		const bool locked = is_last_checked(index);

		if (!locked && row.contains(t_mouse)) {
			t_draw_list.add_rounded_rect(row.inset(4.0f, 0.0f), rounded(6.0f), color_hover);
		}

		const Rect icon{row.x + row_padding_x, row.y + (row.h - icon_size) * 0.5f, icon_size, icon_size};

		if (game.icon != nullptr) {
			t_draw_list.add_image(icon, game.icon, locked ? color_icon_disabled : color_icon, rounded(4.0f));
		} else {
			t_draw_list.add_rounded_rect(icon, rounded(4.0f), locked ? faded(game.accent, 140) : game.accent);
		}

		draw_text(t_draw_list, font, Vec2{icon.right() + 10.0f, font.centered_baseline(row)}, game.title,
				  locked ? color_text_disabled : color_text);

		if (has_bit(m_mask, index)) {
			draw_check(t_draw_list, row, icon_size / 20.0f, locked ? color_check_disabled : color_check);
		}
	}

	t_draw_list.pop_clip();
}
