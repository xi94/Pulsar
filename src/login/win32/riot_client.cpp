#include "login/riot_client.h"

#include <algorithm>
#include <concepts>
#include <cwchar>
#include <fstream>
#include <iterator>
#include <optional>
#include <ranges>
#include <span>
#include <thread>
#include <tuple>
#include <vector>

#include <Windows.h>
#include <TlHelp32.h>

#include "core/debug_log.h"
#include "core/thread_util.h"
#include "login/win32/client_windows.h"
#include "login/win32/hidden_client.h"
#include "login/win32/ui_automation.h"
#include "os/win32/win32.h"

using os::win32::to_utf8;
using os::win32::to_wide;

namespace {
constexpr const char* K_LOG_CATEGORY = "riot";

constexpr const wchar_t*    K_INSTALLS_JSON_NAME          = L"Riot Games\\RiotClientInstalls.json";
constexpr const wchar_t*    K_CLIENT_EXECUTABLE_NAME      = L"RiotClientServices.exe";
constexpr const char*       K_CLIENT_EXECUTABLE_NAME_UTF8 = "RiotClientServices.exe";
constexpr const char*       K_DEFAULT_INSTALL_FOLDER      = "C:\\Riot Games\\Riot Client";
constexpr const wchar_t*    K_DEFAULT_CLIENT_FOLDER       = L"Riot Games\\Riot Client\\";
constexpr const wchar_t*    K_CLIENT_FOLDER_NAME          = L"Riot Client";
constexpr u32               K_CHOSEN_PATH_SEARCH_LEVELS   = 3;
constexpr const wchar_t*    K_PROTOCOL_COMMAND_KEY        = L"Software\\Classes\\riotclient\\shell\\open\\command";
constexpr const wchar_t*    K_UNINSTALL_KEY               = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
constexpr std::wstring_view K_RIOT_UNINSTALL_PREFIX       = L"Riot Game ";

constexpr const char* K_AUTOMATION_FAILED_MESSAGE = "Couldn't start Windows UI Automation - try again.";

constexpr const wchar_t* K_USERNAME_FIELD_NAME      = L"USERNAME";
constexpr const wchar_t* K_PASSWORD_FIELD_NAME      = L"PASSWORD";
constexpr const wchar_t* K_PLAY_BUTTON_NAME         = L"Play";
constexpr const wchar_t* K_LOGIN_ERROR_TOOLTIP_NAME = L"Login error";

constexpr const wchar_t* K_CLIENT_PROCESS_NAMES[]{
	L"Riot Client.exe",
	L"RiotClientServices.exe",
	L"RiotClientUx.exe",
	L"RiotClientUxRender.exe",
	L"LeagueClient.exe",
	L"LeagueClientUx.exe",
	L"LoR.exe",
};

struct GameProcess {
	const wchar_t*   executable;
	std::string_view launch_product;
};

constexpr GameProcess K_GAME_PROCESSES[]{
	{L"VALORANT-Win64-Shipping.exe", "valorant"},
	{L"League of Legends.exe", "league_of_legends"},
	{L"TFTClient-Win64-Shipping.exe", "teamfighttactics"},
};

constexpr const wchar_t* K_STARTED_GAME_PROCESS_NAMES[]{
	L"LeagueClient.exe",
	L"LeagueClientUx.exe",
	L"League of Legends.exe",
	L"VALORANT.exe",
	L"VALORANT-Win64-Shipping.exe",
	L"TFTClient.exe",
	L"TFTClient-Win64-Shipping.exe",
	L"LoR.exe",
};

constexpr auto K_WINDOW_ELEMENT_LIFETIME = std::chrono::milliseconds(2000);
constexpr auto K_POLL_INTERVAL           = std::chrono::milliseconds(100);
constexpr u32  K_RESPONSIVENESS_PROBE_MS = 750;
constexpr u32  K_FOCUS_SETTLE_MS         = 500;
constexpr u32  K_SIGN_IN_ARROW_WAIT_MS   = 5000;
constexpr auto K_SIGN_IN_ARROW_POLL      = std::chrono::milliseconds(50);
constexpr u32  K_PRESS_CONFIRMATION_MS   = 2500;
constexpr auto K_PRESS_CONFIRMATION_POLL = std::chrono::milliseconds(40);
constexpr u32  K_MAX_SIGN_IN_PRESSES     = 3;

// The typed account is wiped from memory as soon as the form has it.
class WideSecret {
  public:
	explicit WideSecret(std::string_view t_text)
		: m_text(to_wide(t_text))
	{
	}

	~WideSecret()
	{
		SecureZeroMemory(m_text.data(), m_text.capacity() * sizeof(wchar_t));
	}

	WideSecret(const WideSecret&)                    = delete;
	auto operator=(const WideSecret&) -> WideSecret& = delete;

	[[nodiscard]] auto text() const -> const std::wstring&
	{
		return m_text;
	}

