#include "platform/installation.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <iterator>
#include <utility>

#include <ShlObj.h>
#include <propsys.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <wrl/client.h>

#include "core/app_identity.h"
#include "core/str.h"
#include "platform/process.h"

using Microsoft::WRL::ComPtr;

namespace {
constexpr const wchar_t *app_key = L"Software\\" PULSAR_APP_NAME;
constexpr const wchar_t *uninstall_key = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" PULSAR_APP_NAME;
constexpr const wchar_t *run_key = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr const wchar_t *run_value = L"" PULSAR_APP_NAME;
constexpr const wchar_t *setup_complete_value = L"SetupComplete";
constexpr const wchar_t *app_folder_name = L"" PULSAR_APP_NAME;
constexpr const wchar_t *data_folder_name = L"" PULSAR_APP_NAME;
constexpr const wchar_t *executable_name = L"" PULSAR_EXE_NAME;
constexpr const wchar_t *shortcut_name = L"" PULSAR_APP_NAME L".lnk";
constexpr const wchar_t *shortcut_description = L"Riot account manager";
constexpr DWORD close_wait_ms = 10000;
constexpr int delete_attempts = 20;
constexpr const wchar_t *removal_executable_variable = L"PULSAR_REMOVE_EXECUTABLE";
constexpr const wchar_t *removal_folder_variable = L"PULSAR_REMOVE_FOLDER";
constexpr auto minimum_step_time = std::chrono::milliseconds(280);

// PKEY_AppUserModel_ID, spelled out so the shortcut code needs no extra import library.
constexpr PROPERTYKEY app_user_model_id_key{
	{0x9F4C2855, 0x9F79, 0x4B39, {0xA8, 0xD0, 0xE1, 0xD4, 0x2D, 0xE1, 0xD5, 0xF3}},
	5,
};

constexpr const char *install_steps[]{"Copying Pulsar", "Creating shortcuts", "Registering Pulsar"};
constexpr const char *apply_steps[]{"Updating shortcuts"};
constexpr const char *uninstall_steps[]{"Closing Pulsar", "Removing Pulsar", "Cleaning up"};

class ComScope {
  public:
	ComScope()
		: m_initialized(SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)))
	{
	}

	~ComScope()
	{
		if (m_initialized) {
			CoUninitialize();
		}
	}

	ComScope(const ComScope &) = delete;
	ComScope &operator=(const ComScope &) = delete;

  private:
	bool m_initialized;
};

class RegistryKey {
  public:
	RegistryKey() = default;

	~RegistryKey()
	{
		if (m_handle != nullptr) {
			RegCloseKey(m_handle);
		}
	}

	RegistryKey(const RegistryKey &) = delete;
	RegistryKey &operator=(const RegistryKey &) = delete;

	bool create(const wchar_t *t_path)
	{
		return RegCreateKeyExW(HKEY_CURRENT_USER, t_path, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &m_handle, nullptr) ==
			   ERROR_SUCCESS;
	}

	bool set(const wchar_t *t_name, const std::wstring &t_value) const
	{
		const auto bytes = static_cast<DWORD>((t_value.size() + 1) * sizeof(wchar_t));

		return RegSetValueExW(m_handle, t_name, 0, REG_SZ, reinterpret_cast<const BYTE *>(t_value.c_str()), bytes) ==
			   ERROR_SUCCESS;
	}

	bool set(const wchar_t *t_name, DWORD t_value) const
	{
		return RegSetValueExW(m_handle, t_name, 0, REG_DWORD, reinterpret_cast<const BYTE *>(&t_value),
							  sizeof(t_value)) == ERROR_SUCCESS;
	}

  private:
	HKEY m_handle = nullptr;
};

std::optional<installation::Installed> g_pending_removal;

std::wstring joined(const std::wstring &t_folder, std::wstring_view t_name)
{
	if (t_folder.empty()) return std::wstring{t_name};

	const wchar_t last = t_folder.back();
	if (last == L'\\' || last == L'/') return t_folder + std::wstring{t_name};

	return t_folder + L"\\" + std::wstring{t_name};
}

