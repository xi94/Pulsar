#pragma once

#include <Windows.h>

#include "core/types.h"

// The single Win32 window, with a custom-drawn title bar in place of the OS caption.
// WS_OVERLAPPEDWINDOW is kept so Aero Snap, the minimize animation and Alt+Tab thumbnails keep
// working; DwmExtendFrameIntoClientArea preserves the drop shadow and rounded corners once
// WM_NCCALCSIZE removes the frame.
//
// DPI awareness runs on a logical/physical split, so nothing above this class reasons about DPI:
// GetWidth, layout constants and input coordinates are all logical, and GetPhysicalWidth exists
// only for the renderer boundary.
//
// The two callbacks are function pointers rather than virtuals because Win32 delivers both
// synchronously - a live border drag runs inside DefWindowProc's own modal loop - so neither can
// be polled once per frame.

enum class EInputEventType : u8 {
	MouseDown,
	MouseUp,
	MouseMove,
	MouseWheel,
	RightMouseUp, // right-click-down is not tracked; nothing needs a right-drag
	KeyDown,	  // a virtual-key code, so a physical key rather than a printable character
	CharTyped,	  // a real typed character
};

struct InputEvent {
	EInputEventType Type;
	float X; // logical pixels
	float Y;
	float WheelDelta;
	u32 KeyCode; // a VK_* code for KeyDown, a UTF-16 code unit for CharTyped
};

constexpr u32 kMaxInputEventsPerFrame = 64;

constexpr float kTitleBarHeight = 40.0f;
constexpr float kTitleBarButtonWidth = 46.0f;

/// The update slot is wider than the rest because it is a labelled pill, not a glyph.
constexpr float kUpdateButtonWidth = 170.0f;

/// The bottom chrome strip. Declared here rather than in the app so CCarousel can derive its
/// own rect from the same constant.
constexpr float kStatusBarHeight = 26.0f;

/// The floor the OS enforces on a resize drag. Below this the title bar's own buttons stop
/// fitting, so no layout below is ever asked for something it cannot produce.
constexpr float kMinWindowWidth = 640.0f;
constexpr float kMinWindowHeight = 440.0f;

enum class ETitleBarButton : u8 {
	None,
	Menu,

	/// Reserved as a real hit-test slot unconditionally, even though the title bar only draws
	/// into it while an update is in flight. Keeping the geometry constant here means CWindow
	/// never has to know CUpdater exists; SetUpdateButtonVisible is what makes the slot fall
	/// through to draggable caption space when there is nothing to show.
	Update,

	Minimize,
	Maximize,
	Close,
};

/// Called synchronously whenever the window needs a frame on screen right now: a live resize
/// drag and a WM_PAINT both run inside their own modal loop, so nothing polled once per frame
/// can answer them. The handler is expected to resize the swapchain if needed and draw.
using RedrawCallback = void (*)(void *pUserData);

/// Called before the WM_SIZE that follows a monitor change, so font atlases can be re-baked at
/// the new scale before anything samples them.
using DpiChangedCallback = void (*)(void *pUserData);

class CWindow {
  public:
	CWindow() = default;
	~CWindow();

	CWindow(const CWindow &) = delete;
	CWindow &operator=(const CWindow &) = delete;

	/// width and height are logical pixels; the real window is sized in physical pixels for
	/// whichever monitor it lands on, so it always looks the same size at any scale factor.
	///
	/// The instance's address must stay stable afterwards - the window procedure stashes `this`
	/// in GWLP_USERDATA - so a CWindow lives on the stack or behind a stable pointer, never
	/// moved.
	///
	/// The window is created hidden. Everything below is already valid when this returns, so a
	/// caller can finish renderer and asset init and draw one real frame before Show makes it
	/// visible; showing immediately is what used to flash an uninitialized backbuffer.
	bool Create(const wchar_t *pTitle, u32 width, u32 height);

	/// Call only once the renderer is up and one real frame has been presented.
	void Show()
	{
		ShowWindow(m_hWnd, SW_SHOW);
	}

	void SetRedrawCallback(RedrawCallback callback, void *pUserData);
	void SetDpiChangedCallback(DpiChangedCallback callback, void *pUserData);

	/// Pumps this thread's message queue - which also dispatches the tray's message-only window
	/// - and refills the input-event queue. The previous frame's events are discarded here, so
	/// a caller reads them once between two calls.
	void PumpMessages();

	HWND GetHandle() const
	{
		return m_hWnd;
	}

	u32 GetWidth() const
	{
		return m_nWidth;
	}

	u32 GetHeight() const
	{
		return m_nHeight;
	}

	u32 GetPhysicalWidth() const
	{
		return m_nPhysicalWidth;
	}

	u32 GetPhysicalHeight() const
	{
		return m_nPhysicalHeight;
	}

	/// Physical over logical; 1.0 at 100% Windows scaling.
	float GetDpiScale() const
	{
		return m_flDpiScale;
	}

	bool ShouldClose() const
	{
		return m_bShouldClose;
	}

	const InputEvent *GetInputEvents() const
	{
		return m_aInputEvents;
	}