  private:
	std::wstring m_text;
};

[[nodiscard]] auto element_bounds_of(const RECT& t_rect) -> ElementBounds
{
	return ElementBounds{.left = t_rect.left, .top = t_rect.top, .right = t_rect.right, .bottom = t_rect.bottom};
}

[[nodiscard]] auto form_buttons_of(std::span<const UiButton> t_buttons) -> std::vector<FormButton>
{
	const auto to_form_button = [](const UiButton& t_button) { return FormButton{.bounds = element_bounds_of(t_button.bounds), .named = t_button.named, .invokable = t_button.invokable, .enabled = t_button.enabled}; };

	return t_buttons | std::views::transform(to_form_button) | std::ranges::to<std::vector>();
}

[[nodiscard]] auto is_cancelled(const std::atomic<bool>* t_cancel) -> bool
{
	return t_cancel->load(std::memory_order_relaxed);
}

[[nodiscard]] auto deadline_after(u32 t_timeout_ms) -> std::chrono::steady_clock::time_point
{
	return std::chrono::steady_clock::now() + std::chrono::milliseconds(t_timeout_ms);
}

[[nodiscard]] auto is_past(std::chrono::steady_clock::time_point t_deadline) -> bool
{
	return std::chrono::steady_clock::now() >= t_deadline;
}

[[nodiscard]] auto matches_any(const wchar_t* t_exe_name, std::span<const wchar_t* const> t_names) -> bool
{
	return std::ranges::any_of(t_names, [t_exe_name](const wchar_t* t_name) { return CompareStringOrdinal(t_exe_name, -1, t_name, -1, TRUE) == CSTR_EQUAL; });
}

auto for_each_process(std::invocable<const PROCESSENTRY32W&> auto t_visitor) -> void
{
	const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == INVALID_HANDLE_VALUE) return;

	PROCESSENTRY32W entry{.dwSize = sizeof(entry)};
	for (bool more = Process32FirstW(snapshot, &entry); more; more = Process32NextW(snapshot, &entry)) {
		t_visitor(entry);
	}

	CloseHandle(snapshot);
}

[[nodiscard]] auto is_file(const std::wstring& t_path) -> bool
{
	const DWORD attributes = GetFileAttributesW(t_path.c_str());

	return !t_path.empty() && attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

[[nodiscard]] auto first_path_in_command(std::wstring_view t_command) -> std::wstring
{
	if (t_command.starts_with(L'"')) {
		const usize end = t_command.find(L'"', 1);
		return std::wstring{t_command.substr(1, end == std::wstring_view::npos ? std::wstring_view::npos : end - 1)};
	}

	return std::wstring{t_command.substr(0, t_command.find(L' '))};
}

[[nodiscard]] auto registry_string(HKEY t_root, const wchar_t* t_key, const wchar_t* t_value) -> std::wstring
{
	DWORD bytes = 0;
	if (RegGetValueW(t_root, t_key, t_value, RRF_RT_REG_SZ, nullptr, nullptr, &bytes) != ERROR_SUCCESS || bytes == 0) return {};

	std::wstring text(bytes / sizeof(wchar_t), L'\0');
	if (RegGetValueW(t_root, t_key, t_value, RRF_RT_REG_SZ, nullptr, text.data(), &bytes) != ERROR_SUCCESS) return {};

	text.resize(std::wcslen(text.c_str()));

	return text;
}

[[nodiscard]] auto program_data_folder() -> std::wstring
{
	wchar_t     folder[MAX_PATH];
	const DWORD length = GetEnvironmentVariableW(L"ProgramData", folder, MAX_PATH);

	return length > 0 && length < MAX_PATH ? std::wstring{folder, length} : std::wstring{L"C:\\ProgramData"};
}

auto installs_json_paths(std::vector<std::wstring>* t_out) -> void
{
	std::ifstream file(program_data_folder() + L"\\" + K_INSTALLS_JSON_NAME, std::ios::binary);
	if (!file.is_open()) return;

	const std::string json{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};

	for (const std::string& path : RiotClient::paths_in_installs_file(json)) {
		std::wstring wide = to_wide(path);
		std::ranges::replace(wide, L'/', L'\\');
		t_out->push_back(std::move(wide));
	}
}

auto uninstall_entry_paths(HKEY t_root, std::vector<std::wstring>* t_out) -> void
{
	HKEY uninstall = nullptr;
	if (RegOpenKeyExW(t_root, K_UNINSTALL_KEY, 0, KEY_READ, &uninstall) != ERROR_SUCCESS) return;

	wchar_t name[256];
	for (DWORD index = 0;; index += 1) {
		DWORD         length = ARRAYSIZE(name);
		const LSTATUS status = RegEnumKeyExW(uninstall, index, name, &length, nullptr, nullptr, nullptr, nullptr);
		if (status == ERROR_NO_MORE_ITEMS) break;
		if (status != ERROR_SUCCESS || !std::wstring_view{name, length}.starts_with(K_RIOT_UNINSTALL_PREFIX)) continue;

		const std::wstring command = registry_string(uninstall, name, L"UninstallString");
		if (!command.empty()) {
			t_out->push_back(first_path_in_command(command));
		}
	}

	RegCloseKey(uninstall);
}

auto running_client_path(std::vector<std::wstring>* t_out) -> void
{
	for_each_process([t_out](const PROCESSENTRY32W& t_entry) {
		if (CompareStringOrdinal(t_entry.szExeFile, -1, K_CLIENT_EXECUTABLE_NAME, -1, TRUE) != CSTR_EQUAL) return;

		const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, t_entry.th32ProcessID);
		if (process == nullptr) return;

		wchar_t path[MAX_PATH * 2];
		DWORD   length = ARRAYSIZE(path);
		if (QueryFullProcessImageNameW(process, 0, path, &length)) {
			t_out->push_back(std::wstring{path, length});
		}

		CloseHandle(process);
	});
}

