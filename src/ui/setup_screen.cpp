#include "ui/setup_screen.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <span>
#include <utility>

#include <Windows.h>

#include "core/animation.h"
#include "core/settings.h"
#include "core/storage.h"
#include "core/str.h"
#include "gfx/assets.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "platform/process.h"
#include "platform/window.h"
#include "ui/controls.h"
#include "ui/text.h"
#include "ui/theme.h"

namespace {
constexpr float content_inset = 36.0f;
constexpr float footer_height = 64.0f;
constexpr float footer_button_height = 34.0f;
constexpr float footer_button_min_width = 96.0f;
constexpr float button_padding = 20.0f;
constexpr float header_icon_size = 56.0f;
constexpr float header_gap = 16.0f;
constexpr float header_rows_gap = 30.0f;
constexpr float manage_top_gap = 20.0f;
constexpr float installed_gap = 16.0f;
constexpr float choice_height = 60.0f;
constexpr float choice_gap = 10.0f;
constexpr float choice_radius = 10.0f;
constexpr float choice_tile_size = 34.0f;
constexpr float choice_tile_radius = 9.0f;
constexpr float choice_icon_size = 18.0f;
constexpr float choice_padding = 13.0f;
constexpr float choice_line_gap = 2.0f;
constexpr float choice_border_accent = 0.55f;
constexpr Vec2 chevron_size{5.0f, 9.0f};
constexpr float chevron_nudge = 2.0f;
constexpr float chevron_thickness = 1.5f;
constexpr float heading_top_gap = 16.0f;
constexpr float caption_gap = 2.0f;
constexpr float field_gap = 12.0f;
constexpr float field_height = 38.0f;
constexpr float field_radius = 8.0f;
constexpr float field_padding = 12.0f;
constexpr float field_focus_border = 1.5f;
constexpr float browse_size = 28.0f;
constexpr float browse_inset = 5.0f;
constexpr float browse_radius = 6.0f;
constexpr float browse_icon_size = 16.0f;
constexpr float error_gap = 5.0f;
constexpr float options_gap = 24.0f;
constexpr float option_height = 28.0f;
constexpr float check_size = 16.0f;
constexpr float check_radius = 4.0f;
constexpr float check_border = 1.5f;
constexpr float check_inset = 2.0f;
constexpr float check_grow = 0.4f;
constexpr float check_label_gap = 10.0f;
constexpr float check_hover_accent = 0.6f;
constexpr float option_reach = 6.0f;
constexpr float body_gap = 14.0f;
constexpr float body_line_gap = 4.0f;
constexpr float status_icon_size = 52.0f;
constexpr float status_icon_gap = 18.0f;
constexpr float status_text_gap = 12.0f;
constexpr float status_bias = 6.0f;
constexpr float uninstalled_icon_dim = 0.5f;
constexpr float badge_size = 20.0f;
constexpr float badge_ring = 2.0f;
constexpr float badge_offset = 4.0f;
constexpr float badge_pop = 0.4f;
constexpr float progress_width = 240.0f;
constexpr float progress_height = 6.0f;
constexpr float progress_running_cap = 0.96f;
constexpr u32 max_message_lines = 3;

constexpr float transition_seconds = 0.32f;
constexpr float slide_distance = 28.0f;
constexpr float stagger_seconds = 0.055f;
constexpr float reveal_seconds = 0.34f;
constexpr float reveal_rise = 8.0f;
constexpr float reveal_total_seconds = 0.8f;
constexpr float settled_page_seconds = 10.0f;
constexpr float hover_ease_rate = 18.0f;
constexpr float check_ease_rate = 20.0f;
constexpr float progress_ease_rate = 7.0f;
constexpr float job_poll_seconds = 0.05f;
constexpr float picker_poll_seconds = 0.1f;
constexpr float done_hold_seconds = 0.25f;
constexpr float progress_done = 0.995f;

constexpr std::string_view app_title = "Pulsar";
constexpr std::string_view option_labels[]{"Desktop shortcut", "Start menu shortcut", "Start with Windows"};
constexpr std::string_view delete_data_label = "Also delete my accounts and settings";

float ease_out(float t_amount)
{
	const float inverse = 1.0f - t_amount;

	return 1.0f - inverse * inverse * inverse;
}

bool option_of(const installation::Options &t_options, u32 t_option)
{
	switch (t_option) {
		case 0:
			return t_options.desktop_shortcut;
		case 1:
			return t_options.start_menu_shortcut;
		default:
			return t_options.start_with_windows;
	}
}

void toggle_option(installation::Options *t_options, u32 t_option)
{
	switch (t_option) {
		case 0:
			t_options->desktop_shortcut = !t_options->desktop_shortcut;
			break;
		case 1:
			t_options->start_menu_shortcut = !t_options->start_menu_shortcut;
			break;
		default:
			t_options->start_with_windows = !t_options->start_with_windows;
			break;
	}
}

bool is_absolute(std::string_view t_path)
{
	const bool drive = t_path.size() >= 3 && ((t_path[0] >= 'A' && t_path[0] <= 'Z') || (t_path[0] >= 'a' && t_path[0] <= 'z')) && t_path[1] == ':' &&
					   (t_path[2] == '\\' || t_path[2] == '/');

	return drive || t_path.starts_with("\\\\");
}

std::wstring trimmed_folder(std::string_view t_path)
{
	std::wstring folder = to_wide(t_path);
	while (folder.size() > 3 && (folder.back() == L'\\' || folder.back() == L'/' || folder.back() == L' ')) {
		folder.pop_back();
	}

	return folder;
}
}

