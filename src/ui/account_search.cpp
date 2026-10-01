#include "ui/account_search.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <Windows.h>

#include "core/animation.h"
#include "core/str.h"
#include "gfx/assets.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "platform/window.h"
#include "ui/controls.h"
#include "ui/text.h"
#include "ui/theme.h"

namespace {
constexpr float open_ease_rate = 22.0f;
constexpr float panel_max_width = 520.0f;
constexpr float panel_side_margin = 24.0f;
constexpr float panel_top_gap = 56.0f;
constexpr float panel_radius = 10.0f;
constexpr float panel_rise = 8.0f;
constexpr float header_height = 46.0f;
constexpr float header_padding = 14.0f;
constexpr float header_icon_size = 15.0f;
constexpr float header_icon_gap = 10.0f;
constexpr float list_padding = 4.0f;
constexpr float row_radius = 6.0f;
constexpr float row_padding = 10.0f;
constexpr float row_icon_size = 16.0f;
constexpr float row_icon_gap = 10.0f;
constexpr float row_text_gap = 8.0f;
constexpr float region_chip_padding = 6.0f;
constexpr float column_gap = 12.0f;
constexpr std::string_view name_column_sample = "0000000000";
constexpr float footer_padding_x = 12.0f;
constexpr float footer_padding_y = 6.0f;
constexpr float hint_gap = 14.0f;
constexpr float hint_key_gap = 5.0f;
constexpr Vec2 hint_arrow_size{7.0f, 4.0f};
constexpr u32 max_shown_rows = 8;
constexpr u32 query_max_length = 64;
constexpr u32 group_break_row = 3;
constexpr float group_gap = 9.0f;

std::string_view account_name(const Account *t_account)
{
	return t_account->note[0] != '\0' ? std::string_view{t_account->note} : std::string_view{t_account->username};
}

bool matches(const Account *t_account, std::string_view t_query)
{
	return find_ignoring_case(t_account->note, t_query) != std::string_view::npos ||
		   find_ignoring_case(t_account->username, t_query) != std::string_view::npos ||
		   find_ignoring_case(t_account->region, t_query) != std::string_view::npos;
}

bool starts_with_query(const Account *t_account, std::string_view t_query)
{
	return find_ignoring_case(t_account->note, t_query) == 0 || find_ignoring_case(t_account->username, t_query) == 0;
}
}

AccountSearch::AccountSearch(const Library *t_library, const Fonts *t_fonts, const Assets *t_assets, const Window *t_window, CommandQueue *t_commands)
	: m_library(t_library)
	, m_fonts(t_fonts)
	, m_assets(t_assets)
	, m_window(t_window)
	, m_commands(t_commands)
{
	m_query.set_max_length(query_max_length);
	m_query.set_placeholder("Search all accounts");
}

void AccountSearch::open()
{
	m_open = true;
	m_query.set_value("");
	m_query.set_focused(true);
	show_results();
}

void AccountSearch::close()
{
	m_open = false;
	m_query.set_focused(false);
	m_query.on_pointer_up();
	m_pressed_row.reset();
	m_pressed_outside = false;
	m_pressed_back = false;
}

bool AccountSearch::is_valid(AccountRef t_account) const
{
	return t_account.game < m_library->game_count && t_account.index < m_library->games[t_account.game].account_count;
}