auto default_folder_paths(std::vector<std::wstring>* t_out) -> void
{
	const DWORD drives = GetLogicalDrives();

	for (wchar_t letter = L'A'; letter <= L'Z'; letter += 1) {
		if ((drives & (1u << (letter - L'A'))) == 0) continue;

		const std::wstring root{letter, L':', L'\\'};
		if (GetDriveTypeW(root.c_str()) != DRIVE_FIXED) continue;

		t_out->push_back(root + K_DEFAULT_CLIENT_FOLDER + K_CLIENT_EXECUTABLE_NAME);
	}
}

[[nodiscard]] auto client_path_candidates() -> std::vector<std::wstring>
{
	std::vector<std::wstring> candidates;

	installs_json_paths(&candidates);
	candidates.push_back(first_path_in_command(registry_string(HKEY_CURRENT_USER, K_PROTOCOL_COMMAND_KEY, nullptr)));
	candidates.push_back(first_path_in_command(registry_string(HKEY_LOCAL_MACHINE, K_PROTOCOL_COMMAND_KEY, nullptr)));
	uninstall_entry_paths(HKEY_CURRENT_USER, &candidates);
	uninstall_entry_paths(HKEY_LOCAL_MACHINE, &candidates);
	running_client_path(&candidates);
	default_folder_paths(&candidates);

	return candidates;
}

auto log_window_identity(const char* t_what, HWND t_window) -> void
{
	if (!debug_log::is_enabled()) return;

	DWORD       process_id = 0;
	const DWORD thread_id  = GetWindowThreadProcessId(t_window, &process_id);

	wchar_t title[128]{};
	GetWindowTextW(t_window, title, ARRAYSIZE(title));

	debug_log::write(K_LOG_CATEGORY, "%s: hwnd=0x%p owned by pid %lu / thread t%lu, title \"%ls\"", t_what, t_window, process_id, thread_id, title);
}

auto wait_for_processes_to_exit(const std::vector<HANDLE>& t_processes, const std::atomic<bool>* t_cancel) -> void
{
	for (usize offset = 0; offset < t_processes.size(); offset += MAXIMUM_WAIT_OBJECTS) {
		const auto count = static_cast<DWORD>(std::min<usize>(MAXIMUM_WAIT_OBJECTS, t_processes.size() - offset));

		while (WaitForMultipleObjects(count, t_processes.data() + offset, TRUE, static_cast<DWORD>(K_POLL_INTERVAL.count())) == WAIT_TIMEOUT) {
			if (is_cancelled(t_cancel)) return;
		}
	}
}

[[nodiscard]] auto terminate_processes(std::span<const wchar_t* const> t_names) -> std::vector<HANDLE>
{
	std::vector<HANDLE> terminated;

	for_each_process([&](const PROCESSENTRY32W& t_entry) {
		if (!matches_any(t_entry.szExeFile, t_names)) return;

		const HANDLE process = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, t_entry.th32ProcessID);
		if (process == nullptr) return;

		if (TerminateProcess(process, 0)) {
			debug_log::write(K_LOG_CATEGORY, "terminated %ls (pid %lu)", t_entry.szExeFile, t_entry.th32ProcessID);
			terminated.push_back(process);
		} else {
			debug_log::write(K_LOG_CATEGORY, "TerminateProcess FAILED for %ls (pid %lu), err=%lu", t_entry.szExeFile, t_entry.th32ProcessID, GetLastError());
			CloseHandle(process);
		}
	});

	return terminated;
}

[[nodiscard]] auto is_window_responsive(HWND t_window) -> bool
{
	const debug_log::Scope scope(K_LOG_CATEGORY, "WM_NULL responsiveness probe (hwnd=0x%p)", t_window);

	DWORD_PTR ignored = 0;

	return SendMessageTimeoutW(t_window, WM_NULL, 0, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK, K_RESPONSIVENESS_PROBE_MS, &ignored) != 0;
}

