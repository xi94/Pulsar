#pragma once

// Keeps third-party game overlays - Discord's above all - out of this process, and keeps this
// process from looking like a game to the things that go looking for one.
//
// Two independent halves, because there are two independent problems:
//
//   Identity. Discord classifies a process as a game from its executable name, its window class
//   and its version-resource strings. Everything here is named so none of those match a real
//   game, which is what stops Discord announcing a password manager as "Playing Pulsar" in the
//   first place. See core/app_identity.h, which owns those names.
//
//   Injection. An overlay that has decided to attach loads a DLL into this process. The one
//   injection route a process can genuinely refuse from the inside is the legacy extension-point
//   family - AppInit_DLLs, SetWindowsHookEx and IMEs - which is the route Discord's overlay
//   uses, and which is also how most keyloggers reach a password field.
//
// What this cannot do is stop a CreateRemoteThread-style injection from a process running at the
// same integrity level; nothing running in-process can. Discord's own per-app overlay switch
// remains the authoritative control, and this is defence in depth behind it.
namespace OverlayGuard {

enum class EInjectionBlockResult : u8 {
	/// Extension-point DLLs are now refused for the life of the process.
	Blocked,

	/// Windows declined the policy, usually because something already set a conflicting one.
	Refused,

	/// The policy is not available on this build of Windows.
	Unsupported,
};

/// Refuses every extension-point DLL from this point on. Call once, early, before the first
/// window exists - the policy only governs loads that have not happened yet.
///
/// The cost is real and worth stating: it also disables third-party IMEs, so anything typed
/// through one stops working. Callers gate this behind a user-visible setting for that reason.
EInjectionBlockResult BlockHookInjection();

/// Gives the process an explicit taskbar identity that is not derived from the executable name.
/// Cheap, always safe, and one less thing for a detector to pattern-match on.
void ApplyProcessIdentity();

/// The name of a known overlay module currently loaded in this process, or nullptr. Detection
/// only - by the time a module answers here it is already in, so this exists to make that
/// visible in the diagnostic log rather than to do anything about it.
const wchar_t *DetectInjectedOverlay();

} // namespace OverlayGuard