void AccountSearch::rebuild_results()
{
	const std::string_view query = m_query.value();
	const Font &body = m_fonts->body;
	const Font &secondary = m_fonts->secondary;
	const float name_limit = text_width(body, name_column_sample);
	m_results.clear();
	m_name_column = 0.0f;
	m_region_column = 0.0f;

	for (u32 game = 0; game < m_library->game_count; game += 1) {
		for (u32 index = 0; index < m_library->games[game].account_count; index += 1) {
			const AccountRef ref{game, index};
			const Account *account = m_library->account(ref);

			if (account->note[0] != '\0') {
				m_name_column = std::max(m_name_column, std::min(text_width(body, account->note), name_limit));
			}

			if (account->region[0] != '\0') {
				m_region_column = std::max(m_region_column, text_width(secondary, account->region) + region_chip_padding * 2.0f);
			}

			if (query.empty() || matches(account, query)) {
				m_results.push_back(ref);
			}
		}
	}

	m_name_column = std::ceil(m_name_column);
	m_region_column = std::ceil(m_region_column);

	std::ranges::stable_sort(m_results, [&](AccountRef t_a, AccountRef t_b) {
		const Account *a = m_library->account(t_a);
		const Account *b = m_library->account(t_b);
		const bool a_starts = !query.empty() && starts_with_query(a, query);
		const bool b_starts = !query.empty() && starts_with_query(b, query);

		if (a_starts != b_starts) return a_starts;

		return a->last_used > b->last_used;
	});
}

void AccountSearch::rebuild_actions()
{
	m_action_count = 0;
	if (!m_account) return;

	m_actions[m_action_count++] = Action{ActionKind::Edit, 0};
	m_actions[m_action_count++] = Action{ActionKind::CopyUsername, 0};
	m_actions[m_action_count++] = Action{ActionKind::CopyPassword, 0};

	const u16 visible = m_library->account(*m_account)->visible_games(m_account->game);

	for (u32 game = 0; game < m_library->game_count && m_action_count < max_actions; game += 1) {
		if ((visible & (1u << game)) != 0) {
			m_actions[m_action_count++] = Action{ActionKind::Login, game};
		}
	}
}

void AccountSearch::show_account(AccountRef t_account)
{
	m_account = t_account;
	m_query.set_focused(false);
	rebuild_actions();
	m_highlighted = m_action_count > group_break_row ? group_break_row : 0;
	m_first_row = 0;
}

void AccountSearch::show_results()
{
	m_account.reset();
	m_action_count = 0;
	m_query.set_focused(true);
	m_highlighted = 0;
	m_first_row = 0;
	rebuild_results();
}

void AccountSearch::activate(u32 t_row)
{
	if (!m_account) {
		if (t_row < m_results.size()) {
			show_account(m_results[t_row]);
		}

		return;
	}

	if (t_row >= m_action_count) return;

	const Action action = m_actions[t_row];
	const AccountRef account = *m_account;

	switch (action.kind) {
		case ActionKind::Edit:
			m_commands->push(Command{.type = CommandType::EditAccount, .account = account});
			break;
		case ActionKind::CopyUsername:
			m_commands->push(Command{.type = CommandType::CopyAccountUsername, .account = account});
			break;
		case ActionKind::CopyPassword:
			m_commands->push(Command{.type = CommandType::CopyAccountPassword, .account = account});
			break;
		case ActionKind::Login:
			m_commands->push(Command{.type = CommandType::LoginAccount, .index = static_cast<i32>(action.game), .account = account});
			break;
	}

	close();
}

void AccountSearch::move_highlight(i32 t_rows)
{
	const u32 count = row_count();
	if (count == 0) return;

	const auto last = static_cast<i32>(count) - 1;
	m_highlighted = static_cast<u32>(std::clamp(static_cast<i32>(m_highlighted) + t_rows, 0, last));

	const u32 shown = shown_rows();
	if (m_highlighted < m_first_row) {
		m_first_row = m_highlighted;
	} else if (m_highlighted >= m_first_row + shown) {
		m_first_row = m_highlighted + 1 - shown;
	}
}

u32 AccountSearch::row_count() const
{
	return m_account ? m_action_count : static_cast<u32>(m_results.size());
}

u32 AccountSearch::shown_rows() const
{
	return std::clamp(row_count(), 1u, max_shown_rows);
}

float AccountSearch::row_height() const
{
	return std::max(36.0f, m_fonts->body.line_height() + 14.0f);
}