class AttachedInputQueue {
  public:
	explicit AttachedInputQueue(HWND t_window)
	{
		if (!is_window_responsive(t_window)) {
			debug_log::write(K_LOG_CATEGORY, "activation: target window is not pumping - skipping AttachThreadInput");
			return;
		}

		m_target_thread  = GetWindowThreadProcessId(t_window, nullptr);
		m_current_thread = GetCurrentThreadId();

		if (m_target_thread != 0 && m_target_thread != m_current_thread) {
			const debug_log::Scope scope(K_LOG_CATEGORY, "AttachThreadInput(TRUE) to t%lu", m_target_thread);
			m_attached = AttachThreadInput(m_current_thread, m_target_thread, TRUE) != 0;
		}

		debug_log::write(K_LOG_CATEGORY, "activation: input queue %s target thread t%lu", m_attached ? "attached to" : "NOT attached to", m_target_thread);
	}

	~AttachedInputQueue()
	{
		if (!m_attached) return;

		const debug_log::Scope scope(K_LOG_CATEGORY, "AttachThreadInput(FALSE) from t%lu", m_target_thread);
		AttachThreadInput(m_current_thread, m_target_thread, FALSE);
	}

	AttachedInputQueue(const AttachedInputQueue&)                    = delete;
	auto operator=(const AttachedInputQueue&) -> AttachedInputQueue& = delete;

  private:
	DWORD m_target_thread  = 0;
	DWORD m_current_thread = 0;
	bool  m_attached       = false;
};

auto activate_window(HWND t_window, bool t_take_focus) -> void
{
	// Windows ignores SetForegroundWindow and SetFocus from a thread outside the target's input queue.
	const AttachedInputQueue attached(t_window);

	if (IsIconic(t_window)) {
		const debug_log::Scope scope(K_LOG_CATEGORY, "ShowWindow(SW_RESTORE)");
		ShowWindow(t_window, SW_RESTORE);
	}

	{
		const debug_log::Scope scope(K_LOG_CATEGORY, "SetForegroundWindow");
		SetForegroundWindow(t_window);
	}

	{
		const debug_log::Scope scope(K_LOG_CATEGORY, "BringWindowToTop");
		BringWindowToTop(t_window);
	}

	if (t_take_focus) {
		const debug_log::Scope scope(K_LOG_CATEGORY, "SetFocus");
		SetFocus(t_window);
	}
}

[[nodiscard]] auto activate_window_unless_cancelled(HWND t_window, bool t_take_focus, const std::atomic<bool>* t_cancel) -> bool
{
	debug_log::write(K_LOG_CATEGORY, "activation pass starting (hwnd=0x%p, focus=%s)", t_window, t_take_focus ? "yes" : "no");

	const bool completed = run_unless_cancelled([t_window, t_take_focus]() { activate_window(t_window, t_take_focus); }, t_cancel);

	debug_log::write(K_LOG_CATEGORY, "activation pass %s", completed ? "completed" : "ABANDONED - cancelled while stuck in the client's window procedure");

	return completed;
}

auto wait_for_keyboard_focus(const UiElement& t_element) -> void
{
	const auto deadline = deadline_after(K_FOCUS_SETTLE_MS);

	while (!t_element.has_keyboard_focus() && !is_past(deadline)) {
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
}

[[nodiscard]] auto is_in_front(HWND t_window) -> bool
{
	return GetAncestor(GetForegroundWindow(), GA_ROOT) == t_window;
}

struct LoginFields {
	UiElement username;
	UiElement password;
	bool      by_name = false;

	[[nodiscard]] auto found() const -> bool
	{
		return username.is_valid() && password.is_valid();
	}
};

[[nodiscard]] auto find_login_fields(const UiAutomation& t_automation, const UiElement& t_window) -> LoginFields
{
	LoginFields by_name{
		.username = t_automation.find_descendant(t_window, K_USERNAME_FIELD_NAME, UIA_EditControlTypeId),
		.password = t_automation.find_descendant(t_window, K_PASSWORD_FIELD_NAME, UIA_EditControlTypeId),
		.by_name  = true,
	};
	if (by_name.found()) return by_name;

	// A client in another language names the fields differently, so fall back to the password box and the plain box beside it.
	const UiElement password = t_automation.find_edit(t_window, true);
	if (!password.is_valid()) return {};

	return LoginFields{.username = t_automation.find_edit(t_window, false), .password = password, .by_name = false};
}

[[nodiscard]] auto shown_login_error(const UiAutomation& t_automation, const UiElement& t_window, bool t_fields_found_by_name) -> std::optional<std::string>
{
	const UiElement named_error = t_automation.find_descendant(t_window, K_LOGIN_ERROR_TOOLTIP_NAME);
	if (named_error.is_valid()) {
		const UiElement tooltip = t_automation.find_descendant(t_window, K_LOGIN_ERROR_TOOLTIP_NAME, UIA_ToolTipControlTypeId);
		return to_utf8(t_automation.text_inside(tooltip.is_valid() ? tooltip : named_error, K_LOGIN_ERROR_TOOLTIP_NAME));
	}

	if (t_fields_found_by_name) return std::nullopt;

	const UiElement tooltip = t_automation.find_of_type(t_window, UIA_ToolTipControlTypeId);
	if (!tooltip.is_valid()) return std::nullopt;

	return to_utf8(t_automation.text_inside(tooltip, K_LOGIN_ERROR_TOOLTIP_NAME));
}

[[nodiscard]] auto launched_game_process_ids() -> std::vector<u32>
{
	std::vector<u32> process_ids;

	for_each_process([&process_ids](const PROCESSENTRY32W& t_entry) {
		if (matches_any(t_entry.szExeFile, K_STARTED_GAME_PROCESS_NAMES)) {
			process_ids.push_back(t_entry.th32ProcessID);
		}
	});

	return process_ids;
}
}

