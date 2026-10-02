#include "ui/update_overlay.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <optional>
#include <span>
#include <utility>

#include <Windows.h>
#include <shellapi.h>

#include "core/animation.h"
#include "core/app_identity.h"
#include "core/settings.h"
#include "core/str.h"
#include "gfx/assets.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "platform/window.h"
#include "ui/text.h"
#include "ui/theme.h"

namespace {
constexpr float K_POPOVER_WIDTH   = 380.0f;
constexpr float K_POPOVER_RADIUS  = 12.0f;
constexpr float K_POPOVER_PADDING = 20.0f;
constexpr float K_POPOVER_OFFSET  = 6.0f;
constexpr float K_WINDOW_MARGIN   = 8.0f;
constexpr float K_SLIDE_DISTANCE  = 6.0f;
constexpr float K_CONTENT_WIDTH   = K_POPOVER_WIDTH - K_POPOVER_PADDING * 2.0f;

constexpr float K_TITLE_ICON_SIZE  = 15.0f;
constexpr float K_TITLE_ICON_GAP   = 8.0f;
constexpr float K_TITLE_GAP        = 4.0f;
constexpr float K_SECTION_GAP      = 14.0f;
constexpr float K_PROGRESS_HEIGHT  = 4.0f;
constexpr float K_CAPTION_GAP      = 8.0f;
constexpr float K_BUTTONS_GAP      = 18.0f;
constexpr float K_BUTTON_HEIGHT    = 32.0f;
constexpr float K_BUTTON_PADDING   = 18.0f;
constexpr float K_BUTTON_MIN_WIDTH = 92.0f;
constexpr float K_BUTTON_SPACING   = 8.0f;
constexpr u32   K_MAX_DETAIL_LINES = 3;

constexpr float K_NOTES_PADDING        = 12.0f;
constexpr float K_NOTES_SCROLLBAR_ROOM = 10.0f;
constexpr float K_NOTES_MAX_HEIGHT     = 240.0f;
constexpr float K_NOTES_RADIUS         = 8.0f;
constexpr float K_NOTES_TEXT_WIDTH     = K_CONTENT_WIDTH - K_NOTES_PADDING * 2.0f - K_NOTES_SCROLLBAR_ROOM;
constexpr u32   K_MAX_NOTE_LINES       = 256;
constexpr u32   K_MAX_PARAGRAPH_LINES  = 32;

constexpr usize K_NOTE_HEADING_MAX_LENGTH = 40;
constexpr float K_NOTE_HEADING_GAP_ABOVE  = 12.0f;
constexpr float K_NOTE_HEADING_GAP_BELOW  = 4.0f;
constexpr float K_NOTE_HEADING_RULE_GAP   = 10.0f;
constexpr float K_NOTE_HEADING_ACCENT_MIX = 0.45f;
constexpr float K_NOTE_ITEM_GAP           = 4.0f;
constexpr float K_NOTE_PARAGRAPH_GAP      = 8.0f;
constexpr float K_NOTE_BULLET_INDENT      = 14.0f;
constexpr float K_NOTE_BULLET_SIZE        = 4.0f;
constexpr float K_NOTE_BULLET_OFFSET      = 2.0f;
constexpr float K_NOTE_X_HEIGHT_SHARE     = 0.36f;

constexpr float K_OPEN_EASE_RATE              = 20.0f;
constexpr float K_HEIGHT_EASE_RATE            = 18.0f;
constexpr float K_CONTENT_EASE_RATE           = 12.0f;
constexpr float K_PROGRESS_EASE_RATE          = 10.0f;
constexpr float K_CONTENT_RISE                = 4.0f;
constexpr float K_SPIN_TURNS_PER_SECOND       = 0.9f;
constexpr float K_MINIMUM_CHECKING_SECONDS    = 0.4f;
constexpr float K_UP_TO_DATE_LINGER_SECONDS   = 2.5f;
constexpr float K_CHECK_FAILED_LINGER_SECONDS = 5.0f;

constexpr const wchar_t* K_RELEASES_URL = L"" PULSAR_RELEASE_REPO L"/releases/latest";

enum class NoteKind : u8 {
	Heading,
	Bullet,
	Continuation,
	Text,
};

struct NoteLine {
	std::string_view text;
	NoteKind         kind;
	float            baseline;
};

[[nodiscard]] auto bullet_text(std::string_view t_line) -> std::optional<std::string_view>
{
	constexpr std::string_view MARKERS[]{"- ", "* ", "\xE2\x80\xA2 "};

	for (const std::string_view marker : MARKERS) {
		if (t_line.starts_with(marker)) return trimmed(t_line.substr(marker.size()));
	}

	return std::nullopt;
}

[[nodiscard]] auto next_content_line(std::string_view t_notes, usize t_start) -> std::string_view
{
	while (t_start < t_notes.size()) {
		const usize            end  = std::min(t_notes.find('\n', t_start), t_notes.size());
		const std::string_view line = trimmed(t_notes.substr(t_start, end - t_start));
		if (!line.empty()) return line;

		t_start = end + 1;
	}

	return {};
}

[[nodiscard]] auto is_heading(std::string_view t_line, std::string_view t_next) -> bool
{
	if (t_line.starts_with('#')) return true;
	if (t_line.size() > K_NOTE_HEADING_MAX_LENGTH || bullet_text(t_line) || t_line.ends_with('.')) return false;

	return bullet_text(t_next).has_value();
}

[[nodiscard]] auto heading_text(std::string_view t_line) -> std::string_view
{
	return trimmed(t_line.substr(std::min(t_line.find_first_not_of('#'), t_line.size())));
}

auto layout_notes(const Font& t_font, std::string_view t_notes, std::span<NoteLine> t_out, float* t_out_height) -> u32
{
	u32   count     = 0;
	float y         = 0.0f;
	float blank_gap = 0.0f;

	const auto push = [&](std::string_view t_text, NoteKind t_kind) {
		if (count == t_out.size()) return;

		t_out[count] = NoteLine{t_text, t_kind, y + t_font.ascent};
		count += 1;
		y += t_font.line_height();
	};

	const auto push_wrapped = [&](std::string_view t_text, NoteKind t_first, NoteKind t_rest, float t_width) {
		std::string_view wrapped[K_MAX_PARAGRAPH_LINES];
		const u32        wrapped_count = wrap_text(t_font, t_text, t_width, wrapped);

		for (u32 i = 0; i < wrapped_count; i += 1) {
			push(wrapped[i], i == 0 ? t_first : t_rest);
		}
	};

	usize start = 0;
	while (start <= t_notes.size()) {
		const usize            end  = std::min(t_notes.find('\n', start), t_notes.size());
		const std::string_view line = trimmed(t_notes.substr(start, end - start));
		start                       = end + 1;

		if (line.empty()) {
			blank_gap = count > 0 ? K_NOTE_PARAGRAPH_GAP : 0.0f;
			continue;
		}

		if (is_heading(line, next_content_line(t_notes, start))) {
			y += count > 0 ? K_NOTE_HEADING_GAP_ABOVE : 0.0f;
			push(heading_text(line), NoteKind::Heading);
			y += K_NOTE_HEADING_GAP_BELOW;
		} else if (const std::optional<std::string_view> bullet = bullet_text(line)) {
			y += blank_gap;
			push_wrapped(*bullet, NoteKind::Bullet, NoteKind::Continuation, K_NOTES_TEXT_WIDTH - K_NOTE_BULLET_INDENT);
			y += K_NOTE_ITEM_GAP;
		} else {
			y += blank_gap;
			push_wrapped(line, NoteKind::Text, NoteKind::Text, K_NOTES_TEXT_WIDTH);
			y += K_NOTE_ITEM_GAP;
		}

		blank_gap = 0.0f;
	}

	*t_out_height = count > 0 ? t_out[count - 1].baseline + t_font.descent : 0.0f;

	return count;
}

[[nodiscard]] auto format_size(double t_bytes, const char* t_suffix, char (&t_buffer)[32]) -> std::string_view
{
	constexpr double KILOBYTE = 1024.0;
	constexpr double MEGABYTE = KILOBYTE * KILOBYTE;

	const int written = t_bytes >= MEGABYTE ? std::snprintf(t_buffer, sizeof(t_buffer), "%.1f MB%s", t_bytes / MEGABYTE, t_suffix)
	                                        : std::snprintf(t_buffer, sizeof(t_buffer), "%.0f KB%s", t_bytes / KILOBYTE, t_suffix);

	return std::string_view{t_buffer, static_cast<usize>(std::max(written, 0))};
}

}

