#include "os/window.h"

#include <algorithm>
#include <cmath>

#include <Windows.h>
#include <dwmapi.h>
#include <windowsx.h>

#include "core/app_identity.h"
#include "os/win32/win32.h"

namespace {
constexpr float     K_DEFAULT_DPI                  = 96.0f;
constexpr ULONGLONG K_ACTIVATE_EXISTING_TIMEOUT_MS = 3000;

[[nodiscard]] auto activate_instance_message() -> UINT
{
	static const UINT MESSAGE = RegisterWindowMessageW(os::win32::K_ACTIVATE_INSTANCE_MESSAGE_NAME);

	return MESSAGE;
}

[[nodiscard]] auto quit_instance_message() -> UINT
{
	static const UINT MESSAGE = RegisterWindowMessageW(os::win32::K_QUIT_INSTANCE_MESSAGE_NAME);

	return MESSAGE;
}

[[nodiscard]] auto class_name_of(os::WindowKind t_kind) -> const wchar_t*
{
	return t_kind == os::WindowKind::Dialog ? os::win32::K_SETUP_WINDOW_CLASS_NAME : os::win32::K_MAIN_WINDOW_CLASS_NAME;
}

[[nodiscard]] auto frame_border(UINT t_dpi) -> POINT
{
	const int padding = GetSystemMetricsForDpi(SM_CXPADDEDBORDER, t_dpi);

	return POINT{GetSystemMetricsForDpi(SM_CXFRAME, t_dpi) + padding, GetSystemMetricsForDpi(SM_CYFRAME, t_dpi) + padding};
}

[[nodiscard]] auto resize_edge_at(HWND t_window, POINT t_cursor) -> LRESULT
{
	if (IsZoomed(t_window)) return HTNOWHERE;

	RECT window_rect;
	GetWindowRect(t_window, &window_rect);

	const POINT border = frame_border(GetDpiForWindow(t_window));
	const bool  left   = t_cursor.x < window_rect.left + border.x;
	const bool  right  = t_cursor.x >= window_rect.right - border.x;
	const bool  top    = t_cursor.y < window_rect.top + border.y;
	const bool  bottom = t_cursor.y >= window_rect.bottom - border.y;

	if (top) return left ? HTTOPLEFT : right ? HTTOPRIGHT : HTTOP;
	if (bottom) return left ? HTBOTTOMLEFT : right ? HTBOTTOMRIGHT : HTBOTTOM;
	if (left) return HTLEFT;
	if (right) return HTRIGHT;

	return HTNOWHERE;
}

[[nodiscard]] auto system_cursor(CursorKind t_cursor) -> HCURSOR
{
	switch (t_cursor) {
		case CursorKind::Hand:
			return LoadCursorW(nullptr, IDC_HAND);
		case CursorKind::IBeam:
			return LoadCursorW(nullptr, IDC_IBEAM);
		case CursorKind::Move:
			return LoadCursorW(nullptr, IDC_SIZEALL);
		case CursorKind::Arrow:
		case CursorKind::Drag:
			break;
	}

	return LoadCursorW(nullptr, IDC_ARROW);
}

[[nodiscard]] auto scaled(u32 t_value, float t_scale) -> u32
{
	return static_cast<u32>(std::lround(t_value * t_scale));
}

auto register_window_class(HINSTANCE t_instance, const wchar_t* t_class_name, WNDPROC t_procedure) -> void
{
	const WNDCLASSEXW window_class{
		.cbSize        = sizeof(WNDCLASSEXW),
		.style         = CS_HREDRAW | CS_VREDRAW,
		.lpfnWndProc   = t_procedure,
		.hInstance     = t_instance,
		.hIcon         = os::win32::load_app_icon(os::win32::AppIconSize::LargeIcon),
		.hCursor       = LoadCursorW(nullptr, IDC_ARROW),
		.lpszClassName = t_class_name,
		.hIconSm       = os::win32::load_app_icon(os::win32::AppIconSize::SmallIcon),
	};

	RegisterClassExW(&window_class);
}
}

namespace os {

struct Window::Native {
	Window* owner          = nullptr;
	HWND    window         = nullptr;
	bool    mouse_captured = false;
	Vec2    last_mouse{};
	u16     high_surrogate = 0;

	static auto CALLBACK window_proc(HWND t_window, UINT t_message, WPARAM t_wparam, LPARAM t_lparam) -> LRESULT;

