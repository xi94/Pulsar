#include "app.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <print>
#include <ranges>
#include <tuple>
#include <utility>

#include <sodium.h>

#include "core/animation.h"
#include "core/app_identity.h"
#include "core/debug_log.h"
#include "core/profiler.h"
#include "core/str.h"
#include "games.h"
#include "login/riot_client.h"
#include "os/clipboard.h"
#include "os/installation.h"
#include "ui/controls.h"
#include "ui/text.h"
#include "ui/text_input.h"
#include "ui/theme.h"
#include "ui/window_layout.h"

namespace {
constexpr u32   K_DRAW_LIST_VERTEX_CAPACITY = 1 << 16;
constexpr u32   K_DRAW_LIST_INDEX_CAPACITY  = (1 << 16) * 3 / 2;
constexpr auto  K_SAVE_DELAY                = std::chrono::milliseconds(500);
constexpr auto  K_WIPE_GRACE                = std::chrono::milliseconds(600);
constexpr float K_IDLE_POLL_SECONDS         = 0.25f;
constexpr auto  K_RESUME_FRAME_TIME         = std::chrono::microseconds(16667);
constexpr auto  K_CLIPBOARD_SECRET_LIFETIME = std::chrono::seconds(30);
constexpr auto  K_GRAPHICS_RETRY            = std::chrono::seconds(1);
constexpr float K_PICKER_POLL_SECONDS       = 0.1f;
constexpr float K_INTRO_SECONDS             = 0.5f;
constexpr float K_INTRO_MAX_STEP            = 1.0f / 30.0f;
constexpr float K_INTRO_START_SCALE         = 0.97f;
constexpr float K_TRAY_ACCENT_TINT          = 0.06f;

constexpr float K_STATUS_PADDING        = 14.0f;
constexpr float K_STATUS_MARK_SIZE      = 15.0f;
constexpr float K_STATUS_MARK_GAP       = 7.0f;
constexpr float K_STATUS_DOT_SIZE       = 3.0f;
constexpr float K_STATUS_DOT_GAP        = 7.0f;
constexpr float K_STATUS_BASELINE_NUDGE = 2.0f;

constexpr os::AppMenuItem K_APP_MENU_ITEMS[]{
	{.label = "Settings\u2026", .shortcut = os::Key::COMMA, .id = enum_index(CommandType::OPEN_SETTINGS)},
	{.label = "Check for Updates\u2026", .shortcut = os::Key::NONE, .id = enum_index(CommandType::CHECK_FOR_UPDATES)},
};

static_assert(os::K_TRAY_MAX_GAMES >= K_MAX_GAMES);

auto log_startup_phase(const char* t_phase) -> void
{
	using Clock = std::chrono::steady_clock;

	static const Clock::time_point PROCESS_START  = Clock::now();
	static Clock::time_point       phase_start    = PROCESS_START;
	static const char*             previous_phase = nullptr;

	const Clock::time_point now = Clock::now();

	if (previous_phase != nullptr) {
		debug_log::write("startup", "%-20s %6.1f ms   (%6.1f ms in)", previous_phase, std::chrono::duration<float, std::milli>(now - phase_start).count(), std::chrono::duration<float, std::milli>(now - PROCESS_START).count());
	}

	previous_phase = t_phase;
	phase_start    = now;
}

[[nodiscard]] auto update_status(UpdateStage t_stage) -> std::string_view
{
	switch (t_stage) {
		using enum UpdateStage;

		case UP_TO_DATE: {
			return "Up to date";
		}

		case AVAILABLE:
		case MANUAL_UPGRADE_REQUIRED: {
			return "Update available";
		}

		case CHECKING: {
			return "Checking...";
		}

		case DOWNLOADING:
		case VERIFYING:
		case INSTALLING: {
			return "Updating...";
		}

		case READY_TO_RELAUNCH: {
			return "Restart to update";
		}

		case IDLE:
		case CHECK_FAILED:
		case UPDATE_FAILED:
		case CANCELLED: {
			break;
		}
	}

	return "";
}

// An update that failed to install or was cancelled is still there to get, so the tray offers it again.
[[nodiscard]] auto tray_update(UpdateStage t_stage) -> os::TrayUpdate
{
	switch (t_stage) {
		using enum UpdateStage;

		case AVAILABLE:
		case MANUAL_UPGRADE_REQUIRED:
		case UPDATE_FAILED:
		case CANCELLED: {
			return os::TrayUpdate::AVAILABLE;
		}

		case DOWNLOADING:
		case VERIFYING:
		case INSTALLING:
		case READY_TO_RELAUNCH: {
			return os::TrayUpdate::INSTALLING;
		}

		case IDLE:
		case CHECKING:
		case UP_TO_DATE:
		case CHECK_FAILED: {
			break;
		}
	}

	return os::TrayUpdate::NONE;
}

auto guard_against_overlays(bool t_block_injection) -> void
{
	os::register_app_identity();

	if (!t_block_injection) {
		debug_log::write("app", "overlay injection guard disabled by setting");
		return;
	}

	switch (os::block_injection()) {
		using enum os::InjectionGuard;

		case BLOCKED: {
			debug_log::write("app", "extension-point DLL injection blocked");
			break;
		}

		case REFUSED: {
			debug_log::write("app", "extension-point block refused, err=%u", os::last_error());
			break;
		}

		case UNSUPPORTED: {
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
	: m_login_session(&m_library, &m_settings, &m_commands)
	, m_carousel(&m_library, &m_settings, &m_fonts, &m_assets, &m_toasts, &m_login_session, &m_commands)
	, m_toasts(&m_settings, &m_fonts, &m_assets, &m_window, &m_commands)
	, m_account_modal(&m_library, &m_settings, &m_fonts, &m_assets, &m_window, &m_toasts, &m_login_session, &m_commands)
	, m_settings_panel(&m_settings, &m_fonts, &m_renderer, &m_window, &m_assets, &m_commands)
	, m_unlock_screen(&m_settings, &m_master_key, &m_fonts, &m_assets, &m_window, &m_commands)
	, m_app_menu(&m_window, &m_fonts, &m_assets, &m_commands)
	, m_update_overlay(&m_updater, &m_settings, &m_fonts, &m_assets, &m_window)
	, m_account_search(&m_library, &m_settings, &m_fonts, &m_assets, &m_window, &m_commands)
	, m_context_menu(&m_fonts, &m_assets, &m_commands)
	, m_title_bar(&m_window, &m_updater, &m_update_overlay, &m_fonts, &m_assets, &m_commands)
	, m_truncation_hint(&m_fonts)
#ifdef PULSAR_PROFILING
	, m_profiler_overlay(&m_fonts)
#endif
{
}

auto App::start(bool t_from_startup) -> App::StartResult
{
	if (!m_instance_guard.m_first_instance) {
		const bool activated = os::activate_running_instance();
		debug_log::write("app", "another instance is already running (%s) - exiting", activated ? "brought it to the front" : "it never answered");

		return StartResult::ALREADY_RUNNING;
	}

	m_assets.begin_decode();
	os::installation::refresh_registration();

	log_startup_phase("LoadSettings");
	const storage::LoadResult settings_result = storage::load_settings(&m_settings);
	m_settings.start_with_system              = os::installation::starts_with_system();
	m_start_with_system                       = m_settings.start_with_system;

	log_startup_phase("GuardAgainstOverlays");
	guard_against_overlays(m_settings.block_overlay_injection);
	RiotClient::prepare_automation();

	if (sodium_init() < 0) {
		std::println("Failed to initialize libsodium.");
		return StartResult::FAILED;
	}

	log_startup_phase("CreateGraphics");
	if (!create_graphics()) return StartResult::FAILED;

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
		m_unlock_screen.show_unlock(false);
	} else {
		m_unlock_screen.show_setup(false);
	}

	m_window.on_redraw([this] { redraw_while_resizing(); });
	m_window.on_dpi_changed([this] { reload_fonts(); });
	m_window.on_session_end([this] {
		save_everything();
		clear_clipboard_secret();
	});

	m_start_time      = std::chrono::steady_clock::now();
	m_last_frame_time = m_start_time;
	m_last_activity   = m_start_time;

	log_startup_phase("FirstFrame");
	frame();

	if (!t_from_startup || !m_settings.start_minimized) {
		m_window.show();
	} else if (!m_settings.close_to_tray || !m_tray.is_icon_visible()) {
		m_window.show_minimized();
	}

	log_startup_phase("Done");

	return StartResult::OK;
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
	m_window.set_title_bar(K_TITLE_BAR_HEIGHT, [this](Vec2 t_point) { return m_title_bar.layout().button_at(t_point) != TitleBarButton::NONE; });
	m_window.set_app_menu(K_APP_MENU_ITEMS);

	if (!m_renderer.init(&m_window, m_settings.renderer)) {
		std::println("Failed to initialize renderer.");
		return false;
	}

	if (!m_assets.finish_upload(&m_renderer)) {
		std::println("Failed to load one or more embedded assets.");
		return false;
	}

	m_snowfall.create_texture(&m_renderer);

	m_unlock_screen.m_app_icon = m_assets.texture(Asset::ICON_LOGO);

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
			.banner         = m_assets.texture(info.banner),
			.icon           = m_assets.texture(info.icon),
		};
		m_library.game_count += 1;
	}

	m_tray.set_logo(Assets::encoded_bytes(Asset::ICON_LOGO));
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

	if (t_load_result == storage::LoadResult::FAILED) return;

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
	m_account_search.close();
	m_title_bar.m_search_visible = false;
	m_tray.set_locked(true);
}

auto App::unlock() -> void
{
	m_locked                     = false;
	m_title_bar.m_search_visible = true;
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
	m_carousel.m_library_view.forget_secrets();
	m_settings_panel.close();
	m_app_menu.close();
	m_context_menu.close();
	m_toasts.dismiss();

	m_master_key.lock();
	m_wipe_due = Clock::now() + K_WIPE_GRACE;

	lock();
	m_unlock_screen.show_unlock(true);
	wipe_locked_accounts(m_window.is_hidden() || m_window.is_minimized());
}

// The accounts stay in memory until the lock screen covers the window, so what's on screen fades under it instead of emptying first.
auto App::wipe_locked_accounts(bool t_now) -> void
{
	if (!m_wipe_due) return;
	if (!t_now && !m_unlock_screen.covers_window() && Clock::now() < *m_wipe_due) return;

	m_wipe_due.reset();
	m_library.wipe_accounts();
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

	for (const u32 i : std::views::iota(0u, m_settings.game_order_count)) {
		for (const u32 game : std::views::iota(0u, m_library.game_count)) {
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
	m_settings.zoom_stop     = m_carousel.m_zoom_stop;
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

auto App::reset_everything() -> void
{
	if (!m_locked) return;

	if (!storage::delete_all_data()) {
		debug_log::write("app", "reset could not delete every saved file");
	}

	m_library.wipe_accounts();
	m_wipe_due.reset();
	m_save_due.reset();
	m_replaced_vault_key.reset();
	m_master_key.lock();

	Settings fresh{};
	fresh.window_width      = m_settings.window_width;
	fresh.window_height     = m_settings.window_height;
	fresh.start_with_system = m_settings.start_with_system;
	m_settings              = fresh;

	apply_settings(storage::LoadResult::NO_FILE);
	reload_fonts();
	save_settings();
	m_unlock_screen.show_setup(true);
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
	if (!os::bring_window_to_front(os::WindowKind::DIALOG)) {
		os::launch_process(os::executable_path(), "--setup");
	}
}

auto App::locate_riot_client(const Command& t_command) -> void
{
	if (m_client_picker.is_open()) return;

	os::PathRequest request{
		.kind              = os::PathKind::FILE,
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

	if (request->game && m_library.find_account(request->account) != nullptr) {
		if (m_carousel.is_library() && !m_account_modal.is_blocking()) {
			m_login_session.request(*request->game, request->account);
		} else {
			m_account_modal.quick_login(*request->game, request->account);
		}
	} else {
		m_toasts.notify(Notification{.message = "Riot Client location saved."});
	}
}

// However a game's accounts open, its card lifts into the popup as if it was clicked, so the glass always has the same thing behind it.
auto App::lift_game_card(u32 t_game) -> void
{
	m_carousel.show_game(t_game);
	m_account_modal.m_art_source = m_carousel.art_source(t_game);
}

auto App::open_account_search() -> void
{
	if (m_locked) return;

	m_app_menu.close();
	m_context_menu.close();
	m_update_overlay.close();
	m_account_search.open();
}

// A login that runs while Pulsar isn't the window in front shows in a small window by the tray, because the app is in the tray or the Riot Client has opened over it. Once shown for a login, it follows that login to the end.
auto App::update_login_toast() -> void
{
	const std::optional<AccountRef> shown   = m_login_session.m_shown_account;
	const bool                      running = shown && !m_login_session.is_finished();

	if (running && !m_window.is_focused()) {
		m_login_toast = true;
	}

	if (!m_login_toast) return;

	const Account* account = shown ? m_library.find_account(*shown) : nullptr;
	if (account == nullptr || m_login_session.stage() == LoginStage::CANCELLED) {
		m_tray.hide_login();
		m_login_toast = false;
		return;
	}

	os::TrayLogin login{
		.state      = os::TrayLoginState::RUNNING,
		.step       = m_login_session.step(),
		.step_count = LoginSession::K_STEP_COUNT,
		.progress   = m_login_session.step_progress(),
	};

	if (!running) {
		login.state   = m_login_session.stage() == LoginStage::SUCCESS ? os::TrayLoginState::SUCCEEDED : os::TrayLoginState::FAILED;
		m_login_toast = false;
	}

	copy_to(account->display_name(), login.account);
	copy_to(m_login_session.m_status, login.status);
	m_tray.show_login(login);
}

auto App::clear_clipboard_secret() -> void
{
	if (!m_clipboard_secret) return;

	os::clear_clipboard_if_unchanged(m_clipboard_secret->sequence);
	m_clipboard_secret.reset();
}

auto App::fill_tray_menu(os::TrayMenu* t_menu) -> void
{
	m_updater.check_for_update();
	t_menu->update = tray_update(m_updater.stage());

	if (m_locked) return;

	t_menu->can_lock = m_settings.master_password_enabled;

	for (const u8 game : m_carousel.order()) {
		if (t_menu->game_count == os::K_TRAY_MAX_GAMES) break;

		const VisibleAccounts visible = m_library.visible_accounts(game);
		const u32             room    = os::K_TRAY_MAX_ACCOUNTS - t_menu->account_count;
		const auto            listed  = visible.view().first(std::min(visible.count, room));

		os::TrayGame* entry = &t_menu->games[t_menu->game_count];
		t_menu->game_count += 1;

		copy_to(m_library.games[game].title, entry->title);
		entry->game          = game;
		entry->first_account = t_menu->account_count;
		entry->account_count = static_cast<u32>(listed.size());

		for (const u32 row : std::views::iota(0u, entry->account_count)) {
			const Account&   account = *m_library.account(listed[row]);
			os::TrayAccount* item    = &t_menu->accounts[entry->first_account + row];

			copy_to(account.display_name(), item->label);
			copy_to(std::string_view{account.region}, item->region);
			item->game = game;
			item->row  = row;
		}

		t_menu->account_count += entry->account_count;
	}
}

auto App::pump_input() -> void
{
	PULSAR_PROFILE_SCOPE("Input");

	m_window.pump_messages();
	m_window.m_close_to_tray = m_settings.close_to_tray && m_tray.is_icon_visible();
	m_tray.set_colors(os::TrayColors{
		.background    = mix(g_theme.surface, m_settings.accent, K_TRAY_ACCENT_TINT),
		.border        = g_theme.border,
		.separator     = g_theme.separator,
		.scroll_thumb  = g_theme.scroll_thumb,
		.shadow        = g_theme.shadow,
		.text          = g_theme.text,
		.text_dim      = g_theme.text_dim,
		.text_disabled = g_theme.text_faint,
		.accent        = m_settings.accent,
		.accent_ink    = controls::contrasting_theme_text(m_settings.accent),
		.success       = g_theme.success,
		.error         = g_theme.error,
	});

	handle_tray_event();
	m_tray.set_update(tray_update(m_updater.stage()));

	for (const os::InputEvent& event : m_window.input_events()) {
		handle_input(event);
	}
}

auto App::handle_tray_event() -> void
{
	const os::TrayEvent event = m_tray.take_event();
	if (event.type != os::TrayEventType::NONE) {
		m_last_activity = Clock::now();
	}

	switch (event.type) {
		using enum os::TrayEventType;

		case EXIT: {
			m_window.request_quit();
			break;
		}

		case SHOW_WINDOW: {
			m_window.restore();
			break;
		}

		case LOCK: {
			lock_vault();
			break;
		}

		case RETRY_LOGIN: {
			if (!m_locked) {
				m_login_session.retry();
			}

			break;
		}

		case SHOW_UPDATES: {
			m_window.restore();
			m_commands.push(Command{.type = CommandType::OPEN_UPDATE_OVERLAY});
			break;
		}

		case QUICK_LOGIN: {
			if (m_locked) break;

			if (const auto account = m_library.visible_account(event.game, event.row)) {
				m_account_modal.quick_login(event.game, *account);
				lift_game_card(event.game);
			}

			break;
		}

		case NONE: {
			break;
		}
	}
}

auto App::handle_input(const os::InputEvent& t_event) -> void
{
	m_last_activity = Clock::now();

	switch (t_event.type) {
		using enum os::InputEventType;

		case MENU_ITEM: {
			m_commands.push(Command{.type = enum_at<CommandType>(t_event.menu_item)});
			return;
		}

		case KEY_DOWN: {
			if (run_shortcut(t_event.key)) return;

			break;
		}

		case MOUSE_MOVE: {
			m_mouse = t_event.position;
			break;
		}

		case MOUSE_DOWN: {
			m_pointer_down = true;
			break;
		}

		case MOUSE_UP: {
			m_pointer_down = false;
			break;
		}

		default: {
			break;
		}
	}

	const bool consumed = m_widgets.dispatch(t_event);
	run_commands();

	const bool is_action = t_event.type == os::InputEventType::MOUSE_UP || t_event.type == os::InputEventType::KEY_DOWN || t_event.type == os::InputEventType::RIGHT_CLICK;
	if (consumed && is_action) {
		request_save();
	}
}

auto App::run_shortcut(os::Key t_key) -> bool
{
	if (m_locked || !os::modifiers().shortcut) return false;

	switch (t_key) {
		using enum os::Key;

		case L: {
			lock_vault();
			return true;
		}

		case S: {
			if (m_account_search.m_open) {
				m_account_search.close();
				return true;
			}

			if (m_account_modal.is_blocking() || m_settings_panel.is_blocking()) return false;

			open_account_search();
			return true;
		}

		case COMMA: {
			if (m_settings_panel.is_blocking()) return false;

			m_app_menu.close();
			m_settings_panel.open();
			return true;
		}

		default: {
			return false;
		}
	}
}

auto App::run_commands() -> void
{
	while (const std::optional<Command> command = m_commands.pop()) {
		run_command(*command);
	}
}

auto App::run_command(const Command& t_command) -> void
{
	animation::request_frame();

	switch (t_command.type) {
		using enum CommandType;

		case TOGGLE_APP_MENU: {
			if (m_app_menu.m_open) {
				m_app_menu.close();
			} else {
				m_update_overlay.close();
				m_app_menu.open(!m_locked, update_status(m_updater.stage()));
			}

			break;
		}

		case TOGGLE_UPDATE_OVERLAY: {
			if (m_update_overlay.m_open) {
				m_update_overlay.close();
			} else {
				m_update_overlay.open();
			}

			break;
		}

		case OPEN_UPDATE_OVERLAY: {
			m_update_overlay.open();
			break;
		}

		case OPEN_SETTINGS: {
			if (m_locked) break;

			// Setup can change whether Pulsar starts with the system, so the panel shows what is set now.
			m_settings.start_with_system = os::installation::starts_with_system();
			m_start_with_system          = m_settings.start_with_system;

			m_app_menu.close();
			m_settings_panel.open();
			break;
		}

		case OPEN_SETUP: {
			open_setup();
			break;
		}

		case OPEN_DATA_FOLDER: {
			const std::string directory = storage::data_directory();
			if (!directory.empty()) {
				os::open_path(directory);
			}

			break;
		}

		case OPEN_PERMISSION_SETTINGS: {
			RiotClient::open_automation_permission_settings();
			break;
		}

		case CHECK_FOR_UPDATES: {
			m_updater.check_for_update();
			m_update_overlay.begin_check();
			break;
		}

		case OPEN_GAME: {
			if (!t_command.game) break;

			m_account_modal.open(*t_command.game);
			m_account_modal.m_art_source = m_carousel.art_source(*t_command.game);
			break;
		}

		case SAVE_CHANGES: {
			request_save();
			break;
		}

		case REQUEST_NEW_MASTER_PASSWORD: {
			m_replaced_vault_key.emplace();
			m_replaced_vault_key->key.swap(&m_master_key);
			m_replaced_vault_key->params = m_settings.master_key;
			lock();
			m_unlock_screen.show_setup(true);
			break;
		}

		case VAULT_UNLOCKED: {
			wipe_locked_accounts(true);
			storage::load_accounts(&m_library, &m_master_key);
			unlock();
			break;
		}

		case VAULT_CREATED: {
			commit_new_vault_key();
			unlock();
			break;
		}

		case RESET_EVERYTHING: {
			reset_everything();
			break;
		}

		case SHOW_ACCOUNT_MENU: {
			open_account_menu(t_command);
			break;
		}

		case SHOW_TEXT_MENU: {
			open_text_menu(t_command);
			break;
		}

		case COPY_USERNAME: {
			if (const Account* account = m_account_modal.account_at_row(t_command.row)) {
				os::set_clipboard_text(account->username);
			}

			break;
		}

		case COPY_PASSWORD: {
			if (const Account* account = m_account_modal.account_at_row(t_command.row)) {
				copy_password(account->password);
			}

			break;
		}

		case EDIT_TEXT: {
			t_command.text_input->apply(t_command.text_edit);
			break;
		}

		case UNDO_DELETE: {
			m_account_modal.undo_delete();
			break;
		}

		case EDIT_ROW: {
			m_account_modal.edit_row(t_command.row);
			break;
		}

		case DELETE_ROW: {
			m_account_modal.delete_row(t_command.row);
			break;
		}

		case DELETE_PICKED_ROWS: {
			m_account_modal.delete_picked();
			break;
		}

		case LOCK_VAULT: {
			lock_vault();
			break;
		}

		case OPEN_ACCOUNT_SEARCH: {
			open_account_search();
			break;
		}

		case EDIT_ACCOUNT: {
			if (m_library.find_account(t_command.account) != nullptr) {
				m_settings_panel.close();
				m_account_modal.edit_account(t_command.account);
				lift_game_card(t_command.account.game);
			}

			break;
		}

		case LOGIN_ACCOUNT: {
			if (m_library.find_account(t_command.account) != nullptr && t_command.game) {
				m_settings_panel.close();
				m_account_modal.quick_login(*t_command.game, t_command.account);
				lift_game_card(*t_command.game);
			}

			break;
		}

		case UNDO_LIBRARY_DELETE: {
			m_carousel.m_library_view.undo_delete();
			break;
		}

		case EDIT_ACCOUNT_IN_PLACE: {
			if (m_library.find_account(t_command.account) != nullptr) {
				m_carousel.m_library_view.edit(t_command.account);
			}

			break;
		}

		case DELETE_ACCOUNT_IN_PLACE: {
			if (m_library.find_account(t_command.account) != nullptr) {
				m_carousel.m_library_view.delete_account(t_command.account);
			}

			break;
		}

		case DELETE_PICKED_IN_PLACE: {
			m_carousel.m_library_view.delete_picked();
			break;
		}

		case COPY_ACCOUNT_USERNAME: {
			if (const Account* account = m_library.find_account(t_command.account)) {
				os::set_clipboard_text(account->username);
				m_toasts.notify(Notification{.message = "Username copied."});
			}

			break;
		}

		case COPY_ACCOUNT_PASSWORD: {
			if (const Account* account = m_library.find_account(t_command.account)) {
				copy_password(account->password);
			}

			break;
		}

		case LOCATE_RIOT_CLIENT: {
			locate_riot_client(t_command);
			break;
		}
	}
}

auto App::open_account_menu(const Command& t_command) -> void
{
	if (t_command.picked > 1) {
		open_picked_menu(t_command);
		return;
	}

	if (!t_command.row) {
		open_library_account_menu(t_command);
		return;
	}

	if (m_account_modal.account_at_row(t_command.row) == nullptr) return;

	const std::optional<u32> row = t_command.row;
	const ContextMenuItem    items[]{
		{.label = "Edit", .command = Command{.type = CommandType::EDIT_ROW, .row = row}, .icon = Asset::ICON_EDIT_BOX},
		{.label = "Delete", .command = Command{.type = CommandType::DELETE_ROW, .row = row}, .icon = Asset::ICON_TRASH, .destructive = true},
		{.label = "Copy username", .command = Command{.type = CommandType::COPY_USERNAME, .row = row}, .icon = Asset::ICON_COPY, .separated = true},
		{.label = "Copy password", .command = Command{.type = CommandType::COPY_PASSWORD, .row = row}, .icon = Asset::ICON_KEY},
	};

	m_context_menu.open(t_command.position, items, m_window.size());
}

auto App::open_library_account_menu(const Command& t_command) -> void
{
	const Account* account = m_library.find_account(t_command.account);
	if (account == nullptr) return;

	const AccountRef      target = t_command.account;
	const Command         remove{.type = CommandType::DELETE_ACCOUNT_IN_PLACE, .account = target};
	const Command         copy_username{.type = CommandType::COPY_ACCOUNT_USERNAME, .account = target};
	const ContextMenuItem items[]{
		{.label = "Edit", .command = Command{.type = CommandType::EDIT_ACCOUNT_IN_PLACE, .account = target}, .icon = Asset::ICON_EDIT_BOX},
		{.label = "Delete", .command = remove, .icon = Asset::ICON_TRASH, .destructive = true},
		{.label = "Copy username", .command = copy_username, .icon = Asset::ICON_COPY, .separated = true},
		{.label = "Copy password", .command = Command{.type = CommandType::COPY_ACCOUNT_PASSWORD, .account = target}, .icon = Asset::ICON_KEY},
	};

	m_context_menu.open(t_command.position, items, m_window.size());
}

auto App::open_picked_menu(const Command& t_command) -> void
{
	const CommandType      remove = !t_command.row ? CommandType::DELETE_PICKED_IN_PLACE : CommandType::DELETE_PICKED_ROWS;
	const std::string_view label  = format_into(m_picked_label, "Delete {} accounts", t_command.picked);

	const ContextMenuItem items[]{
		{.label = label, .command = Command{.type = remove}, .icon = Asset::ICON_TRASH, .destructive = true},
	};

	m_context_menu.open(t_command.position, items, m_window.size());
}

auto App::open_text_menu(const Command& t_command) -> void
{
	TextInput* input = t_command.text_input;

	const auto item = [input](std::string_view t_label, TextEdit t_edit, std::string_view t_shortcut) {
		const Command edit{.type = CommandType::EDIT_TEXT, .text_input = input, .text_edit = t_edit};

		return ContextMenuItem{t_label, edit, input->can_apply(t_edit), t_shortcut};
	};

	const ContextMenuItem items[]{
		item("Cut", TextEdit::CUT, PULSAR_SHORTCUT_KEY "+X"),
		item("Copy", TextEdit::COPY, PULSAR_SHORTCUT_KEY "+C"),
		item("Paste", TextEdit::PASTE, PULSAR_SHORTCUT_KEY "+V"),
		item("Select all", TextEdit::SELECT_ALL, PULSAR_SHORTCUT_KEY "+A"),
	};

	m_context_menu.open(t_command.position, items, m_window.size());
}

auto App::announce_update_stage() -> void
{
	const UpdateStage stage = m_updater.stage();
	if (stage == m_announced_update_stage) return;

	m_announced_update_stage = stage;

	const Command open_updates{.type = CommandType::OPEN_UPDATE_OVERLAY};
	const bool    watching  = m_update_overlay.m_open || m_update_overlay.wants_status();
	const bool    available = stage == UpdateStage::AVAILABLE || stage == UpdateStage::MANUAL_UPGRADE_REQUIRED;
	if (watching) return;

	if (available) {
		char                   message[96];
		const std::string_view text = format_into(message, "Version {} available", std::string_view{m_updater.m_manifest.version});
		m_toasts.notify(Notification{.message = text, .icon = Asset::ICON_DOWNLOAD, .on_click = open_updates});
	} else if (stage == UpdateStage::UPDATE_FAILED) {
		m_toasts.notify(Notification{.message = "The update could not be installed.", .icon = Asset::ICON_UPDATE, .on_click = open_updates});
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

	char                   message[96];
	const std::string_view text = format_into(message, "Updated to {}", K_APP_VERSION);
	m_toasts.notify(Notification{.message = text, .icon = Asset::ICON_UPDATE});
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

	copy_to(m_updater.m_manifest.version, m_settings.release_notes_version);
	copy_to(m_updater.m_manifest.notes, m_settings.release_notes);
	save_everything();

	// The replacement build would otherwise find this process's mutex and exit as a duplicate.
	m_instance_guard.release();

	os::launch_process(os::executable_path());
	m_window.request_quit();
}

auto App::redraw_while_resizing() -> void
{
	if (m_window.m_physical_width == 0 || m_window.m_physical_height == 0) return;

	m_renderer.resize(&m_window);
	frame();
}

auto App::reload_fonts() -> bool
{
	const auto load = [this] { return m_fonts.load(&m_renderer, m_settings.font_name, m_settings.font_size, m_settings.secondary_font_size, m_window.m_dpi_scale); };

	if (load()) return true;

	copy_to(Settings{}.font_name, m_settings.font_name);

	return load();
}

auto App::recover_graphics() -> void
{
	if (!m_renderer.is_device_lost()) return;

	if (Clock::now() < m_graphics_retry_at || !m_renderer.recover(&m_window)) {
		m_graphics_retry_at = std::max(m_graphics_retry_at, Clock::now() + K_GRAPHICS_RETRY);
		animation::request_frame_after(std::chrono::duration<float>(K_GRAPHICS_RETRY).count());
		return;
	}

	m_assets.restore(&m_renderer);
	m_snowfall.create_texture(&m_renderer);
	std::ignore = reload_fonts();
	animation::request_frame();
}

auto App::frame() -> void
{
	if (m_in_frame) return;

	m_in_frame = true;
	PULSAR_PROFILE_FRAME_BEGIN();

	const auto  now           = std::chrono::steady_clock::now();
	const float delta_seconds = std::chrono::duration<float>(now - m_last_frame_time).count();
	m_last_frame_time         = now;
	m_intro                   = animation::step_toward(m_intro, 1.0f, K_INTRO_SECONDS, std::min(delta_seconds, K_INTRO_MAX_STEP));

	const Vec2 restored = m_window.restored_size();
	if (restored.x >= K_MIN_WINDOW_WIDTH && restored.y >= K_MIN_WINDOW_HEIGHT) {
		m_settings.window_width  = static_cast<u32>(std::lround(restored.x));
		m_settings.window_height = static_cast<u32>(std::lround(restored.y));
	}

	// The games stay drawn under the lock screen until it covers them, so locking fades over them instead of blanking first.
	m_carousel.m_visible = !m_locked || !m_unlock_screen.covers_window();
	m_carousel.m_bounds  = content_rect(m_window.size());
	set_pixel_scale(m_window.m_dpi_scale);
	set_caret_style(m_settings.caret_style, m_settings.caret_trail, m_settings.caret_trail_strength);
	controls::set_glass(m_renderer.supports_backdrop_blur(), m_settings.glass, m_settings.glass_tint, m_settings.glass_blur);
	update_theme(delta_seconds);

	{
		PULSAR_PROFILE_SCOPE("Widgets.Update");
		m_login_session.update(delta_seconds);
		update_login_toast();
		m_widgets.update(m_mouse, delta_seconds);
	}

	m_truncation_hint.update(delta_seconds, m_pointer_down);

	const std::optional<u32> detached_game = m_account_modal.detached_game();
	if (detached_game) {
		m_account_modal.m_art_source = m_carousel.art_source(*detached_game);
	}

	m_carousel.m_detached_game = detached_game;

	if (!m_window.m_mouse_over_resize_border) {
		m_window.set_cursor(m_widgets.cursor());
	}

	m_effect_seconds += delta_seconds;
	m_renderer.m_effect_time_seconds = m_effect_seconds;

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
	const Rect title_seam{0.0f, K_TITLE_BAR_HEIGHT, window.x, 1.0f};
	const Rect status_seam{0.0f, status_bar.y - 1.0f, window.x, 1.0f};

	m_draw_list.add_rect(title_seam, g_theme.chrome_seam);
	m_draw_list.add_rect(status_seam, g_theme.chrome_seam);
	m_draw_list.add_rect(status_bar, g_theme.chrome);

	const Font& font   = m_fonts.secondary;
	const float mark_y = status_bar.y + (K_STATUS_BAR_HEIGHT - K_STATUS_MARK_SIZE) * 0.5f;
	const Rect  mark{K_STATUS_PADDING, mark_y, K_STATUS_MARK_SIZE, K_STATUS_MARK_SIZE};
	const float baseline        = font.centered_baseline(status_bar) - K_STATUS_BASELINE_NUDGE;
	const bool  protected_vault = m_settings.master_password_enabled;

	controls::draw_lock(&m_draw_list, mark, g_theme.text_dim, g_theme.chrome, !m_locked);

	std::string_view state = "Vault unlocked";
	if (m_locked) {
		state = "Vault locked";
	} else if (!protected_vault) {
		state = "No master password";
	}

	const Vec2 state_baseline{mark.right() + K_STATUS_MARK_GAP, baseline};
	draw_text(&m_draw_list, font, state_baseline, state, g_theme.text_dim);

	if (!m_locked && protected_vault && m_settings.auto_lock_minutes != 0) {
		const float state_end = state_baseline.x + text_width(font, state);
		draw_auto_lock_countdown(state_end, baseline);
	}

	const bool panel_open = m_account_modal.is_blocking() || m_settings_panel.is_blocking();
	if (m_carousel.m_visible && !panel_open) {
		m_carousel.draw_status_bar(&m_draw_list);
	}
}

auto App::draw_auto_lock_countdown(float t_x, float t_baseline) -> void
{
	using std::chrono::seconds;

	const Font& font      = m_fonts.secondary;
	const auto  limit     = std::chrono::minutes(m_settings.auto_lock_minutes);
	const auto  idle      = std::chrono::duration_cast<seconds>(Clock::now() - m_last_activity);
	const auto  remaining = std::max(seconds(1), limit - idle);
	const auto  minutes   = static_cast<u32>((remaining.count() + 59) / 60);

	char                   countdown[48];
	const std::string_view text  = format_into(countdown, "auto-locks in {} min", minutes);
	const float            dot_y = t_baseline - font.ascent * 0.35f - K_STATUS_DOT_SIZE * 0.5f;
	const Rect             dot{t_x + K_STATUS_DOT_GAP, dot_y, K_STATUS_DOT_SIZE, K_STATUS_DOT_SIZE};
	const Vec2             text_baseline{dot.right() + K_STATUS_DOT_GAP, t_baseline};
	const auto             next_minute_in = remaining - std::chrono::minutes(minutes - 1);
	const float            redraw_after   = std::chrono::duration<float>(next_minute_in).count() + 0.05f;

	m_draw_list.add_rounded_rect(dot, rounded(K_STATUS_DOT_SIZE * 0.5f), g_theme.text_faint);
	draw_text(&m_draw_list, font, text_baseline, text, g_theme.text_faint);
	animation::request_frame_after(redraw_after);
}

auto App::render() -> void
{
	PULSAR_PROFILE_SCOPE("Render");

	m_draw_list.clear();

	const Vec2  window = m_window.size();
	const Rect  whole_window{0.0f, 0.0f, window.x, window.y};
	const Color backdrop = g_theme.window;
	m_draw_list.add_backdrop(whole_window, backdrop, backdrop, backdrop, backdrop);

	const float intro       = 1.0f - std::pow(1.0f - m_intro, 3.0f);
	const bool  in_intro    = intro < 1.0f;
	const float intro_scale = lerp(K_INTRO_START_SCALE, 1.0f, intro);
	if (in_intro) {
		m_draw_list.push_scale(whole_window.center(), intro_scale);
	}

	begin_truncation_probe(&m_draw_list, m_mouse);
	draw_status_bar();
	m_snowfall.draw(&m_draw_list);

	{
		PULSAR_PROFILE_SCOPE("Render.BuildGeometry");
		m_widgets.draw(&m_draw_list);
		m_truncation_hint.capture(&m_draw_list);
		m_truncation_hint.draw(&m_draw_list, content_rect(m_window.size()));

		if (in_intro) {
			const Color cover = faded(backdrop, to_alpha(1.0f - intro));

			m_draw_list.pop_scale();
			m_draw_list.add_rect(whole_window, cover);
		}

		m_draw_list.finish();
	}

	if (m_draw_list.m_has_animated_effects) {
		animation::request_frame();
	}

	m_renderer.m_backdrop = Backdrop{.style = m_settings.background_style, .intensity = m_settings.background_intensity, .light = m_settings.background_light ? m_settings.background_light_intensity : 0.0f, .grain = m_settings.background_grain ? m_settings.background_grain_intensity : 0.0f};
	m_renderer.render(&m_draw_list, g_theme.window);
}

auto App::run() -> void
{
	while (!m_window.m_should_quit) {
		pump_input();
		announce_first_run_after_update();
		announce_unreadable_storage();
		frame();
		recover_graphics();

		m_updater.update();
		announce_update_stage();
		run_commands();
		take_picked_riot_client();
		relaunch_if_update_installed();
		save_if_due();
		lock_if_idle();
		wipe_locked_accounts(m_window.is_hidden() || m_window.is_minimized());
		if (m_clipboard_secret && Clock::now() >= m_clipboard_secret->clear_at) {
			clear_clipboard_secret();
		}

		m_window.set_excluded_from_capture(m_settings.hide_from_capture);

		if (m_settings.start_with_system != m_start_with_system) {
			m_start_with_system = m_settings.start_with_system;
			os::installation::set_starts_with_system(m_start_with_system);
		}

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
