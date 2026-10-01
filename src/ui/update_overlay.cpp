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
constexpr float popover_width = 380.0f;
constexpr float popover_radius = 12.0f;
constexpr float popover_padding = 20.0f;
constexpr float popover_offset = 6.0f;
constexpr float window_margin = 8.0f;
constexpr float slide_distance = 6.0f;
constexpr float content_width = popover_width - popover_padding * 2.0f;

constexpr float title_icon_size = 15.0f;
constexpr float title_icon_gap = 8.0f;
constexpr float title_gap = 4.0f;
constexpr float section_gap = 14.0f;
constexpr float progress_height = 4.0f;
constexpr float caption_gap = 8.0f;
constexpr float buttons_gap = 18.0f;
constexpr float button_height = 32.0f;
constexpr float button_padding = 18.0f;
constexpr float button_min_width = 92.0f;
constexpr float button_spacing = 8.0f;
constexpr u32 max_detail_lines = 3;

constexpr float notes_padding = 12.0f;
constexpr float notes_scrollbar_room = 10.0f;
constexpr float notes_max_height = 240.0f;
constexpr float notes_radius = 8.0f;
constexpr float notes_text_width = content_width - notes_padding * 2.0f - notes_scrollbar_room;
constexpr u32 max_note_lines = 256;
constexpr u32 max_paragraph_lines = 32;

constexpr usize note_heading_max_length = 40;
constexpr float note_heading_gap_above = 12.0f;
constexpr float note_heading_gap_below = 4.0f;
constexpr float note_heading_rule_gap = 10.0f;
constexpr float note_heading_accent_mix = 0.45f;
constexpr float note_item_gap = 4.0f;
constexpr float note_paragraph_gap = 8.0f;
constexpr float note_bullet_indent = 14.0f;
constexpr float note_bullet_size = 4.0f;
constexpr float note_bullet_offset = 2.0f;
constexpr float note_x_height_share = 0.36f;

constexpr float open_ease_rate = 20.0f;
constexpr float height_ease_rate = 18.0f;
constexpr float content_ease_rate = 12.0f;
constexpr float progress_ease_rate = 10.0f;
constexpr float content_rise = 4.0f;
constexpr float spin_turns_per_second = 0.9f;
constexpr float minimum_checking_seconds = 0.4f;
constexpr float up_to_date_linger_seconds = 2.5f;
constexpr float check_failed_linger_seconds = 5.0f;

constexpr const wchar_t *releases_url = L"" PULSAR_RELEASE_REPO L"/releases/latest";

enum class NoteKind : u8 {
	Heading,
	Bullet,
	Continuation,
	Text,
};

struct NoteLine {
	std::string_view text;
	NoteKind kind;
	float baseline;
};

std::optional<std::string_view> bullet_text(std::string_view t_line)
{
	constexpr std::string_view markers[]{"- ", "* ", "\xE2\x80\xA2 "};

	for (const std::string_view marker : markers) {
		if (t_line.starts_with(marker)) return trimmed(t_line.substr(marker.size()));
	}

	return std::nullopt;
}

std::string_view next_content_line(std::string_view t_notes, usize t_start)
{
	while (t_start < t_notes.size()) {
		const usize end = std::min(t_notes.find('\n', t_start), t_notes.size());
		const std::string_view line = trimmed(t_notes.substr(t_start, end - t_start));
		if (!line.empty()) return line;

		t_start = end + 1;
	}

	return {};
}

bool is_heading(std::string_view t_line, std::string_view t_next)
{
	if (t_line.starts_with('#')) return true;
	if (t_line.size() > note_heading_max_length || bullet_text(t_line) || t_line.ends_with('.')) return false;

	return bullet_text(t_next).has_value();
}

std::string_view heading_text(std::string_view t_line)
{
	return trimmed(t_line.substr(std::min(t_line.find_first_not_of('#'), t_line.size())));
}

