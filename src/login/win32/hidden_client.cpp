#include "login/win32/hidden_client.h"

#include <algorithm>
#include <chrono>
#include <string>

#include <dwmapi.h>
#include <TlHelp32.h>

#include "core/debug_log.h"
#include "login/win32/client_windows.h"
#include "os/win32/win32.h"

namespace {
constexpr const char*    K_LOG_CATEGORY      = "riot";
constexpr const wchar_t* K_CONTROL_CLASS     = L"PulsarHiddenClientControl";
constexpr const wchar_t* K_LAUNCHER_NAME     = L"RiotClientServices.exe";
constexpr const wchar_t* K_SHELL_PROCESSES[] = {L"explorer.exe", L"StartMenuExperienceHost.exe", L"SearchHost.exe", L"SearchApp.exe", L"ShellExperienceHost.exe", L"ShellHost.exe"};

constexpr UINT K_HIDE_OPEN_WINDOWS_MESSAGE = WM_APP + 1;
constexpr UINT K_WATCH_FOR_USER_MESSAGE    = WM_APP + 2;
constexpr UINT K_HAND_BACK_MESSAGE         = WM_APP + 3;

constexpr UINT_PTR K_LAUNCH_CHECK_TIMER      = 1;
constexpr UINT     K_LAUNCH_CHECK_MS         = 250;
constexpr DWORD    K_HOOK_START_TIMEOUT_MS   = 3000;
constexpr UINT     K_HOOK_THREAD_TIMEOUT_MS  = 5000;
constexpr u64      K_USER_WATCH_DELAY_MS     = 1000;
constexpr u64      K_OWN_ACTION_GRACE_MS     = 750;
constexpr auto     K_LIVE_VIEW_LINGER        = std::chrono::milliseconds(3000);
constexpr LONG_PTR K_HIDDEN_EX_STYLE_BITS    = WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW;
constexpr UINT     K_KEEP_PLACE              = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER;
constexpr POINT    K_OFF_SCREEN_MINIMIZED_AT = {-32000, -32000};

constexpr usize K_CRASH_SLOTS = 8;

std::atomic<HiddenClient*> g_hidden_client{nullptr};

// Read by the crash handler, so plain atomics rather than anything that locks or allocates.
std::atomic<HWND>     g_windows_to_show_after_crash[K_CRASH_SLOTS]{};
std::atomic<LONG_PTR> g_original_styles_after_crash[K_CRASH_SLOTS]{};

enum class Restore : u8 {
	STYLE_ONLY,
	MINIMIZED,
	SHOWN,
};

[[nodiscard]] auto restore_for(HandBack t_mode) -> Restore
{
	return t_mode == HandBack::MINIMIZED ? Restore::MINIMIZED : Restore::SHOWN;
}

[[nodiscard]] auto hand_back_name(HandBack t_mode) -> const char*
{
	return t_mode == HandBack::MINIMIZED ? "minimized" : "shown";
}

auto remember_for_crash(HWND t_window, LONG_PTR t_original_ex_style) -> void
{
	for (usize slot = 0; slot < K_CRASH_SLOTS; slot += 1) {
		if (g_windows_to_show_after_crash[slot].load() != nullptr) continue;

		g_original_styles_after_crash[slot].store(t_original_ex_style);
		g_windows_to_show_after_crash[slot].store(t_window);
		return;
	}
}

auto forget_all_for_crash() -> void
{
	for (std::atomic<HWND>& window : g_windows_to_show_after_crash) {
		window.store(nullptr);
	}
}

[[nodiscard]] auto is_shell_process(DWORD t_process_id) -> bool
{
	const std::wstring executable = executable_name_of(t_process_id);

	return std::ranges::any_of(K_SHELL_PROCESSES, [&executable](const wchar_t* t_name) { return CompareStringOrdinal(executable.c_str(), -1, t_name, -1, TRUE) == CSTR_EQUAL; });
}

[[nodiscard]] auto launcher_process_ids() -> std::vector<DWORD>
{
	std::vector<DWORD> process_ids;

	const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == INVALID_HANDLE_VALUE) return process_ids;

