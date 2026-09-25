#include "ui/account_modal.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>
#include <span>
#include <utility>

#include <Windows.h>

#include "core/animation.h"
#include "core/profiler.h"
#include "core/settings.h"
#include "gfx/assets.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "platform/window.h"
#include "ui/controls.h"
#include "ui/text.h"
#include "ui/toasts.h"

namespace {
constexpr float open_ease_rate = 14.0f;

constexpr float panel_width_fraction = 0.78f;
constexpr float panel_height_fraction = 0.71f;
constexpr Vec2 panel_max_size{810.0f, 480.0f};
constexpr float reference_body_pixel_height = 24.0f;
constexpr float panel_margin = 48.0f;
constexpr float panel_closed_scale = 0.92f;
constexpr float panel_border = 1.5f;
constexpr float panel_radius = 16.0f;

constexpr float art_column_fraction = 0.35f;
constexpr float art_separator_width = 2.0f;
constexpr float close_badge_size = 40.0f;
constexpr float close_badge_margin = 12.0f;
constexpr float close_badge_icon_size = 18.0f;

constexpr float row_padding = 24.0f;
constexpr float row_top_padding = 14.0f;
constexpr float row_line_gap = 4.0f;
constexpr float row_bottom_padding = 10.0f;
constexpr float row_button_gap = 10.0f;
constexpr float row_icon_inset = 5.0f;
constexpr float confirm_pill_extra_width = 46.0f;
constexpr float scrollbar_margin = 4.0f;

constexpr float action_button_width = 108.0f;
constexpr float action_button_gap = 16.0f;

constexpr float field_label_gap = 8.0f;
constexpr float field_group_gap = 26.0f;
constexpr float field_radius = 8.0f;
constexpr float reveal_button_size = 24.0f;
constexpr float reveal_button_margin = 6.0f;

constexpr float chip_icon_size = 16.0f;
constexpr float chip_icon_inset = 10.0f;
constexpr float chip_icon_gap = 7.0f;
constexpr float chip_padding_right = 12.0f;

constexpr float ring_outer_radius = 40.0f;
constexpr float ring_inner_radius = 32.0f;
constexpr float ring_glow_margin = 22.0f;
constexpr float ring_sweep_degrees = 112.0f;
constexpr float ring_spin_degrees_per_second = 260.0f;
constexpr u32 max_message_lines = 3;

constexpr Color color_panel_border{74, 74, 80, 255};
constexpr Color color_panel{24, 24, 27, 255};
constexpr Color color_separator{48, 48, 53, 255};
constexpr Color color_art_separator{90, 90, 96, 255};
constexpr Color color_text_bright{232, 232, 236, 255};
constexpr Color color_text_dim{150, 150, 156, 255};
constexpr Color color_text_faint{130, 130, 136, 255};
constexpr Color color_success{80, 200, 120, 255};
constexpr Color color_error{220, 90, 80, 255};
constexpr Color color_white{255, 255, 255, 255};
constexpr Color color_hover_badge{56, 56, 62, 255};
constexpr Color color_row_selected{58, 58, 62, 255};
constexpr Color color_row_hover{38, 38, 43, 255};
constexpr Color color_remove_hover{68, 42, 42, 255};
constexpr Color color_neutral_button{46, 46, 52, 255};
constexpr Color color_neutral_button_hover{60, 60, 66, 255};
constexpr Color color_disabled_button{60, 58, 70, 255};
constexpr Color color_delete_button{60, 45, 45, 255};
constexpr Color color_scroll_thumb{120, 120, 128, 190};
constexpr Color color_field_border{46, 46, 52, 255};
constexpr Color color_field{24, 24, 27, 255};
constexpr Color color_field_border_focused{225, 225, 230, 255};
constexpr Color color_field_focused{42, 42, 46, 255};

struct FieldSpec {
	const char *label;
	u32 max_length;
};

constexpr FieldSpec field_specs[]{
	{"Note", sizeof(Account::note) - 1},
	{"Username", sizeof(Account::username) - 1},
	{"Password", sizeof(Account::password) - 1},
};

float row_height(const Fonts &t_fonts)
{
	return row_top_padding + t_fonts.body().line_height() + row_line_gap + t_fonts.secondary().line_height() +
		   row_bottom_padding;
}

float header_height(const Fonts &t_fonts)
{
	return t_fonts.body().line_height() + 20.0f;
}

float footer_height(const Fonts &t_fonts)
{
	return std::max(56.0f, t_fonts.body().line_height() + 20.0f);
}

float action_button_height(const Fonts &t_fonts)
{
	return std::max(36.0f, t_fonts.body().line_height() + 14.0f);
}

float field_input_height(const Fonts &t_fonts)
{
	return std::max(34.0f, t_fonts.body().line_height() + 12.0f);
}

float field_block_height(const Fonts &t_fonts)
{
	return t_fonts.secondary().line_height() + field_label_gap + field_input_height(t_fonts) + field_group_gap;
}

float row_button_size(const Fonts &t_fonts)
{
	return std::max(28.0f, t_fonts.secondary().line_height() + 8.0f);
}

Rect vertically_centered(Rect t_strip, float t_x, float t_width, float t_height)
{
	return Rect{t_x, t_strip.y + (t_strip.h - t_height) * 0.5f, t_width, t_height};
}

std::string_view game_count_text(u16 t_mask, char (&t_buffer)[16])
{
	const int count = t_mask == 0 ? 1 : std::popcount(t_mask);
	const int written = std::snprintf(t_buffer, sizeof(t_buffer), "%d %s", count, count == 1 ? "game" : "games");

	return std::string_view{t_buffer, static_cast<usize>(std::max(written, 0))};
}

std::string_view stage_message(LoginStage t_stage)
{
	switch (t_stage) {
		case LoginStage::idle:
			return "";
		case LoginStage::waiting_for_process:
			return "Launching Riot Client...";
		case LoginStage::connecting:
			return "Waiting for Riot Client...";
		case LoginStage::authenticating:
			return "Logging in...";
		case LoginStage::launching:
			return "Launching game...";
		case LoginStage::success:
			return "Logged in!";
		case LoginStage::error:
			return "Something went wrong.";
		case LoginStage::cancelled:
			return "Cancelled.";
	}

	return "";
}
}

