#include "os/installation.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <iterator>
#include <utility>

#include <Windows.h>
#include <ShlObj.h>
#include <propsys.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <wrl/client.h>

#include "core/app_identity.h"
#include "os/process.h"
#include "os/win32/win32.h"

using Microsoft::WRL::ComPtr;
using os::win32::to_utf8;
using os::win32::to_wide;

namespace {
constexpr const wchar_t* K_APP_KEY                     = L"Software\\" PULSAR_APP_NAME;
constexpr const wchar_t* K_UNINSTALL_KEY               = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" PULSAR_APP_NAME;
constexpr const wchar_t* K_RUN_KEY                     = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr const wchar_t* K_RUN_VALUE                   = L"" PULSAR_APP_NAME;
constexpr const wchar_t* K_SETUP_COMPLETE_VALUE        = L"SetupComplete";
constexpr const wchar_t* K_APP_FOLDER_NAME             = L"" PULSAR_APP_NAME;
constexpr const wchar_t* K_DATA_FOLDER_NAME            = L"" PULSAR_APP_NAME;
constexpr const wchar_t* K_EXECUTABLE_NAME             = L"" PULSAR_EXE_NAME;
constexpr const wchar_t* K_SHORTCUT_NAME               = L"" PULSAR_APP_NAME L".lnk";
constexpr const wchar_t* K_SHORTCUT_DESCRIPTION        = L"Riot account manager";
constexpr DWORD          K_CLOSE_WAIT_MS               = 10000;
constexpr int            K_DELETE_ATTEMPTS             = 20;
constexpr const wchar_t* K_REMOVAL_EXECUTABLE_VARIABLE = L"PULSAR_REMOVE_EXECUTABLE";
constexpr const wchar_t* K_REMOVAL_FOLDER_VARIABLE     = L"PULSAR_REMOVE_FOLDER";
constexpr auto           K_MINIMUM_STEP_TIME           = std::chrono::milliseconds(280);

// PKEY_AppUserModel_ID, spelled out so the shortcut code needs no extra import library.
constexpr PROPERTYKEY K_APP_USER_MODEL_ID_KEY{
	{0x9F4C2855, 0x9F79, 0x4B39, {0xA8, 0xD0, 0xE1, 0xD4, 0x2D, 0xE1, 0xD5, 0xF3}},
	5,
};

constexpr const char* K_INSTALL_STEPS[]{"Copying Pulsar", "Creating shortcuts", "Registering Pulsar"};
constexpr const char* K_APPLY_STEPS[]{"Updating shortcuts"};
constexpr const char* K_UNINSTALL_STEPS[]{"Closing Pulsar", "Removing Pulsar", "Cleaning up"};

class RegistryKey {
  public:
	RegistryKey() = default;

	~RegistryKey()
	{
		if (m_handle != nullptr) {
			RegCloseKey(m_handle);
		}
	}

	RegistryKey(const RegistryKey&)                    = delete;
	auto operator=(const RegistryKey&) -> RegistryKey& = delete;

	[[nodiscard]] auto create(const wchar_t* t_path) -> bool
	{
		return RegCreateKeyExW(HKEY_CURRENT_USER, t_path, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &m_handle, nullptr) == ERROR_SUCCESS;
	}

	auto set(const wchar_t* t_name, const std::wstring& t_value) const -> bool
	{
		const auto bytes = static_cast<DWORD>((t_value.size() + 1) * sizeof(wchar_t));

		return RegSetValueExW(m_handle, t_name, 0, REG_SZ, reinterpret_cast<const BYTE*>(t_value.c_str()), bytes) == ERROR_SUCCESS;
	}

	auto set(const wchar_t* t_name, DWORD t_value) const -> bool
	{
		return RegSetValueExW(m_handle, t_name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&t_value), sizeof(t_value)) == ERROR_SUCCESS;
	}

  private:
	HKEY m_handle = nullptr;
};

struct WideInstalled {
	std::wstring location;
	std::wstring executable;
};

std::optional<WideInstalled> g_pending_removal;

[[nodiscard]] auto widened(const os::installation::Installed& t_installed) -> WideInstalled
{
	return WideInstalled{.location = to_wide(t_installed.location), .executable = to_wide(t_installed.executable)};
}

[[nodiscard]] auto running_executable() -> std::wstring
{
	return to_wide(os::executable_path());
}