struct RiotClient::Native {
	HANDLE                                process       = nullptr;
	u32                                   process_id    = 0;
	HWND                                  cached_window = nullptr;
	UiElement                             cached_window_element;
	std::chrono::steady_clock::time_point cached_window_expiry;
	std::optional<UiAutomation>           automation;
	HiddenClient                          hidden;
	std::vector<u32>                      games_before_hiding;
	bool                                  password_typed_unsent = false;

	[[nodiscard]] auto find_client_window() const -> HWND;
	[[nodiscard]] auto current_window_element(const UiAutomation* t_automation) -> UiElement;
	auto wait_for_responsive_window(const std::atomic<bool>* t_cancel) const -> HWND;
	[[nodiscard]] auto should_stop(const std::atomic<bool>* t_cancel) const -> bool;
	[[nodiscard]] auto bring_client_in_front(const UiElement& t_field, const std::atomic<bool>* t_cancel) const -> bool;
	[[nodiscard]] auto fill_field(const UiElement& t_field, const std::wstring& t_value, const std::atomic<bool>* t_cancel) -> bool;
	[[nodiscard]] auto wait_for_sign_in_arrow(const UiElement& t_window, ElementBounds t_password_field, std::span<const FormButton> t_before_filling, const std::atomic<bool>* t_cancel) const -> std::optional<UiButton>;
	[[nodiscard]] auto was_press_taken(const UiElement& t_window, bool t_error_shown_before, const std::atomic<bool>* t_cancel) const -> bool;
	[[nodiscard]] auto press_sign_in_arrow(const UiElement& t_window, const UiElement& t_password_field, std::span<const FormButton> t_before_filling, const std::atomic<bool>* t_cancel) -> bool;
	[[nodiscard]] auto submit_with_enter(const UiElement& t_password_field, const std::atomic<bool>* t_cancel) const -> bool;
	auto clear_password_field() -> void;
};

auto RiotClient::Native::find_client_window() const -> HWND
{
	const HWND window = main_client_window();
	if (window != nullptr || process_id == 0) return window;

	return UiAutomation::find_top_level_window(process_id);
}

auto RiotClient::Native::current_window_element(const UiAutomation* t_automation) -> UiElement
{
	const HWND window = find_client_window();
	if (window == nullptr) return {};

	const auto now = std::chrono::steady_clock::now();
	if (window == cached_window && cached_window_element.is_valid() && now < cached_window_expiry) {
		return cached_window_element;
	}

	UiElement element     = t_automation->element_from_window(window);
	cached_window         = element.is_valid() ? window : nullptr;
	cached_window_element = element;
	cached_window_expiry  = now + K_WINDOW_ELEMENT_LIFETIME;

	return element;
}

auto RiotClient::Native::wait_for_responsive_window(const std::atomic<bool>* t_cancel) const -> HWND
{
	const auto started = std::chrono::steady_clock::now();

	debug_log::write(K_LOG_CATEGORY, "waiting for a responsive client window (until cancelled)");

	u32 polls                = 0;
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
			const auto waited    = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);

			debug_log::write(K_LOG_CATEGORY, "still waiting for a responsive client window after %lldms (%s)", waited.count(), window == nullptr ? "no window yet" : "window is not pumping messages");
		}

		if (is_cancelled(t_cancel)) {
			debug_log::write(K_LOG_CATEGORY, "stopped waiting for a responsive client window (cancelled)");
			return nullptr;
		}

		std::this_thread::sleep_for(K_POLL_INTERVAL);
	}
}

// Keystrokes go to whichever window is in front, so nothing is typed unless the client is that window.
auto RiotClient::Native::bring_client_in_front(const UiElement& t_field, const std::atomic<bool>* t_cancel) const -> bool
{
	const HWND window = find_client_window();
	if (window == nullptr) return false;

	if (!is_in_front(window) || !t_field.has_keyboard_focus()) {
		std::ignore = activate_window_unless_cancelled(window, true, t_cancel);
		t_field.focus();
		wait_for_keyboard_focus(t_field);
	}

	const bool in_front = is_in_front(window);
	debug_log::write(K_LOG_CATEGORY, "client %s in front, field %s keyboard focus", in_front ? "is" : "is NOT", t_field.has_keyboard_focus() ? "has" : "does NOT have");

	return in_front;
}

auto RiotClient::Native::should_stop(const std::atomic<bool>* t_cancel) const -> bool
{
	return is_cancelled(t_cancel) || hidden.was_opened_by_user();
}

