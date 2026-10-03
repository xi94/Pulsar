#include "login/riot_client.h"

#include <algorithm>
#include <concepts>
#include <cwchar>
#include <fstream>
#include <optional>
#include <span>
#include <thread>
#include <vector>

#include <Windows.h>
#include <TlHelp32.h>

#include <nlohmann/json.hpp>

#include "core/debug_log.h"
#include "core/thread_util.h"
#include "login/win32/ui_automation.h"
#include "os/win32/win32.h"

using os::win32::to_utf8;
using os::win32::to_wide;

namespace {
constexpr const char* K_LOG_CATEGORY = "riot";

constexpr const wchar_t*    K_INSTALLS_JSON_NAME = L"Riot Games\\RiotClientInstalls.json";
constexpr const char*       K_INSTALLS_JSON_KEYS[]{"rc_default", "rc_live", "rc_beta"};
constexpr const wchar_t*    K_CLIENT_EXECUTABLE_NAME      = L"RiotClientServices.exe";
constexpr const char*       K_CLIENT_EXECUTABLE_NAME_UTF8 = "RiotClientServices.exe";
constexpr const char*       K_DEFAULT_INSTALL_FOLDER      = "C:\\Riot Games\\Riot Client";
constexpr const wchar_t*    K_DEFAULT_CLIENT_FOLDER       = L"Riot Games\\Riot Client\\";
constexpr const wchar_t*    K_CLIENT_FOLDER_NAME          = L"Riot Client";
constexpr u32               K_CHOSEN_PATH_SEARCH_LEVELS   = 3;
constexpr const wchar_t*    K_PROTOCOL_COMMAND_KEY        = L"Software\\Classes\\riotclient\\shell\\open\\command";
constexpr const wchar_t*    K_UNINSTALL_KEY               = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
constexpr std::wstring_view K_RIOT_UNINSTALL_PREFIX       = L"Riot Game ";

constexpr const wchar_t* K_CLIENT_WINDOW_TITLE        = L"Riot Client";
constexpr const wchar_t* K_USERNAME_FIELD_NAME        = L"USERNAME";
constexpr const wchar_t* K_PASSWORD_FIELD_NAME        = L"PASSWORD";
constexpr const wchar_t* K_PLAY_BUTTON_NAME           = L"Play";
constexpr const wchar_t* K_LOGIN_ERROR_TOOLTIP_NAME   = L"Login error";
constexpr const wchar_t* K_INVALID_CREDENTIALS_REASON = L"Your login credentials don't match an account in our system.";
constexpr const wchar_t* K_UNRECOGNIZED_ERROR_REASON  = L"";
constexpr const wchar_t* K_TROUBLE_SIGNING_IN_REASON  = L"Sorry, we're having trouble signing you in right now. Please try again later.";

constexpr const wchar_t* K_CLIENT_PROCESS_NAMES[]{
	L"Riot Client.exe", L"RiotClientServices.exe", L"RiotClientUx.exe", L"RiotClientUxRender.exe", L"LeagueClient.exe", L"LeagueClientUx.exe", L"LoR.exe",
};

constexpr const wchar_t* K_GAME_PROCESS_NAMES[]{
	L"VALORANT-Win64-Shipping.exe",
	L"League of Legends.exe",
};

constexpr auto K_WINDOW_ELEMENT_LIFETIME = std::chrono::milliseconds(2000);
constexpr auto K_POLL_INTERVAL           = std::chrono::milliseconds(100);
constexpr u32  K_RESPONSIVENESS_PROBE_MS = 750;
constexpr u32  K_FOCUS_SETTLE_MS         = 500;
constexpr u32  K_FORM_GONE_POLLS         = 30;
constexpr u32  K_FORM_STUCK_POLLS        = 150;

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
	std::ifstream file(program_data_folder() + L"\\" + K_INSTALLS_JSON_NAME);
	if (!file.is_open()) return;

	const nlohmann::json installs = nlohmann::json::parse(file, nullptr, false);
	if (!installs.is_object()) return;

