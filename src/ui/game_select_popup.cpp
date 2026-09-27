#include "ui/game_select_popup.h"

#include <algorithm>
#include <bit>
#include <cstdio>
#include <string_view>

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
constexpr float popup_radius = 12.0f;
constexpr float bounds_margin = 8.0f;
constexpr float anchor_gap = 6.0f;
constexpr float header_padding_x = 14.0f;
constexpr float row_padding_x = 12.0f;
constexpr float row_inset = 4.0f;
constexpr float row_radius = 8.0f;
constexpr float icon_inset = 8.0f;
constexpr float icon_radius = 7.0f;
constexpr float text_gap = 12.0f;
constexpr float line_gap = 2.0f;
constexpr float current_dot_gap = 8.0f;
constexpr float current_dot_size = 6.0f;
constexpr float check_size = 18.0f;
constexpr float check_ring = 1.5f;
constexpr float hover_strength = 0.05f;
constexpr u8 unchecked_icon_alpha = 140;
constexpr u8 locked_check_alpha = 150;
constexpr float open_ease_rate = 20.0f;

constexpr std::string_view header_text = "Show this account in";
constexpr std::string_view select_all_text = "Select all";
constexpr std::string_view only_current_text = "Only this game";

constexpr Color color_icon{255, 255, 255, 255};

float row_height_for(const Fonts &t_fonts)
{
	return std::max(44.0f, t_fonts.body().line_height() + t_fonts.secondary().line_height() + 10.0f);
}

float header_height_for(const Fonts &t_fonts)
{
	return t_fonts.secondary().line_height() + 16.0f;
}

bool has_bit(u16 t_mask, u32 t_bit)
{
	return (t_mask & (1u << t_bit)) != 0;
}

std::string_view account_count_text(u32 t_count, char (&t_buffer)[32])
{
	if (t_count == 0) return "No accounts yet";

	const int written = std::snprintf(t_buffer, sizeof(t_buffer), "%u account%s", t_count, t_count == 1 ? "" : "s");

	return std::string_view{t_buffer, static_cast<usize>(std::max(written, 0))};
}
}

GameSelectPopup::GameSelectPopup(const Library &t_library, const Settings &t_settings, const Fonts &t_fonts)
	: m_library(t_library)
	, m_settings(t_settings)
	, m_fonts(t_fonts)
{
}

void GameSelectPopup::open(Rect t_anchor, Rect t_bounds, u32 t_current_game)
{
	m_anchor = t_anchor;
	m_bounds = t_bounds;
	m_current_game = t_current_game;
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
	const float height = popup_padding * 2.0f + header_height_for(m_fonts) + row_height_for(m_fonts) * row_count;
	const float top_limit = m_bounds.y + bounds_margin;
	const float bottom_limit = m_bounds.bottom() - bounds_margin;
	const float below_y = m_anchor.bottom() + anchor_gap;
	const float above_y = m_anchor.y - anchor_gap - height;

	if (below_y + height <= bottom_limit) return Placement{Rect{m_anchor.x, below_y, m_anchor.w, height}, true};
	if (above_y >= top_limit) return Placement{Rect{m_anchor.x, above_y, m_anchor.w, height}, false};

	const float y = std::clamp(below_y, top_limit, std::max(top_limit, bottom_limit - height));

	return Placement{Rect{m_anchor.x, y, m_anchor.w, height}, true};
}

Rect GameSelectPopup::shown_rect() const
{
	const Placement current = placement();
	const float height = current.full.h * m_open_amount;
	const float y = current.below ? current.full.y : current.full.bottom() - height;

	return Rect{current.full.x, y, current.full.w, height};
}

Rect GameSelectPopup::header_rect() const
{
	const Rect popup = placement().full;

	return Rect{popup.x, popup.y + popup_padding, popup.w, header_height_for(m_fonts)};
}

Rect GameSelectPopup::toggle_all_rect() const
{
	const Rect header = header_rect();
	const Font &font = m_fonts.secondary();
	const bool all = m_mask == all_games_mask();
	const float width = text_width(font, all ? only_current_text : select_all_text) + header_padding_x * 2.0f;

	return Rect{header.right() - width, header.y, width, header.h};
}

Rect GameSelectPopup::row_rect(u32 t_game) const
{
	const Rect header = header_rect();
	const float height = row_height_for(m_fonts);

	return Rect{header.x, header.bottom() + t_game * height, header.w, height};
}

u16 GameSelectPopup::all_games_mask() const
{
	return static_cast<u16>((1u << m_library.game_count()) - 1u);
}

bool GameSelectPopup::is_last_checked(u32 t_game) const
{
	return has_bit(m_mask, t_game) && std::popcount(m_mask) == 1;
}

