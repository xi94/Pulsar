#include "os/installation.h"

#include <utility>

namespace {
constexpr const char* K_UNSUPPORTED_MESSAGE = "Installing isn't available on macOS yet.";
constexpr const char* K_UNSUPPORTED_STEP    = "Not available on macOS";
}

namespace os::installation {

auto is_supported() -> bool
{
	return false;
}

auto default_location() -> std::string
{
	return "/Applications";
}

auto with_app_folder(std::string_view t_folder) -> std::string
{
	return std::string{t_folder};
}

auto find_installation() -> std::optional<Installed>
{
	return std::nullopt;
}

auto is_running_installed_copy() -> bool
{
	return false;
}

auto should_offer_setup() -> bool
{
	return false;
}

auto mark_setup_complete() -> void {}

auto refresh_registration() -> void {}

auto is_main_window_open() -> bool
{
	return false;
}

auto close_running_app(std::string_view) -> bool
{
	return true;
}

auto finish_pending_removal() -> void {}

Job::~Job()
{
	if (m_thread.joinable()) {
		m_thread.join();
	}
}

auto Job::reset() -> void
{
	if (m_thread.joinable()) {
		m_thread.join();
	}

	m_step.store(0, std::memory_order_release);
	m_finished.store(false, std::memory_order_release);
	m_succeeded = false;
	m_error.clear();
}

auto Job::begin(Task t_task) -> void
{
	reset();
	m_task = t_task;
	m_installed_executable.clear();
}

auto Job::finish(bool t_succeeded, std::string t_error) -> void
{
	m_succeeded = t_succeeded;
	m_error     = std::move(t_error);
	m_finished.store(true, std::memory_order_release);
}

auto Job::step_count() const -> u32
{
	return 1;
}

auto Job::step_label() const -> std::string_view
{
	return K_UNSUPPORTED_STEP;
}

auto Job::start_install(const std::string&, Options) -> void
{
	begin(Task::INSTALL);
	finish(false, K_UNSUPPORTED_MESSAGE);
}

auto Job::start_apply(const Installed&, Options) -> void
{
	begin(Task::APPLY);
	finish(false, K_UNSUPPORTED_MESSAGE);
}

auto Job::start_uninstall(const Installed&, std::optional<std::string>) -> void
{
	begin(Task::UNINSTALL);
	finish(false, K_UNSUPPORTED_MESSAGE);
}

}
