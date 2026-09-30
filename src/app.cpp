#include "app.h"

#include <cstdio>
#include <utility>
#include <print>

#include <Windows.h>
#include <shellapi.h>
#include <sodium.h>

#include "core/animation.h"
#include "core/app_identity.h"
#include "core/debug_log.h"
#include "core/profiler.h"
#include "core/str.h"
#include "core/ui_automation.h"
#include "platform/clipboard.h"
#include "platform/overlay_guard.h"
#include "ui/text.h"
#include "ui/theme.h"

namespace {
constexpr u32 draw_list_vertex_capacity = 1 << 16;
constexpr u32 draw_list_index_capacity = (1 << 16) * 3 / 2;
constexpr auto save_delay = std::chrono::milliseconds(500);
constexpr float idle_poll_seconds = 0.25f;
constexpr auto resume_frame_time = std::chrono::microseconds(16667);
constexpr auto clipboard_secret_lifetime = std::chrono::seconds(30);

constexpr float status_padding = 14.0f;
constexpr float status_mark_size = 15.0f;
constexpr float status_mark_gap = 7.0f;
constexpr float status_baseline_nudge = 2.0f;

struct GameInfo {
	const char *title;
	const char *short_title;
	Asset banner;
	Asset icon;
	Color accent;
};

constexpr GameInfo games[]{
	{"League of Legends", "LoL", Asset::banner_league_of_legends, Asset::icon_league_of_legends, {210, 175, 55, 255}},
	{"Teamfight Tactics", "TFT", Asset::banner_teamfight_tactics, Asset::icon_teamfight_tactics, {70, 140, 190, 255}},
	{"Valorant", "Valorant", Asset::banner_valorant, Asset::icon_valorant, {210, 55, 60, 255}},
	{"2XKO", "2XKO", Asset::banner_two_xko, Asset::icon_two_xko, {45, 205, 210, 255}},
	{"Legends of Runeterra", "LoR", Asset::banner_runeterra, Asset::icon_runeterra, {140, 90, 200, 255}},
};

static_assert(std::size(games) <= max_games);
static_assert(tray_max_games >= max_games);

void log_startup_phase(const char *t_phase)
{
	using Clock = std::chrono::steady_clock;

	static const Clock::time_point process_start = Clock::now();
	static Clock::time_point phase_start = process_start;
	static const char *previous_phase = nullptr;

	const Clock::time_point now = Clock::now();

	if (previous_phase != nullptr) {
		debug_log::write("startup", "%-20s %6.1f ms   (%6.1f ms in)", previous_phase,
						 std::chrono::duration<float, std::milli>(now - phase_start).count(),
						 std::chrono::duration<float, std::milli>(now - process_start).count());
	}

	previous_phase = t_phase;
	phase_start = now;
}

void guard_against_overlays(bool t_block_injection)
{
	overlay_guard::apply_process_identity();

	if (!t_block_injection) {
		debug_log::write("app", "overlay injection guard disabled by setting");
		return;
	}

	switch (overlay_guard::block_hook_injection()) {
		case overlay_guard::BlockResult::blocked:
			debug_log::write("app", "extension-point DLL injection blocked");
			break;
		case overlay_guard::BlockResult::refused:
			debug_log::write("app", "extension-point block refused, err=%lu", GetLastError());
			break;
		case overlay_guard::BlockResult::unsupported:
			debug_log::write("app", "extension-point block unsupported on this Windows build");
			break;
	}

	if (const wchar_t *module = overlay_guard::injected_overlay_module()) {
		debug_log::write("app", "an overlay module was already loaded before the guard ran: %ls", module);
	}
}

void launch_self()
{
	wchar_t path[MAX_PATH];
	if (GetModuleFileNameW(nullptr, path, MAX_PATH) == 0) return;

	STARTUPINFOW startup_info{.cb = sizeof(startup_info)};
	PROCESS_INFORMATION process_info{};

	if (CreateProcessW(path, nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup_info, &process_info)) {
		CloseHandle(process_info.hProcess);
		CloseHandle(process_info.hThread);
	}
}
}