std::wstring known_folder(REFKNOWNFOLDERID t_folder)
{
	PWSTR path = nullptr;
	std::wstring result;

	if (SUCCEEDED(SHGetKnownFolderPath(t_folder, KF_FLAG_DEFAULT, nullptr, &path))) {
		result = path;
	}

	CoTaskMemFree(path);

	return result;
}

std::wstring shortcut_in(REFKNOWNFOLDERID t_folder)
{
	const std::wstring folder = known_folder(t_folder);

	return folder.empty() ? std::wstring{} : joined(folder, shortcut_name);
}

std::wstring quoted(const std::wstring &t_text)
{
	return L"\"" + t_text + L"\"";
}

bool file_exists(const std::wstring &t_path)
{
	const DWORD attributes = GetFileAttributesW(t_path.c_str());

	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool folder_exists(const std::wstring &t_path)
{
	const DWORD attributes = GetFileAttributesW(t_path.c_str());

	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

std::wstring full_path(const std::wstring &t_path)
{
	const DWORD needed = GetFullPathNameW(t_path.c_str(), 0, nullptr, nullptr);
	if (needed == 0) return t_path;

	std::wstring result(needed, L'\0');
	const DWORD length = GetFullPathNameW(t_path.c_str(), needed, result.data(), nullptr);
	if (length == 0 || length >= needed) return t_path;

	result.resize(length);

	return result;
}

bool same_path(const std::wstring &t_left, const std::wstring &t_right)
{
	const std::wstring left = full_path(t_left);
	const std::wstring right = full_path(t_right);

	return CompareStringOrdinal(left.c_str(), static_cast<int>(left.size()), right.c_str(),
								static_cast<int>(right.size()), TRUE) == CSTR_EQUAL;
}

std::optional<std::wstring> read_string(const wchar_t *t_key, const wchar_t *t_value)
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

bool has_value(const wchar_t *t_key, const wchar_t *t_value)
{
	return RegGetValueW(HKEY_CURRENT_USER, t_key, t_value, RRF_RT_ANY, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
}

bool is_setup_complete()
{
	DWORD value = 0;
	DWORD bytes = sizeof(value);

	return RegGetValueW(HKEY_CURRENT_USER, app_key, setup_complete_value, RRF_RT_REG_DWORD, nullptr, &value, &bytes) ==
			   ERROR_SUCCESS &&
		   value != 0;
}

DWORD file_kilobytes(const std::wstring &t_path)
{
	WIN32_FILE_ATTRIBUTE_DATA data{};
	if (!GetFileAttributesExW(t_path.c_str(), GetFileExInfoStandard, &data)) return 0;

	const u64 bytes = (static_cast<u64>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;

	return static_cast<DWORD>((bytes + 1023) / 1024);
}

std::wstring today()
{
	SYSTEMTIME now{};
	GetLocalTime(&now);

	wchar_t date[16];
	std::swprintf(date, std::size(date), L"%04u%02u%02u", now.wYear, now.wMonth, now.wDay);

	return date;
}

bool write_shortcut(const std::wstring &t_link, const std::wstring &t_target, const std::wstring &t_folder)
{
	ComPtr<IShellLinkW> link;
	if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link)))) return false;

	link->SetPath(t_target.c_str());
	link->SetWorkingDirectory(t_folder.c_str());
	link->SetIconLocation(t_target.c_str(), 0);
	link->SetDescription(shortcut_description);

	ComPtr<IPropertyStore> properties;
	if (SUCCEEDED(link.As(&properties))) {
		const usize bytes = (std::wcslen(app_user_model_id) + 1) * sizeof(wchar_t);
		PROPVARIANT value{};
		value.vt = VT_LPWSTR;
		value.pwszVal = static_cast<PWSTR>(CoTaskMemAlloc(bytes));

		if (value.pwszVal != nullptr) {
			std::memcpy(value.pwszVal, app_user_model_id, bytes);
			properties->SetValue(app_user_model_id_key, value);
			properties->Commit();
		}

		PropVariantClear(&value);
	}

	ComPtr<IPersistFile> file;

	return SUCCEEDED(link.As(&file)) && SUCCEEDED(file->Save(t_link.c_str(), TRUE));
}

