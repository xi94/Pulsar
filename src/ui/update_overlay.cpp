#include "ui/update_overlay.h"

#include <algorithm>
#include <cstdio>
#include <optional>
#include <span>

#include "core/app_identity.h"
#include "core/animation.h"
#include "core/settings.h"
#include "core/str.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "platform/window.h"
#include "ui/controls.h"
#include "ui/text.h"
#include "ui/theme.h"

namespace {
constexpr float card_width = 460.0f;
constexpr float card_height = 300.0f;
constexpr float card_radius = 16.0f;
constexpr float card_padding = 28.0f;
constexpr float button_height = 40.0f;
constexpr float gap = 14.0f;
constexpr float progress_height = 10.0f;
constexpr float progress_glow = 3.0f;
constexpr float close_size = 28.0f;
constexpr float close_margin = 14.0f;
constexpr float card_margin = 24.0f;
constexpr float notes_padding = 14.0f;
constexpr float notes_scrollbar_room = 14.0f;
constexpr float notes_max_height = 320.0f;
constexpr float notes_text_width = card_width - card_padding * 2.0f - notes_padding - notes_scrollbar_room;
constexpr u32 max_note_lines = 256;
constexpr u32 max_paragraph_lines = 32;

constexpr usize note_heading_max_length = 40;
constexpr float note_heading_gap_above = 14.0f;
constexpr float note_heading_gap_below = 6.0f;
constexpr float note_heading_rule_gap = 10.0f;
constexpr float note_heading_accent_mix = 0.45f;
constexpr float note_item_gap = 5.0f;
constexpr float note_paragraph_gap = 8.0f;
constexpr float note_bullet_indent = 16.0f;
constexpr float note_bullet_size = 4.0f;
constexpr float note_bullet_offset = 3.0f;
constexpr float note_x_height_share = 0.36f;
constexpr u32 max_error_lines = 4;

constexpr u8 card_top_edge_alpha = 90;

bool can_dismiss(UpdateStage t_stage)
{
	return t_stage != UpdateStage::downloading && t_stage != UpdateStage::verifying &&
		   t_stage != UpdateStage::installing;
}

bool has_primary_action(UpdateStage t_stage)
{
	switch (t_stage) {
		case UpdateStage::available:
		case UpdateStage::downloading:
		case UpdateStage::error:
		case UpdateStage::cancelled:
		case UpdateStage::up_to_date:
		case UpdateStage::check_failed:
			return true;
		default:
			return false;
	}
}

enum class NoteKind : u8 {
	heading,
	bullet,
	continuation,
	text,
};

struct NoteLine {
	std::string_view text;
	NoteKind kind;
	float baseline;
};

std::string_view trimmed(std::string_view t_text)
{
	constexpr std::string_view blank = " \t\r";

	const usize first = t_text.find_first_not_of(blank);
	if (first == std::string_view::npos) return {};

	return t_text.substr(first, t_text.find_last_not_of(blank) - first + 1);
}

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

u32 layout_notes(const Font &t_font, std::string_view t_notes, std::span<NoteLine> t_out, float &t_out_height)
{
	u32 count = 0;
	float y = 0.0f;
	float blank_gap = 0.0f;

	const auto push = [&](std::string_view t_text, NoteKind t_kind) {
		if (count == t_out.size()) return;

		t_out[count] = NoteLine{t_text, t_kind, y + t_font.ascent()};
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
			push(heading_text(line), NoteKind::heading);
			y += note_heading_gap_below;
		} else if (const std::optional<std::string_view> bullet = bullet_text(line)) {
			y += blank_gap;
			push_wrapped(*bullet, NoteKind::bullet, NoteKind::continuation, notes_text_width - note_bullet_indent);
			y += note_item_gap;
		} else {
			y += blank_gap;
			push_wrapped(line, NoteKind::text, NoteKind::text, notes_text_width);
			y += note_item_gap;
		}

		blank_gap = 0.0f;
	}

	t_out_height = count > 0 ? t_out[count - 1].baseline + t_font.descent() : 0.0f;

	return count;
}

