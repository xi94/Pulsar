#include "app.h"

#include <cstdio>
#include <print>
#include <utility>

#include <sodium.h>

#include "core/animation.h"
#include "core/app_identity.h"
#include "core/debug_log.h"
#include "core/profiler.h"
#include "core/str.h"
#include "games.h"
#include "login/riot_client.h"
#include "os/app_icon.h"
#include "os/clipboard.h"
#include "os/installation.h"
#include "ui/controls.h"
#include "ui/text.h"
#include "ui/theme.h"
#include "ui/window_layout.h"

namespace {
constexpr u32   K_DRAW_LIST_VERTEX_CAPACITY = 1 << 16;
constexpr u32   K_DRAW_LIST_INDEX_CAPACITY  = (1 << 16) * 3 / 2;
constexpr auto  K_SAVE_DELAY                = std::chrono::milliseconds(500);
constexpr float K_IDLE_POLL_SECONDS         = 0.25f;
constexpr auto  K_RESUME_FRAME_TIME         = std::chrono::microseconds(16667);
constexpr auto  K_CLIPBOARD_SECRET_LIFETIME = std::chrono::seconds(30);
constexpr float K_PICKER_POLL_SECONDS       = 0.1f;

constexpr float K_STATUS_PADDING        = 14.0f;
constexpr float K_STATUS_MARK_SIZE      = 15.0f;
constexpr float K_STATUS_MARK_GAP       = 7.0f;
constexpr float K_STATUS_DOT_SIZE       = 3.0f;
constexpr float K_STATUS_DOT_GAP        = 7.0f;
constexpr float K_STATUS_BASELINE_NUDGE = 2.0f;

static_assert(os::K_TRAY_MAX_GAMES >= K_MAX_GAMES);

auto log_startup_phase(const char* t_phase) -> void
{
	using Clock = std::chrono::steady_clock;

	static const Clock::time_point PROCESS_START  = Clock::now();
	static Clock::time_point       phase_start    = PROCESS_START;
	static const char*             previous_phase = nullptr;

	const Clock::time_point now = Clock::now();

	if (previous_phase != nullptr) {
		debug_log::write("startup", "%-20s %6.1f ms   (%6.1f ms in)", previous_phase, std::chrono::duration<float, std::milli>(now - phase_start).count(),
		                 std::chrono::duration<float, std::milli>(now - PROCESS_START).count());
	}

	previous_phase = t_phase;
	phase_start    = now;
}

[[nodiscard]] auto update_status(UpdateStage t_stage) -> std::string_view
{
	switch (t_stage) {
		case UpdateStage::UpToDate: {
			return "Up to date";
		}

		case UpdateStage::Available:
		case UpdateStage::ManualUpgradeRequired: {
			return "Update available";
		}

		case UpdateStage::Checking: {
			return "Checking...";
		}

		case UpdateStage::Downloading:
		case UpdateStage::Verifying:
		case UpdateStage::Installing: {
			return "Updating...";
		}

		case UpdateStage::ReadyToRelaunch: {
			return "Restart to update";
		}

		case UpdateStage::Idle:
		case UpdateStage::CheckFailed:
		case UpdateStage::Error:
		case UpdateStage::Cancelled: {
			break;
		}
	}

	return "";
}

auto guard_against_overlays(bool t_block_injection) -> void
{
	os::register_app_identity();

	if (!t_block_injection) {
		debug_log::write("app", "overlay injection guard disabled by setting");
		return;
	}

	switch (os::block_injection()) {
		case os::InjectionGuard::Blocked: {
			debug_log::write("app", "extension-point DLL injection blocked");
			break;
		}

		case os::InjectionGuard::Refused: {
			debug_log::write("app", "extension-point block refused, err=%u", os::last_error());
			break;
		}

		case os::InjectionGuard::Unsupported: {
			debug_log::write("app", "extension-point block unsupported on this system");
			break;
		}
	}

	if (const std::optional<std::string> module = os::injected_overlay()) {
		debug_log::write("app", "an overlay module was already loaded before the guard ran: %s", module->c_str());
	}
}
}

