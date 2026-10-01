#pragma once

#include <atomic>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

#include <Windows.h>

#include "core/types.h"

namespace installation {

struct Options {
	bool desktop_shortcut = true;
	bool start_menu_shortcut = true;
	bool start_with_windows = false;

	bool operator==(const Options &) const = default;
};

struct Installed {
	std::wstring location;
	std::wstring executable;
	Options options;
};

std::wstring default_location();
std::wstring with_app_folder(std::wstring_view t_folder);
std::optional<Installed> find_installation();
bool is_running_installed_copy();
bool should_offer_setup();
void mark_setup_complete();
void refresh_registration();
bool is_main_window_open();
bool close_running_app(std::wstring_view t_only_executable = {});
void open_folder(const std::wstring &t_folder);
void finish_pending_removal();

enum class Task : u8 {
	Install,
	Apply,
	Uninstall,
};

class Job {
  public:
	Job() = default;
	~Job();

	Job(const Job &) = delete;
	Job &operator=(const Job &) = delete;

	void start_install(std::wstring t_location, Options t_options);
	void start_apply(Installed t_installed, Options t_options);
	void start_uninstall(Installed t_installed, std::optional<std::wstring> t_data_folder);

	bool is_finished() const
	{
		return m_finished.load(std::memory_order_acquire);
	}

	bool succeeded() const
	{
		return is_finished() && m_succeeded;
	}

	std::string_view error() const
	{
		return is_finished() ? std::string_view{m_error} : std::string_view{};
	}

	u32 step() const
	{
		return m_step.load(std::memory_order_acquire);
	}

	std::string_view step_label() const;
	u32 step_count() const;

	Task task() const
	{
		return m_task;
	}

	const std::wstring &installed_executable() const
	{
		return m_installed_executable;
	}

	void reset();

  private:
	void begin(Task t_task);
	void finish(bool t_succeeded, std::string t_error);

	Task m_task = Task::Install;
	std::thread m_thread;
	std::atomic<u32> m_step{0};
	std::atomic<bool> m_finished{false};
	bool m_succeeded = false;
	std::string m_error;
	std::wstring m_installed_executable;
};

class FolderPicker {
  public:
	FolderPicker() = default;
	~FolderPicker();

	FolderPicker(const FolderPicker &) = delete;
	FolderPicker &operator=(const FolderPicker &) = delete;

	void open(HWND t_owner, std::wstring t_initial_folder);

	bool is_open() const
	{
		return m_thread.joinable() && !m_finished.load(std::memory_order_acquire);
	}

	std::optional<std::wstring> take_result();

  private:
	std::thread m_thread;
	std::atomic<bool> m_finished{false};
	std::atomic<DWORD> m_thread_id{0};
	std::optional<std::wstring> m_result;
};

}