UpdateOverlay::UpdateOverlay(Updater* t_updater, const Settings* t_settings, const Fonts* t_fonts, const Assets* t_assets, const Window* t_window)
	: m_updater(t_updater)
	, m_settings(t_settings)
	, m_fonts(t_fonts)
	, m_assets(t_assets)
	, m_window(t_window)
{
}

auto UpdateOverlay::open() -> void
{
	if (!is_shown()) {
		m_height       = 0.0f;
		m_content_fade = 1.0f;
		m_content_key  = content_key();
	}

	m_open = true;
}

auto UpdateOverlay::begin_check() -> void
{
	m_shown_stage     = UpdateStage::Checking;
	m_shown_seconds   = 0.0f;
	m_check_requested = true;
	m_status_linger   = 0.0f;
}

auto UpdateOverlay::close() -> void
{
	m_open            = false;
	m_pressed_outside = false;
	m_notes_scroll.on_pointer_up();
}

auto UpdateOverlay::show_release_notes(std::string_view t_version, std::string_view t_notes) -> void
{
	copy_to(t_version, m_release_version);
	copy_to(t_notes, m_release_notes);
	m_showing_release = true;
	m_notes_scroll    = Scrollable{};
	open();
}

auto UpdateOverlay::shown_notes() const -> std::string_view
{
	return m_showing_release ? std::string_view{m_release_notes} : std::string_view{m_updater->manifest().notes};
}