App::App()
	: m_carousel(&m_library, &m_settings, &m_fonts, &m_assets, &m_commands)
	, m_toasts(&m_settings, &m_fonts, &m_assets, &m_window, &m_commands)
	, m_account_modal(&m_library, &m_settings, &m_fonts, &m_assets, &m_window, &m_toasts, &m_commands)
	, m_settings_panel(&m_settings, &m_fonts, &m_renderer, &m_window, &m_assets, &m_commands)
	, m_unlock_screen(&m_settings, &m_master_key, &m_fonts, &m_assets, &m_window, &m_commands)
	, m_app_menu(&m_fonts, &m_assets, &m_commands)
	, m_update_overlay(&m_updater, &m_settings, &m_fonts, &m_assets, &m_window)
	, m_account_search(&m_library, &m_fonts, &m_assets, &m_window, &m_commands)
	, m_context_menu(&m_fonts, &m_commands)
	, m_title_bar(&m_window, &m_updater, &m_update_overlay, &m_fonts, &m_assets, &m_commands)
	, m_truncation_hint(&m_fonts)
#ifdef PULSAR_PROFILING
	, m_profiler_overlay(&m_fonts)
#endif
{
}

auto App::start(bool t_from_startup) -> App::StartResult
{
	if (!m_instance_guard.is_first_instance()) {
		const bool activated = os::activate_running_instance();
		debug_log::write("app", "another instance is already running (%s) - exiting", activated ? "brought it to the front" : "it never answered");

		return StartResult::AlreadyRunning;
	}

	m_assets.begin_decode();
	os::installation::refresh_registration();

	log_startup_phase("LoadSettings");
	const storage::LoadResult settings_result = storage::load_settings(&m_settings);

	log_startup_phase("GuardAgainstOverlays");
	guard_against_overlays(m_settings.block_overlay_injection);
	RiotClient::prepare_automation();

	if (sodium_init() < 0) {
		std::println("Failed to initialize libsodium.");
		return StartResult::Failed;
	}

	log_startup_phase("CreateGraphics");
	if (!create_graphics()) return StartResult::Failed;

	log_startup_phase("CreateTray");
	m_tray.create(K_APP_NAME);
	m_draw_list.init(K_DRAW_LIST_VERTEX_CAPACITY, K_DRAW_LIST_INDEX_CAPACITY);

	log_startup_phase("AddGames");
	add_games();
	m_updater.check_for_update();

	log_startup_phase("ApplySettings");
	apply_settings(settings_result);
	stack_widgets();
	lock();

	if (m_settings.master_password_enabled) {
		m_unlock_screen.show_unlock();
	} else {
		m_unlock_screen.show_setup();
	}

	m_window.on_redraw([this] { redraw_while_resizing(); });
	m_window.on_dpi_changed([this] { reload_fonts(); });

	m_start_time      = std::chrono::steady_clock::now();
	m_last_frame_time = m_start_time;
	m_last_activity   = m_start_time;

	log_startup_phase("FirstFrame");
	frame();

	if (!t_from_startup) {
		m_window.show();
	} else if (!m_settings.close_to_tray || !m_tray.is_icon_visible()) {
		m_window.show_minimized();
	}

	log_startup_phase("Done");

	return StartResult::Ok;
}

auto App::create_graphics() -> bool
{
	const u32 width  = std::max(m_settings.window_width, static_cast<u32>(K_MIN_WINDOW_WIDTH));
	const u32 height = std::max(m_settings.window_height, static_cast<u32>(K_MIN_WINDOW_HEIGHT));

	if (!m_window.create(K_APP_NAME, width, height)) {
		std::println("Failed to create window.");
		return false;
	}

	m_window.set_min_size(Vec2{K_MIN_WINDOW_WIDTH, K_MIN_WINDOW_HEIGHT});
	m_window.set_title_bar(K_TITLE_BAR_HEIGHT, [this](Vec2 t_point) { return m_title_bar.layout().button_at(t_point) != TitleBarButton::None; });

	if (!m_renderer.init(&m_window, m_settings.renderer)) {
		std::println("Failed to initialize renderer.");
		return false;
	}

	if (!m_assets.finish_upload(&m_renderer)) {
		std::println("Failed to load one or more embedded assets.");
		return false;
	}

	m_snowfall.create_texture(&m_renderer);

	if (!reload_fonts()) {
		std::println("Failed to load the UI font.");
		return false;
	}

	return true;
}

auto App::add_games() -> void
{
	for (const GameInfo& info : K_GAME_INFOS) {
		if (!RiotClient::supports_product(info.launch_product)) continue;

		m_tray.set_game_icon(m_library.game_count, Assets::encoded_bytes(info.icon));
		m_library.games[m_library.game_count] = Game{
			.title          = info.title,
			.short_title    = info.short_title,
			.launch_product = info.launch_product,
			.accent         = info.accent,
			.banner         = m_assets.get(info.banner),
			.icon           = m_assets.get(info.icon),
		};
		m_library.game_count += 1;
	}

	m_tray.on_menu_open([this](os::TrayMenu* t_menu) { fill_tray_menu(t_menu); });
}

