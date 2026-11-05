#pragma once

#include <atomic>
#include <memory>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#include <Windows.h>
#include <ShObjIdl.h>
#include <wrl/client.h>

#include "login/riot_client.h"
#include "login/win32/live_view.h"

// Keeps every Riot Client window invisible, click-through and out of the taskbar while Pulsar signs in, and gives the foreground back whenever the client takes it.
// Windows are styled from an out-of-process WinEvent hook the moment they exist, so the client never shows a frame. Nothing is injected into the client.
class HiddenClient {
  public:
	HiddenClient() = default;
	~HiddenClient();

	HiddenClient(const HiddenClient&)                    = delete;
	auto operator=(const HiddenClient&) -> HiddenClient& = delete;

	[[nodiscard]] auto start(bool t_show_live_view) -> bool;
	[[nodiscard]] auto is_hiding() const -> bool;
	auto watch_for_user_opening() -> void;
	[[nodiscard]] auto was_opened_by_user() const -> bool;
	auto note_own_action() -> void;
	auto show_progress(std::string_view t_progress) -> void;
	auto hand_back(HandBack t_mode) -> void;

	static auto show_windows_left_hidden() -> void;
	static auto show_hidden_windows_after_crash() -> void;

  private:
	struct HiddenWindow {
		HWND     window;
		LONG_PTR original_ex_style;
	};

	static auto CALLBACK on_window_event(HWINEVENTHOOK t_hook, DWORD t_event, HWND t_window, LONG t_object, LONG t_child, DWORD t_thread, DWORD t_time) -> void;
	static auto CALLBACK control_window_proc(HWND t_window, UINT t_message, WPARAM t_wparam, LPARAM t_lparam) -> LRESULT;

	auto run_hooks(HANDLE t_ready) -> void;
	auto send_to_hook_thread(UINT t_message, WPARAM t_wparam) -> void;
	auto hide(HWND t_window) -> void;
	auto hide_open_windows() -> void;
	auto on_foreground(HWND t_window) -> void;
	auto give_foreground_back() -> void;
	auto begin_watching_for_user() -> void;
	auto check_for_new_client_launch() -> void;
	auto take_as_opened_by_user(const char* t_how) -> void;
	auto restore_windows(HandBack t_mode) -> void;
	[[nodiscard]] auto is_client_interface_process_cached(DWORD t_process_id) -> bool;
	[[nodiscard]] auto is_own_action_recent() const -> bool;

	std::thread               m_thread;
	HWND                      m_control_window = nullptr;
	std::atomic<bool>         m_hiding{false};
	std::atomic<bool>         m_opened_by_user{false};
	std::atomic<u64>          m_own_action_until{0};
	std::unique_ptr<LiveView> m_live_view;

	std::vector<HiddenWindow>            m_hidden_windows;
	Microsoft::WRL::ComPtr<ITaskbarList> m_taskbar;
	std::unordered_map<DWORD, bool>      m_interface_processes;
	HWND                                 m_last_other_foreground         = nullptr;
	DWORD                                m_last_other_foreground_process = 0;
	bool                                 m_watching_for_user             = false;
	u64                                  m_watch_for_user_from           = 0;
	std::vector<DWORD>                   m_known_client_launchers;
	bool                                 m_handed_back = false;
};
