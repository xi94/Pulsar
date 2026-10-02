#pragma once

#include <chrono>
#include <optional>
#include <string_view>

#include "core/crypto.h"
#include "core/library.h"
#include "core/settings.h"
#include "core/storage.h"
#include "core/updater.h"
#include "gfx/assets.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "gfx/renderer.h"
#include "platform/path_picker.h"
#include "platform/process.h"
#include "platform/tray.h"
#include "platform/window.h"
#include "ui/account_modal.h"
#include "ui/account_search.h"
#include "ui/app_menu.h"
#include "ui/carousel.h"
#include "ui/commands.h"
#include "ui/context_menu.h"
#include "ui/profiler_overlay.h"
#include "ui/settings_panel.h"
#include "ui/snowfall.h"
#include "ui/title_bar.h"
#include "ui/toasts.h"
#include "ui/truncation_hint.h"
#include "ui/unlock_screen.h"
#include "ui/update_overlay.h"
#include "ui/widget.h"

class App {
  public:
	enum class StartResult : u8 {
		Ok,
		AlreadyRunning,
		Failed,
	};

	App();

	App(const App&)                    = delete;
	auto operator=(const App&) -> App& = delete;

	[[nodiscard]] auto start(bool t_from_startup) -> StartResult;
	auto run() -> void;

  private:
	using Clock = std::chrono::steady_clock;

	struct VaultKey {
		MasterKey       key;
		MasterKeyParams params;
	};

	struct ClipboardSecret {
		u32               sequence;
		Clock::time_point clear_at;
	};

	[[nodiscard]] auto create_graphics() -> bool;
	auto add_games() -> void;
	auto stack_widgets() -> void;
	auto apply_settings(storage::LoadResult t_load_result) -> void;
	auto lock() -> void;
	auto unlock() -> void;
	auto lock_vault() -> void;
	auto lock_if_idle() -> void;

	auto apply_game_order() -> void;
	auto save_settings() -> void;
	auto save_everything() -> void;
	auto request_save() -> void;
	auto save_if_due() -> void;
	auto commit_new_vault_key() -> void;
	auto copy_password(std::string_view t_password) -> void;
	auto open_account_search() -> void;
	auto open_setup() -> void;
	auto locate_riot_client(const Command& t_command) -> void;
	auto take_picked_riot_client() -> void;
	[[nodiscard]] auto account_for(AccountRef t_account) const -> const Account*;
	auto clear_clipboard_secret() -> void;

	auto pump_input() -> void;
	auto handle_tray_event() -> void;
	auto handle_input(const InputEvent& t_event) -> void;
	auto process_commands() -> void;
	auto process(const Command& t_command) -> void;
	auto open_account_menu(const Command& t_command) -> void;
	auto open_text_menu(const Command& t_command) -> void;
	auto fill_tray_menu(TrayMenu* t_menu) const -> void;

	auto announce_update_stage() -> void;
	auto announce_first_run_after_update() -> void;
	auto announce_unreadable_storage() -> void;
	auto relaunch_if_update_installed() -> void;

	auto redraw_while_resizing() -> void;
	auto reload_fonts() -> bool;
	auto frame() -> void;
	auto render() -> void;
	auto draw_status_bar() -> void;

	SingleInstanceGuard m_instance_guard;
	Settings            m_settings;
	Library             m_library;
	MasterKey           m_master_key;
	Updater             m_updater;
	CommandQueue        m_commands;

	Window   m_window;
	Renderer m_renderer;
	Assets   m_assets;
	Fonts    m_fonts;
	Tray     m_tray;
	DrawList m_draw_list;

	Carousel       m_carousel;
	Snowfall       m_snowfall;
	Toasts         m_toasts;
	AccountModal   m_account_modal;
	SettingsPanel  m_settings_panel;
	UnlockScreen   m_unlock_screen;
	AppMenu        m_app_menu;
	UpdateOverlay  m_update_overlay;
	AccountSearch  m_account_search;
	ContextMenu    m_context_menu;
	TitleBar       m_title_bar;
	TruncationHint m_truncation_hint;
#ifdef PULSAR_PROFILING
	ProfilerOverlay m_profiler_overlay;
#endif
	WidgetStack m_widgets;

	bool        m_locked                       = true;
	bool        m_just_updated                 = false;
	bool        m_unreadable_storage_announced = false;
	bool        m_in_frame                     = false;
	UpdateStage m_announced_update_stage       = UpdateStage::Idle;
	Vec2        m_mouse{-1.0f, -1.0f};
	bool        m_pointer_down = false;

	std::chrono::steady_clock::time_point m_start_time;
	std::chrono::steady_clock::time_point m_last_frame_time;
	float                                 m_effect_seconds = 0.0f;
	Clock::time_point                     m_last_activity;

	std::optional<Clock::time_point> m_save_due;
	std::optional<VaultKey>          m_replaced_vault_key;
	std::optional<ClipboardSecret>   m_clipboard_secret;

	PathPicker             m_client_picker;
	std::optional<Command> m_locate_request;
};