auto UpdateOverlay::content_key() const -> u32
{
	return m_showing_release ? 0xFFu : static_cast<u32>(m_shown_stage);
}

auto UpdateOverlay::describe() const -> UpdateOverlay::Content
{
	const UpdateStage stage   = m_shown_stage;
	const char*       version = m_updater->manifest().version;
	Content           content;

	const auto set = [&content](const char* t_title, const char* t_detail) {
		copy_to(t_title, content.title);
		copy_to(t_detail, content.detail);
	};

	if (m_showing_release) {
		std::snprintf(content.title, sizeof(content.title), "What's new in %s", m_release_version);
		copy_to("Pulsar was updated. Here's what changed.", content.detail);
		content.notes   = true;
		content.primary = Button{"Got it", Action::Close, controls::ButtonStyle::Accent};
		return content;
	}

	switch (stage) {
		case UpdateStage::Idle:
		case UpdateStage::Checking:
			set("Checking for updates", "This only takes a moment.");
			content.spinning = true;
			break;

		case UpdateStage::UpToDate:
			set("You're up to date", "");
			std::snprintf(content.detail, sizeof(content.detail), "%s %s is the latest version.", K_APP_NAME, K_APP_VERSION);
			content.primary = Button{"Check again", Action::Check, controls::ButtonStyle::Neutral};
			break;

		case UpdateStage::CheckFailed:
			set("Couldn't check for updates", m_updater->error_message());
			content.detail_is_error = true;
			content.primary         = Button{"Try again", Action::Check, controls::ButtonStyle::Accent};
			break;

		case UpdateStage::Available:
			copy_to("Update available", content.title);
			std::snprintf(content.detail, sizeof(content.detail), "Version %s is ready to install.", version);
			content.notes     = true;
			content.secondary = Button{"Later", Action::Close, controls::ButtonStyle::Ghost};
			content.primary   = Button{"Install update", Action::Download, controls::ButtonStyle::Accent};
			break;

		case UpdateStage::ManualUpgradeRequired:
			copy_to("Update available", content.title);
			std::snprintf(content.detail, sizeof(content.detail), "Version %s has to be downloaded from GitHub.", version);
			content.secondary = Button{"Later", Action::Close, controls::ButtonStyle::Ghost};
			content.primary   = Button{"Open GitHub", Action::Releases, controls::ButtonStyle::Accent};
			break;

		case UpdateStage::Downloading:
			std::snprintf(content.title, sizeof(content.title), "Downloading %s", version);
			copy_to("You can keep using Pulsar meanwhile.", content.detail);
			content.progress = true;
			content.primary  = Button{"Cancel", Action::Cancel, controls::ButtonStyle::Neutral};
			break;

		case UpdateStage::Verifying:
			std::snprintf(content.title, sizeof(content.title), "Verifying %s", version);
			copy_to("Making sure the download is genuine.", content.detail);
			content.spinning = true;
			content.progress = true;
			break;

		case UpdateStage::Installing:
			std::snprintf(content.title, sizeof(content.title), "Installing %s", version);
			copy_to("Pulsar restarts when it's done.", content.detail);
			content.spinning = true;
			content.progress = true;
			break;

		case UpdateStage::ReadyToRelaunch:
			set("Restarting", "Pulsar will be right back.");
			content.spinning = true;
			break;

		case UpdateStage::Error:
			set("Update failed", m_updater->error_message());
			content.detail_is_error = true;
			content.secondary       = Button{"Later", Action::Close, controls::ButtonStyle::Ghost};
			content.primary         = Button{"Try again", Action::Download, controls::ButtonStyle::Accent};
			break;

		case UpdateStage::Cancelled:
			set("Update cancelled", "The download was stopped.");
			content.secondary = Button{"Later", Action::Close, controls::ButtonStyle::Ghost};
			content.primary   = Button{"Try again", Action::Download, controls::ButtonStyle::Accent};
			break;
	}

	return content;
}