std::string_view format_size(double t_bytes, const char *t_suffix, char (&t_buffer)[32])
{
	constexpr double kilobyte = 1024.0;
	constexpr double megabyte = kilobyte * kilobyte;

	const int written = t_bytes >= megabyte
							? std::snprintf(t_buffer, sizeof(t_buffer), "%.1f MB%s", t_bytes / megabyte, t_suffix)
							: std::snprintf(t_buffer, sizeof(t_buffer), "%.0f KB%s", t_bytes / kilobyte, t_suffix);

	return std::string_view{t_buffer, static_cast<usize>(std::max(written, 0))};
}
}

UpdateOverlay::UpdateOverlay(Updater &t_updater, const Settings &t_settings, const Fonts &t_fonts,
							 const Window &t_window)
	: m_updater(t_updater)
	, m_settings(t_settings)
	, m_fonts(t_fonts)
	, m_window(t_window)
{
}

void UpdateOverlay::open()
{
	m_open = true;
}

void UpdateOverlay::close()
{
	m_open = false;
	m_showing_release = false;
}

void UpdateOverlay::show_release_notes(std::string_view t_version, std::string_view t_notes)
{
	copy_to(t_version, m_release_version);
	copy_to(t_notes, m_release_notes);
	m_showing_release = true;
	m_notes_scroll = Scrollable{};
	m_open = true;
}

std::string_view UpdateOverlay::shown_notes() const
{
	return m_showing_release ? std::string_view{m_release_notes} : std::string_view{m_updater.manifest().notes};
}

bool UpdateOverlay::has_notes() const
{
	return m_showing_release || m_updater.stage() == UpdateStage::available;
}

void UpdateOverlay::update(float t_delta_seconds)
{
	m_notes_scroll.update(t_delta_seconds);

	switch (m_updater.stage()) {
		case UpdateStage::checking:
		case UpdateStage::downloading:
		case UpdateStage::verifying:
		case UpdateStage::installing:
			if (m_open) {
				animation::request_frame();
			}
			break;
		default:
			break;
	}
}

Rect UpdateOverlay::card_rect() const
{
	const Vec2 window = m_window.size();
	const Rect client{0.0f, 0.0f, window.x, window.y};
	if (!has_notes()) return client.centered(card_width, card_height);

	const float wanted = notes_chrome_height() + std::min(notes_content_height(), notes_max_height);
	const float height = std::max(card_height, std::min(wanted, window.y - card_margin * 2.0f));

	return client.centered(card_width, height);
}

float UpdateOverlay::notes_chrome_height() const
{
	const Font &body = m_fonts.body();
	const Font &secondary = m_fonts.secondary();

	return card_padding * 2.0f + body.ascent() + body.line_height() + 10.0f + secondary.ascent() + secondary.descent() +
		   gap * 2.0f + button_height;
}

float UpdateOverlay::notes_content_height() const
{
	NoteLine lines[max_note_lines];
	float height = 0.0f;
	layout_notes(m_fonts.secondary(), shown_notes(), lines, height);

	return height + notes_padding * 2.0f;
}

Rect UpdateOverlay::close_button_rect() const
{
	const Rect card = card_rect();

	return Rect{card.right() - close_size - close_margin, card.y + close_margin, close_size, close_size};
}

Rect UpdateOverlay::primary_button_rect() const
{
	const Rect card = card_rect();

	return Rect{card.x + card_padding, card.bottom() - card_padding - button_height, card.w - card_padding * 2.0f,
				button_height};
}

float UpdateOverlay::text_column_x() const
{
	return card_rect().x + card_padding;
}

float UpdateOverlay::text_column_width() const
{
	return card_rect().w - card_padding * 2.0f;
}

UpdateOverlay::Notes UpdateOverlay::notes() const
{
	const Font &body = m_fonts.body();
	const Font &secondary = m_fonts.secondary();
	const Rect card = card_rect();

	const float subtitle_baseline =
		card.y + card_padding + body.ascent() + body.line_height() + 10.0f + secondary.ascent();
	const float top = subtitle_baseline + secondary.descent() + gap;
	const Rect box{card.x + card_padding, top, card.w - card_padding * 2.0f, primary_button_rect().y - gap - top};
	const Rect track{box.right() - scrollbar_width, box.y, scrollbar_width, box.h};

	return Notes{box, ScrollGeometry{track, notes_content_height(), box.h}};
}