SetupScreen::SetupScreen(SetupMode t_mode, const Settings *t_settings, const Fonts *t_fonts, const Fonts *t_heading_fonts, const Fonts *t_title_fonts,
						 const Assets *t_assets, Window *t_window)
	: m_mode(t_mode)
	, m_settings(t_settings)
	, m_fonts(t_fonts)
	, m_heading_fonts(t_heading_fonts)
	, m_title_fonts(t_title_fonts)
	, m_assets(t_assets)
	, m_window(t_window)
	, m_installed(installation::find_installation())
{
	m_location.set_max_length(text_input_capacity - 1);
	m_location.set_value(to_utf8(m_installed ? m_installed->location : installation::default_location()));

	if (m_installed && t_mode != SetupMode::FirstRun) {
		m_options = m_installed->options;
		m_page = t_mode == SetupMode::Uninstall ? Page::ConfirmUninstall : Page::Manage;
	}

	for (u32 i = 0; i < option_count; i += 1) {
		m_check[i] = option_of(m_options, i) ? 1.0f : 0.0f;
	}
}

void SetupScreen::go_to(Page t_page, bool t_forward)
{
	m_previous_page = m_page;
	m_page = t_page;
	m_transition = 0.0f;
	m_direction = t_forward ? 1.0f : -1.0f;
	m_page_seconds = 0.0f;
	m_pressed = Hit::None;
	m_location.set_focused(false);
}

void SetupScreen::finish(SetupOutcome t_outcome)
{
	m_outcome = t_outcome;
}

bool SetupScreen::is_busy() const
{
	return m_saving || m_page == Page::Working;
}

bool SetupScreen::has_footer(Page t_page) const
{
	return t_page != Page::Choose && t_page != Page::Working;
}

bool SetupScreen::is_option_page(Page t_page) const
{
	return t_page == Page::Location || t_page == Page::Manage;
}

void SetupScreen::start_install()
{
	const std::string_view path = m_location.value();

	if (path.empty()) {
		m_field_error = "Choose a folder for Pulsar.";
		return;
	}

	if (!is_absolute(path)) {
		m_field_error = "Use a full path, like C:\\Apps\\Pulsar.";
		return;
	}

	m_field_error = {};
	m_progress = 0.0f;
	m_finished_seconds = 0.0f;
	m_job.start_install(trimmed_folder(path), m_options);
	go_to(Page::Working, true);
}

void SetupScreen::start_uninstall()
{
	if (!m_installed) return;

	m_progress = 0.0f;
	m_finished_seconds = 0.0f;
	std::optional<std::wstring> data_folder;
	if (m_delete_data) {
		data_folder = to_wide(storage::data_directory());
	}

	m_job.start_uninstall(*m_installed, std::move(data_folder));
	go_to(Page::Working, true);
}

void SetupScreen::open_installed_app()
{
	const std::wstring executable = m_job.installed_executable();

	if (!installation::close_running_app()) return;

	launch_process(executable);
	finish(SetupOutcome::Quit);
}

void SetupScreen::go_back()
{
	switch (m_page) {
		case Page::Location:
			go_to(Page::Choose, false);
			break;

		case Page::ConfirmUninstall:
			go_to(m_installed ? Page::Manage : Page::Choose, false);
			break;

		case Page::Failed:
			m_installed = installation::find_installation();
			go_to(m_job.task() == installation::Task::Uninstall && m_installed ? Page::Manage : Page::Location, false);
			break;

		default:
			break;
	}
}

void SetupScreen::activate(Hit t_hit)
{
	const auto option = [t_hit] { return static_cast<u32>(t_hit) - static_cast<u32>(Hit::Option0); };
	const bool is_option = t_hit >= Hit::Option0 && t_hit <= Hit::Option2;

	switch (m_page) {
		case Page::Choose:
			if (t_hit == Hit::Install) {
				go_to(Page::Location, true);
			} else if (t_hit == Hit::Portable) {
				if (m_mode == SetupMode::FirstRun) {
					installation::mark_setup_complete();
					finish(SetupOutcome::Portable);
				} else {
					finish(SetupOutcome::Closed);
				}
			}

			break;

		case Page::Location:
			if (t_hit == Hit::Secondary) {
				go_back();
			} else if (t_hit == Hit::Primary) {
				start_install();
			} else if (t_hit == Hit::Browse) {
				m_location.set_focused(false);
				m_picker.open(m_window->handle(), to_wide(m_location.value()));
			} else if (is_option) {
				toggle_option(&m_options, option());
			}

			break;

		case Page::Manage:
			if (t_hit == Hit::OpenFolder && m_installed) {
				installation::open_folder(m_installed->location);
			} else if (t_hit == Hit::Secondary) {
				go_to(Page::ConfirmUninstall, true);
			} else if (t_hit == Hit::Primary) {
				if (m_installed && m_options != m_installed->options) {
					m_job.start_apply(*m_installed, m_options);
					m_saving = true;
				} else {
					finish(SetupOutcome::Closed);
				}
			} else if (is_option) {
				toggle_option(&m_options, option());
			}

			break;

		case Page::ConfirmUninstall:
			if (t_hit == Hit::Secondary) {
				go_back();
			} else if (t_hit == Hit::Primary) {
				start_uninstall();
			} else if (t_hit == Hit::DeleteData) {
				m_delete_data = !m_delete_data;
			}

			break;

		case Page::Done:
			if (t_hit == Hit::Primary) {
				if (m_job.task() == installation::Task::Install) {
					open_installed_app();
				} else {
					finish(SetupOutcome::Quit);
				}
			}

			break;

		case Page::Failed:
			if (t_hit == Hit::Primary) {
				go_back();
			}

			break;

		case Page::Working:
			break;
	}
}

