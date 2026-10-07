#include "ui/unlock_screen.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <numbers>
#include <string>
#include <utility>

#include <sodium.h>

#include "core/animation.h"
#include "core/settings.h"
#include "gfx/assets.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "os/input.h"
#include "os/window.h"
#include "ui/controls.h"
#include "ui/text.h"
#include "ui/theme.h"
#include "ui/window_layout.h"

namespace {
constexpr float K_FORM_WIDTH       = 300.0f;
constexpr float K_ICON_SIZE        = 48.0f;
constexpr float K_ICON_GAP         = 18.0f;
constexpr float K_TITLE_GAP        = 6.0f;
constexpr float K_FIELDS_GAP       = 22.0f;
constexpr float K_FIELD_HEIGHT     = 40.0f;
constexpr float K_FIELD_RADIUS     = 8.0f;
constexpr float K_FIELD_SPACING    = 10.0f;
constexpr float K_NOTICE_GAP       = 12.0f;
constexpr float K_SUBMIT_SIZE      = 28.0f;
constexpr float K_SUBMIT_INSET     = 6.0f;
constexpr float K_SUBMIT_RADIUS    = 6.0f;
constexpr float K_SUBMIT_ICON_SIZE = 16.0f;
constexpr float K_REVEAL_SIZE      = 20.0f;
constexpr float K_REVEAL_GAP       = 8.0f;
constexpr float K_OPTICAL_CENTER   = 0.46f;
constexpr float K_SHAKE_SECONDS    = 0.4f;
constexpr float K_SHAKE_DISTANCE   = 8.0f;
constexpr float K_SHAKE_TURNS      = 3.0f;

constexpr float K_BACKDROP_IN_RATE  = 16.0f;
constexpr float K_BACKDROP_OUT_RATE = 9.0f;
constexpr float K_CONTENT_IN_RATE   = 11.0f;
constexpr float K_CONTENT_OUT_RATE  = 16.0f;
constexpr float K_STAGGER           = 0.55f;
constexpr float K_ARRIVE_SCALE      = 0.94f;
constexpr float K_LEAVE_SCALE       = 1.06f;
constexpr float K_TRANSITION_STEP   = 1.0f / 30.0f;
constexpr float K_SPINNER_TURNS     = 1.2f;
}

UnlockScreen::UnlockScreen(Settings*         t_settings,
                           MasterKey*        t_master_key,
                           const Fonts*      t_fonts,
                           const Assets*     t_assets,
                           const os::Window* t_window,
                           CommandQueue*     t_commands)
	: m_settings(t_settings)
	, m_master_key(t_master_key)
	, m_fonts(t_fonts)
	, m_assets(t_assets)
	, m_window(t_window)
	, m_commands(t_commands)
{
	m_fields[K_PASSWORD].set_placeholder("Master password");
	m_fields[K_CONFIRMATION].set_placeholder("Type it again");
}

auto UnlockScreen::reset_fields() -> void
{
	for (TextInput& field : m_fields) {
		field.set_value("");
		field.set_masked(true);
	}

	focus_field(K_PASSWORD);
}

auto UnlockScreen::show_unlock(bool t_animated) -> void
{
	m_setup = false;
	show(t_animated);
}

auto UnlockScreen::show_setup(bool t_animated) -> void
{
	m_setup = true;
	show(t_animated);
}

auto UnlockScreen::show(bool t_animated) -> void
{
	clear_errors();
	reset_fields();
	m_active = true;

	if (!t_animated) {
		m_backdrop = 1.0f;
		m_content  = 1.0f;
	}
}

auto UnlockScreen::hide() -> void
{
	m_active = false;

	for (TextInput& field : m_fields) {
		field.set_focused(false);
	}
}