bool UpdateOverlay::on_pointer_down(Vec2 t_point)
{
	if (!m_open) return false;

	if (has_notes()) {
		m_notes_scroll.on_pointer_down(t_point, notes().scroll);
	}

	return true;
}

bool UpdateOverlay::on_pointer_move(Vec2 t_point)
{
	if (!m_open) return false;

	if (m_notes_scroll.is_dragging()) {
		m_notes_scroll.on_pointer_move(t_point.y, notes().scroll);
	}

	return true;
}

bool UpdateOverlay::on_pointer_up(Vec2 t_point)
{
	if (!m_open) return false;

	if (m_notes_scroll.is_dragging()) {
		m_notes_scroll.on_pointer_up();
		return true;
	}

	const UpdateStage stage = m_updater.stage();

	if ((m_showing_release || can_dismiss(stage)) && close_button_rect().contains(t_point)) {
		close();
		return true;
	}

	if (m_showing_release) {
		if (primary_button_rect().contains(t_point)) {
			close();
		}

		return true;
	}

	if (!primary_button_rect().contains(t_point)) return true;

	switch (stage) {
		case UpdateStage::available:
		case UpdateStage::error:
		case UpdateStage::cancelled:
			m_updater.start_download();
			break;

		case UpdateStage::downloading:
			m_updater.request_cancel();
			break;

		case UpdateStage::up_to_date:
		case UpdateStage::check_failed:
			m_updater.check_for_update();
			break;

		default:
			break;
	}

	return true;
}

bool UpdateOverlay::on_scroll(Vec2 t_point, float t_wheel_delta)
{
	if (!m_open) return false;

	if (has_notes()) {
		const Notes current = notes();

		if (current.box.contains(t_point)) {
			m_notes_scroll.on_scroll(t_wheel_delta, current.scroll);
		}
	}

	return true;
}

CursorKind UpdateOverlay::cursor() const
{
	if (!m_open) return CursorKind::arrow;

	const UpdateStage stage = m_updater.stage();
	const bool over_close = (m_showing_release || can_dismiss(stage)) && close_button_rect().contains(m_mouse);
	const bool over_primary =
		(m_showing_release || has_primary_action(stage)) && primary_button_rect().contains(m_mouse);

	return over_close || over_primary ? CursorKind::hand : CursorKind::arrow;
}

void UpdateOverlay::draw_close_button(DrawList &t_draw_list) const
{
	const Rect close = close_button_rect();
	const bool hovered = close.contains(m_mouse);
	const Vec2 center = close.center();
	const Color glyph = hovered ? theme().text : theme().text_dim;

	if (hovered) {
		t_draw_list.add_rounded_rect(close, rounded(close.w * 0.5f), theme().control_hover);
	}

	t_draw_list.add_line({center.x - 5.0f, center.y - 5.0f}, {center.x + 5.0f, center.y + 5.0f}, 1.5f, glyph);
	t_draw_list.add_line({center.x - 5.0f, center.y + 5.0f}, {center.x + 5.0f, center.y - 5.0f}, 1.5f, glyph);
}

void UpdateOverlay::draw_title(DrawList &t_draw_list, float &t_baseline, std::string_view t_title) const
{
	const Font &body = m_fonts.body();

	draw_text(t_draw_list, body, Vec2{text_column_x(), t_baseline}, t_title, theme().text);
	t_baseline += body.line_height() + 10.0f + m_fonts.secondary().ascent();
}

void UpdateOverlay::draw_detail(DrawList &t_draw_list, float t_baseline, std::string_view t_text) const
{
	draw_text(t_draw_list, m_fonts.secondary(), Vec2{text_column_x(), t_baseline}, t_text, theme().text_dim);
}

void UpdateOverlay::draw_primary_button(DrawList &t_draw_list, std::string_view t_label, bool t_accented) const
{
	const Rect button = primary_button_rect();
	const controls::ButtonStyle style = t_accented ? controls::ButtonStyle::accent : controls::ButtonStyle::neutral;

	controls::draw_button(t_draw_list, m_fonts.body(), button, t_label, style, m_settings.accent, true,
						  button.contains(m_mouse), 255);
}