App::App()
	: m_carousel(m_library, m_settings, m_fonts, m_assets, m_commands)
	, m_toasts(m_settings, m_fonts, m_assets, m_window, m_commands)
	, m_account_modal(m_library, m_settings, m_fonts, m_assets, m_window, m_toasts, m_commands)
	, m_settings_panel(m_settings, m_fonts, m_renderer, m_window, m_assets, m_commands)
	, m_unlock_screen(m_settings, m_master_key, m_fonts, m_assets, m_window, m_commands)
	, m_app_menu(m_settings, m_fonts, m_assets, m_commands)
	, m_update_overlay(m_updater, m_settings, m_fonts, m_window)
	, m_context_menu(m_settings, m_fonts, m_commands)
	, m_title_bar(m_window, m_updater, m_fonts, m_assets, m_commands)
	, m_truncation_hint(m_fonts)
#ifdef PULSAR_PROFILING
	, m_profiler_overlay(m_fonts)
#endif
{
}

App::StartResult App::start()
{
	if (!m_instance_guard.is_first_instance()) {
		const bool activated = Window::activate_existing_instance();
		debug_log::write("app", "another instance is already running (%s) - exiting",
						 activated ? "brought it to the front" : "it never answered");

		return StartResult::already_running;
	}

	m_assets.begin_decode();

	log_startup_phase("LoadSettings");
	const storage::LoadResult settings_result = storage::load_settings(m_settings);

	log_startup_phase("GuardAgainstOverlays");
	guard_against_overlays(m_settings.block_overlay_injection);
	UiAutomation::keep_process_mta_alive();

	if (sodium_init() < 0) {
		std::println("Failed to initialize libsodium.");
		return StartResult::failed;
	}

	log_startup_phase("CreateGraphics");
	if (!create_graphics()) return StartResult::failed;

	log_startup_phase("CreateTray");
	m_tray.create(app_name_wide);
	m_draw_list.init(draw_list_vertex_capacity, draw_list_index_capacity);

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

	m_start_time = std::chrono::steady_clock::now();
	m_last_frame_time = m_start_time;
	m_last_activity = m_start_time;

	log_startup_phase("FirstFrame");
	frame();
	m_window.show();
	log_startup_phase("Done");

	return StartResult::ok;
}

bool App::create_graphics()
{
	if (!m_window.create(app_name_wide, m_settings.window_width, m_settings.window_height)) {
		std::println("Failed to create window.");
		return false;
	}

	if (!m_renderer.init(m_window)) {
		std::println("Failed to initialize renderer.");
		return false;
	}

	m_swap_chain_width = m_window.physical_width();
	m_swap_chain_height = m_window.physical_height();

	if (!m_assets.finish_upload(m_renderer)) {
		std::println("Failed to load one or more embedded assets.");
		return false;
	}

	if (!reload_fonts()) {
		std::println("Failed to load the UI font.");
		return false;
	}

	return true;
}

void App::add_games()
{
	for (u32 i = 0; i < std::size(games); i += 1) {
		const GameInfo &game = games[i];

		m_library.add_game(game.title, game.short_title, m_assets.get(game.banner), m_assets.get(game.icon),
						   game.accent);
		m_tray.set_game_icon(i, Assets::encoded_bytes(game.icon));
	}

	m_tray.on_menu_open([this](TrayMenu &t_menu) { fill_tray_menu(t_menu); });
}

void App::stack_widgets()
{
	m_widgets.push(m_carousel);
	m_widgets.push(m_account_modal);
	m_widgets.push(m_settings_panel);
	m_widgets.push(m_unlock_screen);
	m_widgets.push(m_app_menu);
	m_widgets.push(m_update_overlay);
	m_widgets.push(m_context_menu);

	m_widgets.push_overlay(m_toasts);
	m_widgets.push_overlay(m_title_bar);
#ifdef PULSAR_PROFILING
	m_widgets.push_overlay(m_profiler_overlay);
#endif
}

void App::apply_settings(storage::LoadResult t_load_result)
{
	apply_theme(m_settings.theme);
	apply_game_order();
	m_carousel.restore(m_settings.zoom_stop, m_settings.selected_game);

	if (t_load_result == storage::LoadResult::failed) return;

	animation::set_enabled(m_settings.animations_enabled);
	animation::set_speed(m_settings.animation_speed);
	set_corner_roundness(m_settings.corner_roundness);
	m_settings_panel.sync_with_settings();

	const std::string_view last_run_version = m_settings.last_run_version;
	m_just_updated = !last_run_version.empty() && last_run_version != app_version;
	copy_to(app_version, m_settings.last_run_version);
}

