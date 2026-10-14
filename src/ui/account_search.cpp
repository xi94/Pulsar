#include "ui/account_search.h"

#include <algorithm>
#include <cmath>
#include <ranges>
#include <utility>

#include "core/animation.h"
#include "core/settings.h"
#include "core/str.h"
#include "gfx/assets.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "os/window.h"
#include "ui/controls.h"
#include "ui/text.h"
#include "ui/theme.h"
#include "ui/window_layout.h"

namespace {
constexpr float            K_OPEN_EASE_RATE      = 22.0f;
constexpr float            K_PANEL_MAX_WIDTH     = 520.0f;
constexpr float            K_PANEL_SIDE_MARGIN   = 24.0f;
constexpr float            K_PANEL_TOP_GAP       = 56.0f;
constexpr float            K_PANEL_RADIUS        = 10.0f;
constexpr float            K_PANEL_RISE          = 8.0f;
constexpr float            K_HEADER_HEIGHT       = 46.0f;
constexpr float            K_HEADER_PADDING      = 14.0f;
constexpr float            K_HEADER_ICON_SIZE    = 15.0f;
constexpr float            K_HEADER_ICON_GAP     = 10.0f;
constexpr float            K_LIST_PADDING        = 4.0f;
constexpr float            K_ROW_RADIUS          = 6.0f;
constexpr float            K_ROW_PADDING         = 10.0f;
constexpr float            K_ROW_ICON_SIZE       = 16.0f;
constexpr float            K_ROW_ICON_GAP        = 10.0f;
constexpr float            K_ROW_TEXT_GAP        = 8.0f;
constexpr float            K_REGION_CHIP_PADDING = 6.0f;
constexpr float            K_COLUMN_GAP          = 12.0f;
constexpr std::string_view K_NAME_COLUMN_SAMPLE  = "0000000000";
constexpr float            K_FOOTER_PADDING_X    = 12.0f;
constexpr float            K_FOOTER_PADDING_Y    = 6.0f;
constexpr float            K_HINT_GAP            = 14.0f;
constexpr float            K_HINT_KEY_GAP        = 5.0f;
constexpr u32              K_MAX_SHOWN_ROWS      = 8;
constexpr u32              K_QUERY_MAX_LENGTH    = 64;
constexpr u32              K_GROUP_BREAK_ROW     = 3;
constexpr float            K_GROUP_GAP           = 9.0f;
constexpr u8               K_FOOTER_ALPHA        = 110;

[[nodiscard]] auto starts_with_query(const Account* t_account, std::string_view t_query) -> bool
{
	return find_ignoring_case(t_account->note, t_query) == 0 || find_ignoring_case(t_account->username, t_query) == 0;
}
}

AccountSearch::AccountSearch(const Library* t_library, const Settings* t_settings, const Fonts* t_fonts, const Assets* t_assets, const os::Window* t_window, CommandQueue* t_commands)
	: m_library(t_library)
	, m_settings(t_settings)
	, m_fonts(t_fonts)
	, m_assets(t_assets)
	, m_window(t_window)
	, m_commands(t_commands)
{
	m_query.set_max_length(K_QUERY_MAX_LENGTH);
	m_query.m_placeholder = "Search all accounts";
}

auto AccountSearch::open() -> void
{
	m_open = true;
	m_query.set_value("");
	m_query.set_focused(true);
	show_results();
}

auto AccountSearch::close() -> void
{
	m_open = false;
	m_query.set_focused(false);
	m_query.on_pointer_up();
	m_pressed_row.reset();
	m_pressed_outside = false;
	m_pressed_back    = false;
}

auto AccountSearch::is_valid(AccountRef t_account) const -> bool
{
	return t_account.game < m_library->game_count && t_account.index < m_library->games[t_account.game].account_count;
}

