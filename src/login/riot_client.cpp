#include "login/riot_client.h"

#include <algorithm>
#include <fstream>
#include <span>
#include <thread>
#include <vector>

#include <TlHelp32.h>

#include <nlohmann/json.hpp>

#include "core/debug_log.h"
#include "core/str.h"
#include "core/thread_util.h"

namespace {
constexpr const char *log_category = "riot";

constexpr const char *installs_json_path = "C:\\ProgramData\\Riot Games\\RiotClientInstalls.json";
constexpr const char *executable_path_key = "rc_default";

constexpr const wchar_t *client_window_title = L"Riot Client";
constexpr const wchar_t *username_field_name = L"USERNAME";
constexpr const wchar_t *password_field_name = L"PASSWORD";
constexpr const wchar_t *play_button_name = L"Play";
constexpr const wchar_t *login_error_tooltip_name = L"Login error";
constexpr const wchar_t *invalid_credentials_reason = L"Your login credentials don't match an account in our system.";
constexpr const wchar_t *trouble_signing_in_reason =
	L"Sorry, we're having trouble signing you in right now. Please try again later.";

constexpr const wchar_t *client_process_names[]{
	L"Riot Client.exe",	 L"RiotClientServices.exe", L"RiotClientUx.exe", L"RiotClientUxRender.exe",
	L"LeagueClient.exe", L"LeagueClientUx.exe",		L"LoR.exe",
};

constexpr const wchar_t *game_process_names[]{
	L"VALORANT-Win64-Shipping.exe",
	L"League of Legends.exe",
};

constexpr auto window_element_lifetime = std::chrono::milliseconds(2000);
constexpr auto poll_interval = std::chrono::milliseconds(100);
constexpr u32 responsiveness_probe_ms = 750;
constexpr u32 focus_settle_ms = 500;
constexpr u32 form_gone_polls = 30;

bool is_cancelled(const std::atomic<bool> &t_cancel)
{
	return t_cancel.load(std::memory_order_relaxed);
}

std::chrono::steady_clock::time_point deadline_after(u32 t_timeout_ms)
{
	return std::chrono::steady_clock::now() + std::chrono::milliseconds(t_timeout_ms);
}

bool is_past(std::chrono::steady_clock::time_point t_deadline)
{
	return std::chrono::steady_clock::now() >= t_deadline;
}

bool matches_any(const wchar_t *t_exe_name, std::span<const wchar_t *const> t_names)
{
	return std::ranges::any_of(t_names, [t_exe_name](const wchar_t *t_name) {
		return CompareStringOrdinal(t_exe_name, -1, t_name, -1, TRUE) == CSTR_EQUAL;
	});
}

template <typename Visitor>
void for_each_process(Visitor t_visitor)
{
	const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == INVALID_HANDLE_VALUE) return;

	PROCESSENTRY32W entry{.dwSize = sizeof(entry)};
	for (bool more = Process32FirstW(snapshot, &entry); more; more = Process32NextW(snapshot, &entry)) {
		t_visitor(entry);
	}

	CloseHandle(snapshot);
}

void log_window_identity(const char *t_what, HWND t_window)
{
	if (!debug_log::is_enabled()) return;

	DWORD process_id = 0;
	const DWORD thread_id = GetWindowThreadProcessId(t_window, &process_id);

	wchar_t title[128]{};
	GetWindowTextW(t_window, title, ARRAYSIZE(title));

	debug_log::write(log_category, "%s: hwnd=0x%p owned by pid %lu / thread t%lu, title \"%ls\"", t_what, t_window,
					 process_id, thread_id, title);
}

void wait_for_processes_to_exit(const std::vector<HANDLE> &t_processes, const std::atomic<bool> &t_cancel)
{
	for (usize offset = 0; offset < t_processes.size(); offset += MAXIMUM_WAIT_OBJECTS) {
		const auto count = static_cast<DWORD>(std::min<usize>(MAXIMUM_WAIT_OBJECTS, t_processes.size() - offset));

		while (WaitForMultipleObjects(count, t_processes.data() + offset, TRUE,
									  static_cast<DWORD>(poll_interval.count())) == WAIT_TIMEOUT) {
			if (is_cancelled(t_cancel)) return;
		}
	}
}

std::vector<HANDLE> terminate_processes(std::span<const wchar_t *const> t_names)
{
	std::vector<HANDLE> terminated;

	for_each_process([&](const PROCESSENTRY32W &t_entry) {
		if (!matches_any(t_entry.szExeFile, t_names)) return;

		const HANDLE process = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, t_entry.th32ProcessID);
		if (process == nullptr) return;

		if (TerminateProcess(process, 0)) {
			debug_log::write(log_category, "terminated %ls (pid %lu)", t_entry.szExeFile, t_entry.th32ProcessID);
			terminated.push_back(process);
		} else {
			debug_log::write(log_category, "TerminateProcess FAILED for %ls (pid %lu), err=%lu", t_entry.szExeFile,
							 t_entry.th32ProcessID, GetLastError());
			CloseHandle(process);
		}
	});

	return terminated;
}