void UpdateOverlay::draw_notes(DrawList &t_draw_list) const
{
	const Notes current = notes();
	const Font &font = m_fonts.secondary();
	const Theme &colors = theme();
	const Color heading = mix(colors.text, m_settings.accent, note_heading_accent_mix);
	const float line_height = font.line_height();
	const float left = current.box.x + notes_padding;
	const float right = left + notes_text_width;
	const float mark_offset = font.ascent() * note_x_height_share;

	t_draw_list.add_bordered_rect(current.box, rounded(8.0f), colors.field, colors.separator, 1.0f);

	NoteLine lines[max_note_lines];
	float height = 0.0f;
	const u32 line_count = layout_notes(font, shown_notes(), lines, height);
	const float top = current.box.y + notes_padding - m_notes_scroll.offset();

	t_draw_list.push_clip(current.box);

	for (const NoteLine &line : std::span{lines, line_count}) {
		const float baseline = top + line.baseline;
		if (baseline < current.box.y - line_height || baseline > current.box.bottom() + line_height) continue;

		switch (line.kind) {
			case NoteKind::heading: {
				draw_text(t_draw_list, font, Vec2{left, baseline}, line.text, heading);

				const float rule_x = left + text_width(font, line.text) + note_heading_rule_gap;
				if (rule_x < right) {
					t_draw_list.add_rect(Rect{rule_x, snapped_to_pixel(baseline - mark_offset), right - rule_x, 1.0f},
										 colors.separator);
				}

				break;
			}

			case NoteKind::bullet: {
				const Rect dot{left + note_bullet_offset, baseline - mark_offset - note_bullet_size * 0.5f,
							   note_bullet_size, note_bullet_size};
				t_draw_list.add_rounded_rect(dot, rounded(note_bullet_size * 0.5f), heading);
				draw_text(t_draw_list, font, Vec2{left + note_bullet_indent, baseline}, line.text, colors.text);
				break;
			}

			case NoteKind::continuation:
				draw_text(t_draw_list, font, Vec2{left + note_bullet_indent, baseline}, line.text, colors.text);
				break;

			case NoteKind::text:
				draw_text(t_draw_list, font, Vec2{left, baseline}, line.text, colors.text_dim);
				break;
		}
	}

	t_draw_list.pop_clip();

	m_notes_scroll.draw_edge_fade(t_draw_list, current.box, current.scroll, colors.field);
	m_notes_scroll.draw(t_draw_list, current.scroll, m_mouse, 255);
}

void UpdateOverlay::draw_progress(DrawList &t_draw_list, float t_baseline, UpdateStage t_stage) const
{
	const Color accent = m_settings.accent;
	const u64 downloaded = m_updater.bytes_downloaded();
	const u64 total = m_updater.total_bytes();
	const float download_fraction = total > 0 ? static_cast<float>(downloaded) / static_cast<float>(total) : 0.0f;
	const float progress = t_stage == UpdateStage::downloading ? download_fraction : 1.0f;

	const Rect track{text_column_x(), t_baseline + 8.0f, text_column_width(), progress_height};
	const Rect fill{track.x, track.y, track.w * std::max(progress, 0.02f), track.h};

	t_draw_list.add_rounded_rect(track, rounded(track.h * 0.5f), theme().control);
	t_draw_list.add_rounded_rect(Rect{fill.x - progress_glow, fill.y - progress_glow * 0.5f,
									  fill.w + progress_glow * 2.0f, fill.h + progress_glow},
								 rounded(track.h * 0.5f + progress_glow), with_alpha(accent, 40));
	t_draw_list.add_rounded_rect(fill, rounded(track.h * 0.5f), accent);

	char line[64];
	if (t_stage == UpdateStage::downloading) {
		char downloaded_text[32];
		char total_text[32];
		char speed_text[32];
		const std::string_view downloaded_size = format_size(static_cast<double>(downloaded), "", downloaded_text);
		const std::string_view total_size = format_size(static_cast<double>(total), "", total_text);
		const std::string_view speed = format_size(m_updater.bytes_per_second(), "/s", speed_text);

		std::snprintf(line, sizeof(line), "%.*s / %.*s  -  %.*s", static_cast<int>(downloaded_size.size()),
					  downloaded_size.data(), static_cast<int>(total_size.size()), total_size.data(),
					  static_cast<int>(speed.size()), speed.data());
	} else {
		std::snprintf(line, sizeof(line), "%s", t_stage == UpdateStage::verifying ? "Verifying..." : "Installing...");
	}

	draw_detail(t_draw_list, track.bottom() + 14.0f + m_fonts.secondary().ascent(), line);

	if (t_stage == UpdateStage::downloading) {
		draw_primary_button(t_draw_list, "Cancel", false);
	}
}