auto RiotClient::Native::fill_field(const UiElement& t_field, const std::wstring& t_value, const std::atomic<bool>* t_cancel) -> bool
{
	hidden.note_own_action();
	if (t_field.set_value(t_value)) return true;

	if (hidden.is_hiding()) {
		debug_log::write(K_LOG_CATEGORY, "not typing into the hidden client - its field refused the value");
		return false;
	}

	if (!bring_client_in_front(t_field, t_cancel)) return false;

	automation->type_text(t_value.c_str());

	return true;
}

auto RiotClient::Native::wait_for_sign_in_arrow(const UiElement& t_window, ElementBounds t_password_field, std::span<const FormButton> t_before_filling, const std::atomic<bool>* t_cancel) const -> std::optional<UiButton>
{
	const auto deadline = deadline_after(K_SIGN_IN_ARROW_WAIT_MS);

	while (!is_past(deadline) && !should_stop(t_cancel)) {
		const std::vector<UiButton> buttons = automation->find_buttons(t_window);
		const std::optional<usize>  arrow   = find_sign_in_arrow(t_before_filling, form_buttons_of(buttons), t_password_field);
		if (arrow) return buttons[*arrow];

		std::this_thread::sleep_for(K_SIGN_IN_ARROW_POLL);
	}

	return std::nullopt;
}

// A press is taken when the form switches to loading, which hides the password box, or when Riot answers with an error that wasn't there before.
auto RiotClient::Native::was_press_taken(const UiElement& t_window, bool t_error_shown_before, const std::atomic<bool>* t_cancel) const -> bool
{
	const auto deadline = deadline_after(K_PRESS_CONFIRMATION_MS);

	while (!is_past(deadline) && !should_stop(t_cancel)) {
		if (!automation->find_edit(t_window, true).is_valid()) return true;
		if (!t_error_shown_before && automation->find_descendant(t_window, K_LOGIN_ERROR_TOOLTIP_NAME).is_valid()) return true;

		std::this_thread::sleep_for(K_PRESS_CONFIRMATION_POLL);
	}

	return false;
}

// The arrow has no name, so it is recognised by turning enabled once the form is filled in. Pressing it needs no focus, so nothing is ever typed into another window.
auto RiotClient::Native::press_sign_in_arrow(const UiElement& t_window, const UiElement& t_password_field, std::span<const FormButton> t_before_filling, const std::atomic<bool>* t_cancel) -> bool
{
	const std::optional<RECT> password_bounds = t_password_field.bounds();
	if (!password_bounds) return false;

	for (u32 press = 1; press <= K_MAX_SIGN_IN_PRESSES; press += 1) {
		const std::optional<UiButton> arrow = wait_for_sign_in_arrow(t_window, element_bounds_of(*password_bounds), t_before_filling, t_cancel);
		if (!arrow) {
			debug_log::write(K_LOG_CATEGORY, "the sign-in arrow never became enabled after filling the form");
			return false;
		}

		const bool error_shown_before = automation->find_descendant(t_window, K_LOGIN_ERROR_TOOLTIP_NAME).is_valid();

		hidden.note_own_action();
		if (!arrow->element.invoke()) continue;

		password_typed_unsent = false;

		if (was_press_taken(t_window, error_shown_before, t_cancel)) {
			debug_log::write(K_LOG_CATEGORY, "pressed the sign-in arrow and the form took it (press %u)", press);
			return true;
		}

		debug_log::write(K_LOG_CATEGORY, "sign-in press %u of %u wasn't taken", press, K_MAX_SIGN_IN_PRESSES);
	}

	return false;
}

auto RiotClient::Native::submit_with_enter(const UiElement& t_password_field, const std::atomic<bool>* t_cancel) const -> bool
{
	if (!bring_client_in_front(t_password_field, t_cancel)) {
		debug_log::write(K_LOG_CATEGORY, "not pressing Enter - the client is not in front");
		return false;
	}

	debug_log::write(K_LOG_CATEGORY, "submitting the login form with Enter");
	automation->press_key(VK_RETURN);

	return true;
}

// Handing the client back before the sign-in was sent leaves nothing Pulsar typed in its password box.
auto RiotClient::Native::clear_password_field() -> void
{
	if (!password_typed_unsent) return;

	password_typed_unsent = false;

	const std::atomic<bool> never_cancelled{false};
	UiAutomation            cleanup(&never_cancelled);
	const HWND              window = find_client_window();
	if (window == nullptr || !cleanup.init()) return;

	const UiElement field = cleanup.find_edit(cleanup.element_from_window(window), true);
	if (field.set_value(L"")) {
		debug_log::write(K_LOG_CATEGORY, "cleared the password box before handing the client back");
	}
}

RiotClient::RiotClient(const std::atomic<bool>* t_cancel)
	: m_cancel(t_cancel)
	, m_native(std::make_unique<Native>())
{
}

RiotClient::~RiotClient()
{
	if (m_native->process != nullptr) {
		CloseHandle(m_native->process);
	}
}