AccountModal::AccountModal(Library &t_library, const Settings &t_settings, const Fonts &t_fonts, const Assets &t_assets,
						   const Window &t_window, Toasts &t_toasts, CommandQueue &t_commands)
	: m_library(t_library)
	, m_settings(t_settings)
	, m_fonts(t_fonts)
	, m_assets(t_assets)
	, m_window(t_window)
	, m_toasts(t_toasts)
	, m_commands(t_commands)
	, m_visible_games(t_library, t_fonts)
{
	for (u32 i = 0; i < field_count; i += 1) {
		m_fields[i].set_max_length(field_specs[i].max_length);
	}
}

bool AccountModal::has_game() const
{
	return m_game >= 0 && static_cast<u32>(m_game) < m_library.game_count();
}

Rect AccountModal::panel_rect() const
{
	const float size_scale = std::max(1.0f, m_fonts.body().pixel_height() / reference_body_pixel_height);
	const Vec2 max_size{panel_max_size.x * size_scale, panel_max_size.y * size_scale};
	const Vec2 window = m_window.size();

	float width =
		std::min({window.x * panel_width_fraction, max_size.x, std::max(0.0f, window.x - panel_margin * 2.0f)});
	float height =
		std::min({window.y * panel_height_fraction, max_size.y, std::max(0.0f, window.y - panel_margin * 2.0f)});

	const float aspect = max_size.x / max_size.y;
	if (width / height > aspect) {
		width = height * aspect;
	} else {
		height = width / aspect;
	}

	const float scale = panel_closed_scale + (1.0f - panel_closed_scale) * m_open_amount;

	return Rect{0.0f, 0.0f, window.x, window.y}.centered(width * scale, height * scale);
}

AccountModal::Layout AccountModal::layout() const
{
	Layout result{};
	result.panel = panel_rect();
	result.inner = result.panel.inset(panel_border);

	Rect content = result.inner;
	result.footer = content.split_bottom(footer_height(m_fonts));
	result.art_column = Rect{content.x, content.y, content.w * art_column_fraction, content.h};

	const float main_x = result.art_column.right() + art_separator_width;
	result.main_column = Rect{main_x, content.y, content.right() - main_x, content.h};

	return result;
}

AccountModal::AccountRows AccountModal::account_rows(const Layout &t_layout) const
{
	const Rect main = t_layout.main_column;
	const float header = header_height(m_fonts);

	AccountRows rows{};
	rows.region = Rect{main.x, main.y + header, main.w, main.h - header};
	rows.row_height = row_height(m_fonts);

	if (has_game()) {
		rows.accounts = m_library.visible_accounts(static_cast<u32>(m_game));
	}

	const Rect track{rows.region.right() - scrollbar_width - scrollbar_margin, rows.region.y, scrollbar_width,
					 rows.region.h};
	rows.scroll = ScrollGeometry{track, rows.accounts.count * rows.row_height, rows.region.h};

	return rows;
}

Rect AccountModal::row_rect(const Layout &t_layout, const AccountRows &t_rows, u32 t_row) const
{
	const Rect main = t_layout.main_column;
	const float y = t_rows.region.y + t_row * t_rows.row_height - m_rows_scroll.offset();

	return Rect{main.x + row_padding, y, main.w - row_padding * 2.0f, t_rows.row_height};
}

i32 AccountModal::row_at(const Layout &t_layout, const AccountRows &t_rows, Vec2 t_point) const
{
	if (!t_rows.region.contains(t_point)) return -1;

	for (u32 i = 0; i < t_rows.accounts.count; i += 1) {
		const Rect row = row_rect(t_layout, t_rows, i);
		if (row.overlaps_vertically(t_rows.region) && row.contains(t_point)) return static_cast<i32>(i);
	}

	return -1;
}

Rect AccountModal::remove_button_rect(Rect t_row) const
{
	const float size = row_button_size(m_fonts);

	return vertically_centered(t_row, t_row.right() - size, size, size);
}

Rect AccountModal::confirm_delete_rect(Rect t_row) const
{
	const Rect remove = remove_button_rect(t_row);

	return Rect{remove.x - confirm_pill_extra_width, remove.y, remove.w + confirm_pill_extra_width, remove.h};
}

Rect AccountModal::edit_button_rect(Rect t_row) const
{
	const Rect remove = remove_button_rect(t_row);

	return Rect{remove.x - row_button_gap - remove.w, remove.y, remove.w, remove.h};
}

Rect AccountModal::add_button_rect(Rect t_main) const
{
	const float size = row_button_size(m_fonts);
	const Rect header{t_main.x, t_main.y, t_main.w, header_height(m_fonts)};

	return vertically_centered(header, t_main.right() - row_padding - size, size, size);
}

Rect AccountModal::primary_button_rect(Rect t_footer) const
{
	return vertically_centered(t_footer, t_footer.right() - row_padding - action_button_width, action_button_width,
							   action_button_height(m_fonts));
}

Rect AccountModal::cancel_button_rect(Rect t_primary) const
{
	return Rect{t_primary.x - action_button_gap - action_button_width, t_primary.y, action_button_width, t_primary.h};
}

Rect AccountModal::delete_button_rect(Rect t_footer) const
{
	return vertically_centered(t_footer, t_footer.x + row_padding, action_button_width, action_button_height(m_fonts));
}

Rect AccountModal::field_block_rect(Rect t_main, u32 t_field) const
{
	const float header = header_height(m_fonts);
	const float block_height = field_block_height(m_fonts);
	const float slack = std::max(0.0f, (t_main.h - header - block_height * field_count) * 0.28f);

	return Rect{t_main.x + row_padding, t_main.y + header + slack + t_field * block_height,
				t_main.w - row_padding * 2.0f, block_height};
}

Rect AccountModal::field_input_rect(Rect t_main, u32 t_field) const
{
	const Rect block = field_block_rect(t_main, t_field);

	return Rect{block.x, block.y + m_fonts.secondary().line_height() + field_label_gap, block.w,
				field_input_height(m_fonts)};
}