u32 layout_notes(const Font &t_font, std::string_view t_notes, std::span<NoteLine> t_out, float *t_out_height)
{
	u32 count = 0;
	float y = 0.0f;
	float blank_gap = 0.0f;

	const auto push = [&](std::string_view t_text, NoteKind t_kind) {
		if (count == t_out.size()) return;

		t_out[count] = NoteLine{t_text, t_kind, y + t_font.ascent};
		count += 1;
		y += t_font.line_height();
	};

	const auto push_wrapped = [&](std::string_view t_text, NoteKind t_first, NoteKind t_rest, float t_width) {
		std::string_view wrapped[max_paragraph_lines];
		const u32 wrapped_count = wrap_text(t_font, t_text, t_width, wrapped);

		for (u32 i = 0; i < wrapped_count; i += 1) {
			push(wrapped[i], i == 0 ? t_first : t_rest);
		}
	};

	usize start = 0;
	while (start <= t_notes.size()) {
		const usize end = std::min(t_notes.find('\n', start), t_notes.size());
		const std::string_view line = trimmed(t_notes.substr(start, end - start));
		start = end + 1;

		if (line.empty()) {
			blank_gap = count > 0 ? note_paragraph_gap : 0.0f;
			continue;
		}

		if (is_heading(line, next_content_line(t_notes, start))) {
			y += count > 0 ? note_heading_gap_above : 0.0f;
			push(heading_text(line), NoteKind::Heading);
			y += note_heading_gap_below;
		} else if (const std::optional<std::string_view> bullet = bullet_text(line)) {
			y += blank_gap;
			push_wrapped(*bullet, NoteKind::Bullet, NoteKind::Continuation, notes_text_width - note_bullet_indent);
			y += note_item_gap;
		} else {
			y += blank_gap;
			push_wrapped(line, NoteKind::Text, NoteKind::Text, notes_text_width);
			y += note_item_gap;
		}

		blank_gap = 0.0f;
	}

	*t_out_height = count > 0 ? t_out[count - 1].baseline + t_font.descent : 0.0f;

	return count;
}

std::string_view format_size(double t_bytes, const char *t_suffix, char (&t_buffer)[32])
{
	constexpr double kilobyte = 1024.0;
	constexpr double megabyte = kilobyte * kilobyte;

	const int written = t_bytes >= megabyte ? std::snprintf(t_buffer, sizeof(t_buffer), "%.1f MB%s", t_bytes / megabyte, t_suffix)
											: std::snprintf(t_buffer, sizeof(t_buffer), "%.0f KB%s", t_bytes / kilobyte, t_suffix);

	return std::string_view{t_buffer, static_cast<usize>(std::max(written, 0))};
}

}

UpdateOverlay::UpdateOverlay(Updater *t_updater, const Settings *t_settings, const Fonts *t_fonts, const Assets *t_assets, const Window *t_window)
	: m_updater(t_updater)
	, m_settings(t_settings)
	, m_fonts(t_fonts)
	, m_assets(t_assets)
	, m_window(t_window)
{
}

void UpdateOverlay::open()
{
	if (!is_shown()) {
		m_height = 0.0f;
		m_content_fade = 1.0f;
		m_content_key = content_key();
	}

	m_open = true;
}

void UpdateOverlay::begin_check()
{
	m_shown_stage = UpdateStage::Checking;
	m_shown_seconds = 0.0f;
	m_check_requested = true;
	m_status_linger = 0.0f;
}

void UpdateOverlay::close()
{
	m_open = false;
	m_pressed_outside = false;
	m_notes_scroll.on_pointer_up();
}

void UpdateOverlay::show_release_notes(std::string_view t_version, std::string_view t_notes)
{
	copy_to(t_version, m_release_version);
	copy_to(t_notes, m_release_notes);
	m_showing_release = true;
	m_notes_scroll = Scrollable{};
	open();
}

std::string_view UpdateOverlay::shown_notes() const
{
	return m_showing_release ? std::string_view{m_release_notes} : std::string_view{m_updater->manifest().notes};
}

