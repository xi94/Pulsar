#include "platform/window.h"

#include <cassert>
#include <cmath>

#include <dwmapi.h>
#include <windowsx.h>

#include "core/app_identity.h"
#include "platform/app_icon.h"
#include "platform/resource.h"

namespace {
// A registered message rather than WM_APP+n, so it can never collide with anything else.
UINT ActivateInstanceMessage()
{
	static const UINT message = RegisterWindowMessageW(kActivateInstanceMessageName);

	return message;
}

float ToLogical(float physical, float dpiScale)
{
	return physical / dpiScale;
}

float ToPhysical(float logical, float dpiScale)
{
	return logical * dpiScale;
}

LONG FrameBorderX(UINT dpi)
{
	return GetSystemMetricsForDpi(SM_CXFRAME, dpi) + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
}

LONG FrameBorderY(UINT dpi)
{
	return GetSystemMetricsForDpi(SM_CYFRAME, dpi) + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
}

// A maximized window's rect extends past the visible monitor by design, so the resize border
// lands off-screen. Insetting keeps content inside the work area instead of covering the
// taskbar - which is what Windows does as part of the frame WM_NCCALCSIZE removes. Per-DPI
// rather than the system-wide metric, so this stays right on a scaled monitor.
void ApplyMaximizedInset(HWND hWnd, RECT &rect, UINT dpi)
{
	if (!IsZoomed(hWnd)) return;

	const LONG borderX = FrameBorderX(dpi);
	const LONG borderY = FrameBorderY(dpi);

	rect.left += borderX;
	rect.top += borderY;
	rect.right -= borderX;
	rect.bottom -= borderY;
}

// WM_NCCALCSIZE removes the standard frame entirely, so the OS no longer knows where the
// resize grips are - this reimplements that part of DefWindowProc's job.
LRESULT HitTestResizeEdges(HWND hWnd, POINT cursorScreen, UINT dpi)
{
	if (IsZoomed(hWnd)) return HTNOWHERE;

	RECT windowRect;
	GetWindowRect(hWnd, &windowRect);

	const LONG borderX = FrameBorderX(dpi);
	const LONG borderY = FrameBorderY(dpi);

	const bool atLeft = cursorScreen.x < windowRect.left + borderX;
	const bool atRight = cursorScreen.x >= windowRect.right - borderX;
	const bool atTop = cursorScreen.y < windowRect.top + borderY;
	const bool atBottom = cursorScreen.y >= windowRect.bottom - borderY;

	if (atTop) return atLeft ? HTTOPLEFT : atRight ? HTTOPRIGHT : HTTOP;

	if (atBottom) return atLeft ? HTBOTTOMLEFT : atRight ? HTBOTTOMRIGHT : HTBOTTOM;

	if (atLeft) return HTLEFT;

	if (atRight) return HTRIGHT;

	return HTNOWHERE;
}

HCURSOR CursorForKind(ECursorKind kind)
{
	switch (kind) {
		case ECursorKind::Hand:
			return LoadCursorW(nullptr, IDC_HAND);
		case ECursorKind::IBeam:
			return LoadCursorW(nullptr, IDC_IBEAM);
		case ECursorKind::Drag:
		case ECursorKind::Arrow:
			break;
	}

	return LoadCursorW(nullptr, IDC_ARROW);
}

} // namespace

CWindow::~CWindow()
{
	if (m_hWnd != nullptr) {
		DestroyWindow(m_hWnd);
		m_hWnd = nullptr;
	}
}

bool CWindow::RegisterWindowClass(HINSTANCE hInstance)
{
	const WNDCLASSEXW windowClass{
		.cbSize = sizeof(WNDCLASSEXW),
		.style = CS_HREDRAW | CS_VREDRAW,
		.lpfnWndProc = WindowProc,
		.cbClsExtra = 0,
		.cbWndExtra = 0,
		.hInstance = hInstance,
		.hIcon = LoadAppIcon(EAppIconSize::Large),
		.hCursor = LoadCursorW(nullptr, IDC_ARROW),
		.hbrBackground = nullptr,
		.lpszMenuName = nullptr,
		.lpszClassName = kMainWindowClassName,
		.hIconSm = LoadAppIcon(EAppIconSize::Small),
	};

	return RegisterClassExW(&windowClass) != 0;
}