Rect AccountModal::field_text_rect(Rect t_main, u32 t_field) const
{
	Rect input = field_input_rect(t_main, t_field);

	if (t_field == static_cast<u32>(EditField::password)) {
		input.w -= reveal_button_size + reveal_button_margin;
	}

	return input;
}

Rect AccountModal::reveal_button_rect(Rect t_main) const
{
	const Rect password = field_input_rect(t_main, static_cast<u32>(EditField::password));

	return vertically_centered(password, password.right() - reveal_button_size - reveal_button_margin,
							   reveal_button_size, reveal_button_size);
}

Rect AccountModal::visibility_chip_rect(Rect t_main) const
{
	char buffer[16];
	const float text = text_width(m_fonts.secondary(), game_count_text(m_visible_games.mask(), buffer));
	const float width = chip_icon_inset + chip_icon_size + chip_icon_gap + text + chip_padding_right;
	const float height = row_button_size(m_fonts);
	const Rect header{t_main.x, t_main.y, t_main.w, header_height(m_fonts)};

	return vertically_centered(header, t_main.right() - row_padding - width, width, height);
}

i32 AccountModal::focused_field() const
{
	for (u32 i = 0; i < field_count; i += 1) {
		if (m_fields[i].is_focused()) return static_cast<i32>(i);
	}

	return -1;
}

void AccountModal::focus_field(i32 t_field)
{
	for (u32 i = 0; i < field_count; i += 1) {
		m_fields[i].set_focused(static_cast<i32>(i) == t_field);
	}
}

i32 AccountModal::field_at(Rect t_main, Vec2 t_point) const
{
	if (m_visible_games.is_open() || reveal_button_rect(t_main).contains(t_point)) return -1;

	for (u32 i = 0; i < field_count; i += 1) {
		if (field_input_rect(t_main, i).contains(t_point)) return static_cast<i32>(i);
	}

	return -1;
}

void AccountModal::open(i32 t_game)
{
	m_open = true;
	m_game = t_game;
	m_mode = Mode::account_list;
	m_rows_scroll = Scrollable{};
	m_selected_row = -1;
}

void AccountModal::close()
{
	m_open = false;
	m_row_delete.disarm();
	m_form_delete.disarm();
}

bool AccountModal::can_quick_login(i32 t_game, i32 t_row) const
{
	return t_game >= 0 && t_row >= 0 && m_library.visible_account(static_cast<u32>(t_game), static_cast<u32>(t_row));
}

void AccountModal::quick_login(i32 t_game, i32 t_row)
{
	open(t_game);
	request_login(t_game, t_row);
}

const Account *AccountModal::account_at_row(i32 t_row) const
{
	if (!has_game() || t_row < 0) return nullptr;

	const auto ref = m_library.visible_account(static_cast<u32>(m_game), static_cast<u32>(t_row));

	return ref ? &m_library.account(*ref) : nullptr;
}

void AccountModal::start_adding()
{
	m_form_delete.disarm();
	m_mode = Mode::edit_account;
	m_edited_row = -1;

	for (TextInput &input : m_fields) {
		input.set_value("");
	}

	focus_field(static_cast<i32>(EditField::username));
	field(EditField::password).set_masked(true);

	m_visible_games.set_mask(static_cast<u16>(1u << m_game));
	m_visible_games.close();
}

void AccountModal::start_editing(u32 t_row)
{
	m_row_delete.disarm();
	m_form_delete.disarm();
	m_mode = Mode::edit_account;
	m_edited_row = static_cast<i32>(t_row);

	u16 mask = 0;
	if (const auto ref = m_library.visible_account(static_cast<u32>(m_game), t_row)) {
		const Account &account = m_library.account(*ref);
		field(EditField::note).set_value(account.note);
		field(EditField::username).set_value(account.username);
		field(EditField::password).set_value(account.password);
		mask = account.visible_games(ref->game);
	}

	focus_field(static_cast<i32>(EditField::username));
	field(EditField::password).set_masked(true);

	m_visible_games.set_mask(mask);
	m_visible_games.close();
}

bool AccountModal::can_save() const
{
	return !field(EditField::username).value().empty() && !field(EditField::password).value().empty();
}

void AccountModal::save_edit()
{
	m_mode = Mode::account_list;
	if (!has_game()) return;

	const auto game = static_cast<u32>(m_game);

	if (m_edited_row < 0) {
		Account account{.visible_game_mask = m_visible_games.mask()};
		account.assign(field(EditField::username).value(), field(EditField::note).value(),
					   field(EditField::password).value());

		if (const auto added = m_library.add_account(game, account)) {
			m_selected_row = static_cast<i32>(added->index);
		} else {
			notify("This game can't hold any more accounts.");
		}

		return;
	}

	if (const auto ref = m_library.visible_account(game, static_cast<u32>(m_edited_row))) {
		Account &account = m_library.account(*ref);
		account.assign(field(EditField::username).value(), field(EditField::note).value(),
					   field(EditField::password).value());
		account.visible_game_mask = m_visible_games.mask();
	}
}

void AccountModal::delete_edited_account()
{
	m_mode = Mode::account_list;
	if (!has_game()) return;

	if (const auto ref = m_library.visible_account(static_cast<u32>(m_game), static_cast<u32>(m_edited_row))) {
		m_library.remove_account(*ref);
	}

	m_selected_row = -1;
}

void AccountModal::remove_row(u32 t_row)
{
	const auto ref = m_library.visible_account(static_cast<u32>(m_game), t_row);
	if (!ref) return;

	m_library.remove_account(*ref);

	const auto removed = static_cast<i32>(t_row);
	if (m_selected_row == removed) {
		m_selected_row = -1;
	} else if (m_selected_row > removed) {
		m_selected_row -= 1;
	}
}

void AccountModal::confirm_row_delete(u32 t_row)
{
	if (m_row_delete.confirm(static_cast<i32>(t_row))) {
		remove_row(t_row);
		notify("Account deleted.");
	} else {
		notify_delete_armed();
	}
}