u32 UpdateOverlay::content_key() const
{
	return m_showing_release ? 0xFFu : static_cast<u32>(m_shown_stage);
}

UpdateOverlay::Content UpdateOverlay::describe() const
{
	const UpdateStage stage = m_shown_stage;
	const char *version = m_updater->manifest().version;
	Content content;

	const auto set = [&content](const char *t_title, const char *t_detail) {
		copy_to(t_title, content.title);
		copy_to(t_detail, content.detail);
	};

	if (m_showing_release) {
		std::snprintf(content.title, sizeof(content.title), "What's new in %s", m_release_version);
		copy_to("Pulsar was updated. Here's what changed.", content.detail);
		content.notes = true;
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
			std::snprintf(content.detail, sizeof(content.detail), "%s %s is the latest version.", app_name, app_version);
			content.primary = Button{"Check again", Action::Check, controls::ButtonStyle::Neutral};
			break;

		case UpdateStage::CheckFailed:
			set("Couldn't check for updates", m_updater->error_message());
			content.detail_is_error = true;
			content.primary = Button{"Try again", Action::Check, controls::ButtonStyle::Accent};
			break;

		case UpdateStage::Available:
			copy_to("Update available", content.title);
			std::snprintf(content.detail, sizeof(content.detail), "Version %s is ready to install.", version);
			content.notes = true;
			content.secondary = Button{"Later", Action::Close, controls::ButtonStyle::Ghost};
			content.primary = Button{"Install update", Action::Download, controls::ButtonStyle::Accent};
			break;

		case UpdateStage::ManualUpgradeRequired:
			copy_to("Update available", content.title);
			std::snprintf(content.detail, sizeof(content.detail), "Version %s has to be downloaded from GitHub.", version);
			content.secondary = Button{"Later", Action::Close, controls::ButtonStyle::Ghost};
			content.primary = Button{"Open GitHub", Action::Releases, controls::ButtonStyle::Accent};
			break;

		case UpdateStage::Downloading:
			std::snprintf(content.title, sizeof(content.title), "Downloading %s", version);
			copy_to("You can keep using Pulsar meanwhile.", content.detail);
			content.progress = true;
			content.primary = Button{"Cancel", Action::Cancel, controls::ButtonStyle::Neutral};
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
			content.secondary = Button{"Later", Action::Close, controls::ButtonStyle::Ghost};
			content.primary = Button{"Try again", Action::Download, controls::ButtonStyle::Accent};
			break;

		case UpdateStage::Cancelled:
			set("Update cancelled", "The download was stopped.");
			content.secondary = Button{"Later", Action::Close, controls::ButtonStyle::Ghost};
			content.primary = Button{"Try again", Action::Download, controls::ButtonStyle::Accent};
			break;
	}

	return content;
}

float UpdateOverlay::notes_content_height() const
{
	NoteLine lines[max_note_lines];
	float height = 0.0f;
	layout_notes(m_fonts->secondary, shown_notes(), lines, &height);

	return height + notes_padding * 2.0f;
}

float UpdateOverlay::notes_box_height() const
{
	return std::min(notes_content_height(), notes_max_height);
}

float UpdateOverlay::content_height(const Content &t_content) const
{
	const Font &body = m_fonts->body;
	const Font &secondary = m_fonts->secondary;
	std::string_view lines[max_detail_lines];
	const u32 detail_lines = t_content.detail[0] != '\0' ? wrap_text(secondary, t_content.detail, content_width, lines) : 0;

	float height = popover_padding + body.line_height();
	height += detail_lines > 0 ? title_gap + static_cast<float>(detail_lines) * secondary.line_height() : 0.0f;

	if (t_content.progress) {
		height += section_gap + progress_height + caption_gap + secondary.line_height();
	}

	if (t_content.notes && !shown_notes().empty()) {
		height += section_gap + notes_box_height();
	}

	if (!t_content.primary.label.empty()) {
		height += buttons_gap + button_height;
	}

	return height + popover_padding;
}

