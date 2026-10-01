#include "platform/window.h"

#include <algorithm>
#include <cmath>

#include <dwmapi.h>
#include <windowsx.h>

#include "core/app_identity.h"
#include "platform/app_icon.h"

namespace {
constexpr float default_dpi = 96.0f;
constexpr ULONGLONG activate_existing_timeout_ms = 3000;

UINT activate_instance_message()
{
	static const UINT message = RegisterWindowMessageW(activate_instance_message_name);

	return message;
}

UINT quit_instance_message()
{
	static const UINT message = RegisterWindowMessageW(quit_instance_message_name);

	return message;
}

POINT frame_border(UINT t_dpi)
{
	const int padding = GetSystemMetricsForDpi(SM_CXPADDEDBORDER, t_dpi);

	return POINT{GetSystemMetricsForDpi(SM_CXFRAME, t_dpi) + padding, GetSystemMetricsForDpi(SM_CYFRAME, t_dpi) + padding};
}

LRESULT resize_edge_at(HWND t_window, POINT t_cursor)
{
	if (IsZoomed(t_window)) return HTNOWHERE;

	RECT window_rect;
	GetWindowRect(t_window, &window_rect);

	const POINT border = frame_border(GetDpiForWindow(t_window));
	const bool left = t_cursor.x < window_rect.left + border.x;
	const bool right = t_cursor.x >= window_rect.right - border.x;
	const bool top = t_cursor.y < window_rect.top + border.y;
	const bool bottom = t_cursor.y >= window_rect.bottom - border.y;

	if (top) return left ? HTTOPLEFT : right ? HTTOPRIGHT : HTTOP;
	if (bottom) return left ? HTBOTTOMLEFT : right ? HTBOTTOMRIGHT : HTBOTTOM;
	if (left) return HTLEFT;
	if (right) return HTRIGHT;

	return HTNOWHERE;
}

HCURSOR system_cursor(CursorKind t_cursor)
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

u32 scaled(u32 t_value, float t_scale)
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

bool Window::activate_existing_instance()
{
	const ULONGLONG deadline = GetTickCount64() + activate_existing_timeout_ms;

	for (;;) {
		const HWND existing = FindWindowW(main_window_class_name, nullptr);
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

void Window::register_window_class(HINSTANCE t_instance, const wchar_t *t_class_name) const
{
	const WNDCLASSEXW window_class{
		.cbSize = sizeof(WNDCLASSEXW),
		.style = CS_HREDRAW | CS_VREDRAW,
		.lpfnWndProc = window_proc,
		.hInstance = t_instance,
		.hIcon = load_app_icon(AppIconSize::LargeIcon),
		.hCursor = LoadCursorW(nullptr, IDC_ARROW),
		.lpszClassName = t_class_name,
		.hIconSm = load_app_icon(AppIconSize::SmallIcon),
	};

	RegisterClassExW(&window_class);
}

bool Window::create(const wchar_t *t_title, u32 t_width, u32 t_height, WindowKind t_kind)
{
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	m_kind = t_kind;
	m_dpi_scale = GetDpiForSystem() / default_dpi;
	m_width = t_width;
	m_height = t_height;
	m_physical_width = scaled(t_width, m_dpi_scale);
	m_physical_height = scaled(t_height, m_dpi_scale);

	const HINSTANCE instance = GetModuleHandleW(nullptr);
	const bool dialog = t_kind == WindowKind::Dialog;
	const wchar_t *class_name = dialog ? setup_window_class_name : main_window_class_name;
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

void Window::correct_size_for_actual_dpi(u32 t_width, u32 t_height)
{
	const float actual_scale = GetDpiForWindow(m_window) / default_dpi;
	if (actual_scale == m_dpi_scale) return;

	m_dpi_scale = actual_scale;
	SetWindowPos(m_window, nullptr, 0, 0, static_cast<int>(scaled(t_width, actual_scale)), static_cast<int>(scaled(t_height, actual_scale)),
				 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void Window::show()
{
	ShowWindow(m_window, SW_SHOW);
}

void Window::show_minimized()
{
	ShowWindow(m_window, SW_SHOWMINNOACTIVE);
}

Vec2 Window::restored_size() const
{
	WINDOWPLACEMENT placement{.length = sizeof(WINDOWPLACEMENT)};
	if (!GetWindowPlacement(m_window, &placement)) return size();

	const RECT &normal = placement.rcNormalPosition;

	return Vec2{static_cast<float>(normal.right - normal.left) / m_dpi_scale, static_cast<float>(normal.bottom - normal.top) / m_dpi_scale};
}

void Window::minimize()
{
	ShowWindow(m_window, SW_MINIMIZE);
}

void Window::restore()
{
	ShowWindow(m_window, SW_SHOW);

	if (IsIconic(m_window)) {
		ShowWindow(m_window, SW_RESTORE);
	}

	SetForegroundWindow(m_window);
}

void Window::on_redraw(std::function<void()> t_callback)
{
	m_redraw = std::move(t_callback);
}

void Window::on_dpi_changed(std::function<void()> t_callback)
{
	m_dpi_changed = std::move(t_callback);
}

void Window::redraw()
{
	if (m_redraw) {
		m_redraw();
	}
}

void Window::pump_messages()
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

void Window::set_cursor(CursorKind t_cursor)
{
	if (t_cursor == m_cursor) return;

	m_cursor = t_cursor;
	SetCursor(system_cursor(t_cursor));
}

void Window::set_excluded_from_capture(bool t_excluded)
{
	if (t_excluded == m_excluded_from_capture) return;

	SetWindowDisplayAffinity(m_window, t_excluded ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE);
	m_excluded_from_capture = t_excluded;
}

Rect Window::title_bar_button_rect(TitleBarButton t_button) const
{
	const float right = static_cast<float>(m_width);

	if (m_kind == WindowKind::Dialog) {
		if (t_button == TitleBarButton::Minimize) {
			return Rect{right - title_bar_button_width * 2.0f, 0.0f, title_bar_button_width, title_bar_height};
		}

		if (t_button == TitleBarButton::Close) {
			return Rect{right - title_bar_button_width, 0.0f, title_bar_button_width, title_bar_height};
		}

		return Rect{};
	}

	switch (t_button) {
		case TitleBarButton::Menu:
			return Rect{0.0f, 0.0f, title_bar_button_width, title_bar_height};
		case TitleBarButton::Search: {
			const float side = m_update_button_visible ? m_update_button_width + search_button_margin : 0.0f;
			const float left = title_bar_button_width + std::max(search_button_side_room, side);
			const float limit = right - title_bar_button_width * 3.0f - search_button_margin;
			const float width = std::min(search_button_width, limit - left);
			if (width < search_button_min_width) return Rect{};

			const float x = std::clamp((right - width) * 0.5f, left, limit - width);

			return Rect{std::floor(x), 0.0f, width, title_bar_height};
		}
		case TitleBarButton::Update:
			return Rect{title_bar_button_width, 0.0f, m_update_button_width, title_bar_height};
		case TitleBarButton::Minimize:
			return Rect{right - title_bar_button_width * 3.0f, 0.0f, title_bar_button_width, title_bar_height};
		case TitleBarButton::Maximize:
			return Rect{right - title_bar_button_width * 2.0f, 0.0f, title_bar_button_width, title_bar_height};
		case TitleBarButton::Close:
			return Rect{right - title_bar_button_width, 0.0f, title_bar_button_width, title_bar_height};
		case TitleBarButton::None:
			break;
	}

	return Rect{};
}

TitleBarButton Window::title_bar_button_at(Vec2 t_point) const
{
	constexpr TitleBarButton buttons[]{
		TitleBarButton::Menu, TitleBarButton::Search, TitleBarButton::Update, TitleBarButton::Minimize, TitleBarButton::Maximize, TitleBarButton::Close,
	};

	for (const TitleBarButton button : buttons) {
		if (button == TitleBarButton::Update && !m_update_button_visible) continue;
		if (button == TitleBarButton::Search && !m_search_button_visible) continue;

		if (title_bar_button_rect(button).contains(t_point)) return button;
	}

	return TitleBarButton::None;
}

Vec2 Window::to_logical(POINT t_physical) const
{
	return Vec2{t_physical.x / m_dpi_scale, t_physical.y / m_dpi_scale};
}

void Window::push_input(const InputEvent &t_event)
{
	if (m_input_event_count >= max_input_events) return;

	m_input_events[m_input_event_count] = t_event;
	m_input_event_count += 1;
}

void Window::push_mouse(InputEventType t_type, LPARAM t_lparam)
{
	m_last_mouse = to_logical(POINT{GET_X_LPARAM(t_lparam), GET_Y_LPARAM(t_lparam)});
	push_input(InputEvent{.type = t_type, .position = m_last_mouse});
}

LRESULT Window::handle_hit_test(LPARAM t_lparam)
{
	const POINT cursor{GET_X_LPARAM(t_lparam), GET_Y_LPARAM(t_lparam)};
	const LRESULT edge = m_kind == WindowKind::Dialog ? HTNOWHERE : resize_edge_at(m_window, cursor);

	m_mouse_over_resize_border = edge != HTNOWHERE;
	if (m_mouse_over_resize_border) return edge;

	POINT client = cursor;
	ScreenToClient(m_window, &client);

	const Vec2 point = to_logical(client);
	const bool over_caption = point.y >= 0.0f && point.y < title_bar_height && title_bar_button_at(point) == TitleBarButton::None;

	return over_caption ? HTCAPTION : HTCLIENT;
}

void Window::handle_dpi_changed(WPARAM t_wparam, LPARAM t_lparam)
{
	m_dpi_scale = HIWORD(t_wparam) / default_dpi;

	if (m_dpi_changed) {
		m_dpi_changed();
	}

	const RECT *suggested = reinterpret_cast<const RECT *>(t_lparam);
	SetWindowPos(m_window, nullptr, suggested->left, suggested->top, suggested->right - suggested->left, suggested->bottom - suggested->top,
				 SWP_NOZORDER | SWP_NOACTIVATE);
}

void Window::handle_size(WPARAM t_wparam, LPARAM t_lparam)
{
	// Minimizing reports the tiny iconic size, which would otherwise be saved as the window size.
	if (t_wparam == SIZE_MINIMIZED) return;

	m_physical_width = LOWORD(t_lparam);
	m_physical_height = HIWORD(t_lparam);
	m_width = scaled(m_physical_width, 1.0f / m_dpi_scale);
	m_height = scaled(m_physical_height, 1.0f / m_dpi_scale);

	redraw();
}

void Window::handle_min_max_info(LPARAM t_lparam) const
{
	if (m_kind == WindowKind::Dialog) return;

	auto *info = reinterpret_cast<MINMAXINFO *>(t_lparam);
	info->ptMinTrackSize.x = std::lround(min_window_width * m_dpi_scale);
	info->ptMinTrackSize.y = std::lround(min_window_height * m_dpi_scale);
}

LRESULT Window::handle_message(UINT t_message, WPARAM t_wparam, LPARAM t_lparam)
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
			RECT *client = t_wparam ? &reinterpret_cast<NCCALCSIZE_PARAMS *>(t_lparam)->rgrc[0] : reinterpret_cast<RECT *>(t_lparam);

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
			return 0;

		case WM_MOUSEWHEEL: {
			POINT cursor{GET_X_LPARAM(t_lparam), GET_Y_LPARAM(t_lparam)};
			ScreenToClient(m_window, &cursor);

			push_input(InputEvent{
				.type = InputEventType::MouseWheel,
				.position = to_logical(cursor),
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

LRESULT CALLBACK Window::window_proc(HWND t_window, UINT t_message, WPARAM t_wparam, LPARAM t_lparam)
{
	if (t_message == WM_NCCREATE) {
		const auto *create = reinterpret_cast<const CREATESTRUCTW *>(t_lparam);
		SetWindowLongPtrW(t_window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
	}

	auto *window = reinterpret_cast<Window *>(GetWindowLongPtrW(t_window, GWLP_USERDATA));
	if (window == nullptr) return DefWindowProcW(t_window, t_message, t_wparam, t_lparam);

	window->m_window = t_window;

	return window->handle_message(t_message, t_wparam, t_lparam);
}