void UpdateOverlay::draw(DrawList &t_draw_list)
{
	if (!m_open) return;

	const Vec2 window = m_window.size();
	const Rect card = card_rect();
	const UpdateStage stage = m_updater.stage();
	const Font &secondary = m_fonts.secondary();
	const float edge_inset = scaled_radius(card_radius);

	t_draw_list.add_rect(Rect{0.0f, 0.0f, window.x, window.y}, theme().scrim);
	controls::draw_panel_shadow(t_draw_list, card, card_radius, 1.0f);
	t_draw_list.add_bordered_rect(card, rounded(card_radius), theme().surface, theme().border, 1.0f);
	t_draw_list.add_rect(Rect{card.x + edge_inset, card.y + 1.0f, card.w - edge_inset * 2.0f, 1.0f},
						 with_alpha(theme().text_faint, card_top_edge_alpha));

	if (m_showing_release || can_dismiss(stage)) {
		draw_close_button(t_draw_list);
	}

	float baseline = card.y + card_padding + m_fonts.body().ascent();
	char line[96];

	if (m_showing_release) {
		std::snprintf(line, sizeof(line), "What's new in %s", m_release_version);
		draw_title(t_draw_list, baseline, line);
		draw_detail(t_draw_list, baseline, "Pulsar was updated. Here's what changed.");
		draw_notes(t_draw_list);
		draw_primary_button(t_draw_list, "Got it", true);
		return;
	}

	switch (stage) {
		case UpdateStage::idle:
		case UpdateStage::checking:
			draw_title(t_draw_list, baseline, "Checking for updates");
			draw_detail(t_draw_list, baseline, "This will only take a moment.");
			break;

		case UpdateStage::up_to_date:
			draw_title(t_draw_list, baseline, "You're up to date");
			std::snprintf(line, sizeof(line), "%s %s is the latest version.", app_name, app_version);
			draw_detail(t_draw_list, baseline, line);
			draw_primary_button(t_draw_list, "Check again", false);
			break;

		case UpdateStage::check_failed:
			draw_title(t_draw_list, baseline, "Couldn't check for updates");
			draw_wrapped_text(t_draw_list, secondary, Vec2{text_column_x(), baseline}, text_column_width(),
							  m_updater.error_message(), theme().error, max_error_lines);
			draw_primary_button(t_draw_list, "Try again", true);
			break;

		case UpdateStage::available:
			draw_title(t_draw_list, baseline, "Update available");
			std::snprintf(line, sizeof(line), "Version %s is ready to install.", m_updater.manifest().version);
			draw_detail(t_draw_list, baseline, line);
			draw_notes(t_draw_list);
			draw_primary_button(t_draw_list, "Download & Install", true);
			break;

		case UpdateStage::manual_upgrade_required:
			draw_title(t_draw_list, baseline, "Manual update required");
			std::snprintf(line, sizeof(line), "Version %s is out - please download it manually",
						  m_updater.manifest().version);
			draw_detail(t_draw_list, baseline, line);
			draw_detail(t_draw_list, baseline + secondary.line_height(), "from the GitHub releases page.");
			break;

		case UpdateStage::downloading:
		case UpdateStage::verifying:
		case UpdateStage::installing:
			draw_title(t_draw_list, baseline, "Updating");
			draw_progress(t_draw_list, baseline, stage);
			break;

		case UpdateStage::ready_to_relaunch:
			draw_title(t_draw_list, baseline, "Restarting...");
			break;

		case UpdateStage::error:
		case UpdateStage::cancelled:
			draw_title(t_draw_list, baseline, stage == UpdateStage::error ? "Update failed" : "Update cancelled");

			if (stage == UpdateStage::error) {
				draw_wrapped_text(t_draw_list, secondary, Vec2{text_column_x(), baseline}, text_column_width(),
								  m_updater.error_message(), theme().error, max_error_lines);
			}

			draw_primary_button(t_draw_list, "Try again", true);
			break;
	}
}