auto AccountSearch::rebuild_results() -> void
{
	const std::string_view query      = m_query.value();
	const Font&            body       = m_fonts->body;
	const Font&            secondary  = m_fonts->secondary;
	const float            name_limit = text_width(body, K_NAME_COLUMN_SAMPLE);
	m_results.clear();
	m_name_column   = 0.0f;
	m_region_column = 0.0f;

	for (const u32 game : std::views::iota(0u, m_library->game_count)) {
		for (const u32 index : std::views::iota(0u, m_library->games[game].account_count)) {
			const AccountRef ref{game, index};
			const Account*   account = m_library->account(ref);

			if (account->note[0] != '\0') {
				m_name_column = std::max(m_name_column, std::min(text_width(body, account->note), name_limit));
			}

			if (account->region[0] != '\0') {
				m_region_column = std::max(m_region_column, text_width(secondary, account->region) + K_REGION_CHIP_PADDING * 2.0f);
			}

			if (query.empty() || account->matches(query)) {
				m_results.push_back(ref);
			}
		}
	}

	m_name_column   = std::ceil(m_name_column);
	m_region_column = std::ceil(m_region_column);

	std::ranges::stable_sort(m_results, [&](AccountRef t_a, AccountRef t_b) {
		const Account* a        = m_library->account(t_a);
		const Account* b        = m_library->account(t_b);
		const bool     a_starts = !query.empty() && starts_with_query(a, query);
		const bool     b_starts = !query.empty() && starts_with_query(b, query);

		if (a_starts != b_starts) return a_starts;

		return a->last_used > b->last_used;
	});
}

auto AccountSearch::rebuild_actions() -> void
{
	m_action_count = 0;
	if (!m_account) return;

	m_actions[m_action_count++] = Action{ActionKind::EDIT, 0};
	m_actions[m_action_count++] = Action{ActionKind::COPY_USERNAME, 0};
	m_actions[m_action_count++] = Action{ActionKind::COPY_PASSWORD, 0};

	const u16 visible = m_library->account(*m_account)->visible_games(m_account->game);

	for (u32 game = 0; game < m_library->game_count && m_action_count < K_MAX_ACTIONS; game += 1) {
		if ((visible & (1u << game)) != 0) {
			m_actions[m_action_count++] = Action{ActionKind::LOGIN, game};
		}
	}
}

auto AccountSearch::show_account(AccountRef t_account) -> void
{
	m_account = t_account;
	m_query.set_focused(false);
	rebuild_actions();
	m_highlighted = m_action_count > K_GROUP_BREAK_ROW ? K_GROUP_BREAK_ROW : 0;
	m_first_row   = 0;
}

auto AccountSearch::show_results() -> void
{
	m_account.reset();
	m_action_count = 0;
	m_query.set_focused(true);
	m_highlighted = 0;
	m_first_row   = 0;
	rebuild_results();
}

auto AccountSearch::activate(u32 t_row) -> void
{
	if (!m_account) {
		if (t_row < m_results.size()) {
			show_account(m_results[t_row]);
		}

		return;
	}

	if (t_row >= m_action_count) return;

	const Action     action  = m_actions[t_row];
	const AccountRef account = *m_account;

	switch (action.kind) {
		using enum ActionKind;

		case EDIT: {
			m_commands->push(Command{.type = CommandType::EDIT_ACCOUNT, .account = account});
			break;
		}

		case COPY_USERNAME: {
			m_commands->push(Command{.type = CommandType::COPY_ACCOUNT_USERNAME, .account = account});
			break;
		}

		case COPY_PASSWORD: {
			m_commands->push(Command{.type = CommandType::COPY_ACCOUNT_PASSWORD, .account = account});
			break;
		}

		case LOGIN: {
			m_commands->push(Command{.type = CommandType::LOGIN_ACCOUNT, .game = action.game, .account = account});
			break;
		}
	}

	close();
}

auto AccountSearch::move_highlight(i32 t_rows) -> void
{
	const u32 count = row_count();
	if (count == 0) return;

	const auto last = static_cast<i32>(count) - 1;
	m_highlighted   = static_cast<u32>(std::clamp(static_cast<i32>(m_highlighted) + t_rows, 0, last));

	const u32 shown = shown_rows();
	if (m_highlighted < m_first_row) {
		m_first_row = m_highlighted;
	} else if (m_highlighted >= m_first_row + shown) {
		m_first_row = m_highlighted + 1 - shown;
	}
}

auto AccountSearch::rows_in_view() const -> std::ranges::iota_view<u32, u32>
{
	const u32 count = row_count();
	const u32 last  = std::min(count, m_first_row + shown_rows());
	const u32 first = std::min(m_first_row, last);

	return std::views::iota(first, last);
}