void SetupScreen::update(float t_delta_seconds)
{
	m_transition = animation::step_toward(m_transition, 1.0f, transition_seconds, t_delta_seconds);
	if (m_transition >= 1.0f) {
		m_previous_page.reset();
	}

	m_page_seconds = std::min(m_page_seconds + t_delta_seconds, settled_page_seconds);
	if (m_page_seconds < reveal_total_seconds) {
		animation::request_frame();
	}

	m_location.update(t_delta_seconds);

	if (const std::optional<std::wstring> picked = m_picker.take_result()) {
		m_location.set_value(to_utf8(*picked));
		m_field_error = {};
	}

	if (m_picker.is_open()) {
		animation::request_frame_after(picker_poll_seconds);
	}

	const bool live = m_transition >= 1.0f && !m_picker.is_open() && !is_busy();
	const Hit hovered = live ? hit_at(m_mouse) : Hit::None;

	for (u32 i = 0; i < hit_count; i += 1) {
		const float target = static_cast<u32>(hovered) == i && hovered != Hit::None ? 1.0f : 0.0f;
		m_hover[i] = animation::ease_toward(m_hover[i], target, hover_ease_rate, t_delta_seconds);
	}

	for (u32 i = 0; i < option_count; i += 1) {
		m_check[i] = animation::ease_toward(m_check[i], option_of(m_options, i) ? 1.0f : 0.0f, check_ease_rate, t_delta_seconds);
	}

	m_delete_check = animation::ease_toward(m_delete_check, m_delete_data ? 1.0f : 0.0f, check_ease_rate, t_delta_seconds);

	if (m_saving) {
		if (m_job.is_finished()) {
			finish(SetupOutcome::Closed);
		} else {
			animation::request_frame_after(job_poll_seconds);
		}
	}

	if (m_page != Page::Working) return;

	const auto steps = static_cast<float>(m_job.step_count());
	const float running = (static_cast<float>(std::min(m_job.step(), m_job.step_count())) + 0.5f) / steps;
	const float target = m_job.succeeded() ? 1.0f : std::min(running, progress_running_cap);
	m_progress = animation::ease_toward(m_progress, target, progress_ease_rate, t_delta_seconds);

	if (!m_job.is_finished()) {
		animation::request_frame_after(job_poll_seconds);
		return;
	}

	if (!m_job.succeeded()) {
		go_to(Page::Failed, true);
		return;
	}

	if (m_progress < progress_done) return;

	m_finished_seconds += t_delta_seconds;
	if (m_finished_seconds >= done_hold_seconds) {
		go_to(Page::Done, true);
	} else {
		animation::request_frame_after(done_hold_seconds - m_finished_seconds);
	}
}

Rect SetupScreen::content_rect() const
{
	const Vec2 size = m_window->size();

	return Rect{content_inset, title_bar_height, std::max(0.0f, size.x - content_inset * 2.0f), std::max(0.0f, size.y - title_bar_height)};
}

Rect SetupScreen::footer_rect() const
{
	const Vec2 size = m_window->size();

	return Rect{0.0f, size.y - footer_height, size.x, footer_height};
}

Rect SetupScreen::footer_button(bool t_right, std::string_view t_label) const
{
	const Rect footer = footer_rect();
	const float width = std::max(footer_button_min_width, std::ceil(text_width(m_fonts->body, t_label)) + button_padding * 2.0f);
	const float x = t_right ? footer.right() - content_inset - width : footer.x + content_inset;

	return Rect{x, snapped_to_pixel(footer.center().y - footer_button_height * 0.5f), width, footer_button_height};
}

std::string_view SetupScreen::primary_label(Page t_page) const
{
	switch (t_page) {
		case Page::Location:
			return "Install";
		case Page::Manage:
			return m_saving ? "Saving" : "Done";
		case Page::ConfirmUninstall:
			return "Uninstall";
		case Page::Done:
			return m_job.task() == installation::Task::Install ? "Open Pulsar" : "Close";
		case Page::Failed:
			return "Back";
		default:
			return {};
	}
}

std::string_view SetupScreen::secondary_label(Page t_page) const
{
	switch (t_page) {
		case Page::Location:
		case Page::ConfirmUninstall:
			return "Back";
		case Page::Manage:
			return "Uninstall";
		default:
			return {};
	}
}

Rect SetupScreen::header_icon_rect(Page t_page) const
{
	const Rect content = content_rect();

	if (t_page == Page::Manage) {
		return Rect{content.x, title_bar_height + manage_top_gap, header_icon_size, header_icon_size};
	}

	const float block = header_icon_size + header_rows_gap + choice_height * 2.0f + choice_gap;
	const float top = content.y + (content.h - block) * 0.5f - status_bias;

	return Rect{content.x, snapped_to_pixel(top), header_icon_size, header_icon_size};
}

Rect SetupScreen::choice_rect(u32 t_choice) const
{
	const Rect content = content_rect();
	const Rect header = header_icon_rect(Page::Choose);

	return Rect{content.x, header.bottom() + header_rows_gap + static_cast<float>(t_choice) * (choice_height + choice_gap), content.w, choice_height};
}

float SetupScreen::location_heading_top() const
{
	return title_bar_height + heading_top_gap;
}