[[nodiscard]] auto joined(const std::wstring& t_folder, std::wstring_view t_name) -> std::wstring
{
	if (t_folder.empty()) return std::wstring{t_name};

	const wchar_t last = t_folder.back();
	if (last == L'\\' || last == L'/') return t_folder + std::wstring{t_name};

	return t_folder + L"\\" + std::wstring{t_name};
}

[[nodiscard]] auto known_folder(REFKNOWNFOLDERID t_folder) -> std::wstring
{
	PWSTR        path = nullptr;
	std::wstring result;

	if (SUCCEEDED(SHGetKnownFolderPath(t_folder, KF_FLAG_DEFAULT, nullptr, &path))) {
		result = path;
	}

	CoTaskMemFree(path);

	return result;
}

[[nodiscard]] auto shortcut_in(REFKNOWNFOLDERID t_folder) -> std::wstring
{
	const std::wstring folder = known_folder(t_folder);

	return folder.empty() ? std::wstring{} : joined(folder, K_SHORTCUT_NAME);
}

[[nodiscard]] auto quoted(const std::wstring& t_text) -> std::wstring
{
	return L"\"" + t_text + L"\"";
}

[[nodiscard]] auto file_exists(const std::wstring& t_path) -> bool
{
	const DWORD attributes = GetFileAttributesW(t_path.c_str());

	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

[[nodiscard]] auto full_path(const std::wstring& t_path) -> std::wstring
{
	const DWORD needed = GetFullPathNameW(t_path.c_str(), 0, nullptr, nullptr);
	if (needed == 0) return t_path;

	std::wstring result(needed, L'\0');
	const DWORD  length = GetFullPathNameW(t_path.c_str(), needed, result.data(), nullptr);
	if (length == 0 || length >= needed) return t_path;

	result.resize(length);

	return result;
}

[[nodiscard]] auto same_path(const std::wstring& t_left, const std::wstring& t_right) -> bool
{
	const std::wstring left  = full_path(t_left);
	const std::wstring right = full_path(t_right);

	return CompareStringOrdinal(left.c_str(), static_cast<int>(left.size()), right.c_str(), static_cast<int>(right.size()), TRUE) == CSTR_EQUAL;
}

[[nodiscard]] auto read_string(const wchar_t* t_key, const wchar_t* t_value) -> std::optional<std::wstring>
{
	DWORD bytes = 0;
	if (RegGetValueW(HKEY_CURRENT_USER, t_key, t_value, RRF_RT_REG_SZ, nullptr, nullptr, &bytes) != ERROR_SUCCESS) {
		return std::nullopt;
	}

	std::wstring text(bytes / sizeof(wchar_t) + 1, L'\0');
	bytes = static_cast<DWORD>(text.size() * sizeof(wchar_t));
	if (RegGetValueW(HKEY_CURRENT_USER, t_key, t_value, RRF_RT_REG_SZ, nullptr, text.data(), &bytes) != ERROR_SUCCESS) {
		return std::nullopt;
	}

	text.resize(std::wcslen(text.c_str()));

	return text;
}

[[nodiscard]] auto has_value(const wchar_t* t_key, const wchar_t* t_value) -> bool
{
	return RegGetValueW(HKEY_CURRENT_USER, t_key, t_value, RRF_RT_ANY, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
}

[[nodiscard]] auto is_setup_complete() -> bool
{
	DWORD value = 0;
	DWORD bytes = sizeof(value);

	return RegGetValueW(HKEY_CURRENT_USER, K_APP_KEY, K_SETUP_COMPLETE_VALUE, RRF_RT_REG_DWORD, nullptr, &value, &bytes) == ERROR_SUCCESS && value != 0;
}

[[nodiscard]] auto file_kilobytes(const std::wstring& t_path) -> DWORD
{
	WIN32_FILE_ATTRIBUTE_DATA data{};
	if (!GetFileAttributesExW(t_path.c_str(), GetFileExInfoStandard, &data)) return 0;

	const u64 bytes = (static_cast<u64>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;

	return static_cast<DWORD>((bytes + 1023) / 1024);
}

[[nodiscard]] auto today() -> std::wstring
{
	SYSTEMTIME now{};
	GetLocalTime(&now);

	wchar_t date[16];
	std::swprintf(date, std::size(date), L"%04u%02u%02u", now.wYear, now.wMonth, now.wDay);

	return date;
}

auto write_shortcut(const std::wstring& t_link, const std::wstring& t_target, const std::wstring& t_folder) -> bool
{
	ComPtr<IShellLinkW> link;
	if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link)))) return false;

	link->SetPath(t_target.c_str());
	link->SetWorkingDirectory(t_folder.c_str());
	link->SetIconLocation(t_target.c_str(), 0);
	link->SetDescription(K_SHORTCUT_DESCRIPTION);

	ComPtr<IPropertyStore> properties;
	if (SUCCEEDED(link.As(&properties))) {
		const usize bytes = (std::wcslen(os::win32::K_APP_USER_MODEL_ID) + 1) * sizeof(wchar_t);
		PROPVARIANT value{};
		value.vt      = VT_LPWSTR;
		value.pwszVal = static_cast<PWSTR>(CoTaskMemAlloc(bytes));

		if (value.pwszVal != nullptr) {
			std::memcpy(value.pwszVal, os::win32::K_APP_USER_MODEL_ID, bytes);
			properties->SetValue(K_APP_USER_MODEL_ID_KEY, value);
			properties->Commit();
		}

		PropVariantClear(&value);
	}

	ComPtr<IPersistFile> file;

	return SUCCEEDED(link.As(&file)) && SUCCEEDED(file->Save(t_link.c_str(), TRUE));
}