auto App::stack_widgets() -> void
{
	m_widgets.push(&m_carousel);
	m_widgets.push(&m_account_modal);
	m_widgets.push(&m_settings_panel);
	m_widgets.push(&m_unlock_screen);
	m_widgets.push(&m_app_menu);
	m_widgets.push(&m_update_overlay);
	m_widgets.push(&m_account_search);
	m_widgets.push(&m_context_menu);

	m_widgets.push_overlay(&m_toasts);
	m_widgets.push_overlay(&m_title_bar);
#ifdef PULSAR_PROFILING
	m_widgets.push_overlay(&m_profiler_overlay);
#endif
}

auto App::apply_settings(storage::LoadResult t_load_result) -> void
{
	apply_theme(m_settings.theme);
	apply_game_order();
	m_carousel.restore(m_settings.zoom_stop, m_settings.selected_game);

	if (t_load_result == storage::LoadResult::Failed) return;

	animation::set_enabled(m_settings.animations_enabled);
	animation::set_speed(m_settings.animation_speed);
	set_corner_roundness(m_settings.corner_roundness);
	m_settings_panel.sync_with_settings();

	const std::string_view last_run_version = m_settings.last_run_version;

	m_just_updated = !last_run_version.empty() && last_run_version != K_APP_VERSION;
	copy_to(K_APP_VERSION, m_settings.last_run_version);
}

auto App::lock() -> void
{
	m_locked = true;
	m_carousel.set_visible(false);
	m_account_search.close();
	m_title_bar.set_search_visible(false);
	m_tray.set_locked(true);
}

auto App::unlock() -> void
{
	m_locked = false;
	m_carousel.set_visible(true);
	m_title_bar.set_search_visible(true);
	m_unlock_screen.hide();
	m_tray.set_locked(false);
	m_last_activity = Clock::now();
}

auto App::lock_vault() -> void
{
	if (m_locked || !m_settings.master_password_enabled) return;

	m_save_due.reset();
	save_everything();
	clear_clipboard_secret();

	m_account_modal.forget_secrets();
	m_settings_panel.close();
	m_app_menu.close();
	m_context_menu.close();
	m_toasts.dismiss();

	m_library.wipe_accounts();
	m_master_key.lock();

	lock();
	m_unlock_screen.show_unlock();
}

auto App::lock_if_idle() -> void
{
	if (m_locked || m_settings.auto_lock_minutes == 0) return;
	if (Clock::now() - m_last_activity < std::chrono::minutes(m_settings.auto_lock_minutes)) return;

	lock_vault();
}

auto App::apply_game_order() -> void
{
	u8  order[K_MAX_GAMES]{};
	u32 count = 0;

	for (u32 i = 0; i < m_settings.game_order_count; i += 1) {
		for (u32 game = 0; game < m_library.game_count; game += 1) {
			if (m_library.games[game].title == m_settings.game_order[i]) {
				order[count] = static_cast<u8>(game);
				count += 1;
				break;
			}
		}
	}

	m_carousel.set_order({order, count});
}

auto App::save_settings() -> void
{
	m_settings.zoom_stop     = m_carousel.zoom_stop();
	m_settings.selected_game = m_carousel.selected_game();

	m_settings.game_order_count = 0;
	for (const u8 game : m_carousel.order()) {
		copy_to(m_library.games[game].title, m_settings.game_order[m_settings.game_order_count]);
		m_settings.game_order_count += 1;
	}

	Settings committed = m_settings;
	m_settings_panel.restore_committed_previews(&committed);
	storage::save_settings(&committed);
}

auto App::save_everything() -> void
{
	storage::save_accounts(&m_library, &m_master_key);
	save_settings();
}

auto App::request_save() -> void
{
	m_save_due = Clock::now() + K_SAVE_DELAY;
}

auto App::save_if_due() -> void
{
	if (!m_save_due || Clock::now() < *m_save_due) return;

	m_save_due.reset();
	save_everything();
}

auto App::commit_new_vault_key() -> void
{
	if (storage::save_accounts(&m_library, &m_master_key)) {
		save_settings();
	} else if (m_replaced_vault_key) {
		m_master_key.swap(&m_replaced_vault_key->key);
		m_settings.master_key = m_replaced_vault_key->params;
		m_toasts.notify(Notification{.message = "Your vault could not be saved, so the password was not changed."});
	}

	m_replaced_vault_key.reset();
}

