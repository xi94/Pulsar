#include "platform/overlay_guard.h"

#include <Windows.h>
#include <shobjidl.h>

#include "core/app_identity.h"

namespace {
constexpr const wchar_t *known_overlay_modules[]{
	L"DiscordHook64.dll",		L"DiscordHook32.dll", L"GameOverlayRenderer64.dll",
	L"GameOverlayRenderer.dll", L"RTSSHooks64.dll",
};
}

void overlay_guard::apply_process_identity()
{
	SetCurrentProcessExplicitAppUserModelID(app_user_model_id);
}

overlay_guard::BlockResult overlay_guard::block_hook_injection()
{
	using SetMitigationPolicy = BOOL(WINAPI *)(PROCESS_MITIGATION_POLICY, PVOID, SIZE_T);

	const HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
	const auto set_policy =
		kernel32 != nullptr
			? reinterpret_cast<SetMitigationPolicy>(GetProcAddress(kernel32, "SetProcessMitigationPolicy"))
			: nullptr;
	if (set_policy == nullptr) return BlockResult::unsupported;

	PROCESS_MITIGATION_EXTENSION_POINT_DISABLE_POLICY policy{};
	policy.DisableExtensionPoints = 1;

	return set_policy(ProcessExtensionPointDisablePolicy, &policy, sizeof(policy)) ? BlockResult::blocked
																				   : BlockResult::refused;
}

const wchar_t *overlay_guard::injected_overlay_module()
{
	for (const wchar_t *module : known_overlay_modules) {
		if (GetModuleHandleW(module) != nullptr) return module;
	}

	return nullptr;
}