AccountSearch::Layout AccountSearch::layout() const
{
	const Vec2 window = m_window->size();
	const float width = std::min(panel_max_width, window.x - panel_side_margin * 2.0f);
	const float list_height = list_padding * 2.0f + row_height() * static_cast<float>(shown_rows()) + (has_group_gap() ? group_gap : 0.0f);
	const float footer_height = controls::keycap_height(m_fonts->secondary) + footer_padding_y * 2.0f;
	const float rise = panel_rise * (1.0f - m_open_amount);

	Layout result{};
	result.panel = Rect{snapped_to_pixel((window.x - width) * 0.5f), snapped_to_pixel(title_bar_height + panel_top_gap - rise), width,
						header_height + 1.0f + list_height + 1.0f + footer_height + 1.0f};

	Rect remaining = result.panel;
	result.header = remaining.split_top(header_height);
	remaining.split_top(1.0f);
	result.footer = remaining.split_bottom(footer_height + 1.0f).inset(1.0f, 0.0f);
	result.footer.h -= 1.0f;
	remaining.split_bottom(1.0f);
	result.list = remaining.inset(list_padding);

	return result;
}

Rect AccountSearch::query_text_rect(const Layout &t_layout) const
{
	const Rect &header = t_layout.header;
	const float left = header.x + header_padding + header_icon_size + header_icon_gap;
	const float right = header.right() - header_padding - controls::keycap_width(m_fonts->secondary, "Esc") - 8.0f;

	return Rect{left, header.y, std::max(0.0f, right - left), header.h};
}

Rect AccountSearch::back_button_rect(const Layout &t_layout) const
{
	const Rect &header = t_layout.header;
	const float size = header.h - 16.0f;

	return Rect{header.x + 8.0f, header.y + 8.0f, size, size};
}

Rect AccountSearch::row_rect(const Layout &t_layout, u32 t_row) const
{
	const float height = row_height();
	const bool below_gap = has_group_gap() && m_first_row < group_break_row && t_row >= group_break_row;

	return Rect{t_layout.list.x, t_layout.list.y + static_cast<float>(t_row - m_first_row) * height + (below_gap ? group_gap : 0.0f), t_layout.list.w, height};
}

std::optional<u32> AccountSearch::row_at(const Layout &t_layout, Vec2 t_point) const
{
	if (!t_layout.list.contains(t_point)) return std::nullopt;

	const u32 last = std::min(row_count(), m_first_row + shown_rows());

	for (u32 row = m_first_row; row < last; row += 1) {
		if (row_rect(t_layout, row).contains(t_point)) return row;
	}

	return std::nullopt;
}

bool AccountSearch::has_group_gap() const
{
	return m_account && m_action_count > group_break_row;
}

void AccountSearch::update(float t_delta_seconds)
{
	m_open_amount = animation::ease_toward(m_open_amount, m_open ? 1.0f : 0.0f, open_ease_rate, t_delta_seconds);
	if (!m_open) return;

	m_query.update(t_delta_seconds);

	if (m_account && !is_valid(*m_account)) {
		show_results();
	}

	if (!m_account) {
		rebuild_results();
	}

	const u32 count = row_count();
	m_highlighted = count == 0 ? 0 : std::min(m_highlighted, count - 1);
	m_first_row = std::min(m_first_row, count > shown_rows() ? count - shown_rows() : 0);
}

bool AccountSearch::on_pointer_down(Vec2 t_point)
{
	if (!is_blocking()) return false;
	if (!m_open) return true;

	const Layout current = layout();
	m_pressed_row.reset();
	m_pressed_outside = !current.panel.contains(t_point);
	m_pressed_back = m_account && back_button_rect(current).contains(t_point);

	if (m_pressed_outside || m_pressed_back) return true;

	if (!m_account && current.header.contains(t_point)) {
		m_query.set_focused(true);
		m_query.on_pointer_down(m_fonts->body, query_text_rect(current), t_point.x);
		return true;
	}

	m_pressed_row = row_at(current, t_point);

	return true;
}

