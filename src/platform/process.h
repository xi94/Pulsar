#pragma once

#include <string>

#include <Windows.h>

#include "core/types.h"

enum class HookBlockResult : u8 {
	Blocked,
	Refused,
	Unsupported,
};

[[nodiscard]] auto executable_path() -> std::wstring;
auto launch_process(const std::wstring& t_executable, const wchar_t* t_arguments = nullptr) -> void;
[[nodiscard]] auto bring_window_to_front(const wchar_t* t_class_name) -> bool;

auto set_app_user_model_id() -> void;
[[nodiscard]] auto block_hook_injection() -> HookBlockResult;
[[nodiscard]] auto injected_overlay_module() -> const wchar_t*;

class SingleInstanceGuard {
  public:
	SingleInstanceGuard();
	~SingleInstanceGuard();

	SingleInstanceGuard(const SingleInstanceGuard&)                    = delete;
	auto operator=(const SingleInstanceGuard&) -> SingleInstanceGuard& = delete;

	[[nodiscard]] auto is_first_instance() const -> bool
	{
		return m_first_instance;
	}

	auto release() -> void;

  private:
	HANDLE m_mutex          = nullptr;
	bool   m_first_instance = false;
};