	u32 GetInputEventCount() const
	{
		return m_nInputEventCount;
	}

	/// Menu and Update are left-aligned; Minimize, Maximize and Close are right-aligned in that
	/// order.
	Rect GetTitleBarButtonRect(ETitleBarButton button) const;
	ETitleBarButton TitleBarHitTest(float clientX, float clientY) const;

	/// Call once per frame with whatever the widget stack asked for. Applies immediately, so a
	/// frame that changes cursor kind without the mouse moving still looks right, and remembers
	/// the kind so WM_SETCURSOR - which Windows sends on nearly every mouse move and would
	/// otherwise reset to the class default - can keep reapplying it.
	void SetCursorKind(ECursorKind kind);

	/// Kept up to date by WM_NCHITTEST rather than recomputed from a polled mouse position:
	/// once the cursor crosses into that strip it is non-client, so WM_MOUSEMOVE stops arriving
	/// and the polled position goes stale at exactly the moment it matters. The frame loop skips
	/// its SetCursorKind call while this is true so it does not fight the OS's resize arrows.
	bool IsMouseOverResizeBorder() const
	{
		return m_bMouseOverResizeBorder;
	}

	/// False, the default, makes the update slot fall through to plain caption space rather than
	/// leaving a dead undraggable strip next to the menu button.
	void SetUpdateButtonVisible(bool visible)
	{
		m_bUpdateButtonVisible = visible;
	}

	/// While true, a close request hides the window instead of quitting, leaving the tray as the
	/// only way back. Minimizing is unaffected and always goes to the taskbar.
	void SetCloseToTray(bool closeToTray)
	{
		m_bCloseToTray = closeToTray;
	}

	/// Ends the app regardless of close-to-tray, for the paths that genuinely mean exit: the
	/// tray's Exit item and the updater's relaunch.
	void RequestClose()
	{
		m_bShouldClose = true;
	}

	bool IsHidden() const
	{
		return IsWindowVisible(m_hWnd) == FALSE;
	}

	/// Minimized to the taskbar: visible as far as IsHidden is concerned, but with a zero client
	/// size and nothing on screen to present to.
	bool IsMinimized() const
	{
		return IsIconic(m_hWnd) != FALSE;
	}

	/// True for the whole of a border drag or title-bar move. Both run inside a DefWindowProc
	/// modal loop that never returns to the frame loop, so anything with a side effect beyond
	/// drawing - spawning a process, closing the window - has to sit these out.
	bool IsInSizeMove() const
	{
		return m_bInSizeMove;
	}

	void Restore();

	/// Asks an already-running instance to show itself, for a second instance to call before bowing
	/// out. The wait exists because the running instance may still be starting up with no window
	/// yet, and a double-click during that window should still surface it.
	static bool ActivateExistingInstance(u32 timeoutMs = 3000);

  private:
	static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
	LRESULT HandleHitTest(LPARAM lParam);
	LRESULT HandleDpiChanged(WPARAM wParam, LPARAM lParam);
	void HandleSize(LPARAM lParam);
	LRESULT HandleEraseBackground();
	LRESULT HandlePaint();
	LRESULT HandleMinMaxInfo(LPARAM lParam) const;
	LRESULT HandleEnterSizeMove();
	LRESULT HandleExitSizeMove();

	void Redraw();

	bool RegisterWindowClass(HINSTANCE hInstance);
	void CorrectSizeForActualDpi(u32 logicalWidth, u32 logicalHeight);

	void PushInputEvent(const InputEvent &event);
	void PushMouseEvent(EInputEventType type, LPARAM lParam);

	HWND m_hWnd = nullptr;

	u32 m_nWidth = 0;
	u32 m_nHeight = 0;
	u32 m_nPhysicalWidth = 0;
	u32 m_nPhysicalHeight = 0;
	float m_flDpiScale = 1.0f;

	bool m_bShouldClose = false;
	bool m_bUpdateButtonVisible = false;
	bool m_bCloseToTray = false;

	/// SetCapture on button-down keeps move and up events routed here once the cursor leaves the
	/// client area mid-drag. Without it, releasing outside the window sends the up event
	/// elsewhere and any in-progress drag stays stuck pressed forever. The flag and last
	/// position back a WM_CAPTURECHANGED handler that synthesizes the missing up event when
	/// capture is lost some other way - Alt+Tab, a system dialog, WM_CANCELMODE.
	bool m_bMouseCaptured = false;
	float m_flLastMouseX = 0.0f;
	float m_flLastMouseY = 0.0f;

	bool m_bMouseOverResizeBorder = false;

	RedrawCallback m_pRedrawCallback = nullptr;
	void *m_pRedrawCallbackUserData = nullptr;

	// Set for the whole of a border drag or a title-bar move, both of which run inside
	// DefWindowProc modal loops.
	bool m_bInSizeMove = false;

	DpiChangedCallback m_pDpiChangedCallback = nullptr;
	void *m_pDpiChangedCallbackUserData = nullptr;

	ECursorKind m_cursorKind = ECursorKind::Arrow;

	InputEvent m_aInputEvents[kMaxInputEventsPerFrame]{};
	u32 m_nInputEventCount = 0;
};
