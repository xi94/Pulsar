#include "platform/window.h"

#include <algorithm>
#include <cmath>

#include <dwmapi.h>
#include <windowsx.h>

#include "core/app_identity.h"
#include "platform/app_icon.h"

namespace {
constexpr float     K_DEFAULT_DPI                  = 96.0f;
constexpr ULONGLONG K_ACTIVATE_EXISTING_TIMEOUT_MS = 3000;

[[nodiscard]] auto activate_instance_message() -> UINT
{
	static const UINT MESSAGE = RegisterWindowMessageW(K_ACTIVATE_INSTANCE_MESSAGE_NAME);

	return MESSAGE;
}

[[nodiscard]] auto quit_instance_message() -> UINT
{
	static const UINT MESSAGE = RegisterWindowMessageW(K_QUIT_INSTANCE_MESSAGE_NAME);

	return MESSAGE;
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
}

Window::~Window()
{
	if (m_window != nullptr) {
		DestroyWindow(m_window);
	}
}

auto Window::activate_existing_instance() -> bool
{
	const ULONGLONG deadline = GetTickCount64() + K_ACTIVATE_EXISTING_TIMEOUT_MS;

	for (;;) {
		const HWND existing = FindWindowW(K_MAIN_WINDOW_CLASS_NAME, nullptr);
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

auto Window::register_window_class(HINSTANCE t_instance, const wchar_t* t_class_name) const -> void
{
	const WNDCLASSEXW window_class{
		.cbSize        = sizeof(WNDCLASSEXW),
		.style         = CS_HREDRAW | CS_VREDRAW,
		.lpfnWndProc   = window_proc,
		.hInstance     = t_instance,
		.hIcon         = load_app_icon(AppIconSize::LargeIcon),
		.hCursor       = LoadCursorW(nullptr, IDC_ARROW),
		.lpszClassName = t_class_name,
		.hIconSm       = load_app_icon(AppIconSize::SmallIcon),
	};

	RegisterClassExW(&window_class);
}

auto Window::create(const wchar_t* t_title, u32 t_width, u32 t_height, WindowKind t_kind) -> bool
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
	const wchar_t*  class_name = dialog ? K_SETUP_WINDOW_CLASS_NAME : K_MAIN_WINDOW_CLASS_NAME;
	register_window_class(instance, class_name);

	RECT work_area{};
	SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0);
	const int x = work_area.left + (work_area.right - work_area.left - static_cast<int>(m_physical_width)) / 2;
	const int y = work_area.top + (work_area.bottom - work_area.top - static_cast<int>(m_physical_height)) / 2;

	const DWORD style = dialog ? WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX : WS_OVERLAPPEDWINDOW;
	m_window = CreateWindowExW(0, class_name, t_title, style, x, y, static_cast<int>(m_physical_width), static_cast<int>(m_physical_height), nullptr, nullptr,
	                           instance, this);
	if (m_window == nullptr) return false;

	correct_size_for_actual_dpi(t_width, t_height);

	const MARGINS keep_dwm_shadow{.cxLeftWidth = 0, .cxRightWidth = 0, .cyTopHeight = 1, .cyBottomHeight = 0};
	DwmExtendFrameIntoClientArea(m_window, &keep_dwm_shadow);

	const DWM_WINDOW_CORNER_PREFERENCE corners = DWMWCP_ROUND;
	DwmSetWindowAttribute(m_window, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof(corners));

	return true;
}

auto Window::correct_size_for_actual_dpi(u32 t_width, u32 t_height) -> void
{
	const float actual_scale = GetDpiForWindow(m_window) / K_DEFAULT_DPI;
	if (actual_scale == m_dpi_scale) return;

	m_dpi_scale = actual_scale;
	SetWindowPos(m_window, nullptr, 0, 0, static_cast<int>(scaled(t_width, actual_scale)), static_cast<int>(scaled(t_height, actual_scale)),
	             SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

auto Window::show() -> void
{
	ShowWindow(m_window, SW_SHOW);
}

auto Window::show_minimized() -> void
{
	ShowWindow(m_window, SW_SHOWMINNOACTIVE);
}

auto Window::restored_size() const -> Vec2
{
	WINDOWPLACEMENT placement{.length = sizeof(WINDOWPLACEMENT)};
	if (!GetWindowPlacement(m_window, &placement)) return size();

	const RECT& normal = placement.rcNormalPosition;

	return Vec2{static_cast<float>(normal.right - normal.left) / m_dpi_scale, static_cast<float>(normal.bottom - normal.top) / m_dpi_scale};
}

auto Window::minimize() -> void
{
	ShowWindow(m_window, SW_MINIMIZE);
}

auto Window::restore() -> void
{
	ShowWindow(m_window, SW_SHOW);

	if (IsIconic(m_window)) {
		ShowWindow(m_window, SW_RESTORE);
	}

	SetForegroundWindow(m_window);
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
			m_should_close = true;
		}

		TranslateMessage(&message);
		DispatchMessageW(&message);
	}
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

	SetWindowDisplayAffinity(m_window, t_excluded ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE);
	m_excluded_from_capture = t_excluded;
}

auto Window::title_bar_button_rect(TitleBarButton t_button) const -> Rect
{
	const float right = static_cast<float>(m_width);

	if (m_kind == WindowKind::Dialog) {
		if (t_button == TitleBarButton::Minimize) {
			return Rect{right - K_TITLE_BAR_BUTTON_WIDTH * 2.0f, 0.0f, K_TITLE_BAR_BUTTON_WIDTH, K_TITLE_BAR_HEIGHT};
		}

		if (t_button == TitleBarButton::Close) {
			return Rect{right - K_TITLE_BAR_BUTTON_WIDTH, 0.0f, K_TITLE_BAR_BUTTON_WIDTH, K_TITLE_BAR_HEIGHT};
		}

		return Rect{};
	}

	switch (t_button) {
		case TitleBarButton::Menu:
			return Rect{0.0f, 0.0f, K_TITLE_BAR_BUTTON_WIDTH, K_TITLE_BAR_HEIGHT};
		case TitleBarButton::Search: {
			const float side  = m_update_button_visible ? m_update_button_width + K_SEARCH_BUTTON_MARGIN : 0.0f;
			const float left  = K_TITLE_BAR_BUTTON_WIDTH + std::max(K_SEARCH_BUTTON_SIDE_ROOM, side);
			const float limit = right - K_TITLE_BAR_BUTTON_WIDTH * 3.0f - K_SEARCH_BUTTON_MARGIN;
			const float width = std::min(K_SEARCH_BUTTON_WIDTH, limit - left);
			if (width < K_SEARCH_BUTTON_MIN_WIDTH) return Rect{};

			const float x = std::clamp((right - width) * 0.5f, left, limit - width);

			return Rect{std::floor(x), 0.0f, width, K_TITLE_BAR_HEIGHT};
		}
		case TitleBarButton::Update:
			return Rect{K_TITLE_BAR_BUTTON_WIDTH, 0.0f, m_update_button_width, K_TITLE_BAR_HEIGHT};
		case TitleBarButton::Minimize:
			return Rect{right - K_TITLE_BAR_BUTTON_WIDTH * 3.0f, 0.0f, K_TITLE_BAR_BUTTON_WIDTH, K_TITLE_BAR_HEIGHT};
		case TitleBarButton::Maximize:
			return Rect{right - K_TITLE_BAR_BUTTON_WIDTH * 2.0f, 0.0f, K_TITLE_BAR_BUTTON_WIDTH, K_TITLE_BAR_HEIGHT};
		case TitleBarButton::Close:
			return Rect{right - K_TITLE_BAR_BUTTON_WIDTH, 0.0f, K_TITLE_BAR_BUTTON_WIDTH, K_TITLE_BAR_HEIGHT};
		case TitleBarButton::None:
			break;
	}

	return Rect{};
}

auto Window::title_bar_button_at(Vec2 t_point) const -> TitleBarButton
{
	constexpr TitleBarButton BUTTONS[]{
		TitleBarButton::Menu, TitleBarButton::Search, TitleBarButton::Update, TitleBarButton::Minimize, TitleBarButton::Maximize, TitleBarButton::Close,
	};

	for (const TitleBarButton button : BUTTONS) {
		if (button == TitleBarButton::Update && !m_update_button_visible) continue;
		if (button == TitleBarButton::Search && !m_search_button_visible) continue;

		if (title_bar_button_rect(button).contains(t_point)) return button;
	}

	return TitleBarButton::None;
}

auto Window::to_logical(POINT t_physical) const -> Vec2
{
	return Vec2{t_physical.x / m_dpi_scale, t_physical.y / m_dpi_scale};
}

auto Window::push_input(const InputEvent& t_event) -> void
{
	if (m_input_event_count >= K_MAX_INPUT_EVENTS) return;

	m_input_events[m_input_event_count] = t_event;
	m_input_event_count += 1;
}

auto Window::push_mouse(InputEventType t_type, LPARAM t_lparam) -> void
{
	m_last_mouse = to_logical(POINT{GET_X_LPARAM(t_lparam), GET_Y_LPARAM(t_lparam)});
	push_input(InputEvent{.type = t_type, .position = m_last_mouse});
}

auto Window::push_mouse_at(POINT t_client) -> void
{
	m_last_mouse = to_logical(t_client);
	push_input(InputEvent{.type = InputEventType::MouseMove, .position = m_last_mouse});
}

auto Window::track_mouse_leave(bool t_non_client) -> void
{
	TRACKMOUSEEVENT track{
		.cbSize    = sizeof(TRACKMOUSEEVENT),
		.dwFlags   = TME_LEAVE | (t_non_client ? static_cast<DWORD>(TME_NONCLIENT) : 0u),
		.hwndTrack = m_window,
	};
	TrackMouseEvent(&track);
}

auto Window::handle_mouse_leave() -> void
{
	POINT cursor{};
	GetCursorPos(&cursor);

	// Moving between the title bar and the client area also ends tracking; the next move message carries the position.
	if (WindowFromPoint(cursor) == m_window) return;

	m_last_mouse = Vec2{-1.0f, -1.0f};
	push_input(InputEvent{.type = InputEventType::MouseMove, .position = m_last_mouse});
}

auto Window::handle_hit_test(LPARAM t_lparam) -> LRESULT
{
	const POINT   cursor{GET_X_LPARAM(t_lparam), GET_Y_LPARAM(t_lparam)};
	const LRESULT edge = m_kind == WindowKind::Dialog ? HTNOWHERE : resize_edge_at(m_window, cursor);

	m_mouse_over_resize_border = edge != HTNOWHERE;
	if (m_mouse_over_resize_border) return edge;

	POINT client = cursor;
	ScreenToClient(m_window, &client);

	const Vec2 point        = to_logical(client);
	const bool over_caption = point.y >= 0.0f && point.y < K_TITLE_BAR_HEIGHT && title_bar_button_at(point) == TitleBarButton::None;

	return over_caption ? HTCAPTION : HTCLIENT;
}

auto Window::handle_dpi_changed(WPARAM t_wparam, LPARAM t_lparam) -> void
{
	m_dpi_scale = HIWORD(t_wparam) / K_DEFAULT_DPI;

	if (m_dpi_changed) {
		m_dpi_changed();
	}

	const RECT* suggested = reinterpret_cast<const RECT*>(t_lparam);
	SetWindowPos(m_window, nullptr, suggested->left, suggested->top, suggested->right - suggested->left, suggested->bottom - suggested->top,
	             SWP_NOZORDER | SWP_NOACTIVATE);
}

auto Window::handle_size(WPARAM t_wparam, LPARAM t_lparam) -> void
{
	// Minimizing reports the tiny iconic size, which would otherwise be saved as the window size.
	if (t_wparam == SIZE_MINIMIZED) return;

	m_physical_width  = LOWORD(t_lparam);
	m_physical_height = HIWORD(t_lparam);
	m_width           = scaled(m_physical_width, 1.0f / m_dpi_scale);
	m_height          = scaled(m_physical_height, 1.0f / m_dpi_scale);

	redraw();
}

auto Window::handle_min_max_info(LPARAM t_lparam) const -> void
{
	if (m_kind == WindowKind::Dialog) return;

	auto* info             = reinterpret_cast<MINMAXINFO*>(t_lparam);
	info->ptMinTrackSize.x = std::lround(K_MIN_WINDOW_WIDTH * m_dpi_scale);
	info->ptMinTrackSize.y = std::lround(K_MIN_WINDOW_HEIGHT * m_dpi_scale);
}

auto Window::handle_message(UINT t_message, WPARAM t_wparam, LPARAM t_lparam) -> LRESULT
{
	if (t_message == activate_instance_message()) {
		restore();
		return 0;
	}

	if (t_message == quit_instance_message()) {
		m_should_close = true;
		return 0;
	}

	switch (t_message) {
		case WM_NCCALCSIZE: {
			// The first calculation arrives with wParam FALSE and a bare RECT; skipping it leaves the OS caption drawn.
			RECT* client = t_wparam ? &reinterpret_cast<NCCALCSIZE_PARAMS*>(t_lparam)->rgrc[0] : reinterpret_cast<RECT*>(t_lparam);

			if (IsZoomed(m_window)) {
				const POINT border = frame_border(GetDpiForWindow(m_window));
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
			if (m_close_to_tray) {
				ShowWindow(m_window, SW_HIDE);
			} else {
				m_should_close = true;
			}

			return 0;

		case WM_SETCURSOR:
			if (LOWORD(t_lparam) != HTCLIENT) break;

			SetCursor(system_cursor(m_cursor));
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
			BeginPaint(m_window, &paint);
			redraw();
			EndPaint(m_window, &paint);
			return 0;
		}

		case WM_GETMINMAXINFO:
			handle_min_max_info(t_lparam);
			return 0;

		case WM_ENTERSIZEMOVE:
		case WM_EXITSIZEMOVE:
			m_input_event_count = 0;
			return 0;

		case WM_LBUTTONDOWN:
			push_mouse(InputEventType::MouseDown, t_lparam);
			SetCapture(m_window);
			m_mouse_captured = true;
			return 0;

		case WM_LBUTTONUP:
			push_mouse(InputEventType::MouseUp, t_lparam);
			m_mouse_captured = false;
			ReleaseCapture();
			return 0;

		case WM_CAPTURECHANGED:
			if (m_mouse_captured) {
				m_mouse_captured = false;
				push_input(InputEvent{.type = InputEventType::MouseUp, .position = m_last_mouse});
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
			ScreenToClient(m_window, &cursor);
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
			ScreenToClient(m_window, &cursor);

			push_input(InputEvent{
				.type        = InputEventType::MouseWheel,
				.position    = to_logical(cursor),
				.wheel_delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(t_wparam)) / WHEEL_DELTA,
			});
			return 0;
		}

		case WM_KEYDOWN:
			push_input(InputEvent{.type = InputEventType::KeyDown, .key = static_cast<u32>(t_wparam)});
			return 0;

		case WM_CHAR:
			push_input(InputEvent{.type = InputEventType::Character, .key = static_cast<u32>(t_wparam)});
			return 0;

		case WM_DESTROY:
			// The setup window closes before the main window opens, and a quit message would end that one too.
			if (m_kind == WindowKind::Main) {
				PostQuitMessage(0);
			}

			return 0;

		default:
			break;
	}

	return DefWindowProcW(m_window, t_message, t_wparam, t_lparam);
}

auto CALLBACK Window::window_proc(HWND t_window, UINT t_message, WPARAM t_wparam, LPARAM t_lparam) -> LRESULT
{
	if (t_message == WM_NCCREATE) {
		const auto* create = reinterpret_cast<const CREATESTRUCTW*>(t_lparam);
		SetWindowLongPtrW(t_window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
	}

	auto* window = reinterpret_cast<Window*>(GetWindowLongPtrW(t_window, GWLP_USERDATA));
	if (window == nullptr) return DefWindowProcW(t_window, t_message, t_wparam, t_lparam);

	window->m_window = t_window;

	return window->handle_message(t_message, t_wparam, t_lparam);
}
