#include "ui/update_overlay.h"

#include <algorithm>
#include <cstdio>
#include <span>

#include "core/app_identity.h"
#include "core/settings.h"
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
constexpr float notes_padding = 12.0f;
constexpr float notes_scrollbar_room = 14.0f;
constexpr u32 max_note_lines = 256;
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
}

void UpdateOverlay::update(float t_delta_seconds)
{
	m_notes_scroll.update(t_delta_seconds);
}

Rect UpdateOverlay::card_rect() const
{
	const Vec2 window = m_window.size();

	return Rect{0.0f, 0.0f, window.x, window.y}.centered(card_width, card_height);
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

	std::string_view lines[max_note_lines];
	const u32 line_count = wrap_text(secondary, m_updater.manifest().notes, box.w - notes_scrollbar_room, lines);
	const Rect track{box.right() - scrollbar_width, box.y, scrollbar_width, box.h};

	return Notes{box, ScrollGeometry{track, line_count * secondary.line_height(), box.h}};
}

bool UpdateOverlay::on_pointer_down(Vec2 t_point)
{
	if (!m_open) return false;

	if (m_updater.stage() == UpdateStage::available) {
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

	if (can_dismiss(stage) && close_button_rect().contains(t_point)) {
		close();
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

	if (m_updater.stage() == UpdateStage::available) {
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
	const bool over_close = can_dismiss(stage) && close_button_rect().contains(m_mouse);
	const bool over_primary = has_primary_action(stage) && primary_button_rect().contains(m_mouse);

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
	const float line_height = font.line_height();

	t_draw_list.add_bordered_rect(current.box, rounded(8.0f), theme().field, theme().separator, 1.0f);

	std::string_view lines[max_note_lines];
	const u32 line_count = wrap_text(font, m_updater.manifest().notes, current.box.w - notes_scrollbar_room, lines);

	t_draw_list.push_clip(current.box);

	float baseline = current.box.y + notes_padding + font.ascent() - m_notes_scroll.offset();
	for (const std::string_view line : std::span{lines, line_count}) {
		if (baseline > current.box.y - line_height && baseline < current.box.bottom() + line_height) {
			draw_text(t_draw_list, font, Vec2{current.box.x + notes_padding, baseline}, line, theme().text_dim);
		}

		baseline += line_height;
	}

	t_draw_list.pop_clip();

	m_notes_scroll.draw_edge_fade(t_draw_list, current.box, current.scroll, theme().field);
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

	if (can_dismiss(stage)) {
		draw_close_button(t_draw_list);
	}

	float baseline = card.y + card_padding + m_fonts.body().ascent();
	char line[96];

	switch (stage) {
		case UpdateStage::idle:
		case UpdateStage::checking:
			draw_title(t_draw_list, baseline, "Checking for Updates");
			draw_detail(t_draw_list, baseline, "This will only take a moment.");
			break;

		case UpdateStage::up_to_date:
			draw_title(t_draw_list, baseline, "You're Up to Date");
			std::snprintf(line, sizeof(line), "%s %s is the latest version.", app_name, app_version);
			draw_detail(t_draw_list, baseline, line);
			draw_primary_button(t_draw_list, "Check Again", false);
			break;

		case UpdateStage::check_failed:
			draw_title(t_draw_list, baseline, "Couldn't Check for Updates");
			draw_wrapped_text(t_draw_list, secondary, Vec2{text_column_x(), baseline}, text_column_width(),
							  m_updater.error_message(), theme().error, max_error_lines);
			draw_primary_button(t_draw_list, "Try Again", true);
			break;

		case UpdateStage::available:
			draw_title(t_draw_list, baseline, "Update Available");
			std::snprintf(line, sizeof(line), "Version %s is ready to install.", m_updater.manifest().version);
			draw_detail(t_draw_list, baseline, line);
			draw_notes(t_draw_list);
			draw_primary_button(t_draw_list, "Download & Install", true);
			break;

		case UpdateStage::manual_upgrade_required:
			draw_title(t_draw_list, baseline, "Manual Update Required");
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
			draw_title(t_draw_list, baseline, stage == UpdateStage::error ? "Update Failed" : "Update Cancelled");

			if (stage == UpdateStage::error) {
				draw_wrapped_text(t_draw_list, secondary, Vec2{text_column_x(), baseline}, text_column_width(),
								  m_updater.error_message(), theme().error, max_error_lines);
			}

			draw_primary_button(t_draw_list, "Try Again", true);
			break;
	}
}