auto AccountSearch::row_count() const -> u32
{
	return m_account ? m_action_count : static_cast<u32>(m_results.size());
}

auto AccountSearch::shown_rows() const -> u32
{
	return std::clamp(row_count(), 1u, K_MAX_SHOWN_ROWS);
}

auto AccountSearch::row_height() const -> float
{
	return std::max(36.0f, m_fonts->body.line_height() + 14.0f);
}

auto AccountSearch::layout() const -> AccountSearch::Layout
{
	const Vec2  window        = m_window->size();
	const float width         = std::min(K_PANEL_MAX_WIDTH, window.x - K_PANEL_SIDE_MARGIN * 2.0f);
	const float list_height   = K_LIST_PADDING * 2.0f + row_height() * static_cast<float>(shown_rows()) + (has_group_gap() ? K_GROUP_GAP : 0.0f);
	const float footer_height = controls::keycap_height(m_fonts->secondary) + K_FOOTER_PADDING_Y * 2.0f;
	const float rise          = K_PANEL_RISE * (1.0f - m_open_amount);

	Layout result{};
	result.panel = Rect{snapped_to_pixel((window.x - width) * 0.5f), snapped_to_pixel(K_TITLE_BAR_HEIGHT + K_PANEL_TOP_GAP - rise), width, K_HEADER_HEIGHT + 1.0f + list_height + 1.0f + footer_height + 1.0f};

	Rect remaining = result.panel;
	result.header  = remaining.split_top(K_HEADER_HEIGHT);
	remaining.split_top(1.0f);
	result.footer = remaining.split_bottom(footer_height + 1.0f).inset(1.0f, 0.0f);
	result.footer.h -= 1.0f;
	remaining.split_bottom(1.0f);
	result.list = remaining.inset(K_LIST_PADDING);

	return result;
}

auto AccountSearch::query_text_rect(const Layout& t_layout) const -> Rect
{
	const Rect& header = t_layout.header;
	const float left   = header.x + K_HEADER_PADDING + K_HEADER_ICON_SIZE + K_HEADER_ICON_GAP;
	const float right  = header.right() - K_HEADER_PADDING - controls::keycap_width(m_fonts->secondary, "Esc") - 8.0f;

	return Rect{left, header.y, std::max(0.0f, right - left), header.h};
}

auto AccountSearch::back_button_rect(const Layout& t_layout) const -> Rect
{
	const Rect& header = t_layout.header;
	const float size   = header.h - 16.0f;

	return Rect{header.x + 8.0f, header.y + 8.0f, size, size};
}

auto AccountSearch::row_rect(const Layout& t_layout, u32 t_row) const -> Rect
{
	const float height    = row_height();
	const bool  below_gap = has_group_gap() && m_first_row < K_GROUP_BREAK_ROW && t_row >= K_GROUP_BREAK_ROW;

	return Rect{t_layout.list.x, t_layout.list.y + static_cast<float>(t_row - m_first_row) * height + (below_gap ? K_GROUP_GAP : 0.0f), t_layout.list.w, height};
}

auto AccountSearch::row_at(const Layout& t_layout, Vec2 t_point) const -> std::optional<u32>
{
	if (!t_layout.list.contains(t_point)) return std::nullopt;

	for (const u32 row : rows_in_view()) {
		if (row_rect(t_layout, row).contains(t_point)) return row;
	}

	return std::nullopt;
}

auto AccountSearch::has_group_gap() const -> bool
{
	return m_account && m_action_count > K_GROUP_BREAK_ROW;
}

auto AccountSearch::update(float t_delta_seconds) -> void
{
	m_open_amount = animation::ease_toward(m_open_amount, m_open ? 1.0f : 0.0f, K_OPEN_EASE_RATE, t_delta_seconds);
	if (!m_open) return;

	m_query.update(t_delta_seconds);

	if (m_account && !is_valid(*m_account)) {
		show_results();
	}

	if (!m_account) {
		rebuild_results();
	}

	const u32 count = row_count();
	m_highlighted   = count == 0 ? 0 : std::min(m_highlighted, count - 1);
	m_first_row     = std::min(m_first_row, count > shown_rows() ? count - shown_rows() : 0);
}

