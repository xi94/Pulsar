#include "setup_app.h"

#include <utility>
#include <vector>

#include "core/animation.h"
#include "core/storage.h"
#include "core/str.h"
#include "platform/app_icon.h"
#include "ui/controls.h"
#include "ui/text.h"
#include "ui/theme.h"

namespace {
constexpr u32 window_width = 480;
constexpr u32 window_height = 380;
constexpr u32 draw_list_vertex_capacity = 1 << 14;
constexpr u32 draw_list_index_capacity = (1 << 14) * 3 / 2;
constexpr u32 app_icon_texture_size = 256;
constexpr float heading_font_scale = 1.3f;
constexpr float title_font_scale = 1.75f;
constexpr float idle_poll_seconds = 0.25f;
constexpr auto resume_frame_time = std::chrono::microseconds(16667);
constexpr float control_icon_size = 16.0f;
}

SetupApp::SetupApp(SetupMode t_mode)
	: m_mode(t_mode)
	, m_screen(t_mode, m_settings, m_fonts, m_heading_fonts, m_title_fonts, m_assets, m_window)
	, m_truncation_hint(m_fonts)
{
}

bool SetupApp::create()
{
	m_assets.begin_decode();
	storage::load_settings(m_settings);

	apply_theme(m_settings.theme);
	animation::set_enabled(m_settings.animations_enabled);
	animation::set_speed(m_settings.animation_speed);
	set_corner_roundness(m_settings.corner_roundness);

	if (!m_window.create(L"Pulsar setup", window_width, window_height, WindowKind::Dialog)) return false;
	if (!m_renderer.init(m_window)) return false;

	m_assets.finish_upload(m_renderer);

	if (!reload_fonts()) return false;

	const std::vector<u8> icon = app_icon_pixels(app_icon_texture_size);
	if (!icon.empty()) {
		m_app_icon = Assets::create_texture(m_renderer, icon.data(), app_icon_texture_size, app_icon_texture_size);
		m_screen.set_app_icon(m_app_icon.get());
	}

	m_draw_list.init(draw_list_vertex_capacity, draw_list_index_capacity);
	m_window.on_redraw([this] { redraw_while_moving(); });
	m_window.on_dpi_changed([this] { reload_fonts(); });
	m_last_frame_time = std::chrono::steady_clock::now();

	return true;
}

bool SetupApp::reload_fonts()
{
	const auto load = [this] {
		const float scale = m_window.dpi_scale();

		return m_fonts.load(m_renderer, m_settings.font_name, m_settings.font_size, m_settings.secondary_font_size,
							scale) &&
			   m_heading_fonts.load(m_renderer, m_settings.font_name, m_settings.font_size * heading_font_scale,
									m_settings.secondary_font_size, scale) &&
			   m_title_fonts.load(m_renderer, m_settings.font_name, m_settings.font_size * title_font_scale,
								  m_settings.secondary_font_size, scale);
	};

	if (load()) return true;

	copy_to(Settings{}.font_name, m_settings.font_name);

	return load();
}

SetupOutcome SetupApp::run()
{
	if (!create()) return m_mode == SetupMode::FirstRun ? SetupOutcome::Portable : SetupOutcome::Closed;

	frame();
	m_window.show();
	SetForegroundWindow(m_window.handle());

	while (!m_window.should_close()) {
		m_window.pump_messages();

		for (const InputEvent &event : m_window.input_events()) {
			handle_input(event);
		}

		frame();

		if (const std::optional<SetupOutcome> outcome = m_screen.outcome()) return *outcome;

		const float requested_wait = animation::take_idle_wait(idle_poll_seconds);
		const float wait = m_window.is_minimized() ? idle_poll_seconds : requested_wait;

		if (wait > 0.0f && m_window.wait_for_messages(wait)) {
			m_last_frame_time = std::chrono::steady_clock::now() - resume_frame_time;
		}
	}

	return SetupOutcome::Closed;
}