auto UpdateOverlay::notes_content_height() const -> float
{
	NoteLine lines[K_MAX_NOTE_LINES];
	float    height = 0.0f;
	layout_notes(m_fonts->secondary, shown_notes(), lines, &height);

	return height + K_NOTES_PADDING * 2.0f;
}

auto UpdateOverlay::notes_box_height() const -> float
{
	return std::min(notes_content_height(), K_NOTES_MAX_HEIGHT);
}

auto UpdateOverlay::content_height(const Content& t_content) const -> float
{
	const Font&      body      = m_fonts->body;
	const Font&      secondary = m_fonts->secondary;
	std::string_view lines[K_MAX_DETAIL_LINES];
	const u32        detail_lines = t_content.detail[0] != '\0' ? wrap_text(secondary, t_content.detail, K_CONTENT_WIDTH, lines) : 0;

	float height = K_POPOVER_PADDING + body.line_height();
	height += detail_lines > 0 ? K_TITLE_GAP + static_cast<float>(detail_lines) * secondary.line_height() : 0.0f;

	if (t_content.progress) {
		height += K_SECTION_GAP + K_PROGRESS_HEIGHT + K_CAPTION_GAP + secondary.line_height();
	}

	if (t_content.notes && !shown_notes().empty()) {
		height += K_SECTION_GAP + notes_box_height();
	}

	if (!t_content.primary.label.empty()) {
		height += K_BUTTONS_GAP + K_BUTTON_HEIGHT;
	}

	return height + K_POPOVER_PADDING;
}

auto UpdateOverlay::anchor_rect() const -> Rect
{
	return m_window->title_bar_button_rect(TitleBarButton::Update);
}

auto UpdateOverlay::layout(const Content& t_content) const -> UpdateOverlay::Layout
{
	const Font& body      = m_fonts->body;
	const Font& secondary = m_fonts->secondary;
	const Vec2  window    = m_window->size();
	const float x         = std::clamp(anchor_rect().x, K_WINDOW_MARGIN, std::max(K_WINDOW_MARGIN, window.x - K_WINDOW_MARGIN - K_POPOVER_WIDTH));
	const float y         = K_TITLE_BAR_HEIGHT + K_POPOVER_OFFSET - K_SLIDE_DISTANCE * (1.0f - m_open_amount);

	Layout result{};
	result.popover   = Rect{snapped_to_pixel(x), snapped_to_pixel(y), K_POPOVER_WIDTH, m_height};
	result.title_top = result.popover.y + K_POPOVER_PADDING;

	std::string_view lines[K_MAX_DETAIL_LINES];
	const u32        detail_lines = t_content.detail[0] != '\0' ? wrap_text(secondary, t_content.detail, K_CONTENT_WIDTH, lines) : 0;
	result.detail_top             = result.title_top + body.line_height() + (detail_lines > 0 ? K_TITLE_GAP : 0.0f);

	float       y_cursor = result.detail_top + static_cast<float>(detail_lines) * secondary.line_height();
	const float left     = result.popover.x + K_POPOVER_PADDING;

	if (t_content.progress) {
		result.progress    = Rect{left, snapped_to_pixel(y_cursor + K_SECTION_GAP), K_CONTENT_WIDTH, K_PROGRESS_HEIGHT};
		result.caption_top = result.progress.bottom() + K_CAPTION_GAP;
		y_cursor           = result.caption_top + secondary.line_height();
	}

	if (t_content.notes && !shown_notes().empty()) {
		result.notes = Rect{left, snapped_to_pixel(y_cursor + K_SECTION_GAP), K_CONTENT_WIDTH, notes_box_height()};
		y_cursor     = result.notes.bottom();
	}

	const float buttons_top = snapped_to_pixel(y_cursor + K_BUTTONS_GAP);
	const auto  width_of    = [&body](std::string_view t_label) {
		return std::max(K_BUTTON_MIN_WIDTH, std::ceil(text_width(body, t_label)) + K_BUTTON_PADDING * 2.0f);
	};

	const float primary_width   = t_content.primary.label.empty() ? 0.0f : width_of(t_content.primary.label);
	const float secondary_width = t_content.secondary.label.empty() ? 0.0f : width_of(t_content.secondary.label);
	const float total           = primary_width + secondary_width + (secondary_width > 0.0f ? K_BUTTON_SPACING : 0.0f);
	const float start           = snapped_to_pixel(result.popover.center().x - total * 0.5f);

	if (secondary_width > 0.0f) {
		result.secondary = Rect{start, buttons_top, secondary_width, K_BUTTON_HEIGHT};
	}

	if (primary_width > 0.0f) {
		result.primary = Rect{start + total - primary_width, buttons_top, primary_width, K_BUTTON_HEIGHT};
	}

	return result;
}