Rect SetupScreen::field_rect() const
{
	const Rect content = content_rect();
	const float top = location_heading_top() + m_fonts->secondary.line_height() + caption_gap + m_heading_fonts->body.line_height() + field_gap;

	return Rect{content.x, snapped_to_pixel(top), content.w, field_height};
}

Rect SetupScreen::browse_rect() const
{
	const Rect field = field_rect();

	return Rect{field.right() - browse_inset - browse_size, field.center().y - browse_size * 0.5f, browse_size, browse_size};
}

Rect SetupScreen::field_text_rect() const
{
	const Rect field = field_rect();
	const float left = field.x + field_padding;

	return Rect{left, field.y, std::max(0.0f, browse_rect().x - browse_inset - left), field.h};
}

Rect SetupScreen::option_rect(Page t_page, u32 t_option) const
{
	const float top = t_page == Page::Location ? field_rect().bottom() + options_gap : installed_box_rect().bottom() + options_gap;
	const float width = check_size + check_label_gap + text_width(m_fonts->body, option_labels[t_option]) + option_reach;

	return Rect{content_rect().x, top + static_cast<float>(t_option) * option_height, width, option_height};
}

Rect SetupScreen::delete_data_rect() const
{
	const Font &secondary = m_fonts->secondary;
	const float body_top = location_heading_top() + secondary.line_height() + caption_gap + m_heading_fonts->body.line_height() + body_gap;
	const float top = body_top + (secondary.line_height() + body_line_gap) * 2.0f + options_gap - body_line_gap;
	const float width = check_size + check_label_gap + text_width(m_fonts->body, delete_data_label) + option_reach;

	return Rect{content_rect().x, top, width, option_height};
}

Rect SetupScreen::installed_box_rect() const
{
	const float top = header_icon_rect(Page::Manage).bottom() + installed_gap + m_fonts->secondary.line_height() + caption_gap * 2.0f;

	return Rect{content_rect().x, snapped_to_pixel(top), content_rect().w, field_height};
}

Rect SetupScreen::open_folder_rect() const
{
	const Rect box = installed_box_rect();

	return Rect{box.right() - browse_inset - browse_size, box.center().y - browse_size * 0.5f, browse_size, browse_size};
}

SetupScreen::Hit SetupScreen::hit_at(Vec2 t_point) const
{
	switch (m_page) {
		case Page::Choose:
			if (choice_rect(0).contains(t_point)) return Hit::Install;
			if (choice_rect(1).contains(t_point)) return Hit::Portable;
			return Hit::None;

		case Page::Location:
			if (browse_rect().contains(t_point)) return Hit::Browse;
			if (field_rect().contains(t_point)) return Hit::Field;
			break;

		case Page::ConfirmUninstall:
			if (delete_data_rect().contains(t_point)) return Hit::DeleteData;
			break;

		case Page::Manage:
			if (m_installed && open_folder_rect().contains(t_point)) return Hit::OpenFolder;
			break;

		default:
			break;
	}

	if (is_option_page(m_page)) {
		for (u32 i = 0; i < option_count; i += 1) {
			if (option_rect(m_page, i).contains(t_point)) {
				return static_cast<Hit>(static_cast<u32>(Hit::Option0) + i);
			}
		}
	}

	if (!has_footer(m_page)) return Hit::None;

	const std::string_view secondary = secondary_label(m_page);
	if (!secondary.empty() && footer_button(false, secondary).contains(t_point)) return Hit::Secondary;

	const std::string_view primary = primary_label(m_page);
	if (!primary.empty() && footer_button(true, primary).contains(t_point)) return Hit::Primary;

	return Hit::None;
}

bool SetupScreen::on_pointer_down(Vec2 t_point)
{
	m_pressed = Hit::None;
	if (m_outcome || m_transition < 1.0f || m_picker.is_open() || is_busy()) return true;

	m_pressed = hit_at(t_point);

	if (m_page == Page::Location) {
		if (m_pressed == Hit::Field) {
			m_location.set_focused(true);
			m_location.on_pointer_down(m_fonts->body, field_text_rect(), t_point.x);
		} else if (m_pressed != Hit::Browse) {
			m_location.set_focused(false);
		}
	}

	return true;
}

bool SetupScreen::on_pointer_move(Vec2 t_point)
{
	if (m_location.is_selecting()) {
		m_location.on_pointer_move(m_fonts->body, field_text_rect(), t_point.x);
	}

	return true;
}

bool SetupScreen::on_pointer_up(Vec2 t_point)
{
	m_location.on_pointer_up();

	const Hit pressed = std::exchange(m_pressed, Hit::None);
	if (pressed == Hit::None || pressed == Hit::Field || m_outcome || is_busy()) return true;

	if (hit_at(t_point) == pressed) {
		activate(pressed);
	}

	return true;
}

bool SetupScreen::on_key_down(u32 t_key)
{
	if (m_outcome || m_picker.is_open() || is_busy()) return true;

	if (m_location.is_focused() && t_key != VK_RETURN && t_key != VK_ESCAPE) {
		m_location.on_key_down(t_key);
		m_field_error = {};
		return true;
	}

	if (t_key == VK_ESCAPE) {
		if (m_location.is_focused()) {
			m_location.set_focused(false);
		} else if (m_page == Page::Location || m_page == Page::ConfirmUninstall || m_page == Page::Failed) {
			go_back();
		} else if (m_page == Page::Manage || (m_page == Page::Choose && m_mode != SetupMode::FirstRun)) {
			finish(SetupOutcome::Closed);
		}
	} else if (t_key == VK_RETURN) {
		if (m_page == Page::Location) {
			start_install();
		} else if (m_page == Page::Done || m_page == Page::Failed) {
			activate(Hit::Primary);
		}
	}

	return true;
}