// CreateWindowExW has now placed the window on a real monitor, so its true DPI is knowable.
// Corrects the physical size in place if the pre-placement guess was wrong, keeping the
// logical size exactly what the caller asked for either way.
void CWindow::CorrectSizeForActualDpi(u32 logicalWidth, u32 logicalHeight)
{
	const float actualDpiScale = static_cast<float>(GetDpiForWindow(m_hWnd)) / 96.0f;
	if (actualDpiScale == m_flDpiScale) return;

	m_flDpiScale = actualDpiScale;

	const auto correctedWidth = static_cast<int>(std::lround(static_cast<float>(logicalWidth) * actualDpiScale));
	const auto correctedHeight = static_cast<int>(std::lround(static_cast<float>(logicalHeight) * actualDpiScale));
	SetWindowPos(m_hWnd, nullptr, 0, 0, correctedWidth, correctedHeight, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

bool CWindow::Create(const wchar_t *pTitle, u32 width, u32 height)
{
	// Per-Monitor-V2: this window is told its true DPI on every monitor and scales itself. The
	// alternative has Windows bitmap-stretch an already-rendered frame, which is exactly the
	// blur a hand-drawn UI should not have. Also declared in app.manifest, which is the
	// authoritative source; this call is the documented runtime fallback.
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	// A guess until the window exists to ask, corrected immediately after creation.
	const float initialDpiScale = static_cast<float>(GetDpiForSystem()) / 96.0f;

	m_nWidth = width;
	m_nHeight = height;
	m_nPhysicalWidth = static_cast<u32>(std::lround(static_cast<float>(width) * initialDpiScale));
	m_nPhysicalHeight = static_cast<u32>(std::lround(static_cast<float>(height) * initialDpiScale));
	m_flDpiScale = initialDpiScale;
	m_bShouldClose = false;

	const HINSTANCE hInstance = GetModuleHandleW(nullptr);
	RegisterWindowClass(hInstance);

	// Centred on the primary monitor's work area rather than CW_USEDEFAULT's cascade position:
	// a first run should open somewhere predictable.
	RECT workArea{};
	SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
	const int windowX = workArea.left + ((workArea.right - workArea.left) - static_cast<int>(m_nPhysicalWidth)) / 2;
	const int windowY = workArea.top + ((workArea.bottom - workArea.top) - static_cast<int>(m_nPhysicalHeight)) / 2;

	// No AdjustWindowRect: WM_NCCALCSIZE makes the client rect equal the window rect outside
	// the maximized case, so the requested size already is the client size. `this` rides along
	// as lpCreateParams so WM_NCCREATE can stash it before any other message arrives.
	m_hWnd = CreateWindowExW(0, kMainWindowClassName, pTitle, WS_OVERLAPPEDWINDOW, windowX, windowY,
							 static_cast<int>(m_nPhysicalWidth), static_cast<int>(m_nPhysicalHeight), nullptr, nullptr,
							 hInstance, this);
	if (m_hWnd == nullptr) return false;

	CorrectSizeForActualDpi(width, height);

	// The one real DWM dependency: keeps the drop shadow and rounded corners now that the
	// standard frame is gone.
	const MARGINS margins{.cxLeftWidth = 0, .cxRightWidth = 0, .cyTopHeight = 1, .cyBottomHeight = 0};
	DwmExtendFrameIntoClientArea(m_hWnd, &margins);

	return true;
}

void CWindow::Restore()
{
	ShowWindow(m_hWnd, SW_SHOW);
	if (IsIconic(m_hWnd)) ShowWindow(m_hWnd, SW_RESTORE);
	SetForegroundWindow(m_hWnd);
}

bool CWindow::ActivateExistingInstance(u32 timeoutMs)
{
	// 64-bit throughout: GetTickCount64 is what makes this immune to the 49-day wrap, and truncating
	// its result back into a DWORD would put the wrap straight back.
	const ULONGLONG deadline = GetTickCount64() + timeoutMs;

	for (;;) {
		const HWND hWnd = FindWindowW(kMainWindowClassName, nullptr);
		if (hWnd != nullptr) {
			DWORD processId = 0;
			GetWindowThreadProcessId(hWnd, &processId);
			if (processId != 0) AllowSetForegroundWindow(processId);

			PostMessageW(hWnd, ActivateInstanceMessage(), 0, 0);

			return true;
		}

		if (GetTickCount64() >= deadline) return false;

		Sleep(100);
	}
}

void CWindow::SetRedrawCallback(RedrawCallback callback, void *pUserData)
{
	m_pRedrawCallback = callback;
	m_pRedrawCallbackUserData = pUserData;
}

void CWindow::Redraw()
{
	if (m_pRedrawCallback != nullptr) m_pRedrawCallback(m_pRedrawCallbackUserData);
}

void CWindow::SetDpiChangedCallback(DpiChangedCallback callback, void *pUserData)
{
	m_pDpiChangedCallback = callback;
	m_pDpiChangedCallbackUserData = pUserData;
}

void CWindow::PumpMessages()
{
	m_nInputEventCount = 0;

	MSG message;
	while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
		if (message.message == WM_QUIT) m_bShouldClose = true;
		TranslateMessage(&message);
		DispatchMessageW(&message);
	}
}

Rect CWindow::GetTitleBarButtonRect(ETitleBarButton button) const
{
	assert(button != ETitleBarButton::None);

	if (button == ETitleBarButton::Menu) return Rect{0.0f, 0.0f, kTitleBarButtonWidth, kTitleBarHeight};
	if (button == ETitleBarButton::Update) return Rect{kTitleBarButtonWidth, 0.0f, kUpdateButtonWidth, kTitleBarHeight};

	float indexFromRight = 0.0f;
	switch (button) {
		case ETitleBarButton::Close:
			indexFromRight = 0.0f;
			break;
		case ETitleBarButton::Maximize:
			indexFromRight = 1.0f;
			break;
		case ETitleBarButton::Minimize:
			indexFromRight = 2.0f;
			break;
		case ETitleBarButton::Menu:
		case ETitleBarButton::Update:
		case ETitleBarButton::None:
			break;
	}

	const float right = static_cast<float>(m_nWidth);

	return Rect{right - (indexFromRight + 1.0f) * kTitleBarButtonWidth, 0.0f, kTitleBarButtonWidth, kTitleBarHeight};
}

ETitleBarButton CWindow::TitleBarHitTest(float clientX, float clientY) const
{
	if (clientY < 0.0f || clientY >= kTitleBarHeight) return ETitleBarButton::None;

	constexpr ETitleBarButton kButtons[]{
		ETitleBarButton::Menu,	   ETitleBarButton::Update,	  ETitleBarButton::Close,
		ETitleBarButton::Maximize, ETitleBarButton::Minimize,
	};

	for (ETitleBarButton button : kButtons) {
		if (button == ETitleBarButton::Update && !m_bUpdateButtonVisible) continue;

		const Rect rect = GetTitleBarButtonRect(button);
		if (clientX >= rect.X && clientX < rect.X + rect.W) return button;
	}

	return ETitleBarButton::None;
}

void CWindow::SetCursorKind(ECursorKind kind)
{
	m_cursorKind = kind;

	// Applied now rather than only stashed for the next WM_SETCURSOR, which fires mainly on
	// mouse movement: a frame where the kind changes under a stationary cursor - a button
	// appearing right beneath it - would otherwise keep the old cursor until the next twitch.
	SetCursor(CursorForKind(kind));
}

void CWindow::PushInputEvent(const InputEvent &event)
{
	// Fixed capacity, silently dropping the overflow rather than growing.
	if (m_nInputEventCount >= kMaxInputEventsPerFrame) return;

	m_aInputEvents[m_nInputEventCount] = event;
	m_nInputEventCount += 1;
}

void CWindow::PushMouseEvent(EInputEventType type, LPARAM lParam)
{
	const float x = ToLogical(static_cast<float>(GET_X_LPARAM(lParam)), m_flDpiScale);
	const float y = ToLogical(static_cast<float>(GET_Y_LPARAM(lParam)), m_flDpiScale);

	m_flLastMouseX = x;
	m_flLastMouseY = y;

	PushInputEvent(InputEvent{type, x, y, 0.0f, 0});
}

// Resize edges first, then the hand-drawn caption strip, else plain client area. Title bar
// buttons fall through to HTCLIENT rather than synthetic HTCLOSE and friends, so
// their clicks flow through the normal input pipeline like everything else.
LRESULT CWindow::HandleHitTest(LPARAM lParam)
{
	const POINT cursorScreen{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
	const LRESULT edgeHit = HitTestResizeEdges(m_hWnd, cursorScreen, GetDpiForWindow(m_hWnd));

	// Recorded here rather than derived from a polled position: once the cursor is over this
	// strip, this is the only message still tracking it.
	m_bMouseOverResizeBorder = edgeHit != HTNOWHERE;

	if (edgeHit != HTNOWHERE) return edgeHit;

	POINT clientPoint = cursorScreen;
	ScreenToClient(m_hWnd, &clientPoint);

	const float logicalX = ToLogical(static_cast<float>(clientPoint.x), m_flDpiScale);
	const float logicalY = ToLogical(static_cast<float>(clientPoint.y), m_flDpiScale);

	const bool overCaption =
		logicalY >= 0.0f && logicalY < kTitleBarHeight && TitleBarHitTest(logicalX, logicalY) == ETitleBarButton::None;

	return overCaption ? HTCAPTION : HTCLIENT;
}

// Fires when the window moves to a monitor with a different scale factor. wParam's high word
// is the new DPI for both axes; lParam is the window rect Windows already computed, applied
// verbatim. The SetWindowPos then delivers WM_SIZE, which drives the repaint like any resize.
LRESULT CWindow::HandleDpiChanged(WPARAM wParam, LPARAM lParam)
{
	m_flDpiScale = static_cast<float>(HIWORD(wParam)) / 96.0f;

	// Re-bake fonts at the new scale before the move below triggers a repaint, so that repaint
	// never samples a stale atlas.
	if (m_pDpiChangedCallback != nullptr) m_pDpiChangedCallback(m_pDpiChangedCallbackUserData);

	const RECT *pSuggested = reinterpret_cast<RECT *>(lParam);
	SetWindowPos(m_hWnd, nullptr, pSuggested->left, pSuggested->top, pSuggested->right - pSuggested->left,
				 pSuggested->bottom - pSuggested->top, SWP_NOZORDER | SWP_NOACTIVATE);

	return 0;
}

void CWindow::HandleSize(LPARAM lParam)
{
	m_nPhysicalWidth = LOWORD(lParam);
	m_nPhysicalHeight = HIWORD(lParam);
	m_nWidth = static_cast<u32>(std::lround(ToLogical(static_cast<float>(m_nPhysicalWidth), m_flDpiScale)));
	m_nHeight = static_cast<u32>(std::lround(ToLogical(static_cast<float>(m_nPhysicalHeight), m_flDpiScale)));

	// Synchronous rather than polled: a border drag runs inside DefWindowProc's own modal loop,
	// so the frame loop gets no turn until the mouse comes up.
	Redraw();
}

// The client area never shows through: every pixel is painted by the redraw below, and letting
// the OS erase to the class brush first is a white flash on every resize step.
LRESULT CWindow::HandleEraseBackground()
{
	return 1;
}

// A frame is drawn straight into the paint cycle, which covers the cases WM_SIZE does not: an
// occluded window being uncovered, and a move that crosses monitors.
LRESULT CWindow::HandlePaint()
{
	PAINTSTRUCT paint;
	BeginPaint(m_hWnd, &paint);
	Redraw();
	EndPaint(m_hWnd, &paint);

	return 0;
}

// Below this the title bar's own buttons stop fitting and every panel is clamped to a size
// nothing is legible at. Windows measures the whole window, which WM_NCCALCSIZE has made equal
// to the client area.
LRESULT CWindow::HandleMinMaxInfo(LPARAM lParam) const
{
	auto *pInfo = reinterpret_cast<MINMAXINFO *>(lParam);
	pInfo->ptMinTrackSize.x = static_cast<LONG>(std::lround(ToPhysical(kMinWindowWidth, m_flDpiScale)));
	pInfo->ptMinTrackSize.y = static_cast<LONG>(std::lround(ToPhysical(kMinWindowHeight, m_flDpiScale)));

	return 0;
}

// Events queued during the drag are dropped at both ends. The queue is only drained by
// PumpMessages, which does not run inside the modal loop, so it would otherwise overflow during
// the drag and then replay a mouse-down from minutes ago the moment the loop exits.
LRESULT CWindow::HandleEnterSizeMove()
{
	m_bInSizeMove = true;
	m_nInputEventCount = 0;

	return 0;
}

LRESULT CWindow::HandleExitSizeMove()
{
	m_bInSizeMove = false;
	m_nInputEventCount = 0;

	return 0;
}

LRESULT CWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == ActivateInstanceMessage()) {
		Restore();
		return 0;
	}

	switch (message) {
		// Removes the standard title bar and frame so this app can draw its own. Both wParam
		// cases matter: the very first calculation at creation fires with FALSE and a plain
		// RECT*, and skipping it leaves the OS caption drawn until the first resize.
		case WM_NCCALCSIZE: {
			RECT *pRect = wParam == TRUE ? &reinterpret_cast<NCCALCSIZE_PARAMS *>(lParam)->rgrc[0]
										 : reinterpret_cast<RECT *>(lParam);
			ApplyMaximizedInset(m_hWnd, *pRect, GetDpiForWindow(m_hWnd));
			return 0;
		}

		case WM_NCHITTEST:
			return HandleHitTest(lParam);

		case WM_CLOSE:
			if (m_bCloseToTray) {
				ShowWindow(m_hWnd, SW_HIDE);
				return 0;
			}

			m_bShouldClose = true;
			return 0;

		// DefWindowProc's answer here is always the window class's static cursor, which would
		// undo SetCursorKind the next time the mouse twitched. Only intercepted for HTCLIENT,
		// so the OS's resize arrows at the window edges still work.
		case WM_SETCURSOR:
			if (LOWORD(lParam) == HTCLIENT) {
				SetCursor(CursorForKind(m_cursorKind));
				return TRUE;
			}

			break;

		case WM_DPICHANGED:
			return HandleDpiChanged(wParam, lParam);

		case WM_SIZE:
			HandleSize(lParam);
			return 0;

		case WM_ERASEBKGND:
			return HandleEraseBackground();

		case WM_PAINT:
			return HandlePaint();

		case WM_GETMINMAXINFO:
			return HandleMinMaxInfo(lParam);

		case WM_ENTERSIZEMOVE:
			return HandleEnterSizeMove();

		case WM_EXITSIZEMOVE:
			return HandleExitSizeMove();

		case WM_LBUTTONDOWN:
			PushMouseEvent(EInputEventType::MouseDown, lParam);
			SetCapture(m_hWnd);
			m_bMouseCaptured = true;
			return 0;

		case WM_LBUTTONUP:
			PushMouseEvent(EInputEventType::MouseUp, lParam);
			// Cleared before ReleaseCapture so the WM_CAPTURECHANGED it synchronously triggers
			// sees a normal release and skips synthesizing a second up event.
			m_bMouseCaptured = false;
			ReleaseCapture();
			return 0;

		case WM_CAPTURECHANGED:
			// Capture was lost some way other than the release above, so synthesize the up
			// event that will now never arrive and leave nothing stuck pressed.
			if (m_bMouseCaptured) {
				m_bMouseCaptured = false;
				PushInputEvent(InputEvent{EInputEventType::MouseUp, m_flLastMouseX, m_flLastMouseY, 0.0f, 0});
			}

			return 0;

		case WM_RBUTTONUP:
			PushMouseEvent(EInputEventType::RightMouseUp, lParam);
			return 0;

		case WM_MOUSEMOVE:
			PushMouseEvent(EInputEventType::MouseMove, lParam);
			return 0;

		// The wheel's coordinates arrive in screen space, unlike every other mouse message.
		case WM_MOUSEWHEEL: {
			POINT cursor{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
			ScreenToClient(m_hWnd, &cursor);

			const float wheelDelta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA;
			PushInputEvent(InputEvent{EInputEventType::MouseWheel,
									  ToLogical(static_cast<float>(cursor.x), m_flDpiScale),
									  ToLogical(static_cast<float>(cursor.y), m_flDpiScale), wheelDelta, 0});
			return 0;
		}

		case WM_KEYDOWN:
			PushInputEvent(InputEvent{EInputEventType::KeyDown, 0.0f, 0.0f, 0.0f, static_cast<u32>(wParam)});
			return 0;

		case WM_CHAR:
			PushInputEvent(InputEvent{EInputEventType::CharTyped, 0.0f, 0.0f, 0.0f, static_cast<u32>(wParam)});
			return 0;

		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;

		default:
			break;
	}

	return DefWindowProcW(m_hWnd, message, wParam, lParam);
}

// m_hWnd is assigned on every message rather than only by Create: WM_NCCREATE, WM_NCCALCSIZE
// and the first WM_SIZE all fire during CreateWindowExW, before it has returned a handle to
// assign. The value is always the one Create assigns moments later, just available sooner.
LRESULT CALLBACK CWindow::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WM_NCCREATE) {
		const auto *pCreate = reinterpret_cast<const CREATESTRUCTW *>(lParam);
		SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreate->lpCreateParams));
	}

	auto *pWindow = reinterpret_cast<CWindow *>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
	if (pWindow == nullptr) return DefWindowProcW(hWnd, message, wParam, lParam);

	pWindow->m_hWnd = hWnd;

	return pWindow->HandleMessage(message, wParam, lParam);
}
