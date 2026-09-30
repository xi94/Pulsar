#pragma once

#include <chrono>
#include <optional>
#include <string_view>

#include "core/library.h"
#include "core/master_key.h"
#include "core/settings.h"
#include "core/storage.h"
#include "core/updater.h"
#include "gfx/assets.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "gfx/renderer.h"
#include "platform/single_instance.h"
#include "platform/tray.h"
#include "platform/window.h"
#include "ui/account_modal.h"
#include "ui/app_menu.h"
#include "ui/carousel.h"
#include "ui/commands.h"
#include "ui/context_menu.h"
#include "ui/profiler_overlay.h"
#include "ui/settings_panel.h"
#include "ui/title_bar.h"
#include "ui/toasts.h"
#include "ui/truncation_hint.h"
#include "ui/unlock_screen.h"
#include "ui/update_overlay.h"
#include "ui/widget.h"

class App {
  public:
	enum class StartResult : u8 {
		ok,
		already_running,
		failed,
	};

	App();

	App(const App &) = delete;
	App &operator=(const App &) = delete;

	StartResult start();
	void run();

  private:
	using Clock = std::chrono::steady_clock;

	struct VaultKey {
		MasterKey key;
		MasterKeyParams params;
	};

	struct ClipboardSecret {
		u32 sequence;
		Clock::time_point clear_at;
	};

	bool create_graphics();
	void add_games();
	void stack_widgets();
	void apply_settings(storage::LoadResult t_load_result);
	void lock();
	void unlock();
	void lock_vault();
	void lock_if_idle();

	void apply_game_order();
	void save_settings();
	void save_everything();
	void request_save();
	void save_if_due();
	void commit_new_vault_key();
	void copy_password(std::string_view t_password);
	void clear_clipboard_secret();

	void pump_input();
	void handle_tray_event();
	void handle_input(const InputEvent &t_event);
	void process_commands();
	void process(const Command &t_command);
	void open_account_menu(const Command &t_command);
	void open_text_menu(const Command &t_command);
	void fill_tray_menu(TrayMenu &t_menu) const;

	void announce_update_stage();
	void announce_first_run_after_update();
	void announce_unreadable_storage();
	void relaunch_if_update_installed();

	void redraw_while_resizing();
	bool reload_fonts();
	void frame();
	void render();
	void draw_status_bar();

	SingleInstanceGuard m_instance_guard;
	Settings m_settings;
	Library m_library;
	MasterKey m_master_key;
	Updater m_updater;
	CommandQueue m_commands;

	Window m_window;
	Renderer m_renderer;
	Assets m_assets;
	Fonts m_fonts;
	Tray m_tray;
	DrawList m_draw_list;

	Carousel m_carousel;
	Toasts m_toasts;
	AccountModal m_account_modal;
	SettingsPanel m_settings_panel;
	UnlockScreen m_unlock_screen;
	AppMenu m_app_menu;
	UpdateOverlay m_update_overlay;
	ContextMenu m_context_menu;
	TitleBar m_title_bar;
	TruncationHint m_truncation_hint;
#ifdef PULSAR_PROFILING
	ProfilerOverlay m_profiler_overlay;
#endif
	WidgetStack m_widgets;

	bool m_locked = true;
	bool m_just_updated = false;
	bool m_unreadable_storage_announced = false;
	bool m_in_frame = false;
	UpdateStage m_announced_update_stage = UpdateStage::idle;
	Vec2 m_mouse{-1.0f, -1.0f};
	bool m_pointer_down = false;
	u32 m_swap_chain_width = 0;
	u32 m_swap_chain_height = 0;

	std::chrono::steady_clock::time_point m_start_time;
	std::chrono::steady_clock::time_point m_last_frame_time;
	float m_effect_seconds = 0.0f;
	Clock::time_point m_last_activity;

	std::optional<Clock::time_point> m_save_due;
	std::optional<VaultKey> m_replaced_vault_key;
	std::optional<ClipboardSecret> m_clipboard_secret;
};