bool SetupScreen::on_char(u32 t_character)
{
	if (m_location.is_focused() && !m_outcome) {
		m_location.on_char(t_character);
		m_field_error = {};
	}

	return true;
}

CursorKind SetupScreen::cursor() const
{
	if (m_location.is_selecting()) return CursorKind::IBeam;
	if (m_transition < 1.0f || is_busy() || m_picker.is_open()) return CursorKind::Arrow;

	const Hit hit = hit_at(m_mouse);
	if (hit == Hit::Field) return CursorKind::IBeam;

	return hit != Hit::None ? CursorKind::Hand : CursorKind::Arrow;
}

SetupScreen::Reveal SetupScreen::reveal(float t_seconds, u32 t_order) const
{
	const float amount = std::clamp((t_seconds - static_cast<float>(t_order) * stagger_seconds) / reveal_seconds, 0.0f, 1.0f);
	const float eased = ease_out(amount);

	return Reveal{eased, (1.0f - eased) * reveal_rise};
}

void SetupScreen::draw_app_icon(DrawList *t_draw_list, Rect t_rect, u8 t_alpha) const
{
	if (m_app_icon != nullptr) {
		t_draw_list->add_image(t_rect, m_app_icon, faded(Color{255, 255, 255, 255}, t_alpha));
	} else {
		t_draw_list->add_image(t_rect, m_assets->get(Asset::IconApp), faded(theme().text_dim, t_alpha));
	}
}

void SetupScreen::draw_header(DrawList *t_draw_list, Page t_page, const PageDraw &t_draw) const
{
	const Theme &colors = theme();
	const Font &heading = m_title_fonts->body;
	const Reveal shown = reveal(t_draw.seconds, 0);
	const u8 alpha = to_alpha(t_draw.alpha * shown.alpha);
	const Rect icon = header_icon_rect(t_page).moved(Vec2{t_draw.offset.x, t_draw.offset.y + shown.rise});
	const float text_x = icon.right() + header_gap;

	draw_app_icon(t_draw_list, icon, alpha);

	draw_text(t_draw_list, heading, Vec2{text_x, heading.centered_baseline(icon)}, app_title, faded(colors.text, alpha));
}

void SetupScreen::draw_installed_location(DrawList *t_draw_list, const PageDraw &t_draw) const
{
	if (!m_installed) return;

	const Theme &colors = theme();
	const Font &body = m_fonts->body;
	const Font &secondary = m_fonts->secondary;
	const Reveal shown = reveal(t_draw.seconds, 1);
	const u8 alpha = to_alpha(t_draw.alpha * shown.alpha);
	const Vec2 offset{t_draw.offset.x, t_draw.offset.y + shown.rise};
	const Rect box = installed_box_rect().moved(offset);
	const Rect button = open_folder_rect().moved(offset);
	const float hover = m_hover[static_cast<u32>(Hit::OpenFolder)];
	const float caption_y = box.y - caption_gap - secondary.line_height();
	const float text_x = box.x + field_padding;

	draw_text(t_draw_list, secondary, Vec2{box.x, caption_y + secondary.ascent}, "Installed in", faded(colors.text_faint, alpha));
	t_draw_list->add_bordered_rect(box, rounded(field_radius), faded(colors.field, alpha), faded(colors.separator, alpha), 1.0f);
	draw_text_truncated(t_draw_list, body, Vec2{text_x, body.centered_baseline(box)}, to_utf8(m_installed->location), button.x - browse_inset - text_x,
						faded(colors.text_dim, alpha));

	t_draw_list->add_rounded_rect(button, rounded(browse_radius), faded(mix(colors.control, colors.control_hover, hover), alpha));
	t_draw_list->add_image(button.centered(browse_icon_size, browse_icon_size), m_assets->get(Asset::IconFolderOpen),
						   faded(mix(colors.text_dim, colors.text, hover), alpha));
}

void SetupScreen::draw_choice(DrawList *t_draw_list, u32 t_choice, const PageDraw &t_draw) const
{
	const Theme &colors = theme();
	const Font &body = m_fonts->body;
	const Font &secondary = m_fonts->secondary;
	const Color accent = m_settings->accent;
	const bool install = t_choice == 0;
	const float hover = m_hover[static_cast<u32>(install ? Hit::Install : Hit::Portable)];
	const Reveal shown = reveal(t_draw.seconds, 1 + t_choice);
	const u8 alpha = to_alpha(t_draw.alpha * shown.alpha);
	const Rect row = choice_rect(t_choice).moved(Vec2{t_draw.offset.x, t_draw.offset.y + shown.rise});

	const Color fill = mix(colors.surface, hovered(colors.surface), hover);
	const Color border = mix(colors.separator, mix(colors.border, accent, choice_border_accent), hover);
	t_draw_list->add_bordered_rect(row, rounded(choice_radius), faded(fill, alpha), faded(border, alpha), 1.0f);

	const Rect tile{row.x + choice_padding, snapped_to_pixel(row.center().y - choice_tile_size * 0.5f), choice_tile_size, choice_tile_size};
	const Color tile_fill = mix(colors.control, colors.control_hover, hover);
	const Color icon_color = mix(colors.text_dim, colors.text, hover);

	t_draw_list->add_rounded_rect(tile, rounded(choice_tile_radius), faded(tile_fill, alpha));
	t_draw_list->add_image(tile.centered(choice_icon_size, choice_icon_size), m_assets->get(install ? Asset::IconDownload : Asset::IconFile),
						   faded(icon_color, alpha));

	const float lines = body.line_height() + choice_line_gap + secondary.line_height();
	const float top = row.center().y - lines * 0.5f;
	const float text_x = tile.right() + choice_padding;

	draw_text(t_draw_list, body, Vec2{text_x, top + body.ascent}, install ? "Install" : "Portable", faded(colors.text, alpha));
	draw_text(t_draw_list, secondary, Vec2{text_x, top + body.line_height() + choice_line_gap + secondary.ascent},
			  install ? "Adds shortcuts and an uninstaller" : "Nothing is added to your system", faded(colors.text_faint, alpha));

	const float chevron_x = row.right() - choice_padding - chevron_size.x - chevron_nudge + chevron_nudge * hover;
	const float center_y = row.center().y;
	const Color chevron = faded(mix(colors.text_faint, colors.text_dim, hover), alpha);

	t_draw_list->add_line(Vec2{chevron_x, center_y - chevron_size.y * 0.5f}, Vec2{chevron_x + chevron_size.x, center_y}, chevron_thickness, chevron);
	t_draw_list->add_line(Vec2{chevron_x + chevron_size.x, center_y}, Vec2{chevron_x, center_y + chevron_size.y * 0.5f}, chevron_thickness, chevron);
}

