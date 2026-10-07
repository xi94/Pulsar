#include <memory>
#include <print>
#include <string_view>

#include "app.h"
#include "core/debug_log.h"
#include "os/crash_handler.h"
#include "os/installation.h"
#include "os/self_update.h"
#include "os/window.h"
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

	DebugLogSession(const DebugLogSession&)                    = delete;
	auto operator=(const DebugLogSession&) -> DebugLogSession& = delete;
};

struct LaunchFlags {
	bool setup     = false;
	bool uninstall = false;
	bool startup   = false;
};

[[nodiscard]] auto launch_flags(int t_argument_count, char** t_arguments) -> LaunchFlags
{
	LaunchFlags flags;

	for (int i = 1; i < t_argument_count; i += 1) {
		const std::string_view argument = t_arguments[i];
		flags.setup                     = flags.setup || argument == "--setup";
		flags.uninstall                 = flags.uninstall || argument == "--uninstall";
		flags.startup                   = flags.startup || argument == "--startup";
	}

	return flags;
}
}

auto main(int t_argument_count, char** t_arguments) -> int
{
	if (os::handed_off_to_repaired_copy()) return 0;

	os::install_crash_handler();

	const DebugLogSession debug_log_session;
	if (debug_log::is_enabled() && debug_log::file_path()[0] != '\0') {
		std::println("Diagnostic log: {}", debug_log::file_path());
	}

	const LaunchFlags flags = launch_flags(t_argument_count, t_arguments);

	if (flags.setup || flags.uninstall) {
		if (os::bring_window_to_front(os::WindowKind::DIALOG)) return 0;

		auto setup = std::make_unique<SetupApp>(flags.uninstall ? SetupMode::UNINSTALL : SetupMode::MANAGE);
		setup->run();
		setup.reset();
		os::installation::finish_pending_removal();
		return 0;
	}

	if (!flags.startup && !os::installation::is_main_window_open() && os::installation::should_offer_setup()) {
		if (os::bring_window_to_front(os::WindowKind::DIALOG)) return 0;

		auto               setup   = std::make_unique<SetupApp>(SetupMode::FIRST_RUN);
		const SetupOutcome outcome = setup->run();
		setup.reset();

		if (outcome != SetupOutcome::PORTABLE) {
			os::installation::finish_pending_removal();
			return 0;
		}
	}

	const auto app = std::make_unique<App>();

	switch (app->start(flags.startup)) {
		using enum App::StartResult;

		case OK: {
			app->run();
			return 0;
		}

		case ALREADY_RUNNING: {
			return 0;
		}

		case FAILED: {
			return 1;
		}
	}

	return 1;
}