// Locking brings the backdrop in first and then settles the form into place; unlocking lets the form fly toward the viewer and fade,
// then lifts the backdrop off the app underneath.
auto UnlockScreen::animate_presence(float t_delta_seconds) -> void
{
	if (m_active) {
		m_backdrop = animation::ease_toward(m_backdrop, 1.0f, K_BACKDROP_IN_RATE, t_delta_seconds);
		m_content  = animation::ease_toward(m_content, m_backdrop > K_STAGGER ? 1.0f : 0.0f, K_CONTENT_IN_RATE, t_delta_seconds);
	} else {
		m_content  = animation::ease_toward(m_content, 0.0f, K_CONTENT_OUT_RATE, t_delta_seconds);
		m_backdrop = animation::ease_toward(m_backdrop, m_content < K_STAGGER ? 0.0f : 1.0f, K_BACKDROP_OUT_RATE, t_delta_seconds);
	}
}

auto UnlockScreen::clear_errors() -> void
{
	m_wrong_password   = false;
	m_passwords_differ = false;
	m_setup_failed     = false;
}

auto UnlockScreen::layout() const -> Layout
{
	const Font& title     = m_fonts->title;
	const Font& secondary = m_fonts->secondary;
	const auto  fields    = static_cast<float>(field_count());
	const float height    = K_ICON_SIZE + K_ICON_GAP + title.line_height() + K_TITLE_GAP + secondary.line_height() * static_cast<float>(subtitle_lines()) +
	                        K_FIELDS_GAP + fields * K_FIELD_HEIGHT + (fields - 1.0f) * K_FIELD_SPACING + K_NOTICE_GAP + secondary.line_height();

	const Rect  area   = content_rect(m_window->size());
	const float center = area.center().x;
	const float left   = snapped_to_pixel(center - K_FORM_WIDTH * 0.5f + shake_offset());
	float       y      = snapped_to_pixel(area.y + std::max(0.0f, area.h - height) * K_OPTICAL_CENTER);

	Layout result{};
	result.icon = Rect{snapped_to_pixel(center - K_ICON_SIZE * 0.5f), y, K_ICON_SIZE, K_ICON_SIZE};
	y += K_ICON_SIZE + K_ICON_GAP;

	result.title_baseline = y + title.ascent;
	y += title.line_height() + K_TITLE_GAP;

	result.subtitle_baseline = y + secondary.ascent;
	y += secondary.line_height() * static_cast<float>(subtitle_lines()) + K_FIELDS_GAP;

	for (u32 i = 0; i < field_count(); i += 1) {
		result.fields[i] = Rect{left, snapped_to_pixel(y), K_FORM_WIDTH, K_FIELD_HEIGHT};
		y += K_FIELD_HEIGHT + K_FIELD_SPACING;
	}

	result.notice_top = snapped_to_pixel(y - K_FIELD_SPACING + K_NOTICE_GAP);

	return result;
}

// A wrong password nudges the fields side to side, fading out.
auto UnlockScreen::shake_offset() const -> float
{
	if (m_shake_seconds <= 0.0f || !animation::is_enabled()) return 0.0f;

	const float progress = 1.0f - m_shake_seconds / K_SHAKE_SECONDS;

	return std::sin(progress * K_SHAKE_TURNS * 2.0f * std::numbers::pi_v<float>) * K_SHAKE_DISTANCE * (1.0f - progress);
}

auto UnlockScreen::submit_rect(const Layout& t_layout) const -> Rect
{
	const Rect field = t_layout.fields[field_count() - 1];

	return Rect{field.right() - K_SUBMIT_INSET - K_SUBMIT_SIZE, field.y + (field.h - K_SUBMIT_SIZE) * 0.5f, K_SUBMIT_SIZE, K_SUBMIT_SIZE};
}

auto UnlockScreen::reveal_rect(const Layout& t_layout, u32 t_field) const -> Rect
{
	const Rect  field = t_layout.fields[t_field];
	const float right = t_field == field_count() - 1 ? submit_rect(t_layout).x - K_REVEAL_GAP : field.right() - K_REVEAL_GAP;

	return Rect{right - K_REVEAL_SIZE, field.y + (field.h - K_REVEAL_SIZE) * 0.5f, K_REVEAL_SIZE, K_REVEAL_SIZE};
}

auto UnlockScreen::field_text_rect(const Layout& t_layout, u32 t_field) const -> Rect
{
	Rect field = t_layout.fields[t_field];
	field.w    = reveal_rect(t_layout, t_field).x - field.x;

	return field;
}