auto App::copy_password(std::string_view t_password) -> void
{
	os::set_clipboard_secret(t_password);
	m_clipboard_secret = ClipboardSecret{os::clipboard_sequence(), Clock::now() + K_CLIPBOARD_SECRET_LIFETIME};

	constexpr auto LIFETIME_SECONDS = std::chrono::duration<float>(K_CLIPBOARD_SECRET_LIFETIME).count();
	m_toasts.notify_countdown("Password copied - it clears itself in 30 seconds.", LIFETIME_SECONDS);
}

auto App::open_setup() -> void
{
	if (!os::bring_window_to_front(os::WindowKind::Dialog)) {
		os::launch_process(os::executable_path(), "--setup");
	}
}

auto App::locate_riot_client(const Command& t_command) -> void
{
	if (m_client_picker.is_open()) return;

	os::PathRequest request{
		.kind              = os::PathKind::File,
		.title             = "Locate the Riot Client",
		.ok_label          = "Use this client",
		.start_path        = m_settings.riot_client_path[0] != '\0' ? std::string{m_settings.riot_client_path} : RiotClient::default_install_folder(),
		.file_type_name    = "Riot Client",
		.file_type_pattern = RiotClient::executable_name(),
	};

	m_locate_request = t_command;
	m_client_picker.open(&m_window, std::move(request));
}

auto App::take_picked_riot_client() -> void
{
	if (m_client_picker.is_open()) {
		animation::request_frame_after(K_PICKER_POLL_SECONDS);
		return;
	}

	const std::optional<std::string> picked  = m_client_picker.take_result();
	const std::optional<Command>     request = std::exchange(m_locate_request, std::nullopt);
	if (!picked || !request) return;

	const std::string client = RiotClient::executable_near(*picked);
	if (client.empty()) {
		m_toasts.notify(Notification{.message = "That isn't the Riot Client - pick RiotClientServices.exe.", .always_show = true});
		return;
	}

	copy_to(client, m_settings.riot_client_path);
	request_save();

	if (request->index >= 0 && account_for(request->account) != nullptr) {
		m_account_modal.quick_login(static_cast<u32>(request->index), request->account);
	} else {
		m_toasts.notify(Notification{.message = "Riot Client location saved."});
	}
}

auto App::open_account_search() -> void
{
	if (m_locked) return;

	m_app_menu.close();
	m_context_menu.close();
	m_update_overlay.close();
	m_account_search.open();
}

auto App::account_for(AccountRef t_account) const -> const Account*
{
	if (t_account.game >= m_library.game_count) return nullptr;
	if (t_account.index >= m_library.games[t_account.game].account_count) return nullptr;

	return m_library.account(t_account);
}

auto App::clear_clipboard_secret() -> void
{
	if (!m_clipboard_secret) return;

	os::clear_clipboard_if_unchanged(m_clipboard_secret->sequence);
	m_clipboard_secret.reset();
}

auto App::fill_tray_menu(os::TrayMenu* t_menu) const -> void
{
	if (m_locked) return;

	for (const u8 game : m_carousel.order()) {
		if (t_menu->game_count == os::K_TRAY_MAX_GAMES) break;

		const VisibleAccounts visible = m_library.visible_accounts(game);

		os::TrayGame* entry = &t_menu->games[t_menu->game_count];
		t_menu->game_count += 1;

		copy_to(m_library.games[game].title, entry->title);
		entry->game          = static_cast<i32>(game);
		entry->first_account = t_menu->account_count;
		entry->account_count = 0;

		for (u32 row = 0; row < visible.count && t_menu->account_count < os::K_TRAY_MAX_ACCOUNTS; row += 1) {
			const Account*         account = m_library.account(visible.refs[row]);
			const std::string_view note    = account->note;

			os::TrayAccount* item = &t_menu->accounts[t_menu->account_count];
			copy_to(note.empty() ? std::string_view{account->username} : note, item->label);
			item->game = static_cast<i32>(game);
			item->row  = static_cast<i32>(row);

			t_menu->account_count += 1;
			entry->account_count += 1;
		}
	}
}

auto App::pump_input() -> void
{
	PULSAR_PROFILE_SCOPE("Input");

	m_window.pump_messages();
	m_window.set_close_to_tray(m_settings.close_to_tray && m_tray.is_icon_visible());
	m_tray.set_colors(os::TrayColors{
		.background    = g_theme.popup,
		.hover         = mix(g_theme.popup, m_settings.accent, 0.42f),
		.text          = g_theme.text,
		.text_disabled = g_theme.text_faint,
		.separator     = g_theme.separator,
	});

	handle_tray_event();

	for (const os::InputEvent& event : m_window.input_events()) {
		handle_input(event);
	}
}