auto UpdateOverlay::notes_scroll(const Layout& t_layout) const -> ScrollGeometry
{
	const Rect& box = t_layout.notes;
	const Rect  track{box.right() - K_SCROLLBAR_WIDTH - 2.0f, box.y + 2.0f, K_SCROLLBAR_WIDTH, std::max(0.0f, box.h - 4.0f)};

	return ScrollGeometry{track, notes_content_height(), box.h};
}

// A check that answers instantly would flash past, so the checking state stays up for a moment first.
auto UpdateOverlay::advance_shown_stage(float t_delta_seconds) -> void
{
	const UpdateStage actual = m_updater->stage();
	m_shown_seconds += t_delta_seconds;

	if (m_status_linger > 0.0f) {
		m_status_linger = std::max(0.0f, m_status_linger - t_delta_seconds);
		animation::request_frame_after(m_status_linger);
	}

	if (actual == m_shown_stage) return;

	const bool watched = m_check_requested || is_shown();
	if (watched && m_shown_stage == UpdateStage::Checking && m_shown_seconds < K_MINIMUM_CHECKING_SECONDS) {
		animation::request_frame_after(K_MINIMUM_CHECKING_SECONDS - m_shown_seconds);
		return;
	}

	const bool answered = m_check_requested && m_shown_stage == UpdateStage::Checking;
	m_shown_stage       = actual;
	m_shown_seconds     = 0.0f;

	if (!answered) return;

	// A check someone asked for only opens the popover when there is something to install.
	m_check_requested = false;

	if (actual == UpdateStage::Available || actual == UpdateStage::ManualUpgradeRequired) {
		open();
	} else if (actual == UpdateStage::UpToDate) {
		m_status_linger = K_UP_TO_DATE_LINGER_SECONDS;
	} else if (actual == UpdateStage::CheckFailed) {
		m_status_linger = K_CHECK_FAILED_LINGER_SECONDS;
	}
}