bool AccountSearch::on_pointer_move(Vec2 t_point)
{
	if (!is_blocking()) return false;
	if (!m_open) return true;

	const Layout current = layout();

	if (m_query.is_selecting()) {
		m_query.on_pointer_move(m_fonts->body, query_text_rect(current), t_point.x);
	}

	if (const std::optional<u32> hovered_row = row_at(current, t_point)) {
		m_highlighted = *hovered_row;
	}

	return true;
}

bool AccountSearch::on_pointer_up(Vec2 t_point)
{
	if (!is_blocking()) return false;
	if (!m_open) return true;

	const Layout current = layout();
	const std::optional<u32> pressed = std::exchange(m_pressed_row, std::nullopt);
	m_query.on_pointer_up();

	if (std::exchange(m_pressed_outside, false)) {
		if (!current.panel.contains(t_point)) {
			close();
		}

		return true;
	}

	if (std::exchange(m_pressed_back, false)) {
		if (back_button_rect(current).contains(t_point)) {
			show_results();
		}

		return true;
	}

	if (pressed && row_at(current, t_point) == pressed) {
		activate(*pressed);
	}

	return true;
}

bool AccountSearch::on_right_click(Vec2 t_point)
{
	if (!is_blocking()) return false;
	if (!m_open || m_account) return true;

	const Layout current = layout();

	if (current.header.contains(t_point)) {
		m_query.set_focused(true);
		m_query.on_right_click(m_fonts->body, query_text_rect(current), t_point.x);
		m_commands->push(Command{.type = CommandType::ShowTextMenu, .position = t_point, .text_input = &m_query});
	}

	return true;
}

bool AccountSearch::on_scroll(Vec2, float t_wheel_delta)
{
	if (!is_blocking()) return false;
	if (!m_open) return true;

	const u32 count = row_count();
	const u32 shown = shown_rows();
	if (count <= shown) return true;

	const i32 step = t_wheel_delta > 0.0f ? -1 : 1;
	m_first_row = static_cast<u32>(std::clamp(static_cast<i32>(m_first_row) + step, 0, static_cast<i32>(count - shown)));
	m_highlighted = std::clamp(m_highlighted, m_first_row, m_first_row + shown - 1);

	return true;
}

bool AccountSearch::on_key_down(u32 t_key)
{
	if (!is_blocking()) return false;
	if (!m_open) return true;

	switch (t_key) {
		case VK_ESCAPE:
			if (m_account) {
				show_results();
			} else {
				close();
			}

			return true;

		case VK_UP:
			move_highlight(-1);
			return true;

		case VK_DOWN:
			move_highlight(1);
			return true;

		case VK_PRIOR:
			move_highlight(-static_cast<i32>(max_shown_rows) + 1);
			return true;

		case VK_NEXT:
			move_highlight(static_cast<i32>(max_shown_rows) - 1);
			return true;

		case VK_RETURN:
			activate(m_highlighted);
			return true;

		case VK_TAB:
			if (!m_account) {
				activate(m_highlighted);
			}

			return true;

		case VK_LEFT:
		case VK_BACK:
			if (m_account) {
				show_results();
				return true;
			}

			break;

		default:
			break;
	}

	if (!m_account) {
		m_query.on_key_down(t_key);
	}

	return true;
}

bool AccountSearch::on_char(u32 t_character)
{
	if (!is_blocking()) return false;
	if (!m_open) return true;

	const bool printable = t_character >= 32 && t_character != 127;

	if (m_account && printable) {
		show_results();
	}

	if (!m_account) {
		m_query.on_char(t_character);
	}

	return true;
}