void set_shortcut(const std::wstring &t_link, bool t_wanted, const std::wstring &t_target, const std::wstring &t_folder)
{
	if (t_link.empty()) return;

	if (t_wanted) {
		write_shortcut(t_link, t_target, t_folder);
	} else {
		DeleteFileW(t_link.c_str());
	}
}

void set_start_with_windows(bool t_wanted, const std::wstring &t_executable)
{
	if (!t_wanted) {
		RegDeleteKeyValueW(HKEY_CURRENT_USER, run_key, run_value);
		return;
	}

	RegistryKey key;
	if (key.create(run_key)) {
		key.set(run_value, quoted(t_executable) + L" --startup");
	}
}

void apply_options(const std::wstring &t_location, const std::wstring &t_executable,
				   const installation::Options &t_options)
{
	set_shortcut(shortcut_in(FOLDERID_Desktop), t_options.desktop_shortcut, t_executable, t_location);
	set_shortcut(shortcut_in(FOLDERID_Programs), t_options.start_menu_shortcut, t_executable, t_location);
	set_start_with_windows(t_options.start_with_windows, t_executable);
}

bool register_uninstall(const std::wstring &t_location, const std::wstring &t_executable)
{
	RegistryKey key;
	if (!key.create(uninstall_key)) return false;

	return key.set(L"DisplayName", std::wstring{app_name_wide}) && key.set(L"DisplayVersion", to_wide(app_version)) &&
		   key.set(L"DisplayIcon", t_executable + L",0") && key.set(L"Publisher", std::wstring{app_name_wide}) &&
		   key.set(L"InstallLocation", t_location) && key.set(L"InstallDate", today()) &&
		   key.set(L"UninstallString", quoted(t_executable) + L" --uninstall") &&
		   key.set(L"ModifyPath", quoted(t_executable) + L" --setup") &&
		   key.set(L"URLInfoAbout", std::wstring{L"" PULSAR_RELEASE_REPO}) && key.set(L"NoRepair", 1) &&
		   key.set(L"EstimatedSize", file_kilobytes(t_executable));
}

std::string copy_error(DWORD t_error)
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
	std::snprintf(message, sizeof(message), "Pulsar couldn't be copied there (error %lu).",
				  static_cast<unsigned long>(t_error));

	return message;
}

bool delete_with_retries(const std::wstring &t_path)
{
	for (int attempt = 0; attempt < delete_attempts; attempt += 1) {
		if (DeleteFileW(t_path.c_str()) || GetLastError() == ERROR_FILE_NOT_FOUND) return true;

		Sleep(100);
	}

	return false;
}

// A running executable can't delete itself, so a hidden shell finishes the job once this process has exited. The
// paths travel through environment variables so characters like % or & in a folder name are never parsed by cmd.
void remove_after_exit(const installation::Installed &t_installed)
{
	wchar_t system[MAX_PATH];
	const UINT length = GetSystemDirectoryW(system, MAX_PATH);
	if (length == 0 || length >= MAX_PATH) return;

	SetEnvironmentVariableW(removal_executable_variable, t_installed.executable.c_str());
	SetEnvironmentVariableW(removal_folder_variable, t_installed.location.c_str());

	const std::wstring folder{system, length};
	const std::wstring shell = folder + L"\\cmd.exe";
	std::wstring command = quoted(shell) +
						   L" /d /c ping 127.0.0.1 -n 4 >nul & del /f /q \"%PULSAR_REMOVE_EXECUTABLE%\" "
						   L"\"%PULSAR_REMOVE_EXECUTABLE%.old\" \"%PULSAR_REMOVE_EXECUTABLE%.update\" & rmdir "
						   L"\"%PULSAR_REMOVE_FOLDER%\"";

	STARTUPINFOW startup{.cb = sizeof(startup)};
	PROCESS_INFORMATION process{};

	if (CreateProcessW(shell.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
					   folder.c_str(), &startup, &process)) {
		CloseHandle(process.hProcess);
		CloseHandle(process.hThread);
	}
}