auto set_shortcut(const std::wstring& t_link, bool t_wanted, const std::wstring& t_target, const std::wstring& t_folder) -> void
{
	if (t_link.empty()) return;

	if (t_wanted) {
		write_shortcut(t_link, t_target, t_folder);
	} else {
		DeleteFileW(t_link.c_str());
	}
}

auto set_start_with_windows(bool t_wanted, const std::wstring& t_executable) -> void
{
	if (!t_wanted) {
		RegDeleteKeyValueW(HKEY_CURRENT_USER, K_RUN_KEY, K_RUN_VALUE);
		return;
	}

	RegistryKey key;
	if (key.create(K_RUN_KEY)) {
		key.set(K_RUN_VALUE, quoted(t_executable) + L" --startup");
	}
}

auto apply_options(const std::wstring& t_location, const std::wstring& t_executable, const os::installation::Options& t_options) -> void
{
	set_shortcut(shortcut_in(FOLDERID_Desktop), t_options.desktop_shortcut, t_executable, t_location);
	set_shortcut(shortcut_in(FOLDERID_Programs), t_options.start_menu_shortcut, t_executable, t_location);
	set_start_with_windows(t_options.start_with_windows, t_executable);
}

[[nodiscard]] auto register_uninstall(const std::wstring& t_location, const std::wstring& t_executable) -> bool
{
	RegistryKey key;
	if (!key.create(K_UNINSTALL_KEY)) return false;

	return key.set(L"DisplayName", std::wstring{os::win32::K_APP_NAME_WIDE}) && key.set(L"DisplayVersion", to_wide(K_APP_VERSION)) &&
	       key.set(L"DisplayIcon", t_executable + L",0") && key.set(L"Publisher", std::wstring{os::win32::K_APP_NAME_WIDE}) &&
	       key.set(L"InstallLocation", t_location) && key.set(L"InstallDate", today()) && key.set(L"UninstallString", quoted(t_executable) + L" --uninstall") &&
	       key.set(L"ModifyPath", quoted(t_executable) + L" --setup") && key.set(L"URLInfoAbout", std::wstring{L"" PULSAR_RELEASE_REPO}) &&
	       key.set(L"NoRepair", 1) && key.set(L"EstimatedSize", file_kilobytes(t_executable));
}

[[nodiscard]] auto copy_error(DWORD t_error) -> std::string
{
	switch (t_error) {
		case ERROR_ACCESS_DENIED:
		case ERROR_PRIVILEGE_NOT_HELD:
			return "Pulsar can't write to that folder. Pick another one.";
		case ERROR_SHARING_VIOLATION:
		case ERROR_LOCK_VIOLATION:
			return "Pulsar is open from that folder. Close it and try again.";
		case ERROR_DISK_FULL:
		case ERROR_HANDLE_DISK_FULL:
			return "There isn't enough space on that drive.";
		default:
			break;
	}

	char message[96];
	std::snprintf(message, sizeof(message), "Pulsar couldn't be copied there (error %lu).", static_cast<unsigned long>(t_error));

	return message;
}