void SetupScreen::draw_location(DrawList *t_draw_list, const PageDraw &t_draw)
{
	const Theme &colors = theme();
	const Font &body = m_fonts->body;
	const Font &secondary = m_fonts->secondary;
	const Font &heading = m_heading_fonts->body;
	const Color accent = m_settings->accent;
	const float x = content_rect().x + t_draw.offset.x;

	const Reveal title = reveal(t_draw.seconds, 0);
	const u8 title_alpha = to_alpha(t_draw.alpha * title.alpha);
	const float top = location_heading_top() + title.rise;

	draw_text(t_draw_list, secondary, Vec2{x, top + secondary.ascent}, "Install", faded(colors.text_faint, title_alpha));
	draw_text(t_draw_list, heading, Vec2{x, top + secondary.line_height() + caption_gap + heading.ascent}, "Where should Pulsar go?",
			  faded(colors.text, title_alpha));

	const Reveal shown = reveal(t_draw.seconds, 1);
	const u8 alpha = to_alpha(t_draw.alpha * shown.alpha);
	const Vec2 offset{t_draw.offset.x, shown.rise};
	const Rect field = field_rect().moved(offset);
	const Rect browse = browse_rect().moved(offset);
	const bool focused = m_location.is_focused();
	const float field_hover = m_hover[static_cast<u32>(Hit::Field)];
	const float browse_hover = m_hover[static_cast<u32>(Hit::Browse)];

	Color border = mix(colors.separator, colors.border, field_hover);
	if (!m_field_error.empty()) {
		border = colors.error;
	} else if (focused) {
		border = accent;
	}

	t_draw_list->add_bordered_rect(field, rounded(field_radius), faded(colors.field, alpha), faded(border, alpha), focused ? field_focus_border : 1.0f);
	m_location.draw(t_draw_list, body, field_text_rect().moved(offset), faded(colors.text, alpha), faded(accent, alpha), std::nullopt);

	t_draw_list->add_rounded_rect(browse, rounded(browse_radius), faded(mix(colors.control, colors.control_hover, browse_hover), alpha));
	t_draw_list->add_image(browse.centered(browse_icon_size, browse_icon_size), m_assets->get(Asset::IconFolder),
						   faded(mix(colors.text_dim, colors.text, browse_hover), alpha));

	if (!m_field_error.empty()) {
		draw_text(t_draw_list, secondary, Vec2{field.x + 2.0f, field.bottom() + error_gap + secondary.ascent}, m_field_error, faded(colors.error, alpha));
	}
}

void SetupScreen::draw_check_row(DrawList *t_draw_list, Rect t_row, std::string_view t_label, float t_checked, float t_hover, float t_alpha, Color t_fill) const
{
	const Theme &colors = theme();
	const Font &body = m_fonts->body;
	const u8 alpha = to_alpha(t_alpha);
	const Rect box{t_row.x, snapped_to_pixel(t_row.center().y - check_size * 0.5f), check_size, check_size};
	const Color frame = mix(colors.border, mix(colors.border, t_fill, check_hover_accent), t_hover);

	t_draw_list->add_bordered_rect(box, rounded(check_radius), faded(colors.field, alpha), faded(frame, alpha), check_border);

	if (t_checked > 0.001f) {
		const u8 checked_alpha = to_alpha(t_alpha * t_checked);
		const float scale = 1.0f - check_grow + check_grow * t_checked;

		t_draw_list->add_rounded_rect(box, rounded(check_radius), faded(t_fill, checked_alpha));
		controls::draw_check(t_draw_list, m_assets, box.centered(box.w * scale, box.h * scale).inset(check_inset), faded(foreground_on(t_fill), checked_alpha));
	}

	const Color label = mix(colors.text_dim, colors.text, std::max(t_hover, t_checked * 0.6f));
	draw_text(t_draw_list, body, Vec2{box.right() + check_label_gap, body.centered_baseline(t_row)}, t_label, faded(label, alpha));
}