auto App::handle_tray_event() -> void
{
	const os::TrayEvent event = m_tray.take_event();
	if (event.type != os::TrayEventType::None) {
		m_last_activity = Clock::now();
	}

	switch (event.type) {
		case os::TrayEventType::Exit: {
			m_window.request_quit();
			break;
		}

		case os::TrayEventType::ShowWindow: {
			m_window.restore();
			break;
		}

		case os::TrayEventType::QuickLogin: {
			if (m_locked || event.game < 0 || event.row < 0) break;

			const auto game = static_cast<u32>(event.game);
			if (const auto account = m_library.visible_account(game, static_cast<u32>(event.row))) {
				m_account_modal.quick_login(game, *account);
			}

			break;
		}

		case os::TrayEventType::None: {
			break;
		}
	}
}

auto App::handle_input(const os::InputEvent& t_event) -> void
{
	m_last_activity = Clock::now();

	const bool shortcut_held = os::modifiers().shortcut;
	if (t_event.type == os::InputEventType::KeyDown && t_event.key == os::Key::L && shortcut_held && !m_locked) {
		lock_vault();
		return;
	}

	if (t_event.type == os::InputEventType::KeyDown && t_event.key == os::Key::S && shortcut_held && !m_locked) {
		if (m_account_search.is_open()) {
			m_account_search.close();
			return;
		}

		if (!m_account_modal.is_blocking() && !m_settings_panel.is_blocking()) {
			open_account_search();
			return;
		}
	}

	if (t_event.type == os::InputEventType::KeyDown && t_event.key == os::Key::Comma && shortcut_held && !m_locked && !m_settings_panel.is_blocking()) {
		m_app_menu.close();
		m_settings_panel.open();
		return;
	}

	if (t_event.type == os::InputEventType::MouseMove) {
		m_mouse = t_event.position;
	} else if (t_event.type == os::InputEventType::MouseDown) {
		m_pointer_down = true;
	} else if (t_event.type == os::InputEventType::MouseUp) {
		m_pointer_down = false;
	}

	const bool consumed = m_widgets.dispatch(t_event);
	process_commands();

	const bool is_action =
		t_event.type == os::InputEventType::MouseUp || t_event.type == os::InputEventType::KeyDown || t_event.type == os::InputEventType::RightClick;
	if (consumed && is_action) {
		request_save();
	}
}

auto App::process_commands() -> void
{
	while (const std::optional<Command> command = m_commands.pop()) {
		process(*command);
	}
}

auto App::process(const Command& t_command) -> void
{
	animation::request_frame();

	switch (t_command.type) {
		case CommandType::ToggleAppMenu: {
			if (m_app_menu.is_open()) {
				m_app_menu.close();
			} else {
				m_update_overlay.close();
				m_app_menu.open(!m_locked, update_status(m_updater.stage()), m_title_bar.layout().button_rect(TitleBarButton::Menu).x);
			}

			break;
		}

		case CommandType::ToggleUpdateOverlay: {
			if (m_update_overlay.is_open()) {
				m_update_overlay.close();
			} else {
				m_update_overlay.open();
			}

			break;
		}

		case CommandType::OpenUpdateOverlay: {
			m_update_overlay.open();
			break;
		}

		case CommandType::OpenSettings: {
			m_settings_panel.open();
			break;
		}

		case CommandType::OpenSetup: {
			open_setup();
			break;
		}

		case CommandType::OpenDataFolder: {
			const std::string directory = storage::data_directory();
			if (!directory.empty()) {
				os::open_path(directory);
			}

			break;
		}

		case CommandType::CheckForUpdates: {
			m_updater.check_for_update();
			m_update_overlay.begin_check();
			break;
		}

		case CommandType::OpenGame: {
			m_account_modal.open(t_command.index);
			m_account_modal.set_art_source(m_carousel.art_source(static_cast<u32>(t_command.index)));
			break;
		}

		case CommandType::SaveChanges: {
			request_save();
			break;
		}

		case CommandType::RequestNewMasterPassword: {
			m_replaced_vault_key.emplace();
			m_replaced_vault_key->key.swap(&m_master_key);
			m_replaced_vault_key->params = m_settings.master_key;
			lock();
			m_unlock_screen.show_setup();
			break;
		}

		case CommandType::VaultUnlocked: {
			storage::load_accounts(&m_library, &m_master_key);
			unlock();
			break;
		}

		case CommandType::VaultCreated: {
			commit_new_vault_key();
			unlock();
			break;
		}

		case CommandType::ShowAccountMenu: {
			open_account_menu(t_command);
			break;
		}

		case CommandType::ShowTextMenu: {
			open_text_menu(t_command);
			break;
		}

		case CommandType::CopyUsername: {
			if (const Account* account = m_account_modal.account_at_row(t_command.index)) {
				os::set_clipboard_text(account->username);
			}

			break;
		}

		case CommandType::CopyPassword: {
			if (const Account* account = m_account_modal.account_at_row(t_command.index)) {
				copy_password(account->password);
			}

			break;
		}

		case CommandType::EditText: {
			t_command.text_input->apply(t_command.text_edit);
			break;
		}

		case CommandType::UndoDelete: {
			m_account_modal.undo_delete();
			break;
		}

		case CommandType::ToggleFavorite: {
			m_account_modal.toggle_favorite(t_command.index);
			break;
		}

		case CommandType::LockVault: {
			lock_vault();
			break;
		}

		case CommandType::OpenAccountSearch: {
			open_account_search();
			break;
		}

		case CommandType::EditAccount: {
			if (account_for(t_command.account) != nullptr) {
				m_settings_panel.close();
				m_account_modal.edit_account(t_command.account);
			}

			break;
		}

		case CommandType::LoginAccount: {
			if (account_for(t_command.account) != nullptr && t_command.index >= 0) {
				m_settings_panel.close();
				m_account_modal.quick_login(static_cast<u32>(t_command.index), t_command.account);
			}

			break;
		}

		case CommandType::CopyAccountUsername: {
			if (const Account* account = account_for(t_command.account)) {
				os::set_clipboard_text(account->username);
				m_toasts.notify(Notification{.message = "Username copied."});
			}

			break;
		}

		case CommandType::CopyAccountPassword: {
			if (const Account* account = account_for(t_command.account)) {
				copy_password(account->password);
			}

			break;
		}

		case CommandType::LocateRiotClient: {
			locate_riot_client(t_command);
			break;
		}
	}
}