bool is_window_responsive(HWND t_window)
{
	const debug_log::Scope scope(log_category, "WM_NULL responsiveness probe (hwnd=0x%p)", t_window);

	DWORD_PTR ignored = 0;

	return SendMessageTimeoutW(t_window, WM_NULL, 0, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK, responsiveness_probe_ms,
							   &ignored) != 0;
}

class AttachedInputQueue {
  public:
	explicit AttachedInputQueue(HWND t_window)
	{
		if (!is_window_responsive(t_window)) {
			debug_log::write(log_category, "activation: target window is not pumping - skipping AttachThreadInput");
			return;
		}

		m_target_thread = GetWindowThreadProcessId(t_window, nullptr);
		m_current_thread = GetCurrentThreadId();

		if (m_target_thread != 0 && m_target_thread != m_current_thread) {
			const debug_log::Scope scope(log_category, "AttachThreadInput(TRUE) to t%lu", m_target_thread);
			m_attached = AttachThreadInput(m_current_thread, m_target_thread, TRUE) != 0;
		}

		debug_log::write(log_category, "activation: input queue %s target thread t%lu",
						 m_attached ? "attached to" : "NOT attached to", m_target_thread);
	}

	~AttachedInputQueue()
	{
		if (!m_attached) return;

		const debug_log::Scope scope(log_category, "AttachThreadInput(FALSE) from t%lu", m_target_thread);
		AttachThreadInput(m_current_thread, m_target_thread, FALSE);
	}

	AttachedInputQueue(const AttachedInputQueue &) = delete;
	AttachedInputQueue &operator=(const AttachedInputQueue &) = delete;

  private:
	DWORD m_target_thread = 0;
	DWORD m_current_thread = 0;
	bool m_attached = false;
};

void activate_window(HWND t_window, bool t_take_focus)
{
	// Windows ignores SetForegroundWindow and SetFocus from a thread outside the target's input queue.
	const AttachedInputQueue attached(t_window);

	if (IsIconic(t_window)) {
		const debug_log::Scope scope(log_category, "ShowWindow(SW_RESTORE)");
		ShowWindow(t_window, SW_RESTORE);
	}

	{
		const debug_log::Scope scope(log_category, "SetForegroundWindow");
		SetForegroundWindow(t_window);
	}

	{
		const debug_log::Scope scope(log_category, "BringWindowToTop");
		BringWindowToTop(t_window);
	}

	if (t_take_focus) {
		const debug_log::Scope scope(log_category, "SetFocus");
		SetFocus(t_window);
	}
}

bool activate_window_unless_cancelled(HWND t_window, bool t_take_focus, const std::atomic<bool> &t_cancel)
{
	debug_log::write(log_category, "activation pass starting (hwnd=0x%p, focus=%s)", t_window,
					 t_take_focus ? "yes" : "no");

	const bool completed =
		run_unless_cancelled([t_window, t_take_focus]() { activate_window(t_window, t_take_focus); }, t_cancel);

	debug_log::write(log_category, "activation pass %s",
					 completed ? "completed" : "ABANDONED - cancelled while stuck in the client's window procedure");

	return completed;
}

void wait_for_keyboard_focus(const UiElement &t_element)
{
	const auto deadline = deadline_after(focus_settle_ms);

	while (!t_element.has_keyboard_focus() && !is_past(deadline)) {
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
}

void fill_field(const UiAutomation &t_automation, const UiElement &t_field, const std::wstring &t_value)
{
	if (t_field.set_value(t_value.c_str())) return;

	t_field.focus();
	wait_for_keyboard_focus(t_field);
	t_automation.type_text(t_value.c_str());
}

std::wstring login_error_reason(const UiAutomation &t_automation, const UiElement &t_window)
{
	for (const wchar_t *reason : {invalid_credentials_reason, trouble_signing_in_reason}) {
		if (t_automation.find_descendant(t_window, reason).is_valid()) return reason;
	}

	return login_error_tooltip_name;
}
}

RiotClient::~RiotClient()
{
	if (m_process != nullptr) {
		CloseHandle(m_process);
	}
}

bool RiotClient::is_game_in_progress()
{
	bool in_progress = false;
	for_each_process([&in_progress](const PROCESSENTRY32W &t_entry) {
		in_progress = in_progress || matches_any(t_entry.szExeFile, game_process_names);
	});

	debug_log::write(log_category, "is_game_in_progress -> %s", in_progress ? "yes" : "no");

	return in_progress;
}

