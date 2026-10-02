#include "ui/unlock_screen.h"

#include <Windows.h>

#include "core/crypto.h"
#include "core/settings.h"
#include "gfx/assets.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "platform/window.h"
#include "ui/controls.h"
#include "ui/text.h"
#include "ui/theme.h"

namespace {
constexpr float K_CARD_WIDTH           = 380.0f;
constexpr float K_UNLOCK_CARD_HEIGHT   = 210.0f;
constexpr float K_SETUP_CARD_HEIGHT    = 366.0f;
constexpr float K_CARD_RADIUS          = 16.0f;
constexpr float K_CARD_PADDING         = 28.0f;
constexpr float K_FIELD_HEIGHT         = 40.0f;
constexpr float K_FIELD_RADIUS         = 6.0f;
constexpr float K_BUTTON_HEIGHT        = 40.0f;
constexpr float K_GAP                  = 14.0f;
constexpr float K_REVEAL_SIZE          = 22.0f;
constexpr float K_REVEAL_MARGIN        = 6.0f;
constexpr float K_UNLOCK_FIRST_FIELD_Y = 74.0f;
constexpr float K_SETUP_FIRST_FIELD_Y  = 96.0f;
constexpr float K_HALO_BLUR            = 90.0f;
constexpr u8    K_HALO_ALPHA           = 46;
}

UnlockScreen::UnlockScreen(Settings*     t_settings,
                           MasterKey*    t_master_key,
                           const Fonts*  t_fonts,
                           const Assets* t_assets,
                           const Window* t_window,
                           CommandQueue* t_commands)
	: m_settings(t_settings)
	, m_master_key(t_master_key)
	, m_fonts(t_fonts)
	, m_assets(t_assets)
	, m_window(t_window)
	, m_commands(t_commands)
{
}

auto UnlockScreen::reset_fields() -> void
{
	for (TextInput& field : m_fields) {
		field.set_value("");
		field.set_masked(true);
	}

	focus_field(K_PASSWORD);
}

auto UnlockScreen::show_unlock() -> void
{
	m_setup          = false;
	m_wrong_password = false;
	reset_fields();
	m_active = true;
}

auto UnlockScreen::show_setup() -> void
{
	m_setup            = true;
	m_passwords_differ = false;
	m_setup_failed     = false;
	reset_fields();
	m_active = true;
}

auto UnlockScreen::hide() -> void
{
	m_active = false;
}

auto UnlockScreen::card_rect() const -> Rect
{
	const Rect  area        = m_window->content_rect();
	const float card_height = m_setup ? K_SETUP_CARD_HEIGHT : K_UNLOCK_CARD_HEIGHT;
	const float top         = area.y + (area.h - card_height) * 0.5f;

	return Rect{area.center().x - K_CARD_WIDTH * 0.5f, top, K_CARD_WIDTH, card_height};
}

auto UnlockScreen::field_rect(u32 t_field) const -> Rect
{
	const Rect  card    = card_rect();
	const float first_y = card.y + (m_setup ? K_SETUP_FIRST_FIELD_Y : K_UNLOCK_FIRST_FIELD_Y);

	return Rect{card.x + K_CARD_PADDING, first_y + t_field * (K_FIELD_HEIGHT + K_GAP), card.w - K_CARD_PADDING * 2.0f, K_FIELD_HEIGHT};
}

auto UnlockScreen::field_text_rect(u32 t_field) const -> Rect
{
	Rect field = field_rect(t_field);
	field.w -= K_REVEAL_SIZE + K_REVEAL_MARGIN;

	return field;
}

auto UnlockScreen::reveal_rect(u32 t_field) const -> Rect
{
	const Rect field = field_rect(t_field);

	return Rect{field.right() - K_REVEAL_SIZE - K_REVEAL_MARGIN, field.y + (field.h - K_REVEAL_SIZE) * 0.5f, K_REVEAL_SIZE, K_REVEAL_SIZE};
}

auto UnlockScreen::submit_rect() const -> Rect
{
	const Rect last_field = field_rect(field_count() - 1);

	return Rect{last_field.x, last_field.bottom() + K_GAP, last_field.w, K_BUTTON_HEIGHT};
}

auto UnlockScreen::is_reveal_hit(Vec2 t_point) const -> bool
{
	for (u32 i = 0; i < field_count(); i += 1) {
		if (reveal_rect(i).contains(t_point)) return true;
	}

	return false;
}