void App::lock()
{
	m_locked = true;
	m_carousel.set_visible(false);
	m_tray.set_locked(true);
}

void App::unlock()
{
	m_locked = false;
	m_carousel.set_visible(true);
	m_unlock_screen.hide();
	m_tray.set_locked(false);
	m_last_activity = Clock::now();
}

void App::lock_vault()
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

void App::lock_if_idle()
{
	if (m_locked || m_settings.auto_lock_minutes == 0) return;
	if (Clock::now() - m_last_activity < std::chrono::minutes(m_settings.auto_lock_minutes)) return;

	lock_vault();
}

void App::apply_game_order()
{
	u8 order[max_games]{};
	u32 count = 0;

	for (u32 i = 0; i < m_settings.game_order_count; i += 1) {
		for (u32 game = 0; game < m_library.game_count(); game += 1) {
			if (m_library.game(game).title == m_settings.game_order[i]) {
				order[count] = static_cast<u8>(game);
				count += 1;
				break;
			}
		}
	}

	m_carousel.set_order({order, count});
}

void App::save_settings()
{
	m_settings.zoom_stop = m_carousel.zoom_stop();
	m_settings.selected_game = m_carousel.selected_game();

	m_settings.game_order_count = 0;
	for (const u8 game : m_carousel.order()) {
		copy_to(m_library.game(game).title, m_settings.game_order[m_settings.game_order_count]);
		m_settings.game_order_count += 1;
	}

	Settings committed = m_settings;
	m_settings_panel.restore_committed_previews(committed);
	storage::save_settings(committed);
}

void App::save_everything()
{
	storage::save_accounts(m_library, m_master_key);
	save_settings();
}

void App::request_save()
{
	m_save_due = Clock::now() + save_delay;
}

void App::save_if_due()
{
	if (!m_save_due || Clock::now() < *m_save_due) return;

	m_save_due.reset();
	save_everything();
}

void App::commit_new_vault_key()
{
	if (storage::save_accounts(m_library, m_master_key)) {
		save_settings();
	} else if (m_replaced_vault_key) {
		m_master_key.swap(m_replaced_vault_key->key);
		m_settings.master_key = m_replaced_vault_key->params;
		m_toasts.notify(Notification{.message = "Your vault could not be saved, so the password was not changed."});
	}

	m_replaced_vault_key.reset();
}

void App::copy_password(std::string_view t_password)
{
	set_clipboard_secret(t_password);
	m_clipboard_secret = ClipboardSecret{clipboard_sequence(), Clock::now() + clipboard_secret_lifetime};

	constexpr auto lifetime_seconds = std::chrono::duration<float>(clipboard_secret_lifetime).count();
	m_toasts.notify_countdown("Password copied - it clears itself in 30 seconds.", lifetime_seconds);
}

void App::clear_clipboard_secret()
{
	if (!m_clipboard_secret) return;

	clear_clipboard_if_unchanged(m_clipboard_secret->sequence);
	m_clipboard_secret.reset();
}

void App::fill_tray_menu(TrayMenu &t_menu) const
{
	if (m_locked) return;

	for (const u8 game : m_carousel.order()) {
		if (t_menu.game_count == tray_max_games) break;

		const VisibleAccounts visible = m_library.visible_accounts(game);

		TrayGame &entry = t_menu.games[t_menu.game_count];
		t_menu.game_count += 1;

		copy_to(m_library.game(game).title, entry.title);
		entry.game = static_cast<i32>(game);
		entry.first_account = t_menu.account_count;
		entry.account_count = 0;

		for (u32 row = 0; row < visible.count && t_menu.account_count < tray_max_accounts; row += 1) {
			const Account &account = m_library.account(visible.refs[row]);
			const std::string_view note = account.note;

			TrayAccount &item = t_menu.accounts[t_menu.account_count];
			copy_to(note.empty() ? std::string_view{account.username} : note, item.label);
			item.game = static_cast<i32>(game);
			item.row = static_cast<i32>(row);

			t_menu.account_count += 1;
			entry.account_count += 1;
		}
	}
}