auto App::open_account_menu(const Command& t_command) -> void
{
	const Account* account = m_account_modal.account_at_row(t_command.index);
	if (account == nullptr) return;

	const ContextMenuItem items[]{
		{account->favorite ? "Unpin" : "Pin to top", Command{.type = CommandType::ToggleFavorite, .index = t_command.index}},
		{"Copy username", Command{.type = CommandType::CopyUsername, .index = t_command.index}},
		{"Copy password", Command{.type = CommandType::CopyPassword, .index = t_command.index}},
	};

	m_context_menu.open(t_command.position, items, m_window.size());
}

auto App::open_text_menu(const Command& t_command) -> void
{
	TextInput* input = t_command.text_input;

	const auto item = [input](std::string_view t_label, TextEdit t_edit, std::string_view t_shortcut) {
		const Command edit{.type = CommandType::EditText, .text_input = input, .text_edit = t_edit};

		return ContextMenuItem{t_label, edit, input->can_apply(t_edit), t_shortcut};
	};

	const ContextMenuItem items[]{
		item("Cut", TextEdit::Cut, PULSAR_SHORTCUT_KEY "+X"),
		item("Copy", TextEdit::Copy, PULSAR_SHORTCUT_KEY "+C"),
		item("Paste", TextEdit::Paste, PULSAR_SHORTCUT_KEY "+V"),
		item("Select all", TextEdit::SelectAll, PULSAR_SHORTCUT_KEY "+A"),
	};

	m_context_menu.open(t_command.position, items, m_window.size());
}

auto App::announce_update_stage() -> void
{
	const UpdateStage stage = m_updater.stage();
	if (stage == m_announced_update_stage) return;

	m_announced_update_stage = stage;

	const Command open_updates{.type = CommandType::OpenUpdateOverlay};

	const bool watching = m_update_overlay.is_open() || m_update_overlay.wants_status();

	if ((stage == UpdateStage::Available || stage == UpdateStage::ManualUpgradeRequired) && !watching) {
		char message[96];
		std::snprintf(message, sizeof(message), "Version %s available", m_updater.manifest().version);
		m_toasts.notify(Notification{.message = message, .icon = Asset::IconDownload, .on_click = open_updates});
	} else if (stage == UpdateStage::Error && !watching) {
		m_toasts.notify(Notification{.message = "The update could not be installed.", .icon = Asset::IconUpdate, .on_click = open_updates});
	}
}

