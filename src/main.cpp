#include <memory>
#include <print>

#include "app.h"
#include "core/crash_handler.h"
#include "core/debug_log.h"
#include "core/updater.h"

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
}

int main()
{
	if (Updater::handed_off_to_repaired_copy()) return 0;

	install_crash_handler();

	const DebugLogSession debug_log_session;
	if (debug_log::is_enabled() && debug_log::file_path()[0] != '\0') {
		std::println("Diagnostic log: {}", debug_log::file_path());
	}

	const auto app = std::make_unique<App>();

	switch (app->start()) {
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
