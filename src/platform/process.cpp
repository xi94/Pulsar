#include "platform/process.h"

#include <shobjidl.h>

#include "core/app_identity.h"

namespace {
constexpr const wchar_t* K_KNOWN_OVERLAY_MODULES[]{
	L"DiscordHook64.dll", L"DiscordHook32.dll", L"GameOverlayRenderer64.dll", L"GameOverlayRenderer.dll", L"RTSSHooks64.dll",
};
}

[[nodiscard]] auto executable_path() -> std::wstring
{
	std::wstring path(MAX_PATH, L'\0');

	for (;;) {
		const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
		if (length == 0) return {};

		if (length < path.size()) {
			path.resize(length);
			return path;
		}

		path.resize(path.size() * 2);
	}
}

auto launch_process(const std::wstring& t_executable, const wchar_t* t_arguments) -> void
{
	std::wstring command = L"\"" + t_executable + L"\"";
	if (t_arguments != nullptr && *t_arguments != L'\0') {
		command += L' ';
		command += t_arguments;
	}

	const usize        slash  = t_executable.find_last_of(L"\\/");
	const std::wstring folder = slash == std::wstring::npos ? std::wstring{} : t_executable.substr(0, slash);

	STARTUPINFOW        startup{.cb = sizeof(startup)};
	PROCESS_INFORMATION process{};

	if (CreateProcessW(t_executable.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr, folder.empty() ? nullptr : folder.c_str(), &startup,
	                   &process)) {
		CloseHandle(process.hProcess);
		CloseHandle(process.hThread);
	}
}

[[nodiscard]] auto bring_window_to_front(const wchar_t* t_class_name) -> bool
{
	const HWND window = FindWindowW(t_class_name, nullptr);
	if (window == nullptr) return false;

	if (IsIconic(window)) {
		ShowWindow(window, SW_RESTORE);
	}

	SetForegroundWindow(window);

	return true;
}

auto set_app_user_model_id() -> void
{
	SetCurrentProcessExplicitAppUserModelID(K_APP_USER_MODEL_ID);
}

[[nodiscard]] auto block_hook_injection() -> HookBlockResult
{
	using SetMitigationPolicy = BOOL(WINAPI*)(PROCESS_MITIGATION_POLICY, PVOID, SIZE_T);

	const HMODULE kernel32   = GetModuleHandleW(L"kernel32.dll");
	const auto    set_policy = kernel32 != nullptr ? reinterpret_cast<SetMitigationPolicy>(GetProcAddress(kernel32, "SetProcessMitigationPolicy")) : nullptr;
	if (set_policy == nullptr) return HookBlockResult::Unsupported;

	PROCESS_MITIGATION_EXTENSION_POINT_DISABLE_POLICY policy{};
	policy.DisableExtensionPoints = 1;

	return set_policy(ProcessExtensionPointDisablePolicy, &policy, sizeof(policy)) ? HookBlockResult::Blocked : HookBlockResult::Refused;
}

[[nodiscard]] auto injected_overlay_module() -> const wchar_t*
{
	for (const wchar_t* module : K_KNOWN_OVERLAY_MODULES) {
		if (GetModuleHandleW(module) != nullptr) return module;
	}

	return nullptr;
}

SingleInstanceGuard::SingleInstanceGuard()
	: m_mutex(CreateMutexW(nullptr, TRUE, K_SINGLE_INSTANCE_MUTEX_NAME))
	, m_first_instance(m_mutex != nullptr && GetLastError() != ERROR_ALREADY_EXISTS)
{
}

SingleInstanceGuard::~SingleInstanceGuard()
{
	release();
}

auto SingleInstanceGuard::release() -> void
{
	if (m_mutex == nullptr) return;

	if (m_first_instance) {
		ReleaseMutex(m_mutex);
	}

	CloseHandle(m_mutex);
	m_mutex = nullptr;
}