	PROCESSENTRY32W entry{.dwSize = sizeof(entry)};
	for (bool more = Process32FirstW(snapshot, &entry); more; more = Process32NextW(snapshot, &entry)) {
		if (CompareStringOrdinal(entry.szExeFile, -1, K_LAUNCHER_NAME, -1, TRUE) == CSTR_EQUAL) {
			process_ids.push_back(entry.th32ProcessID);
		}
	}

	CloseHandle(snapshot);

	return process_ids;
}

[[nodiscard]] auto is_client_window_process(HWND t_window) -> bool
{
	DWORD process_id = 0;
	GetWindowThreadProcessId(t_window, &process_id);

	return is_client_interface_process(process_id);
}

// The window the user is in: whatever is in front, or, when the Riot Client holds the foreground, the topmost ordinary window under it.
[[nodiscard]] auto window_to_give_focus_back_to() -> HWND
{
	const HWND front = GetAncestor(GetForegroundWindow(), GA_ROOT);
	if (front != nullptr && !is_client_window_process(front)) return front;

	for (HWND window = GetTopWindow(nullptr); window != nullptr; window = GetWindow(window, GW_HWNDNEXT)) {
		if (!IsWindowVisible(window) || IsIconic(window) || GetWindow(window, GW_OWNER) != nullptr) continue;
		if ((GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) != 0 || GetWindowTextLengthW(window) == 0) continue;

		DWORD cloaked = 0;
		DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
		if (cloaked != 0 || is_client_window_process(window)) continue;

		return window;
	}

	return nullptr;
}

auto minimize_off_screen(HWND t_window) -> void
{
	WINDOWPLACEMENT placement{.length = sizeof(placement)};
	GetWindowPlacement(t_window, &placement);
	placement.flags |= WPF_SETMINPOSITION;
	placement.ptMinPosition = K_OFF_SCREEN_MINIMIZED_AT;
	placement.showCmd       = SW_SHOWMINNOACTIVE;
	SetWindowPlacement(t_window, &placement);
}

auto restore_window(HWND t_window, LONG_PTR t_original_ex_style, Restore t_restore, ITaskbarList* t_taskbar) -> void
{
	if (!IsWindow(t_window)) return;

	const bool visible   = IsWindowVisible(t_window) != FALSE;
	const bool minimized = IsIconic(t_window) != FALSE;

	SetWindowPos(t_window, HWND_NOTOPMOST, 0, 0, 0, 0, K_KEEP_PLACE);
	SetWindowLongPtrW(t_window, GWL_EXSTYLE, ((t_original_ex_style | WS_EX_LAYERED) & ~WS_EX_TOPMOST) | (GetWindowLongPtrW(t_window, GWL_EXSTYLE) & WS_EX_TOPMOST));

	if (t_taskbar != nullptr && visible && t_restore != Restore::STYLE_ONLY) {
		t_taskbar->AddTab(t_window);
	}

	const bool stays_minimized = t_restore == Restore::MINIMIZED || (t_restore == Restore::STYLE_ONLY && minimized);

	if (visible && stays_minimized) {
		minimize_off_screen(t_window);
	} else if (t_restore == Restore::SHOWN && !visible) {
		ShowWindow(t_window, SW_SHOWNA);
		if (t_taskbar != nullptr) {
			t_taskbar->AddTab(t_window);
		}
	} else if (t_restore == Restore::SHOWN && minimized) {
		ShowWindow(t_window, SW_SHOWNOACTIVATE);
	}

	SetLayeredWindowAttributes(t_window, 0, 255, LWA_ALPHA);
	if ((t_original_ex_style & WS_EX_LAYERED) == 0) {
		SetWindowLongPtrW(t_window, GWL_EXSTYLE, GetWindowLongPtrW(t_window, GWL_EXSTYLE) & ~WS_EX_LAYERED);
	}

	SetWindowPos(t_window, nullptr, 0, 0, 0, 0, K_KEEP_PLACE | SWP_NOZORDER | SWP_FRAMECHANGED);
	RedrawWindow(t_window, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_FRAME);

	if (t_restore == Restore::SHOWN) {
		FLASHWINFO flash{.cbSize = sizeof(flash), .hwnd = t_window, .dwFlags = FLASHW_TRAY | FLASHW_TIMERNOFG};
		FlashWindowEx(&flash);
	}
}