	[[nodiscard]] auto handle_message(UINT t_message, WPARAM t_wparam, LPARAM t_lparam) -> LRESULT;
	[[nodiscard]] auto handle_hit_test(LPARAM t_lparam) const -> LRESULT;
	auto handle_dpi_changed(WPARAM t_wparam, LPARAM t_lparam) const -> void;
	auto handle_size(WPARAM t_wparam, LPARAM t_lparam) const -> void;
	auto handle_min_max_info(LPARAM t_lparam) const -> void;
	auto handle_character(WPARAM t_wparam) -> void;
	auto correct_size_for_actual_dpi(u32 t_width, u32 t_height) const -> void;

	[[nodiscard]] auto to_logical(POINT t_physical) const -> Vec2;
	auto push_mouse(InputEventType t_type, LPARAM t_lparam) -> void;
	auto push_mouse_at(POINT t_client) -> void;
	auto track_mouse_leave(bool t_non_client) const -> void;
	auto handle_mouse_leave() -> void;
};

Window::Window()
	: m_native(std::make_unique<Native>())
{
	m_native->owner = this;
}

Window::~Window()
{
	if (m_native->window != nullptr) {
		DestroyWindow(m_native->window);
	}
}

auto Window::create(std::string_view t_title, u32 t_width, u32 t_height, WindowKind t_kind) -> bool
{
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	m_kind            = t_kind;
	m_dpi_scale       = GetDpiForSystem() / K_DEFAULT_DPI;
	m_width           = t_width;
	m_height          = t_height;
	m_physical_width  = scaled(t_width, m_dpi_scale);
	m_physical_height = scaled(t_height, m_dpi_scale);

	const HINSTANCE instance   = GetModuleHandleW(nullptr);
	const bool      dialog     = t_kind == WindowKind::Dialog;
	const wchar_t*  class_name = class_name_of(t_kind);
	register_window_class(instance, class_name, Native::window_proc);

	RECT work_area{};
	SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0);
	const int x = work_area.left + (work_area.right - work_area.left - static_cast<int>(m_physical_width)) / 2;
	const int y = work_area.top + (work_area.bottom - work_area.top - static_cast<int>(m_physical_height)) / 2;

	const std::wstring title = win32::to_wide(t_title);
	const DWORD        style = dialog ? WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX : WS_OVERLAPPEDWINDOW;

	const HWND window = CreateWindowExW(0, class_name, title.c_str(), style, x, y, static_cast<int>(m_physical_width), static_cast<int>(m_physical_height),
	                                    nullptr, nullptr, instance, m_native.get());
	if (window == nullptr) return false;

	m_native->correct_size_for_actual_dpi(t_width, t_height);

	const MARGINS keep_dwm_shadow{.cxLeftWidth = 0, .cxRightWidth = 0, .cyTopHeight = 1, .cyBottomHeight = 0};
	DwmExtendFrameIntoClientArea(window, &keep_dwm_shadow);

	const DWM_WINDOW_CORNER_PREFERENCE corners = DWMWCP_ROUND;
	DwmSetWindowAttribute(window, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof(corners));

	return true;
}

auto Window::set_min_size(Vec2 t_size) -> void
{
	m_min_size = t_size;
}

auto Window::set_title_bar(float t_height, std::function<bool(Vec2)> t_is_button) -> void
{
	m_title_bar_height    = t_height;
	m_is_title_bar_button = std::move(t_is_button);
}

auto Window::show() -> void
{
	ShowWindow(m_native->window, SW_SHOW);
}

auto Window::show_minimized() -> void
{
	ShowWindow(m_native->window, SW_SHOWMINNOACTIVE);
}

auto Window::minimize() -> void
{
	ShowWindow(m_native->window, SW_MINIMIZE);
}

auto Window::toggle_maximized() -> void
{
	ShowWindow(m_native->window, is_maximized() ? SW_RESTORE : SW_MAXIMIZE);
}

auto Window::restore() -> void
{
	ShowWindow(m_native->window, SW_SHOW);

	if (IsIconic(m_native->window)) {
		ShowWindow(m_native->window, SW_RESTORE);
	}

	SetForegroundWindow(m_native->window);
}

auto Window::bring_to_front() -> void
{
	SetForegroundWindow(m_native->window);
}

auto Window::close() -> void
{
	if (m_close_to_tray) {
		ShowWindow(m_native->window, SW_HIDE);
	} else {
		m_should_quit = true;
	}
}

auto Window::restored_size() const -> Vec2
{
	WINDOWPLACEMENT placement{.length = sizeof(WINDOWPLACEMENT)};
	if (!GetWindowPlacement(m_native->window, &placement)) return size();

	const RECT& normal = placement.rcNormalPosition;

	return Vec2{static_cast<float>(normal.right - normal.left) / m_dpi_scale, static_cast<float>(normal.bottom - normal.top) / m_dpi_scale};
}