auto App::announce_first_run_after_update() -> void
{
	if (m_locked || !std::exchange(m_just_updated, false)) return;

	const std::string_view notes_version = m_settings.release_notes_version;
	const std::string_view notes         = m_settings.release_notes;

	if (notes_version == K_APP_VERSION && !notes.empty()) {
		m_update_overlay.show_release_notes(notes_version, notes);
		m_settings.release_notes_version[0] = '\0';
		m_settings.release_notes[0]         = '\0';
		request_save();
		return;
	}

	char message[96];
	std::snprintf(message, sizeof(message), "Updated to %s", K_APP_VERSION);
	m_toasts.notify(Notification{.message = message, .icon = Asset::IconUpdate});
}

auto App::announce_unreadable_storage() -> void
{
	if (m_unreadable_storage_announced || (storage::can_save_settings() && storage::can_save_accounts())) return;

	m_unreadable_storage_announced = true;
	m_toasts.notify(Notification{.message = "Saved data could not be read - changes will not be kept"});
}

auto App::relaunch_if_update_installed() -> void
{
	if (!m_updater.consume_ready_to_relaunch()) return;

	copy_to(m_updater.manifest().version, m_settings.release_notes_version);
	copy_to(m_updater.manifest().notes, m_settings.release_notes);
	save_everything();

	// The replacement build would otherwise find this process's mutex and exit as a duplicate.
	m_instance_guard.release();

	os::launch_process(os::executable_path());
	m_window.request_quit();
}

auto App::redraw_while_resizing() -> void
{
	if (m_window.physical_width() == 0 || m_window.physical_height() == 0) return;

	m_renderer.resize(&m_window);
	frame();
}

auto App::reload_fonts() -> bool
{
	const auto load = [this] {
		return m_fonts.load(&m_renderer, m_settings.font_name, m_settings.font_size, m_settings.secondary_font_size, m_window.dpi_scale());
	};

	if (load()) return true;

	copy_to(Settings{}.font_name, m_settings.font_name);

	return load();
}

auto App::frame() -> void
{
	if (m_in_frame) return;

	m_in_frame = true;
	PULSAR_PROFILE_FRAME_BEGIN();

	const auto  now           = std::chrono::steady_clock::now();
	const float delta_seconds = std::chrono::duration<float>(now - m_last_frame_time).count();
	m_last_frame_time         = now;

	const Vec2 restored = m_window.restored_size();
	if (restored.x >= K_MIN_WINDOW_WIDTH && restored.y >= K_MIN_WINDOW_HEIGHT) {
		m_settings.window_width  = static_cast<u32>(std::lround(restored.x));
		m_settings.window_height = static_cast<u32>(std::lround(restored.y));
	}

	m_carousel.set_bounds(content_rect(m_window.size()));
	set_pixel_scale(m_window.dpi_scale());
	update_theme(delta_seconds);

	{
		PULSAR_PROFILE_SCOPE("Widgets.Update");
		m_widgets.update(m_mouse, delta_seconds);
	}

	m_truncation_hint.update(delta_seconds, m_pointer_down);

	const i32 detached_game = m_account_modal.detached_game();
	if (detached_game >= 0) {
		m_account_modal.set_art_source(m_carousel.art_source(static_cast<u32>(detached_game)));
	}

	m_carousel.set_detached_game(detached_game);

	if (!m_window.is_mouse_over_resize_border()) {
		m_window.set_cursor(m_widgets.cursor());
	}

	m_effect_seconds += delta_seconds;
	m_renderer.set_effect_time(m_effect_seconds);

	const bool shown = !m_window.is_minimized() && !m_window.is_hidden();

	if (!m_settings.snow || !m_settings.animations_enabled) {
		m_snowfall.clear();
	} else if (shown) {
		m_snowfall.update(delta_seconds, content_rect(m_window.size()), m_window.is_focused());
	}

	if (shown) {
		render();
	}

	PULSAR_PROFILE_FRAME_END();
	m_in_frame = false;
}