void RiotClient::kill_all_client_processes(const std::atomic<bool> &t_cancel)
{
	debug_log::write(log_category, "killing every known Riot Client process");

	const std::vector<HANDLE> terminated = terminate_processes(client_process_names);

	const debug_log::Scope scope(log_category, "wait for %zu killed client process(es) to exit", terminated.size());
	wait_for_processes_to_exit(terminated, t_cancel);

	for (const HANDLE process : terminated) {
		CloseHandle(process);
	}

	debug_log::write(log_category, "killed client processes gone after %llums", scope.elapsed_ms());
}

bool RiotClient::resolve_executable_path()
{
	m_executable_path.clear();

	std::ifstream file(installs_json_path);
	if (!file.is_open()) return false;

	const nlohmann::json installs = nlohmann::json::parse(file, nullptr, false);
	if (!installs.is_object()) return false;

	const auto path = installs.find(executable_path_key);
	if (path == installs.end() || !path->is_string()) return false;

	m_executable_path = to_wide(path->get_ref<const std::string &>());
	std::ranges::replace(m_executable_path, L'/', L'\\');

	debug_log::write(log_category, "resolved Riot Client executable: %ls", m_executable_path.c_str());

	return !m_executable_path.empty();
}

bool RiotClient::launch(std::string_view t_launch_product)
{
	if (m_executable_path.empty()) return false;

	std::wstring command_line = L"\"" + m_executable_path + L"\"";
	if (!t_launch_product.empty()) {
		command_line += L" --launch-product=" + to_wide(t_launch_product) + L" --launch-patchline=live";
	}

	STARTUPINFOW startup_info{.cb = sizeof(startup_info)};
	PROCESS_INFORMATION process_info{};

	if (!CreateProcessW(m_executable_path.c_str(), command_line.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr,
						&startup_info, &process_info)) {
		debug_log::write(log_category, "CreateProcessW FAILED err=%lu for %ls", GetLastError(),
						 m_executable_path.c_str());
		return false;
	}

	debug_log::write(log_category, "launched Riot Client pid %lu (%ls)", process_info.dwProcessId,
					 command_line.c_str());
	CloseHandle(process_info.hThread);

	if (m_process != nullptr) {
		CloseHandle(m_process);
	}

	m_process = process_info.hProcess;
	m_process_id = process_info.dwProcessId;

	return true;
}

HWND RiotClient::find_client_window() const
{
	const HWND window = FindWindowW(nullptr, client_window_title);
	if (window != nullptr || m_process_id == 0) return window;

	return UiAutomation::find_top_level_window(m_process_id);
}

UiElement RiotClient::current_window_element(const UiAutomation &t_automation) const
{
	const HWND window = find_client_window();
	if (window == nullptr) return {};

	const auto now = std::chrono::steady_clock::now();
	if (window == m_cached_window && m_cached_window_element.is_valid() && now < m_cached_window_expiry) {
		return m_cached_window_element;
	}

	UiElement element = t_automation.element_from_window(window);
	m_cached_window = element.is_valid() ? window : nullptr;
	m_cached_window_element = element;
	m_cached_window_expiry = now + window_element_lifetime;

	return element;
}

HWND RiotClient::wait_for_responsive_window(const std::atomic<bool> &t_cancel) const
{
	const auto started = std::chrono::steady_clock::now();

	debug_log::write(log_category, "waiting for a responsive client window (until cancelled)");

	u32 polls = 0;
	u32 next_progress_report = 20;

	for (;;) {
		const HWND window = find_client_window();
		if (window != nullptr && is_window_responsive(window)) {
			log_window_identity("responsive client window", window);
			return window;
		}

		polls += 1;
		if (polls >= next_progress_report) {
			next_progress_report = polls + 50;
			const auto waited =
				std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);

			debug_log::write(log_category, "still waiting for a responsive client window after %lldms (%s)",
							 waited.count(), window == nullptr ? "no window yet" : "window is not pumping messages");
		}

		if (is_cancelled(t_cancel)) {
			debug_log::write(log_category, "stopped waiting for a responsive client window (cancelled)");
			return nullptr;
		}

		std::this_thread::sleep_for(poll_interval);
	}
}

bool RiotClient::bring_to_foreground(const std::atomic<bool> &t_cancel) const
{
	const HWND window = wait_for_responsive_window(t_cancel);

	return window != nullptr && activate_window_unless_cancelled(window, false, t_cancel);
}

bool RiotClient::take_keyboard_focus(const std::atomic<bool> &t_cancel) const
{
	const HWND window = wait_for_responsive_window(t_cancel);

	return window != nullptr && activate_window_unless_cancelled(window, true, t_cancel);
}