auto UnlockScreen::is_reveal_hit(const Layout& t_layout, Vec2 t_point) const -> bool
{
	for (u32 i = 0; i < field_count(); i += 1) {
		if (reveal_rect(t_layout, i).contains(t_point)) return true;
	}

	return false;
}

auto UnlockScreen::field_at(const Layout& t_layout, Vec2 t_point) const -> i32
{
	if (is_reveal_hit(t_layout, t_point) || submit_rect(t_layout).contains(t_point)) return -1;

	for (u32 i = 0; i < field_count(); i += 1) {
		if (t_layout.fields[i].contains(t_point)) return static_cast<i32>(i);
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
	const std::string_view password = m_fields[K_PASSWORD].value();
	if (password.empty()) return;

	start_derivation(password, false);
}

auto UnlockScreen::attempt_setup() -> void
{
	const std::string_view chosen = m_fields[K_PASSWORD].value();
	if (chosen.empty()) return;

	if (chosen != m_fields[K_CONFIRMATION].value()) {
		m_passwords_differ = true;
		m_setup_failed     = false;
		m_shake_seconds    = K_SHAKE_SECONDS;
		m_fields[K_CONFIRMATION].set_value("");
		focus_field(K_CONFIRMATION);
		return;
	}

	start_derivation(chosen, true);
}

// Argon2id is slow on purpose, so the key is derived on another thread and the screen keeps animating meanwhile.
auto UnlockScreen::start_derivation(std::string_view t_password, bool t_create) -> void
{
	m_derivation = std::async(std::launch::async, [password = std::string{t_password}, params = m_settings->master_key, t_create]() mutable {
		auto derived       = std::make_unique<DerivedKey>();
		derived->succeeded = t_create ? derived->key.create(password, &derived->params) : derived->key.unlock(password, params);
		sodium_memzero(password.data(), password.size());

		return derived;
	});
}

auto UnlockScreen::finish_derivation(std::unique_ptr<DerivedKey> t_derived) -> void
{
	for (TextInput& field : m_fields) {
		field.set_value("");
	}

	if (!t_derived->succeeded) {
		m_wrong_password = !m_setup;
		m_setup_failed   = m_setup;
		m_shake_seconds  = m_setup ? 0.0f : K_SHAKE_SECONDS;
		focus_field(K_PASSWORD);
		return;
	}

	m_master_key->swap(&t_derived->key);

	if (m_setup) {
		m_settings->master_key              = t_derived->params;
		m_settings->master_password_enabled = true;
		m_commands->push(Command{.type = CommandType::VAULT_CREATED});
	} else {
		m_commands->push(Command{.type = CommandType::VAULT_UNLOCKED});
	}
}

auto UnlockScreen::update(float t_delta_seconds) -> void
{
	animate_presence(std::min(t_delta_seconds, K_TRANSITION_STEP));

	if (is_deriving()) {
		m_spin = std::fmod(m_spin + t_delta_seconds * K_SPINNER_TURNS * 2.0f * std::numbers::pi_v<float>, 2.0f * std::numbers::pi_v<float>);
		animation::request_frame();

		if (m_derivation.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
			finish_derivation(m_derivation.get());
		}
	}

	if (!m_active) return;

	for (u32 i = 0; i < field_count(); i += 1) {
		m_fields[i].update(t_delta_seconds);
	}

	if (m_shake_seconds > 0.0f) {
		m_shake_seconds = std::max(0.0f, m_shake_seconds - t_delta_seconds);
		animation::request_frame();
	}
}

auto UnlockScreen::on_char(u32 t_character) -> bool
{
	if (!m_active) return false;
	if (is_deriving()) return true;

	clear_errors();

	for (u32 i = 0; i < field_count(); i += 1) {
		m_fields[i].on_char(t_character);
	}

	return true;
}

auto UnlockScreen::on_key_down(os::Key t_key) -> bool
{
	if (!m_active) return false;
	if (is_deriving()) return true;

	if (t_key == os::Key::ENTER) {
		submit();
	} else if (m_setup && t_key == os::Key::TAB) {
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
	if (is_deriving()) return true;

	const Layout current = layout();
	const i32    pressed = field_at(current, t_point);
	if (pressed >= 0) {
		const auto field = static_cast<u32>(pressed);

		focus_field(field);
		m_fields[field].on_pointer_down(m_fonts->body, field_text_rect(current, field), t_point.x);
	}

	return true;
}

auto UnlockScreen::on_pointer_move(Vec2 t_point) -> bool
{
	if (!m_active) return false;

	const Layout current = layout();

	for (u32 i = 0; i < field_count(); i += 1) {
		if (m_fields[i].is_selecting()) {
			m_fields[i].on_pointer_move(m_fonts->body, field_text_rect(current, i), t_point.x);
		}
	}

	return true;
}

auto UnlockScreen::on_pointer_up(Vec2 t_point) -> bool
{
	if (!m_active) return false;
	if (is_deriving()) return true;

	bool ended_text_selection = false;
	for (TextInput& field : m_fields) {
		ended_text_selection = ended_text_selection || field.is_selecting();
		field.on_pointer_up();
	}

	if (ended_text_selection) return true;

	const Layout current = layout();

	if (is_reveal_hit(current, t_point)) {
		const bool reveal = m_fields[K_PASSWORD].is_masked();

		for (TextInput& field : m_fields) {
			field.set_masked(!reveal);
		}
	} else if (submit_rect(current).contains(t_point)) {
		submit();
	}

	return true;
}

auto UnlockScreen::on_right_click(Vec2 t_point) -> bool
{
	if (!m_active) return false;
	if (is_deriving()) return true;

	const Layout current = layout();
	const i32    clicked = field_at(current, t_point);
	if (clicked >= 0) {
		const auto field = static_cast<u32>(clicked);

		focus_field(field);
		m_fields[field].on_right_click(m_fonts->body, field_text_rect(current, field), t_point.x);
		m_commands->push(Command{.type = CommandType::SHOW_TEXT_MENU, .position = t_point, .text_input = &m_fields[field]});
	}

	return true;
}

auto UnlockScreen::cursor() const -> CursorKind
{
	if (!m_active) return CursorKind::ARROW;

	for (const TextInput& field : m_fields) {
		if (field.is_selecting()) return CursorKind::I_BEAM;
	}

	const Layout current = layout();
	if (is_reveal_hit(current, m_mouse) || submit_rect(current).contains(m_mouse)) return CursorKind::HAND;

	return field_at(current, m_mouse) >= 0 ? CursorKind::I_BEAM : CursorKind::ARROW;
}

auto UnlockScreen::error_message() const -> std::string_view
{
	if (m_wrong_password) return "That password isn't right";
	if (m_passwords_differ) return "The passwords don't match";
	if (m_setup_failed) return "Something went wrong. Try again";

	return {};
}

auto UnlockScreen::draw_field(DrawList* t_draw_list, const Layout& t_layout, u32 t_field, u8 t_alpha) -> void
{
	TextInput*  field  = &m_fields[t_field];
	const Rect  reveal = reveal_rect(t_layout, t_field);
	const bool  failed = !error_message().empty() && (m_wrong_password || t_field == K_CONFIRMATION);
	const Color border = failed ? g_theme.error : field->is_focused() ? m_settings->accent : g_theme.control;

	controls::draw_field(t_draw_list, t_layout.fields[t_field], K_FIELD_RADIUS, border, g_theme.field, t_alpha);
	field->draw(t_draw_list, m_fonts->body, field_text_rect(t_layout, t_field), faded(g_theme.text, t_alpha), faded(m_settings->accent, t_alpha));
	controls::draw_eye(t_draw_list, m_assets, reveal, !field->is_masked(), faded(reveal.contains(m_mouse) ? g_theme.text : g_theme.text_dim, t_alpha));
}

auto UnlockScreen::draw_submit_button(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) const -> void
{
	const Color accent = m_settings->accent;
	const Rect  button = submit_rect(t_layout);
	const Rect  arrow  = button.centered(K_SUBMIT_ICON_SIZE, K_SUBMIT_ICON_SIZE);

	t_draw_list->add_rounded_rect(button, rounded(K_SUBMIT_RADIUS), faded(button.contains(m_mouse) ? hovered(accent) : accent, t_alpha));
	const Color ink = faded(controls::ink_on(accent), t_alpha);
	if (is_deriving()) {
		t_draw_list->add_rotated_image(arrow, m_spin, m_assets->get(Asset::ICON_UPDATE), ink);
	} else {
		t_draw_list->add_rotated_image(arrow, std::numbers::pi_v<float>, m_assets->get(Asset::ICON_ARROW_BACK), ink);
	}
}

// One line under the fields: what went wrong, or else a Caps Lock warning while typing.
auto UnlockScreen::draw_notice(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) const -> void
{
	const Font&            font   = m_fonts->secondary;
	const std::string_view error  = error_message();
	const bool             typing = m_fields[K_PASSWORD].is_focused() || m_fields[K_CONFIRMATION].is_focused();

	controls::NoticeKind kind  = controls::NoticeKind::ALERT;
	std::string_view     text  = error;
	Color                color = g_theme.error;

	if (error.empty()) {
		if (!typing || !os::is_caps_lock_on()) return;

		kind  = controls::NoticeKind::CAPS_LOCK;
		text  = "Caps Lock is on";
		color = controls::caution_color();
	}

	const float left = snapped_to_pixel(content_rect(m_window->size()).center().x - controls::notice_width(font, text) * 0.5f);
	controls::draw_notice(t_draw_list, font, Vec2{left, t_layout.notice_top}, kind, text, faded(color, t_alpha));
}

auto UnlockScreen::draw(DrawList* t_draw_list) -> void
{
	if (m_backdrop <= 0.0f) return;

	const Vec2   window    = m_window->size();
	const Layout current   = layout();
	const Font&  title     = m_fonts->title;
	const Font&  secondary = m_fonts->secondary;
	const float  center    = content_rect(window).center().x;

	const Color backdrop = faded(g_theme.window, to_alpha(m_backdrop));
	t_draw_list->add_backdrop(Rect{0.0f, 0.0f, window.x, window.y - K_STATUS_BAR_HEIGHT}, backdrop, backdrop, backdrop, backdrop);

	if (m_content <= 0.0f) return;

	const u8    alpha = to_alpha(m_content);
	const float scale = m_active ? K_ARRIVE_SCALE + (1.0f - K_ARRIVE_SCALE) * m_content : K_LEAVE_SCALE + (1.0f - K_LEAVE_SCALE) * m_content;
	t_draw_list->push_scale(Vec2{center, (current.icon.y + current.notice_top) * 0.5f}, scale);

	const auto draw_centered = [&](const Font& t_font, float t_baseline, std::string_view t_text, Color t_color) {
		draw_text(t_draw_list, t_font, Vec2{snapped_to_pixel(center - text_width(t_font, t_text) * 0.5f), t_baseline}, t_text, faded(t_color, alpha));
	};

	if (m_app_icon != nullptr) {
		t_draw_list->add_image(current.icon, m_app_icon, faded(Color{255, 255, 255, 255}, alpha));
	}

	if (m_setup) {
		draw_centered(title, current.title_baseline, "Create a master password", g_theme.text);
		draw_centered(secondary, current.subtitle_baseline, "It encrypts your saved account passwords.", g_theme.text_dim);
		draw_centered(secondary, current.subtitle_baseline + secondary.line_height(), "Pick something memorable - it can't be recovered.", g_theme.text_dim);
	} else {
		draw_centered(title, current.title_baseline, "Pulsar is locked", g_theme.text);
		draw_centered(secondary, current.subtitle_baseline, "Enter your master password to open your vault.", g_theme.text_dim);
	}

	for (u32 i = 0; i < field_count(); i += 1) {
		draw_field(t_draw_list, current, i, alpha);
	}

	draw_submit_button(t_draw_list, current, alpha);
	draw_notice(t_draw_list, current, alpha);

	t_draw_list->pop_scale();
}
