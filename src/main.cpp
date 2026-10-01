#include <memory>
#include <print>
#include <string_view>

#include <Windows.h>
#include <shellapi.h>

#include "app.h"
#include "core/app_identity.h"
#include "core/crash_handler.h"
#include "core/debug_log.h"
#include "core/updater.h"
#include "platform/installation.h"
#include "platform/process.h"
#include "setup_app.h"

namespace {
class DebugLogSession {
  public:
	DebugLogSession()
	{
		debug_log::init();
	}

	~DebugLogSession()
	{
		debug_log::shutdown();
	}

	DebugLogSession(const DebugLogSession &) = delete;
	DebugLogSession &operator=(const DebugLogSession &) = delete;
};

struct LaunchFlags {
	bool setup = false;
	bool uninstall = false;
	bool startup = false;
};

LaunchFlags launch_flags()
{
	LaunchFlags flags;
	int count = 0;
	wchar_t **arguments = CommandLineToArgvW(GetCommandLineW(), &count);
	if (arguments == nullptr) return flags;

	for (int i = 1; i < count; i += 1) {
		const std::wstring_view argument = arguments[i];
		flags.setup = flags.setup || argument == L"--setup";
		flags.uninstall = flags.uninstall || argument == L"--uninstall";
		flags.startup = flags.startup || argument == L"--startup";
	}

	LocalFree(arguments);

	return flags;
}
}

int main()
{
	if (Updater::handed_off_to_repaired_copy()) return 0;

	install_crash_handler();

	const DebugLogSession debug_log_session;
	if (debug_log::is_enabled() && debug_log::file_path()[0] != '\0') {
		std::println("Diagnostic log: {}", debug_log::file_path());
	}

	const LaunchFlags flags = launch_flags();

	if (flags.setup || flags.uninstall) {
		if (bring_window_to_front(setup_window_class_name)) return 0;

		auto setup = std::make_unique<SetupApp>(flags.uninstall ? SetupMode::Uninstall : SetupMode::Manage);
		setup->run();
		setup.reset();
		installation::finish_pending_removal();
		return 0;
	}

	if (!flags.startup && !installation::is_main_window_open() && installation::should_offer_setup()) {
		if (bring_window_to_front(setup_window_class_name)) return 0;

		auto setup = std::make_unique<SetupApp>(SetupMode::FirstRun);
		const SetupOutcome outcome = setup->run();
		setup.reset();

		if (outcome != SetupOutcome::Portable) {
			installation::finish_pending_removal();
			return 0;
		}
	}

	const auto app = std::make_unique<App>();

	switch (app->start(flags.startup)) {
		case App::StartResult::Ok:
			app->run();
			return 0;

		case App::StartResult::AlreadyRunning:
			return 0;

		case App::StartResult::Failed:
			return 1;
	}

	return 1;
}
