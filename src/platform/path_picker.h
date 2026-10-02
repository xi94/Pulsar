#pragma once

#include <atomic>
#include <optional>
#include <string>
#include <thread>

#include <Windows.h>

#include "core/types.h"

class ComScope {
  public:
	ComScope();
	~ComScope();

	ComScope(const ComScope&)                    = delete;
	auto operator=(const ComScope&) -> ComScope& = delete;

  private:
	bool m_initialized;
};

enum class PathKind : u8 {
	Folder,
	File,
};

struct PathRequest {
	PathKind       kind     = PathKind::Folder;
	const wchar_t* title    = L"";
	const wchar_t* ok_label = nullptr;
	std::wstring   start_path;
	const wchar_t* file_type_name    = nullptr;
	const wchar_t* file_type_pattern = nullptr;
};

class PathPicker {
  public:
	PathPicker() = default;
	~PathPicker();

	PathPicker(const PathPicker&)                    = delete;
	auto operator=(const PathPicker&) -> PathPicker& = delete;

	auto open(HWND t_owner, PathRequest t_request) -> void;

	[[nodiscard]] auto is_open() const -> bool
	{
		return m_thread.joinable() && !m_finished.load(std::memory_order_acquire);
	}

	[[nodiscard]] auto take_result() -> std::optional<std::wstring>;

  private:
	std::thread                 m_thread;
	std::atomic<bool>           m_finished{false};
	std::atomic<DWORD>          m_thread_id{0};
	std::optional<std::wstring> m_result;
};