	for (const char* key : K_INSTALLS_JSON_KEYS) {
		const auto path = installs.find(key);
		if (path == installs.end() || !path->is_string()) continue;

		std::wstring wide = to_wide(path->get_ref<const std::string&>());
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

auto fill_field(const UiAutomation& t_automation, const UiElement& t_field, const std::wstring& t_value) -> void
{
	if (t_field.set_value(t_value.c_str())) return;

	t_field.focus();
	wait_for_keyboard_focus(t_field);
	t_automation.type_text(t_value.c_str());
}

[[nodiscard]] auto login_error_reason(const UiAutomation& t_automation, const UiElement& t_window) -> std::wstring
{
	for (const wchar_t* reason : {K_INVALID_CREDENTIALS_REASON, K_TROUBLE_SIGNING_IN_REASON}) {
		if (t_automation.find_descendant(t_window, reason).is_valid()) return reason;
	}

	return K_LOGIN_ERROR_TOOLTIP_NAME;
}

[[nodiscard]] auto find_login_fields(const UiAutomation& t_automation, const UiElement& t_window, UiElement* t_username, UiElement* t_password, bool* t_by_name)
	-> bool
{
	*t_username = t_automation.find_descendant(t_window, K_USERNAME_FIELD_NAME, UIA_EditControlTypeId);
	*t_password = t_automation.find_descendant(t_window, K_PASSWORD_FIELD_NAME, UIA_EditControlTypeId);
	*t_by_name  = true;
	if (t_username->is_valid() && t_password->is_valid()) return true;

	// A client in another language names the fields differently, so fall back to the password box and the plain box beside it.
	*t_password = t_automation.find_edit(t_window, true);
	*t_username = t_password->is_valid() ? t_automation.find_edit(t_window, false) : UiElement{};
	*t_by_name  = false;

	return t_username->is_valid() && t_password->is_valid();
}

[[nodiscard]] auto is_login_form_shown(const UiAutomation& t_automation, const UiElement& t_window, bool t_by_name) -> bool
{
	if (!t_by_name) return t_automation.find_edit(t_window, true).is_valid();

	return t_automation.find_descendant(t_window, K_USERNAME_FIELD_NAME, UIA_EditControlTypeId).is_valid() ||
	       t_automation.find_descendant(t_window, K_PASSWORD_FIELD_NAME, UIA_EditControlTypeId).is_valid();
}

[[nodiscard]] auto shown_login_error(const UiAutomation& t_automation, const UiElement& t_window, bool t_by_name) -> std::optional<std::wstring>
{
	if (t_automation.find_descendant(t_window, K_LOGIN_ERROR_TOOLTIP_NAME).is_valid()) return login_error_reason(t_automation, t_window);
	if (!t_by_name && t_automation.find_of_type(t_window, UIA_ToolTipControlTypeId).is_valid()) return std::wstring{K_UNRECOGNIZED_ERROR_REASON};

	return std::nullopt;
}
}

struct RiotClient::Native {
	HANDLE                                process       = nullptr;
	u32                                   process_id    = 0;
	HWND                                  cached_window = nullptr;
	UiElement                             cached_window_element;
	std::chrono::steady_clock::time_point cached_window_expiry;
	bool                                  form_found_by_name = true;
	std::optional<UiAutomation>           automation;

	[[nodiscard]] auto find_client_window() const -> HWND;
	[[nodiscard]] auto current_window_element(const UiAutomation* t_automation) -> UiElement;
	auto wait_for_responsive_window(const std::atomic<bool>* t_cancel) const -> HWND;
};

auto RiotClient::Native::find_client_window() const -> HWND
{
	const HWND window = FindWindowW(nullptr, K_CLIENT_WINDOW_TITLE);
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

			debug_log::write(K_LOG_CATEGORY, "still waiting for a responsive client window after %lldms (%s)", waited.count(),
			                 window == nullptr ? "no window yet" : "window is not pumping messages");
		}

		if (is_cancelled(t_cancel)) {
			debug_log::write(K_LOG_CATEGORY, "stopped waiting for a responsive client window (cancelled)");
			return nullptr;
		}

