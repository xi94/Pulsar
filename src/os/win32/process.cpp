#include "os/process.h"

#include <Windows.h>
#include <shellapi.h>
#include <shobjidl.h>

#include "core/app_identity.h"
#include "os/win32/win32.h"

namespace {
constexpr const wchar_t* K_KNOWN_OVERLAY_MODULES[]{
	L"DiscordHook64.dll", L"DiscordHook32.dll", L"GameOverlayRenderer64.dll", L"GameOverlayRenderer.dll", L"RTSSHooks64.dll",
};

auto shell_open(std::string_view t_target) -> void
{
	const std::wstring target = os::win32::to_wide(t_target);
	ShellExecuteW(nullptr, L"open", target.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}
}

namespace os {

struct SingleInstanceGuard::Native {
	HANDLE mutex = nullptr;
};

auto executable_path() -> std::string
{
	std::wstring path(MAX_PATH, L'\0');

	for (;;) {
		const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
		if (length == 0) return {};

		if (length < path.size()) {
			path.resize(length);
			return win32::to_utf8(path);
		}

		path.resize(path.size() * 2);
	}
}

auto launch_process(std::string_view t_executable, std::string_view t_arguments) -> void
{
	const std::wstring executable = win32::to_wide(t_executable);

	std::wstring command = L"\"" + executable + L"\"";
	if (!t_arguments.empty()) {
		command += L' ';
		command += win32::to_wide(t_arguments);
	}

	const usize        slash  = executable.find_last_of(L"\\/");
	const std::wstring folder = slash == std::wstring::npos ? std::wstring{} : executable.substr(0, slash);

	STARTUPINFOW        startup{.cb = sizeof(startup)};
	PROCESS_INFORMATION process{};

	if (CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr, folder.empty() ? nullptr : folder.c_str(), &startup,
	                   &process)) {
		CloseHandle(process.hProcess);
		CloseHandle(process.hThread);
	}
}

auto open_path(std::string_view t_path) -> void
{
	shell_open(t_path);
}

auto open_url(std::string_view t_url) -> void
{
	shell_open(t_url);
}

auto register_app_identity() -> void
{
	SetCurrentProcessExplicitAppUserModelID(os::win32::K_APP_USER_MODEL_ID);
}

auto block_injection() -> InjectionGuard
{
	using SetMitigationPolicy = BOOL(WINAPI*)(PROCESS_MITIGATION_POLICY, PVOID, SIZE_T);

	const HMODULE kernel32   = GetModuleHandleW(L"kernel32.dll");
	const auto    set_policy = kernel32 != nullptr ? reinterpret_cast<SetMitigationPolicy>(GetProcAddress(kernel32, "SetProcessMitigationPolicy")) : nullptr;
	if (set_policy == nullptr) return InjectionGuard::Unsupported;

	PROCESS_MITIGATION_EXTENSION_POINT_DISABLE_POLICY policy{};
	policy.DisableExtensionPoints = 1;

	return set_policy(ProcessExtensionPointDisablePolicy, &policy, sizeof(policy)) ? InjectionGuard::Blocked : InjectionGuard::Refused;
}

auto injected_overlay() -> std::optional<std::string>
{
	for (const wchar_t* module : K_KNOWN_OVERLAY_MODULES) {
		if (GetModuleHandleW(module) != nullptr) return win32::to_utf8(module);
	}

	return std::nullopt;
}

auto last_error() -> u32
{
	return GetLastError();
}

SingleInstanceGuard::SingleInstanceGuard()
	: m_native(std::make_unique<Native>())
{
	m_native->mutex  = CreateMutexW(nullptr, TRUE, os::win32::K_SINGLE_INSTANCE_MUTEX_NAME);
	m_first_instance = m_native->mutex != nullptr && GetLastError() != ERROR_ALREADY_EXISTS;
}

SingleInstanceGuard::~SingleInstanceGuard()
{
	release();
}

auto SingleInstanceGuard::release() -> void
{
	if (m_native->mutex == nullptr) return;

	if (m_first_instance) {
		ReleaseMutex(m_native->mutex);
	}

	CloseHandle(m_native->mutex);
	m_native->mutex = nullptr;
}

}
