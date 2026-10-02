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
constexpr float K_CONTENT_INSET           = 36.0f;
constexpr float K_FOOTER_HEIGHT           = 64.0f;
constexpr float K_FOOTER_BUTTON_HEIGHT    = 34.0f;
constexpr float K_FOOTER_BUTTON_MIN_WIDTH = 96.0f;
constexpr float K_BUTTON_PADDING          = 20.0f;
constexpr float K_HEADER_ICON_SIZE        = 56.0f;
constexpr float K_HEADER_GAP              = 16.0f;
constexpr float K_HEADER_ROWS_GAP         = 30.0f;
constexpr float K_MANAGE_TOP_GAP          = 20.0f;
constexpr float K_INSTALLED_GAP           = 16.0f;
constexpr float K_CHOICE_HEIGHT           = 60.0f;
constexpr float K_CHOICE_GAP              = 10.0f;
constexpr float K_CHOICE_RADIUS           = 10.0f;
constexpr float K_CHOICE_TILE_SIZE        = 34.0f;
constexpr float K_CHOICE_TILE_RADIUS      = 9.0f;
constexpr float K_CHOICE_ICON_SIZE        = 18.0f;
constexpr float K_CHOICE_PADDING          = 13.0f;
constexpr float K_CHOICE_LINE_GAP         = 2.0f;
constexpr float K_CHOICE_BORDER_ACCENT    = 0.55f;
constexpr Vec2  K_CHEVRON_SIZE{5.0f, 9.0f};
constexpr float K_CHEVRON_NUDGE        = 2.0f;
constexpr float K_CHEVRON_THICKNESS    = 1.5f;
constexpr float K_HEADING_TOP_GAP      = 16.0f;
constexpr float K_CAPTION_GAP          = 2.0f;
constexpr float K_FIELD_GAP            = 12.0f;
constexpr float K_FIELD_HEIGHT         = 38.0f;
constexpr float K_FIELD_RADIUS         = 8.0f;
constexpr float K_FIELD_PADDING        = 12.0f;
constexpr float K_FIELD_FOCUS_BORDER   = 1.5f;
constexpr float K_BROWSE_SIZE          = 28.0f;
constexpr float K_BROWSE_INSET         = 5.0f;
constexpr float K_BROWSE_RADIUS        = 6.0f;
constexpr float K_BROWSE_ICON_SIZE     = 16.0f;
constexpr float K_ERROR_GAP            = 5.0f;
constexpr float K_OPTIONS_GAP          = 24.0f;
constexpr float K_OPTION_HEIGHT        = 28.0f;
constexpr float K_CHECK_SIZE           = 16.0f;
constexpr float K_CHECK_RADIUS         = 4.0f;
constexpr float K_CHECK_BORDER         = 1.5f;
constexpr float K_CHECK_INSET          = 2.0f;
constexpr float K_CHECK_GROW           = 0.4f;
constexpr float K_CHECK_LABEL_GAP      = 10.0f;
constexpr float K_CHECK_HOVER_ACCENT   = 0.6f;
constexpr float K_OPTION_REACH         = 6.0f;
constexpr float K_BODY_GAP             = 14.0f;
constexpr float K_BODY_LINE_GAP        = 4.0f;
constexpr float K_STATUS_ICON_SIZE     = 52.0f;
constexpr float K_STATUS_ICON_GAP      = 18.0f;
constexpr float K_STATUS_TEXT_GAP      = 12.0f;
constexpr float K_STATUS_BIAS          = 6.0f;
constexpr float K_UNINSTALLED_ICON_DIM = 0.5f;
constexpr float K_BADGE_SIZE           = 20.0f;
constexpr float K_BADGE_RING           = 2.0f;
constexpr float K_BADGE_OFFSET         = 4.0f;
constexpr float K_BADGE_POP            = 0.4f;
constexpr float K_PROGRESS_WIDTH       = 240.0f;
constexpr float K_PROGRESS_HEIGHT      = 6.0f;
constexpr float K_PROGRESS_RUNNING_CAP = 0.96f;
constexpr u32   K_MAX_MESSAGE_LINES    = 3;

constexpr float K_TRANSITION_SECONDS   = 0.32f;
constexpr float K_SLIDE_DISTANCE       = 28.0f;
constexpr float K_STAGGER_SECONDS      = 0.055f;
constexpr float K_REVEAL_SECONDS       = 0.34f;
constexpr float K_REVEAL_RISE          = 8.0f;
constexpr float K_REVEAL_TOTAL_SECONDS = 0.8f;
constexpr float K_SETTLED_PAGE_SECONDS = 10.0f;
constexpr float K_HOVER_EASE_RATE      = 18.0f;
constexpr float K_CHECK_EASE_RATE      = 20.0f;
constexpr float K_PROGRESS_EASE_RATE   = 7.0f;
constexpr float K_JOB_POLL_SECONDS     = 0.05f;
constexpr float K_PICKER_POLL_SECONDS  = 0.1f;
constexpr float K_DONE_HOLD_SECONDS    = 0.25f;
constexpr float K_PROGRESS_DONE        = 0.995f;

constexpr std::string_view K_APP_TITLE = "Pulsar";
constexpr std::string_view K_OPTION_LABELS[]{"Desktop shortcut", "Start menu shortcut", "Start with Windows"};
constexpr std::string_view K_DELETE_DATA_LABEL = "Also delete my accounts and settings";