auto UpdateOverlay::update(float t_delta_seconds) -> void
{
	m_open_amount = animation::ease_toward(m_open_amount, m_open ? 1.0f : 0.0f, K_OPEN_EASE_RATE, t_delta_seconds);
	advance_shown_stage(t_delta_seconds);

	if (!m_open && m_open_amount <= 0.01f) {
		m_showing_release = false;
		return;
	}

	const u32 key = content_key();
	if (key != m_content_key) {
		m_content_key  = key;
		m_content_fade = 0.0f;

		if (m_shown_stage == UpdateStage::Downloading) {
			m_progress = 0.0f;
		}
	}

	m_content_fade = animation::ease_toward(m_content_fade, 1.0f, K_CONTENT_EASE_RATE, t_delta_seconds);

	const Content content = describe();
	const float   target  = content_height(content);
	m_height = m_height <= 0.0f ? target : animation::ease_toward(m_height, target, K_HEIGHT_EASE_RATE, t_delta_seconds, animation::K_SETTLED_PIXELS);

	const UpdateStage stage      = m_shown_stage;
	const u64         total      = m_updater->total_bytes();
	const float       downloaded = total > 0 ? static_cast<float>(m_updater->bytes_downloaded()) / static_cast<float>(total) : 0.0f;
	const float       progress   = stage == UpdateStage::Downloading ? downloaded : 1.0f;
	m_progress                   = animation::ease_toward(m_progress, progress, K_PROGRESS_EASE_RATE, t_delta_seconds);

	if (content.spinning) {
		m_spin = std::fmod(m_spin + t_delta_seconds * K_SPIN_TURNS_PER_SECOND * std::numbers::pi_v<float> * 2.0f, std::numbers::pi_v<float> * 2.0f);
		animation::request_frame();
	}

	if (stage == UpdateStage::Checking || stage == UpdateStage::Downloading) {
		animation::request_frame();
	}

	m_notes_scroll.update(t_delta_seconds);
}

auto UpdateOverlay::run(Action t_action) -> void
{
	switch (t_action) {
		case Action::Check:
			m_updater->check_for_update();
			close();
			begin_check();
			break;

		case Action::Download:
			m_updater->start_download();
			break;

		case Action::Cancel:
			m_updater->request_cancel();
			break;

		case Action::Close:
			close();
			break;

		case Action::Releases:
			ShellExecuteW(nullptr, L"open", K_RELEASES_URL, nullptr, nullptr, SW_SHOWNORMAL);
			close();
			break;

		case Action::None:
			break;
	}
}

auto UpdateOverlay::on_pointer_down(Vec2 t_point) -> bool
{
	if (!is_shown()) return false;
	if (!m_open) return true;

	const Content content = describe();
	const Layout  current = layout(content);
	m_pressed_outside     = !current.popover.contains(t_point);

	if (!m_pressed_outside && content.notes && current.notes.h > 0.0f) {
		m_notes_scroll.on_pointer_down(t_point, notes_scroll(current));
	}

	return true;
}

auto UpdateOverlay::on_pointer_move(Vec2 t_point) -> bool
{
	if (!is_shown()) return false;

	if (m_notes_scroll.is_dragging()) {
		m_notes_scroll.on_pointer_move(t_point.y, notes_scroll(layout(describe())));
	}

	return true;
}

auto UpdateOverlay::on_pointer_up(Vec2 t_point) -> bool
{
	if (!is_shown()) return false;
	if (!m_open) return true;

	if (m_notes_scroll.is_dragging()) {
		m_notes_scroll.on_pointer_up();
		return true;
	}

	const Content content = describe();
	const Layout  current = layout(content);

	if (std::exchange(m_pressed_outside, false)) {
		if (!current.popover.contains(t_point)) {
			close();
		}

		return true;
	}

	if (current.primary.w > 0.0f && current.primary.contains(t_point)) {
		run(content.primary.action);
	} else if (current.secondary.w > 0.0f && current.secondary.contains(t_point)) {
		run(content.secondary.action);
	}

	return true;
}

auto UpdateOverlay::on_scroll(Vec2 t_point, float t_wheel_delta) -> bool
{
	if (!is_shown()) return false;

	const Content content = describe();
	const Layout  current = layout(content);

	if (content.notes && current.notes.contains(t_point)) {
		m_notes_scroll.on_scroll(t_wheel_delta, notes_scroll(current));
	}

	return true;
}

auto UpdateOverlay::on_key_down(u32 t_key) -> bool
{
	if (!is_shown()) return false;

	if (t_key == VK_ESCAPE) {
		close();
	}

	return true;
}

auto UpdateOverlay::cursor() const -> CursorKind
{
	if (!is_shown()) return CursorKind::Arrow;
	if (m_notes_scroll.is_dragging()) return CursorKind::Drag;

	const Content content = describe();
	const Layout  current = layout(content);
	const bool    over_button =
		(current.primary.w > 0.0f && current.primary.contains(m_mouse)) || (current.secondary.w > 0.0f && current.secondary.contains(m_mouse));

	return over_button ? CursorKind::Hand : CursorKind::Arrow;
}

