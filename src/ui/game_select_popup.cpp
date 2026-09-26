#include "ui/game_select_popup.h"

#include <algorithm>
#include <bit>

#include "core/animation.h"
#include "core/library.h"
#include "core/settings.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "ui/controls.h"
#include "ui/text.h"
#include "ui/theme.h"

namespace {
constexpr float popup_padding = 6.0f;
constexpr float popup_radius = 10.0f;
constexpr float bounds_margin = 8.0f;
constexpr float anchor_gap = 4.0f;
constexpr float row_padding_x = 12.0f;
constexpr float row_inset = 4.0f;
constexpr float row_radius = 7.0f;
constexpr float icon_radius = 5.0f;
constexpr float icon_gap = 10.0f;
constexpr float checkbox_size = 16.0f;
constexpr float hover_strength = 0.22f;
constexpr u8 unchecked_icon_alpha = 150;
constexpr float open_ease_rate = 20.0f;

constexpr Color color_icon{255, 255, 255, 255};

float row_height_for(const Fonts &t_fonts)
{
	return std::max(30.0f, t_fonts.secondary().line_height() + 14.0f);
}

float icon_size_for(const Fonts &t_fonts)
{
	return t_fonts.secondary().line_height() * 0.95f;
}

bool has_bit(u16 t_mask, u32 t_bit)
{
	return (t_mask & (1u << t_bit)) != 0;
}
}

GameSelectPopup::GameSelectPopup(const Library &t_library, const Settings &t_settings, const Fonts &t_fonts)
	: m_library(t_library)
	, m_settings(t_settings)
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

GameSelectPopup::Placement GameSelectPopup::placement() const
{
	const u32 row_count = std::max<u32>(1, m_library.game_count());
	const float height = popup_padding * 2.0f + row_height_for(m_fonts) * row_count;
	const float room_below = m_bounds.bottom() - bounds_margin - (m_anchor.bottom() + anchor_gap);
	const float room_above = m_anchor.y - anchor_gap - (m_bounds.y + bounds_margin);
	const bool below = room_below >= height || room_below >= room_above;
	const float y = below ? m_anchor.bottom() + anchor_gap : m_anchor.y - anchor_gap - height;

	return Placement{Rect{m_anchor.x, y, m_anchor.w, height}, below};
}

Rect GameSelectPopup::shown_rect() const
{
	const Placement current = placement();
	const float height = current.full.h * m_open_amount;
	const float y = current.below ? current.full.y : current.full.bottom() - height;

	return Rect{current.full.x, y, current.full.w, height};
}

Rect GameSelectPopup::row_rect(u32 t_game) const
{
	const Rect popup = placement().full;
	const float height = row_height_for(m_fonts);

	return Rect{popup.x, popup.y + popup_padding + t_game * height, popup.w, height};
}

bool GameSelectPopup::is_last_checked(u32 t_game) const
{
	return has_bit(m_mask, t_game) && std::popcount(m_mask) == 1;
}

bool GameSelectPopup::on_pointer_down(Vec2 t_point)
{
	if (!is_open() || !shown_rect().contains(t_point)) return false;

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

	const Theme &colors = theme();
	const Rect popup = shown_rect();
	const Font &font = m_fonts.secondary();
	const float icon_size = icon_size_for(m_fonts);
	const Color hover = mix(colors.popup, m_settings.accent, hover_strength);

	controls::draw_popup_shadow(t_draw_list, popup, popup_radius, m_open_amount);
	t_draw_list.add_bordered_rect(popup, rounded(popup_radius), colors.popup, colors.border, 1.0f);
	t_draw_list.push_clip(popup);

	for (u32 index = 0; index < m_library.game_count(); index += 1) {
		const Game &game = m_library.game(index);
		const Rect row = row_rect(index);
		const bool checked = has_bit(m_mask, index);
		const bool locked = is_last_checked(index);

		if (!locked && row.contains(t_mouse)) {
			t_draw_list.add_rounded_rect(row.inset(row_inset, 0.0f), rounded(row_radius), hover);
		}

		const Rect icon{row.x + row_padding_x, row.center().y - icon_size * 0.5f, icon_size, icon_size};
		const u8 icon_alpha = checked ? 255 : unchecked_icon_alpha;

		if (game.icon != nullptr) {
			t_draw_list.add_image(icon, game.icon, with_alpha(color_icon, icon_alpha), rounded(icon_radius));
		} else {
			t_draw_list.add_rounded_rect(icon, rounded(icon_radius), with_alpha(game.accent, icon_alpha));
		}

		draw_text(t_draw_list, font, Vec2{icon.right() + icon_gap, font.centered_baseline(row)}, game.title,
				  checked ? colors.text : colors.text_dim);

		const Rect box{row.right() - row_padding_x - checkbox_size, row.center().y - checkbox_size * 0.5f,
					   checkbox_size, checkbox_size};
		controls::draw_checkbox(t_draw_list, box, checked, !locked, m_settings.accent);
	}

	t_draw_list.pop_clip();
}