void AccountModal::notify(std::string_view t_message)
{
	m_toasts.notify(Notification{.message = t_message});
}

void AccountModal::notify_delete_armed()
{
	m_toasts.notify_countdown("Click again to delete this account.", ConfirmLatch::window_seconds);
}

void AccountModal::request_login(i32 t_game, i32 t_row)
{
	m_selected_row = t_row;
	m_mode = Mode::login_progress;
	m_login_seconds = 0.0f;

	if (m_login.is_active() && !LoginAttempt::is_terminal(m_login.stage())) {
		m_login.cancel();
		m_queued_login_game = t_game;
		m_queued_login_row = t_row;
		return;
	}

	m_queued_login_game = -1;
	m_queued_login_row = -1;
	start_login(t_game, t_row);
}

void AccountModal::start_login(i32 t_game, i32 t_row)
{
	const auto ref = m_library.visible_account(static_cast<u32>(t_game), static_cast<u32>(t_row));
	if (!ref) return;

	const Account &account = m_library.account(*ref);
	m_login.start(account.username, account.password, m_library.game(static_cast<u32>(t_game)).title);
}

void AccountModal::update(float t_delta_seconds)
{
	m_open_amount = animation::ease_toward(m_open_amount, m_open ? 1.0f : 0.0f, open_ease_rate, t_delta_seconds);

	if (!m_open && m_open_amount == 0.0f) {
		m_game = -1;
	}

	const bool was_armed = m_row_delete.is_armed() || m_form_delete.is_armed();
	m_row_delete.update(t_delta_seconds);
	m_form_delete.update(t_delta_seconds);

	if (was_armed && !m_row_delete.is_armed() && !m_form_delete.is_armed()) {
		m_toasts.dismiss_countdown();
	}

	switch (m_mode) {
		case Mode::account_list:
			m_rows_scroll.update(t_delta_seconds);
			break;

		case Mode::login_progress:
			m_login_seconds += t_delta_seconds;
			break;

		case Mode::edit_account:
			for (TextInput &input : m_fields) {
				input.update(t_delta_seconds);
			}

			break;
	}

	m_visible_games.update(t_delta_seconds);
	m_login.update();

	if (has_queued_login() && !m_login.is_active()) {
		const i32 game = std::exchange(m_queued_login_game, -1);
		const i32 row = std::exchange(m_queued_login_row, -1);

		m_login_seconds = 0.0f;
		start_login(game, row);
	}
}

bool AccountModal::on_pointer_down(Vec2 t_point)
{
	if (!is_blocking()) return false;

	const Layout current = layout();

	if (m_mode == Mode::edit_account) {
		const i32 pressed = field_at(current.main_column, t_point);

		if (pressed >= 0) {
			focus_field(pressed);
			m_fields[pressed].on_pointer_down(m_fonts.body(), field_text_rect(current.main_column, pressed), t_point.x);
		}
	} else if (m_mode == Mode::account_list) {
		m_rows_scroll.on_pointer_down(t_point, account_rows(current).scroll);
	}

	return true;
}

bool AccountModal::on_pointer_move(Vec2 t_point)
{
	if (!is_blocking()) return false;

	const Layout current = layout();

	for (u32 i = 0; i < field_count; i += 1) {
		if (m_fields[i].is_selecting()) {
			m_fields[i].on_pointer_move(m_fonts.body(), field_text_rect(current.main_column, i), t_point.x);
		}
	}

	if (m_rows_scroll.is_dragging()) {
		m_rows_scroll.on_pointer_move(t_point.y, account_rows(current).scroll);
	}

	return true;
}

bool AccountModal::on_pointer_up(Vec2 t_point)
{
	if (m_rows_scroll.is_dragging()) {
		m_rows_scroll.on_pointer_up();
		return true;
	}

	if (!is_blocking()) return false;

	bool ended_text_selection = false;
	for (TextInput &input : m_fields) {
		ended_text_selection = ended_text_selection || input.is_selecting();
		input.on_pointer_up();
	}

	if (ended_text_selection) return true;

	const Layout current = layout();
	const bool clicked_away = !current.panel.contains(t_point);
	const bool clicked_close = Rect{current.art_column.x + close_badge_margin,
									current.art_column.y + close_badge_margin, close_badge_size, close_badge_size}
								   .contains(t_point);

	if (m_mode != Mode::login_progress && (clicked_away || clicked_close)) {
		close();
		return true;
	}

	switch (m_mode) {
		case Mode::account_list:
			if (has_game()) {
				handle_list_click(current, t_point);
			}

			break;

		case Mode::login_progress:
			if (primary_button_rect(current.footer).contains(t_point)) {
				m_queued_login_game = -1;
				m_queued_login_row = -1;
				m_login.cancel();
				m_mode = Mode::account_list;
			}

			break;

		case Mode::edit_account:
			handle_edit_click(current, t_point);
			break;
	}

	return true;
}

void AccountModal::handle_list_click(const Layout &t_layout, Vec2 t_point)
{
	if (add_button_rect(t_layout.main_column).contains(t_point)) {
		start_adding();
		return;
	}

	const AccountRows rows = account_rows(t_layout);

	if (rows.region.contains(t_point)) {
		for (u32 i = 0; i < rows.accounts.count; i += 1) {
			const Rect row = row_rect(t_layout, rows, i);
			if (!row.overlaps_vertically(rows.region)) continue;

			const bool armed = m_row_delete.is_armed(static_cast<i32>(i));
			if ((armed ? confirm_delete_rect(row) : remove_button_rect(row)).contains(t_point)) {
				confirm_row_delete(i);
				return;
			}

			if (!armed && edit_button_rect(row).contains(t_point)) {
				start_editing(i);
				return;
			}
		}

		m_selected_row = row_at(t_layout, rows, t_point);
	}

	if (m_selected_row >= 0 && primary_button_rect(t_layout.footer).contains(t_point)) {
		request_login(m_game, m_selected_row);
	}
}