Rect UpdateOverlay::anchor_rect() const
{
	return m_window->title_bar_button_rect(TitleBarButton::Update);
}

UpdateOverlay::Layout UpdateOverlay::layout(const Content &t_content) const
{
	const Font &body = m_fonts->body;
	const Font &secondary = m_fonts->secondary;
	const Vec2 window = m_window->size();
	const float x = std::clamp(anchor_rect().x, window_margin, std::max(window_margin, window.x - window_margin - popover_width));
	const float y = title_bar_height + popover_offset - slide_distance * (1.0f - m_open_amount);

	Layout result{};
	result.popover = Rect{snapped_to_pixel(x), snapped_to_pixel(y), popover_width, m_height};
	result.title_top = result.popover.y + popover_padding;

	std::string_view lines[max_detail_lines];
	const u32 detail_lines = t_content.detail[0] != '\0' ? wrap_text(secondary, t_content.detail, content_width, lines) : 0;
	result.detail_top = result.title_top + body.line_height() + (detail_lines > 0 ? title_gap : 0.0f);

	float y_cursor = result.detail_top + static_cast<float>(detail_lines) * secondary.line_height();
	const float left = result.popover.x + popover_padding;

	if (t_content.progress) {
		result.progress = Rect{left, snapped_to_pixel(y_cursor + section_gap), content_width, progress_height};
		result.caption_top = result.progress.bottom() + caption_gap;
		y_cursor = result.caption_top + secondary.line_height();
	}

	if (t_content.notes && !shown_notes().empty()) {
		result.notes = Rect{left, snapped_to_pixel(y_cursor + section_gap), content_width, notes_box_height()};
		y_cursor = result.notes.bottom();
	}

	const float buttons_top = snapped_to_pixel(y_cursor + buttons_gap);
	const auto width_of = [&body](std::string_view t_label) {
		return std::max(button_min_width, std::ceil(text_width(body, t_label)) + button_padding * 2.0f);
	};

	const float primary_width = t_content.primary.label.empty() ? 0.0f : width_of(t_content.primary.label);
	const float secondary_width = t_content.secondary.label.empty() ? 0.0f : width_of(t_content.secondary.label);
	const float total = primary_width + secondary_width + (secondary_width > 0.0f ? button_spacing : 0.0f);
	const float start = snapped_to_pixel(result.popover.center().x - total * 0.5f);

	if (secondary_width > 0.0f) {
		result.secondary = Rect{start, buttons_top, secondary_width, button_height};
	}

	if (primary_width > 0.0f) {
		result.primary = Rect{start + total - primary_width, buttons_top, primary_width, button_height};
	}

	return result;
}

ScrollGeometry UpdateOverlay::notes_scroll(const Layout &t_layout) const
{
	const Rect &box = t_layout.notes;
	const Rect track{box.right() - scrollbar_width - 2.0f, box.y + 2.0f, scrollbar_width, std::max(0.0f, box.h - 4.0f)};

	return ScrollGeometry{track, notes_content_height(), box.h};
}

// A check that answers instantly would flash past, so the checking state stays up for a moment first.
void UpdateOverlay::advance_shown_stage(float t_delta_seconds)
{
	const UpdateStage actual = m_updater->stage();
	m_shown_seconds += t_delta_seconds;

	if (m_status_linger > 0.0f) {
		m_status_linger = std::max(0.0f, m_status_linger - t_delta_seconds);
		animation::request_frame_after(m_status_linger);
	}

	if (actual == m_shown_stage) return;

	const bool watched = m_check_requested || is_shown();
	if (watched && m_shown_stage == UpdateStage::Checking && m_shown_seconds < minimum_checking_seconds) {
		animation::request_frame_after(minimum_checking_seconds - m_shown_seconds);
		return;
	}

	const bool answered = m_check_requested && m_shown_stage == UpdateStage::Checking;
	m_shown_stage = actual;
	m_shown_seconds = 0.0f;

	if (!answered) return;

	// A check someone asked for only opens the popover when there is something to install.
	m_check_requested = false;

	if (actual == UpdateStage::Available || actual == UpdateStage::ManualUpgradeRequired) {
		open();
	} else if (actual == UpdateStage::UpToDate) {
		m_status_linger = up_to_date_linger_seconds;
	} else if (actual == UpdateStage::CheckFailed) {
		m_status_linger = check_failed_linger_seconds;
	}
}