bool RiotClient::submit_login(const UiAutomation &t_automation, std::string_view t_username,
							  std::string_view t_password, const std::atomic<bool> &t_cancel) const
{
	UiElement username_field;
	UiElement password_field;
	u32 polls = 0;

	debug_log::write(log_category, "looking for the login form (until cancelled)");

	for (;;) {
		const UiElement window = current_window_element(t_automation);
		if (window.is_valid()) {
			username_field = t_automation.find_descendant(window, username_field_name, UIA_EditControlTypeId);
			password_field = t_automation.find_descendant(window, password_field_name, UIA_EditControlTypeId);

			if (username_field.is_valid() && password_field.is_valid()) {
				debug_log::write(log_category, "login form found after %u poll(s)", polls);
				break;
			}
		}

		polls += 1;

		if (is_cancelled(t_cancel)) {
			debug_log::write(log_category,
							 "login form NOT found after %u poll(s) (cancelled; window %s, username %s, password %s)",
							 polls, window.is_valid() ? "yes" : "no", username_field.is_valid() ? "yes" : "no",
							 password_field.is_valid() ? "yes" : "no");
			return false;
		}

		std::this_thread::sleep_for(poll_interval);
	}

	fill_field(t_automation, username_field, to_wide(t_username));
	fill_field(t_automation, password_field, to_wide(t_password));

	password_field.focus();
	wait_for_keyboard_focus(password_field);

	debug_log::write(log_category, "submitting the login form (password field %s real keyboard focus)",
					 password_field.has_keyboard_focus() ? "has" : "does NOT have");
	t_automation.press_key(VK_RETURN);

	return true;
}

bool RiotClient::wait_for_login_result(const UiAutomation &t_automation, std::wstring &t_out_error,
									   const std::atomic<bool> &t_cancel, const std::wstring *t_error_to_ignore) const
{
	bool saw_no_tooltip = t_error_to_ignore == nullptr;
	u32 polls_without_form = 0;

	while (!is_cancelled(t_cancel)) {
		const UiElement window = current_window_element(t_automation);

		if (window.is_valid()) {
			if (t_automation.find_descendant(window, login_error_tooltip_name).is_valid()) {
				std::wstring message = login_error_reason(t_automation, window);

				if (saw_no_tooltip || message != *t_error_to_ignore) {
					debug_log::write(log_category, "login error shown: \"%ls\"", message.c_str());
					t_out_error = std::move(message);
					return true;
				}

				debug_log::write(log_category, "ignoring the previous attempt's error tooltip, still on screen");
			} else {
				saw_no_tooltip = true;
			}

			if (t_automation.find_descendant(window, play_button_name, UIA_ButtonControlTypeId).is_valid()) {
				debug_log::write(log_category, "the Play button is up - the client signed in");
				return false;
			}

			const bool form_shown =
				t_automation.find_descendant(window, username_field_name, UIA_EditControlTypeId).is_valid() ||
				t_automation.find_descendant(window, password_field_name, UIA_EditControlTypeId).is_valid();
			polls_without_form = form_shown ? 0 : polls_without_form + 1;

			// The client hides the form while it talks to Riot, so only a long absence counts as signed in.
			if (polls_without_form >= form_gone_polls) {
				debug_log::write(log_category, "the login form stayed gone - the client signed in");
				return false;
			}
		}

		std::this_thread::sleep_for(poll_interval);
	}

	debug_log::write(log_category, "stopped waiting for the login result (cancelled)");

	return false;
}

PlayResult RiotClient::click_play_when_ready(const UiAutomation &t_automation, u32 t_timeout_ms,
											 const std::atomic<bool> &t_cancel, std::wstring &t_out_error) const
{
	const auto deadline = deadline_after(t_timeout_ms);

	for (;;) {
		const UiElement window = current_window_element(t_automation);

		if (window.is_valid() && t_automation.find_descendant(window, login_error_tooltip_name).is_valid()) {
			t_out_error = login_error_reason(t_automation, window);
			debug_log::write(log_category, "login error shown while waiting for Play: \"%ls\"", t_out_error.c_str());
			return PlayResult::LoginError;
		}

		const UiElement play_button =
			window.is_valid() ? t_automation.find_descendant(window, play_button_name, UIA_ButtonControlTypeId)
							  : UiElement{};

		if (play_button.is_valid()) {
			debug_log::write(log_category, "Play button found - invoking it");
			play_button.invoke();
			return PlayResult::Clicked;
		}

		if (is_cancelled(t_cancel) || is_past(deadline)) {
			debug_log::write(log_category, "Play button not found (%s) - leaving the game unlaunched",
							 is_cancelled(t_cancel) ? "cancelled" : "timed out");
			return PlayResult::NotFound;
		}

		std::this_thread::sleep_for(poll_interval);
	}
}
