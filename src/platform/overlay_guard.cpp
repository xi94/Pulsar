#include "platform/overlay_guard.h"

#include <Windows.h>

#include <shobjidl.h>

#include "core/app_identity.h"

namespace {
// Every overlay whose injected module has been seen in the wild, so a log line can name which
// one got in rather than just reporting that something did.
constexpr const wchar_t *kKnownOverlayModules[]{
	L"DiscordHook64.dll",		L"DiscordHook32.dll", L"GameOverlayRenderer64.dll",
	L"GameOverlayRenderer.dll", L"RTSSHooks64.dll",
};

/// Not the executable name, and not a GUID either: Windows shows this to nothing, but processes
/// that classify other processes do read it.
constexpr const wchar_t *kAppUserModelId = L"Pulsar.DesktopApp.AccountManager";
} // namespace

OverlayGuard::EInjectionBlockResult OverlayGuard::BlockHookInjection()
{
	// Resolved at runtime rather than linked: the policy is old enough to rely on, but resolving
	// it keeps this from being a hard load-time dependency on a particular SDK's kernel32.
	using SetProcessMitigationPolicyFn = BOOL(WINAPI *)(PROCESS_MITIGATION_POLICY, PVOID, SIZE_T);

	const HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
	if (hKernel32 == nullptr) return EInjectionBlockResult::Unsupported;

	const auto pSetPolicy =
		reinterpret_cast<SetProcessMitigationPolicyFn>(GetProcAddress(hKernel32, "SetProcessMitigationPolicy"));
	if (pSetPolicy == nullptr) return EInjectionBlockResult::Unsupported;

	PROCESS_MITIGATION_EXTENSION_POINT_DISABLE_POLICY policy{};
	policy.DisableExtensionPoints = 1;

	if (pSetPolicy(ProcessExtensionPointDisablePolicy, &policy, sizeof(policy)) == FALSE) {
		return EInjectionBlockResult::Refused;
	}

	return EInjectionBlockResult::Blocked;
}

void OverlayGuard::ApplyProcessIdentity()
{
	SetCurrentProcessExplicitAppUserModelID(kAppUserModelId);
}

const wchar_t *OverlayGuard::DetectInjectedOverlay()
{
	for (const wchar_t *pModuleName : kKnownOverlayModules) {
		if (GetModuleHandleW(pModuleName) != nullptr) return pModuleName;
	}

	return nullptr;
}
