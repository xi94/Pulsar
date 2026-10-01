#pragma once

#include <string>

#include <Windows.h>

#include "core/types.h"

enum class HookBlockResult : u8 {
	Blocked,
	Refused,
	Unsupported,
};

std::wstring executable_path();
void launch_process(const std::wstring &t_executable, const wchar_t *t_arguments = nullptr);
bool bring_window_to_front(const wchar_t *t_class_name);

void set_app_user_model_id();
HookBlockResult block_hook_injection();
const wchar_t *injected_overlay_module();

class SingleInstanceGuard {
  public:
	SingleInstanceGuard();
	~SingleInstanceGuard();

	SingleInstanceGuard(const SingleInstanceGuard &) = delete;
	SingleInstanceGuard &operator=(const SingleInstanceGuard &) = delete;

	bool is_first_instance() const
	{
		return m_first_instance;
	}

	void release();

  private:
	HANDLE m_mutex = nullptr;
	bool m_first_instance = false;
};
