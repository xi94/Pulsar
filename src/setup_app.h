#pragma once

#include <chrono>
#include <memory>

#include "core/settings.h"
#include "gfx/assets.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "gfx/renderer.h"
#include "platform/window.h"
#include "ui/setup_screen.h"
#include "ui/truncation_hint.h"

class SetupApp {
  public:
	explicit SetupApp(SetupMode t_mode);

	SetupApp(const SetupApp&)                    = delete;
	auto operator=(const SetupApp&) -> SetupApp& = delete;

	auto run() -> SetupOutcome;

  private:
	[[nodiscard]] auto create() -> bool;
	auto reload_fonts() -> bool;
	auto handle_input(const InputEvent& t_event) -> void;
	auto redraw_while_moving() -> void;
	auto frame() -> void;
	auto render() -> void;
	auto draw_window_controls() -> void;

	SetupMode                m_mode;
	Settings                 m_settings;
	Window                   m_window;
	Renderer                 m_renderer;
	Assets                   m_assets;
	Fonts                    m_fonts;
	Fonts                    m_heading_fonts;
	Fonts                    m_title_fonts;
	DrawList                 m_draw_list;
	std::unique_ptr<Texture> m_app_icon;
	SetupScreen              m_screen;
	TruncationHint           m_truncation_hint;

	Vec2                                  m_mouse{-1.0f, -1.0f};
	TitleBarButton                        m_pressed_button = TitleBarButton::None;
	bool                                  m_pointer_down   = false;
	std::chrono::steady_clock::time_point m_last_frame_time;
	bool                                  m_in_frame = false;
};
