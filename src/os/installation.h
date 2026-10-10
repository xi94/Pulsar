#pragma once

#include <atomic>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

#include "core/types.h"

namespace os::installation {

struct Options {
	bool desktop_shortcut    = true;
	bool start_menu_shortcut = true;
	bool start_with_windows  = false;

	auto operator==(const Options&) const -> bool = default;
};

struct Installed {
	std::string location;
	std::string executable;
	Options     options;
};

[[nodiscard]] auto is_supported() -> bool;
[[nodiscard]] auto default_location() -> std::string;
[[nodiscard]] auto with_app_folder(std::string_view t_folder) -> std::string;
[[nodiscard]] auto find_installation() -> std::optional<Installed>;
[[nodiscard]] auto is_running_installed_copy() -> bool;
[[nodiscard]] auto should_offer_setup() -> bool;
auto mark_setup_complete() -> void;
auto refresh_registration() -> void;
[[nodiscard]] auto starts_with_system() -> bool;
auto set_starts_with_system(bool t_enabled) -> void;
[[nodiscard]] auto is_main_window_open() -> bool;
[[nodiscard]] auto close_running_app(std::string_view t_only_executable = {}) -> bool;
auto finish_pending_removal() -> void;

enum class Task : u8 {
	INSTALL,
	APPLY,
	UNINSTALL,
};

class Job {
  public:
	Job() = default;
	~Job();

	Job(const Job&)                    = delete;
	auto operator=(const Job&) -> Job& = delete;

	auto start_install(const std::string& t_location, Options t_options) -> void;
	auto start_apply(const Installed& t_installed, Options t_options) -> void;
	auto start_uninstall(const Installed& t_installed, std::optional<std::string> t_data_folder) -> void;

	[[nodiscard]] auto is_finished() const -> bool
	{
		return m_finished.load(std::memory_order_acquire);
	}

	[[nodiscard]] auto succeeded() const -> bool
	{
		return is_finished() && m_succeeded;
	}

	[[nodiscard]] auto error() const -> std::string_view
	{
		return is_finished() ? std::string_view{m_error} : std::string_view{};
	}

	[[nodiscard]] auto step() const -> u32
	{
		return m_step.load(std::memory_order_acquire);
	}

	[[nodiscard]] auto step_label() const -> std::string_view;
	[[nodiscard]] auto step_count() const -> u32;

	[[nodiscard]] auto task() const -> Task
	{
		return m_task;
	}

	[[nodiscard]] auto installed_executable() const -> const std::string&
	{
		return m_installed_executable;
	}

	auto reset() -> void;

  private:
	auto begin(Task t_task) -> void;
	auto finish(bool t_succeeded, std::string t_error) -> void;

	Task              m_task = Task::INSTALL;
	std::thread       m_thread;
	std::atomic<u32>  m_step{0};
	std::atomic<bool> m_finished{false};
	bool              m_succeeded = false;
	std::string       m_error;
	std::string       m_installed_executable;
};

}
