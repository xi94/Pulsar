#include "ui/unlock_screen.h"

#include <Windows.h>

#include "core/master_key.h"
#include "core/settings.h"
#include "gfx/assets.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "platform/window.h"
#include "ui/controls.h"
#include "ui/text.h"

namespace {
constexpr float card_width = 380.0f;
constexpr float unlock_card_height = 210.0f;
constexpr float setup_card_height = 366.0f;
constexpr float card_radius = 16.0f;
constexpr float card_padding = 28.0f;
constexpr float field_height = 40.0f;
constexpr float field_radius = 6.0f;
constexpr float button_height = 40.0f;
constexpr float gap = 14.0f;
constexpr float reveal_size = 22.0f;
constexpr float reveal_margin = 6.0f;
constexpr float unlock_first_field_y = 74.0f;
constexpr float setup_first_field_y = 96.0f;

constexpr Color color_backdrop{12, 12, 14, 255};
constexpr Color color_card{26, 26, 30, 255};
constexpr Color color_field{24, 24, 27, 255};
constexpr Color color_field_border{40, 40, 45, 255};
constexpr Color color_text_bright{232, 232, 236, 255};
constexpr Color color_text_dim{150, 150, 156, 255};
constexpr Color color_error{220, 90, 80, 255};
}

UnlockScreen::UnlockScreen(Settings &t_settings, MasterKey &t_master_key, const Fonts &t_fonts, const Assets &t_assets,
						   const Window &t_window, CommandQueue &t_commands)
	: m_settings(t_settings)
	, m_master_key(t_master_key)
	, m_fonts(t_fonts)
	, m_assets(t_assets)
	, m_window(t_window)
	, m_commands(t_commands)
{
}

void UnlockScreen::reset_fields()
{
	for (TextInput &field : m_fields) {
		field.set_value("");
		field.set_masked(true);
	}

	focus_field(password);
}

void UnlockScreen::show_unlock()
{
	m_setup = false;
	m_wrong_password = false;
	reset_fields();
	m_active = true;
}

void UnlockScreen::show_setup()
{
	m_setup = true;
	m_passwords_differ = false;
	m_setup_failed = false;
	reset_fields();
	m_active = true;
}

void UnlockScreen::hide()
{
	m_active = false;
}

Rect UnlockScreen::card_rect() const
{
	const Vec2 window = m_window.size();

	return Rect{0.0f, 0.0f, window.x, window.y}.centered(card_width, m_setup ? setup_card_height : unlock_card_height);
}

Rect UnlockScreen::field_rect(u32 t_field) const
{
	const Rect card = card_rect();
	const float first_y = card.y + (m_setup ? setup_first_field_y : unlock_first_field_y);

	return Rect{card.x + card_padding, first_y + t_field * (field_height + gap), card.w - card_padding * 2.0f,
				field_height};
}

Rect UnlockScreen::field_text_rect(u32 t_field) const
{
	Rect field = field_rect(t_field);
	field.w -= reveal_size + reveal_margin;

	return field;
}

Rect UnlockScreen::reveal_rect(u32 t_field) const
{
	const Rect field = field_rect(t_field);

	return Rect{field.right() - reveal_size - reveal_margin, field.y + (field.h - reveal_size) * 0.5f, reveal_size,
				reveal_size};
}

Rect UnlockScreen::submit_rect() const
{
	const Rect last_field = field_rect(field_count() - 1);

	return Rect{last_field.x, last_field.bottom() + gap, last_field.w, button_height};
}

bool UnlockScreen::is_reveal_hit(Vec2 t_point) const
{
	for (u32 i = 0; i < field_count(); i += 1) {
		if (reveal_rect(i).contains(t_point)) return true;
	}

	return false;
}

i32 UnlockScreen::field_at(Vec2 t_point) const
{
	if (is_reveal_hit(t_point)) return -1;

	for (u32 i = 0; i < field_count(); i += 1) {
		if (field_rect(i).contains(t_point)) return static_cast<i32>(i);
	}

	return -1;
}