auto RiotClient::prepare_automation() -> void
{
	UiAutomation::keep_process_mta_alive();
}

auto RiotClient::show_windows_left_hidden() -> void
{
	HiddenClient::show_windows_left_hidden();
}

auto RiotClient::show_hidden_windows_after_crash() -> void
{
	HiddenClient::show_hidden_windows_after_crash();
}

auto RiotClient::running_game() -> std::optional<std::string_view>
{
	std::optional<std::string_view> running;

	for_each_process([&running](const PROCESSENTRY32W& t_entry) {
		const auto is_this_game = [&t_entry](const GameProcess& t_game) { return CompareStringOrdinal(t_entry.szExeFile, -1, t_game.executable, -1, TRUE) == CSTR_EQUAL; };
		const auto game         = std::ranges::find_if(K_GAME_PROCESSES, is_this_game);

		if (!running && game != std::end(K_GAME_PROCESSES)) {
			running = game->launch_product;
		}
	});

	const std::string_view logged = running.value_or("none");
	debug_log::write(K_LOG_CATEGORY, "running game -> %.*s", static_cast<int>(logged.size()), logged.data());

	return running;
}

auto RiotClient::kill_all_client_processes(const std::atomic<bool>* t_cancel) -> void
{
	debug_log::write(K_LOG_CATEGORY, "killing every known Riot Client process");

	const std::vector<HANDLE> terminated = terminate_processes(K_CLIENT_PROCESS_NAMES);

	const debug_log::Scope scope(K_LOG_CATEGORY, "wait for %zu killed client process(es) to exit", terminated.size());
	wait_for_processes_to_exit(terminated, t_cancel);

	for (const HANDLE process : terminated) {
		CloseHandle(process);
	}

	debug_log::write(K_LOG_CATEGORY, "killed client processes gone after %llums", scope.elapsed_ms());
}

auto RiotClient::automation_failure_message() -> const char*
{
	return K_AUTOMATION_FAILED_MESSAGE;
}

auto RiotClient::request_automation_permission() -> bool
{
	return true;
}

auto RiotClient::permission_missing_message() -> const char*
{
	return "";
}

auto RiotClient::open_automation_permission_settings() -> void {}

auto RiotClient::supports_product(std::string_view) -> bool
{
	return true;
}

auto RiotClient::forwards_launch_to_open_client() -> bool
{
	return true;
}

auto RiotClient::executable_name() -> const char*
{
	return K_CLIENT_EXECUTABLE_NAME_UTF8;
}

auto RiotClient::default_install_folder() -> std::string
{
	return K_DEFAULT_INSTALL_FOLDER;
}

auto RiotClient::executable_near(std::string_view t_chosen_path) -> std::string
{
	std::wstring folder = to_wide(t_chosen_path);

	for (u32 level = 0; level < K_CHOSEN_PATH_SEARCH_LEVELS; level += 1) {
		const usize slash = folder.find_last_of(L"\\/");
		if (slash == std::wstring::npos) break;

		folder.resize(slash);

		for (const std::wstring& candidate : {folder + L"\\" + K_CLIENT_EXECUTABLE_NAME, folder + L"\\" + K_CLIENT_FOLDER_NAME + L"\\" + K_CLIENT_EXECUTABLE_NAME}) {
			if (is_file(candidate)) return to_utf8(candidate);
		}
	}

	return {};
}

auto RiotClient::resolve_executable_path(std::string_view t_remembered_path) -> bool
{
	const std::wstring remembered = to_wide(t_remembered_path);

	if (!remembered.empty() && is_file(remembered)) {
		m_executable_path = std::string{t_remembered_path};
		debug_log::write(K_LOG_CATEGORY, "using the remembered Riot Client executable: %s", m_executable_path.c_str());

		return true;
	}

	if (!remembered.empty()) {
		debug_log::write(K_LOG_CATEGORY, "the remembered Riot Client executable is gone, searching again");
	}

	m_executable_path.clear();

	for (const std::wstring& candidate : client_path_candidates()) {
		if (!is_file(candidate)) continue;

		m_executable_path = to_utf8(candidate);
		debug_log::write(K_LOG_CATEGORY, "resolved Riot Client executable: %s", m_executable_path.c_str());

		return true;
	}

	debug_log::write(K_LOG_CATEGORY, "no Riot Client executable found in the installs file, registry, running processes or default folders");

	return false;
}

auto RiotClient::launch(std::string_view t_launch_product) -> bool
{
	if (m_executable_path.empty()) return false;

	const std::wstring executable   = to_wide(m_executable_path);
	std::wstring       command_line = L"\"" + executable + L"\"";
	if (!t_launch_product.empty()) {
		command_line += L" --launch-product=" + to_wide(t_launch_product) + L" --launch-patchline=live";
	}

	STARTUPINFOW        startup_info{.cb = sizeof(startup_info)};
	PROCESS_INFORMATION process_info{};

	if (!CreateProcessW(executable.c_str(), command_line.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup_info, &process_info)) {
		debug_log::write(K_LOG_CATEGORY, "CreateProcessW FAILED err=%lu for %s", GetLastError(), m_executable_path.c_str());
		return false;
	}

	debug_log::write(K_LOG_CATEGORY, "launched Riot Client pid %lu (%ls)", process_info.dwProcessId, command_line.c_str());
	CloseHandle(process_info.hThread);

	if (m_native->process != nullptr) {
		CloseHandle(m_native->process);
	}

	m_native->process    = process_info.hProcess;
	m_native->process_id = process_info.dwProcessId;

	return true;
}