void App::pump_input()
{
	PULSAR_PROFILE_SCOPE("Input");

	m_window.pump_messages();
	m_window.set_close_to_tray(m_settings.close_to_tray && m_tray.is_icon_visible());
	m_tray.set_colors(TrayColors{
		.background = theme().popup,
		.hover = mix(theme().popup, m_settings.accent, 0.42f),
		.text = theme().text,
		.text_disabled = theme().text_faint,
		.separator = theme().separator,
	});

	handle_tray_event();

	for (const InputEvent &event : m_window.input_events()) {
		handle_input(event);
	}
}

void App::handle_tray_event()
{
	const TrayEvent event = m_tray.take_event();
	if (event.type != TrayEventType::none) {
		m_last_activity = Clock::now();
	}

	switch (event.type) {
		case TrayEventType::exit:
			m_window.request_close();
			break;

		case TrayEventType::show_window:
			m_window.restore();
			break;

		case TrayEventType::quick_login: {
			if (m_locked || event.game < 0 || event.row < 0) break;

			const auto game = static_cast<u32>(event.game);
			if (const auto account = m_library.visible_account(game, static_cast<u32>(event.row))) {
				m_account_modal.quick_login(game, *account);
			}

			break;
		}

		case TrayEventType::none:
			break;
	}
}

void App::handle_input(const InputEvent &t_event)
{
	m_last_activity = Clock::now();

	const bool control_down = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
	if (t_event.type == InputEventType::key_down && t_event.key == 'L' && control_down && !m_locked) {
		lock_vault();
		return;
	}

	if (t_event.type == InputEventType::mouse_move) {
		m_mouse = t_event.position;
	} else if (t_event.type == InputEventType::mouse_down) {
		m_pointer_down = true;
	} else if (t_event.type == InputEventType::mouse_up) {
		m_pointer_down = false;
	}

	const bool consumed = m_widgets.dispatch(t_event);
	process_commands();

	const bool is_action = t_event.type == InputEventType::mouse_up || t_event.type == InputEventType::key_down ||
						   t_event.type == InputEventType::right_click;
	if (consumed && is_action) {
		request_save();
	}
}

void App::process_commands()
{
	while (const std::optional<Command> command = m_commands.pop()) {
		process(*command);
	}
}