void SetupApp::handle_input(const InputEvent &t_event)
{
	switch (t_event.type) {
		case InputEventType::MouseMove:
			m_mouse = t_event.position;
			m_screen.on_pointer_move(m_mouse);
			break;

		case InputEventType::MouseDown: {
			m_mouse = t_event.position;
			m_pointer_down = true;

			const TitleBarButton button = m_window.title_bar_button_at(m_mouse);
			if (button != TitleBarButton::None) {
				m_pressed_button = button;
				break;
			}

			m_screen.on_pointer_down(m_mouse);
			break;
		}

		case InputEventType::MouseUp: {
			m_mouse = t_event.position;
			m_pointer_down = false;

			const TitleBarButton pressed = std::exchange(m_pressed_button, TitleBarButton::None);
			if (pressed == TitleBarButton::None) {
				m_screen.on_pointer_up(m_mouse);
				break;
			}

			if (m_window.title_bar_button_at(m_mouse) != pressed) break;

			if (pressed == TitleBarButton::Close) {
				m_window.request_close();
			} else if (pressed == TitleBarButton::Minimize) {
				m_window.minimize();
			}

			break;
		}

		case InputEventType::KeyDown:
			m_screen.on_key_down(t_event.key);
			break;

		case InputEventType::Character:
			m_screen.on_char(t_event.key);
			break;

		case InputEventType::MouseWheel:
		case InputEventType::RightClick:
			break;
	}
}

void SetupApp::redraw_while_moving()
{
	if (m_window.physical_width() == 0 || m_window.physical_height() == 0) return;

	m_renderer.resize(m_window);
	frame();
}

void SetupApp::frame()
{
	if (m_in_frame) return;

	m_in_frame = true;

	const auto now = std::chrono::steady_clock::now();
	const float delta_seconds = std::chrono::duration<float>(now - m_last_frame_time).count();
	m_last_frame_time = now;

	set_pixel_scale(m_window.dpi_scale());
	m_screen.set_mouse(m_mouse);
	m_screen.update(delta_seconds);
	m_truncation_hint.update(delta_seconds, m_pointer_down);

	const bool over_controls = m_window.title_bar_button_at(m_mouse) != TitleBarButton::None;
	m_window.set_cursor(over_controls ? CursorKind::Arrow : m_screen.cursor());

	if (!m_window.is_minimized() && !m_window.is_hidden()) {
		render();
	}

	m_in_frame = false;
}

void SetupApp::render()
{
	const Vec2 size = m_window.size();

	m_draw_list.clear();
	m_draw_list.add_rect(Rect{0.0f, 0.0f, size.x, size.y}, theme().window);
	begin_truncation_probe(m_draw_list, m_mouse);
	m_screen.draw(m_draw_list);
	draw_window_controls();
	m_truncation_hint.capture(m_draw_list);
	m_truncation_hint.draw(m_draw_list, Rect{0.0f, title_bar_height, size.x, size.y - title_bar_height});
	m_draw_list.finish();

	m_renderer.render(m_draw_list, theme().window);
}

void SetupApp::draw_window_controls()
{
	const TitleBarButton hovered_button =
		m_pressed_button != TitleBarButton::None ? m_pressed_button : m_window.title_bar_button_at(m_mouse);

	for (const TitleBarButton button : {TitleBarButton::Minimize, TitleBarButton::Close}) {
		const Rect rect = m_window.title_bar_button_rect(button);
		const bool close = button == TitleBarButton::Close;
		const bool is_hovered = hovered_button == button && rect.contains(m_mouse);

		if (is_hovered) {
			m_draw_list.add_rect(rect, close ? title_bar_close_hover : with_alpha(theme().text, title_bar_hover_alpha));
		}

		Color glyph = theme().text_dim;
		if (is_hovered) {
			glyph = close ? title_bar_close_glyph_hover : theme().text;
		}

		m_draw_list.add_image(rect.centered(control_icon_size, control_icon_size),
							  m_assets.get(close ? Asset::IconClose : Asset::IconMinimize), glyph);
	}
}
