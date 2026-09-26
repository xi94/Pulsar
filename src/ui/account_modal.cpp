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
#include "ui/theme.h"
#include "ui/toasts.h"

namespace {
constexpr float open_ease_rate = 14.0f;

constexpr float panel_width_fraction = 0.78f;
constexpr float panel_height_fraction = 0.71f;
constexpr Vec2 panel_max_size{810.0f, 480.0f};
constexpr Vec2 panel_min_size{560.0f, 380.0f};
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

constexpr float empty_icon_size = 52.0f;
constexpr float empty_icon_radius = 12.0f;
constexpr float empty_gap = 16.0f;
constexpr float empty_line_gap = 4.0f;
constexpr float empty_button_width = 140.0f;

constexpr float field_label_gap = 8.0f;
constexpr float field_group_gap = 26.0f;
constexpr float form_top_padding = 12.0f;
constexpr u32 form_block_count = static_cast<u32>(EditField::count) + 1;
constexpr float field_radius = 8.0f;
constexpr float reveal_button_size = 24.0f;
constexpr float reveal_button_margin = 6.0f;

constexpr float ring_outer_radius = 40.0f;
constexpr float ring_inner_radius = 32.0f;
constexpr float ring_glow_margin = 22.0f;
constexpr float ring_sweep_degrees = 112.0f;
constexpr float ring_spin_degrees_per_second = 260.0f;
constexpr u32 max_message_lines = 3;

constexpr Color color_on_art{255, 255, 255, 255};
constexpr Color color_art_badge{20, 20, 22, 255};
constexpr Color color_top_highlight{255, 255, 255, 22};
constexpr float danger_tint = 0.22f;

struct FieldSpec {
	const char *label;
	u32 max_length;
};

constexpr FieldSpec field_specs[]{
	{"Note", sizeof(Account::note) - 1},
	{"Username", sizeof(Account::username) - 1},
	{"Password", sizeof(Account::password) - 1},
};

float panel_size_scale(const Fonts &t_fonts)
{
	return std::max(1.0f, t_fonts.body().pixel_height() / reference_body_pixel_height);
}

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
	, m_visible_games(t_library, t_settings, t_fonts)
{
	for (u32 i = 0; i < field_count; i += 1) {
		m_fields[i].set_max_length(field_specs[i].max_length);
	}
}

bool AccountModal::has_game() const
{
	return m_game >= 0 && static_cast<u32>(m_game) < m_library.game_count();
}

Vec2 AccountModal::floating_panel_size() const
{
	const float size_scale = panel_size_scale(m_fonts);
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

	return Vec2{width, height};
}

bool AccountModal::is_docked() const
{
	const float size_scale = panel_size_scale(m_fonts);
	const Vec2 floating = floating_panel_size();

	return floating.x < panel_min_size.x * size_scale || floating.y < panel_min_size.y * size_scale;
}

Rect AccountModal::panel_rect() const
{
	if (is_docked()) return m_window.content_rect().inset(0.0f, 1.0f);

	const Vec2 size = floating_panel_size();
	const Vec2 window = m_window.size();
	const float scale = panel_closed_scale + (1.0f - panel_closed_scale) * m_open_amount;

	return Rect{0.0f, 0.0f, window.x, window.y}.centered(size.x * scale, size.y * scale);
}

AccountModal::Layout AccountModal::layout() const
{
	Layout result{};
	result.panel = panel_rect();
	result.inner = result.panel.inset(panel_border);

	Rect content = result.inner;
	result.footer = content.split_bottom(footer_height(m_fonts));
	const float art_width = std::max(content.w * art_column_fraction, close_badge_size + close_badge_margin * 2.0f);
	result.art_column = Rect{content.x, content.y, art_width, content.h};

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

Rect AccountModal::form_region(Rect t_main) const
{
	const float header = header_height(m_fonts);

	return Rect{t_main.x, t_main.y + header, t_main.w, std::max(0.0f, t_main.h - header)};
}

ScrollGeometry AccountModal::form_scroll(Rect t_main) const
{
	const Rect region = form_region(t_main);
	const Rect track{region.right() - scrollbar_width - scrollbar_margin, region.y, scrollbar_width, region.h};

	return ScrollGeometry{track, form_top_padding + field_block_height(m_fonts) * form_block_count, region.h};
}

Rect AccountModal::field_block_rect(Rect t_main, u32 t_field) const
{
	const Rect region = form_region(t_main);
	const float block_height = field_block_height(m_fonts);
	const float content_height = form_top_padding + block_height * form_block_count;
	const float slack = std::max(0.0f, (region.h - content_height) * 0.28f);
	const float top = region.y + form_top_padding + slack - m_form_scroll.offset();

	return Rect{t_main.x + row_padding, top + t_field * block_height, t_main.w - row_padding * 2.0f, block_height};
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

Rect AccountModal::show_in_rect(Rect t_main) const
{
	return field_input_rect(t_main, form_block_count - 1);
}

bool AccountModal::is_show_in_hit(Rect t_main, Vec2 t_point) const
{
	return form_region(t_main).contains(t_point) && show_in_rect(t_main).contains(t_point);
}

std::string_view AccountModal::visibility_summary(char (&t_buffer)[32]) const
{
	const u16 mask = m_visible_games.mask();
	const auto count = static_cast<u32>(std::popcount(mask));

	if (count == m_library.game_count()) return "All games";
	if (count == 1) return m_library.game(static_cast<u32>(std::countr_zero(mask))).title;

	const int written = std::snprintf(t_buffer, sizeof(t_buffer), "%u games", count);

	return std::string_view{t_buffer, static_cast<usize>(std::max(written, 0))};
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
	if (m_visible_games.is_open() || !form_region(t_main).contains(t_point) || is_reveal_hit(t_main, t_point)) {
		return -1;
	}

	for (u32 i = 0; i < field_count; i += 1) {
		if (field_input_rect(t_main, i).contains(t_point)) return static_cast<i32>(i);
	}

	return -1;
}

bool AccountModal::is_reveal_hit(Rect t_main, Vec2 t_point) const
{
	return form_region(t_main).contains(t_point) && reveal_button_rect(t_main).contains(t_point);
}

void AccountModal::reveal_field(i32 t_field)
{
	if (t_field < 0) return;

	const Rect main = layout().main_column;
	const Rect region = form_region(main);
	const Rect block = field_block_rect(main, static_cast<u32>(t_field));

	m_form_scroll.reveal(block.y, block.bottom() - field_group_gap, region.y, region.bottom(), form_scroll(main));
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
	m_form_scroll = Scrollable{};
	m_edited_row = -1;

	for (TextInput &input : m_fields) {
		input.set_value("");
	}

	focus_field(static_cast<i32>(EditField::note));
	field(EditField::password).set_masked(true);

	m_visible_games.set_mask(static_cast<u16>(1u << m_game));
	m_visible_games.close();
}

void AccountModal::start_editing(u32 t_row)
{
	m_row_delete.disarm();
	m_form_delete.disarm();
	m_mode = Mode::edit_account;
	m_form_scroll = Scrollable{};
	m_edited_row = static_cast<i32>(t_row);

	u16 mask = 0;
	if (const auto ref = m_library.visible_account(static_cast<u32>(m_game), t_row)) {
		const Account &account = m_library.account(*ref);
		field(EditField::note).set_value(account.note);
		field(EditField::username).set_value(account.username);
		field(EditField::password).set_value(account.password);
		mask = account.visible_games(ref->game);
	}

	focus_field(static_cast<i32>(EditField::note));
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

Rect AccountModal::back_badge_rect(const Layout &t_layout) const
{
	const Rect &art = t_layout.art_column;

	return Rect{art.x + close_badge_margin, art.y + close_badge_margin, close_badge_size, close_badge_size};
}

void AccountModal::request_tooltip()
{
	if (!is_blocking() || !has_game() || m_visible_games.is_open()) return;

	const Layout current = layout();
	const Rect back = back_badge_rect(current);

	if (m_mode != Mode::login_progress && back.contains(m_mouse)) {
		m_tooltip.request("Back", back);
		return;
	}

	if (m_mode == Mode::edit_account) {
		const Rect main = current.main_column;

		if (is_reveal_hit(main, m_mouse)) {
			m_tooltip.request(field(EditField::password).is_masked() ? "Show password" : "Hide password",
							  reveal_button_rect(main));
		}

		return;
	}

	if (m_mode != Mode::account_list) return;

	const Rect add = add_button_rect(current.main_column);
	if (add.contains(m_mouse)) {
		m_tooltip.request("Add account", add);
		return;
	}

	const AccountRows rows = account_rows(current);
	const i32 hovered_row = row_at(current, rows, m_mouse);
	if (hovered_row < 0 || m_row_delete.is_armed(hovered_row)) return;

	const Rect row = row_rect(current, rows, static_cast<u32>(hovered_row));
	const Rect edit = edit_button_rect(row);
	const Rect remove = remove_button_rect(row);

	if (edit.contains(m_mouse)) {
		m_tooltip.request("Edit account", edit);
	} else if (remove.contains(m_mouse)) {
		m_tooltip.request("Delete account", remove);
	}
}

void AccountModal::update(float t_delta_seconds)
{
	const float scale_travel = m_window.size().x * 0.5f * (1.0f - panel_closed_scale);
	m_open_amount = animation::ease_toward(m_open_amount, m_open ? 1.0f : 0.0f, open_ease_rate, t_delta_seconds,
										   animation::settled_pixels / scale_travel);

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
			m_form_scroll.update(t_delta_seconds);

			for (TextInput &input : m_fields) {
				input.update(t_delta_seconds);
			}

			break;
	}

	m_visible_games.update(t_delta_seconds);
	request_tooltip();
	m_tooltip.update(t_delta_seconds);
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
		if (!m_visible_games.is_open() && m_form_scroll.on_pointer_down(t_point, form_scroll(current.main_column))) {
			return true;
		}

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

	if (m_form_scroll.is_dragging()) {
		m_form_scroll.on_pointer_move(t_point.y, form_scroll(current.main_column));
	}

	return true;
}

bool AccountModal::on_pointer_up(Vec2 t_point)
{
	if (m_rows_scroll.is_dragging()) {
		m_rows_scroll.on_pointer_up();
		return true;
	}

	if (m_form_scroll.is_dragging()) {
		m_form_scroll.on_pointer_up();
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
	const bool clicked_close = back_badge_rect(current).contains(t_point);

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

	if (rows.accounts.count == 0 && empty_state(rows.region).button.contains(t_point)) {
		start_adding();
		return;
	}

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

	if (is_show_in_hit(main, t_point)) {
		m_visible_games.open(show_in_rect(main), main);
		return;
	}

	if (is_reveal_hit(main, t_point)) {
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
	} else if (m_mode == Mode::edit_account && !m_visible_games.is_open()) {
		m_form_scroll.on_scroll(t_wheel_delta, form_scroll(layout().main_column));
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
		const auto count = static_cast<i32>(field_count);
		const i32 step = (GetKeyState(VK_SHIFT) & 0x8000) != 0 ? count - 1 : 1;
		const i32 current = focused_field();
		focus_field(current < 0 ? 0 : (current + step) % count);
		reveal_field(focused_field());
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
	if (rows.accounts.count == 0 && empty_state(rows.region).button.contains(m_mouse)) return CursorKind::hand;
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
	if (is_show_in_hit(main, m_mouse) || is_reveal_hit(main, m_mouse)) {
		return CursorKind::hand;
	}

	if (field_at(main, m_mouse) >= 0) return CursorKind::ibeam;
	if (m_form_scroll.is_over_track(m_mouse, form_scroll(main))) return CursorKind::hand;

	const Rect save = primary_button_rect(t_layout.footer);
	const bool over_button = cancel_button_rect(save).contains(m_mouse) || (can_save() && save.contains(m_mouse)) ||
							 (m_edited_row >= 0 && delete_button_rect(t_layout.footer).contains(m_mouse));

	return over_button ? CursorKind::hand : CursorKind::arrow;
}

CursorKind AccountModal::cursor() const
{
	if (!is_blocking()) return CursorKind::arrow;
	if (m_rows_scroll.is_dragging() || m_form_scroll.is_dragging()) return CursorKind::drag;

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
	const bool docked = is_docked();
	const float radius = docked ? 0.0f : panel_radius;

	if (docked) {
		t_draw_list.add_rect(t_layout.panel, faded(theme().surface, t_alpha));
	} else {
		controls::draw_panel_shadow(t_draw_list, t_layout.panel, panel_radius, m_open_amount);
		t_draw_list.add_bordered_rect(t_layout.panel, rounded(panel_radius), faded(theme().surface, t_alpha),
									  faded(theme().border, t_alpha), panel_border);

		const float highlight_inset = scaled_radius(panel_radius);
		t_draw_list.add_rect(
			Rect{t_layout.inner.x + highlight_inset, t_layout.inner.y, t_layout.inner.w - highlight_inset * 2.0f, 1.0f},
			faded(color_top_highlight, t_alpha));
	}

	if (game.banner != nullptr) {
		t_draw_list.add_image(art, game.banner, faded(color_on_art, t_alpha),
							  rounded(std::max(0.0f, radius - panel_border), 0.0f, 0.0f, 0.0f),
							  cover_uv(art.w / art.h, game.banner->aspect()));
	} else {
		t_draw_list.add_rect(art, faded(game.accent, t_alpha));
	}

	t_draw_list.add_rect(Rect{art.right(), art.y, art_separator_width, art.h}, faded(theme().border, t_alpha));

	const Rect badge = back_badge_rect(t_layout);
	const auto badge_alpha = static_cast<u8>(badge.contains(m_mouse) ? 210 : 170);

	t_draw_list.add_rounded_rect(badge, rounded(badge.w * 0.5f),
								 faded(with_alpha(color_art_badge, badge_alpha), t_alpha));
	controls::draw_icon(t_draw_list, badge.centered(close_badge_icon_size, close_badge_icon_size),
						m_assets.get(Asset::icon_arrow_back), faded(color_on_art, t_alpha));
}

void AccountModal::draw_section_title(DrawList &t_draw_list, Rect t_main, std::string_view t_title, u8 t_alpha) const
{
	const Font &font = m_fonts.body();
	const Rect header{t_main.x, t_main.y, t_main.w, header_height(m_fonts)};

	draw_text(t_draw_list, font, Vec2{t_main.x + row_padding, font.centered_baseline(header)}, t_title,
			  faded(theme().text, t_alpha));
	t_draw_list.add_rect(Rect{t_main.x + row_padding, header.bottom(), t_main.w - row_padding * 2.0f, 1.0f},
						 faded(theme().separator, t_alpha));
}

void AccountModal::draw_account_row(DrawList &t_draw_list, Rect t_main, Rect t_row, const Account &t_account,
									bool t_selected, float t_delete_armed, u8 t_alpha) const
{
	const Rect highlight{t_row.x - 8.0f, t_row.y + 3.0f, t_row.w + 16.0f, t_row.h - 6.0f};

	if (t_selected) {
		t_draw_list.add_rounded_rect(highlight, rounded(10.0f), faded(theme().row_selected, t_alpha));
		t_draw_list.add_rounded_rect(Rect{t_main.x + 8.0f, highlight.y, 3.0f, highlight.h}, rounded(1.5f),
									 faded(m_settings.accent, t_alpha));
	} else if (highlight.contains(m_mouse)) {
		t_draw_list.add_rounded_rect(highlight, rounded(10.0f), faded(theme().row_hover, t_alpha));
	}

	const Font &body = m_fonts.body();
	const Font &secondary = m_fonts.secondary();
	const std::string_view note = t_account.note;

	const float block_height = body.line_height() + row_line_gap + secondary.line_height();
	const float block_y = t_row.y + (t_row.h - block_height) * 0.5f;
	const float username_baseline = note.empty() ? body.centered_baseline(t_row) : block_y + body.ascent();

	draw_text(t_draw_list, body, Vec2{t_row.x, username_baseline}, t_account.username, faded(theme().text, t_alpha));

	if (!note.empty()) {
		const float note_baseline = block_y + body.line_height() + row_line_gap + secondary.ascent();
		draw_text(t_draw_list, secondary, Vec2{t_row.x, note_baseline}, note, faded(theme().text_dim, t_alpha));
	}

	t_draw_list.add_rect(Rect{t_row.x, t_row.bottom() - 1.0f, t_row.w, 1.0f}, faded(theme().separator, t_alpha));

	const Rect edit = edit_button_rect(t_row);
	const Rect remove = remove_button_rect(t_row);
	const Texture *edit_icon = m_assets.get(Asset::icon_edit);

	if (t_delete_armed > 0.01f) {
		const Rect pill = confirm_delete_rect(t_row);
		const auto armed_alpha = static_cast<u8>(t_alpha * t_delete_armed);
		const auto fading_alpha = static_cast<u8>(t_alpha * (1.0f - t_delete_armed));

		t_draw_list.add_rounded_rect(pill, rounded(pill.h * 0.5f), faded(theme().error, armed_alpha));
		draw_text_centered(t_draw_list, secondary, pill, "Delete?", faded(foreground_on(theme().error), armed_alpha));
		controls::draw_icon(t_draw_list, edit.inset(row_icon_inset), edit_icon, faded(theme().text_dim, fading_alpha));
		return;
	}

	const bool edit_hovered = edit.contains(m_mouse);
	const bool remove_hovered = remove.contains(m_mouse);

	if (edit_hovered) {
		controls::draw_circular_hover(t_draw_list, edit, m_settings.accent, theme().control_hover, t_alpha);
	}

	if (remove_hovered) {
		controls::draw_circular_hover(t_draw_list, remove, theme().error,
									  mix(theme().surface, theme().error, danger_tint), t_alpha);
	}

	controls::draw_icon(t_draw_list, edit.inset(row_icon_inset), edit_icon,
						faded(edit_hovered ? theme().text : theme().text_dim, t_alpha));
	controls::draw_x(t_draw_list, remove, faded(remove_hovered ? theme().error : theme().text_dim, t_alpha));
}

AccountModal::EmptyState AccountModal::empty_state(Rect t_region) const
{
	const Font &body = m_fonts.body();
	const Font &secondary = m_fonts.secondary();
	const float button_height = action_button_height(m_fonts);
	const float stack_height = empty_icon_size + empty_gap + body.line_height() + empty_line_gap +
							   secondary.line_height() + empty_gap + button_height;
	const float center_x = t_region.center().x;
	const float top = t_region.center().y - stack_height * 0.5f;

	EmptyState state{};
	state.icon = Rect{center_x - empty_icon_size * 0.5f, top, empty_icon_size, empty_icon_size};
	state.title_baseline = state.icon.bottom() + empty_gap + body.ascent();
	state.hint_baseline = state.icon.bottom() + empty_gap + body.line_height() + empty_line_gap + secondary.ascent();
	state.button = Rect{center_x - empty_button_width * 0.5f,
						state.icon.bottom() + empty_gap + body.line_height() + empty_line_gap +
							secondary.line_height() + empty_gap,
						empty_button_width, button_height};

	return state;
}

void AccountModal::draw_empty_state(DrawList &t_draw_list, Rect t_region, u8 t_alpha) const
{
	const Game &game = m_library.game(static_cast<u32>(m_game));
	const EmptyState state = empty_state(t_region);
	const Font &body = m_fonts.body();
	const Font &secondary = m_fonts.secondary();
	constexpr std::string_view title = "No accounts yet";
	constexpr std::string_view hint = "Add one to log in with a single click.";

	if (game.icon != nullptr) {
		t_draw_list.add_image(state.icon, game.icon, faded(color_on_art, t_alpha), rounded(empty_icon_radius));
	} else {
		t_draw_list.add_rounded_rect(state.icon, rounded(empty_icon_radius), faded(game.accent, t_alpha));
	}

	draw_text(t_draw_list, body, Vec2{t_region.center().x - text_width(body, title) * 0.5f, state.title_baseline},
			  title, faded(theme().text, t_alpha));
	draw_text(t_draw_list, secondary,
			  Vec2{t_region.center().x - text_width(secondary, hint) * 0.5f, state.hint_baseline}, hint,
			  faded(theme().text_dim, t_alpha));
	controls::draw_button(t_draw_list, body, state.button, "Add Account", controls::ButtonStyle::accent,
						  m_settings.accent, true, state.button.contains(m_mouse), t_alpha);
}

void AccountModal::draw_account_list(DrawList &t_draw_list, const Layout &t_layout, u8 t_alpha) const
{
	const Rect main = t_layout.main_column;
	draw_section_title(t_draw_list, main, "Accounts", t_alpha);

	const Rect add = add_button_rect(main);
	const bool add_hovered = add.contains(m_mouse);

	if (add_hovered) {
		controls::draw_circular_hover(t_draw_list, add, m_settings.accent, theme().control_hover, t_alpha);
	}

	controls::draw_icon(t_draw_list, add.centered(24.0f, 24.0f), m_assets.get(Asset::icon_add),
						faded(add_hovered ? theme().text : theme().text_dim, t_alpha));

	const AccountRows rows = account_rows(t_layout);

	if (rows.accounts.count == 0) {
		draw_empty_state(t_draw_list, rows.region, t_alpha);
		return;
	}

	t_draw_list.push_clip(rows.region);

	for (u32 i = 0; i < rows.accounts.count; i += 1) {
		const Rect row = row_rect(t_layout, rows, i);
		if (!row.overlaps_vertically(rows.region)) continue;

		draw_account_row(t_draw_list, main, row, m_library.account(rows.accounts.refs[i]),
						 static_cast<i32>(i) == m_selected_row, m_row_delete.armed_amount(static_cast<i32>(i)),
						 t_alpha);
	}

	t_draw_list.pop_clip();

	m_rows_scroll.draw_edge_fade(t_draw_list, rows.region, rows.scroll, faded(theme().surface, t_alpha));
	m_rows_scroll.draw(t_draw_list, rows.scroll, m_mouse, t_alpha);
}

void AccountModal::draw_login_progress(DrawList &t_draw_list, Rect t_main, u8 t_alpha) const
{
	const LoginStage stage = m_login.stage();
	const bool finished = !has_queued_login() && LoginAttempt::is_terminal(stage);
	const Vec2 center{t_main.center().x, t_main.center().y - 24.0f};

	Color ring_color = m_settings.accent;
	if (finished && stage == LoginStage::success) {
		ring_color = theme().success;
	} else if (finished && stage == LoginStage::error) {
		ring_color = theme().error;
	}

	const float pulse = finished ? 1.0f : 0.55f + 0.45f * (0.5f + 0.5f * std::sin(m_login_seconds * 2.6f));
	const float glow_strength = 0.85f * pulse * (t_alpha / 255.0f);
	const float spin = std::fmod(m_login_seconds * ring_spin_degrees_per_second, 360.0f);
	const float start_degrees = finished ? 0.0f : spin - 90.0f - ring_sweep_degrees;
	const float sweep_degrees = finished ? 360.0f : ring_sweep_degrees;

	t_draw_list.add_circular_progress(center, ring_outer_radius, ring_inner_radius, ring_glow_margin, start_degrees,
									  sweep_degrees, glow_strength, faded(ring_color, t_alpha), theme().control);

	if (finished) {
		const Color glyph = faded(theme().text, t_alpha);

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
				  faded(theme().text, t_alpha));
		baseline += font.line_height();
	}
}

void AccountModal::draw_edit_form(DrawList &t_draw_list, Rect t_main, u8 t_alpha)
{
	draw_section_title(t_draw_list, t_main, m_edited_row < 0 ? "Add Account" : "Edit Account", t_alpha);

	const Font &label_font = m_fonts.secondary();
	const Color text = faded(theme().text, t_alpha);
	const Rect region = form_region(t_main);
	const ScrollGeometry scroll = form_scroll(t_main);

	t_draw_list.push_clip(region);

	for (u32 i = 0; i < field_count; i += 1) {
		const Rect block = field_block_rect(t_main, i);
		const Rect input = field_input_rect(t_main, i);
		const bool focused = m_fields[i].is_focused();

		draw_text(t_draw_list, label_font, Vec2{block.x, block.y + label_font.ascent()}, field_specs[i].label,
				  faded(theme().text_faint, t_alpha));
		controls::draw_field(t_draw_list, input, field_radius, focused ? theme().text_dim : theme().control,
							 focused ? theme().row_hover : theme().field, t_alpha);
		m_fields[i].draw(t_draw_list, m_fonts.body(), field_text_rect(t_main, i), text, text);
	}

	const Rect reveal = reveal_button_rect(t_main);
	controls::draw_eye(t_draw_list, m_assets, reveal, !field(EditField::password).is_masked(),
					   faded(is_reveal_hit(t_main, m_mouse) ? theme().text : theme().text_dim, t_alpha));

	char summary[32];
	const Rect show_in_block = field_block_rect(t_main, form_block_count - 1);
	draw_text(t_draw_list, label_font, Vec2{show_in_block.x, show_in_block.y + label_font.ascent()}, "Show in",
			  faded(theme().text_faint, t_alpha));
	controls::draw_dropdown(t_draw_list, m_fonts.body(), show_in_rect(t_main), visibility_summary(summary),
							m_visible_games.is_open(), is_show_in_hit(t_main, m_mouse), m_settings.accent, t_alpha);

	t_draw_list.pop_clip();

	m_form_scroll.draw_edge_fade(t_draw_list, region, scroll, faded(theme().surface, t_alpha));
	m_form_scroll.draw(t_draw_list, scroll, m_mouse, t_alpha);

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

	const Rect save = primary_button_rect(t_footer);
	const float hint_x = editing ? delete_button_rect(t_footer).right() + row_padding : t_footer.x + row_padding;
	const float hint_width = cancel_button_rect(save).x - row_padding - hint_x;

	draw_text_truncated(t_draw_list, secondary, Vec2{hint_x, secondary.centered_baseline(t_footer)}, hint, hint_width,
						faded(theme().text_faint, t_alpha));

	const Rect cancel = cancel_button_rect(save);
	controls::draw_button(t_draw_list, body, save, "Save", controls::ButtonStyle::accent, m_settings.accent, can_save(),
						  save.contains(m_mouse), t_alpha);
	controls::draw_button(t_draw_list, body, cancel, "Cancel", controls::ButtonStyle::neutral, m_settings.accent, true,
						  cancel.contains(m_mouse), t_alpha);

	if (!editing) return;

	const Rect del = delete_button_rect(t_footer);
	const bool armed = m_form_delete.armed_amount(0) > 0.5f;
	const controls::ButtonStyle delete_style =
		armed ? controls::ButtonStyle::danger_confirm : controls::ButtonStyle::danger;

	controls::draw_button(t_draw_list, body, del, armed ? "Delete?" : "Delete", delete_style, m_settings.accent, true,
						  del.contains(m_mouse), t_alpha);
}

void AccountModal::draw_footer(DrawList &t_draw_list, Rect t_footer, u8 t_alpha) const
{
	const Font &body = m_fonts.body();
	const Font &secondary = m_fonts.secondary();
	const float hint_baseline = secondary.centered_baseline(t_footer);
	const Rect primary = primary_button_rect(t_footer);
	const float hint_width = primary.x - row_padding * 2.0f - t_footer.x;

	t_draw_list.add_rect(Rect{t_footer.x, t_footer.y, t_footer.w, 1.0f}, faded(theme().separator, t_alpha));

	switch (m_mode) {
		case Mode::edit_account:
			draw_edit_footer(t_draw_list, t_footer, t_alpha);
			break;

		case Mode::login_progress: {
			const bool finished = !has_queued_login() && LoginAttempt::is_terminal(m_login.stage());

			draw_text_truncated(t_draw_list, secondary, Vec2{t_footer.x + row_padding, hint_baseline},
								finished ? "" : "Logging in...", hint_width, faded(theme().text_faint, t_alpha));
			controls::draw_button(t_draw_list, body, primary, finished ? "Back" : "Cancel",
								  controls::ButtonStyle::neutral, m_settings.accent, true, primary.contains(m_mouse),
								  t_alpha);
			break;
		}

		case Mode::account_list: {
			const bool can_login = m_selected_row >= 0;

			draw_text_truncated(t_draw_list, secondary, Vec2{t_footer.x + row_padding, hint_baseline},
								"Select an account to log in", hint_width, faded(theme().text_faint, t_alpha));
			controls::draw_button(t_draw_list, body, primary, "Login", controls::ButtonStyle::accent, m_settings.accent,
								  can_login, primary.contains(m_mouse), t_alpha);
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
	if (!is_docked()) {
		t_draw_list.add_rect(Rect{0.0f, 0.0f, window.x, window.y},
							 faded(theme().scrim, static_cast<u8>(255.0f * m_open_amount)));
	}

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
	m_tooltip.draw(t_draw_list, m_fonts, current.panel, alpha);
}