void UnlockScreen::focus_field(u32 t_field)
{
	for (u32 i = 0; i < 2; i += 1) {
		m_fields[i].set_focused(i == t_field);
	}
}

void UnlockScreen::submit()
{
	if (m_setup) {
		attempt_setup();
	} else {
		attempt_unlock();
	}
}

void UnlockScreen::attempt_unlock()
{
	TextInput &field = m_fields[password];
	const bool unlocked = m_master_key.unlock(field.value(), m_settings.master_key);

	field.set_value("");
	field.set_focused(!unlocked);
	m_wrong_password = !unlocked;

	if (unlocked) {
		m_commands.push(Command{.type = CommandType::vault_unlocked});
	}
}

void UnlockScreen::attempt_setup()
{
	const std::string_view chosen = m_fields[password].value();
	if (chosen.empty()) return;

	if (chosen != m_fields[confirmation].value()) {
		m_passwords_differ = true;
		m_setup_failed = false;
		m_fields[confirmation].set_value("");
		focus_field(confirmation);
		return;
	}

	if (!m_master_key.create(chosen, m_settings.master_key)) {
		m_setup_failed = true;
		m_passwords_differ = false;
		return;
	}

	m_settings.master_password_enabled = true;
	m_passwords_differ = false;
	m_setup_failed = false;

	for (TextInput &field : m_fields) {
		field.set_value("");
	}

	m_commands.push(Command{.type = CommandType::vault_created});
}

void UnlockScreen::update(float t_delta_seconds)
{
	if (!m_active) return;

	for (u32 i = 0; i < field_count(); i += 1) {
		m_fields[i].update(t_delta_seconds);
	}
}

bool UnlockScreen::on_char(u32 t_character)
{
	if (!m_active) return false;

	for (u32 i = 0; i < field_count(); i += 1) {
		m_fields[i].on_char(t_character);
	}

	return true;
}

bool UnlockScreen::on_key_down(u32 t_key)
{
	if (!m_active) return false;

	if (t_key == VK_RETURN) {
		submit();
	} else if (m_setup && t_key == VK_TAB) {
		focus_field(m_fields[password].is_focused() ? confirmation : password);
	} else {
		for (u32 i = 0; i < field_count(); i += 1) {
			m_fields[i].on_key_down(t_key);
		}
	}

	return true;
}

bool UnlockScreen::on_pointer_down(Vec2 t_point)
{
	if (!m_active) return false;

	const i32 pressed = field_at(t_point);
	if (pressed >= 0) {
		focus_field(static_cast<u32>(pressed));
		m_fields[pressed].on_pointer_down(m_fonts.body(), field_text_rect(static_cast<u32>(pressed)), t_point.x);
	}

	return true;
}

bool UnlockScreen::on_pointer_move(Vec2 t_point)
{
	if (!m_active) return false;

	for (u32 i = 0; i < field_count(); i += 1) {
		if (m_fields[i].is_selecting()) {
			m_fields[i].on_pointer_move(m_fonts.body(), field_text_rect(i), t_point.x);
		}
	}

	return true;
}

bool UnlockScreen::on_pointer_up(Vec2 t_point)
{
	if (!m_active) return false;

	bool ended_text_selection = false;
	for (TextInput &field : m_fields) {
		ended_text_selection = ended_text_selection || field.is_selecting();
		field.on_pointer_up();
	}

	if (ended_text_selection) return true;

	if (is_reveal_hit(t_point)) {
		const bool reveal = m_fields[password].is_masked();

		for (TextInput &field : m_fields) {
			field.set_masked(!reveal);
		}
	} else if (submit_rect().contains(t_point)) {
		submit();
	}

	return true;
}

