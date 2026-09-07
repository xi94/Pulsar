#include <memory>
#include <print>

#include "app.h"
#include "core/crash_handler.h"
#include "core/debug_log.h"
#include "core/updater.h"

namespace {
/// Starts the diagnostic log on construction and stops it on destruction. Declared as the first
/// local in main(), so reverse-order teardown destroys it last and every other destructor still
/// has somewhere to log - which is how a worker wedged in a UI Automation call at shutdown gets
/// recorded at all.
class CDebugLogSession {
  public:
	CDebugLogSession()
	{
		DebugLog::Init();
	}

	~CDebugLogSession()
	{
		DebugLog::Shutdown();
	}

	CDebugLogSession(const CDebugLogSession &) = delete;
	CDebugLogSession &operator=(const CDebugLogSession &) = delete;
};
} // namespace

int main()
{
	// Before literally anything else, including the crash handler - see
	// CUpdater::RunStartupRecoveryAndMaybeExit's comment. A true return means this process
	// has nothing further to do: it either just relaunched a repaired copy of itself, or there
	// was nothing for it to do at all.
	if (CUpdater::RunStartupRecoveryAndMaybeExit()) return 0;

	// Everything it installs is process-wide, so this one call covers every thread this process
	// ever creates, not just this one.
	InstallCrashHandler();

	// Right after the crash handler and before anything that could hang: this is what turns "it
	// froze" into a log naming the exact call that never returned, plus a minidump of every
	// thread taken while it is still stuck.
	const CDebugLogSession debugLogSession;
	if (DebugLog::IsEnabled() && DebugLog::GetFilePath()[0] != '\0') {
		std::println("Diagnostic log: {}", DebugLog::GetFilePath());
	}

	// Heap-allocated rather than a local: CApp holds every subsystem inline and comes to a little
	// over 100 KB, which is a tenth of the default stack for one object. Its address has to stay
	// put either way - the window procedure keeps a pointer to it.
	const auto pApp = std::make_unique<CApp>();

	switch (pApp->Init()) {
		case CApp::EStartResult::Ok:
			break;

		case CApp::EStartResult::AlreadyRunning:
			return 0;

		case CApp::EStartResult::Failed:
			return 1;
	}

	pApp->Run();

	return 0;
}