auto UnlockScreen::field_at(Vec2 t_point) const -> i32
{
	if (is_reveal_hit(t_point)) return -1;

	for (u32 i = 0; i < field_count(); i += 1) {
		if (field_rect(i).contains(t_point)) return static_cast<i32>(i);
	}

	return -1;
}

auto UnlockScreen::focus_field(u32 t_field) -> void
{
	for (u32 i = 0; i < 2; i += 1) {
		m_fields[i].set_focused(i == t_field);
	}
}

auto UnlockScreen::submit() -> void
{
	if (m_setup) {
		attempt_setup();
	} else {
		attempt_unlock();
	}
}

auto UnlockScreen::attempt_unlock() -> void
{
	TextInput* field    = &m_fields[K_PASSWORD];
	const bool unlocked = m_master_key->unlock(field->value(), m_settings->master_key);

	field->set_value("");
	field->set_focused(!unlocked);
	m_wrong_password = !unlocked;

	if (unlocked) {
		m_commands->push(Command{.type = CommandType::VaultUnlocked});
	}
}

auto UnlockScreen::attempt_setup() -> void
{
	const std::string_view chosen = m_fields[K_PASSWORD].value();
	if (chosen.empty()) return;

	if (chosen != m_fields[K_CONFIRMATION].value()) {
		m_passwords_differ = true;
		m_setup_failed     = false;
		m_fields[K_CONFIRMATION].set_value("");
		focus_field(K_CONFIRMATION);
		return;
	}

	if (!m_master_key->create(chosen, &m_settings->master_key)) {
		m_setup_failed     = true;
		m_passwords_differ = false;
		return;
	}

	m_settings->master_password_enabled = true;
	m_passwords_differ                  = false;
	m_setup_failed                      = false;

	for (TextInput& field : m_fields) {
		field.set_value("");
	}

	m_commands->push(Command{.type = CommandType::VaultCreated});
}

auto UnlockScreen::update(float t_delta_seconds) -> void
{
	if (!m_active) return;

	for (u32 i = 0; i < field_count(); i += 1) {
		m_fields[i].update(t_delta_seconds);
	}
}

auto UnlockScreen::on_char(u32 t_character) -> bool
{
	if (!m_active) return false;

	for (u32 i = 0; i < field_count(); i += 1) {
		m_fields[i].on_char(t_character);
	}

	return true;
}

auto UnlockScreen::on_key_down(u32 t_key) -> bool
{
	if (!m_active) return false;

	if (t_key == VK_RETURN) {
		submit();
	} else if (m_setup && t_key == VK_TAB) {
		focus_field(m_fields[K_PASSWORD].is_focused() ? K_CONFIRMATION : K_PASSWORD);
	} else {
		for (u32 i = 0; i < field_count(); i += 1) {
			m_fields[i].on_key_down(t_key);
		}
	}

	return true;
}

auto UnlockScreen::on_pointer_down(Vec2 t_point) -> bool
{
	if (!m_active) return false;

	const i32 pressed = field_at(t_point);
	if (pressed >= 0) {
		focus_field(static_cast<u32>(pressed));
		m_fields[pressed].on_pointer_down(m_fonts->body, field_text_rect(static_cast<u32>(pressed)), t_point.x);
	}

	return true;
}

auto UnlockScreen::on_pointer_move(Vec2 t_point) -> bool
{
	if (!m_active) return false;

	for (u32 i = 0; i < field_count(); i += 1) {
		if (m_fields[i].is_selecting()) {
			m_fields[i].on_pointer_move(m_fonts->body, field_text_rect(i), t_point.x);
		}
	}

	return true;
}

auto UnlockScreen::on_pointer_up(Vec2 t_point) -> bool
{
	if (!m_active) return false;

	bool ended_text_selection = false;
	for (TextInput& field : m_fields) {
		ended_text_selection = ended_text_selection || field.is_selecting();
		field.on_pointer_up();
	}

	if (ended_text_selection) return true;

	if (is_reveal_hit(t_point)) {
		const bool reveal = m_fields[K_PASSWORD].is_masked();

		for (TextInput& field : m_fields) {
			field.set_masked(!reveal);
		}
	} else if (submit_rect().contains(t_point)) {
		submit();
	}

	return true;
}