bool UnlockScreen::on_right_click(Vec2 t_point)
{
	if (!m_active) return false;

	const i32 clicked = field_at(t_point);
	if (clicked >= 0) {
		const auto field = static_cast<u32>(clicked);

		focus_field(field);
		m_fields[field].on_right_click(m_fonts.body(), field_text_rect(field), t_point.x);
		m_commands.push(
			Command{.type = CommandType::show_text_menu, .position = t_point, .text_input = &m_fields[field]});
	}

	return true;
}

CursorKind UnlockScreen::cursor() const
{
	if (!m_active) return CursorKind::arrow;

	for (const TextInput &field : m_fields) {
		if (field.is_selecting()) return CursorKind::ibeam;
	}

	if (is_reveal_hit(m_mouse) || submit_rect().contains(m_mouse)) return CursorKind::hand;

	return field_at(m_mouse) >= 0 ? CursorKind::ibeam : CursorKind::arrow;
}

void UnlockScreen::draw_field(DrawList &t_draw_list, u32 t_field)
{
	const Color accent = m_settings.accent;
	TextInput &field = m_fields[t_field];
	const Rect reveal = reveal_rect(t_field);

	controls::draw_field(t_draw_list, field_rect(t_field), field_radius,
						 field.is_focused() ? accent : color_field_border, color_field, 255);
	field.draw(t_draw_list, m_fonts.body(), field_text_rect(t_field), color_text_bright, accent);
	controls::draw_eye(t_draw_list, m_assets, reveal, !field.is_masked(),
					   reveal.contains(m_mouse) ? color_text_bright : color_text_dim);
}

void UnlockScreen::draw_submit_button(DrawList &t_draw_list, std::string_view t_label) const
{
	const Color accent = m_settings.accent;
	const Rect button = submit_rect();

	t_draw_list.add_bordered_rect(button, rounded(8.0f), accent, outline_on(accent), 1.0f);
	draw_text_centered(t_draw_list, m_fonts.body(), button, t_label, foreground_on(accent));
}

void UnlockScreen::draw(DrawList &t_draw_list)
{
	if (!m_active) return;

	const Vec2 window = m_window.size();
	const Font &body = m_fonts.body();
	const Font &secondary = m_fonts.secondary();

	t_draw_list.add_rect(Rect{0.0f, 0.0f, window.x, window.y - status_bar_height}, color_backdrop);

	const Rect card = card_rect();
	t_draw_list.add_rounded_rect(card, rounded(card_radius), color_card);

	const float text_x = card.x + card_padding;
	const float title_baseline = card.y + card_padding + body.ascent();
	const float description_baseline = card.y + card_padding + body.line_height() + 4.0f + secondary.ascent();
	const float error_baseline = submit_rect().bottom() + gap + secondary.ascent();

	if (m_setup) {
		draw_text(t_draw_list, body, Vec2{text_x, title_baseline}, "Create a Master Password", color_text_bright);
		draw_text(t_draw_list, secondary, Vec2{text_x, description_baseline},
				  "This encrypts your saved account passwords. Choose", color_text_dim);
		draw_text(t_draw_list, secondary, Vec2{text_x, description_baseline + secondary.line_height()},
				  "something memorable - it can't be recovered if lost.", color_text_dim);
	} else {
		draw_text(t_draw_list, body, Vec2{text_x, title_baseline}, "Master Password", color_text_bright);
		draw_text(t_draw_list, secondary, Vec2{text_x, description_baseline}, "Enter your master password to continue.",
				  color_text_dim);
	}

	for (u32 i = 0; i < field_count(); i += 1) {
		draw_field(t_draw_list, i);
	}

	draw_submit_button(t_draw_list, m_setup ? "Create Password" : "Unlock");

	std::string_view error;
	if (!m_setup && m_wrong_password) {
		error = "Incorrect password.";
	} else if (m_setup && m_passwords_differ) {
		error = "Passwords don't match.";
	} else if (m_setup && m_setup_failed) {
		error = "Something went wrong - try again.";
	}

	draw_text(t_draw_list, secondary, Vec2{text_x, error_baseline}, error, color_error);
}