// Without Pulsar's taskbar object, re-showing the window is what brings its taskbar button back.
auto restore_window_without_pulsar(HWND t_window, LONG_PTR t_original_ex_style) -> void
{
	if (!IsWindow(t_window)) return;

	const bool visible = IsWindowVisible(t_window) != FALSE;

	SetWindowPos(t_window, HWND_NOTOPMOST, 0, 0, 0, 0, K_KEEP_PLACE);
	SetLayeredWindowAttributes(t_window, 0, 255, LWA_ALPHA);
	SetWindowLongPtrW(t_window, GWL_EXSTYLE, t_original_ex_style);
	SetWindowPos(t_window, nullptr, 0, 0, 0, 0, K_KEEP_PLACE | SWP_NOZORDER | SWP_FRAMECHANGED);

	if (visible) {
		ShowWindow(t_window, SW_HIDE);
		ShowWindow(t_window, SW_SHOWNA);
	}

	if (visible && IsIconic(t_window)) {
		minimize_off_screen(t_window);
	}
}
}

HiddenClient::~HiddenClient()
{
	if (m_thread.joinable()) {
		hand_back(HandBack::SHOWN);
	}
}

auto HiddenClient::start(bool t_show_live_view) -> bool
{
	HiddenClient* expected = nullptr;
	if (m_thread.joinable() || !g_hidden_client.compare_exchange_strong(expected, this)) return false;

	m_last_other_foreground = window_to_give_focus_back_to();
	m_hiding.store(true);

	const HANDLE ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
	m_thread           = std::thread([this, ready] { run_hooks(ready); });
	WaitForSingleObject(ready, K_HOOK_START_TIMEOUT_MS);
	CloseHandle(ready);

	if (m_control_window == nullptr) {
		debug_log::write(K_LOG_CATEGORY, "the window hooks could not be installed - the Riot Client will not be hidden");
		m_hiding.store(false);
		m_thread.join();
		g_hidden_client.store(nullptr);
		return false;
	}

	if (t_show_live_view) {
		m_live_view = std::make_unique<LiveView>();
		m_live_view->open();
	}

	send_to_hook_thread(K_HIDE_OPEN_WINDOWS_MESSAGE, 0);

	return true;
}

auto HiddenClient::is_hiding() const -> bool
{
	return m_hiding.load();
}

auto HiddenClient::watch_for_user_opening() -> void
{
	send_to_hook_thread(K_WATCH_FOR_USER_MESSAGE, 0);
}

auto HiddenClient::was_opened_by_user() const -> bool
{
	return m_opened_by_user.load();
}

auto HiddenClient::note_own_action() -> void
{
	m_own_action_until.store(GetTickCount64() + K_OWN_ACTION_GRACE_MS);
}

auto HiddenClient::show_progress(std::string_view t_progress) -> void
{
	if (m_live_view != nullptr) {
		m_live_view->set_status(os::win32::to_wide(t_progress));
	}
}

auto HiddenClient::hand_back(HandBack t_mode) -> void
{
	if (!m_thread.joinable()) return;

	send_to_hook_thread(K_HAND_BACK_MESSAGE, static_cast<WPARAM>(t_mode));
	PostMessageW(m_control_window, WM_CLOSE, 0, 0);
	m_thread.join();

	m_control_window = nullptr;
	g_hidden_client.store(nullptr);

	if (m_live_view != nullptr) {
		m_live_view->close_after(K_LIVE_VIEW_LINGER);
	}
}