[[nodiscard]] auto delete_with_retries(const std::wstring& t_path) -> bool
{
	for (int attempt = 0; attempt < K_DELETE_ATTEMPTS; attempt += 1) {
		if (DeleteFileW(t_path.c_str()) || GetLastError() == ERROR_FILE_NOT_FOUND) return true;

		Sleep(100);
	}

	return false;
}

// A running executable can't delete itself, so a hidden shell finishes the job once this process has exited. The
// paths travel through environment variables so characters like % or & in a folder name are never parsed by cmd.
auto remove_after_exit(const WideInstalled& t_installed) -> void
{
	wchar_t    system[MAX_PATH];
	const UINT length = GetSystemDirectoryW(system, MAX_PATH);
	if (length == 0 || length >= MAX_PATH) return;

	SetEnvironmentVariableW(K_REMOVAL_EXECUTABLE_VARIABLE, t_installed.executable.c_str());
	SetEnvironmentVariableW(K_REMOVAL_FOLDER_VARIABLE, t_installed.location.c_str());

	const std::wstring folder{system, length};
	const std::wstring shell   = folder + L"\\cmd.exe";
	std::wstring       command = quoted(shell) + L" /d /c ping 127.0.0.1 -n 4 >nul & del /f /q \"%PULSAR_REMOVE_EXECUTABLE%\" "
	                                             L"\"%PULSAR_REMOVE_EXECUTABLE%.old\" \"%PULSAR_REMOVE_EXECUTABLE%.update\" & rmdir "
	                                             L"\"%PULSAR_REMOVE_FOLDER%\"";

	STARTUPINFOW        startup{.cb = sizeof(startup)};
	PROCESS_INFORMATION process{};

	if (CreateProcessW(shell.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, folder.c_str(), &startup, &process)) {
		CloseHandle(process.hProcess);
		CloseHandle(process.hThread);
	}
}

// Only a folder named like Pulsar's own data folder is ever deleted, whatever path is passed in.
auto delete_data_folder(const std::wstring& t_folder) -> void
{
	const std::filesystem::path folder{t_folder};
	const std::wstring          name = folder.filename().wstring();
	const std::wstring_view     expected{K_DATA_FOLDER_NAME};

	if (CompareStringOrdinal(name.c_str(), static_cast<int>(name.size()), expected.data(), static_cast<int>(expected.size()), TRUE) != CSTR_EQUAL) {
		return;
	}

	std::error_code error;
	std::filesystem::remove_all(folder, error);
}

[[nodiscard]] auto process_executable(HANDLE t_process) -> std::wstring
{
	std::wstring path(MAX_PATH * 2, L'\0');
	auto         length = static_cast<DWORD>(path.size());
	if (!QueryFullProcessImageNameW(t_process, 0, path.data(), &length)) return {};

	path.resize(length);

	return path;
}
}