void AccountModal::handle_edit_click(const Layout &t_layout, Vec2 t_point)
{
	const Rect main = t_layout.main_column;

	if (m_visible_games.is_open()) {
		if (!m_visible_games.on_pointer_down(t_point)) {
			m_visible_games.close();
		}

		return;
	}

	const Rect chip = visibility_chip_rect(main);
	if (chip.contains(t_point)) {
		m_visible_games.open(chip, main);
		return;
	}

	if (reveal_button_rect(main).contains(t_point)) {
		TextInput &password = field(EditField::password);
		password.set_masked(!password.is_masked());
		return;
	}

	focus_field(-1);

	const Rect save = primary_button_rect(t_layout.footer);

	if (cancel_button_rect(save).contains(t_point)) {
		m_mode = Mode::account_list;
	} else if (can_save() && save.contains(t_point)) {
		save_edit();
	} else if (m_edited_row >= 0 && delete_button_rect(t_layout.footer).contains(t_point)) {
		if (m_form_delete.confirm(0)) {
			delete_edited_account();
			notify("Account deleted.");
		} else {
			notify_delete_armed();
		}
	}
}

bool AccountModal::on_right_click(Vec2 t_point)
{
	if (!is_blocking()) return false;

	const Layout current = layout();

	if (m_mode == Mode::edit_account) {
		const i32 clicked = field_at(current.main_column, t_point);

		if (clicked >= 0) {
			focus_field(clicked);
			m_fields[clicked].on_right_click(m_fonts.body(), field_text_rect(current.main_column, clicked), t_point.x);
			m_commands.push(
				Command{.type = CommandType::show_text_menu, .position = t_point, .text_input = &m_fields[clicked]});
		}
	} else if (m_mode == Mode::account_list && has_game()) {
		const i32 row = row_at(current, account_rows(current), t_point);

		if (row >= 0) {
			m_commands.push(Command{.type = CommandType::show_account_menu, .index = row, .position = t_point});
		}
	}

	return true;
}

bool AccountModal::on_scroll(Vec2, float t_wheel_delta)
{
	if (!is_blocking()) return false;

	if (m_mode == Mode::account_list) {
		m_rows_scroll.on_scroll(t_wheel_delta, account_rows(layout()).scroll);
	}

	return true;
}

bool AccountModal::handle_list_key(u32 t_key)
{
	const Layout current = layout();
	const AccountRows rows = account_rows(current);
	if (rows.accounts.count == 0) return false;

	switch (t_key) {
		case VK_UP:
		case VK_DOWN: {
			const i32 step = t_key == VK_DOWN ? 1 : -1;
			const i32 last = static_cast<i32>(rows.accounts.count) - 1;
			m_selected_row = m_selected_row < 0 ? 0 : std::clamp(m_selected_row + step, 0, last);

			const Rect row = row_rect(current, rows, static_cast<u32>(m_selected_row));
			m_rows_scroll.reveal(row.y, row.bottom(), rows.region.y, rows.region.bottom(), rows.scroll);
			return true;
		}

		case VK_RETURN:
			if (m_selected_row >= 0) {
				request_login(m_game, m_selected_row);
			}

			return true;

		case VK_DELETE:
			if (m_selected_row >= 0) {
				confirm_row_delete(static_cast<u32>(m_selected_row));
			}

			return true;

		default:
			return false;
	}
}

bool AccountModal::on_key_down(u32 t_key)
{
	if (!is_blocking()) return false;

	if (t_key == VK_ESCAPE) {
		if (m_mode == Mode::edit_account) {
			m_mode = Mode::account_list;
		} else if (m_mode == Mode::account_list && m_selected_row >= 0) {
			m_selected_row = -1;
		} else if (m_mode == Mode::account_list) {
			close();
		}

		return true;
	}

	if (m_mode == Mode::account_list) {
		handle_list_key(t_key);
	} else if (m_mode == Mode::edit_account && t_key == VK_TAB) {
		focus_field((focused_field() + 1) % static_cast<i32>(field_count));
	} else if (m_mode == Mode::edit_account) {
		for (TextInput &input : m_fields) {
			input.on_key_down(t_key);
		}
	}

	return true;
}

bool AccountModal::on_char(u32 t_character)
{
	if (!is_blocking()) return false;

	if (m_mode == Mode::edit_account) {
		for (TextInput &input : m_fields) {
			input.on_char(t_character);
		}
	}

	return true;
}

CursorKind AccountModal::list_cursor(const Layout &t_layout) const
{
	if (add_button_rect(t_layout.main_column).contains(m_mouse)) return CursorKind::hand;

	const AccountRows rows = account_rows(t_layout);
	if (row_at(t_layout, rows, m_mouse) >= 0 || m_rows_scroll.is_over_track(m_mouse, rows.scroll)) {
		return CursorKind::hand;
	}

	const bool over_login = primary_button_rect(t_layout.footer).contains(m_mouse);

	return m_selected_row >= 0 && !m_login.is_active() && over_login ? CursorKind::hand : CursorKind::arrow;
}

CursorKind AccountModal::edit_cursor(const Layout &t_layout) const
{
	const Rect main = t_layout.main_column;

	if (m_visible_games.is_open()) return m_visible_games.cursor(m_mouse);
	if (visibility_chip_rect(main).contains(m_mouse) || reveal_button_rect(main).contains(m_mouse)) {
		return CursorKind::hand;
	}

	if (field_at(main, m_mouse) >= 0) return CursorKind::ibeam;

	const Rect save = primary_button_rect(t_layout.footer);
	const bool over_button = cancel_button_rect(save).contains(m_mouse) || (can_save() && save.contains(m_mouse)) ||
							 (m_edited_row >= 0 && delete_button_rect(t_layout.footer).contains(m_mouse));

	return over_button ? CursorKind::hand : CursorKind::arrow;
}

CursorKind AccountModal::cursor() const
{
	if (!is_blocking()) return CursorKind::arrow;
	if (m_rows_scroll.is_dragging()) return CursorKind::drag;

	for (const TextInput &input : m_fields) {
		if (input.is_selecting()) return CursorKind::ibeam;
	}

	const Layout current = layout();
	const Rect close_badge{current.art_column.x + close_badge_margin, current.art_column.y + close_badge_margin,
						   close_badge_size, close_badge_size};

	if (m_mode != Mode::login_progress && close_badge.contains(m_mouse)) return CursorKind::hand;

	switch (m_mode) {
		case Mode::account_list:
			return has_game() ? list_cursor(current) : CursorKind::arrow;

		case Mode::login_progress:
			return primary_button_rect(current.footer).contains(m_mouse) ? CursorKind::hand : CursorKind::arrow;

		case Mode::edit_account:
			return edit_cursor(current);
	}

	return CursorKind::arrow;
}