auto HiddenClient::show_windows_left_hidden() -> void
{
	for (const HWND window : client_windows()) {
		const LONG_PTR ex_style = GetWindowLongPtrW(window, GWL_EXSTYLE);
		if ((ex_style & (WS_EX_LAYERED | WS_EX_TRANSPARENT)) != (WS_EX_LAYERED | WS_EX_TRANSPARENT)) continue;

		BYTE  alpha = 255;
		DWORD flags = 0;
		if (!GetLayeredWindowAttributes(window, nullptr, &alpha, &flags) || (flags & LWA_ALPHA) == 0 || alpha != 0) continue;

		debug_log::write(K_LOG_CATEGORY, "showing Riot Client window 0x%p that an earlier Pulsar left hidden", window);
		restore_window_without_pulsar(window, ex_style & ~K_HIDDEN_EX_STYLE_BITS);
	}
}

auto HiddenClient::show_hidden_windows_after_crash() -> void
{
	for (usize slot = 0; slot < K_CRASH_SLOTS; slot += 1) {
		const HWND window = g_windows_to_show_after_crash[slot].exchange(nullptr);
		if (window != nullptr) {
			restore_window_without_pulsar(window, g_original_styles_after_crash[slot].load());
		}
	}
}

auto CALLBACK HiddenClient::on_window_event(HWINEVENTHOOK, DWORD t_event, HWND t_window, LONG t_object, LONG t_child, DWORD, DWORD) -> void
{
	HiddenClient* self = g_hidden_client.load();
	if (self == nullptr || t_window == nullptr) return;

	if (t_event == EVENT_SYSTEM_FOREGROUND) {
		self->on_foreground(t_window);
		return;
	}

	if (t_event != EVENT_OBJECT_CREATE && t_event != EVENT_OBJECT_SHOW) return;
	if (t_object != OBJID_WINDOW || t_child != CHILDID_SELF || !self->m_hiding.load()) return;
	if (GetAncestor(t_window, GA_PARENT) != GetDesktopWindow() || !has_client_window_class(t_window)) return;

	DWORD process_id = 0;
	GetWindowThreadProcessId(t_window, &process_id);

	if (self->is_client_interface_process_cached(process_id)) {
		self->hide(t_window);
	}
}

auto CALLBACK HiddenClient::control_window_proc(HWND t_window, UINT t_message, WPARAM t_wparam, LPARAM t_lparam) -> LRESULT
{
	auto* self = reinterpret_cast<HiddenClient*>(GetWindowLongPtrW(t_window, GWLP_USERDATA));

	switch (t_message) {
		case WM_NCCREATE: {
			SetWindowLongPtrW(t_window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(t_lparam)->lpCreateParams));
			break;
		}

		case K_HIDE_OPEN_WINDOWS_MESSAGE: {
			self->hide_open_windows();
			return 0;
		}

		case K_WATCH_FOR_USER_MESSAGE: {
			self->begin_watching_for_user();
			return 0;
		}

		case K_HAND_BACK_MESSAGE: {
			self->restore_windows(static_cast<HandBack>(t_wparam));
			return 0;
		}

		case WM_TIMER: {
			self->check_for_new_client_launch();
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

auto HiddenClient::run_hooks(HANDLE t_ready) -> void
{
	const HRESULT com_result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	if (FAILED(CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_taskbar))) || FAILED(m_taskbar->HrInit())) {
		m_taskbar.Reset();
	}

	const HINSTANCE instance = GetModuleHandleW(nullptr);
	WNDCLASSW       window_class{.lpfnWndProc = control_window_proc, .hInstance = instance, .lpszClassName = K_CONTROL_CLASS};
	RegisterClassW(&window_class);

	const HWND          control    = CreateWindowExW(0, K_CONTROL_CLASS, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, instance, this);
	const HWINEVENTHOOK foreground = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, on_window_event, 0, 0, WINEVENT_OUTOFCONTEXT);
	const HWINEVENTHOOK windows    = SetWinEventHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_SHOW, nullptr, on_window_event, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
	const bool          hooked     = control != nullptr && foreground != nullptr && windows != nullptr;

	debug_log::write(K_LOG_CATEGORY, "window hooks %s (taskbar object %s)", hooked ? "installed" : "FAILED", m_taskbar != nullptr ? "ready" : "unavailable");

	if (hooked) {
		m_control_window = control;
	}

	SetEvent(t_ready);

	if (hooked) {
		MSG message;
		while (GetMessageW(&message, nullptr, 0, 0) > 0) {
			TranslateMessage(&message);
			DispatchMessageW(&message);
		}
	}

	if (windows != nullptr) UnhookWinEvent(windows);
	if (foreground != nullptr) UnhookWinEvent(foreground);
	if (control != nullptr && IsWindow(control)) DestroyWindow(control);

	m_taskbar.Reset();

	if (SUCCEEDED(com_result)) {
		CoUninitialize();
	}
}