namespace os::installation {

auto is_supported() -> bool
{
	return true;
}

auto default_location() -> std::string
{
	const std::wstring programs = known_folder(FOLDERID_UserProgramFiles);
	if (!programs.empty()) return to_utf8(programs + L"\\" + K_APP_FOLDER_NAME);

	return to_utf8(known_folder(FOLDERID_LocalAppData) + L"\\Programs\\" + K_APP_FOLDER_NAME);
}

auto with_app_folder(std::string_view t_folder) -> std::string
{
	std::wstring folder = to_wide(t_folder);
	while (folder.size() > 3 && (folder.back() == L'\\' || folder.back() == L'/')) {
		folder.pop_back();
	}

	if (folder.empty()) return {};

	const usize             slash = folder.find_last_of(L"\\/");
	const std::wstring_view last  = slash == std::wstring::npos ? std::wstring_view{folder} : std::wstring_view{folder}.substr(slash + 1);
	const std::wstring_view name{K_APP_FOLDER_NAME};

	if (CompareStringOrdinal(last.data(), static_cast<int>(last.size()), name.data(), static_cast<int>(name.size()), TRUE) == CSTR_EQUAL) {
		return to_utf8(folder);
	}

	if (folder.back() != L'\\' && folder.back() != L'/') {
		folder += L'\\';
	}

	return to_utf8(folder + K_APP_FOLDER_NAME);
}

auto find_installation() -> std::optional<Installed>
{
	const std::optional<std::wstring> location = read_string(K_UNINSTALL_KEY, L"InstallLocation");
	if (!location || location->empty()) return std::nullopt;

	const std::wstring executable = joined(*location, K_EXECUTABLE_NAME);
	if (!file_exists(executable)) return std::nullopt;

	Installed installed{.location = to_utf8(*location), .executable = to_utf8(executable)};

	installed.options.desktop_shortcut    = file_exists(shortcut_in(FOLDERID_Desktop));
	installed.options.start_menu_shortcut = file_exists(shortcut_in(FOLDERID_Programs));
	installed.options.start_with_windows  = has_value(K_RUN_KEY, K_RUN_VALUE);

	return installed;
}

auto is_running_installed_copy() -> bool
{
	const std::optional<Installed> installed = find_installation();

	return installed && same_path(running_executable(), to_wide(installed->executable));
}

auto should_offer_setup() -> bool
{
	return !is_setup_complete() && !is_running_installed_copy();
}

auto mark_setup_complete() -> void
{
	RegistryKey key;
	if (key.create(K_APP_KEY)) {
		key.set(K_SETUP_COMPLETE_VALUE, 1);
	}
}

auto refresh_registration() -> void
{
	const std::optional<Installed> installed = find_installation();
	if (!installed || !same_path(running_executable(), to_wide(installed->executable))) return;

	RegistryKey key;
	if (key.create(K_UNINSTALL_KEY)) {
		key.set(L"DisplayVersion", to_wide(K_APP_VERSION));
		key.set(L"EstimatedSize", file_kilobytes(to_wide(installed->executable)));
	}
}

auto is_main_window_open() -> bool
{
	const HWND window = FindWindowW(os::win32::K_MAIN_WINDOW_CLASS_NAME, nullptr);
	if (window == nullptr) return false;

	DWORD process_id = 0;
	GetWindowThreadProcessId(window, &process_id);

	return process_id != GetCurrentProcessId();
}

auto close_running_app(std::string_view t_only_executable) -> bool
{
	const HWND window = FindWindowW(os::win32::K_MAIN_WINDOW_CLASS_NAME, nullptr);
	if (window == nullptr) return true;

	DWORD process_id = 0;
	GetWindowThreadProcessId(window, &process_id);
	if (process_id == GetCurrentProcessId()) return true;

	const HANDLE process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);

	if (!t_only_executable.empty() && process != nullptr && !same_path(process_executable(process), to_wide(t_only_executable))) {
		CloseHandle(process);
		return true;
	}

	PostMessageW(window, RegisterWindowMessageW(os::win32::K_QUIT_INSTANCE_MESSAGE_NAME), 0, 0);

	if (process == nullptr) {
		Sleep(500);
		return FindWindowW(os::win32::K_MAIN_WINDOW_CLASS_NAME, nullptr) == nullptr;
	}

	const DWORD result = WaitForSingleObject(process, K_CLOSE_WAIT_MS);
	CloseHandle(process);

	return result == WAIT_OBJECT_0;
}

auto finish_pending_removal() -> void
{
	if (g_pending_removal) {
		remove_after_exit(*g_pending_removal);
		g_pending_removal.reset();
	}
}

Job::~Job()
{
	if (m_thread.joinable()) {
		m_thread.join();
	}
}

auto Job::reset() -> void
{
	if (m_thread.joinable()) {
		m_thread.join();
	}

	m_step.store(0, std::memory_order_release);
	m_finished.store(false, std::memory_order_release);
	m_succeeded = false;
	m_error.clear();
}

auto Job::begin(Task t_task) -> void
{
	reset();
	m_task = t_task;
	m_installed_executable.clear();
}

auto Job::finish(bool t_succeeded, std::string t_error) -> void
{
	m_succeeded = t_succeeded;
	m_error     = std::move(t_error);
	m_finished.store(true, std::memory_order_release);
}

auto Job::step_count() const -> u32
{
	switch (m_task) {
		case Task::Install:
			return static_cast<u32>(std::size(K_INSTALL_STEPS));
		case Task::Apply:
			return static_cast<u32>(std::size(K_APPLY_STEPS));
		case Task::Uninstall:
			return static_cast<u32>(std::size(K_UNINSTALL_STEPS));
	}

	return 1;
}