void AccountModal::draw_chrome(DrawList &t_draw_list, const Layout &t_layout, u8 t_alpha) const
{
	const Game &game = m_library.game(static_cast<u32>(m_game));
	const Rect art = t_layout.art_column;

	controls::draw_panel_shadow(t_draw_list, t_layout.panel, panel_radius, m_open_amount);
	t_draw_list.add_bordered_rect(t_layout.panel, rounded(panel_radius), faded(color_panel, t_alpha),
								  faded(color_panel_border, t_alpha), panel_border);

	const float highlight_inset = scaled_radius(panel_radius);
	t_draw_list.add_rect(
		Rect{t_layout.inner.x + highlight_inset, t_layout.inner.y, t_layout.inner.w - highlight_inset * 2.0f, 1.0f},
		faded(Color{255, 255, 255, 22}, t_alpha));

	if (game.banner != nullptr) {
		t_draw_list.add_image(art, game.banner, faded(color_white, t_alpha),
							  rounded(panel_radius - panel_border, 0.0f, 0.0f, 0.0f),
							  cover_uv(art.w / art.h, game.banner->aspect()));
	} else {
		t_draw_list.add_rect(art, faded(game.accent, t_alpha));
	}

	t_draw_list.add_rect(Rect{art.right(), art.y, art_separator_width, art.h}, faded(color_art_separator, t_alpha));

	const Rect badge{art.x + close_badge_margin, art.y + close_badge_margin, close_badge_size, close_badge_size};
	const auto badge_alpha = static_cast<u8>(badge.contains(m_mouse) ? 210 : 170);

	t_draw_list.add_rounded_rect(badge, rounded(badge.w * 0.5f), faded(Color{20, 20, 22, badge_alpha}, t_alpha));
	controls::draw_icon(t_draw_list, badge.centered(close_badge_icon_size, close_badge_icon_size),
						m_assets.get(Asset::icon_arrow_back), faded(color_white, t_alpha));
}

void AccountModal::draw_section_title(DrawList &t_draw_list, Rect t_main, std::string_view t_title, u8 t_alpha) const
{
	const Font &font = m_fonts.body();
	const Rect header{t_main.x, t_main.y, t_main.w, header_height(m_fonts)};

	draw_text(t_draw_list, font, Vec2{t_main.x + row_padding, font.centered_baseline(header)}, t_title,
			  faded(color_text_bright, t_alpha));
	t_draw_list.add_rect(Rect{t_main.x + row_padding, header.bottom(), t_main.w - row_padding * 2.0f, 1.0f},
						 faded(color_separator, t_alpha));
}

void AccountModal::draw_account_row(DrawList &t_draw_list, Rect t_main, Rect t_row, const Account &t_account,
									bool t_selected, float t_delete_armed, u8 t_alpha) const
{
	const Rect highlight{t_row.x - 8.0f, t_row.y + 3.0f, t_row.w + 16.0f, t_row.h - 6.0f};

	if (t_selected) {
		t_draw_list.add_rounded_rect(highlight, rounded(10.0f), faded(color_row_selected, t_alpha));
		t_draw_list.add_rounded_rect(Rect{t_main.x + 8.0f, highlight.y, 3.0f, highlight.h}, rounded(1.5f),
									 faded(m_settings.accent, t_alpha));
	} else if (highlight.contains(m_mouse)) {
		t_draw_list.add_rounded_rect(highlight, rounded(10.0f), faded(color_row_hover, t_alpha));
	}

	const Font &body = m_fonts.body();
	const Font &secondary = m_fonts.secondary();
	const std::string_view note = t_account.note;

	const float block_height = body.line_height() + row_line_gap + secondary.line_height();
	const float block_y = t_row.y + (t_row.h - block_height) * 0.5f;
	const float username_baseline = note.empty() ? body.centered_baseline(t_row) : block_y + body.ascent();

	draw_text(t_draw_list, body, Vec2{t_row.x, username_baseline}, t_account.username,
			  faded(color_text_bright, t_alpha));

	if (!note.empty()) {
		const float note_baseline = block_y + body.line_height() + row_line_gap + secondary.ascent();
		draw_text(t_draw_list, secondary, Vec2{t_row.x, note_baseline}, note, faded(color_text_dim, t_alpha));
	}

	t_draw_list.add_rect(Rect{t_row.x, t_row.bottom() - 1.0f, t_row.w, 1.0f}, faded(color_separator, t_alpha));

	const Rect edit = edit_button_rect(t_row);
	const Rect remove = remove_button_rect(t_row);
	const Texture *edit_icon = m_assets.get(Asset::icon_edit);

	if (t_delete_armed > 0.01f) {
		const Rect pill = confirm_delete_rect(t_row);
		const auto armed_alpha = static_cast<u8>(t_alpha * t_delete_armed);
		const auto fading_alpha = static_cast<u8>(t_alpha * (1.0f - t_delete_armed));

		t_draw_list.add_rounded_rect(pill, rounded(pill.h * 0.5f), faded(color_error, armed_alpha));
		draw_text_centered(t_draw_list, secondary, pill, "Delete?", faded(foreground_on(color_error), armed_alpha));
		controls::draw_icon(t_draw_list, edit.inset(row_icon_inset), edit_icon, faded(color_text_dim, fading_alpha));
		return;
	}

	const bool edit_hovered = edit.contains(m_mouse);
	const bool remove_hovered = remove.contains(m_mouse);

	if (edit_hovered) {
		controls::draw_circular_hover(t_draw_list, edit, m_settings.accent, color_hover_badge, t_alpha);
	}

	if (remove_hovered) {
		controls::draw_circular_hover(t_draw_list, remove, color_error, color_remove_hover, t_alpha);
	}

	controls::draw_icon(t_draw_list, edit.inset(row_icon_inset), edit_icon,
						faded(edit_hovered ? color_text_bright : color_text_dim, t_alpha));
	controls::draw_x(t_draw_list, remove, faded(remove_hovered ? color_error : color_text_dim, t_alpha));
}