void SetupScreen::draw_options(DrawList *t_draw_list, Page t_page, const PageDraw &t_draw, u32 t_first_order) const
{
	for (u32 i = 0; i < option_count; i += 1) {
		const Reveal shown = reveal(t_draw.seconds, t_first_order + i);
		const Rect row = option_rect(t_page, i).moved(Vec2{t_draw.offset.x, t_draw.offset.y + shown.rise});

		draw_check_row(t_draw_list, row, option_labels[i], m_check[i], m_hover[static_cast<u32>(Hit::Option0) + i], t_draw.alpha * shown.alpha,
					   m_settings->accent);
	}
}

void SetupScreen::draw_confirm(DrawList *t_draw_list, const PageDraw &t_draw) const
{
	const Theme &colors = theme();
	const Font &secondary = m_fonts->secondary;
	const Font &heading = m_heading_fonts->body;
	const Rect content = content_rect();
	const float x = content.x + t_draw.offset.x;
	const Reveal title = reveal(t_draw.seconds, 0);
	const u8 title_alpha = to_alpha(t_draw.alpha * title.alpha);
	const float top = location_heading_top() + title.rise;

	draw_text(t_draw_list, secondary, Vec2{x, top + secondary.ascent}, "Uninstall", faded(colors.text_faint, title_alpha));
	draw_text(t_draw_list, heading, Vec2{x, top + secondary.line_height() + caption_gap + heading.ascent}, "Uninstall Pulsar?",
			  faded(colors.text, title_alpha));

	const std::string_view data_line = m_delete_data ? "Your accounts and settings will be deleted too." : "Your accounts and settings stay on this PC.";
	const std::string_view lines[]{"Pulsar and its shortcuts will be removed.", data_line};
	const Color line_colors[]{colors.text_dim, mix(colors.text_dim, colors.error, m_delete_check)};
	float y = location_heading_top() + secondary.line_height() + caption_gap + heading.line_height() + body_gap;

	for (u32 i = 0; i < 2; i += 1) {
		const Reveal shown = reveal(t_draw.seconds, 1 + i);
		draw_text_truncated(t_draw_list, secondary, Vec2{x, y + shown.rise + secondary.ascent}, lines[i], content.w,
							faded(line_colors[i], to_alpha(t_draw.alpha * shown.alpha)));
		y += secondary.line_height() + body_line_gap;
	}

	const Reveal shown = reveal(t_draw.seconds, 3);
	draw_check_row(t_draw_list, delete_data_rect().moved(Vec2{t_draw.offset.x, t_draw.offset.y + shown.rise}), delete_data_label, m_delete_check,
				   m_hover[static_cast<u32>(Hit::DeleteData)], t_draw.alpha * shown.alpha, colors.error);
}

void SetupScreen::draw_status(DrawList *t_draw_list, Page t_page, const PageDraw &t_draw) const
{
	const Theme &colors = theme();
	const Font &heading = m_heading_fonts->body;
	const Font &secondary = m_fonts->secondary;
	const Color accent = m_settings->accent;
	const Vec2 size = m_window->size();
	const bool install = m_job.task() != installation::Task::Uninstall;
	const float bottom = has_footer(t_page) ? footer_rect().y : size.y;
	const Rect content = content_rect();

	std::string_view title;
	std::string_view lines[max_message_lines];
	u32 line_count = 0;
	std::string_view detail;

	if (t_page == Page::Working) {
		title = install ? "Installing Pulsar" : "Uninstalling Pulsar";
	} else if (t_page == Page::Done) {
		title = install ? "Pulsar is installed" : "Pulsar was uninstalled";

		if (!install) {
			detail = m_delete_data ? "Your accounts and settings were deleted too." : "Your accounts and settings are still on this PC.";
		} else if (m_options.start_menu_shortcut) {
			detail = "Find it in the Start menu.";
		} else if (m_options.desktop_shortcut) {
			detail = "Find it on your desktop.";
		} else {
			detail = "It's ready in the folder you picked.";
		}
	} else {
		title = install ? "Pulsar couldn't be installed" : "Pulsar couldn't be uninstalled";
		detail = m_job.error();
	}

	if (t_page != Page::Working && !detail.empty()) {
		line_count = wrap_text(secondary, detail, content.w, lines);
	}

	const float body_height =
		t_page == Page::Working ? progress_height + status_text_gap + secondary.line_height() : static_cast<float>(line_count) * secondary.line_height();
	const float block = status_icon_size + status_icon_gap + heading.line_height() + status_text_gap + body_height;
	const float top = title_bar_height + (bottom - title_bar_height - block) * 0.5f - status_bias;
	const float center_x = size.x * 0.5f + t_draw.offset.x;

	const Reveal icon_shown = reveal(t_draw.seconds, 0);
	const float icon_dim = t_page == Page::Done && !install ? uninstalled_icon_dim : 1.0f;
	const Rect icon{snapped_to_pixel(center_x - status_icon_size * 0.5f), snapped_to_pixel(top + icon_shown.rise), status_icon_size, status_icon_size};
	draw_app_icon(t_draw_list, icon, to_alpha(t_draw.alpha * icon_shown.alpha * icon_dim));

	if (t_page == Page::Failed) {
		const Reveal badge_shown = reveal(t_draw.seconds, 2);
		const float badge = badge_size * (1.0f - badge_pop + badge_pop * badge_shown.alpha);
		const Vec2 badge_center{icon.right() - badge_offset, icon.bottom() - badge_offset};
		const Rect circle{badge_center.x - badge * 0.5f, badge_center.y - badge * 0.5f, badge, badge};
		const Color fill = colors.error;
		const u8 alpha = to_alpha(t_draw.alpha * badge_shown.alpha);

		t_draw_list->add_rounded_rect(circle.inset(-badge_ring), rounded(badge * 0.5f + badge_ring), faded(colors.window, alpha));
		t_draw_list->add_rounded_rect(circle, rounded(badge * 0.5f), faded(fill, alpha));

		controls::draw_x(t_draw_list, circle.inset(badge * 0.3f), faded(foreground_on(fill), alpha));
	}

	const Reveal title_shown = reveal(t_draw.seconds, 1);
	const float title_y = icon.bottom() - icon_shown.rise + status_icon_gap + title_shown.rise;
	draw_text_centered(t_draw_list, heading, Rect{t_draw.offset.x, title_y, size.x, heading.line_height()}, title,
					   faded(colors.text, to_alpha(t_draw.alpha * title_shown.alpha)));

	const Reveal body_shown = reveal(t_draw.seconds, 2);
	const u8 body_alpha = to_alpha(t_draw.alpha * body_shown.alpha);
	float y = title_y - title_shown.rise + heading.line_height() + status_text_gap + body_shown.rise;

	if (t_page == Page::Working) {
		const Rect track{snapped_to_pixel(center_x - progress_width * 0.5f), snapped_to_pixel(y), progress_width, progress_height};
		const float filled = std::max(progress_height, progress_width * std::clamp(m_progress, 0.0f, 1.0f));

		t_draw_list->add_rounded_rect(track, rounded(progress_height * 0.5f), faded(colors.track, body_alpha));
		t_draw_list->add_rounded_rect(Rect{track.x, track.y, filled, progress_height}, rounded(progress_height * 0.5f), faded(accent, body_alpha));

		y = track.bottom() + status_text_gap;
		draw_text_centered(t_draw_list, secondary, Rect{t_draw.offset.x, y, size.x, secondary.line_height()}, m_job.step_label(),
						   faded(colors.text_faint, body_alpha));
		return;
	}

	for (const std::string_view line : std::span{lines, line_count}) {
		draw_text_centered(t_draw_list, secondary, Rect{t_draw.offset.x, y, size.x, secondary.line_height()}, line, faded(colors.text_dim, body_alpha));
		y += secondary.line_height();
	}
}