auto AccountSearch::on_pointer_down(Vec2 t_point) -> bool
{
	if (!is_blocking()) return false;
	if (!m_open) return true;

	const Layout current = layout();
	m_pressed_row.reset();
	m_pressed_outside = !current.panel.contains(t_point);
	m_pressed_back    = m_account && back_button_rect(current).contains(t_point);

	if (m_pressed_outside || m_pressed_back) return true;

	if (!m_account && current.header.contains(t_point)) {
		m_query.set_focused(true);
		m_query.on_pointer_down(m_fonts->body, query_text_rect(current), t_point.x);
		return true;
	}

	m_pressed_row = row_at(current, t_point);

	return true;
}

auto AccountSearch::on_pointer_move(Vec2 t_point) -> bool
{
	if (!is_blocking()) return false;
	if (!m_open) return true;

	const Layout current = layout();

	if (m_query.m_selecting) {
		m_query.on_pointer_move(m_fonts->body, query_text_rect(current), t_point.x);
	}

	if (const std::optional<u32> hovered_row = row_at(current, t_point)) {
		m_highlighted = *hovered_row;
	}

	return true;
}

auto AccountSearch::on_pointer_up(Vec2 t_point) -> bool
{
	if (!is_blocking()) return false;
	if (!m_open) return true;

	const Layout             current = layout();
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

auto AccountSearch::on_right_click(Vec2 t_point) -> bool
{
	if (!is_blocking()) return false;
	if (!m_open || m_account) return true;

	const Layout current = layout();

	if (current.header.contains(t_point)) {
		m_query.set_focused(true);
		m_query.on_right_click(m_fonts->body, query_text_rect(current), t_point.x);
		m_commands->push(Command{.type = CommandType::SHOW_TEXT_MENU, .position = t_point, .text_input = &m_query});
	}

	return true;
}

auto AccountSearch::on_scroll(Vec2, float t_wheel_delta) -> bool
{
	if (!is_blocking()) return false;
	if (!m_open) return true;

	const u32 count = row_count();
	const u32 shown = shown_rows();
	if (count <= shown) return true;

	const i32 step = t_wheel_delta > 0.0f ? -1 : 1;
	m_first_row    = static_cast<u32>(std::clamp(static_cast<i32>(m_first_row) + step, 0, static_cast<i32>(count - shown)));
	m_highlighted  = std::clamp(m_highlighted, m_first_row, m_first_row + shown - 1);

	return true;
}

auto AccountSearch::on_key_down(os::Key t_key) -> bool
{
	if (!is_blocking()) return false;
	if (!m_open) return true;

	switch (t_key) {
		using enum os::Key;

		case ESCAPE: {
			if (m_account) {
				show_results();
			} else {
				close();
			}

			return true;
		}

		case UP: {
			move_highlight(-1);
			return true;
		}

		case DOWN: {
			move_highlight(1);
			return true;
		}

		case PAGE_UP: {
			move_highlight(-static_cast<i32>(K_MAX_SHOWN_ROWS) + 1);
			return true;
		}

		case PAGE_DOWN: {
			move_highlight(static_cast<i32>(K_MAX_SHOWN_ROWS) - 1);
			return true;
		}

		case ENTER: {
			activate(m_highlighted);
			return true;
		}

		case TAB: {
			if (!m_account) {
				activate(m_highlighted);
			}

			return true;
		}

		case LEFT:
		case BACKSPACE: {
			if (m_account) {
				show_results();
				return true;
			}

			break;
		}

		default: {
			break;
		}
	}

	if (!m_account) {
		m_query.on_key_down(t_key);
	}

	return true;
}

auto AccountSearch::on_char(u32 t_character) -> bool
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

auto AccountSearch::cursor() const -> CursorKind
{
	if (!m_open) return CursorKind::ARROW;
	if (m_query.m_selecting) return CursorKind::I_BEAM;

	const Layout current = layout();

	if (m_account && back_button_rect(current).contains(m_mouse)) return CursorKind::HAND;
	if (!m_account && current.header.contains(m_mouse)) return CursorKind::I_BEAM;

	return row_at(current, m_mouse) ? CursorKind::HAND : CursorKind::ARROW;
}

auto AccountSearch::draw_header(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) -> void
{
	const Rect& header    = t_layout.header;
	const Font& body      = m_fonts->body;
	const Font& secondary = m_fonts->secondary;

	if (!m_account) {
		const Rect icon{header.x + K_HEADER_PADDING, header.center().y - K_HEADER_ICON_SIZE * 0.5f, K_HEADER_ICON_SIZE, K_HEADER_ICON_SIZE};

		const Rect text_area = query_text_rect(t_layout);
		const Vec2 escape_right_center{header.right() - K_HEADER_PADDING, header.center().y};

		controls::draw_magnifier(t_draw_list, icon, faded(g_theme.text_dim, t_alpha));
		m_query.draw(t_draw_list, body, text_area, faded(g_theme.text, t_alpha), faded(m_settings->accent, t_alpha), text_area);
		controls::draw_shortcut(t_draw_list, secondary, escape_right_center, "Esc", g_theme.popup, t_alpha);
		return;
	}

	const Account* account      = m_library->account(*m_account);
	const Rect     back         = back_button_rect(t_layout);
	const Rect     arrow        = back.centered(K_HEADER_ICON_SIZE, K_HEADER_ICON_SIZE);
	const bool     back_hovered = back.contains(m_mouse);
	const Color    arrow_color  = back_hovered ? g_theme.text : g_theme.text_dim;

	if (back_hovered) {
		const Color highlight = faded(controls::glass_highlight(), t_alpha);
		t_draw_list->add_rounded_rect(back, rounded(K_ROW_RADIUS), highlight);
	}

	t_draw_list->add_image(arrow, m_assets->texture(Asset::ICON_ARROW_BACK), faded(arrow_color, t_alpha));

	const Rect icon{back.right() + 6.0f, header.center().y - K_ROW_ICON_SIZE * 0.5f, K_ROW_ICON_SIZE, K_ROW_ICON_SIZE};
	t_draw_list->add_image(icon, m_assets->texture(Asset::ICON_ACCOUNT), faded(g_theme.text_dim, t_alpha));

	const bool             has_note   = account->note[0] != '\0';
	const std::string_view name       = account->display_name();
	const float            name_x     = icon.right() + K_ROW_ICON_GAP;
	const float            room       = header.right() - K_HEADER_PADDING - name_x;
	const float            name_room  = has_note ? room * 0.6f : room;
	const float            name_width = std::min(text_width(body, name), name_room);
	const float            baseline   = body.centered_baseline(header);
	const Vec2             name_baseline{name_x, baseline};

	draw_text_truncated(t_draw_list, body, name_baseline, name, name_width, faded(g_theme.text, t_alpha));

	if (has_note) {
		const float username_x    = name_x + name_width + K_ROW_TEXT_GAP;
		const float username_room = header.right() - K_HEADER_PADDING - username_x;
		const Vec2  username_baseline{username_x, baseline};

		draw_text_truncated(t_draw_list, secondary, username_baseline, account->username, username_room, faded(g_theme.text_faint, t_alpha));
	}
}

auto AccountSearch::draw_result(DrawList* t_draw_list, Rect t_row, AccountRef t_account, bool t_highlighted, u8 t_alpha) const -> void
{
	const Font&    body      = m_fonts->body;
	const Font&    secondary = m_fonts->secondary;
	const Account* account   = m_library->account(t_account);

	if (t_highlighted) {
		const Color highlight = faded(controls::glass_highlight(), t_alpha);
		t_draw_list->add_rounded_rect(t_row, rounded(K_ROW_RADIUS), highlight);
	}

	const Rect icon{t_row.x + K_ROW_PADDING, t_row.center().y - K_ROW_ICON_SIZE * 0.5f, K_ROW_ICON_SIZE, K_ROW_ICON_SIZE};
	t_draw_list->add_image(icon, m_assets->texture(Asset::ICON_ACCOUNT), faded(g_theme.text_dim, t_alpha));

	const std::string_view region     = account->region;
	const float            right      = t_row.right() - K_ROW_PADDING - (m_region_column > 0.0f ? m_region_column + K_COLUMN_GAP : 0.0f);
	const float            baseline   = body.centered_baseline(t_row);
	const float            name_x     = icon.right() + K_ROW_ICON_GAP;
	const Color            name_color = faded(t_highlighted ? g_theme.text : mix(g_theme.text_dim, g_theme.text, 0.5f), t_alpha);

	const Vec2 name_baseline{name_x, baseline};

	if (account->note[0] == '\0') {
		draw_text_truncated(t_draw_list, body, name_baseline, account->username, right - name_x, name_color);
	} else {
		const float name_width = std::min(m_name_column, std::max(0.0f, (right - name_x) * 0.6f));
		const float username_x = name_x + name_width + K_COLUMN_GAP;
		const Vec2  username_baseline{username_x, baseline};

		draw_text_truncated(t_draw_list, body, name_baseline, account->note, name_width, name_color);
		draw_text_truncated(t_draw_list, secondary, username_baseline, account->username, right - username_x, faded(g_theme.text_faint, t_alpha));
	}

	if (!region.empty()) {
		const float height = secondary.line_height() + 2.0f;
		const Rect  chip{t_row.right() - K_ROW_PADDING - m_region_column, t_row.center().y - height * 0.5f, m_region_column, height};

		t_draw_list->add_bordered_rect(chip, rounded(4.0f), faded(g_theme.popup, t_alpha), faded(g_theme.border, t_alpha), 1.0f);
		draw_text_centered(t_draw_list, secondary, chip, region, faded(g_theme.text_dim, t_alpha));
	}
}

auto AccountSearch::draw_action(DrawList* t_draw_list, Rect t_row, const Action& t_action, bool t_highlighted, u8 t_alpha) const -> void
{
	const Font& body = m_fonts->body;

	if (t_highlighted) {
		const Color highlight = faded(controls::glass_highlight(), t_alpha);
		t_draw_list->add_rounded_rect(t_row, rounded(K_ROW_RADIUS), highlight);
	}

	const Rect       icon{t_row.x + K_ROW_PADDING, t_row.center().y - K_ROW_ICON_SIZE * 0.5f, K_ROW_ICON_SIZE, K_ROW_ICON_SIZE};
	const Color      icon_color = faded(g_theme.text_dim, t_alpha);
	char             label[96];
	std::string_view text;

	switch (t_action.kind) {
		using enum ActionKind;

		case EDIT: {
			t_draw_list->add_image(icon, m_assets->texture(Asset::ICON_EDIT_BOX), icon_color);
			text = "Edit account";
			break;
		}

		case COPY_USERNAME: {
			t_draw_list->add_image(icon, m_assets->texture(Asset::ICON_COPY), icon_color);
			text = "Copy username";
			break;
		}

		case COPY_PASSWORD: {
			t_draw_list->add_image(icon, m_assets->texture(Asset::ICON_KEY), icon_color);
			text = "Copy password";
			break;
		}

		case LOGIN: {
			const Game& game = m_library->games[t_action.game];

			if (game.icon != nullptr) {
				t_draw_list->add_image(icon, game.icon, faded(Color{255, 255, 255, 255}, t_alpha), rounded(4.0f));
			} else {
				t_draw_list->add_rounded_rect(icon, rounded(4.0f), faded(game.accent, t_alpha));
			}

			text = format_into(label, "Log in to {}", game.title);
			break;
		}
	}

	const Vec2  text_baseline{icon.right() + K_ROW_ICON_GAP, body.centered_baseline(t_row)};
	const float text_room  = t_row.right() - K_ROW_PADDING - text_baseline.x;
	const Color text_color = t_highlighted ? g_theme.text : mix(g_theme.text_dim, g_theme.text, 0.5f);

	draw_text_truncated(t_draw_list, body, text_baseline, text, text_room, faded(text_color, t_alpha));
}

auto AccountSearch::draw_footer(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) const -> void
{
	const Font& font   = m_fonts->secondary;
	const Rect& footer = t_layout.footer;
	const float cap    = controls::keycap_height(font);
	const float cap_y  = snapped_to_pixel(footer.center().y - cap * 0.5f);
	const Rect  cap_row{footer.x, cap_y, footer.w, cap};
	const float baseline = font.centered_baseline(cap_row);
	const Color label    = faded(g_theme.text_faint, t_alpha);
	float       x        = footer.x + K_FOOTER_PADDING_X;

	const float       bottom_radius  = K_PANEL_RADIUS - 1.0f;
	const CornerRadii footer_corners = rounded(0.0f, 0.0f, bottom_radius, bottom_radius);
	const Color       footer_fill    = faded(with_alpha(g_theme.field, K_FOOTER_ALPHA), t_alpha);

	t_draw_list->add_rounded_rect(footer, footer_corners, footer_fill);

	const auto key = [&](std::string_view t_key) {
		const float width = controls::keycap_width(font, t_key);
		const Rect  keycap{snapped_to_pixel(x), cap_y, width, cap};

		controls::draw_keycap(t_draw_list, font, keycap, t_key, g_theme.field, t_alpha);
		x += width + K_HINT_KEY_GAP;
	};

	const auto hint = [&](std::string_view t_text) {
		const Vec2 text_baseline{snapped_to_pixel(x), baseline};

		draw_text(t_draw_list, font, text_baseline, t_text, label);
		x += text_width(font, t_text) + K_HINT_GAP;
	};

	const Vec2 arrows_top_left{x, cap_y};
	x += controls::draw_arrow_keys(t_draw_list, arrows_top_left, cap, t_alpha) + K_HINT_KEY_GAP;
	hint("navigate");
	key("Enter");
	hint(m_account ? "run" : "open");

	if (m_account) {
		key("Esc");
		hint("back");
	}
}

auto AccountSearch::draw(DrawList* t_draw_list) -> void
{
	if (m_open_amount <= 0.01f) return;

	const auto           alpha   = to_alpha(m_open_amount);
	const Vec2           window  = m_window->size();
	const Layout         current = layout();
	const RoundnessScope corners{controls::K_POPUP_ROUNDNESS};

	const Rect below_title_bar{0.0f, K_TITLE_BAR_HEIGHT, window.x, window.y - K_TITLE_BAR_HEIGHT};
	const Rect header_line{current.panel.x + 1.0f, current.header.bottom(), current.panel.w - 2.0f, 1.0f};
	const Rect footer_line{current.panel.x + 1.0f, current.footer.y - 1.0f, current.panel.w - 2.0f, 1.0f};

	controls::draw_popup_backdrop(t_draw_list, below_title_bar, m_open_amount);
	controls::draw_popup_shadow(t_draw_list, current.panel, K_PANEL_RADIUS, m_open_amount);
	controls::draw_glass(t_draw_list, current.panel, rounded(K_PANEL_RADIUS), controls::GlassSurface::PANEL, alpha);

	draw_header(t_draw_list, current, alpha);
	t_draw_list->add_rect(header_line, faded(g_theme.separator, alpha));
	t_draw_list->add_rect(footer_line, faded(g_theme.separator, alpha));

	const u32 count = row_count();

	if (count == 0) {
		const bool             empty_library = !m_account && m_query.value().empty();
		const std::string_view message       = empty_library ? "No accounts yet" : "No accounts match";
		draw_text_centered(t_draw_list, m_fonts->secondary, current.list, message, faded(g_theme.text_faint, alpha));
	}

	for (const u32 row : rows_in_view()) {
		const Rect rect        = row_rect(current, row);
		const bool highlighted = row == m_highlighted;

		if (m_account) {
			if (row == K_GROUP_BREAK_ROW && row > m_first_row) {
				const float y = snapped_to_pixel(rect.y - K_GROUP_GAP * 0.5f - 0.5f);
				const Rect  group_line{rect.x + K_ROW_PADDING, y, rect.w - K_ROW_PADDING * 2.0f, 1.0f};
				t_draw_list->add_rect(group_line, faded(g_theme.separator, alpha));
			}

			draw_action(t_draw_list, rect.inset(0.0f, 1.0f), m_actions[row], highlighted, alpha);
		} else {
			draw_result(t_draw_list, rect.inset(0.0f, 1.0f), m_results[row], highlighted, alpha);
		}
	}

	draw_footer(t_draw_list, current, alpha);
}