auto HiddenClient::send_to_hook_thread(UINT t_message, WPARAM t_wparam) -> void
{
	if (m_control_window == nullptr) return;

	DWORD_PTR ignored = 0;
	if (SendMessageTimeoutW(m_control_window, t_message, t_wparam, 0, SMTO_NORMAL, K_HOOK_THREAD_TIMEOUT_MS, &ignored) == 0) {
		debug_log::write(K_LOG_CATEGORY, "the window hook thread didn't answer message 0x%X in time (err=%lu)", t_message, GetLastError());
	}
}

auto HiddenClient::hide(HWND t_window) -> void
{
	if (std::ranges::any_of(m_hidden_windows, [t_window](const HiddenWindow& t_hidden) { return t_hidden.window == t_window; })) return;

	const LONG_PTR original_ex_style = GetWindowLongPtrW(t_window, GWL_EXSTYLE);
	m_hidden_windows.push_back(HiddenWindow{.window = t_window, .original_ex_style = original_ex_style});
	remember_for_crash(t_window, original_ex_style);

	SetWindowLongPtrW(t_window, GWL_EXSTYLE, original_ex_style | K_HIDDEN_EX_STYLE_BITS);
	SetLayeredWindowAttributes(t_window, 0, 0, LWA_ALPHA);
	SetWindowPos(t_window, nullptr, 0, 0, 0, 0, K_KEEP_PLACE | SWP_NOZORDER | SWP_FRAMECHANGED);
	SetWindowPos(t_window, HWND_TOPMOST, 0, 0, 0, 0, K_KEEP_PLACE);

	if (m_taskbar != nullptr) {
		m_taskbar->DeleteTab(t_window);
	}

	debug_log::write(K_LOG_CATEGORY, "hid Riot Client window 0x%p", t_window);
}

auto HiddenClient::hide_open_windows() -> void
{
	for (const HWND window : client_windows()) {
		if (!IsWindowVisible(window)) continue;

		const bool minimized = IsIconic(window) != FALSE;
		hide(window);

		// A minimized page is frozen; restored without activation it stays invisible and keeps running.
		if (minimized) {
			ShowWindow(window, SW_SHOWNOACTIVATE);
		}
	}

	if (is_client_window_process(GetForegroundWindow())) {
		give_foreground_back();
	}
}