void AccountModal::draw_account_list(DrawList &t_draw_list, const Layout &t_layout, u8 t_alpha) const
{
	const Rect main = t_layout.main_column;
	draw_section_title(t_draw_list, main, "Accounts", t_alpha);

	const Rect add = add_button_rect(main);
	const bool add_hovered = add.contains(m_mouse);

	if (add_hovered) {
		controls::draw_circular_hover(t_draw_list, add, m_settings.accent, color_hover_badge, t_alpha);
	}

	controls::draw_icon(t_draw_list, add.centered(24.0f, 24.0f), m_assets.get(Asset::icon_add),
						faded(add_hovered ? color_text_bright : color_text_dim, t_alpha));

	const AccountRows rows = account_rows(t_layout);
	t_draw_list.push_clip(rows.region);

	for (u32 i = 0; i < rows.accounts.count; i += 1) {
		const Rect row = row_rect(t_layout, rows, i);
		if (!row.overlaps_vertically(rows.region)) continue;

		draw_account_row(t_draw_list, main, row, m_library.account(rows.accounts.refs[i]),
						 static_cast<i32>(i) == m_selected_row, m_row_delete.armed_amount(static_cast<i32>(i)),
						 t_alpha);
	}

	t_draw_list.pop_clip();

	m_rows_scroll.draw_edge_fade(t_draw_list, rows.region, rows.scroll, faded(color_panel, t_alpha));
	m_rows_scroll.draw(t_draw_list, rows.scroll, faded(color_scroll_thumb, t_alpha), m_mouse);
}

void AccountModal::draw_login_progress(DrawList &t_draw_list, Rect t_main, u8 t_alpha) const
{
	const LoginStage stage = m_login.stage();
	const bool finished = !has_queued_login() && LoginAttempt::is_terminal(stage);
	const Vec2 center{t_main.center().x, t_main.center().y - 24.0f};

	Color ring_color = m_settings.accent;
	if (finished && stage == LoginStage::success) {
		ring_color = color_success;
	} else if (finished && stage == LoginStage::error) {
		ring_color = color_error;
	}

	const float pulse = finished ? 1.0f : 0.55f + 0.45f * (0.5f + 0.5f * std::sin(m_login_seconds * 2.6f));
	const float glow_strength = 0.85f * pulse * (t_alpha / 255.0f);
	const float spin = std::fmod(m_login_seconds * ring_spin_degrees_per_second, 360.0f);
	const float start_degrees = finished ? 0.0f : spin - 90.0f - ring_sweep_degrees;
	const float sweep_degrees = finished ? 360.0f : ring_sweep_degrees;

	t_draw_list.add_circular_progress(center, ring_outer_radius, ring_inner_radius, ring_glow_margin, start_degrees,
									  sweep_degrees, glow_strength, faded(ring_color, t_alpha));

	if (finished) {
		const Color glyph = faded(color_text_bright, t_alpha);

		if (stage == LoginStage::success) {
			t_draw_list.add_line({center.x - 13.0f, center.y}, {center.x - 3.0f, center.y + 11.0f}, 4.0f, glyph);
			t_draw_list.add_line({center.x - 3.0f, center.y + 11.0f}, {center.x + 15.0f, center.y - 11.0f}, 4.0f,
								 glyph);
		} else {
			t_draw_list.add_line({center.x - 11.0f, center.y - 11.0f}, {center.x + 11.0f, center.y + 11.0f}, 4.0f,
								 glyph);
			t_draw_list.add_line({center.x - 11.0f, center.y + 11.0f}, {center.x + 11.0f, center.y - 11.0f}, 4.0f,
								 glyph);
		}
	}

	std::string_view message = has_queued_login() ? "Switching account..." : stage_message(stage);
	if (finished && !m_login.terminal_message().empty()) {
		message = m_login.terminal_message();
	}

	const Font &font = m_fonts.body();
	std::string_view lines[max_message_lines];
	const u32 line_count = wrap_text(font, message, std::max(t_main.w - row_padding * 2.0f, 40.0f), lines);

	float baseline = center.y + ring_outer_radius + 40.0f;
	for (const std::string_view line : std::span{lines, line_count}) {
		draw_text(t_draw_list, font, Vec2{center.x - text_width(font, line) * 0.5f, baseline}, line,
				  faded(color_text_bright, t_alpha));
		baseline += font.line_height();
	}
}

void AccountModal::draw_visibility_chip(DrawList &t_draw_list, Rect t_main, u8 t_alpha) const
{
	const Rect chip = visibility_chip_rect(t_main);
	const bool hovered = chip.contains(m_mouse);
	const Color content = faded(hovered ? color_text_bright : color_text_dim, t_alpha);

	controls::draw_field(t_draw_list, chip, field_radius, hovered ? color_field_border_focused : color_field_border,
						 hovered ? color_field_focused : color_field, t_alpha);

	const Rect icon{chip.x + chip_icon_inset, chip.y + (chip.h - chip_icon_size) * 0.5f, chip_icon_size,
					chip_icon_size};
	controls::draw_icon(t_draw_list, icon, m_assets.get(Asset::icon_list_arrow), content);

	char buffer[16];
	const Font &font = m_fonts.secondary();
	draw_text(t_draw_list, font, Vec2{icon.right() + chip_icon_gap, font.centered_baseline(chip)},
			  game_count_text(m_visible_games.mask(), buffer), content);
}