auto App::draw_status_bar() -> void
{
	const Vec2 window = m_window.size();
	const Rect status_bar{0.0f, window.y - K_STATUS_BAR_HEIGHT, window.x, K_STATUS_BAR_HEIGHT};

	m_draw_list.add_rect(Rect{0.0f, K_TITLE_BAR_HEIGHT, window.x, 1.0f}, g_theme.chrome_seam);
	m_draw_list.add_rect(Rect{0.0f, status_bar.y - 1.0f, window.x, 1.0f}, g_theme.chrome_seam);
	m_draw_list.add_rect(status_bar, g_theme.chrome);

	const Rect  mark{K_STATUS_PADDING, status_bar.y + (K_STATUS_BAR_HEIGHT - K_STATUS_MARK_SIZE) * 0.5f, K_STATUS_MARK_SIZE, K_STATUS_MARK_SIZE};
	const Font& font            = m_fonts.secondary;
	const float baseline        = font.centered_baseline(status_bar) - K_STATUS_BASELINE_NUDGE;
	const bool  protected_vault = m_settings.master_password_enabled;

	controls::draw_lock(&m_draw_list, mark, g_theme.text_dim, g_theme.chrome, !m_locked);

	std::string_view state = "Vault unlocked";
	if (m_locked) {
		state = "Vault locked";
	} else if (!protected_vault) {
		state = "No master password";
	}

	float x = mark.right() + K_STATUS_MARK_GAP;
	draw_text(&m_draw_list, font, Vec2{x, baseline}, state, g_theme.text_dim);
	x += text_width(font, state);

	if (!m_locked && protected_vault && m_settings.auto_lock_minutes != 0) {
		const auto limit     = std::chrono::minutes(m_settings.auto_lock_minutes);
		const auto idle      = std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - m_last_activity);
		const auto remaining = std::max(std::chrono::seconds(1), limit - idle);
		const auto minutes   = static_cast<u32>((remaining.count() + 59) / 60);

		char       countdown[48];
		const int  written = std::snprintf(countdown, sizeof(countdown), "auto-locks in %u min", minutes);
		const Rect dot{x + K_STATUS_DOT_GAP, baseline - font.ascent * 0.35f - K_STATUS_DOT_SIZE * 0.5f, K_STATUS_DOT_SIZE, K_STATUS_DOT_SIZE};

		m_draw_list.add_rounded_rect(dot, rounded(K_STATUS_DOT_SIZE * 0.5f), g_theme.text_faint);
		draw_text(&m_draw_list, font, Vec2{dot.right() + K_STATUS_DOT_GAP, baseline}, std::string_view{countdown, static_cast<usize>(std::max(written, 0))},
		          g_theme.text_faint);
		animation::request_frame_after(static_cast<float>(remaining.count() - static_cast<i64>(minutes - 1) * 60) + 0.05f);
	}

	const bool panel_open = m_account_modal.is_blocking() || m_settings_panel.is_blocking();
	if (m_carousel.is_visible() && !panel_open) {
		m_carousel.draw_status_bar(&m_draw_list);
	}
}

auto App::render() -> void
{
	PULSAR_PROFILE_SCOPE("Render");

	m_draw_list.clear();

	const Vec2  window   = m_window.size();
	const Color backdrop = g_theme.window;
	m_draw_list.add_backdrop(Rect{0.0f, 0.0f, window.x, window.y}, backdrop, backdrop, backdrop, backdrop);

	begin_truncation_probe(&m_draw_list, m_mouse);
	draw_status_bar();
	m_snowfall.draw(&m_draw_list);

	{
		PULSAR_PROFILE_SCOPE("Render.BuildGeometry");
		m_widgets.draw(&m_draw_list);
		m_truncation_hint.capture(&m_draw_list);
		m_truncation_hint.draw(&m_draw_list, content_rect(m_window.size()));
		m_draw_list.finish();
	}

	if (m_draw_list.has_animated_effects()) {
		animation::request_frame();
	}

	m_renderer.set_backdrop(static_cast<u32>(m_settings.background_style), m_settings.background_intensity,
	                        m_settings.background_light ? m_settings.background_light_intensity : 0.0f,
	                        m_settings.background_grain ? m_settings.background_grain_intensity : 0.0f);
	m_renderer.render(&m_draw_list, g_theme.window);
}

auto App::run() -> void
{
	while (!m_window.should_quit()) {
		pump_input();
		announce_first_run_after_update();
		announce_unreadable_storage();
		frame();

		m_updater.update();
		announce_update_stage();
		process_commands();
		take_picked_riot_client();
		relaunch_if_update_installed();
		save_if_due();
		lock_if_idle();
		if (m_clipboard_secret && Clock::now() >= m_clipboard_secret->clear_at) {
			clear_clipboard_secret();
		}

		m_window.set_excluded_from_capture(m_settings.hide_from_capture);

		const float requested_wait = animation::take_idle_wait(K_IDLE_POLL_SECONDS);
		const float wait           = m_window.is_hidden() || m_window.is_minimized() ? K_IDLE_POLL_SECONDS : requested_wait;

		if (wait > 0.0f && m_window.wait_for_messages(wait)) {
			m_last_frame_time = std::chrono::steady_clock::now() - K_RESUME_FRAME_TIME;
		}

		debug_log::mark_ui_thread_alive();
	}

	save_everything();
	clear_clipboard_secret();
}
