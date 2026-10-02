#pragma once

#include <functional>
#include <span>

#include <Windows.h>

#include "core/types.h"

enum class InputEventType : u8 {
	MouseDown,
	MouseUp,
	MouseMove,
	MouseWheel,
	RightClick,
	KeyDown,
	Character,
};

struct InputEvent {
	InputEventType type;
	Vec2           position;
	float          wheel_delta;
	u32            key;
};

enum class TitleBarButton : u8 {
	None,
	Menu,
	Search,
	Update,
	Minimize,
	Maximize,
	Close,
};

enum class WindowKind : u8 {
	Main,
	Dialog,
};

constexpr float K_TITLE_BAR_HEIGHT        = 40.0f;
constexpr float K_TITLE_BAR_BUTTON_WIDTH  = 46.0f;
constexpr float K_UPDATE_BUTTON_WIDTH     = 170.0f;
constexpr float K_SEARCH_BUTTON_WIDTH     = 300.0f;
constexpr float K_SEARCH_BUTTON_MIN_WIDTH = 150.0f;
constexpr float K_SEARCH_BUTTON_SIDE_ROOM = 120.0f;
constexpr float K_SEARCH_BUTTON_MARGIN    = 16.0f;
constexpr float K_STATUS_BAR_HEIGHT       = 26.0f;
constexpr Color K_TITLE_BAR_CLOSE_HOVER{232, 17, 35, 255};
constexpr Color K_TITLE_BAR_CLOSE_GLYPH_HOVER{255, 255, 255, 255};
constexpr u8    K_TITLE_BAR_HOVER_ALPHA = 18;
constexpr float K_MIN_WINDOW_WIDTH      = 640.0f;
constexpr float K_MIN_WINDOW_HEIGHT     = 440.0f;

[[nodiscard]] inline auto is_key_down(int t_virtual_key) -> bool
{
	return (GetKeyState(t_virtual_key) & 0x8000) != 0;
}

class Window {
  public:
	Window() = default;
	~Window();

	Window(const Window&)                    = delete;
	auto operator=(const Window&) -> Window& = delete;

	[[nodiscard]] static auto activate_existing_instance() -> bool;

	[[nodiscard]] auto create(const wchar_t* t_title, u32 t_width, u32 t_height, WindowKind t_kind = WindowKind::Main) -> bool;
	auto minimize() -> void;
	auto show() -> void;
	auto restore() -> void;
	auto show_minimized() -> void;
	[[nodiscard]] auto restored_size() const -> Vec2;

	auto on_redraw(std::function<void()> t_callback) -> void;
	auto on_dpi_changed(std::function<void()> t_callback) -> void;

	auto pump_messages() -> void;

	[[nodiscard]] auto input_events() const -> std::span<const InputEvent>
	{
		return {m_input_events, m_input_event_count};
	}

	[[nodiscard]] auto handle() const -> HWND
	{
		return m_window;
	}

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

	[[nodiscard]] auto content_rect() const -> Rect
	{
		const Vec2 window = size();

		return Rect{0.0f, K_TITLE_BAR_HEIGHT, window.x, std::max(0.0f, window.y - K_TITLE_BAR_HEIGHT - K_STATUS_BAR_HEIGHT)};
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

	[[nodiscard]] auto should_close() const -> bool
	{
		return m_should_close;
	}

	auto request_close() -> void
	{
		m_should_close = true;
	}

	[[nodiscard]] auto wait_for_messages(float t_seconds) const -> bool
	{
		const auto milliseconds = static_cast<DWORD>(t_seconds * 1000.0f);
		return MsgWaitForMultipleObjectsEx(0, nullptr, milliseconds, QS_ALLINPUT, MWMO_INPUTAVAILABLE) == WAIT_OBJECT_0;
	}

	[[nodiscard]] auto is_hidden() const -> bool
	{
		return !IsWindowVisible(m_window);
	}

	[[nodiscard]] auto is_focused() const -> bool
	{
		return GetForegroundWindow() == m_window;
	}

	[[nodiscard]] auto is_minimized() const -> bool
	{
		return IsIconic(m_window);
	}

	[[nodiscard]] auto is_maximized() const -> bool
	{
		return IsZoomed(m_window);
	}

	[[nodiscard]] auto is_mouse_over_resize_border() const -> bool
	{
		return m_mouse_over_resize_border;
	}

	auto set_close_to_tray(bool t_close_to_tray) -> void
	{
		m_close_to_tray = t_close_to_tray;
	}

	auto set_update_button_visible(bool t_visible) -> void
	{
		m_update_button_visible = t_visible;
	}

	auto set_update_button_width(float t_width) -> void
	{
		m_update_button_width = t_width;
	}

	auto set_search_button_visible(bool t_visible) -> void
	{
		m_search_button_visible = t_visible;
	}

	[[nodiscard]] auto is_search_button_visible() const -> bool
	{
		return m_search_button_visible;
	}

	auto set_cursor(CursorKind t_cursor) -> void;
	auto set_excluded_from_capture(bool t_excluded) -> void;

	[[nodiscard]] auto title_bar_button_rect(TitleBarButton t_button) const -> Rect;
	[[nodiscard]] auto title_bar_button_at(Vec2 t_point) const -> TitleBarButton;

  private:
	static constexpr u32 K_MAX_INPUT_EVENTS = 64;

	static auto CALLBACK window_proc(HWND t_window, UINT t_message, WPARAM t_wparam, LPARAM t_lparam) -> LRESULT;

	[[nodiscard]] auto handle_message(UINT t_message, WPARAM t_wparam, LPARAM t_lparam) -> LRESULT;
	[[nodiscard]] auto handle_hit_test(LPARAM t_lparam) -> LRESULT;
	auto handle_dpi_changed(WPARAM t_wparam, LPARAM t_lparam) -> void;
	auto handle_size(WPARAM t_wparam, LPARAM t_lparam) -> void;
	auto handle_min_max_info(LPARAM t_lparam) const -> void;

	auto register_window_class(HINSTANCE t_instance, const wchar_t* t_class_name) const -> void;
	auto correct_size_for_actual_dpi(u32 t_width, u32 t_height) -> void;

	[[nodiscard]] auto to_logical(POINT t_physical) const -> Vec2;
	auto push_input(const InputEvent& t_event) -> void;
	auto push_mouse(InputEventType t_type, LPARAM t_lparam) -> void;
	auto push_mouse_at(POINT t_client) -> void;
	auto track_mouse_leave(bool t_non_client) -> void;
	auto handle_mouse_leave() -> void;
	auto redraw() -> void;

	WindowKind m_kind   = WindowKind::Main;
	HWND       m_window = nullptr;

	u32   m_width           = 0;
	u32   m_height          = 0;
	u32   m_physical_width  = 0;
	u32   m_physical_height = 0;
	float m_dpi_scale       = 1.0f;

	bool  m_should_close             = false;
	bool  m_close_to_tray            = false;
	bool  m_update_button_visible    = false;
	float m_update_button_width      = K_UPDATE_BUTTON_WIDTH;
	bool  m_search_button_visible    = false;
	bool  m_excluded_from_capture    = false;
	bool  m_mouse_over_resize_border = false;
	bool  m_mouse_captured           = false;
	Vec2  m_last_mouse{};

	CursorKind m_cursor = CursorKind::Arrow;

	std::function<void()> m_redraw;
	std::function<void()> m_dpi_changed;

	InputEvent m_input_events[K_MAX_INPUT_EVENTS]{};
	u32        m_input_event_count = 0;
};
