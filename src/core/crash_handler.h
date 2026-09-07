#pragma once

#include <string>

/// Installs the process-wide last-resort handlers: an SEH exception filter, a
/// std::terminate handler, and the pure-call and invalid-parameter CRT hooks. Together they
/// catch any otherwise-fatal condition on any thread - access violations, stack overflows,
/// uncaught exceptions, pure virtual calls, a std::thread destroyed while joinable.
///
/// On a catch it writes a full-memory minidump to %LOCALAPPDATA%\Pulsar\crashes and shows a
/// blocking TaskDialog naming the crashing module and offset. The dialog has no cancel path:
/// Escape, Alt-F4 and the title bar close button are all ignored, because the entire point
/// is that this cannot be missed.
///
/// Call once, as early in main() as possible - everything installed here is process-wide, so
/// this single call covers every thread the process ever creates.
void InstallCrashHandler();

/// Writes a minidump right now without crashing and without showing anything - the
/// counterpart for a hang, called by debug_log's watchdog once a call has been stuck past its
/// threshold. Captures every thread's stack but not full memory: the process is still alive
/// and expected to keep running, so this stays small and quick.
///
/// pTag becomes part of the file name. Returns the path written, or empty on failure. Safe to
/// call from any thread.
std::wstring WriteDiagnosticDump(const wchar_t *pTag);
