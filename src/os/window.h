#pragma once

#include <functional>
#include <memory>
#include <span>
#include <string_view>

#include "core/types.h"
#include "os/input.h"

namespace os {

enum class WindowKind : u8 {
	MAIN,
	DIALOG,
};

// An entry in the macOS application menu. Choosing it arrives as a MENU_ITEM input event carrying `id`; Windows has no such menu.
struct AppMenuItem {
	const char* label;
	Key         shortcut;
	u32         id;
};

class Window {
  public:
	Window();
	~Window();

	Window(const Window&)                    = delete;
	auto operator=(const Window&) -> Window& = delete;

	[[nodiscard]] auto create(std::string_view t_title, u32 t_width, u32 t_height, WindowKind t_kind = WindowKind::MAIN) -> bool;
	auto set_min_size(Vec2 t_size) -> void;
	auto set_title_bar(float t_height, std::function<bool(Vec2)> t_is_button) -> void;
	auto set_app_menu(std::span<const AppMenuItem> t_items) -> void;

	auto show() -> void;
	auto show_minimized() -> void;
	auto minimize() -> void;
	auto toggle_maximized() -> void;
	auto restore() -> void;
	auto bring_to_front() -> void;
	auto close() -> void;
	[[nodiscard]] auto restored_size() const -> Vec2;

	auto on_redraw(std::function<void()> t_callback) -> void;
	auto on_dpi_changed(std::function<void()> t_callback) -> void;
	// Runs when the user logs out or the computer shuts down. The process ends as soon as it returns, so it must save everything itself.
	auto on_session_end(std::function<void()> t_callback) -> void;

	auto pump_messages() -> void;
	[[nodiscard]] auto wait_for_messages(float t_seconds) const -> bool;

	[[nodiscard]] auto input_events() const -> std::span<const InputEvent>
	{
		return {m_input_events, m_input_event_count};
	}

	[[nodiscard]] auto native_handle() const -> void*;

	[[nodiscard]] auto width() const -> u32
	{
		return m_width;
	}

	[[nodiscard]] auto height() const -> u32
	{
		return m_height;
	}

	[[nodiscard]] auto size() const -> Vec2
	{
		return Vec2{static_cast<float>(m_width), static_cast<float>(m_height)};
	}

	[[nodiscard]] auto physical_width() const -> u32
	{
		return m_physical_width;
	}

	[[nodiscard]] auto physical_height() const -> u32
	{
		return m_physical_height;
	}

	[[nodiscard]] auto dpi_scale() const -> float
	{
		return m_dpi_scale;
	}

	[[nodiscard]] auto native_controls_width() const -> float
	{
		return m_native_controls_width;
	}

	[[nodiscard]] auto should_quit() const -> bool
	{
		return m_should_quit;
	}

	auto request_quit() -> void
	{
		m_should_quit = true;
	}

	[[nodiscard]] auto is_hidden() const -> bool;
	[[nodiscard]] auto is_focused() const -> bool;
	[[nodiscard]] auto is_minimized() const -> bool;
	[[nodiscard]] auto is_maximized() const -> bool;

	[[nodiscard]] auto is_mouse_over_resize_border() const -> bool
	{
		return m_mouse_over_resize_border;
	}

	auto set_close_to_tray(bool t_close_to_tray) -> void
	{
		m_close_to_tray = t_close_to_tray;
	}

	auto set_cursor(CursorKind t_cursor) -> void;
	auto set_excluded_from_capture(bool t_excluded) -> void;

  private:
	struct Native;

	static constexpr u32 K_MAX_INPUT_EVENTS = 64;

	auto push_input(const InputEvent& t_event) -> void;
	auto redraw() -> void;

	std::unique_ptr<Native> m_native;
	WindowKind              m_kind = WindowKind::MAIN;

	u32   m_width           = 0;
	u32   m_height          = 0;
	u32   m_physical_width  = 0;
	u32   m_physical_height = 0;
	float m_dpi_scale       = 1.0f;
	Vec2  m_min_size{};

	float                     m_title_bar_height      = 0.0f;
	float                     m_native_controls_width = 0.0f;
	std::function<bool(Vec2)> m_is_title_bar_button;

	bool       m_should_quit              = false;
	bool       m_close_to_tray            = false;
	bool       m_excluded_from_capture    = false;
	bool       m_mouse_over_resize_border = false;
	CursorKind m_cursor                   = CursorKind::ARROW;

	std::function<void()> m_redraw;
	std::function<void()> m_dpi_changed;
	std::function<void()> m_session_end;

	InputEvent m_input_events[K_MAX_INPUT_EVENTS]{};
	u32        m_input_event_count = 0;
};

[[nodiscard]] auto activate_running_instance() -> bool;
[[nodiscard]] auto bring_window_to_front(WindowKind t_kind) -> bool;

}