void AccountModal::draw_edit_form(DrawList &t_draw_list, Rect t_main, u8 t_alpha)
{
	draw_section_title(t_draw_list, t_main, m_edited_row < 0 ? "Add Account" : "Edit Account", t_alpha);

	const Font &label_font = m_fonts.secondary();
	const Color text = faded(color_text_bright, t_alpha);

	for (u32 i = 0; i < field_count; i += 1) {
		const Rect block = field_block_rect(t_main, i);
		const Rect input = field_input_rect(t_main, i);
		const bool focused = m_fields[i].is_focused();

		draw_text(t_draw_list, label_font, Vec2{block.x, block.y + label_font.ascent()}, field_specs[i].label,
				  faded(color_text_faint, t_alpha));
		controls::draw_field(t_draw_list, input, field_radius,
							 focused ? color_field_border_focused : color_field_border,
							 focused ? color_field_focused : color_field, t_alpha);
		m_fields[i].draw(t_draw_list, m_fonts.body(), field_text_rect(t_main, i), text, text);
	}

	const Rect reveal = reveal_button_rect(t_main);
	controls::draw_eye(t_draw_list, m_assets, reveal, !field(EditField::password).is_masked(),
					   faded(reveal.contains(m_mouse) ? color_text_bright : color_text_dim, t_alpha));

	draw_visibility_chip(t_draw_list, t_main, t_alpha);
	m_visible_games.draw(t_draw_list, m_mouse);
}

void AccountModal::draw_edit_footer(DrawList &t_draw_list, Rect t_footer, u8 t_alpha) const
{
	const Font &body = m_fonts.body();
	const Font &secondary = m_fonts.secondary();
	const bool editing = m_edited_row >= 0;
	const bool missing_username = field(EditField::username).value().empty();
	const bool missing_password = field(EditField::password).value().empty();

	std::string_view hint = editing ? "Edit the account's details" : "Fill in the new account's details";
	if (missing_username && missing_password) {
		hint = "Username and password are required";
	} else if (missing_username) {
		hint = "Username is required";
	} else if (missing_password) {
		hint = "Password is required";
	}

	const float hint_x = editing ? delete_button_rect(t_footer).right() + row_padding : t_footer.x + row_padding;
	draw_text(t_draw_list, secondary, Vec2{hint_x, secondary.centered_baseline(t_footer)}, hint,
			  faded(color_text_faint, t_alpha));

	const Rect save = primary_button_rect(t_footer);
	controls::draw_accent_button(t_draw_list, body, save, "Save", m_settings.accent, can_save(),
								 can_save() && save.contains(m_mouse), color_disabled_button, color_text_dim, t_alpha);

	const Rect cancel = cancel_button_rect(save);
	const controls::ButtonColors neutral{color_neutral_button, color_neutral_button_hover, color_text_bright,
										 color_text_bright};
	controls::draw_button(t_draw_list, body, cancel, "Cancel", neutral, cancel.contains(m_mouse), t_alpha);

	if (!editing) return;

	const Rect del = delete_button_rect(t_footer);
	const float armed = m_form_delete.armed_amount(0);
	const controls::ButtonColors delete_colors{
		mix(color_delete_button, color_error, armed),
		mix(lightened(color_error, 20), lightened(color_error, 40), armed),
		color_text_bright,
		mix(color_error, foreground_on(color_error), armed),
	};

	if (armed > 0.01f) {
		constexpr float ring_thickness = 1.5f;
		t_draw_list.add_rounded_rect(del.inset(-ring_thickness), rounded(8.0f + ring_thickness),
									 faded(lightened(color_error, 60), static_cast<u8>(t_alpha * armed)));
	}

	controls::draw_button(t_draw_list, body, del, armed > 0.5f ? "Delete?" : "Delete", delete_colors,
						  del.contains(m_mouse), t_alpha);
}

void AccountModal::draw_footer(DrawList &t_draw_list, Rect t_footer, u8 t_alpha) const
{
	const Font &body = m_fonts.body();
	const Font &secondary = m_fonts.secondary();
	const float hint_baseline = secondary.centered_baseline(t_footer);
	const Rect primary = primary_button_rect(t_footer);

	t_draw_list.add_rect(Rect{t_footer.x, t_footer.y, t_footer.w, 1.0f}, faded(color_separator, t_alpha));

	switch (m_mode) {
		case Mode::edit_account:
			draw_edit_footer(t_draw_list, t_footer, t_alpha);
			break;

		case Mode::login_progress: {
			const bool finished = !has_queued_login() && LoginAttempt::is_terminal(m_login.stage());
			const controls::ButtonColors neutral{color_neutral_button, color_neutral_button_hover, color_text_bright,
												 color_text_bright};

			draw_text(t_draw_list, secondary, Vec2{t_footer.x + row_padding, hint_baseline},
					  finished ? "" : "Logging in...", faded(color_text_faint, t_alpha));
			controls::draw_button(t_draw_list, body, primary, finished ? "Back" : "Cancel", neutral,
								  primary.contains(m_mouse), t_alpha);
			break;
		}

		case Mode::account_list: {
			const bool can_login = m_selected_row >= 0;

			draw_text(t_draw_list, secondary, Vec2{t_footer.x + row_padding, hint_baseline},
					  "Select an account to log in", faded(color_text_faint, t_alpha));
			controls::draw_accent_button(t_draw_list, body, primary, "Login", m_settings.accent, can_login,
										 can_login && primary.contains(m_mouse), color_disabled_button, color_text_dim,
										 t_alpha);
			break;
		}
	}
}

void AccountModal::draw(DrawList &t_draw_list)
{
	PULSAR_PROFILE_SCOPE("AccountModal.Draw");

	if (m_open_amount <= 0.001f || !has_game()) return;

	const auto alpha = static_cast<u8>(255.0f * m_open_amount);
	const Vec2 window = m_window.size();
	t_draw_list.add_rect(Rect{0.0f, 0.0f, window.x, window.y}, Color{0, 0, 0, static_cast<u8>(160.0f * m_open_amount)});

	const Layout current = layout();
	draw_chrome(t_draw_list, current, alpha);

	switch (m_mode) {
		case Mode::account_list:
			draw_account_list(t_draw_list, current, alpha);
			break;

		case Mode::login_progress:
			draw_login_progress(t_draw_list, current.main_column, alpha);
			break;

		case Mode::edit_account:
			draw_edit_form(t_draw_list, current.main_column, alpha);
			break;
	}

	draw_footer(t_draw_list, current.footer, alpha);
}