auto RiotClient::has_open_window() const -> bool
{
	const HWND window = m_native->find_client_window();

	return window != nullptr && is_window_responsive(window);
}

auto RiotClient::bring_to_front() -> void
{
	const HWND window = m_native->wait_for_responsive_window(m_cancel);

	if (window != nullptr) {
		std::ignore = activate_window_unless_cancelled(window, false, m_cancel);
	}
}

auto RiotClient::start_automation() -> bool
{
	m_native->automation.emplace(m_cancel);
	return m_native->automation->init();
}

auto RiotClient::stop_automation() -> void
{
	m_native->cached_window         = nullptr;
	m_native->cached_window_element = UiElement{};
	m_native->automation.reset();
}

auto RiotClient::start_hiding(bool t_show_live_view) -> bool
{
	m_native->games_before_hiding = launched_game_process_ids();

	return m_native->hidden.start(t_show_live_view);
}

auto RiotClient::is_hiding() const -> bool
{
	return m_native->hidden.is_hiding();
}

auto RiotClient::watch_for_user_opening() -> void
{
	m_native->hidden.watch_for_user_opening();
}

auto RiotClient::was_opened_by_user() const -> bool
{
	return m_native->hidden.was_opened_by_user();
}

auto RiotClient::has_game_started() const -> bool
{
	const auto is_new = [this](u32 t_process_id) { return std::ranges::find(m_native->games_before_hiding, t_process_id) == m_native->games_before_hiding.end(); };

	return std::ranges::any_of(launched_game_process_ids(), is_new);
}

auto RiotClient::show_progress(std::string_view t_progress) -> void
{
	m_native->hidden.show_progress(t_progress);
}

auto RiotClient::hand_back(HandBack t_mode) -> void
{
	if (t_mode == HandBack::SHOWN) {
		m_native->clear_password_field();
	}

	m_native->hidden.hand_back(t_mode);
}

auto RiotClient::look() -> ClientView
{
	if (!m_native->automation) return {};

	const UiAutomation& automation = *m_native->automation;
	const UiElement     window     = m_native->current_window_element(&automation);
	if (!window.is_valid()) return {};

	ClientView view{.window_shown = true};

	const LoginFields fields = find_login_fields(automation, window);
	if (fields.found()) {
		view.login_form_shown = true;
		view.login_error      = shown_login_error(automation, window, fields.by_name);
		return view;
	}

	const UiElement play = automation.find_descendant(window, K_PLAY_BUTTON_NAME, UIA_ButtonControlTypeId);
	view.play_shown      = play.is_valid();
	view.play_enabled    = play.is_enabled();

	return view;
}

auto RiotClient::fill_and_submit(std::string_view t_username, std::string_view t_password) -> bool
{
	if (!m_native->automation) return false;

	const UiAutomation& automation = *m_native->automation;
	const UiElement     window     = m_native->current_window_element(&automation);
	const LoginFields   fields     = window.is_valid() ? find_login_fields(automation, window) : LoginFields{};

	if (!fields.found()) {
		debug_log::write(K_LOG_CATEGORY, "the login form went away before it could be filled");
		return false;
	}

	const std::vector<FormButton> before_filling = form_buttons_of(automation.find_buttons(window));
	const WideSecret              username(t_username);
	const WideSecret              password(t_password);

	if (!m_native->fill_field(fields.username, username.text(), m_cancel) || !m_native->fill_field(fields.password, password.text(), m_cancel)) {
		debug_log::write(K_LOG_CATEGORY, "the login form could not be filled");
		return false;
	}

	m_native->password_typed_unsent = true;

	if (m_native->press_sign_in_arrow(window, fields.password, before_filling, m_cancel)) return true;

	if (is_hiding()) {
		debug_log::write(K_LOG_CATEGORY, "not falling back to Enter while the client is hidden");
		return false;
	}

	const bool submitted            = m_native->submit_with_enter(fields.password, m_cancel);
	m_native->password_typed_unsent = !submitted;

	return submitted;
}

auto RiotClient::press_play() -> bool
{
	if (!m_native->automation) return false;

	const UiAutomation& automation = *m_native->automation;
	const UiElement     window     = m_native->current_window_element(&automation);
	const UiElement     play       = window.is_valid() ? automation.find_descendant(window, K_PLAY_BUTTON_NAME, UIA_ButtonControlTypeId) : UiElement{};

	debug_log::write(K_LOG_CATEGORY, play.is_valid() ? "pressing Play" : "Play went away before it could be pressed");

	return play.invoke();
}
