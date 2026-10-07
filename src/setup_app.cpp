#include "setup_app.h"

#include <utility>
#include <vector>

#include "core/animation.h"
#include "core/storage.h"
#include "core/str.h"
#include "os/app_icon.h"
#include "ui/controls.h"
#include "ui/text.h"
#include "ui/text_input.h"
#include "ui/theme.h"
#include "ui/window_layout.h"

namespace {
constexpr u32   K_WINDOW_WIDTH              = 480;
constexpr u32   K_WINDOW_HEIGHT             = 380;
constexpr u32   K_DRAW_LIST_VERTEX_CAPACITY = 1 << 14;
constexpr u32   K_DRAW_LIST_INDEX_CAPACITY  = (1 << 14) * 3 / 2;
constexpr u32   K_APP_ICON_TEXTURE_SIZE     = 256;
constexpr float K_HEADING_FONT_SCALE        = 1.3f;
constexpr float K_TITLE_FONT_SCALE          = 1.75f;
constexpr float K_IDLE_POLL_SECONDS         = 0.25f;
constexpr auto  K_RESUME_FRAME_TIME         = std::chrono::microseconds(16667);
constexpr float K_CONTROL_ICON_SIZE         = 16.0f;
}

SetupApp::SetupApp(SetupMode t_mode)
	: m_mode(t_mode)
	, m_screen(t_mode, &m_settings, &m_fonts, &m_heading_fonts, &m_title_fonts, &m_assets, &m_window)
	, m_truncation_hint(&m_fonts)
{
}

auto SetupApp::create() -> bool
{
	m_assets.begin_decode();
	storage::load_settings(&m_settings);

	apply_theme(m_settings.theme);
	animation::set_enabled(m_settings.animations_enabled);
	animation::set_speed(m_settings.animation_speed);
	set_caret_style(m_settings.caret_style, m_settings.caret_trail, m_settings.caret_trail_strength);
	set_corner_roundness(m_settings.corner_roundness);

	if (!m_window.create("Pulsar setup", K_WINDOW_WIDTH, K_WINDOW_HEIGHT, os::WindowKind::DIALOG)) return false;
	m_window.set_title_bar(K_TITLE_BAR_HEIGHT, [this](Vec2 t_point) { return title_bar().button_at(t_point) != TitleBarButton::NONE; });

	if (!m_renderer.init(&m_window, m_settings.renderer)) return false;

	if (!m_assets.finish_upload(&m_renderer)) return false;
	if (!reload_fonts()) return false;

	const std::vector<u8> icon = os::app_icon_pixels(K_APP_ICON_TEXTURE_SIZE);
	if (!icon.empty()) {
		m_app_icon = Assets::create_texture(&m_renderer, icon.data(), K_APP_ICON_TEXTURE_SIZE, K_APP_ICON_TEXTURE_SIZE);
		m_screen.set_app_icon(m_app_icon.get());
	}

	m_draw_list.init(K_DRAW_LIST_VERTEX_CAPACITY, K_DRAW_LIST_INDEX_CAPACITY);
	m_window.on_redraw([this] { redraw_while_moving(); });
	m_window.on_dpi_changed([this] { reload_fonts(); });
	m_last_frame_time = std::chrono::steady_clock::now();

	return true;
}

auto SetupApp::reload_fonts() -> bool
{
	const auto load = [this] {
		const float scale = m_window.dpi_scale();

		return m_fonts.load(&m_renderer, m_settings.font_name, m_settings.font_size, m_settings.secondary_font_size, scale) &&
		       m_heading_fonts.load(&m_renderer, m_settings.font_name, m_settings.font_size * K_HEADING_FONT_SCALE, m_settings.secondary_font_size, scale) &&
		       m_title_fonts.load(&m_renderer, m_settings.font_name, m_settings.font_size * K_TITLE_FONT_SCALE, m_settings.secondary_font_size, scale);
	};

	if (load()) return true;

	copy_to(Settings{}.font_name, m_settings.font_name);

	return load();
}

auto SetupApp::title_bar() const -> TitleBarLayout
{
	return TitleBarLayout{.width = static_cast<float>(m_window.width()), .native_controls_width = m_window.native_controls_width(), .dialog = true};
}

auto SetupApp::run() -> SetupOutcome
{
	if (!create()) return m_mode == SetupMode::FIRST_RUN ? SetupOutcome::PORTABLE : SetupOutcome::CLOSED;

	frame();
	m_window.show();
	m_window.bring_to_front();

	while (!m_window.should_quit()) {
		m_window.pump_messages();

		for (const os::InputEvent& event : m_window.input_events()) {
			handle_input(event);
		}

		frame();

		if (const std::optional<SetupOutcome> outcome = m_screen.outcome()) return *outcome;

		const float requested_wait = animation::take_idle_wait(K_IDLE_POLL_SECONDS);
		const float wait           = m_window.is_minimized() ? K_IDLE_POLL_SECONDS : requested_wait;

		if (wait > 0.0f && m_window.wait_for_messages(wait)) {
			m_last_frame_time = std::chrono::steady_clock::now() - K_RESUME_FRAME_TIME;
		}
	}

	return SetupOutcome::CLOSED;
}