[[nodiscard]] auto ease_out(float t_amount) -> float
{
	const float inverse = 1.0f - t_amount;

	return 1.0f - inverse * inverse * inverse;
}

[[nodiscard]] auto option_of(const installation::Options& t_options, u32 t_option) -> bool
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

auto toggle_option(installation::Options* t_options, u32 t_option) -> void
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

[[nodiscard]] auto is_absolute(std::string_view t_path) -> bool
{
	const bool drive = t_path.size() >= 3 && ((t_path[0] >= 'A' && t_path[0] <= 'Z') || (t_path[0] >= 'a' && t_path[0] <= 'z')) && t_path[1] == ':' &&
	                   (t_path[2] == '\\' || t_path[2] == '/');

	return drive || t_path.starts_with("\\\\");
}

[[nodiscard]] auto trimmed_folder(std::string_view t_path) -> std::wstring
{
	std::wstring folder = to_wide(t_path);
	while (folder.size() > 3 && (folder.back() == L'\\' || folder.back() == L'/' || folder.back() == L' ')) {
		folder.pop_back();
	}

	return folder;
}
}

SetupScreen::SetupScreen(SetupMode       t_mode,
                         const Settings* t_settings,
                         const Fonts*    t_fonts,
                         const Fonts*    t_heading_fonts,
                         const Fonts*    t_title_fonts,
                         const Assets*   t_assets,
                         Window*         t_window)
	: m_mode(t_mode)
	, m_settings(t_settings)
	, m_fonts(t_fonts)
	, m_heading_fonts(t_heading_fonts)
	, m_title_fonts(t_title_fonts)
	, m_assets(t_assets)
	, m_window(t_window)
	, m_installed(installation::find_installation())
{
	m_location.set_max_length(K_TEXT_INPUT_CAPACITY - 1);
	m_location.set_value(to_utf8(m_installed ? m_installed->location : installation::default_location()));

	if (m_installed && t_mode != SetupMode::FirstRun) {
		m_options = m_installed->options;
		m_page    = t_mode == SetupMode::Uninstall ? Page::ConfirmUninstall : Page::Manage;
	}

	for (u32 i = 0; i < K_OPTION_COUNT; i += 1) {
		m_check[i] = option_of(m_options, i) ? 1.0f : 0.0f;
	}
}

auto SetupScreen::go_to(Page t_page, bool t_forward) -> void
{
	m_previous_page = m_page;
	m_page          = t_page;
	m_transition    = 0.0f;
	m_direction     = t_forward ? 1.0f : -1.0f;
	m_page_seconds  = 0.0f;
	m_pressed       = Hit::None;
	m_location.set_focused(false);
}

auto SetupScreen::finish(SetupOutcome t_outcome) -> void
{
	m_outcome = t_outcome;
}

auto SetupScreen::is_busy() const -> bool
{
	return m_saving || m_page == Page::Working;
}

auto SetupScreen::has_footer(Page t_page) const -> bool
{
	return t_page != Page::Choose && t_page != Page::Working;
}

auto SetupScreen::is_option_page(Page t_page) const -> bool
{
	return t_page == Page::Location || t_page == Page::Manage;
}

auto SetupScreen::start_install() -> void
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

	m_field_error      = {};
	m_progress         = 0.0f;
	m_finished_seconds = 0.0f;
	m_job.start_install(trimmed_folder(path), m_options);
	go_to(Page::Working, true);
}

auto SetupScreen::start_uninstall() -> void
{
	if (!m_installed) return;

	m_progress         = 0.0f;
	m_finished_seconds = 0.0f;
	std::optional<std::wstring> data_folder;
	if (m_delete_data) {
		data_folder = to_wide(storage::data_directory());
	}

	m_job.start_uninstall(*m_installed, std::move(data_folder));
	go_to(Page::Working, true);
}

auto SetupScreen::open_installed_app() -> void
{
	const std::wstring executable = m_job.installed_executable();

	if (!installation::close_running_app()) return;

	launch_process(executable);
	finish(SetupOutcome::Quit);
}

auto SetupScreen::go_back() -> void
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