CursorKind AccountSearch::cursor() const
{
	if (!m_open) return CursorKind::Arrow;
	if (m_query.is_selecting()) return CursorKind::IBeam;

	const Layout current = layout();

	if (m_account && back_button_rect(current).contains(m_mouse)) return CursorKind::Hand;
	if (!m_account && current.header.contains(m_mouse)) return CursorKind::IBeam;

	return row_at(current, m_mouse) ? CursorKind::Hand : CursorKind::Arrow;
}

void AccountSearch::draw_header(DrawList *t_draw_list, const Layout &t_layout, u8 t_alpha)
{
	const Theme &colors = theme();
	const Rect &header = t_layout.header;
	const Font &body = m_fonts->body;
	const Font &secondary = m_fonts->secondary;

	if (!m_account) {
		const Rect icon{header.x + header_padding, header.center().y - header_icon_size * 0.5f, header_icon_size, header_icon_size};

		controls::draw_magnifier(t_draw_list, icon, faded(colors.text_dim, t_alpha));
		m_query.draw(t_draw_list, body, query_text_rect(t_layout), faded(colors.text, t_alpha), faded(colors.text_dim, t_alpha), query_text_rect(t_layout));
		controls::draw_shortcut(t_draw_list, secondary, Vec2{header.right() - header_padding, header.center().y}, "Esc", colors.popup, t_alpha);
		return;
	}

	const Account *account = m_library->account(*m_account);
	const Rect back = back_button_rect(t_layout);
	const bool back_hovered = back.contains(m_mouse);

	if (back_hovered) {
		t_draw_list->add_rounded_rect(back, rounded(row_radius), faded(hovered(colors.popup), t_alpha));
	}

	t_draw_list->add_image(back.centered(header_icon_size, header_icon_size), m_assets->get(Asset::IconArrowBack),
						   faded(back_hovered ? colors.text : colors.text_dim, t_alpha));

	const Rect icon{back.right() + 6.0f, header.center().y - row_icon_size * 0.5f, row_icon_size, row_icon_size};
	t_draw_list->add_image(icon, m_assets->get(Asset::IconAccount), faded(colors.text_dim, t_alpha));

	const std::string_view name = account_name(account);
	const float name_x = icon.right() + row_icon_gap;
	const float room = header.right() - header_padding - name_x;
	const float name_width = std::min(text_width(body, name), account->note[0] != '\0' ? room * 0.6f : room);
	const float baseline = body.centered_baseline(header);
	draw_text_truncated(t_draw_list, body, Vec2{name_x, baseline}, name, name_width, faded(colors.text, t_alpha));

	if (account->note[0] != '\0') {
		const float username_x = name_x + name_width + row_text_gap;
		draw_text_truncated(t_draw_list, secondary, Vec2{username_x, baseline}, account->username, header.right() - header_padding - username_x,
							faded(colors.text_faint, t_alpha));
	}
}