auto UnlockScreen::on_right_click(Vec2 t_point) -> bool
{
	if (!m_active) return false;

	const i32 clicked = field_at(t_point);
	if (clicked >= 0) {
		const auto field = static_cast<u32>(clicked);

		focus_field(field);
		m_fields[field].on_right_click(m_fonts->body, field_text_rect(field), t_point.x);
		m_commands->push(Command{.type = CommandType::ShowTextMenu, .position = t_point, .text_input = &m_fields[field]});
	}

	return true;
}

auto UnlockScreen::cursor() const -> CursorKind
{
	if (!m_active) return CursorKind::Arrow;

	for (const TextInput& field : m_fields) {
		if (field.is_selecting()) return CursorKind::IBeam;
	}

	if (is_reveal_hit(m_mouse) || submit_rect().contains(m_mouse)) return CursorKind::Hand;

	return field_at(m_mouse) >= 0 ? CursorKind::IBeam : CursorKind::Arrow;
}

auto UnlockScreen::draw_field(DrawList* t_draw_list, u32 t_field) -> void
{
	const Color accent = m_settings->accent;
	TextInput*  field  = &m_fields[t_field];
	const Rect  reveal = reveal_rect(t_field);

	controls::draw_field(t_draw_list, field_rect(t_field), K_FIELD_RADIUS, field->is_focused() ? accent : g_theme.control, g_theme.field, 255);
	field->draw(t_draw_list, m_fonts->body, field_text_rect(t_field), g_theme.text, accent);
	controls::draw_eye(t_draw_list, m_assets, reveal, !field->is_masked(), reveal.contains(m_mouse) ? g_theme.text : g_theme.text_dim);
}

auto UnlockScreen::draw_submit_button(DrawList* t_draw_list, std::string_view t_label) const -> void
{
	const Rect button = submit_rect();

	controls::draw_button(t_draw_list, m_fonts->body, button, t_label, controls::ButtonStyle::Accent, m_settings->accent, true, button.contains(m_mouse), 255);
}

auto UnlockScreen::draw(DrawList* t_draw_list) -> void
{
	if (!m_active) return;

	const Vec2  window    = m_window->size();
	const Font& body      = m_fonts->body;
	const Font& secondary = m_fonts->secondary;
	const Color accent    = m_settings->accent;
	const Rect  card      = card_rect();

	const auto draw_centered = [&](const Font& t_font, float t_baseline, std::string_view t_text, Color t_color) {
		draw_text(t_draw_list, t_font, Vec2{card.center().x - text_width(t_font, t_text) * 0.5f, t_baseline}, t_text, t_color);
	};

	const Color backdrop = g_theme.window;
	t_draw_list->add_backdrop(Rect{0.0f, 0.0f, window.x, window.y - K_STATUS_BAR_HEIGHT}, backdrop, backdrop, backdrop, backdrop);
	t_draw_list->add_shadow(card, K_CARD_RADIUS, K_HALO_BLUR, with_alpha(accent, K_HALO_ALPHA));
	t_draw_list->add_bordered_rect(card, rounded(K_CARD_RADIUS), g_theme.surface, g_theme.border, 1.0f);

	const float title_baseline       = card.y + K_CARD_PADDING + body.ascent;
	const float description_baseline = card.y + K_CARD_PADDING + body.line_height() + 4.0f + secondary.ascent;
	const float error_baseline       = submit_rect().bottom() + K_GAP + secondary.ascent;

	if (m_setup) {
		draw_centered(body, title_baseline, "Create a master password", g_theme.text);
		draw_centered(secondary, description_baseline, "It encrypts your saved account passwords.", g_theme.text_dim);
		draw_centered(secondary, description_baseline + secondary.line_height(), "Pick something memorable - it can't be recovered.", g_theme.text_dim);
	} else {
		draw_centered(body, title_baseline, "Welcome back", g_theme.text);
		draw_centered(secondary, description_baseline, "Enter your master password to continue.", g_theme.text_dim);
	}

	for (u32 i = 0; i < field_count(); i += 1) {
		draw_field(t_draw_list, i);
	}

	draw_submit_button(t_draw_list, m_setup ? "Create password" : "Unlock");

	std::string_view error;
	if (!m_setup && m_wrong_password) {
		error = "Incorrect password.";
	} else if (m_setup && m_passwords_differ) {
		error = "Passwords don't match.";
	} else if (m_setup && m_setup_failed) {
		error = "Something went wrong - try again.";
	}

	draw_centered(secondary, error_baseline, error, g_theme.error);
}