bool GameSelectPopup::on_pointer_down(Vec2 t_point)
{
	if (!is_open() || !shown_rect().contains(t_point)) return false;

	if (toggle_all_rect().contains(t_point)) {
		m_mask = m_mask == all_games_mask() ? static_cast<u16>(1u << m_current_game) : all_games_mask();
		return true;
	}

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
	if (toggle_all_rect().contains(t_mouse)) return CursorKind::hand;

	for (u32 game = 0; game < m_library.game_count(); game += 1) {
		if (!is_last_checked(game) && row_rect(game).contains(t_mouse)) return CursorKind::hand;
	}

	return CursorKind::arrow;
}

void GameSelectPopup::draw(DrawList &t_draw_list, Vec2 t_mouse) const
{
	if (!is_open()) return;

	const Theme &colors = theme();
	const Color accent = m_settings.accent;
	const Rect popup = shown_rect();
	const Font &body = m_fonts.body();
	const Font &secondary = m_fonts.secondary();

	controls::draw_popup_shadow(t_draw_list, popup, popup_radius, m_open_amount);
	t_draw_list.add_bordered_rect(popup, rounded(popup_radius), colors.popup, colors.border, 1.0f);
	t_draw_list.push_clip(popup);

	const Rect header = header_rect();
	const Rect toggle = toggle_all_rect();
	const bool toggle_hovered = toggle.contains(t_mouse);

	draw_text(t_draw_list, secondary, Vec2{header.x + header_padding_x, secondary.centered_baseline(header)},
			  header_text, colors.text_dim);
	draw_text(t_draw_list, secondary, Vec2{toggle.x + header_padding_x, secondary.centered_baseline(header)},
			  m_mask == all_games_mask() ? only_current_text : select_all_text,
			  toggle_hovered ? lightened(accent, 30) : accent);
	t_draw_list.add_rect(Rect{header.x + row_inset, header.bottom() - 1.0f, header.w - row_inset * 2.0f, 1.0f},
						 colors.separator);

	for (u32 index = 0; index < m_library.game_count(); index += 1) {
		const Game &game = m_library.game(index);
		const Rect row = row_rect(index);
		const bool checked = has_bit(m_mask, index);
		const bool locked = is_last_checked(index);

		if (!locked && row.contains(t_mouse)) {
			t_draw_list.add_rounded_rect(row.inset(row_inset, 2.0f), rounded(row_radius),
										 mix(colors.popup, colors.text, hover_strength));
		}

		const float icon_size = row.h - icon_inset * 2.0f;
		const Rect icon{row.x + row_padding_x, row.y + icon_inset, icon_size, icon_size};
		const u8 icon_alpha = checked ? 255 : unchecked_icon_alpha;

		if (game.icon != nullptr) {
			t_draw_list.add_image(icon, game.icon, with_alpha(color_icon, icon_alpha), rounded(icon_radius));
		} else {
			t_draw_list.add_rounded_rect(icon, rounded(icon_radius), with_alpha(game.accent, icon_alpha));
		}

		const Rect check{row.right() - row_padding_x - check_size, row.center().y - check_size * 0.5f, check_size,
						 check_size};
		const float text_x = icon.right() + text_gap;
		const float block_height = body.line_height() + line_gap + secondary.line_height();
		const float block_y = row.y + (row.h - block_height) * 0.5f;
		const float title_baseline = block_y + body.ascent();
		const float title_limit = check.x - text_gap - text_x;

		draw_text_truncated(t_draw_list, body, Vec2{text_x, title_baseline}, game.title, title_limit,
							checked ? colors.text : colors.text_dim);

		if (index == m_current_game) {
			const float dot_x = text_x + std::min(text_width(body, game.title), title_limit) + current_dot_gap;
			const Rect dot{dot_x, title_baseline - body.ascent() * 0.5f - current_dot_size * 0.5f, current_dot_size,
						   current_dot_size};

			if (dot.right() <= check.x - text_gap) {
				t_draw_list.add_rounded_rect(dot, rounded(current_dot_size * 0.5f), accent);
			}
		}

		char count[32];
		draw_text_truncated(
			t_draw_list, secondary, Vec2{text_x, block_y + body.line_height() + line_gap + secondary.ascent()},
			account_count_text(m_library.visible_accounts(index).count, count), title_limit, colors.text_faint);

		if (checked) {
			const Color fill = with_alpha(accent, locked ? locked_check_alpha : 255);
			t_draw_list.add_rounded_rect(check, rounded(check_size * 0.5f), fill);
			controls::draw_check(t_draw_list, check.inset(3.0f), foreground_on(accent));
		} else {
			t_draw_list.add_rounded_rect(check, rounded(check_size * 0.5f), colors.text_faint);
			t_draw_list.add_rounded_rect(check.inset(check_ring), rounded(check_size * 0.5f - check_ring),
										 colors.popup);
		}
	}

	t_draw_list.pop_clip();
}
