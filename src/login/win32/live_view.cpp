#include "login/win32/live_view.h"

#include <atomic>
#include <format>
#include <limits>
#include <mutex>
#include <thread>

#include <Windows.h>

#include "core/debug_log.h"
#include "login/win32/client_windows.h"

using LiveViewClock = std::chrono::steady_clock;

struct LiveViewState {
	std::mutex   mutex;
	std::wstring status;
	HBITMAP      frame = nullptr;
	SIZE         frame_size{};

	std::atomic<u32>                focus_given_back{0};
	std::atomic<LiveViewClock::rep> close_at{std::numeric_limits<LiveViewClock::rep>::max()};
	std::atomic<bool>               closed{false};
	const LiveViewClock::time_point opened = LiveViewClock::now();

	LiveViewState() = default;

	~LiveViewState()
	{
		if (frame != nullptr) {
			DeleteObject(frame);
		}
	}

	LiveViewState(const LiveViewState&)                    = delete;
	auto operator=(const LiveViewState&) -> LiveViewState& = delete;
};

namespace {
constexpr const char*    K_LOG_CATEGORY     = "riot";
constexpr const wchar_t* K_WINDOW_CLASS     = L"PulsarLiveView";
constexpr const wchar_t* K_WINDOW_TITLE     = L"Pulsar - live view of the hidden Riot Client";
constexpr int            K_WIDTH            = 660;
constexpr int            K_STATUS_BAND      = 48;
constexpr int            K_SCREEN_MARGIN    = 16;
constexpr int            K_DEFAULT_SOURCE_W = 1552;
constexpr int            K_DEFAULT_SOURCE_H = 872;
constexpr UINT           K_REPAINT_TIMER_MS = 100;
constexpr auto           K_CAPTURE_INTERVAL = std::chrono::milliseconds(120);
constexpr COLORREF       K_BAND_COLOR       = RGB(22, 22, 28);
constexpr COLORREF       K_STATUS_COLOR     = RGB(245, 245, 245);
constexpr COLORREF       K_DETAIL_COLOR     = RGB(160, 195, 255);
constexpr COLORREF       K_WAITING_COLOR    = RGB(200, 200, 200);

[[nodiscard]] auto is_past_close_time(const LiveViewState& t_state) -> bool
{
	return LiveViewClock::now().time_since_epoch().count() >= t_state.close_at.load(std::memory_order_relaxed);
}

[[nodiscard]] auto scaled(int t_pixels) -> int
{
	return MulDiv(t_pixels, static_cast<int>(GetDpiForSystem()), USER_DEFAULT_SCREEN_DPI);
}

auto capture_client(const std::shared_ptr<LiveViewState>& t_state) -> void
{
	const HWND source = main_client_window();
	RECT       bounds{};
	if (source == nullptr || IsIconic(source) || !GetWindowRect(source, &bounds)) return;

	const int width  = bounds.right - bounds.left;
	const int height = bounds.bottom - bounds.top;
	if (width <= 0 || height <= 0) return;

	const HDC     screen   = GetDC(nullptr);
	const HDC     memory   = CreateCompatibleDC(screen);
	const HBITMAP bitmap   = CreateCompatibleBitmap(screen, width, height);
	const HGDIOBJ previous = SelectObject(memory, bitmap);
	const bool    printed  = PrintWindow(source, memory, PW_RENDERFULLCONTENT) != FALSE;
	SelectObject(memory, previous);
	DeleteDC(memory);
	ReleaseDC(nullptr, screen);

	if (!printed) {
		DeleteObject(bitmap);
		return;
	}

	const std::lock_guard lock(t_state->mutex);

	if (t_state->frame != nullptr) {
		DeleteObject(t_state->frame);
	}

	t_state->frame      = bitmap;
	t_state->frame_size = SIZE{width, height};
}

auto run_capture(std::shared_ptr<LiveViewState> t_state) -> void
{
	while (!t_state->closed.load(std::memory_order_relaxed)) {
		capture_client(t_state);
		std::this_thread::sleep_for(K_CAPTURE_INTERVAL);
	}
}

auto draw_frame(LiveViewState* t_state, HDC t_target, HDC t_reference, RECT t_area) -> bool
{
	const std::lock_guard lock(t_state->mutex);
	if (t_state->frame == nullptr || t_state->frame_size.cx <= 0 || t_state->frame_size.cy <= 0) return false;

	const HDC     frame_dc = CreateCompatibleDC(t_reference);
	const HGDIOBJ previous = SelectObject(frame_dc, t_state->frame);
	SetStretchBltMode(t_target, HALFTONE);
	SetBrushOrgEx(t_target, 0, 0, nullptr);
	StretchBlt(t_target, t_area.left, t_area.top, t_area.right - t_area.left, t_area.bottom - t_area.top, frame_dc, 0, 0, t_state->frame_size.cx, t_state->frame_size.cy, SRCCOPY);
	SelectObject(frame_dc, previous);
	DeleteDC(frame_dc);

	return true;
}

auto draw_text_line(HDC t_target, const std::wstring& t_text, int t_font_height, int t_weight, COLORREF t_color, RECT t_area) -> void
{
	const HFONT   font     = CreateFontW(-t_font_height, 0, 0, 0, t_weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
	const HGDIOBJ previous = SelectObject(t_target, font);
	SetTextColor(t_target, t_color);
	DrawTextW(t_target, t_text.c_str(), -1, &t_area, DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER);
	SelectObject(t_target, previous);
	DeleteObject(font);
}

auto paint(LiveViewState* t_state, HWND t_window, HDC t_target) -> void
{
	RECT client{};
	GetClientRect(t_window, &client);

	const int     band          = scaled(K_STATUS_BAND);
	const int     inset         = scaled(10);
	const HDC     back          = CreateCompatibleDC(t_target);
	const HBITMAP back_bitmap   = CreateCompatibleBitmap(t_target, client.right, client.bottom);
	const HGDIOBJ previous_back = SelectObject(back, back_bitmap);
	FillRect(back, &client, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
	SetBkMode(back, TRANSPARENT);

	const RECT mirror{0, band, client.right, client.bottom};
	if (!draw_frame(t_state, back, t_target, mirror)) {
		draw_text_line(back, L"Waiting for the Riot Client's window...", scaled(13), FW_NORMAL, K_WAITING_COLOR, RECT{inset, band, client.right - inset, client.bottom});
	}

	const RECT   status_band{0, 0, client.right, band};
	const HBRUSH band_brush = CreateSolidBrush(K_BAND_COLOR);
	FillRect(back, &status_band, band_brush);
	DeleteObject(band_brush);

	std::wstring status;
	{
		const std::lock_guard lock(t_state->mutex);
		status = t_state->status;
	}

	const double seconds = std::chrono::duration<double>(LiveViewClock::now() - t_state->opened).count();
	const u32    returns = t_state->focus_given_back.load(std::memory_order_relaxed);
	const auto   detail  = std::format(L"Focus taken and given back {} time{}   |   {:.1f}s", returns, returns == 1 ? L"" : L"s", seconds);

	draw_text_line(back, status, scaled(15), FW_SEMIBOLD, K_STATUS_COLOR, RECT{inset, scaled(4), client.right - inset, scaled(25)});
	draw_text_line(back, detail, scaled(13), FW_NORMAL, K_DETAIL_COLOR, RECT{inset, scaled(25), client.right - inset, scaled(45)});

	BitBlt(t_target, 0, 0, client.right, client.bottom, back, 0, 0, SRCCOPY);
	SelectObject(back, previous_back);
	DeleteObject(back_bitmap);
	DeleteDC(back);
}

auto CALLBACK live_view_proc(HWND t_window, UINT t_message, WPARAM t_wparam, LPARAM t_lparam) -> LRESULT
{
	auto* state = reinterpret_cast<LiveViewState*>(GetWindowLongPtrW(t_window, GWLP_USERDATA));

	switch (t_message) {
		case WM_NCCREATE: {
			SetWindowLongPtrW(t_window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(t_lparam)->lpCreateParams));
			break;
		}

		case WM_TIMER: {
			if (state != nullptr && is_past_close_time(*state)) {
				DestroyWindow(t_window);
			} else {
				InvalidateRect(t_window, nullptr, FALSE);
			}

			return 0;
		}

		case WM_MOUSEACTIVATE: {
			return MA_NOACTIVATE;
		}

		case WM_ERASEBKGND: {
			return 1;
		}

		case WM_PAINT: {
			PAINTSTRUCT painting;
			const HDC   target = BeginPaint(t_window, &painting);
			if (state != nullptr) {
				paint(state, t_window, target);
			}
			EndPaint(t_window, &painting);
			return 0;
		}

		case WM_DESTROY: {
			PostQuitMessage(0);
			return 0;
		}

		default: {
			break;
		}
	}

	return DefWindowProcW(t_window, t_message, t_wparam, t_lparam);
}

[[nodiscard]] auto window_rect_in_corner() -> RECT
{
	const HWND source = main_client_window();
	RECT       bounds{};
	int        source_width  = K_DEFAULT_SOURCE_W;
	int        source_height = K_DEFAULT_SOURCE_H;

	if (source != nullptr && !IsIconic(source) && GetWindowRect(source, &bounds) && bounds.right - bounds.left > 100 && bounds.bottom - bounds.top > 100) {
		source_width  = bounds.right - bounds.left;
		source_height = bounds.bottom - bounds.top;
	}

	const int width  = scaled(K_WIDTH);
	const int height = MulDiv(width, source_height, source_width) + scaled(K_STATUS_BAND);

	RECT work{};
	SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);

	const int margin = scaled(K_SCREEN_MARGIN);

	return RECT{work.right - width - margin, work.bottom - height - margin, work.right - margin, work.bottom - margin};
}

auto run_window(std::shared_ptr<LiveViewState> t_state) -> void
{
	const HINSTANCE instance = GetModuleHandleW(nullptr);

	WNDCLASSW window_class{
		.lpfnWndProc   = live_view_proc,
		.hInstance     = instance,
		.hCursor       = LoadCursorW(nullptr, IDC_ARROW),
		.lpszClassName = K_WINDOW_CLASS,
	};
	RegisterClassW(&window_class);

	constexpr DWORD K_STYLE    = WS_POPUP | WS_BORDER;
	constexpr DWORD K_EX_STYLE = WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW;

	const RECT placed = window_rect_in_corner();
	const HWND window = CreateWindowExW(K_EX_STYLE, K_WINDOW_CLASS, K_WINDOW_TITLE, K_STYLE, placed.left, placed.top, placed.right - placed.left, placed.bottom - placed.top, nullptr, nullptr, instance, t_state.get());

	if (window == nullptr) {
		debug_log::write(K_LOG_CATEGORY, "the live view window could not be created (err=%lu)", GetLastError());
		t_state->closed.store(true, std::memory_order_relaxed);
		return;
	}

	SetTimer(window, 1, K_REPAINT_TIMER_MS, nullptr);
	ShowWindow(window, SW_SHOWNOACTIVATE);
	std::thread(run_capture, t_state).detach();

	MSG message;
	while (GetMessageW(&message, nullptr, 0, 0) > 0) {
		TranslateMessage(&message);
		DispatchMessageW(&message);
	}

	t_state->closed.store(true, std::memory_order_relaxed);
}
}

LiveView::~LiveView()
{
	if (m_state == nullptr) return;

	auto never = std::numeric_limits<LiveViewClock::rep>::max();
	m_state->close_at.compare_exchange_strong(never, LiveViewClock::now().time_since_epoch().count());
}

auto LiveView::open() -> void
{
	if (m_state != nullptr) return;

	m_state = std::make_shared<LiveViewState>();
	std::thread(run_window, m_state).detach();
}

auto LiveView::set_status(std::wstring t_status) -> void
{
	if (m_state == nullptr) return;

	const std::lock_guard lock(m_state->mutex);
	m_state->status = std::move(t_status);
}

auto LiveView::count_focus_given_back() -> void
{
	if (m_state != nullptr) {
		m_state->focus_given_back.fetch_add(1, std::memory_order_relaxed);
	}
}

auto LiveView::close_after(std::chrono::milliseconds t_delay) -> void
{
	if (m_state != nullptr) {
		m_state->close_at.store((LiveViewClock::now() + t_delay).time_since_epoch().count(), std::memory_order_relaxed);
	}
}
