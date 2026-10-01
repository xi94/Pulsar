#pragma once

#include <functional>
#include <span>

#include <Windows.h>

#include "core/types.h"

enum class InputEventType : u8 {
	mouse_down,
	mouse_up,
	mouse_move,
	mouse_wheel,
	right_click,
	key_down,
	character,
};

struct InputEvent {
	InputEventType type;
	Vec2 position;
	float wheel_delta;
	u32 key;
};

enum class TitleBarButton : u8 {
	none,
	menu,
	search,
	update,
	minimize,
	maximize,
	close,
};

constexpr float title_bar_height = 40.0f;
constexpr float title_bar_button_width = 46.0f;
constexpr float update_button_width = 170.0f;
constexpr float search_button_width = 300.0f;
constexpr float search_button_min_width = 150.0f;
constexpr float search_button_side_room = 120.0f;
constexpr float search_button_margin = 16.0f;
constexpr float status_bar_height = 26.0f;
constexpr float min_window_width = 640.0f;
constexpr float min_window_height = 440.0f;

class Window {
  public:
	Window() = default;
	~Window();

	Window(const Window &) = delete;
	Window &operator=(const Window &) = delete;

	static bool activate_existing_instance();

	bool create(const wchar_t *t_title, u32 t_width, u32 t_height);
	void show();
	void restore();

	void on_redraw(std::function<void()> t_callback);
	void on_dpi_changed(std::function<void()> t_callback);

	void pump_messages();

	std::span<const InputEvent> input_events() const
	{
		return {m_input_events, m_input_event_count};
	}

	HWND handle() const
	{
		return m_window;
	}

	u32 width() const
	{
		return m_width;
	}

	u32 height() const
	{
		return m_height;
	}

	Vec2 size() const
	{
		return Vec2{static_cast<float>(m_width), static_cast<float>(m_height)};
	}

	Rect content_rect() const
	{
		const Vec2 window = size();

		return Rect{0.0f, title_bar_height, window.x, std::max(0.0f, window.y - title_bar_height - status_bar_height)};
	}

	u32 physical_width() const
	{
		return m_physical_width;
	}

	u32 physical_height() const
	{
		return m_physical_height;
	}

	float dpi_scale() const
	{
		return m_dpi_scale;
	}

	bool should_close() const
	{
		return m_should_close;
	}

	void request_close()
	{
		m_should_close = true;
	}

	bool wait_for_messages(float t_seconds) const
	{
		const auto milliseconds = static_cast<DWORD>(t_seconds * 1000.0f);
		return MsgWaitForMultipleObjectsEx(0, nullptr, milliseconds, QS_ALLINPUT, MWMO_INPUTAVAILABLE) == WAIT_OBJECT_0;
	}

	bool is_hidden() const
	{
		return !IsWindowVisible(m_window);
	}

	bool is_minimized() const
	{
		return IsIconic(m_window);
	}

	bool is_maximized() const
	{
		return IsZoomed(m_window);
	}

	bool is_mouse_over_resize_border() const
	{
		return m_mouse_over_resize_border;
	}

	void set_close_to_tray(bool t_close_to_tray)
	{
		m_close_to_tray = t_close_to_tray;
	}

	void set_update_button_visible(bool t_visible)
	{
		m_update_button_visible = t_visible;
	}

	void set_search_button_visible(bool t_visible)
	{
		m_search_button_visible = t_visible;
	}

	bool is_search_button_visible() const
	{
		return m_search_button_visible;
	}

	void set_cursor(CursorKind t_cursor);
	void set_excluded_from_capture(bool t_excluded);

	Rect title_bar_button_rect(TitleBarButton t_button) const;
	TitleBarButton title_bar_button_at(Vec2 t_point) const;

  private:
	static constexpr u32 max_input_events = 64;

	static LRESULT CALLBACK window_proc(HWND t_window, UINT t_message, WPARAM t_wparam, LPARAM t_lparam);

	LRESULT handle_message(UINT t_message, WPARAM t_wparam, LPARAM t_lparam);
	LRESULT handle_hit_test(LPARAM t_lparam);
	void handle_dpi_changed(WPARAM t_wparam, LPARAM t_lparam);
	void handle_size(LPARAM t_lparam);
	void handle_min_max_info(LPARAM t_lparam) const;

	void register_window_class(HINSTANCE t_instance) const;
	void correct_size_for_actual_dpi(u32 t_width, u32 t_height);

	Vec2 to_logical(POINT t_physical) const;
	void push_input(const InputEvent &t_event);
	void push_mouse(InputEventType t_type, LPARAM t_lparam);
	void redraw();

	HWND m_window = nullptr;

	u32 m_width = 0;
	u32 m_height = 0;
	u32 m_physical_width = 0;
	u32 m_physical_height = 0;
	float m_dpi_scale = 1.0f;

	bool m_should_close = false;
	bool m_close_to_tray = false;
	bool m_update_button_visible = false;
	bool m_search_button_visible = false;
	bool m_excluded_from_capture = false;
	bool m_mouse_over_resize_border = false;
	bool m_mouse_captured = false;
	Vec2 m_last_mouse{};

	CursorKind m_cursor = CursorKind::arrow;

	std::function<void()> m_redraw;
	std::function<void()> m_dpi_changed;

	InputEvent m_input_events[max_input_events]{};
	u32 m_input_event_count = 0;
};