void UpdateOverlay::update(float t_delta_seconds)
{
	m_open_amount = animation::ease_toward(m_open_amount, m_open ? 1.0f : 0.0f, open_ease_rate, t_delta_seconds);
	advance_shown_stage(t_delta_seconds);

	if (!m_open && m_open_amount <= 0.01f) {
		m_showing_release = false;
		return;
	}

	const u32 key = content_key();
	if (key != m_content_key) {
		m_content_key = key;
		m_content_fade = 0.0f;

		if (m_shown_stage == UpdateStage::Downloading) {
			m_progress = 0.0f;
		}
	}

	m_content_fade = animation::ease_toward(m_content_fade, 1.0f, content_ease_rate, t_delta_seconds);

	const Content content = describe();
	const float target = content_height(content);
	m_height = m_height <= 0.0f ? target : animation::ease_toward(m_height, target, height_ease_rate, t_delta_seconds, animation::settled_pixels);

	const UpdateStage stage = m_shown_stage;
	const u64 total = m_updater->total_bytes();
	const float downloaded = total > 0 ? static_cast<float>(m_updater->bytes_downloaded()) / static_cast<float>(total) : 0.0f;
	const float progress = stage == UpdateStage::Downloading ? downloaded : 1.0f;
	m_progress = animation::ease_toward(m_progress, progress, progress_ease_rate, t_delta_seconds);

	if (content.spinning) {
		m_spin = std::fmod(m_spin + t_delta_seconds * spin_turns_per_second * std::numbers::pi_v<float> * 2.0f, std::numbers::pi_v<float> * 2.0f);
		animation::request_frame();
	}

	if (stage == UpdateStage::Checking || stage == UpdateStage::Downloading) {
		animation::request_frame();
	}

	m_notes_scroll.update(t_delta_seconds);
}

void UpdateOverlay::run(Action t_action)
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
			ShellExecuteW(nullptr, L"open", releases_url, nullptr, nullptr, SW_SHOWNORMAL);
			close();
			break;

		case Action::None:
			break;
	}
}

bool UpdateOverlay::on_pointer_down(Vec2 t_point)
{
	if (!is_shown()) return false;
	if (!m_open) return true;

	const Content content = describe();
	const Layout current = layout(content);
	m_pressed_outside = !current.popover.contains(t_point);

	if (!m_pressed_outside && content.notes && current.notes.h > 0.0f) {
		m_notes_scroll.on_pointer_down(t_point, notes_scroll(current));
	}

	return true;
}

bool UpdateOverlay::on_pointer_move(Vec2 t_point)
{
	if (!is_shown()) return false;

	if (m_notes_scroll.is_dragging()) {
		m_notes_scroll.on_pointer_move(t_point.y, notes_scroll(layout(describe())));
	}

	return true;
}