auto Window::on_redraw(std::function<void()> t_callback) -> void
{
	m_redraw = std::move(t_callback);
}

auto Window::on_dpi_changed(std::function<void()> t_callback) -> void
{
	m_dpi_changed = std::move(t_callback);
}

auto Window::redraw() -> void
{
	if (m_redraw) {
		m_redraw();
	}
}

auto Window::pump_messages() -> void
{
	m_input_event_count = 0;

	MSG message;
	while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
		if (message.message == WM_QUIT) {
			m_should_quit = true;
		}

		TranslateMessage(&message);
		DispatchMessageW(&message);
	}
}

auto Window::wait_for_messages(float t_seconds) const -> bool
{
	const auto milliseconds = static_cast<DWORD>(t_seconds * 1000.0f);

	return MsgWaitForMultipleObjectsEx(0, nullptr, milliseconds, QS_ALLINPUT, MWMO_INPUTAVAILABLE) == WAIT_OBJECT_0;
}

auto Window::native_handle() const -> void*
{
	return m_native->window;
}

auto Window::is_hidden() const -> bool
{
	return !IsWindowVisible(m_native->window);
}

auto Window::is_focused() const -> bool
{
	return GetForegroundWindow() == m_native->window;
}

auto Window::is_minimized() const -> bool
{
	return IsIconic(m_native->window);
}

auto Window::is_maximized() const -> bool
{
	return IsZoomed(m_native->window);
}

auto Window::set_cursor(CursorKind t_cursor) -> void
{
	if (t_cursor == m_cursor) return;

	m_cursor = t_cursor;
	SetCursor(system_cursor(t_cursor));
}

auto Window::set_excluded_from_capture(bool t_excluded) -> void
{
	if (t_excluded == m_excluded_from_capture) return;

	SetWindowDisplayAffinity(m_native->window, t_excluded ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE);
	m_excluded_from_capture = t_excluded;
}

auto Window::push_input(const InputEvent& t_event) -> void
{
	if (m_input_event_count >= K_MAX_INPUT_EVENTS) return;

	m_input_events[m_input_event_count] = t_event;
	m_input_event_count += 1;
}