// Only a folder named like Pulsar's own data folder is ever deleted, whatever path is passed in.
void delete_data_folder(const std::wstring &t_folder)
{
	const std::filesystem::path folder{t_folder};
	const std::wstring name = folder.filename().wstring();
	const std::wstring_view expected{data_folder_name};

	if (CompareStringOrdinal(name.c_str(), static_cast<int>(name.size()), expected.data(),
							 static_cast<int>(expected.size()), TRUE) != CSTR_EQUAL) {
		return;
	}

	std::error_code error;
	std::filesystem::remove_all(folder, error);
}

std::wstring process_executable(HANDLE t_process)
{
	std::wstring path(MAX_PATH * 2, L'\0');
	auto length = static_cast<DWORD>(path.size());
	if (!QueryFullProcessImageNameW(t_process, 0, path.data(), &length)) return {};

	path.resize(length);

	return path;
}

BOOL CALLBACK close_thread_window(HWND t_window, LPARAM)
{
	PostMessageW(t_window, WM_CLOSE, 0, 0);

	return TRUE;
}

std::wstring nearest_existing_folder(std::wstring t_path)
{
	while (!t_path.empty() && !folder_exists(t_path)) {
		const usize slash = t_path.find_last_of(L"\\/");
		if (slash == std::wstring::npos) return {};

		t_path.resize(slash);
	}

	return t_path;
}
}

namespace installation {

std::wstring default_location()
{
	const std::wstring programs = known_folder(FOLDERID_UserProgramFiles);
	if (!programs.empty()) return programs + L"\\" + app_folder_name;

	return known_folder(FOLDERID_LocalAppData) + L"\\Programs\\" + app_folder_name;
}

std::wstring with_app_folder(std::wstring_view t_folder)
{
	std::wstring folder{t_folder};
	while (folder.size() > 3 && (folder.back() == L'\\' || folder.back() == L'/')) {
		folder.pop_back();
	}

	if (folder.empty()) return folder;

	const usize slash = folder.find_last_of(L"\\/");
	const std::wstring_view last =
		slash == std::wstring::npos ? std::wstring_view{folder} : std::wstring_view{folder}.substr(slash + 1);
	const std::wstring_view name{app_folder_name};

	if (CompareStringOrdinal(last.data(), static_cast<int>(last.size()), name.data(), static_cast<int>(name.size()),
							 TRUE) == CSTR_EQUAL) {
		return folder;
	}

	if (folder.back() != L'\\' && folder.back() != L'/') {
		folder += L'\\';
	}

	return folder + app_folder_name;
}

std::optional<Installed> find_installation()
{
	const std::optional<std::wstring> location = read_string(uninstall_key, L"InstallLocation");
	if (!location || location->empty()) return std::nullopt;

	Installed installed{.location = *location, .executable = joined(*location, executable_name)};
	if (!file_exists(installed.executable)) return std::nullopt;

	installed.options.desktop_shortcut = file_exists(shortcut_in(FOLDERID_Desktop));
	installed.options.start_menu_shortcut = file_exists(shortcut_in(FOLDERID_Programs));
	installed.options.start_with_windows = has_value(run_key, run_value);

	return installed;
}

bool is_running_installed_copy()
{
	const std::optional<Installed> installed = find_installation();

	return installed && same_path(executable_path(), installed->executable);
}

bool should_offer_setup()
{
	return !is_setup_complete() && !is_running_installed_copy();
}

void mark_setup_complete()
{
	RegistryKey key;
	if (key.create(app_key)) {
		key.set(setup_complete_value, 1);
	}
}

void refresh_registration()
{
	const std::optional<Installed> installed = find_installation();
	if (!installed || !same_path(executable_path(), installed->executable)) return;

	RegistryKey key;
	if (key.create(uninstall_key)) {
		key.set(L"DisplayVersion", to_wide(app_version));
		key.set(L"EstimatedSize", file_kilobytes(installed->executable));
	}
}

bool is_main_window_open()
{
	const HWND window = FindWindowW(main_window_class_name, nullptr);
	if (window == nullptr) return false;

	DWORD process_id = 0;
	GetWindowThreadProcessId(window, &process_id);

	return process_id != GetCurrentProcessId();
}

bool close_running_app(std::wstring_view t_only_executable)
{
	const HWND window = FindWindowW(main_window_class_name, nullptr);
	if (window == nullptr) return true;

	DWORD process_id = 0;
	GetWindowThreadProcessId(window, &process_id);
	if (process_id == GetCurrentProcessId()) return true;

	const HANDLE process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);