bool UpdateOverlay::on_pointer_up(Vec2 t_point)
{
	if (!is_shown()) return false;
	if (!m_open) return true;

	if (m_notes_scroll.is_dragging()) {
		m_notes_scroll.on_pointer_up();
		return true;
	}

	const Content content = describe();
	const Layout current = layout(content);

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

bool UpdateOverlay::on_scroll(Vec2 t_point, float t_wheel_delta)
{
	if (!is_shown()) return false;

	const Content content = describe();
	const Layout current = layout(content);

	if (content.notes && current.notes.contains(t_point)) {
		m_notes_scroll.on_scroll(t_wheel_delta, notes_scroll(current));
	}

	return true;
}

bool UpdateOverlay::on_key_down(u32 t_key)
{
	if (!is_shown()) return false;

	if (t_key == VK_ESCAPE) {
		close();
	}

	return true;
}

CursorKind UpdateOverlay::cursor() const
{
	if (!is_shown()) return CursorKind::Arrow;
	if (m_notes_scroll.is_dragging()) return CursorKind::Drag;

	const Content content = describe();
	const Layout current = layout(content);
	const bool over_button =
		(current.primary.w > 0.0f && current.primary.contains(m_mouse)) || (current.secondary.w > 0.0f && current.secondary.contains(m_mouse));

	return over_button ? CursorKind::Hand : CursorKind::Arrow;
}

void UpdateOverlay::draw_title(DrawList *t_draw_list, const Content &t_content, Rect t_line, u8 t_alpha) const
{
	const Font &body = m_fonts->body;
	const Color color = faded(theme().text, t_alpha);

	if (!t_content.spinning) {
		draw_text_centered(t_draw_list, body, t_line, t_content.title, color);
		return;
	}

	const float text = text_width(body, t_content.title);
	const float start = snapped_to_pixel(t_line.center().x - (title_icon_size + title_icon_gap + text) * 0.5f);
	const Rect icon{start, t_line.center().y - title_icon_size * 0.5f, title_icon_size, title_icon_size};

	t_draw_list->add_rotated_image(icon, m_spin, m_assets->get(Asset::IconUpdate), faded(theme().text_dim, t_alpha));
	draw_text(t_draw_list, body, Vec2{icon.right() + title_icon_gap, body.centered_baseline(t_line)}, t_content.title, color);
}

void UpdateOverlay::draw_notes(DrawList *t_draw_list, Rect t_box, const ScrollGeometry &t_scroll, u8 t_alpha) const
{
	const Font &font = m_fonts->secondary;
	const Theme &colors = theme();
	const Color heading = faded(mix(colors.text, m_settings->accent, note_heading_accent_mix), t_alpha);
	const float line_height = font.line_height();
	const float left = t_box.x + notes_padding;
	const float right = left + notes_text_width;
	const float mark_offset = font.ascent * note_x_height_share;

	t_draw_list->add_bordered_rect(t_box, rounded(notes_radius), faded(colors.field, t_alpha), faded(colors.separator, t_alpha), 1.0f);

	NoteLine lines[max_note_lines];
	float height = 0.0f;
	const u32 line_count = layout_notes(font, shown_notes(), lines, &height);
	const float top = t_box.y + notes_padding - m_notes_scroll.offset();

	t_draw_list->push_clip(t_box);

	for (const NoteLine &line : std::span{lines, line_count}) {
		const float baseline = top + line.baseline;
		if (baseline < t_box.y - line_height || baseline > t_box.bottom() + line_height) continue;

		switch (line.kind) {
			case NoteKind::Heading: {
				draw_text(t_draw_list, font, Vec2{left, baseline}, line.text, heading);

				const float rule_x = left + text_width(font, line.text) + note_heading_rule_gap;
				if (rule_x < right) {
					t_draw_list->add_rect(Rect{rule_x, snapped_to_pixel(baseline - mark_offset), right - rule_x, 1.0f}, faded(colors.separator, t_alpha));
				}

				break;
			}

			case NoteKind::Bullet: {
				const Rect dot{left + note_bullet_offset, baseline - mark_offset - note_bullet_size * 0.5f, note_bullet_size, note_bullet_size};
				t_draw_list->add_rounded_rect(dot, rounded(note_bullet_size * 0.5f), heading);
				draw_text(t_draw_list, font, Vec2{left + note_bullet_indent, baseline}, line.text, faded(colors.text, t_alpha));
				break;
			}

			case NoteKind::Continuation:
				draw_text(t_draw_list, font, Vec2{left + note_bullet_indent, baseline}, line.text, faded(colors.text, t_alpha));
				break;

			case NoteKind::Text:
				draw_text(t_draw_list, font, Vec2{left, baseline}, line.text, faded(colors.text_dim, t_alpha));
				break;
		}
	}

	t_draw_list->pop_clip();

	m_notes_scroll.draw_edge_fade(t_draw_list, t_box, t_scroll, faded(colors.field, t_alpha));
	m_notes_scroll.draw(t_draw_list, t_scroll, m_mouse, t_alpha);
}

void UpdateOverlay::draw_progress(DrawList *t_draw_list, const Layout &t_layout, u8 t_alpha) const
{
	const Theme &colors = theme();
	const Font &secondary = m_fonts->secondary;
	const Rect &track = t_layout.progress;
	const float filled = std::max(track.h, track.w * std::clamp(m_progress, 0.0f, 1.0f));

	t_draw_list->add_rounded_rect(track, rounded(track.h * 0.5f), faded(colors.control, t_alpha));
	t_draw_list->add_rounded_rect(Rect{track.x, track.y, filled, track.h}, rounded(track.h * 0.5f), faded(m_settings->accent, t_alpha));

	if (m_shown_stage != UpdateStage::Downloading) return;

	char downloaded_text[32];
	char total_text[32];
	char speed_text[32];
	const std::string_view downloaded = format_size(static_cast<double>(m_updater->bytes_downloaded()), "", downloaded_text);
	const std::string_view total = format_size(static_cast<double>(m_updater->total_bytes()), "", total_text);
	const std::string_view speed = format_size(m_updater->bytes_per_second(), "/s", speed_text);

	char caption[96];
	const int written = std::snprintf(caption, sizeof(caption), "%.*s of %.*s  \xC2\xB7  %.*s", static_cast<int>(downloaded.size()), downloaded.data(),
									  static_cast<int>(total.size()), total.data(), static_cast<int>(speed.size()), speed.data());

	draw_text_centered(t_draw_list, secondary, Rect{t_layout.popover.x, t_layout.caption_top, t_layout.popover.w, secondary.line_height()},
					   std::string_view{caption, static_cast<usize>(std::max(written, 0))}, faded(colors.text_faint, t_alpha));
}

void UpdateOverlay::draw(DrawList *t_draw_list)
{
	if (!is_shown() || m_height <= 0.0f) return;

	const Theme &colors = theme();
	const Font &body = m_fonts->body;
	const Font &secondary = m_fonts->secondary;
	const Content content = describe();
	const Layout current = layout(content);
	const u8 frame_alpha = to_alpha(m_open_amount);
	const u8 alpha = to_alpha(m_open_amount * m_content_fade);
	const float rise = (1.0f - m_content_fade) * content_rise;

	controls::draw_popup_shadow(t_draw_list, current.popover, popover_radius, m_open_amount);
	t_draw_list->add_bordered_rect(current.popover, rounded(popover_radius), faded(colors.popup, frame_alpha), faded(colors.border, frame_alpha), 1.0f);
	t_draw_list->push_clip(current.popover.inset(1.0f));

	draw_title(t_draw_list, content, Rect{current.popover.x, current.title_top + rise, current.popover.w, body.line_height()}, alpha);

	std::string_view lines[max_detail_lines];
	const u32 detail_lines = content.detail[0] != '\0' ? wrap_text(secondary, content.detail, content_width, lines) : 0;
	const Color detail_color = content.detail_is_error ? mix(colors.text_dim, colors.error, 0.7f) : colors.text_dim;

	for (u32 i = 0; i < detail_lines; i += 1) {
		const float top = current.detail_top + rise + static_cast<float>(i) * secondary.line_height();
		draw_text_centered(t_draw_list, secondary, Rect{current.popover.x, top, current.popover.w, secondary.line_height()}, lines[i],
						   faded(detail_color, alpha));
	}

	if (content.progress) {
		Layout shifted = current;
		shifted.progress = current.progress.moved(Vec2{0.0f, rise});
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