void AccountSearch::draw_result(DrawList *t_draw_list, Rect t_row, AccountRef t_account, bool t_highlighted, u8 t_alpha) const
{
	const Theme &colors = theme();
	const Font &body = m_fonts->body;
	const Font &secondary = m_fonts->secondary;
	const Account *account = m_library->account(t_account);

	if (t_highlighted) {
		t_draw_list->add_rounded_rect(t_row, rounded(row_radius), faded(hovered(colors.popup), t_alpha));
	}

	const Rect icon{t_row.x + row_padding, t_row.center().y - row_icon_size * 0.5f, row_icon_size, row_icon_size};
	t_draw_list->add_image(icon, m_assets->get(Asset::IconAccount), faded(colors.text_dim, t_alpha));

	const std::string_view region = account->region;
	const float right = t_row.right() - row_padding - (m_region_column > 0.0f ? m_region_column + column_gap : 0.0f);
	const float baseline = body.centered_baseline(t_row);
	const float name_x = icon.right() + row_icon_gap;
	const Color name_color = faded(t_highlighted ? colors.text : mix(colors.text_dim, colors.text, 0.5f), t_alpha);

	if (account->note[0] == '\0') {
		draw_text_truncated(t_draw_list, body, Vec2{name_x, baseline}, account->username, right - name_x, name_color);
	} else {
		const float name_width = std::min(m_name_column, std::max(0.0f, (right - name_x) * 0.6f));
		const float username_x = name_x + name_width + column_gap;

		draw_text_truncated(t_draw_list, body, Vec2{name_x, baseline}, account->note, name_width, name_color);
		draw_text_truncated(t_draw_list, secondary, Vec2{username_x, baseline}, account->username, right - username_x, faded(colors.text_faint, t_alpha));
	}

	if (!region.empty()) {
		const float height = secondary.line_height() + 2.0f;
		const Rect chip{t_row.right() - row_padding - m_region_column, t_row.center().y - height * 0.5f, m_region_column, height};

		t_draw_list->add_bordered_rect(chip, rounded(4.0f), faded(colors.popup, t_alpha), faded(colors.border, t_alpha), 1.0f);
		draw_text_centered(t_draw_list, secondary, chip, region, faded(colors.text_dim, t_alpha));
	}
}

void AccountSearch::draw_action(DrawList *t_draw_list, Rect t_row, const Action &t_action, bool t_highlighted, u8 t_alpha) const
{
	const Theme &colors = theme();
	const Font &body = m_fonts->body;
	const Color backdrop = t_highlighted ? hovered(colors.popup) : colors.popup;

	if (t_highlighted) {
		t_draw_list->add_rounded_rect(t_row, rounded(row_radius), faded(backdrop, t_alpha));
	}

	const Rect icon{t_row.x + row_padding, t_row.center().y - row_icon_size * 0.5f, row_icon_size, row_icon_size};
	const Color icon_color = faded(colors.text_dim, t_alpha);
	char label[96];
	std::string_view text;

	switch (t_action.kind) {
		case ActionKind::Edit:
			t_draw_list->add_image(icon, m_assets->get(Asset::IconEdit), icon_color);
			text = "Edit account";
			break;
		case ActionKind::CopyUsername:
			t_draw_list->add_image(icon, m_assets->get(Asset::IconUsername), icon_color);
			text = "Copy username";
			break;
		case ActionKind::CopyPassword:
			t_draw_list->add_image(icon, m_assets->get(Asset::IconLock), icon_color);
			text = "Copy password";
			break;
		case ActionKind::Login: {
			const Game &game = m_library->games[t_action.game];

			if (game.icon != nullptr) {
				t_draw_list->add_image(icon, game.icon, faded(Color{255, 255, 255, 255}, t_alpha), rounded(4.0f));
			} else {
				t_draw_list->add_rounded_rect(icon, rounded(4.0f), faded(game.accent, t_alpha));
			}

			const int written = std::snprintf(label, sizeof(label), "Log in to %.*s", static_cast<int>(game.title.size()), game.title.data());
			text = std::string_view{label, static_cast<usize>(std::max(written, 0))};
			break;
		}
	}

	draw_text_truncated(t_draw_list, body, Vec2{icon.right() + row_icon_gap, body.centered_baseline(t_row)}, text,
						t_row.right() - row_padding - (icon.right() + row_icon_gap),
						faded(t_highlighted ? colors.text : mix(colors.text_dim, colors.text, 0.5f), t_alpha));
}