auto Window::Native::correct_size_for_actual_dpi(u32 t_width, u32 t_height) const -> void
{
	const float actual_scale = GetDpiForWindow(window) / K_DEFAULT_DPI;
	if (actual_scale == owner->m_dpi_scale) return;

	owner->m_dpi_scale = actual_scale;
	SetWindowPos(window, nullptr, 0, 0, static_cast<int>(scaled(t_width, actual_scale)), static_cast<int>(scaled(t_height, actual_scale)),
	             SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

auto Window::Native::to_logical(POINT t_physical) const -> Vec2
{
	return Vec2{t_physical.x / owner->m_dpi_scale, t_physical.y / owner->m_dpi_scale};
}

auto Window::Native::push_mouse(InputEventType t_type, LPARAM t_lparam) -> void
{
	last_mouse = to_logical(POINT{GET_X_LPARAM(t_lparam), GET_Y_LPARAM(t_lparam)});
	owner->push_input(InputEvent{.type = t_type, .position = last_mouse});
}

auto Window::Native::push_mouse_at(POINT t_client) -> void
{
	last_mouse = to_logical(t_client);
	owner->push_input(InputEvent{.type = InputEventType::MouseMove, .position = last_mouse});
}

auto Window::Native::track_mouse_leave(bool t_non_client) const -> void
{
	TRACKMOUSEEVENT track{
		.cbSize    = sizeof(TRACKMOUSEEVENT),
		.dwFlags   = TME_LEAVE | (t_non_client ? static_cast<DWORD>(TME_NONCLIENT) : 0u),
		.hwndTrack = window,
	};
	TrackMouseEvent(&track);
}

auto Window::Native::handle_mouse_leave() -> void
{
	POINT cursor{};
	GetCursorPos(&cursor);

	// Moving between the title bar and the client area also ends tracking; the next move message carries the position.
	if (WindowFromPoint(cursor) == window) return;

	last_mouse = Vec2{-1.0f, -1.0f};
	owner->push_input(InputEvent{.type = InputEventType::MouseMove, .position = last_mouse});
}

auto Window::Native::handle_hit_test(LPARAM t_lparam) const -> LRESULT
{
	const POINT   cursor{GET_X_LPARAM(t_lparam), GET_Y_LPARAM(t_lparam)};
	const LRESULT edge = owner->m_kind == WindowKind::Dialog ? HTNOWHERE : resize_edge_at(window, cursor);

	owner->m_mouse_over_resize_border = edge != HTNOWHERE;
	if (owner->m_mouse_over_resize_border) return edge;

	POINT client = cursor;
	ScreenToClient(window, &client);

	const Vec2 point        = to_logical(client);
	const bool over_button  = owner->m_is_title_bar_button && owner->m_is_title_bar_button(point);
	const bool over_caption = point.y >= 0.0f && point.y < owner->m_title_bar_height && !over_button;

	return over_caption ? HTCAPTION : HTCLIENT;
}

auto Window::Native::handle_dpi_changed(WPARAM t_wparam, LPARAM t_lparam) const -> void
{
	owner->m_dpi_scale = HIWORD(t_wparam) / K_DEFAULT_DPI;

	if (owner->m_dpi_changed) {
		owner->m_dpi_changed();
	}

	const RECT* suggested = reinterpret_cast<const RECT*>(t_lparam);
	SetWindowPos(window, nullptr, suggested->left, suggested->top, suggested->right - suggested->left, suggested->bottom - suggested->top,
	             SWP_NOZORDER | SWP_NOACTIVATE);
}

auto Window::Native::handle_size(WPARAM t_wparam, LPARAM t_lparam) const -> void
{
	// Minimizing reports the tiny iconic size, which would otherwise be saved as the window size.
	if (t_wparam == SIZE_MINIMIZED) return;

	owner->m_physical_width  = LOWORD(t_lparam);
	owner->m_physical_height = HIWORD(t_lparam);
	owner->m_width           = scaled(owner->m_physical_width, 1.0f / owner->m_dpi_scale);
	owner->m_height          = scaled(owner->m_physical_height, 1.0f / owner->m_dpi_scale);

	owner->redraw();
}

auto Window::Native::handle_min_max_info(LPARAM t_lparam) const -> void
{
	if (owner->m_kind == WindowKind::Dialog) return;

	auto* info             = reinterpret_cast<MINMAXINFO*>(t_lparam);
	info->ptMinTrackSize.x = std::lround(owner->m_min_size.x * owner->m_dpi_scale);
	info->ptMinTrackSize.y = std::lround(owner->m_min_size.y * owner->m_dpi_scale);
}

auto Window::Native::handle_character(WPARAM t_wparam) -> void
{
	const auto unit = static_cast<u32>(t_wparam);

	if (unit >= 0xD800 && unit <= 0xDBFF) {
		high_surrogate = static_cast<u16>(unit);
		return;
	}

	u32 codepoint = unit;

	if (unit >= 0xDC00 && unit <= 0xDFFF) {
		if (high_surrogate == 0) return;

		codepoint = 0x10000 + ((static_cast<u32>(high_surrogate) - 0xD800) << 10) + (unit - 0xDC00);
	}

	high_surrogate = 0;
	owner->push_input(InputEvent{.type = InputEventType::Character, .codepoint = codepoint});
}

auto Window::Native::handle_message(UINT t_message, WPARAM t_wparam, LPARAM t_lparam) -> LRESULT
{
	if (t_message == activate_instance_message()) {
		owner->restore();
		return 0;
	}

	if (t_message == quit_instance_message()) {
		owner->m_should_quit = true;
		return 0;
	}

	switch (t_message) {
		case WM_NCCALCSIZE: {
			// The first calculation arrives with wParam FALSE and a bare RECT; skipping it leaves the OS caption drawn.
			RECT* client = t_wparam ? &reinterpret_cast<NCCALCSIZE_PARAMS*>(t_lparam)->rgrc[0] : reinterpret_cast<RECT*>(t_lparam);

			if (IsZoomed(window)) {
				const POINT border = frame_border(GetDpiForWindow(window));
				client->left += border.x;
				client->top += border.y;
				client->right -= border.x;
				client->bottom -= border.y;
			}

			return 0;
		}

		case WM_NCHITTEST:
			return handle_hit_test(t_lparam);

		case WM_CLOSE:
			owner->close();
			return 0;

		case WM_SETCURSOR:
			if (LOWORD(t_lparam) != HTCLIENT) break;

			SetCursor(system_cursor(owner->m_cursor));
			return TRUE;

		case WM_DPICHANGED:
			handle_dpi_changed(t_wparam, t_lparam);
			return 0;

		case WM_SIZE:
			handle_size(t_wparam, t_lparam);
			return 0;

		case WM_ERASEBKGND:
			return 1;

		case WM_PAINT: {
			PAINTSTRUCT paint;
			BeginPaint(window, &paint);
			owner->redraw();
			EndPaint(window, &paint);
			return 0;
		}

		case WM_GETMINMAXINFO:
			handle_min_max_info(t_lparam);
			return 0;

		case WM_ENTERSIZEMOVE:
		case WM_EXITSIZEMOVE:
			owner->m_input_event_count = 0;
			return 0;

		case WM_LBUTTONDOWN:
			push_mouse(InputEventType::MouseDown, t_lparam);
			SetCapture(window);
			mouse_captured = true;
			return 0;

		case WM_LBUTTONUP:
			push_mouse(InputEventType::MouseUp, t_lparam);
			mouse_captured = false;
			ReleaseCapture();
			return 0;

		case WM_CAPTURECHANGED:
			if (mouse_captured) {
				mouse_captured = false;
				owner->push_input(InputEvent{.type = InputEventType::MouseUp, .position = last_mouse});
			}

			return 0;

		case WM_RBUTTONUP:
			push_mouse(InputEventType::RightClick, t_lparam);
			return 0;

		case WM_MOUSEMOVE:
			push_mouse(InputEventType::MouseMove, t_lparam);
			track_mouse_leave(false);
			return 0;

		case WM_NCMOUSEMOVE: {
			POINT cursor{GET_X_LPARAM(t_lparam), GET_Y_LPARAM(t_lparam)};
			ScreenToClient(window, &cursor);
			push_mouse_at(cursor);
			track_mouse_leave(true);
			break;
		}

		case WM_MOUSELEAVE:
			handle_mouse_leave();
			return 0;

		case WM_NCMOUSELEAVE:
			handle_mouse_leave();
			break;

		case WM_MOUSEWHEEL: {
			POINT cursor{GET_X_LPARAM(t_lparam), GET_Y_LPARAM(t_lparam)};
			ScreenToClient(window, &cursor);

			owner->push_input(InputEvent{
				.type        = InputEventType::MouseWheel,
				.position    = to_logical(cursor),
				.wheel_delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(t_wparam)) / WHEEL_DELTA,
			});
			return 0;
		}

		case WM_KEYDOWN:
			owner->push_input(InputEvent{.type = InputEventType::KeyDown, .key = win32::key_from_virtual_key(t_wparam)});
			return 0;

		case WM_CHAR:
			handle_character(t_wparam);
			return 0;

		case WM_DESTROY:
			// The setup window closes before the main window opens, and a quit message would end that one too.
			if (owner->m_kind == WindowKind::Main) {
				PostQuitMessage(0);
			}

			return 0;

		default:
			break;
	}

	return DefWindowProcW(window, t_message, t_wparam, t_lparam);
}

auto CALLBACK Window::Native::window_proc(HWND t_window, UINT t_message, WPARAM t_wparam, LPARAM t_lparam) -> LRESULT
{
	if (t_message == WM_NCCREATE) {
		const auto* create = reinterpret_cast<const CREATESTRUCTW*>(t_lparam);
		SetWindowLongPtrW(t_window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
	}

	auto* native = reinterpret_cast<Native*>(GetWindowLongPtrW(t_window, GWLP_USERDATA));
	if (native == nullptr) return DefWindowProcW(t_window, t_message, t_wparam, t_lparam);

	native->window = t_window;

	return native->handle_message(t_message, t_wparam, t_lparam);
}

auto activate_running_instance() -> bool
{
	const ULONGLONG deadline = GetTickCount64() + K_ACTIVATE_EXISTING_TIMEOUT_MS;

	for (;;) {
		const HWND existing = FindWindowW(os::win32::K_MAIN_WINDOW_CLASS_NAME, nullptr);
		if (existing != nullptr) {
			DWORD process_id = 0;
			GetWindowThreadProcessId(existing, &process_id);
			AllowSetForegroundWindow(process_id);
			PostMessageW(existing, activate_instance_message(), 0, 0);

			return true;
		}

		if (GetTickCount64() >= deadline) return false;

		Sleep(100);
	}
}

auto bring_window_to_front(WindowKind t_kind) -> bool
{
	const HWND window = FindWindowW(class_name_of(t_kind), nullptr);
	if (window == nullptr) return false;

	if (IsIconic(window)) {
		ShowWindow(window, SW_RESTORE);
	}

	SetForegroundWindow(window);

	return true;
}

}