void App::process(const Command &t_command)
{
	animation::request_frame();

	switch (t_command.type) {
		case CommandType::toggle_app_menu:
			if (m_app_menu.is_open()) {
				m_app_menu.close();
			} else {
				m_app_menu.open(!m_locked);
			}

			break;

		case CommandType::toggle_update_overlay:
			if (m_update_overlay.is_open()) {
				m_update_overlay.close();
			} else {
				m_update_overlay.open();
			}

			break;

		case CommandType::open_update_overlay:
			m_update_overlay.open();
			break;

		case CommandType::open_settings:
			m_settings_panel.open();
			break;

		case CommandType::open_data_folder: {
			const std::string directory = storage::data_directory();
			if (!directory.empty()) {
				ShellExecuteA(nullptr, "open", directory.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
			}

			break;
		}

		case CommandType::check_for_updates:
			m_updater.check_for_update();
			m_update_overlay.open();
			break;

		case CommandType::open_game:
			m_account_modal.open(t_command.index);
			m_account_modal.set_art_source(m_carousel.art_source(static_cast<u32>(t_command.index)));
			break;

		case CommandType::save_changes:
			request_save();
			break;

		case CommandType::request_new_master_password:
			m_replaced_vault_key.emplace();
			m_replaced_vault_key->key.swap(m_master_key);
			m_replaced_vault_key->params = m_settings.master_key;
			lock();
			m_unlock_screen.show_setup();
			break;

		case CommandType::vault_unlocked:
			storage::load_accounts(m_library, m_master_key);
			unlock();
			break;

		case CommandType::vault_created:
			commit_new_vault_key();
			unlock();
			break;

		case CommandType::show_account_menu:
			open_account_menu(t_command);
			break;

		case CommandType::show_text_menu:
			open_text_menu(t_command);
			break;

		case CommandType::copy_username:
			if (const Account *account = m_account_modal.account_at_row(t_command.index)) {
				set_clipboard_text(account->username);
			}

			break;

		case CommandType::copy_password:
			if (const Account *account = m_account_modal.account_at_row(t_command.index)) {
				copy_password(account->password);
			}

			break;

		case CommandType::edit_text:
			t_command.text_input->apply(t_command.text_edit);
			break;

		case CommandType::undo_delete:
			m_account_modal.undo_delete();
			break;

		case CommandType::toggle_favorite:
			m_account_modal.toggle_favorite(t_command.index);
			break;

		case CommandType::lock_vault:
			lock_vault();
			break;
	}
}

void App::open_account_menu(const Command &t_command)
{
	const Account *account = m_account_modal.account_at_row(t_command.index);
	if (account == nullptr) return;

	const ContextMenuItem items[]{
		{account->favorite ? "Unpin" : "Pin to Top",
		 Command{.type = CommandType::toggle_favorite, .index = t_command.index}},
		{"Copy Username", Command{.type = CommandType::copy_username, .index = t_command.index}},
		{"Copy Password", Command{.type = CommandType::copy_password, .index = t_command.index}},
	};

	m_context_menu.open(t_command.position, items, m_window.size());
}

void App::open_text_menu(const Command &t_command)
{
	TextInput &input = *t_command.text_input;

	const auto item = [&input](std::string_view t_label, TextEdit t_edit) {
		const Command edit{.type = CommandType::edit_text, .text_input = &input, .text_edit = t_edit};

		return ContextMenuItem{t_label, edit, input.can_apply(t_edit)};
	};

	const ContextMenuItem items[]{
		item("Cut", TextEdit::cut),
		item("Copy", TextEdit::copy),
		item("Paste", TextEdit::paste),
		item("Select All", TextEdit::select_all),
	};

	m_context_menu.open(t_command.position, items, m_window.size());
}

void App::announce_update_stage()
{
	const UpdateStage stage = m_updater.stage();
	if (stage == m_announced_update_stage) return;

	m_announced_update_stage = stage;

	const Command open_updates{.type = CommandType::open_update_overlay};

	if (stage == UpdateStage::available || stage == UpdateStage::manual_upgrade_required) {
		char message[96];
		std::snprintf(message, sizeof(message), "Version %s available", m_updater.manifest().version);
		m_toasts.notify(
			Notification{.message = message, .icon = Asset::icon_update, .spin_icon = true, .on_click = open_updates});
	} else if (stage == UpdateStage::error) {
		m_toasts.notify(Notification{
			.message = "The update could not be installed.", .icon = Asset::icon_update, .on_click = open_updates});
	}
}

void App::announce_first_run_after_update()
{
	if (m_locked || !std::exchange(m_just_updated, false)) return;

	const std::string_view notes_version = m_settings.release_notes_version;
	const std::string_view notes = m_settings.release_notes;

	if (notes_version == app_version && !notes.empty()) {
		m_update_overlay.show_release_notes(notes_version, notes);
		m_settings.release_notes_version[0] = '\0';
		m_settings.release_notes[0] = '\0';
		request_save();
		return;
	}

	char message[96];
	std::snprintf(message, sizeof(message), "Updated to %s", app_version);
	m_toasts.notify(Notification{.message = message, .icon = Asset::icon_update});
}

void App::announce_unreadable_storage()
{
	if (m_unreadable_storage_announced || (storage::can_save_settings() && storage::can_save_accounts())) return;

	m_unreadable_storage_announced = true;
	m_toasts.notify(Notification{.message = "Saved data could not be read - changes will not be kept"});
}

void App::relaunch_if_update_installed()
{
	if (!m_updater.consume_ready_to_relaunch()) return;

	copy_to(m_updater.manifest().version, m_settings.release_notes_version);
	copy_to(m_updater.manifest().notes, m_settings.release_notes);
	save_everything();

	// The replacement build would otherwise find this process's mutex and exit as a duplicate.
	m_instance_guard.release();

	launch_self();
	m_window.request_close();
}

void App::redraw_while_resizing()
{
	if (m_window.physical_width() == 0 || m_window.physical_height() == 0) return;

	if (m_window.physical_width() != m_swap_chain_width || m_window.physical_height() != m_swap_chain_height) {
		m_renderer.resize(m_window);
		m_swap_chain_width = m_window.physical_width();
		m_swap_chain_height = m_window.physical_height();
	}

	frame();
}

bool App::reload_fonts()
{
	const auto load = [this] {
		return m_fonts.load(m_renderer, m_settings.font_name, m_settings.font_size, m_settings.secondary_font_size,
							m_window.dpi_scale());
	};

	if (load()) return true;

	copy_to(Settings{}.font_name, m_settings.font_name);

	return load();
}

void App::frame()
{
	if (m_in_frame) return;

	m_in_frame = true;
	PULSAR_PROFILE_FRAME_BEGIN();

	const auto now = std::chrono::steady_clock::now();
	const float delta_seconds = std::chrono::duration<float>(now - m_last_frame_time).count();
	m_last_frame_time = now;

	const Vec2 window = m_window.size();
	if (m_window.width() > 0 && m_window.height() > 0) {
		m_settings.window_width = m_window.width();
		m_settings.window_height = m_window.height();
	}

	m_carousel.set_bounds(m_window.content_rect());
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

	if (!m_window.is_minimized() && !m_window.is_hidden()) {
		render();
	}

	PULSAR_PROFILE_FRAME_END();
	m_in_frame = false;
}

void App::draw_status_bar()
{
	const Vec2 window = m_window.size();
	const Rect status_bar{0.0f, window.y - status_bar_height, window.x, status_bar_height};

	m_draw_list.add_rect(Rect{0.0f, title_bar_height, window.x, 1.0f}, theme().chrome_seam);
	m_draw_list.add_rect(Rect{0.0f, status_bar.y - 1.0f, window.x, 1.0f}, theme().chrome_seam);
	m_draw_list.add_rect(status_bar, theme().chrome);

	const Rect mark{status_padding, status_bar.y + (status_bar_height - status_mark_size) * 0.5f, status_mark_size,
					status_mark_size};
	m_draw_list.add_image(mark, m_assets.get(Asset::icon_app), theme().text_dim);

	char version[48];
	const int written =
		std::snprintf(version, sizeof(version), "%s v%s%s", app_name, app_version, is_debug_build ? " [dev]" : "");
	const Font &font = m_fonts.secondary();

	draw_text(m_draw_list, font,
			  Vec2{mark.right() + status_mark_gap, font.centered_baseline(status_bar) - status_baseline_nudge},
			  std::string_view{version, static_cast<usize>(std::max(written, 0))}, theme().text_dim);

	const bool panel_open = m_account_modal.is_blocking() || m_settings_panel.is_blocking();
	if (m_carousel.is_visible() && !panel_open) {
		m_carousel.draw_status_bar(m_draw_list);
	}
}

void App::render()
{
	PULSAR_PROFILE_SCOPE("Render");

	m_draw_list.clear();

	const Vec2 window = m_window.size();
	const Color backdrop = theme().window;
	m_draw_list.add_backdrop(Rect{0.0f, 0.0f, window.x, window.y}, backdrop, backdrop, backdrop, backdrop);

	begin_truncation_probe(m_draw_list, m_mouse);
	draw_status_bar();

	{
		PULSAR_PROFILE_SCOPE("Render.BuildGeometry");
		m_widgets.draw(m_draw_list);
		m_truncation_hint.capture(m_draw_list);
		m_truncation_hint.draw(m_draw_list, m_window.content_rect());
		m_draw_list.finish();
	}

	if (m_draw_list.has_animated_effects()) {
		animation::request_frame();
	}

	m_renderer.set_backdrop(static_cast<u32>(m_settings.background_style), m_settings.background_intensity,
							m_settings.background_light ? m_settings.background_light_intensity : 0.0f,
							m_settings.background_grain ? m_settings.background_grain_intensity : 0.0f);
	m_renderer.render(m_draw_list, theme().window);
}

void App::run()
{
	while (!m_window.should_close()) {
		pump_input();
		announce_first_run_after_update();
		announce_unreadable_storage();
		frame();

		m_updater.update();
		announce_update_stage();
		process_commands();
		relaunch_if_update_installed();
		save_if_due();
		lock_if_idle();
		if (m_clipboard_secret && Clock::now() >= m_clipboard_secret->clear_at) {
			clear_clipboard_secret();
		}

		m_window.set_excluded_from_capture(m_settings.hide_accounts_from_capture && m_account_modal.is_blocking());

		const float requested_wait = animation::take_idle_wait(idle_poll_seconds);
		const float wait = m_window.is_hidden() || m_window.is_minimized() ? idle_poll_seconds : requested_wait;

		if (wait > 0.0f && m_window.wait_for_messages(wait)) {
			m_last_frame_time = std::chrono::steady_clock::now() - resume_frame_time;
		}

		debug_log::mark_ui_thread_alive();
	}

	save_everything();
	clear_clipboard_secret();
}