void AccountSearch::draw_footer(DrawList *t_draw_list, const Layout &t_layout, u8 t_alpha) const
{
	const Theme &colors = theme();
	const Font &font = m_fonts->secondary;
	const Rect &footer = t_layout.footer;
	const float cap = controls::keycap_height(font);
	const float cap_y = snapped_to_pixel(footer.center().y - cap * 0.5f);
	const float baseline = font.centered_baseline(Rect{footer.x, cap_y, footer.w, cap});
	const Color label = faded(colors.text_faint, t_alpha);
	float x = footer.x + footer_padding_x;

	t_draw_list->add_rounded_rect(footer, rounded(0.0f, 0.0f, panel_radius - 1.0f, panel_radius - 1.0f), faded(colors.field, t_alpha));

	const auto key = [&](std::string_view t_key) {
		const float width = controls::keycap_width(font, t_key);
		controls::draw_keycap(t_draw_list, font, Rect{snapped_to_pixel(x), cap_y, width, cap}, t_key, colors.field, t_alpha);
		x += width + hint_key_gap;
	};

	const auto hint = [&](std::string_view t_text) {
		draw_text(t_draw_list, font, Vec2{snapped_to_pixel(x), baseline}, t_text, label);
		x += text_width(font, t_text) + hint_gap;
	};

	for (const bool up : {true, false}) {
		const Rect arrow{snapped_to_pixel(x), cap_y, cap, cap};
		const Rect chevron{arrow.center().x - hint_arrow_size.x * 0.5f, arrow.center().y - hint_arrow_size.y * 0.5f - 0.5f, hint_arrow_size.x,
						   hint_arrow_size.y};

		controls::draw_keycap_frame(t_draw_list, arrow, colors.field, t_alpha);
		controls::draw_chevron(t_draw_list, chevron, up, faded(controls::keycap_label_color(), t_alpha));
		x += cap + 3.0f;
	}

	x += hint_key_gap - 3.0f;
	hint("navigate");
	key("Enter");
	hint(m_account ? "run" : "open");

	if (m_account) {
		key("Esc");
		hint("back");
	}
}

void AccountSearch::draw(DrawList *t_draw_list)
{
	if (m_open_amount <= 0.01f) return;

	const Theme &colors = theme();
	const auto alpha = to_alpha(m_open_amount);
	const Vec2 window = m_window->size();
	const Layout current = layout();

	t_draw_list->add_rect(Rect{0.0f, title_bar_height, window.x, window.y - title_bar_height}, faded(colors.scrim, alpha));

	controls::draw_popup_shadow(t_draw_list, current.panel, panel_radius, m_open_amount);
	t_draw_list->add_bordered_rect(current.panel, rounded(panel_radius), faded(colors.popup, alpha), faded(colors.border, alpha), 1.0f);

	draw_header(t_draw_list, current, alpha);
	t_draw_list->add_rect(Rect{current.panel.x + 1.0f, current.header.bottom(), current.panel.w - 2.0f, 1.0f}, faded(colors.separator, alpha));
	t_draw_list->add_rect(Rect{current.panel.x + 1.0f, current.footer.y - 1.0f, current.panel.w - 2.0f, 1.0f}, faded(colors.separator, alpha));

	const u32 count = row_count();

	if (count == 0) {
		const bool empty_library = !m_account && m_query.value().empty();
		draw_text_centered(t_draw_list, m_fonts->secondary, current.list, empty_library ? "No accounts yet" : "No accounts match",
						   faded(colors.text_faint, alpha));
	}

	const u32 last = std::min(count, m_first_row + shown_rows());

	for (u32 row = m_first_row; row < last; row += 1) {
		const Rect rect = row_rect(current, row);
		const bool highlighted = row == m_highlighted;

		if (m_account) {
			if (row == group_break_row && row > m_first_row) {
				const float y = snapped_to_pixel(rect.y - group_gap * 0.5f - 0.5f);
				t_draw_list->add_rect(Rect{rect.x + row_padding, y, rect.w - row_padding * 2.0f, 1.0f}, faded(colors.separator, alpha));
			}

			draw_action(t_draw_list, rect.inset(0.0f, 1.0f), m_actions[row], highlighted, alpha);
		} else {
			draw_result(t_draw_list, rect.inset(0.0f, 1.0f), m_results[row], highlighted, alpha);
		}
	}

	draw_footer(t_draw_list, current, alpha);
}