auto HiddenClient::on_foreground(HWND t_window) -> void
{
	DWORD process_id = 0;
	GetWindowThreadProcessId(t_window, &process_id);

	if (!is_client_interface_process_cached(process_id)) {
		m_last_other_foreground         = GetAncestor(t_window, GA_ROOT);
		m_last_other_foreground_process = process_id;
		return;
	}

	if (!m_hiding.load()) return;

	const bool watching = m_watching_for_user && GetTickCount64() >= m_watch_for_user_from && !is_own_action_recent();
	if (watching && is_shell_process(m_last_other_foreground_process)) {
		take_as_opened_by_user("the Riot Client came to the front right after the taskbar, tray, Start menu or desktop had it");
		return;
	}

	give_foreground_back();
}

auto HiddenClient::give_foreground_back() -> void
{
	const HWND target = m_last_other_foreground != nullptr && IsWindow(m_last_other_foreground) ? m_last_other_foreground : window_to_give_focus_back_to();
	if (target == nullptr) {
		debug_log::write(K_LOG_CATEGORY, "the Riot Client took the foreground and there is no window to give it back to");
		return;
	}

	const DWORD foreground_thread = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
	const DWORD own_thread        = GetCurrentThreadId();
	const bool  attached          = foreground_thread != 0 && foreground_thread != own_thread && AttachThreadInput(own_thread, foreground_thread, TRUE) != FALSE;
	const bool  given             = SetForegroundWindow(target) != FALSE;

	if (attached) {
		AttachThreadInput(own_thread, foreground_thread, FALSE);
	}

	if (m_live_view != nullptr) {
		m_live_view->count_focus_given_back();
	}

	debug_log::write(K_LOG_CATEGORY, "the Riot Client took the foreground - %s", given ? "gave it back" : "could NOT give it back");
}

auto HiddenClient::begin_watching_for_user() -> void
{
	m_watching_for_user      = true;
	m_watch_for_user_from    = GetTickCount64() + K_USER_WATCH_DELAY_MS;
	m_known_client_launchers = launcher_process_ids();

	SetTimer(m_control_window, K_LAUNCH_CHECK_TIMER, K_LAUNCH_CHECK_MS, nullptr);
}

auto HiddenClient::check_for_new_client_launch() -> void
{
	if (!m_hiding.load()) return;

	const auto is_new = [this](DWORD t_process_id) { return std::ranges::find(m_known_client_launchers, t_process_id) == m_known_client_launchers.end(); };

	if (std::ranges::any_of(launcher_process_ids(), is_new)) {
		take_as_opened_by_user("the Riot Client was started again");
	}
}

auto HiddenClient::take_as_opened_by_user(const char* t_how) -> void
{
	debug_log::write(K_LOG_CATEGORY, "%s - the user opened it, so it is shown and the login stops", t_how);

	m_opened_by_user.store(true);
	restore_windows(HandBack::SHOWN);
}

auto HiddenClient::restore_windows(HandBack t_mode) -> void
{
	if (m_handed_back) return;

	m_handed_back = true;
	m_hiding.store(false);
	KillTimer(m_control_window, K_LAUNCH_CHECK_TIMER);

	const HWND main_window = main_client_window();

	for (const HiddenWindow& hidden : m_hidden_windows) {
		restore_window(hidden.window, hidden.original_ex_style, hidden.window == main_window ? restore_for(t_mode) : Restore::STYLE_ONLY, m_taskbar.Get());
	}

	debug_log::write(K_LOG_CATEGORY, "handed the Riot Client back %s (%zu window(s))", hand_back_name(t_mode), m_hidden_windows.size());

	m_hidden_windows.clear();
	forget_all_for_crash();
}

auto HiddenClient::is_client_interface_process_cached(DWORD t_process_id) -> bool
{
	if (const auto known = m_interface_processes.find(t_process_id); known != m_interface_processes.end()) return known->second;

	const bool is_interface = is_client_interface_process(t_process_id);
	m_interface_processes.emplace(t_process_id, is_interface);

	return is_interface;
}

auto HiddenClient::is_own_action_recent() const -> bool
{
	return GetTickCount64() < m_own_action_until.load();
}