auto SetupScreen::activate(Hit t_hit) -> void
{
	const auto option    = [t_hit] { return static_cast<u32>(t_hit) - static_cast<u32>(Hit::Option0); };
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
				m_picker.open(m_window->handle(),
				              PathRequest{.title = L"Choose where to install Pulsar", .ok_label = L"Select folder", .start_path = to_wide(m_location.value())});
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

auto SetupScreen::update(float t_delta_seconds) -> void
{
	m_transition = animation::step_toward(m_transition, 1.0f, K_TRANSITION_SECONDS, t_delta_seconds);
	if (m_transition >= 1.0f) {
		m_previous_page.reset();
	}

	m_page_seconds = std::min(m_page_seconds + t_delta_seconds, K_SETTLED_PAGE_SECONDS);
	if (m_page_seconds < K_REVEAL_TOTAL_SECONDS) {
		animation::request_frame();
	}

	m_location.update(t_delta_seconds);

	if (const std::optional<std::wstring> picked = m_picker.take_result()) {
		m_location.set_value(to_utf8(installation::with_app_folder(*picked)));
		m_field_error = {};
	}

	if (m_picker.is_open()) {
		animation::request_frame_after(K_PICKER_POLL_SECONDS);
	}

	const bool live    = m_transition >= 1.0f && !m_picker.is_open() && !is_busy();
	const Hit  hovered = live ? hit_at(m_mouse) : Hit::None;

	for (u32 i = 0; i < K_HIT_COUNT; i += 1) {
		const float target = static_cast<u32>(hovered) == i && hovered != Hit::None ? 1.0f : 0.0f;
		m_hover[i]         = animation::ease_toward(m_hover[i], target, K_HOVER_EASE_RATE, t_delta_seconds);
	}

	for (u32 i = 0; i < K_OPTION_COUNT; i += 1) {
		m_check[i] = animation::ease_toward(m_check[i], option_of(m_options, i) ? 1.0f : 0.0f, K_CHECK_EASE_RATE, t_delta_seconds);
	}

	m_delete_check = animation::ease_toward(m_delete_check, m_delete_data ? 1.0f : 0.0f, K_CHECK_EASE_RATE, t_delta_seconds);

	if (m_saving) {
		if (m_job.is_finished()) {
			finish(SetupOutcome::Closed);
		} else {
			animation::request_frame_after(K_JOB_POLL_SECONDS);
		}
	}

	if (m_page != Page::Working) return;

	const auto  steps   = static_cast<float>(m_job.step_count());
	const float running = (static_cast<float>(std::min(m_job.step(), m_job.step_count())) + 0.5f) / steps;
	const float target  = m_job.succeeded() ? 1.0f : std::min(running, K_PROGRESS_RUNNING_CAP);
	m_progress          = animation::ease_toward(m_progress, target, K_PROGRESS_EASE_RATE, t_delta_seconds);

	if (!m_job.is_finished()) {
		animation::request_frame_after(K_JOB_POLL_SECONDS);
		return;
	}

	if (!m_job.succeeded()) {
		go_to(Page::Failed, true);
		return;
	}

	if (m_progress < K_PROGRESS_DONE) return;

	m_finished_seconds += t_delta_seconds;
	if (m_finished_seconds >= K_DONE_HOLD_SECONDS) {
		go_to(Page::Done, true);
	} else {
		animation::request_frame_after(K_DONE_HOLD_SECONDS - m_finished_seconds);
	}
}

auto SetupScreen::content_rect() const -> Rect
{
	const Vec2 size = m_window->size();

	return Rect{K_CONTENT_INSET, K_TITLE_BAR_HEIGHT, std::max(0.0f, size.x - K_CONTENT_INSET * 2.0f), std::max(0.0f, size.y - K_TITLE_BAR_HEIGHT)};
}

auto SetupScreen::footer_rect() const -> Rect
{
	const Vec2 size = m_window->size();

	return Rect{0.0f, size.y - K_FOOTER_HEIGHT, size.x, K_FOOTER_HEIGHT};
}

auto SetupScreen::footer_button(bool t_right, std::string_view t_label) const -> Rect
{
	const Rect  footer = footer_rect();
	const float width  = std::max(K_FOOTER_BUTTON_MIN_WIDTH, std::ceil(text_width(m_fonts->body, t_label)) + K_BUTTON_PADDING * 2.0f);
	const float x      = t_right ? footer.right() - K_CONTENT_INSET - width : footer.x + K_CONTENT_INSET;

	return Rect{x, snapped_to_pixel(footer.center().y - K_FOOTER_BUTTON_HEIGHT * 0.5f), width, K_FOOTER_BUTTON_HEIGHT};
}

auto SetupScreen::primary_label(Page t_page) const -> std::string_view
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

auto SetupScreen::secondary_label(Page t_page) const -> std::string_view
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

auto SetupScreen::header_icon_rect(Page t_page) const -> Rect
{
	const Rect content = content_rect();

	if (t_page == Page::Manage) {
		return Rect{content.x, K_TITLE_BAR_HEIGHT + K_MANAGE_TOP_GAP, K_HEADER_ICON_SIZE, K_HEADER_ICON_SIZE};
	}

	const float block = K_HEADER_ICON_SIZE + K_HEADER_ROWS_GAP + K_CHOICE_HEIGHT * 2.0f + K_CHOICE_GAP;
	const float top   = content.y + (content.h - block) * 0.5f - K_STATUS_BIAS;

	return Rect{content.x, snapped_to_pixel(top), K_HEADER_ICON_SIZE, K_HEADER_ICON_SIZE};
}

auto SetupScreen::choice_rect(u32 t_choice) const -> Rect
{
	const Rect content = content_rect();
	const Rect header  = header_icon_rect(Page::Choose);

	return Rect{content.x, header.bottom() + K_HEADER_ROWS_GAP + static_cast<float>(t_choice) * (K_CHOICE_HEIGHT + K_CHOICE_GAP), content.w, K_CHOICE_HEIGHT};
}

auto SetupScreen::location_heading_top() const -> float
{
	return K_TITLE_BAR_HEIGHT + K_HEADING_TOP_GAP;
}

auto SetupScreen::field_rect() const -> Rect
{
	const Rect  content = content_rect();
	const float top     = location_heading_top() + m_fonts->secondary.line_height() + K_CAPTION_GAP + m_heading_fonts->body.line_height() + K_FIELD_GAP;

	return Rect{content.x, snapped_to_pixel(top), content.w, K_FIELD_HEIGHT};
}

auto SetupScreen::browse_rect() const -> Rect
{
	const Rect field = field_rect();

	return Rect{field.right() - K_BROWSE_INSET - K_BROWSE_SIZE, field.center().y - K_BROWSE_SIZE * 0.5f, K_BROWSE_SIZE, K_BROWSE_SIZE};
}

auto SetupScreen::field_text_rect() const -> Rect
{
	const Rect  field = field_rect();
	const float left  = field.x + K_FIELD_PADDING;

	return Rect{left, field.y, std::max(0.0f, browse_rect().x - K_BROWSE_INSET - left), field.h};
}

auto SetupScreen::option_rect(Page t_page, u32 t_option) const -> Rect
{
	const float top   = t_page == Page::Location ? field_rect().bottom() + K_OPTIONS_GAP : installed_box_rect().bottom() + K_OPTIONS_GAP;
	const float width = K_CHECK_SIZE + K_CHECK_LABEL_GAP + text_width(m_fonts->body, K_OPTION_LABELS[t_option]) + K_OPTION_REACH;

	return Rect{content_rect().x, top + static_cast<float>(t_option) * K_OPTION_HEIGHT, width, K_OPTION_HEIGHT};
}

auto SetupScreen::delete_data_rect() const -> Rect
{
	const Font& secondary = m_fonts->secondary;
	const float body_top  = location_heading_top() + secondary.line_height() + K_CAPTION_GAP + m_heading_fonts->body.line_height() + K_BODY_GAP;
	const float top       = body_top + (secondary.line_height() + K_BODY_LINE_GAP) * 2.0f + K_OPTIONS_GAP - K_BODY_LINE_GAP;
	const float width     = K_CHECK_SIZE + K_CHECK_LABEL_GAP + text_width(m_fonts->body, K_DELETE_DATA_LABEL) + K_OPTION_REACH;

	return Rect{content_rect().x, top, width, K_OPTION_HEIGHT};
}

auto SetupScreen::installed_box_rect() const -> Rect
{
	const float top = header_icon_rect(Page::Manage).bottom() + K_INSTALLED_GAP + m_fonts->secondary.line_height() + K_CAPTION_GAP * 2.0f;

	return Rect{content_rect().x, snapped_to_pixel(top), content_rect().w, K_FIELD_HEIGHT};
}

auto SetupScreen::open_folder_rect() const -> Rect
{
	const Rect box = installed_box_rect();

	return Rect{box.right() - K_BROWSE_INSET - K_BROWSE_SIZE, box.center().y - K_BROWSE_SIZE * 0.5f, K_BROWSE_SIZE, K_BROWSE_SIZE};
}

auto SetupScreen::hit_at(Vec2 t_point) const -> SetupScreen::Hit
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
		for (u32 i = 0; i < K_OPTION_COUNT; i += 1) {
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

auto SetupScreen::on_pointer_down(Vec2 t_point) -> bool
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

auto SetupScreen::on_pointer_move(Vec2 t_point) -> bool
{
	if (m_location.is_selecting()) {
		m_location.on_pointer_move(m_fonts->body, field_text_rect(), t_point.x);
	}

	return true;
}

auto SetupScreen::on_pointer_up(Vec2 t_point) -> bool
{
	m_location.on_pointer_up();

	const Hit pressed = std::exchange(m_pressed, Hit::None);
	if (pressed == Hit::None || pressed == Hit::Field || m_outcome || is_busy()) return true;

	if (hit_at(t_point) == pressed) {
		activate(pressed);
	}

	return true;
}

auto SetupScreen::on_key_down(u32 t_key) -> bool
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

auto SetupScreen::on_char(u32 t_character) -> bool
{
	if (m_location.is_focused() && !m_outcome) {
		m_location.on_char(t_character);
		m_field_error = {};
	}

	return true;
}

auto SetupScreen::cursor() const -> CursorKind
{
	if (m_location.is_selecting()) return CursorKind::IBeam;
	if (m_transition < 1.0f || is_busy() || m_picker.is_open()) return CursorKind::Arrow;

	const Hit hit = hit_at(m_mouse);
	if (hit == Hit::Field) return CursorKind::IBeam;

	return hit != Hit::None ? CursorKind::Hand : CursorKind::Arrow;
}

auto SetupScreen::reveal(float t_seconds, u32 t_order) const -> SetupScreen::Reveal
{
	const float amount = std::clamp((t_seconds - static_cast<float>(t_order) * K_STAGGER_SECONDS) / K_REVEAL_SECONDS, 0.0f, 1.0f);
	const float eased  = ease_out(amount);

	return Reveal{eased, (1.0f - eased) * K_REVEAL_RISE};
}

auto SetupScreen::draw_app_icon(DrawList* t_draw_list, Rect t_rect, u8 t_alpha) const -> void
{
	if (m_app_icon != nullptr) {
		t_draw_list->add_image(t_rect, m_app_icon, faded(Color{255, 255, 255, 255}, t_alpha));
	} else {
		t_draw_list->add_image(t_rect, m_assets->get(Asset::IconApp), faded(g_theme.text_dim, t_alpha));
	}
}

auto SetupScreen::draw_header(DrawList* t_draw_list, Page t_page, const PageDraw& t_draw) const -> void
{
	const Font&  heading = m_title_fonts->body;
	const Reveal shown   = reveal(t_draw.seconds, 0);
	const u8     alpha   = to_alpha(t_draw.alpha * shown.alpha);
	const Rect   icon    = header_icon_rect(t_page).moved(Vec2{t_draw.offset.x, t_draw.offset.y + shown.rise});
	const float  text_x  = icon.right() + K_HEADER_GAP;

	draw_app_icon(t_draw_list, icon, alpha);

	draw_text(t_draw_list, heading, Vec2{text_x, heading.centered_baseline(icon)}, K_APP_TITLE, faded(g_theme.text, alpha));
}

auto SetupScreen::draw_installed_location(DrawList* t_draw_list, const PageDraw& t_draw) const -> void
{
	if (!m_installed) return;

	const Font&  body      = m_fonts->body;
	const Font&  secondary = m_fonts->secondary;
	const Reveal shown     = reveal(t_draw.seconds, 1);
	const u8     alpha     = to_alpha(t_draw.alpha * shown.alpha);
	const Vec2   offset{t_draw.offset.x, t_draw.offset.y + shown.rise};
	const Rect   box       = installed_box_rect().moved(offset);
	const Rect   button    = open_folder_rect().moved(offset);
	const float  hover     = m_hover[static_cast<u32>(Hit::OpenFolder)];
	const float  caption_y = box.y - K_CAPTION_GAP - secondary.line_height();
	const float  text_x    = box.x + K_FIELD_PADDING;

	draw_text(t_draw_list, secondary, Vec2{box.x, caption_y + secondary.ascent}, "Installed in", faded(g_theme.text_faint, alpha));
	t_draw_list->add_bordered_rect(box, rounded(K_FIELD_RADIUS), faded(g_theme.field, alpha), faded(g_theme.separator, alpha), 1.0f);
	draw_text_truncated(t_draw_list, body, Vec2{text_x, body.centered_baseline(box)}, to_utf8(m_installed->location), button.x - K_BROWSE_INSET - text_x,
	                    faded(g_theme.text_dim, alpha));

	t_draw_list->add_rounded_rect(button, rounded(K_BROWSE_RADIUS), faded(mix(g_theme.control, g_theme.control_hover, hover), alpha));
	t_draw_list->add_image(button.centered(K_BROWSE_ICON_SIZE, K_BROWSE_ICON_SIZE), m_assets->get(Asset::IconFolderOpen),
	                       faded(mix(g_theme.text_dim, g_theme.text, hover), alpha));
}

auto SetupScreen::draw_choice(DrawList* t_draw_list, u32 t_choice, const PageDraw& t_draw) const -> void
{
	const Font&  body      = m_fonts->body;
	const Font&  secondary = m_fonts->secondary;
	const Color  accent    = m_settings->accent;
	const bool   install   = t_choice == 0;
	const float  hover     = m_hover[static_cast<u32>(install ? Hit::Install : Hit::Portable)];
	const Reveal shown     = reveal(t_draw.seconds, 1 + t_choice);
	const u8     alpha     = to_alpha(t_draw.alpha * shown.alpha);
	const Rect   row       = choice_rect(t_choice).moved(Vec2{t_draw.offset.x, t_draw.offset.y + shown.rise});

	const Color fill   = mix(g_theme.surface, hovered(g_theme.surface), hover);
	const Color border = mix(g_theme.separator, mix(g_theme.border, accent, K_CHOICE_BORDER_ACCENT), hover);
	t_draw_list->add_bordered_rect(row, rounded(K_CHOICE_RADIUS), faded(fill, alpha), faded(border, alpha), 1.0f);

	const Rect  tile{row.x + K_CHOICE_PADDING, snapped_to_pixel(row.center().y - K_CHOICE_TILE_SIZE * 0.5f), K_CHOICE_TILE_SIZE, K_CHOICE_TILE_SIZE};
	const Color tile_fill  = mix(g_theme.control, g_theme.control_hover, hover);
	const Color icon_color = mix(g_theme.text_dim, g_theme.text, hover);

	t_draw_list->add_rounded_rect(tile, rounded(K_CHOICE_TILE_RADIUS), faded(tile_fill, alpha));
	t_draw_list->add_image(tile.centered(K_CHOICE_ICON_SIZE, K_CHOICE_ICON_SIZE), m_assets->get(install ? Asset::IconDownload : Asset::IconFile),
	                       faded(icon_color, alpha));

	const float lines  = body.line_height() + K_CHOICE_LINE_GAP + secondary.line_height();
	const float top    = row.center().y - lines * 0.5f;
	const float text_x = tile.right() + K_CHOICE_PADDING;

	draw_text(t_draw_list, body, Vec2{text_x, top + body.ascent}, install ? "Install" : "Portable", faded(g_theme.text, alpha));
	draw_text(t_draw_list, secondary, Vec2{text_x, top + body.line_height() + K_CHOICE_LINE_GAP + secondary.ascent},
	          install ? "Adds shortcuts and an uninstaller" : "Nothing is added to your system", faded(g_theme.text_faint, alpha));

	const float chevron_x = row.right() - K_CHOICE_PADDING - K_CHEVRON_SIZE.x - K_CHEVRON_NUDGE + K_CHEVRON_NUDGE * hover;
	const float center_y  = row.center().y;
	const Color chevron   = faded(mix(g_theme.text_faint, g_theme.text_dim, hover), alpha);

	t_draw_list->add_line(Vec2{chevron_x, center_y - K_CHEVRON_SIZE.y * 0.5f}, Vec2{chevron_x + K_CHEVRON_SIZE.x, center_y}, K_CHEVRON_THICKNESS, chevron);
	t_draw_list->add_line(Vec2{chevron_x + K_CHEVRON_SIZE.x, center_y}, Vec2{chevron_x, center_y + K_CHEVRON_SIZE.y * 0.5f}, K_CHEVRON_THICKNESS, chevron);
}

auto SetupScreen::draw_location(DrawList* t_draw_list, const PageDraw& t_draw) -> void
{
	const Font& body      = m_fonts->body;
	const Font& secondary = m_fonts->secondary;
	const Font& heading   = m_heading_fonts->body;
	const Color accent    = m_settings->accent;
	const float x         = content_rect().x + t_draw.offset.x;

	const Reveal title       = reveal(t_draw.seconds, 0);
	const u8     title_alpha = to_alpha(t_draw.alpha * title.alpha);
	const float  top         = location_heading_top() + title.rise;

	draw_text(t_draw_list, secondary, Vec2{x, top + secondary.ascent}, "Install", faded(g_theme.text_faint, title_alpha));
	draw_text(t_draw_list, heading, Vec2{x, top + secondary.line_height() + K_CAPTION_GAP + heading.ascent}, "Where should Pulsar go?",
	          faded(g_theme.text, title_alpha));

	const Reveal shown = reveal(t_draw.seconds, 1);
	const u8     alpha = to_alpha(t_draw.alpha * shown.alpha);
	const Vec2   offset{t_draw.offset.x, shown.rise};
	const Rect   field        = field_rect().moved(offset);
	const Rect   browse       = browse_rect().moved(offset);
	const bool   focused      = m_location.is_focused();
	const float  field_hover  = m_hover[static_cast<u32>(Hit::Field)];
	const float  browse_hover = m_hover[static_cast<u32>(Hit::Browse)];

	Color border = mix(g_theme.separator, g_theme.border, field_hover);
	if (!m_field_error.empty()) {
		border = g_theme.error;
	} else if (focused) {
		border = accent;
	}

	t_draw_list->add_bordered_rect(field, rounded(K_FIELD_RADIUS), faded(g_theme.field, alpha), faded(border, alpha), focused ? K_FIELD_FOCUS_BORDER : 1.0f);
	m_location.draw(t_draw_list, body, field_text_rect().moved(offset), faded(g_theme.text, alpha), faded(accent, alpha), std::nullopt);

	t_draw_list->add_rounded_rect(browse, rounded(K_BROWSE_RADIUS), faded(mix(g_theme.control, g_theme.control_hover, browse_hover), alpha));
	t_draw_list->add_image(browse.centered(K_BROWSE_ICON_SIZE, K_BROWSE_ICON_SIZE), m_assets->get(Asset::IconFolder),
	                       faded(mix(g_theme.text_dim, g_theme.text, browse_hover), alpha));

	if (!m_field_error.empty()) {
		draw_text(t_draw_list, secondary, Vec2{field.x + 2.0f, field.bottom() + K_ERROR_GAP + secondary.ascent}, m_field_error, faded(g_theme.error, alpha));
	}
}

auto SetupScreen::draw_check_row(DrawList* t_draw_list, Rect t_row, std::string_view t_label, float t_checked, float t_hover, float t_alpha, Color t_fill) const
	-> void
{
	const Font& body  = m_fonts->body;
	const u8    alpha = to_alpha(t_alpha);
	const Rect  box{t_row.x, snapped_to_pixel(t_row.center().y - K_CHECK_SIZE * 0.5f), K_CHECK_SIZE, K_CHECK_SIZE};
	const Color frame = mix(g_theme.border, mix(g_theme.border, t_fill, K_CHECK_HOVER_ACCENT), t_hover);

	t_draw_list->add_bordered_rect(box, rounded(K_CHECK_RADIUS), faded(g_theme.field, alpha), faded(frame, alpha), K_CHECK_BORDER);

	if (t_checked > 0.001f) {
		const u8    checked_alpha = to_alpha(t_alpha * t_checked);
		const float scale         = 1.0f - K_CHECK_GROW + K_CHECK_GROW * t_checked;

		t_draw_list->add_rounded_rect(box, rounded(K_CHECK_RADIUS), faded(t_fill, checked_alpha));
		controls::draw_check(t_draw_list, m_assets, box.centered(box.w * scale, box.h * scale).inset(K_CHECK_INSET),
		                     faded(foreground_on(t_fill), checked_alpha));
	}

	const Color label = mix(g_theme.text_dim, g_theme.text, std::max(t_hover, t_checked * 0.6f));
	draw_text(t_draw_list, body, Vec2{box.right() + K_CHECK_LABEL_GAP, body.centered_baseline(t_row)}, t_label, faded(label, alpha));
}

auto SetupScreen::draw_options(DrawList* t_draw_list, Page t_page, const PageDraw& t_draw, u32 t_first_order) const -> void
{
	for (u32 i = 0; i < K_OPTION_COUNT; i += 1) {
		const Reveal shown = reveal(t_draw.seconds, t_first_order + i);
		const Rect   row   = option_rect(t_page, i).moved(Vec2{t_draw.offset.x, t_draw.offset.y + shown.rise});

		draw_check_row(t_draw_list, row, K_OPTION_LABELS[i], m_check[i], m_hover[static_cast<u32>(Hit::Option0) + i], t_draw.alpha * shown.alpha,
		               m_settings->accent);
	}
}

auto SetupScreen::draw_confirm(DrawList* t_draw_list, const PageDraw& t_draw) const -> void
{
	const Font&  secondary   = m_fonts->secondary;
	const Font&  heading     = m_heading_fonts->body;
	const Rect   content     = content_rect();
	const float  x           = content.x + t_draw.offset.x;
	const Reveal title       = reveal(t_draw.seconds, 0);
	const u8     title_alpha = to_alpha(t_draw.alpha * title.alpha);
	const float  top         = location_heading_top() + title.rise;

	draw_text(t_draw_list, secondary, Vec2{x, top + secondary.ascent}, "Uninstall", faded(g_theme.text_faint, title_alpha));
	draw_text(t_draw_list, heading, Vec2{x, top + secondary.line_height() + K_CAPTION_GAP + heading.ascent}, "Uninstall Pulsar?",
	          faded(g_theme.text, title_alpha));

	const std::string_view data_line = m_delete_data ? "Your accounts and settings will be deleted too." : "Your accounts and settings stay on this PC.";
	const std::string_view lines[]{"Pulsar and its shortcuts will be removed.", data_line};
	const Color            line_colors[]{g_theme.text_dim, mix(g_theme.text_dim, g_theme.error, m_delete_check)};
	float                  y = location_heading_top() + secondary.line_height() + K_CAPTION_GAP + heading.line_height() + K_BODY_GAP;

	for (u32 i = 0; i < 2; i += 1) {
		const Reveal shown = reveal(t_draw.seconds, 1 + i);
		draw_text_truncated(t_draw_list, secondary, Vec2{x, y + shown.rise + secondary.ascent}, lines[i], content.w,
		                    faded(line_colors[i], to_alpha(t_draw.alpha * shown.alpha)));
		y += secondary.line_height() + K_BODY_LINE_GAP;
	}

	const Reveal shown = reveal(t_draw.seconds, 3);
	draw_check_row(t_draw_list, delete_data_rect().moved(Vec2{t_draw.offset.x, t_draw.offset.y + shown.rise}), K_DELETE_DATA_LABEL, m_delete_check,
	               m_hover[static_cast<u32>(Hit::DeleteData)], t_draw.alpha * shown.alpha, g_theme.error);
}

auto SetupScreen::draw_status(DrawList* t_draw_list, Page t_page, const PageDraw& t_draw) const -> void
{
	const Font& heading   = m_heading_fonts->body;
	const Font& secondary = m_fonts->secondary;
	const Color accent    = m_settings->accent;
	const Vec2  size      = m_window->size();
	const bool  install   = m_job.task() != installation::Task::Uninstall;
	const float bottom    = has_footer(t_page) ? footer_rect().y : size.y;
	const Rect  content   = content_rect();

	std::string_view title;
	std::string_view lines[K_MAX_MESSAGE_LINES];
	u32              line_count = 0;
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
		title  = install ? "Pulsar couldn't be installed" : "Pulsar couldn't be uninstalled";
		detail = m_job.error();
	}

	if (t_page != Page::Working && !detail.empty()) {
		line_count = wrap_text(secondary, detail, content.w, lines);
	}

	const float body_height =
		t_page == Page::Working ? K_PROGRESS_HEIGHT + K_STATUS_TEXT_GAP + secondary.line_height() : static_cast<float>(line_count) * secondary.line_height();
	const float block    = K_STATUS_ICON_SIZE + K_STATUS_ICON_GAP + heading.line_height() + K_STATUS_TEXT_GAP + body_height;
	const float top      = K_TITLE_BAR_HEIGHT + (bottom - K_TITLE_BAR_HEIGHT - block) * 0.5f - K_STATUS_BIAS;
	const float center_x = size.x * 0.5f + t_draw.offset.x;

	const Reveal icon_shown = reveal(t_draw.seconds, 0);
	const float  icon_dim   = t_page == Page::Done && !install ? K_UNINSTALLED_ICON_DIM : 1.0f;
	const Rect   icon{snapped_to_pixel(center_x - K_STATUS_ICON_SIZE * 0.5f), snapped_to_pixel(top + icon_shown.rise), K_STATUS_ICON_SIZE, K_STATUS_ICON_SIZE};
	draw_app_icon(t_draw_list, icon, to_alpha(t_draw.alpha * icon_shown.alpha * icon_dim));

	if (t_page == Page::Failed) {
		const Reveal badge_shown = reveal(t_draw.seconds, 2);
		const float  badge       = K_BADGE_SIZE * (1.0f - K_BADGE_POP + K_BADGE_POP * badge_shown.alpha);
		const Vec2   badge_center{icon.right() - K_BADGE_OFFSET, icon.bottom() - K_BADGE_OFFSET};
		const Rect   circle{badge_center.x - badge * 0.5f, badge_center.y - badge * 0.5f, badge, badge};
		const Color  fill  = g_theme.error;
		const u8     alpha = to_alpha(t_draw.alpha * badge_shown.alpha);

		t_draw_list->add_rounded_rect(circle.inset(-K_BADGE_RING), rounded(badge * 0.5f + K_BADGE_RING), faded(g_theme.window, alpha));
		t_draw_list->add_rounded_rect(circle, rounded(badge * 0.5f), faded(fill, alpha));

		controls::draw_x(t_draw_list, circle.inset(badge * 0.3f), faded(foreground_on(fill), alpha));
	}

	const Reveal title_shown = reveal(t_draw.seconds, 1);
	const float  title_y     = icon.bottom() - icon_shown.rise + K_STATUS_ICON_GAP + title_shown.rise;
	draw_text_centered(t_draw_list, heading, Rect{t_draw.offset.x, title_y, size.x, heading.line_height()}, title,
	                   faded(g_theme.text, to_alpha(t_draw.alpha * title_shown.alpha)));

	const Reveal body_shown = reveal(t_draw.seconds, 2);
	const u8     body_alpha = to_alpha(t_draw.alpha * body_shown.alpha);
	float        y          = title_y - title_shown.rise + heading.line_height() + K_STATUS_TEXT_GAP + body_shown.rise;

	if (t_page == Page::Working) {
		const Rect  track{snapped_to_pixel(center_x - K_PROGRESS_WIDTH * 0.5f), snapped_to_pixel(y), K_PROGRESS_WIDTH, K_PROGRESS_HEIGHT};
		const float filled = std::max(K_PROGRESS_HEIGHT, K_PROGRESS_WIDTH * std::clamp(m_progress, 0.0f, 1.0f));

		t_draw_list->add_rounded_rect(track, rounded(K_PROGRESS_HEIGHT * 0.5f), faded(g_theme.track, body_alpha));
		t_draw_list->add_rounded_rect(Rect{track.x, track.y, filled, K_PROGRESS_HEIGHT}, rounded(K_PROGRESS_HEIGHT * 0.5f), faded(accent, body_alpha));

		y = track.bottom() + K_STATUS_TEXT_GAP;
		draw_text_centered(t_draw_list, secondary, Rect{t_draw.offset.x, y, size.x, secondary.line_height()}, m_job.step_label(),
		                   faded(g_theme.text_faint, body_alpha));
		return;
	}

	for (const std::string_view line : std::span{lines, line_count}) {
		draw_text_centered(t_draw_list, secondary, Rect{t_draw.offset.x, y, size.x, secondary.line_height()}, line, faded(g_theme.text_dim, body_alpha));
		y += secondary.line_height();
	}
}

auto SetupScreen::draw_footer(DrawList* t_draw_list, Page t_page, const PageDraw& t_draw) const -> void
{
	const Font& body   = m_fonts->body;
	const Color accent = m_settings->accent;
	const u8    alpha  = to_alpha(t_draw.alpha);
	const bool  live   = t_page == m_page && m_transition >= 1.0f && !is_busy();
	const Vec2  offset{t_draw.offset.x, 0.0f};

	const std::string_view secondary = secondary_label(t_page);
	if (!secondary.empty()) {
		const auto style          = t_page == Page::Manage ? controls::ButtonStyle::Danger : controls::ButtonStyle::Ghost;
		const bool hovered_button = live && m_hover[static_cast<u32>(Hit::Secondary)] > 0.5f;

		controls::draw_button(t_draw_list, body, footer_button(false, secondary).moved(offset), secondary, style, accent, !m_saving, hovered_button, alpha);
	}

	const std::string_view primary = primary_label(t_page);
	if (!primary.empty()) {
		const auto style          = t_page == Page::ConfirmUninstall ? controls::ButtonStyle::DangerConfirm : controls::ButtonStyle::Accent;
		const bool hovered_button = live && m_hover[static_cast<u32>(Hit::Primary)] > 0.5f;

		controls::draw_button(t_draw_list, body, footer_button(true, primary).moved(offset), primary, style, accent, !m_saving, hovered_button, alpha);
	}
}

auto SetupScreen::draw_page(DrawList* t_draw_list, Page t_page, const PageDraw& t_draw) -> void
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

auto SetupScreen::draw(DrawList* t_draw_list) -> void
{
	const float eased  = ease_out(m_transition);
	const Rect  footer = footer_rect();

	if (m_previous_page && m_transition < 1.0f) {
		draw_page(t_draw_list, *m_previous_page, PageDraw{Vec2{-m_direction * K_SLIDE_DISTANCE * eased, 0.0f}, 1.0f - eased, K_SETTLED_PAGE_SECONDS});
	}

	draw_page(t_draw_list, m_page, PageDraw{Vec2{m_direction * K_SLIDE_DISTANCE * (1.0f - eased), 0.0f}, eased, m_page_seconds});

	float footer_line = has_footer(m_page) ? eased : 0.0f;
	if (m_previous_page && has_footer(*m_previous_page)) {
		footer_line += 1.0f - eased;
	}

	if (footer_line > 0.001f) {
		t_draw_list->add_rect(Rect{0.0f, footer.y, footer.w, 1.0f}, faded(g_theme.separator, to_alpha(footer_line)));
	}
}