auto SetupApp::handle_input(const os::InputEvent& t_event) -> void
{
	switch (t_event.type) {
		using enum os::InputEventType;

		case MOUSE_MOVE: {
			m_mouse = t_event.position;
			m_screen.on_pointer_move(m_mouse);
			break;
		}

		case MOUSE_DOWN: {
			m_mouse        = t_event.position;
			m_pointer_down = true;

			const TitleBarButton button = title_bar().button_at(m_mouse);
			if (button != TitleBarButton::NONE) {
				m_pressed_button = button;
				break;
			}

			m_screen.on_pointer_down(m_mouse);
			break;
		}

		case MOUSE_UP: {
			m_mouse        = t_event.position;
			m_pointer_down = false;

			const TitleBarButton pressed = std::exchange(m_pressed_button, TitleBarButton::NONE);
			if (pressed == TitleBarButton::NONE) {
				m_screen.on_pointer_up(m_mouse);
				break;
			}

			if (title_bar().button_at(m_mouse) != pressed) break;

			if (pressed == TitleBarButton::CLOSE) {
				m_window.request_quit();
			} else if (pressed == TitleBarButton::MINIMIZE) {
				m_window.minimize();
			}

			break;
		}

		case KEY_DOWN: {
			m_screen.on_key_down(t_event.key);
			break;
		}

		case CHARACTER: {
			m_screen.on_char(t_event.codepoint);
			break;
		}

		case MOUSE_WHEEL:
		case RIGHT_CLICK: {
			break;
		}
	}
}

auto SetupApp::redraw_while_moving() -> void
{
	if (m_window.physical_width() == 0 || m_window.physical_height() == 0) return;

	m_renderer.resize(&m_window);
	frame();
}

auto SetupApp::frame() -> void
{
	if (m_in_frame) return;

	m_in_frame = true;

	const auto  now           = std::chrono::steady_clock::now();
	const float delta_seconds = std::chrono::duration<float>(now - m_last_frame_time).count();
	m_last_frame_time         = now;

	set_pixel_scale(m_window.dpi_scale());
	m_screen.set_mouse(m_mouse);
	m_screen.update(delta_seconds);
	m_truncation_hint.update(delta_seconds, m_pointer_down);

	const bool over_controls = title_bar().button_at(m_mouse) != TitleBarButton::NONE;
	m_window.set_cursor(over_controls ? CursorKind::ARROW : m_screen.cursor());

	if (!m_window.is_minimized() && !m_window.is_hidden()) {
		render();
	}

	m_in_frame = false;
}

auto SetupApp::render() -> void
{
	const Vec2 size = m_window.size();

	m_draw_list.clear();
	m_draw_list.add_rect(Rect{0.0f, 0.0f, size.x, size.y}, g_theme.window);
	begin_truncation_probe(&m_draw_list, m_mouse);
	m_screen.draw(&m_draw_list);
	draw_window_controls();
	m_truncation_hint.capture(&m_draw_list);
	m_truncation_hint.draw(&m_draw_list, Rect{0.0f, K_TITLE_BAR_HEIGHT, size.x, size.y - K_TITLE_BAR_HEIGHT});
	m_draw_list.finish();

	m_renderer.render(&m_draw_list, g_theme.window);
}

auto SetupApp::draw_window_controls() -> void
{
	const TitleBarButton hovered_button = m_pressed_button != TitleBarButton::NONE ? m_pressed_button : title_bar().button_at(m_mouse);

	for (const TitleBarButton button : {TitleBarButton::MINIMIZE, TitleBarButton::CLOSE}) {
		const Rect rect = title_bar().button_rect(button);
		if (rect.w <= 0.0f) continue;

		const bool close      = button == TitleBarButton::CLOSE;
		const bool is_hovered = hovered_button == button && rect.contains(m_mouse);

		if (is_hovered) {
			m_draw_list.add_rect(rect, close ? K_TITLE_BAR_CLOSE_HOVER : with_alpha(g_theme.text, K_TITLE_BAR_HOVER_ALPHA));
		}

		Color glyph = g_theme.text_dim;
		if (is_hovered) {
			glyph = close ? K_TITLE_BAR_CLOSE_GLYPH_HOVER : g_theme.text;
		}

		m_draw_list.add_image(rect.centered(K_CONTROL_ICON_SIZE, K_CONTROL_ICON_SIZE), m_assets.get(close ? Asset::ICON_CLOSE : Asset::ICON_MINIMIZE), glyph);
	}
}