auto Job::step_label() const -> std::string_view
{
	const u32 index = std::min(step(), step_count() - 1);

	switch (m_task) {
		case Task::Install:
			return K_INSTALL_STEPS[index];
		case Task::Apply:
			return K_APPLY_STEPS[index];
		case Task::Uninstall:
			return K_UNINSTALL_STEPS[index];
	}

	return {};
}

auto Job::start_install(const std::string& t_location, Options t_options) -> void
{
	begin(Task::Install);

	m_thread = std::thread([this, location = to_wide(t_location), options = t_options]() {
		const win32::ComScope com;
		auto                  step_started = std::chrono::steady_clock::now();
		const auto            next_step    = [&](u32 t_step) {
			std::this_thread::sleep_until(step_started + K_MINIMUM_STEP_TIME);
			step_started = std::chrono::steady_clock::now();
			m_step.store(t_step, std::memory_order_release);
		};

		const std::wstring executable = joined(location, K_EXECUTABLE_NAME);
		const int          created    = SHCreateDirectoryExW(nullptr, location.c_str(), nullptr);

		if (created != ERROR_SUCCESS && created != ERROR_ALREADY_EXISTS && created != ERROR_FILE_EXISTS) {
			finish(false, "Pulsar can't create that folder. Pick another one.");
			return;
		}

		const std::wstring self = running_executable();
		if (!same_path(self, executable) && !CopyFileW(self.c_str(), executable.c_str(), FALSE)) {
			finish(false, copy_error(GetLastError()));
			return;
		}

		m_installed_executable = to_utf8(executable);

		next_step(1);
		apply_options(location, executable,
		              Options{
						  .desktop_shortcut    = options.desktop_shortcut,
						  .start_menu_shortcut = options.start_menu_shortcut,
						  .start_with_windows  = false,
					  });

		next_step(2);
		if (!register_uninstall(location, executable)) {
			finish(false, "Pulsar couldn't be added to your apps list.");
			return;
		}

		set_start_with_windows(options.start_with_windows, executable);
		mark_setup_complete();

		next_step(3);
		finish(true, {});
	});
}

auto Job::start_apply(const Installed& t_installed, Options t_options) -> void
{
	begin(Task::Apply);

	m_thread = std::thread([this, installed = widened(t_installed), options = t_options]() {
		const win32::ComScope com;
		const auto            started = std::chrono::steady_clock::now();

		apply_options(installed.location, installed.executable, options);
		std::this_thread::sleep_until(started + K_MINIMUM_STEP_TIME);

		m_step.store(1, std::memory_order_release);
		finish(true, {});
	});
}

auto Job::start_uninstall(const Installed& t_installed, std::optional<std::string> t_data_folder) -> void
{
	begin(Task::Uninstall);

	m_thread = std::thread([this, installed = widened(t_installed), data_folder = std::move(t_data_folder)]() {
		const win32::ComScope com;
		auto                  step_started = std::chrono::steady_clock::now();
		const auto            next_step    = [&](u32 t_step) {
			std::this_thread::sleep_until(step_started + K_MINIMUM_STEP_TIME);
			step_started = std::chrono::steady_clock::now();
			m_step.store(t_step, std::memory_order_release);
		};

		if (!close_running_app(to_utf8(installed.executable))) {
			finish(false, "Pulsar is still open. Close it and try again.");
			return;
		}

		next_step(1);

		// Files go first so a locked executable stops the uninstall before anything else has been removed.
		if (same_path(running_executable(), installed.executable)) {
			g_pending_removal = installed;
		} else {
			if (!delete_with_retries(installed.executable)) {
				finish(false, "Pulsar's files are in use. Close it and try again.");
				return;
			}

			DeleteFileW((installed.executable + L".old").c_str());
			DeleteFileW((installed.executable + L".update").c_str());
			RemoveDirectoryW(installed.location.c_str());
		}

		next_step(2);
		DeleteFileW(shortcut_in(FOLDERID_Desktop).c_str());
		DeleteFileW(shortcut_in(FOLDERID_Programs).c_str());
		set_start_with_windows(false, installed.executable);
		RegDeleteTreeW(HKEY_CURRENT_USER, K_UNINSTALL_KEY);
		RegDeleteTreeW(HKEY_CURRENT_USER, K_APP_KEY);

		if (data_folder && !data_folder->empty()) {
			delete_data_folder(to_wide(*data_folder));
		}

		next_step(3);
		finish(true, {});
	});
}

}
