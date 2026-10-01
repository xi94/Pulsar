#include <memory>
#include <print>
#include <string_view>

#include <Windows.h>
#include <shellapi.h>

#include "app.h"
#include "core/crash_handler.h"
#include "core/debug_log.h"
#include "core/app_identity.h"
#include "core/updater.h"
#include "platform/installation.h"
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

bool focus_open_setup()
{
	const HWND existing = FindWindowW(setup_window_class_name, nullptr);
	if (existing == nullptr) return false;

	if (IsIconic(existing)) {
		ShowWindow(existing, SW_RESTORE);
	}

	SetForegroundWindow(existing);

	return true;
}

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
		if (focus_open_setup()) return 0;

		auto setup = std::make_unique<SetupApp>(flags.uninstall ? SetupMode::uninstall : SetupMode::manage);
		setup->run();
		setup.reset();
		installation::finish_pending_removal();
		return 0;
	}

	if (!flags.startup && !installation::is_main_window_open() && installation::should_offer_setup()) {
		if (focus_open_setup()) return 0;

		auto setup = std::make_unique<SetupApp>(SetupMode::first_run);
		const SetupOutcome outcome = setup->run();
		setup.reset();

		if (outcome != SetupOutcome::portable) {
			installation::finish_pending_removal();
			return 0;
		}
	}

	const auto app = std::make_unique<App>();

	switch (app->start(flags.startup)) {
		case App::StartResult::ok:
			app->run();
			return 0;

		case App::StartResult::already_running:
			return 0;

		case App::StartResult::failed:
			return 1;
	}

	return 1;
}