auto UpdateOverlay::draw_title(DrawList* t_draw_list, const Content& t_content, Rect t_line, u8 t_alpha) const -> void
{
	const Font& body  = m_fonts->body;
	const Color color = faded(g_theme.text, t_alpha);

	if (!t_content.spinning) {
		draw_text_centered(t_draw_list, body, t_line, t_content.title, color);
		return;
	}

	const float text  = text_width(body, t_content.title);
	const float start = snapped_to_pixel(t_line.center().x - (K_TITLE_ICON_SIZE + K_TITLE_ICON_GAP + text) * 0.5f);
	const Rect  icon{start, t_line.center().y - K_TITLE_ICON_SIZE * 0.5f, K_TITLE_ICON_SIZE, K_TITLE_ICON_SIZE};

	t_draw_list->add_rotated_image(icon, m_spin, m_assets->get(Asset::IconUpdate), faded(g_theme.text_dim, t_alpha));
	draw_text(t_draw_list, body, Vec2{icon.right() + K_TITLE_ICON_GAP, body.centered_baseline(t_line)}, t_content.title, color);
}

auto UpdateOverlay::draw_notes(DrawList* t_draw_list, Rect t_box, const ScrollGeometry& t_scroll, u8 t_alpha) const -> void
{
	const Font& font        = m_fonts->secondary;
	const Color heading     = faded(mix(g_theme.text, m_settings->accent, K_NOTE_HEADING_ACCENT_MIX), t_alpha);
	const float line_height = font.line_height();
	const float left        = t_box.x + K_NOTES_PADDING;
	const float right       = left + K_NOTES_TEXT_WIDTH;
	const float mark_offset = font.ascent * K_NOTE_X_HEIGHT_SHARE;

	t_draw_list->add_bordered_rect(t_box, rounded(K_NOTES_RADIUS), faded(g_theme.field, t_alpha), faded(g_theme.separator, t_alpha), 1.0f);

	NoteLine    lines[K_MAX_NOTE_LINES];
	float       height     = 0.0f;
	const u32   line_count = layout_notes(font, shown_notes(), lines, &height);
	const float top        = t_box.y + K_NOTES_PADDING - m_notes_scroll.offset();

	t_draw_list->push_clip(t_box);

	for (const NoteLine& line : std::span{lines, line_count}) {
		const float baseline = top + line.baseline;
		if (baseline < t_box.y - line_height || baseline > t_box.bottom() + line_height) continue;

		switch (line.kind) {
			case NoteKind::Heading: {
				draw_text(t_draw_list, font, Vec2{left, baseline}, line.text, heading);

				const float rule_x = left + text_width(font, line.text) + K_NOTE_HEADING_RULE_GAP;
				if (rule_x < right) {
					t_draw_list->add_rect(Rect{rule_x, snapped_to_pixel(baseline - mark_offset), right - rule_x, 1.0f}, faded(g_theme.separator, t_alpha));
				}

				break;
			}

			case NoteKind::Bullet: {
				const Rect dot{left + K_NOTE_BULLET_OFFSET, baseline - mark_offset - K_NOTE_BULLET_SIZE * 0.5f, K_NOTE_BULLET_SIZE, K_NOTE_BULLET_SIZE};
				t_draw_list->add_rounded_rect(dot, rounded(K_NOTE_BULLET_SIZE * 0.5f), heading);
				draw_text(t_draw_list, font, Vec2{left + K_NOTE_BULLET_INDENT, baseline}, line.text, faded(g_theme.text, t_alpha));
				break;
			}

			case NoteKind::Continuation:
				draw_text(t_draw_list, font, Vec2{left + K_NOTE_BULLET_INDENT, baseline}, line.text, faded(g_theme.text, t_alpha));
				break;

			case NoteKind::Text:
				draw_text(t_draw_list, font, Vec2{left, baseline}, line.text, faded(g_theme.text_dim, t_alpha));
				break;
		}
	}

	t_draw_list->pop_clip();

	m_notes_scroll.draw_edge_fade(t_draw_list, t_box, t_scroll, faded(g_theme.field, t_alpha));
	m_notes_scroll.draw(t_draw_list, t_scroll, m_mouse, t_alpha);
}