	if (!t_only_executable.empty() && process != nullptr &&
		!same_path(process_executable(process), std::wstring{t_only_executable})) {
		CloseHandle(process);
		return true;
	}

	PostMessageW(window, RegisterWindowMessageW(quit_instance_message_name), 0, 0);

	if (process == nullptr) {
		Sleep(500);
		return FindWindowW(main_window_class_name, nullptr) == nullptr;
	}

	const DWORD result = WaitForSingleObject(process, close_wait_ms);
	CloseHandle(process);

	return result == WAIT_OBJECT_0;
}

void open_folder(const std::wstring &t_folder)
{
	ShellExecuteW(nullptr, L"open", t_folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void finish_pending_removal()
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

void Job::reset()
{
	if (m_thread.joinable()) {
		m_thread.join();
	}

	m_step.store(0, std::memory_order_release);
	m_finished.store(false, std::memory_order_release);
	m_succeeded = false;
	m_error.clear();
}

void Job::begin(Task t_task)
{
	reset();
	m_task = t_task;
	m_installed_executable.clear();
}

void Job::finish(bool t_succeeded, std::string t_error)
{
	m_succeeded = t_succeeded;
	m_error = std::move(t_error);
	m_finished.store(true, std::memory_order_release);
}

u32 Job::step_count() const
{
	switch (m_task) {
		case Task::Install:
			return static_cast<u32>(std::size(install_steps));
		case Task::Apply:
			return static_cast<u32>(std::size(apply_steps));
		case Task::Uninstall:
			return static_cast<u32>(std::size(uninstall_steps));
	}

	return 1;
}

std::string_view Job::step_label() const
{
	const u32 index = std::min(step(), step_count() - 1);

	switch (m_task) {
		case Task::Install:
			return install_steps[index];
		case Task::Apply:
			return apply_steps[index];
		case Task::Uninstall:
			return uninstall_steps[index];
	}

	return {};
}

void Job::start_install(std::wstring t_location, Options t_options)
{
	begin(Task::Install);

	m_thread = std::thread([this, location = std::move(t_location), options = t_options]() {
		const ComScope com;
		auto step_started = std::chrono::steady_clock::now();
		const auto next_step = [&](u32 t_step) {
			std::this_thread::sleep_until(step_started + minimum_step_time);
			step_started = std::chrono::steady_clock::now();
			m_step.store(t_step, std::memory_order_release);
		};

		const std::wstring executable = joined(location, executable_name);
		const int created = SHCreateDirectoryExW(nullptr, location.c_str(), nullptr);

		if (created != ERROR_SUCCESS && created != ERROR_ALREADY_EXISTS && created != ERROR_FILE_EXISTS) {
			finish(false, "Pulsar can't create that folder. Pick another one.");
			return;
		}

		const std::wstring self = executable_path();
		if (!same_path(self, executable) && !CopyFileW(self.c_str(), executable.c_str(), FALSE)) {
			finish(false, copy_error(GetLastError()));
			return;
		}

		m_installed_executable = executable;

		next_step(1);
		apply_options(location, executable,
					  Options{
						  .desktop_shortcut = options.desktop_shortcut,
						  .start_menu_shortcut = options.start_menu_shortcut,
						  .start_with_windows = false,
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

void Job::start_apply(Installed t_installed, Options t_options)
{
	begin(Task::Apply);

	m_thread = std::thread([this, installed = std::move(t_installed), options = t_options]() {
		const ComScope com;
		const auto started = std::chrono::steady_clock::now();

		apply_options(installed.location, installed.executable, options);
		std::this_thread::sleep_until(started + minimum_step_time);

		m_step.store(1, std::memory_order_release);
		finish(true, {});
	});
}

void Job::start_uninstall(Installed t_installed, std::optional<std::wstring> t_data_folder)
{
	begin(Task::Uninstall);

	m_thread = std::thread([this, installed = std::move(t_installed), data_folder = std::move(t_data_folder)]() {
		const ComScope com;
		auto step_started = std::chrono::steady_clock::now();
		const auto next_step = [&](u32 t_step) {
			std::this_thread::sleep_until(step_started + minimum_step_time);
			step_started = std::chrono::steady_clock::now();
			m_step.store(t_step, std::memory_order_release);
		};

		if (!close_running_app(installed.executable)) {
			finish(false, "Pulsar is still open. Close it and try again.");
			return;
		}

		next_step(1);

		// Files go first so a locked executable stops the uninstall before anything else has been removed.
		if (same_path(executable_path(), installed.executable)) {
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
		RegDeleteTreeW(HKEY_CURRENT_USER, uninstall_key);
		RegDeleteTreeW(HKEY_CURRENT_USER, app_key);

		if (data_folder && !data_folder->empty()) {
			delete_data_folder(*data_folder);
		}

		next_step(3);
		finish(true, {});
	});
}

FolderPicker::~FolderPicker()
{
	if (!m_thread.joinable()) return;

	// The dialog blocks its thread until it closes, so it is closed rather than waited on.
	if (!m_finished.load(std::memory_order_acquire)) {
		for (int attempt = 0; attempt < delete_attempts && m_thread_id.load(std::memory_order_acquire) == 0;
			 attempt += 1) {
			Sleep(10);
		}

		if (const DWORD thread_id = m_thread_id.load(std::memory_order_acquire); thread_id != 0) {
			EnumThreadWindows(thread_id, close_thread_window, 0);
		}
	}

	m_thread.join();
}

void FolderPicker::open(HWND t_owner, std::wstring t_initial_folder)
{
	if (is_open()) return;

	if (m_thread.joinable()) {
		m_thread.join();
	}

	m_finished.store(false, std::memory_order_release);
	m_result.reset();

	m_thread = std::thread([this, t_owner, initial = std::move(t_initial_folder)]() {
		m_thread_id.store(GetCurrentThreadId(), std::memory_order_release);
		const ComScope com;
		ComPtr<IFileOpenDialog> dialog;

		if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
			DWORD options = 0;
			dialog->GetOptions(&options);
			dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
			dialog->SetTitle(L"Choose where to install Pulsar");
			dialog->SetOkButtonLabel(L"Select folder");

			const std::wstring start = nearest_existing_folder(initial);
			ComPtr<IShellItem> start_item;
			if (!start.empty() &&
				SUCCEEDED(SHCreateItemFromParsingName(start.c_str(), nullptr, IID_PPV_ARGS(&start_item)))) {
				dialog->SetFolder(start_item.Get());
			}

			ComPtr<IShellItem> picked;
			PWSTR path = nullptr;

			if (SUCCEEDED(dialog->Show(t_owner)) && SUCCEEDED(dialog->GetResult(&picked)) &&
				SUCCEEDED(picked->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
				m_result = with_app_folder(path);
			}

			CoTaskMemFree(path);
		}

		m_finished.store(true, std::memory_order_release);
	});
}

std::optional<std::wstring> FolderPicker::take_result()
{
	if (!m_finished.load(std::memory_order_acquire)) return std::nullopt;

	if (m_thread.joinable()) {
		m_thread.join();
	}

	m_finished.store(false, std::memory_order_release);

	return std::exchange(m_result, std::nullopt);
}

}