void SetupScreen::draw_footer(DrawList *t_draw_list, Page t_page, const PageDraw &t_draw) const
{
	const Font &body = m_fonts->body;
	const Color accent = m_settings->accent;
	const u8 alpha = to_alpha(t_draw.alpha);
	const bool live = t_page == m_page && m_transition >= 1.0f && !is_busy();
	const Vec2 offset{t_draw.offset.x, 0.0f};

	const std::string_view secondary = secondary_label(t_page);
	if (!secondary.empty()) {
		const auto style = t_page == Page::Manage ? controls::ButtonStyle::Danger : controls::ButtonStyle::Ghost;
		const bool hovered_button = live && m_hover[static_cast<u32>(Hit::Secondary)] > 0.5f;

		controls::draw_button(t_draw_list, body, footer_button(false, secondary).moved(offset), secondary, style, accent, !m_saving, hovered_button, alpha);
	}

	const std::string_view primary = primary_label(t_page);
	if (!primary.empty()) {
		const auto style = t_page == Page::ConfirmUninstall ? controls::ButtonStyle::DangerConfirm : controls::ButtonStyle::Accent;
		const bool hovered_button = live && m_hover[static_cast<u32>(Hit::Primary)] > 0.5f;

		controls::draw_button(t_draw_list, body, footer_button(true, primary).moved(offset), primary, style, accent, !m_saving, hovered_button, alpha);
	}
}

void SetupScreen::draw_page(DrawList *t_draw_list, Page t_page, const PageDraw &t_draw)
{
	switch (t_page) {
		case Page::Choose:
			draw_header(t_draw_list, t_page, t_draw);
			draw_choice(t_draw_list, 0, t_draw);
			draw_choice(t_draw_list, 1, t_draw);
			break;

		case Page::Location:
			draw_location(t_draw_list, t_draw);
			draw_options(t_draw_list, t_page, t_draw, 2);
			break;

		case Page::Manage:
			draw_header(t_draw_list, t_page, t_draw);
			draw_installed_location(t_draw_list, t_draw);
			draw_options(t_draw_list, t_page, t_draw, 2);
			break;

		case Page::ConfirmUninstall:
			draw_confirm(t_draw_list, t_draw);
			break;

		case Page::Working:
		case Page::Done:
		case Page::Failed:
			draw_status(t_draw_list, t_page, t_draw);
			break;
	}

	if (has_footer(t_page)) {
		draw_footer(t_draw_list, t_page, t_draw);
	}
}

void SetupScreen::draw(DrawList *t_draw_list)
{
	const Theme &colors = theme();
	const float eased = ease_out(m_transition);
	const Rect footer = footer_rect();

	if (m_previous_page && m_transition < 1.0f) {
		draw_page(t_draw_list, *m_previous_page, PageDraw{Vec2{-m_direction * slide_distance * eased, 0.0f}, 1.0f - eased, settled_page_seconds});
	}

	draw_page(t_draw_list, m_page, PageDraw{Vec2{m_direction * slide_distance * (1.0f - eased), 0.0f}, eased, m_page_seconds});

	float footer_line = has_footer(m_page) ? eased : 0.0f;
	if (m_previous_page && has_footer(*m_previous_page)) {
		footer_line += 1.0f - eased;
	}

	if (footer_line > 0.001f) {
		t_draw_list->add_rect(Rect{0.0f, footer.y, footer.w, 1.0f}, faded(colors.separator, to_alpha(footer_line)));
	}
}