auto UpdateOverlay::draw_progress(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) const -> void
{
	const Font& secondary = m_fonts->secondary;
	const Rect& track     = t_layout.progress;
	const float filled    = std::max(track.h, track.w * std::clamp(m_progress, 0.0f, 1.0f));

	t_draw_list->add_rounded_rect(track, rounded(track.h * 0.5f), faded(g_theme.control, t_alpha));
	t_draw_list->add_rounded_rect(Rect{track.x, track.y, filled, track.h}, rounded(track.h * 0.5f), faded(m_settings->accent, t_alpha));

	if (m_shown_stage != UpdateStage::Downloading) return;

	char                   downloaded_text[32];
	char                   total_text[32];
	char                   speed_text[32];
	const std::string_view downloaded = format_size(static_cast<double>(m_updater->bytes_downloaded()), "", downloaded_text);
	const std::string_view total      = format_size(static_cast<double>(m_updater->total_bytes()), "", total_text);
	const std::string_view speed      = format_size(m_updater->bytes_per_second(), "/s", speed_text);

	char      caption[96];
	const int written = std::snprintf(caption, sizeof(caption), "%.*s of %.*s  \xC2\xB7  %.*s", static_cast<int>(downloaded.size()), downloaded.data(),
	                                  static_cast<int>(total.size()), total.data(), static_cast<int>(speed.size()), speed.data());

	draw_text_centered(t_draw_list, secondary, Rect{t_layout.popover.x, t_layout.caption_top, t_layout.popover.w, secondary.line_height()},
	                   std::string_view{caption, static_cast<usize>(std::max(written, 0))}, faded(g_theme.text_faint, t_alpha));
}

auto UpdateOverlay::draw(DrawList* t_draw_list) -> void
{
	if (!is_shown() || m_height <= 0.0f) return;

	const Font&   body        = m_fonts->body;
	const Font&   secondary   = m_fonts->secondary;
	const Content content     = describe();
	const Layout  current     = layout(content);
	const u8      frame_alpha = to_alpha(m_open_amount);
	const u8      alpha       = to_alpha(m_open_amount * m_content_fade);
	const float   rise        = (1.0f - m_content_fade) * K_CONTENT_RISE;

	controls::draw_popup_shadow(t_draw_list, current.popover, K_POPOVER_RADIUS, m_open_amount);
	t_draw_list->add_bordered_rect(current.popover, rounded(K_POPOVER_RADIUS), faded(g_theme.popup, frame_alpha), faded(g_theme.border, frame_alpha), 1.0f);
	t_draw_list->push_clip(current.popover.inset(1.0f));

	draw_title(t_draw_list, content, Rect{current.popover.x, current.title_top + rise, current.popover.w, body.line_height()}, alpha);

	std::string_view lines[K_MAX_DETAIL_LINES];
	const u32        detail_lines = content.detail[0] != '\0' ? wrap_text(secondary, content.detail, K_CONTENT_WIDTH, lines) : 0;
	const Color      detail_color = content.detail_is_error ? mix(g_theme.text_dim, g_theme.error, 0.7f) : g_theme.text_dim;

	for (u32 i = 0; i < detail_lines; i += 1) {
		const float top = current.detail_top + rise + static_cast<float>(i) * secondary.line_height();
		draw_text_centered(t_draw_list, secondary, Rect{current.popover.x, top, current.popover.w, secondary.line_height()}, lines[i],
		                   faded(detail_color, alpha));
	}

	if (content.progress) {
		Layout shifted      = current;
		shifted.progress    = current.progress.moved(Vec2{0.0f, rise});
		shifted.caption_top = current.caption_top + rise;
		draw_progress(t_draw_list, shifted, alpha);
	}

	if (content.notes && current.notes.h > 0.0f) {
		draw_notes(t_draw_list, current.notes.moved(Vec2{0.0f, rise}), notes_scroll(current), alpha);
	}

	const bool live = m_open && !m_notes_scroll.is_dragging();
	if (current.secondary.w > 0.0f) {
		controls::draw_button(t_draw_list, body, current.secondary.moved(Vec2{0.0f, rise}), content.secondary.label, content.secondary.style,
		                      m_settings->accent, true, live && current.secondary.contains(m_mouse), alpha);
	}

	if (current.primary.w > 0.0f) {
		controls::draw_button(t_draw_list, body, current.primary.moved(Vec2{0.0f, rise}), content.primary.label, content.primary.style, m_settings->accent,
		                      true, live && current.primary.contains(m_mouse), alpha);
	}

	t_draw_list->pop_clip();
}