		std::this_thread::sleep_for(K_POLL_INTERVAL);
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

auto RiotClient::is_game_in_progress() -> bool
{
	bool in_progress = false;
	for_each_process([&in_progress](const PROCESSENTRY32W& t_entry) { in_progress = in_progress || matches_any(t_entry.szExeFile, K_GAME_PROCESS_NAMES); });

	debug_log::write(K_LOG_CATEGORY, "is_game_in_progress -> %s", in_progress ? "yes" : "no");

	return in_progress;
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

		for (const std::wstring& candidate :
		     {folder + L"\\" + K_CLIENT_EXECUTABLE_NAME, folder + L"\\" + K_CLIENT_FOLDER_NAME + L"\\" + K_CLIENT_EXECUTABLE_NAME}) {
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

auto RiotClient::wait_for_responsive_window() -> void
{
	m_native->wait_for_responsive_window(m_cancel);
}

auto RiotClient::bring_to_foreground() -> bool
{
	const HWND window = m_native->wait_for_responsive_window(m_cancel);

	return window != nullptr && activate_window_unless_cancelled(window, false, m_cancel);
}

auto RiotClient::take_keyboard_focus() -> bool
{
	const HWND window = m_native->wait_for_responsive_window(m_cancel);

	return window != nullptr && activate_window_unless_cancelled(window, true, m_cancel);
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

auto RiotClient::submit_login(std::string_view t_username, std::string_view t_password) -> bool
{
	if (!m_native->automation) return false;

	const UiAutomation* automation = &*m_native->automation;
	UiElement           username_field;
	UiElement           password_field;
	u32                 polls = 0;

	debug_log::write(K_LOG_CATEGORY, "looking for the login form (until cancelled)");

	for (;;) {
		const UiElement window = m_native->current_window_element(automation);
		if (window.is_valid()) {
			if (find_login_fields(*automation, window, &username_field, &password_field, &m_native->form_found_by_name)) {
				debug_log::write(K_LOG_CATEGORY, "login form found after %u poll(s) (%s)", polls, m_native->form_found_by_name ? "by name" : "by field type");
				break;
			}
		}

		polls += 1;

		if (is_cancelled(m_cancel)) {
			debug_log::write(K_LOG_CATEGORY, "login form NOT found after %u poll(s) (cancelled; window %s, username %s, password %s)", polls,
			                 window.is_valid() ? "yes" : "no", username_field.is_valid() ? "yes" : "no", password_field.is_valid() ? "yes" : "no");
			return false;
		}

		std::this_thread::sleep_for(K_POLL_INTERVAL);
	}

	fill_field(*automation, username_field, to_wide(t_username));
	fill_field(*automation, password_field, to_wide(t_password));

	password_field.focus();
	wait_for_keyboard_focus(password_field);

	debug_log::write(K_LOG_CATEGORY, "submitting the login form (password field %s real keyboard focus)",
	                 password_field.has_keyboard_focus() ? "has" : "does NOT have");
	automation->press_key(VK_RETURN);

	return true;
}

auto RiotClient::wait_for_login_result(std::string* t_out_error, const std::string* t_error_to_ignore) -> bool
{
	if (!m_native->automation) {
		t_out_error->clear();
		return true;
	}

	const UiAutomation* automation         = &*m_native->automation;
	bool                saw_no_tooltip     = t_error_to_ignore == nullptr;
	u32                 polls_without_form = 0;
	u32                 polls_with_form    = 0;

	while (!is_cancelled(m_cancel)) {
		const UiElement window = m_native->current_window_element(automation);

		if (window.is_valid()) {
			if (const std::optional<std::wstring> shown = shown_login_error(*automation, window, m_native->form_found_by_name)) {
				std::string message = to_utf8(*shown);

				if (saw_no_tooltip || message != *t_error_to_ignore) {
					debug_log::write(K_LOG_CATEGORY, "login error shown: \"%s\"", message.c_str());
					*t_out_error = std::move(message);
					return true;
				}

				debug_log::write(K_LOG_CATEGORY, "ignoring the previous attempt's error tooltip, still on screen");
			} else {
				saw_no_tooltip = true;
			}

			if (automation->find_descendant(window, K_PLAY_BUTTON_NAME, UIA_ButtonControlTypeId).is_valid()) {
				debug_log::write(K_LOG_CATEGORY, "the Play button is up - the client signed in");
				return false;
			}

			const bool form_shown = is_login_form_shown(*automation, window, m_native->form_found_by_name);
			polls_without_form    = form_shown ? 0 : polls_without_form + 1;
			polls_with_form       = form_shown ? polls_with_form + 1 : 0;

			// Without readable error text, a form that never went away after submitting is the only sign of a refused sign-in.
			if (!m_native->form_found_by_name && polls_with_form >= K_FORM_STUCK_POLLS) {
				debug_log::write(K_LOG_CATEGORY, "the login form never went away - treating it as a refused sign-in");
				t_out_error->clear();
				return true;
			}

			// The client hides the form while it talks to Riot, so only a long absence counts as signed in.
			if (polls_without_form >= K_FORM_GONE_POLLS) {
				debug_log::write(K_LOG_CATEGORY, "the login form stayed gone - the client signed in");
				return false;
			}
		}

		std::this_thread::sleep_for(K_POLL_INTERVAL);
	}

	debug_log::write(K_LOG_CATEGORY, "stopped waiting for the login result (cancelled)");

	return false;
}

auto RiotClient::click_play_when_ready(u32 t_timeout_ms, std::string* t_out_error) -> PlayResult
{
	if (!m_native->automation) return PlayResult::NotFound;

	const UiAutomation* automation = &*m_native->automation;
	const auto          deadline   = deadline_after(t_timeout_ms);

	for (;;) {
		const UiElement window = m_native->current_window_element(automation);

		if (const std::optional<std::wstring> shown = window.is_valid() ? shown_login_error(*automation, window, m_native->form_found_by_name) : std::nullopt) {
			*t_out_error = to_utf8(*shown);
			debug_log::write(K_LOG_CATEGORY, "login error shown while waiting for Play: \"%s\"", t_out_error->c_str());
			return PlayResult::LoginError;
		}

		const UiElement play_button = window.is_valid() ? automation->find_descendant(window, K_PLAY_BUTTON_NAME, UIA_ButtonControlTypeId) : UiElement{};

		if (play_button.is_valid()) {
			debug_log::write(K_LOG_CATEGORY, "Play button found - invoking it");
			play_button.invoke();
			return PlayResult::Clicked;
		}

		if (is_cancelled(m_cancel) || is_past(deadline)) {
			debug_log::write(K_LOG_CATEGORY, "Play button not found (%s) - leaving the game unlaunched", is_cancelled(m_cancel) ? "cancelled" : "timed out");
			return PlayResult::NotFound;
		}

		std::this_thread::sleep_for(K_POLL_INTERVAL);
	}
}
