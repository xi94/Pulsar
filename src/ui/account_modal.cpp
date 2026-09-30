#include "ui/account_modal.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <numbers>
#include <span>
#include <utility>

#include <Windows.h>
#include <sodium.h>

#include "core/animation.h"
#include "core/profiler.h"
#include "core/settings.h"
#include "core/str.h"
#include "gfx/assets.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "platform/window.h"
#include "ui/controls.h"
#include "ui/text.h"
#include "ui/theme.h"
#include "ui/toasts.h"

namespace {
constexpr float open_ease_rate = 14.0f;

constexpr float panel_width_fraction = 0.78f;
constexpr float panel_height_fraction = 0.71f;
constexpr Vec2 panel_max_size{810.0f, 480.0f};
constexpr Vec2 panel_min_size{560.0f, 380.0f};
constexpr float reference_body_pixel_height = 24.0f;
constexpr float panel_margin = 48.0f;
constexpr float panel_closed_scale = 0.92f;
constexpr float panel_border = 1.5f;
constexpr float panel_radius = 16.0f;

constexpr float art_column_fraction = 0.35f;
constexpr float art_separator_width = 2.0f;
constexpr float close_badge_size = 40.0f;
constexpr float close_badge_margin = 12.0f;
constexpr float close_badge_icon_size = 18.0f;
constexpr float icon_crossfade_share = 0.35f;
constexpr float morph_seconds = 0.22f;
constexpr float panel_reveal_progress = 0.6f;
constexpr float morph_return_open_amount = 0.35f;
constexpr float interactive_open_amount = 0.5f;

constexpr std::string_view accounts_title = "Accounts";
constexpr float search_max_width = 170.0f;
constexpr float search_min_width = 90.0f;
constexpr float search_title_gap = 16.0f;
constexpr float search_button_gap = 10.0f;
constexpr float search_inset = 10.0f;
constexpr float search_icon_size = 13.0f;
constexpr float search_icon_gap = 7.0f;
constexpr float search_clear_margin = 8.0f;
constexpr u32 search_max_length = 64;

constexpr float row_padding = 24.0f;
constexpr float row_top_padding = 14.0f;
constexpr float row_line_gap = 4.0f;
constexpr float row_bottom_padding = 10.0f;
constexpr float row_button_gap = 10.0f;
constexpr float row_icon_inset = 5.0f;
constexpr float row_radius = 10.0f;
constexpr float detail_dot_size = 3.0f;
constexpr float detail_dot_gap = 7.0f;
constexpr float scrollbar_margin = 4.0f;

constexpr float drag_threshold = 4.0f;
constexpr float row_shift_ease_rate = 18.0f;
constexpr float lift_ease_rate = 16.0f;
constexpr float auto_scroll_zone = 0.6f;
constexpr float auto_scroll_speed = 540.0f;

constexpr float undo_seconds = 6.0f;
constexpr float delete_confirm_seconds = 3.0f;

constexpr float action_button_width = 108.0f;
constexpr float action_button_gap = 16.0f;

constexpr float empty_icon_size = 52.0f;
constexpr float empty_icon_radius = 12.0f;
constexpr float empty_gap = 16.0f;
constexpr float empty_line_gap = 4.0f;
constexpr float empty_button_width = 140.0f;

constexpr float form_top_padding = 16.0f;
constexpr float card_gap = 14.0f;
constexpr float card_radius = 10.0f;
constexpr float focus_ring_inset_x = 4.0f;
constexpr float focus_ring_inset_y = 3.0f;
constexpr float focus_ring_radius = 8.0f;
constexpr float focus_ring_border = 1.5f;
constexpr float focus_glow_blur = 8.0f;
constexpr u8 focus_glow_alpha = 60;
constexpr float field_label_padding = 14.0f;
constexpr float field_label_gap = 10.0f;
constexpr float value_text_inset = 8.0f;
constexpr float chevron_margin = 14.0f;
constexpr Vec2 chevron_size{9.0f, 5.0f};
constexpr std::string_view show_in_label = "SHOW IN";
constexpr float cancel_padding = 32.0f;
constexpr float reveal_button_size = 24.0f;
constexpr float reveal_button_margin = 6.0f;

constexpr float progress_max_width = 320.0f;
constexpr float progress_bar_height = 6.0f;
constexpr float progress_text_gap = 18.0f;
constexpr float cap_height_share = 0.66f;
constexpr float progress_text_rise = 8.0f;
constexpr float progress_ease_rate = 5.0f;
constexpr float progress_creep_seconds = 2.5f;
constexpr float status_ease_rate = 10.0f;
constexpr float outcome_ease_rate = 8.0f;
constexpr float progress_glow_blur = 8.0f;
constexpr u8 progress_glow_alpha = 70;
constexpr float sheen_width = 60.0f;
constexpr float sheen_passes_per_second = 0.7f;
constexpr u8 sheen_alpha = 90;
constexpr u32 login_step_count = 4;
constexpr u32 max_message_lines = 3;

constexpr Color color_on_art{255, 255, 255, 255};
constexpr Color color_art_badge{20, 20, 22, 255};
constexpr Color color_top_highlight{255, 255, 255, 22};
constexpr float danger_tint = 0.22f;
constexpr float region_chip_padding = 6.0f;
constexpr float region_chip_gap = 8.0f;
constexpr u8 region_chip_alpha = 22;
constexpr float armed_danger_tint = 0.4f;

struct FieldSpec {
	const char *label;
	u32 max_length;
};

constexpr FieldSpec field_specs[]{
	{"NOTE", sizeof(Account::note) - 1},
	{"USERNAME", sizeof(Account::username) - 1},
	{"PASSWORD", sizeof(Account::password) - 1},
};

struct RegionOption {
	std::string_view code;
	std::string_view label;
};

constexpr RegionOption region_options[]{
	{"", "None"},
	{"NA", "North America (NA)"},
	{"EUW", "Europe West (EUW)"},
	{"EUNE", "Europe Nordic & East (EUNE)"},
	{"KR", "Korea (KR)"},
	{"JP", "Japan (JP)"},
	{"BR", "Brazil (BR)"},
	{"LAN", "Latin America North (LAN)"},
	{"LAS", "Latin America South (LAS)"},
	{"OCE", "Oceania (OCE)"},
	{"TR", "Turkiye (TR)"},
	{"RU", "Russia (RU)"},
	{"ME", "Middle East (ME)"},
	{"PH", "Philippines (PH)"},
	{"SG", "Singapore (SG)"},
	{"TH", "Thailand (TH)"},
	{"TW", "Taiwan (TW)"},
	{"VN", "Vietnam (VN)"},
};

constexpr usize region_count = std::size(region_options);

constexpr auto region_labels = [] {
	std::array<std::string_view, region_count> labels{};
	for (usize i = 0; i < region_count; i += 1) {
		labels[i] = region_options[i].label;
	}

	return labels;
}();

constexpr std::string_view region_label = "REGION";

struct TimeUnit {
	i64 seconds;
	const char *name;
};

constexpr TimeUnit time_units[]{
	{365 * 86400, "year"}, {30 * 86400, "month"}, {7 * 86400, "week"}, {86400, "day"}, {3600, "hour"}, {60, "minute"},
};

float lerp(float t_from, float t_to, float t_amount)
{
	return t_from + (t_to - t_from) * t_amount;
}

Rect lerp(Rect t_from, Rect t_to, float t_amount)
{
	return Rect{lerp(t_from.x, t_to.x, t_amount), lerp(t_from.y, t_to.y, t_amount), lerp(t_from.w, t_to.w, t_amount),
				lerp(t_from.h, t_to.h, t_amount)};
}

Rect scaled_about(Rect t_rect, Vec2 t_origin, float t_scale)
{
	return Rect{t_origin.x + (t_rect.x - t_origin.x) * t_scale, t_origin.y + (t_rect.y - t_origin.y) * t_scale,
				t_rect.w * t_scale, t_rect.h * t_scale};
}

float panel_size_scale(const Fonts &t_fonts)
{
	return std::max(1.0f, t_fonts.body().pixel_height() / reference_body_pixel_height);
}

float row_height(const Fonts &t_fonts)
{
	return row_top_padding + t_fonts.body().line_height() + row_line_gap + t_fonts.secondary().line_height() +
		   row_bottom_padding;
}

float header_height(const Fonts &t_fonts)
{
	return t_fonts.body().line_height() + 20.0f;
}

float footer_height(const Fonts &t_fonts)
{
	return std::max(56.0f, t_fonts.body().line_height() + 20.0f);
}

float action_button_height(const Fonts &t_fonts)
{
	return std::max(32.0f, t_fonts.body().line_height() + 10.0f);
}

float field_input_height(const Fonts &t_fonts)
{
	return std::max(46.0f, t_fonts.body().line_height() + 22.0f);
}

void draw_focus_ring(DrawList &t_draw_list, Rect t_rect, float t_radius, Color t_fill, Color t_accent, u8 t_alpha)
{
	t_draw_list.add_shadow(t_rect, t_radius, focus_glow_blur,
						   with_alpha(t_accent, static_cast<u8>(focus_glow_alpha * t_alpha / 255)));
	t_draw_list.add_bordered_rect(t_rect, rounded(t_radius), faded(t_fill, t_alpha), faded(t_accent, t_alpha),
								  focus_ring_border);
}

float label_column_width(const Fonts &t_fonts)
{
	const Font &font = t_fonts.secondary();
	float widest = std::max(text_width(font, show_in_label), text_width(font, region_label));

	for (const FieldSpec &spec : field_specs) {
		widest = std::max(widest, text_width(font, spec.label));
	}

	return field_label_padding + widest + field_label_gap;
}

float row_button_size(const Fonts &t_fonts)
{
	return std::max(28.0f, t_fonts.secondary().line_height() + 8.0f);
}

float search_height(const Fonts &t_fonts)
{
	return std::max(24.0f, t_fonts.secondary().line_height() + 4.0f);
}

Rect vertically_centered(Rect t_strip, float t_x, float t_width, float t_height)
{
	return Rect{t_x, t_strip.y + (t_strip.h - t_height) * 0.5f, t_width, t_height};
}

Rect row_highlight(Rect t_row)
{
	return Rect{t_row.x - 8.0f, t_row.y + 3.0f, t_row.w + 16.0f, t_row.h - 6.0f};
}

float region_chip_width(const Font &t_font, std::string_view t_region)
{
	return t_region.empty() ? 0.0f : text_width(t_font, t_region) + region_chip_padding * 2.0f;
}

void draw_region_chip(DrawList &t_draw_list, const Font &t_font, float t_x, float t_center_y, std::string_view t_region,
					  u8 t_alpha)
{
	const float height = t_font.line_height() + 2.0f;
	const Rect chip{t_x, t_center_y - height * 0.5f, region_chip_width(t_font, t_region), height};

	t_draw_list.add_rounded_rect(chip, rounded(height * 0.5f),
								 faded(with_alpha(theme().text, region_chip_alpha), t_alpha));
	draw_text_centered(t_draw_list, t_font, chip, t_region, faded(theme().text_dim, t_alpha));
}

bool matches_query(const Account &t_account, std::string_view t_query)
{
	return find_ignoring_case(t_account.username, t_query) != std::string_view::npos ||
		   find_ignoring_case(t_account.note, t_query) != std::string_view::npos ||
		   find_ignoring_case(t_account.region, t_query) != std::string_view::npos;
}

std::string_view relative_time(i64 t_then, i64 t_now, char (&t_buffer)[32])
{
	const i64 elapsed = std::max<i64>(0, t_now - t_then);

	for (const TimeUnit &unit : time_units) {
		const i64 count = elapsed / unit.seconds;
		if (count == 0) continue;
		if (unit.seconds == 86400 && count == 1) return "yesterday";

		const int written = std::snprintf(t_buffer, sizeof(t_buffer), "%lld %s%s ago", static_cast<long long>(count),
										  unit.name, count == 1 ? "" : "s");

		return std::string_view{t_buffer, static_cast<usize>(std::max(written, 0))};
	}

	return "just now";
}

void shift_after_insert(std::optional<AccountRef> &t_ref, AccountRef t_inserted)
{
	if (t_ref && t_ref->game == t_inserted.game && t_ref->index >= t_inserted.index) {
		t_ref->index += 1;
	}
}

void shift_after_removal(std::optional<AccountRef> &t_ref, AccountRef t_removed)
{
	if (!t_ref || t_ref->game != t_removed.game) return;

	if (t_ref->index == t_removed.index) {
		t_ref.reset();
	} else if (t_ref->index > t_removed.index) {
		t_ref->index -= 1;
	}
}

struct StageSpan {
	float start;
	float end;
};

std::optional<StageSpan> stage_span(LoginStage t_stage)
{
	switch (t_stage) {
		case LoginStage::idle:
			return StageSpan{0.02f, 0.1f};
		case LoginStage::waiting_for_process:
			return StageSpan{0.08f, 0.3f};
		case LoginStage::connecting:
			return StageSpan{0.32f, 0.55f};
		case LoginStage::authenticating:
			return StageSpan{0.58f, 0.82f};
		case LoginStage::launching:
			return StageSpan{0.85f, 0.97f};
		case LoginStage::success:
			return StageSpan{1.0f, 1.0f};
		case LoginStage::error:
		case LoginStage::cancelled:
			break;
	}

	return std::nullopt;
}

u32 stage_step(LoginStage t_stage)
{
	switch (t_stage) {
		case LoginStage::waiting_for_process:
			return 1;
		case LoginStage::connecting:
			return 2;
		case LoginStage::authenticating:
			return 3;
		case LoginStage::launching:
			return 4;
		case LoginStage::idle:
		case LoginStage::success:
		case LoginStage::error:
		case LoginStage::cancelled:
			break;
	}

	return 0;
}

std::string_view stage_message(LoginStage t_stage)
{
	switch (t_stage) {
		case LoginStage::idle:
			return "";
		case LoginStage::waiting_for_process:
			return "Launching Riot Client...";
		case LoginStage::connecting:
			return "Waiting for Riot Client...";
		case LoginStage::authenticating:
			return "Logging in...";
		case LoginStage::launching:
			return "Launching game...";
		case LoginStage::success:
			return "Logged in!";
		case LoginStage::error:
			return "Something went wrong.";
		case LoginStage::cancelled:
			return "Cancelled.";
	}

	return "";
}
}

AccountModal::AccountModal(Library &t_library, const Settings &t_settings, const Fonts &t_fonts, const Assets &t_assets,
						   const Window &t_window, Toasts &t_toasts, CommandQueue &t_commands)
	: m_library(t_library)
	, m_settings(t_settings)
	, m_fonts(t_fonts)
	, m_assets(t_assets)
	, m_window(t_window)
	, m_toasts(t_toasts)
	, m_commands(t_commands)
	, m_visible_games(t_library, t_settings, t_fonts)
	, m_region_list(t_fonts, t_settings, ListPopupOptions{.empty_message = "No regions"})
{
	for (u32 i = 0; i < field_count; i += 1) {
		m_fields[i].set_max_length(field_specs[i].max_length);
	}

	m_search.set_max_length(search_max_length);
	m_search.set_placeholder("Search");
}

bool AccountModal::has_game() const
{
	return m_game >= 0 && static_cast<u32>(m_game) < m_library.game_count();
}

Vec2 AccountModal::floating_panel_size() const
{
	const float size_scale = panel_size_scale(m_fonts);
	const Vec2 max_size{panel_max_size.x * size_scale, panel_max_size.y * size_scale};
	const Vec2 window = m_window.size();

	float width =
		std::min({window.x * panel_width_fraction, max_size.x, std::max(0.0f, window.x - panel_margin * 2.0f)});
	float height =
		std::min({window.y * panel_height_fraction, max_size.y, std::max(0.0f, window.y - panel_margin * 2.0f)});

	const float aspect = max_size.x / max_size.y;
	if (width / height > aspect) {
		width = height * aspect;
	} else {
		height = width / aspect;
	}

	return Vec2{width, height};
}

bool AccountModal::is_docked() const
{
	const float size_scale = panel_size_scale(m_fonts);
	const Vec2 floating = floating_panel_size();

	return floating.x < panel_min_size.x * size_scale || floating.y < panel_min_size.y * size_scale;
}

Rect AccountModal::panel_rect() const
{
	if (is_docked()) return m_window.content_rect().inset(0.0f, 1.0f);

	const Vec2 size = floating_panel_size();
	const Vec2 window = m_window.size();

	return Rect{0.0f, 0.0f, window.x, window.y}.centered(size.x, size.y);
}

AccountModal::Layout AccountModal::layout() const
{
	Layout result{};
	result.panel = panel_rect();
	result.inner = result.panel.inset(panel_border);

	Rect content = result.inner;
	result.footer = content.split_bottom(footer_height(m_fonts));
	const float art_width = std::max(content.w * art_column_fraction, close_badge_size + close_badge_margin * 2.0f);
	result.art_column = Rect{content.x, content.y, art_width, content.h};

	const float main_x = result.art_column.right() + art_separator_width;
	result.main_column = Rect{main_x, content.y, content.right() - main_x, content.h};

	return result;
}

VisibleAccounts AccountModal::displayed_accounts() const
{
	if (!has_game()) return VisibleAccounts{};

	const VisibleAccounts visible = m_library.visible_accounts(static_cast<u32>(m_game));
	const std::string_view query = m_search.value();
	if (query.empty()) return visible;

	VisibleAccounts matching;
	for (const AccountRef ref : visible.view()) {
		if (!matches_query(m_library.account(ref), query)) continue;

		matching.refs[matching.count] = ref;
		matching.count += 1;
	}

	return matching;
}

AccountModal::AccountRows AccountModal::account_rows(const Layout &t_layout) const
{
	const Rect main = t_layout.main_column;
	const float header = header_height(m_fonts);

	AccountRows rows{};
	rows.region = Rect{main.x, main.y + header, main.w, main.h - header};
	rows.row_height = row_height(m_fonts);
	rows.accounts = displayed_accounts();

	const Rect track{rows.region.right() - scrollbar_width - scrollbar_margin, rows.region.y, scrollbar_width,
					 rows.region.h};
	rows.scroll = ScrollGeometry{track, rows.accounts.count * rows.row_height, rows.region.h};

	return rows;
}

i32 AccountModal::selected_row(const VisibleAccounts &t_accounts) const
{
	if (!m_selected) return -1;

	for (u32 i = 0; i < t_accounts.count; i += 1) {
		if (t_accounts.refs[i] == *m_selected) return static_cast<i32>(i);
	}

	return -1;
}

float AccountModal::content_to_screen(const AccountRows &t_rows, float t_content_y) const
{
	return t_rows.region.y + t_content_y - m_rows_scroll.offset();
}

Rect AccountModal::row_rect_at(const Layout &t_layout, const AccountRows &t_rows, float t_content_top) const
{
	const Rect main = t_layout.main_column;

	return Rect{main.x + row_padding, content_to_screen(t_rows, t_content_top), main.w - row_padding * 2.0f,
				t_rows.row_height};
}

Rect AccountModal::row_rect(const Layout &t_layout, const AccountRows &t_rows, u32 t_row) const
{
	return row_rect_at(t_layout, t_rows, t_row * t_rows.row_height + m_row_offsets[t_row]);
}

i32 AccountModal::row_at(const Layout &t_layout, const AccountRows &t_rows, Vec2 t_point) const
{
	if (!t_rows.region.contains(t_point)) return -1;

	for (u32 i = 0; i < t_rows.accounts.count; i += 1) {
		const Rect row = row_rect(t_layout, t_rows, i);
		if (row.overlaps_vertically(t_rows.region) && row.contains(t_point)) return static_cast<i32>(i);
	}

	return -1;
}

Rect AccountModal::remove_button_rect(Rect t_row) const
{
	const float size = row_button_size(m_fonts);

	return vertically_centered(t_row, t_row.right() - size, size, size);
}

Rect AccountModal::edit_button_rect(Rect t_row) const
{
	const Rect remove = remove_button_rect(t_row);

	return Rect{remove.x - row_button_gap - remove.w, remove.y, remove.w, remove.h};
}

Rect AccountModal::favorite_button_rect(Rect t_row) const
{
	const Rect edit = edit_button_rect(t_row);

	return Rect{edit.x - row_button_gap - edit.w, edit.y, edit.w, edit.h};
}

bool AccountModal::is_row_button_hit(Rect t_row, Vec2 t_point) const
{
	return remove_button_rect(t_row).contains(t_point) || edit_button_rect(t_row).contains(t_point) ||
		   favorite_button_rect(t_row).contains(t_point);
}

Rect AccountModal::add_button_rect(Rect t_main) const
{
	const float size = row_button_size(m_fonts);
	const Rect header{t_main.x, t_main.y, t_main.w, header_height(m_fonts)};

	return vertically_centered(header, t_main.right() - row_padding - size, size, size);
}

Rect AccountModal::search_rect(Rect t_main) const
{
	const Rect header{t_main.x, t_main.y, t_main.w, header_height(m_fonts)};
	const float left = t_main.x + row_padding + text_width(m_fonts.body(), accounts_title) + search_title_gap;
	const float right = add_button_rect(t_main).x - search_button_gap;
	const float width = std::min(search_max_width, right - left);
	if (width < search_min_width) return Rect{};

	return vertically_centered(header, snapped_to_pixel(left + (right - left - width) * 0.5f), width,
							   search_height(m_fonts));
}

Rect AccountModal::search_text_rect(Rect t_search) const
{
	const float left = t_search.x + search_inset + search_icon_size + search_icon_gap;
	const float right = search_clear_rect(t_search).x - search_icon_gap * 0.5f;

	return Rect{left, t_search.y, std::max(0.0f, right - left), t_search.h};
}

Rect AccountModal::search_clear_rect(Rect t_search) const
{
	const float size = search_icon_size;

	return vertically_centered(t_search, t_search.right() - search_clear_margin - size, size, size);
}

Rect AccountModal::primary_button_rect(Rect t_footer) const
{
	return vertically_centered(t_footer, t_footer.right() - row_padding - action_button_width, action_button_width,
							   action_button_height(m_fonts));
}

Rect AccountModal::cancel_button_rect(Rect t_primary) const
{
	const float width = text_width(m_fonts.body(), "Cancel") + cancel_padding;

	return Rect{t_primary.x - action_button_gap - width, t_primary.y, width, t_primary.h};
}

Rect AccountModal::form_region(Rect t_main) const
{
	const float header = header_height(m_fonts);

	return Rect{t_main.x, t_main.y + header, t_main.w, std::max(0.0f, t_main.h - header)};
}

AccountModal::FormLayout AccountModal::form_layout(Rect t_main) const
{
	const float row = field_input_height(m_fonts);
	const float left = t_main.x + row_padding;
	const float width = t_main.w - row_padding * 2.0f;

	FormLayout form{};
	form.region = form_region(t_main);

	const float top = form.region.y + form_top_padding - m_form_scroll.offset();
	form.rows[show_in_row] = Rect{left, top, width, row};
	constexpr u32 card_slots = field_count + 1;
	form.card = Rect{left, form.rows[show_in_row].bottom() + card_gap, width, row * card_slots + (card_slots - 1)};

	const auto slot = [&](u32 t_slot) { return Rect{left, form.card.y + t_slot * (row + 1.0f), width, row}; };
	form.rows[static_cast<u32>(EditField::note)] = slot(0);
	form.rows[region_row] = slot(1);
	form.rows[static_cast<u32>(EditField::username)] = slot(2);
	form.rows[static_cast<u32>(EditField::password)] = slot(3);

	form.content_height = form_top_padding * 2.0f + form.card.h + card_gap + row;

	return form;
}

ScrollGeometry AccountModal::form_scroll(Rect t_main) const
{
	const FormLayout form = form_layout(t_main);
	const Rect track{form.region.right() - scrollbar_width - scrollbar_margin, form.region.y, scrollbar_width,
					 form.region.h};

	return ScrollGeometry{track, form.content_height, form.region.h};
}

Rect AccountModal::field_input_rect(Rect t_main, u32 t_field) const
{
	return form_layout(t_main).rows[t_field];
}

Rect AccountModal::field_text_rect(Rect t_main, u32 t_field) const
{
	Rect input = field_input_rect(t_main, t_field);
	const float label = label_column_width(m_fonts);
	input.x += label;
	input.w -= label;

	if (t_field == static_cast<u32>(EditField::password)) {
		input.w -= reveal_button_size + reveal_button_margin;
	}

	return input;
}

Rect AccountModal::reveal_button_rect(Rect t_main) const
{
	const Rect password = field_input_rect(t_main, static_cast<u32>(EditField::password));

	return vertically_centered(password, password.right() - reveal_button_size - reveal_button_margin,
							   reveal_button_size, reveal_button_size);
}

Rect AccountModal::show_in_rect(Rect t_main) const
{
	return form_layout(t_main).rows[show_in_row];
}

Rect AccountModal::region_rect(Rect t_main) const
{
	return form_layout(t_main).rows[region_row];
}

bool AccountModal::is_region_hit(Rect t_main, Vec2 t_point) const
{
	return form_region(t_main).contains(t_point) && region_rect(t_main).contains(t_point);
}

void AccountModal::open_region_list()
{
	focus_field(-1);
	m_visible_games.close();

	u32 selected = 0;
	for (u32 i = 0; i < region_count; i += 1) {
		if (region_options[i].code == std::string_view{m_region}) {
			selected = i;
		}
	}

	m_region_list.open(region_labels, selected);
}

void AccountModal::choose_region(u32 t_index)
{
	if (t_index < region_count) {
		copy_to(region_options[t_index].code, m_region);
	}
}

bool AccountModal::is_show_in_hit(Rect t_main, Vec2 t_point) const
{
	return form_region(t_main).contains(t_point) && show_in_rect(t_main).contains(t_point);
}

std::string_view AccountModal::visibility_summary(char (&t_buffer)[32]) const
{
	const u16 mask = m_visible_games.mask();
	const auto count = static_cast<u32>(std::popcount(mask));

	if (count == m_library.game_count()) return "All games";
	if (count == 1) return m_library.game(static_cast<u32>(std::countr_zero(mask))).title;

	const int written = std::snprintf(t_buffer, sizeof(t_buffer), "%u games", count);

	return std::string_view{t_buffer, static_cast<usize>(std::max(written, 0))};
}

i32 AccountModal::focused_field() const
{
	for (u32 i = 0; i < field_count; i += 1) {
		if (m_fields[i].is_focused()) return static_cast<i32>(i);
	}

	return -1;
}

void AccountModal::focus_field(i32 t_field)
{
	for (u32 i = 0; i < field_count; i += 1) {
		m_fields[i].set_focused(static_cast<i32>(i) == t_field);
	}
}

i32 AccountModal::field_at(Rect t_main, Vec2 t_point) const
{
	if (m_visible_games.is_open() || !form_region(t_main).contains(t_point) || is_reveal_hit(t_main, t_point)) {
		return -1;
	}

	for (u32 i = 0; i < field_count; i += 1) {
		if (field_input_rect(t_main, i).contains(t_point)) return static_cast<i32>(i);
	}

	return -1;
}

bool AccountModal::is_reveal_hit(Rect t_main, Vec2 t_point) const
{
	return form_region(t_main).contains(t_point) && reveal_button_rect(t_main).contains(t_point);
}

void AccountModal::reveal_field(i32 t_field)
{
	if (t_field < 0) return;

	const Rect main = layout().main_column;
	const Rect region = form_region(main);
	const Rect input = field_input_rect(main, static_cast<u32>(t_field));

	m_form_scroll.reveal(input.y, input.bottom(), region.y, region.bottom(), form_scroll(main));
}

void AccountModal::open(i32 t_game)
{
	m_open = true;
	m_art_source.reset();
	m_armed_delete.reset();
	m_game = t_game;
	m_mode = Mode::account_list;
	m_rows_scroll = Scrollable{};
	m_selected.reset();
	m_search.set_focused(false);
	clear_search();
	reset_row_motion();
}

i32 AccountModal::detached_game() const
{
	return m_art_source && m_morph_progress > 0.0f ? m_game : -1;
}

void AccountModal::close()
{
	m_open = false;
	m_region_list.close();
	m_armed_delete.reset();
	m_search.set_focused(false);
	cancel_row_drag();
}

void AccountModal::quick_login(u32 t_game, AccountRef t_account)
{
	open(static_cast<i32>(t_game));
	request_login(t_game, t_account);
}

const Account *AccountModal::account_at_row(i32 t_row) const
{
	if (t_row < 0) return nullptr;

	const VisibleAccounts shown = displayed_accounts();
	if (static_cast<u32>(t_row) >= shown.count) return nullptr;

	return &m_library.account(shown.refs[t_row]);
}

void AccountModal::forget_secrets()
{
	cancel_login();
	forget_deleted();
	m_armed_delete.reset();

	for (TextInput &input : m_fields) {
		input.set_value("");
		input.set_focused(false);
	}

	m_search.set_value("");
	m_search.set_focused(false);
	sodium_memzero(m_applied_query, sizeof(m_applied_query));

	m_visible_games.close();
	m_edited.reset();
	m_selected.reset();
	reset_row_motion();

	m_mode = Mode::account_list;
	m_open = false;
	m_open_amount = 0.0f;
	m_game = -1;
}

void AccountModal::start_adding()
{
	m_show_required = false;
	m_armed_delete.reset();
	m_mode = Mode::edit_account;
	m_form_scroll = Scrollable{};
	m_edited.reset();
	m_region[0] = '\0';
	m_region_list.close();
	m_search.set_focused(false);

	for (TextInput &input : m_fields) {
		input.set_value("");
	}

	focus_field(static_cast<i32>(EditField::note));
	field(EditField::password).set_masked(true);

	m_visible_games.set_mask(static_cast<u16>(1u << m_game));
	m_visible_games.close();
}

void AccountModal::start_editing(AccountRef t_account)
{
	m_show_required = false;
	m_armed_delete.reset();
	m_mode = Mode::edit_account;
	m_form_scroll = Scrollable{};
	m_edited = t_account;
	m_search.set_focused(false);

	const Account &account = m_library.account(t_account);
	field(EditField::note).set_value(account.note);
	copy_to(std::string_view{account.region}, m_region);
	m_region_list.close();
	field(EditField::username).set_value(account.username);
	field(EditField::password).set_value(account.password);

	focus_field(static_cast<i32>(EditField::note));
	field(EditField::password).set_masked(true);

	m_visible_games.set_mask(account.visible_games(t_account.game));
	m_visible_games.close();
}

bool AccountModal::can_save() const
{
	return !field(EditField::username).value().empty() && !field(EditField::password).value().empty();
}

bool AccountModal::has_changes() const
{
	const std::string_view note = field(EditField::note).value();
	const std::string_view region = m_region;
	const std::string_view username = field(EditField::username).value();
	const std::string_view password = field(EditField::password).value();

	if (!m_edited) return !note.empty() || !region.empty() || !username.empty() || !password.empty();

	const Account &account = m_library.account(*m_edited);

	return note != account.note || region != account.region || username != account.username ||
		   password != account.password || m_visible_games.mask() != account.visible_games(m_edited->game);
}

void AccountModal::save_edit()
{
	m_mode = Mode::account_list;
	if (!has_game()) return;

	const std::string_view username = field(EditField::username).value();
	const std::string_view note = field(EditField::note).value();
	const std::string_view password = field(EditField::password).value();

	if (m_edited) {
		Account &account = m_library.account(*m_edited);
		account.assign(username, note, password);
		copy_to(std::string_view{m_region}, account.region);
		account.visible_game_mask = m_visible_games.mask();
		return;
	}

	Account account{.visible_game_mask = m_visible_games.mask()};
	account.assign(username, note, password);
	copy_to(std::string_view{m_region}, account.region);

	const std::optional<AccountRef> added = m_library.add_account(static_cast<u32>(m_game), account);
	sodium_memzero(&account, sizeof(account));

	if (!added) {
		notify("This game can't hold any more accounts.");
		return;
	}

	reset_row_motion();
	m_selected = added;
	reveal_selected();
}

void AccountModal::delete_account(AccountRef t_account)
{
	forget_deleted();

	m_deleted = DeletedAccount{m_library.account(t_account), t_account};
	m_library.remove_account(t_account);
	follow_removal(t_account);
	reset_row_motion();

	m_toasts.notify(Notification{
		.message = "Account deleted. Click to undo.",
		.on_click = Command{.type = CommandType::undo_delete},
		.seconds = undo_seconds,
		.always_show = true,
	});
}

void AccountModal::arm_or_delete(AccountRef t_account)
{
	if (m_armed_delete == t_account) {
		m_armed_delete.reset();
		delete_account(t_account);
		return;
	}

	m_armed_delete = t_account;
	m_armed_seconds = delete_confirm_seconds;
}

float AccountModal::delete_countdown(AccountRef t_account) const
{
	return m_armed_delete == t_account ? m_armed_seconds / delete_confirm_seconds : 0.0f;
}

void AccountModal::forget_deleted()
{
	if (!m_deleted) return;

	sodium_memzero(&m_deleted->account, sizeof(Account));
	m_deleted.reset();
}

void AccountModal::undo_delete()
{
	if (!m_deleted) return;

	const std::optional<AccountRef> restored = m_library.insert_account(m_deleted->position, m_deleted->account);
	forget_deleted();

	if (!restored) {
		notify("There's no room left to restore that account.");
		return;
	}

	follow_insert(*restored);
	reset_row_motion();
	m_selected = restored;
	reveal_selected();
}

void AccountModal::toggle_favorite(i32 t_row)
{
	if (t_row < 0) return;

	const VisibleAccounts shown = displayed_accounts();
	if (static_cast<u32>(t_row) < shown.count) {
		toggle_favorite(shown.refs[t_row]);
	}
}

void AccountModal::toggle_favorite(AccountRef t_account)
{
	const VisibleAccounts before = displayed_accounts();

	Account &account = m_library.account(t_account);
	account.favorite = !account.favorite;

	animate_reorder(before);

	const VisibleAccounts after = displayed_accounts();
	for (u32 i = 0; i < after.count; i += 1) {
		if (after.refs[i] == t_account) {
			m_raised_row = i;
		}
	}
}

void AccountModal::follow_insert(AccountRef t_inserted)
{
	shift_after_insert(m_selected, t_inserted);
	shift_after_insert(m_edited, t_inserted);
	shift_after_insert(m_login_account, t_inserted);
	shift_after_insert(m_armed_delete, t_inserted);

	if (m_queued_login) {
		std::optional<AccountRef> queued = m_queued_login->account;
		shift_after_insert(queued, t_inserted);
		m_queued_login->account = *queued;
	}
}

void AccountModal::follow_removal(AccountRef t_removed)
{
	shift_after_removal(m_selected, t_removed);
	shift_after_removal(m_edited, t_removed);
	shift_after_removal(m_login_account, t_removed);
	shift_after_removal(m_armed_delete, t_removed);

	if (m_queued_login) {
		std::optional<AccountRef> queued = m_queued_login->account;
		shift_after_removal(queued, t_removed);

		if (queued) {
			m_queued_login->account = *queued;
		} else {
			m_queued_login.reset();
		}
	}
}

void AccountModal::notify(std::string_view t_message)
{
	m_toasts.notify(Notification{.message = t_message});
}

void AccountModal::request_login(u32 t_game, AccountRef t_account)
{
	m_armed_delete.reset();
	m_selected = t_account;
	m_mode = Mode::login_progress;
	m_login_seconds = 0.0f;
	m_login_progress = 0.0f;
	m_login_outcome = 0.0f;
	m_stage_seconds = 0.0f;
	m_status_from[0] = '\0';
	m_status_to[0] = '\0';
	m_status_change = 1.0f;
	m_search.set_focused(false);

	const PendingLogin login{t_game, t_account};

	if (m_login.is_active() && !LoginAttempt::is_terminal(m_login.stage())) {
		m_login.cancel();
		m_login_account.reset();
		m_queued_login = login;
		return;
	}

	m_queued_login.reset();
	start_login(login);
}

void AccountModal::start_login(PendingLogin t_login)
{
	if (t_login.game >= m_library.game_count()) return;
	if (t_login.account.index >= m_library.game(t_login.account.game).account_count) return;

	const Account &account = m_library.account(t_login.account);
	m_login_account = t_login.account;
	m_login.start(account.username, account.password, m_library.game(t_login.game).title);
}

void AccountModal::cancel_login()
{
	m_queued_login.reset();
	m_login_account.reset();
	m_login.cancel();
}

void AccountModal::record_login_result()
{
	if (!m_login_account || !LoginAttempt::is_terminal(m_login.stage())) return;

	if (m_login.stage() == LoginStage::success) {
		m_library.account(*m_login_account).last_used = std::time(nullptr);
		m_commands.push(Command{.type = CommandType::save_changes});
	}

	m_login_account.reset();
}

void AccountModal::refresh_search()
{
	const std::string_view query = m_search.value();
	if (query == std::string_view{m_applied_query}) return;

	copy_to(query, m_applied_query);
	reset_row_motion();

	const Layout current = layout();
	const AccountRows rows = account_rows(current);

	if (query.empty()) {
		reveal_selected();
		return;
	}

	m_rows_scroll.jump_to(0.0f, rows.scroll);
}

void AccountModal::clear_search()
{
	m_search.set_value("");
	refresh_search();
}

void AccountModal::reveal_selected()
{
	const Layout current = layout();
	const AccountRows rows = account_rows(current);
	const i32 row = selected_row(rows.accounts);
	if (row < 0) return;

	const Rect rect = row_rect(current, rows, static_cast<u32>(row));
	m_rows_scroll.reveal(rect.y, rect.bottom(), rows.region.y, rows.region.bottom(), rows.scroll);
}

void AccountModal::select_step(i32 t_step)
{
	const VisibleAccounts shown = displayed_accounts();
	if (shown.count == 0) return;

	const i32 selected = selected_row(shown);
	const i32 last = static_cast<i32>(shown.count) - 1;
	const i32 row = selected < 0 ? 0 : std::clamp(selected + t_step, 0, last);

	m_selected = shown.refs[row];
	reveal_selected();
}

bool AccountModal::is_search_visible() const
{
	return search_rect(layout().main_column).w > 0.0f;
}

AccountModal::RowRange AccountModal::drag_range(const VisibleAccounts &t_accounts, u32 t_row) const
{
	u32 favorites = 0;
	while (favorites < t_accounts.count && m_library.account(t_accounts.refs[favorites]).favorite) {
		favorites += 1;
	}

	if (t_row < favorites) return RowRange{0, favorites - 1};

	return RowRange{favorites, t_accounts.count - 1};
}

float AccountModal::lifted_top(const AccountRows &t_rows) const
{
	const RowRange range = drag_range(t_rows.accounts, m_drag.from_row);
	const float pointer = m_mouse.y - t_rows.region.y + m_rows_scroll.offset();

	return std::clamp(pointer - m_drag.grab_offset, range.first * t_rows.row_height, range.last * t_rows.row_height);
}

void AccountModal::lift_row(const AccountRows &t_rows, u32 t_row, Vec2 t_point)
{
	const float top = t_row * t_rows.row_height + m_row_offsets[t_row];
	const float pointer = t_point.y - t_rows.region.y + m_rows_scroll.offset();

	m_drag.lifted = true;
	m_drag.from_row = t_row;
	m_drag.target_row = t_row;
	m_drag.grab_offset = pointer - top;
	m_raised_row = t_row;
	m_search.set_focused(false);
}

void AccountModal::drop_row()
{
	if (!m_drag.lifted) {
		m_drag = RowDrag{};
		return;
	}

	const AccountRows rows = account_rows(layout());
	const u32 from = m_drag.from_row;
	const u32 to = m_drag.target_row;

	if (from < rows.accounts.count) {
		m_row_offsets[from] = lifted_top(rows) - from * rows.row_height;
	}

	m_drag = RowDrag{};

	if (from >= rows.accounts.count || to >= rows.accounts.count) {
		reset_row_motion();
		return;
	}

	if (from != to) {
		m_library.move_visible_account(static_cast<u32>(m_game), from, to);
		animate_reorder(rows.accounts);
	}

	m_raised_row = to;
}

void AccountModal::cancel_row_drag()
{
	m_drag.target_row = m_drag.from_row;
	drop_row();
}

void AccountModal::update_row_drag(float t_delta_seconds)
{
	const AccountRows rows = account_rows(layout());
	const float height = rows.row_height;

	if (m_drag.lifted && m_drag.from_row >= rows.accounts.count) {
		reset_row_motion();
	}

	if (m_drag.lifted) {
		const float zone = height * auto_scroll_zone;
		const float above = rows.region.y + zone - m_mouse.y;
		const float below = m_mouse.y - (rows.region.bottom() - zone);

		if (above > 0.0f) {
			m_rows_scroll.scroll_by(-auto_scroll_speed * std::min(above / zone, 1.0f) * t_delta_seconds, rows.scroll);
		} else if (below > 0.0f) {
			m_rows_scroll.scroll_by(auto_scroll_speed * std::min(below / zone, 1.0f) * t_delta_seconds, rows.scroll);
		}

		const RowRange range = drag_range(rows.accounts, m_drag.from_row);
		const auto slot = static_cast<u32>(std::lround(lifted_top(rows) / height));
		m_drag.target_row = std::clamp(slot, range.first, range.last);
	}

	for (u32 i = 0; i < rows.accounts.count; i += 1) {
		if (m_drag.lifted && i == m_drag.from_row) continue;

		float target = 0.0f;
		if (m_drag.lifted && m_drag.from_row < i && i <= m_drag.target_row) {
			target = -height;
		} else if (m_drag.lifted && m_drag.target_row <= i && i < m_drag.from_row) {
			target = height;
		}

		m_row_offsets[i] = animation::ease_toward(m_row_offsets[i], target, row_shift_ease_rate, t_delta_seconds,
												  animation::settled_pixels);
	}

	m_lift_amount = animation::ease_toward(m_lift_amount, m_drag.lifted ? 1.0f : 0.0f, lift_ease_rate, t_delta_seconds);

	const bool raised_settled =
		!m_raised_row || *m_raised_row >= rows.accounts.count || m_row_offsets[*m_raised_row] == 0.0f;
	if (!m_drag.lifted && m_lift_amount == 0.0f && raised_settled) {
		m_raised_row.reset();
	}
}

void AccountModal::animate_reorder(const VisibleAccounts &t_before)
{
	const VisibleAccounts after = displayed_accounts();
	const float height = row_height(m_fonts);
	float offsets[max_visible_accounts]{};

	for (u32 i = 0; i < after.count; i += 1) {
		for (u32 j = 0; j < t_before.count; j += 1) {
			if (t_before.refs[j] != after.refs[i]) continue;

			offsets[i] = (static_cast<float>(j) - static_cast<float>(i)) * height + m_row_offsets[j];
			break;
		}
	}

	std::copy(std::begin(offsets), std::end(offsets), std::begin(m_row_offsets));
}

void AccountModal::reset_row_motion()
{
	m_drag = RowDrag{};
	std::fill(std::begin(m_row_offsets), std::end(m_row_offsets), 0.0f);
	m_raised_row.reset();
	m_lift_amount = 0.0f;
}

Rect AccountModal::back_badge_rect(const Layout &t_layout) const
{
	const Rect &art = t_layout.art_column;

	return Rect{art.x + close_badge_margin, art.y + close_badge_margin, close_badge_size, close_badge_size};
}

void AccountModal::request_tooltip()
{
	if (!is_blocking() || !has_game() || m_visible_games.is_open()) return;

	const Layout current = layout();
	const Rect back = back_badge_rect(current);

	if (m_mode != Mode::login_progress && back.contains(m_mouse)) {
		m_tooltip.request("Back", back);
		return;
	}

	if (m_mode == Mode::edit_account) {
		const Rect main = current.main_column;

		if (is_reveal_hit(main, m_mouse)) {
			m_tooltip.request(field(EditField::password).is_masked() ? "Show password" : "Hide password",
							  reveal_button_rect(main));
		}

		return;
	}

	if (m_mode == Mode::account_list) {
		request_row_tooltip(current);
	}
}

void AccountModal::request_row_tooltip(const Layout &t_layout)
{
	if (m_drag.lifted || m_drag.pressed_row) return;

	const Rect add = add_button_rect(t_layout.main_column);
	if (add.contains(m_mouse)) {
		m_tooltip.request("Add account", add);
		return;
	}

	const AccountRows rows = account_rows(t_layout);
	const i32 hovered_row = row_at(t_layout, rows, m_mouse);
	if (hovered_row < 0) return;

	const Rect row = row_rect(t_layout, rows, static_cast<u32>(hovered_row));
	const Rect favorite = favorite_button_rect(row);
	const Rect edit = edit_button_rect(row);
	const Rect remove = remove_button_rect(row);

	if (favorite.contains(m_mouse)) {
		const bool pinned = m_library.account(rows.accounts.refs[hovered_row]).favorite;
		m_tooltip.request(pinned ? "Unpin" : "Pin to top", favorite);
	} else if (edit.contains(m_mouse)) {
		m_tooltip.request("Edit account", edit);
	} else if (remove.contains(m_mouse)) {
		const bool armed = m_armed_delete == rows.accounts.refs[hovered_row];
		m_tooltip.request(armed ? "Click again to delete" : "Delete account", remove);
	}
}

void AccountModal::update(float t_delta_seconds)
{
	const bool morphing = m_art_source && has_game();
	const float morph_target = m_open ? 1.0f : 0.0f;

	if (!morphing) {
		m_morph_progress = morph_target;
	} else if (m_open || m_open_amount <= morph_return_open_amount) {
		m_morph_progress = animation::step_toward(m_morph_progress, morph_target, morph_seconds, t_delta_seconds);
	}

	const bool panel_shown = m_open && (!morphing || m_morph_progress >= panel_reveal_progress);
	const float scale_travel = m_window.size().x * 0.5f * (1.0f - panel_closed_scale);
	m_open_amount = animation::ease_toward(m_open_amount, panel_shown ? 1.0f : 0.0f, open_ease_rate, t_delta_seconds,
										   animation::settled_pixels / scale_travel);

	if (!m_open && m_open_amount == 0.0f && m_morph_progress == 0.0f) {
		m_game = -1;
	}

	if (m_deleted && !m_toasts.is_offering(CommandType::undo_delete)) {
		forget_deleted();
	}

	m_armed_seconds -= t_delta_seconds;
	if (m_armed_delete && m_armed_seconds <= 0.0f) {
		m_armed_delete.reset();
	}

	if (m_armed_delete || m_drag.lifted) {
		animation::request_frame();
	}

	switch (m_mode) {
		case Mode::account_list:
			m_search.update(t_delta_seconds);

			if (has_game()) {
				refresh_search();
			}

			m_rows_scroll.update(t_delta_seconds);
			update_row_drag(t_delta_seconds);
			break;

		case Mode::login_progress:
			m_login_seconds += t_delta_seconds;
			update_login_progress(t_delta_seconds);
			animation::request_frame();
			break;

		case Mode::edit_account:
			m_form_scroll.update(t_delta_seconds);

			for (TextInput &input : m_fields) {
				input.update(t_delta_seconds);
			}

			break;
	}

	m_visible_games.update(t_delta_seconds);
	m_region_list.update(t_delta_seconds, region_rect(layout().main_column), layout().inner);
	request_tooltip();
	m_tooltip.update(t_delta_seconds);
	m_login.update();
	record_login_result();

	if (m_queued_login && !m_login.is_active()) {
		const PendingLogin login = *std::exchange(m_queued_login, std::nullopt);

		m_login_seconds = 0.0f;
		start_login(login);
	}
}

bool AccountModal::on_pointer_down(Vec2 t_point)
{
	if (!is_blocking()) return false;

	if (m_open_amount < interactive_open_amount) {
		m_press_swallowed = true;
		return true;
	}

	const Layout current = layout();

	if (m_mode == Mode::edit_account && m_region_list.is_open()) {
		m_region_list.on_pointer_down(t_point);
		return true;
	}

	if (m_mode == Mode::edit_account) {
		if (!m_visible_games.is_open() && m_form_scroll.on_pointer_down(t_point, form_scroll(current.main_column))) {
			return true;
		}

		const i32 pressed = field_at(current.main_column, t_point);

		if (pressed >= 0) {
			focus_field(pressed);
			m_fields[pressed].on_pointer_down(m_fonts.body(), field_text_rect(current.main_column, pressed), t_point.x);
		}
	} else if (m_mode == Mode::account_list && has_game()) {
		handle_list_press(current, t_point);
	}

	return true;
}

void AccountModal::handle_list_press(const Layout &t_layout, Vec2 t_point)
{
	const AccountRows rows = account_rows(t_layout);
	if (m_rows_scroll.on_pointer_down(t_point, rows.scroll)) return;

	const Rect search = search_rect(t_layout.main_column);
	const bool over_clear = !m_search.value().empty() && search_clear_rect(search).contains(t_point);

	if (search.contains(t_point) && !over_clear) {
		m_search.set_focused(true);
		m_search.on_pointer_down(m_fonts.secondary(), search_text_rect(search), t_point.x);
		return;
	}

	if (!over_clear) {
		m_search.set_focused(false);
	}

	const i32 row = row_at(t_layout, rows, t_point);
	if (row < 0 || is_row_button_hit(row_rect(t_layout, rows, static_cast<u32>(row)), t_point)) return;

	m_drag.pressed_row = static_cast<u32>(row);
	m_drag.press_point = t_point;
}

bool AccountModal::on_pointer_move(Vec2 t_point)
{
	if (!is_blocking()) return false;

	if (m_region_list.is_open()) {
		m_region_list.on_pointer_move(t_point);
		return true;
	}

	const Layout current = layout();

	for (u32 i = 0; i < field_count; i += 1) {
		if (m_fields[i].is_selecting()) {
			m_fields[i].on_pointer_move(m_fonts.body(), field_text_rect(current.main_column, i), t_point.x);
		}
	}

	if (m_search.is_selecting()) {
		m_search.on_pointer_move(m_fonts.secondary(), search_text_rect(search_rect(current.main_column)), t_point.x);
	}

	if (m_rows_scroll.is_dragging()) {
		m_rows_scroll.on_pointer_move(t_point.y, account_rows(current).scroll);
	}

	if (m_form_scroll.is_dragging()) {
		m_form_scroll.on_pointer_move(t_point.y, form_scroll(current.main_column));
	}

	if (m_drag.pressed_row && !m_drag.lifted && m_search.value().empty()) {
		const float dx = t_point.x - m_drag.press_point.x;
		const float dy = t_point.y - m_drag.press_point.y;
		const AccountRows rows = account_rows(current);

		if (dx * dx + dy * dy > drag_threshold * drag_threshold && *m_drag.pressed_row < rows.accounts.count) {
			lift_row(rows, *m_drag.pressed_row, t_point);
		}
	}

	return true;
}

bool AccountModal::on_pointer_up(Vec2 t_point)
{
	if (std::exchange(m_press_swallowed, false)) return true;

	if (m_region_list.is_open()) {
		if (const std::optional<u32> chosen = m_region_list.on_pointer_up(t_point)) {
			choose_region(*chosen);
		}

		return true;
	}

	if (m_rows_scroll.is_dragging()) {
		m_rows_scroll.on_pointer_up();
		return true;
	}

	if (m_form_scroll.is_dragging()) {
		m_form_scroll.on_pointer_up();
		return true;
	}

	if (m_drag.lifted) {
		drop_row();
		return true;
	}

	m_drag.pressed_row.reset();

	if (!is_blocking()) return false;

	bool ended_text_selection = m_search.is_selecting();
	m_search.on_pointer_up();

	for (TextInput &input : m_fields) {
		ended_text_selection = ended_text_selection || input.is_selecting();
		input.on_pointer_up();
	}

	if (ended_text_selection) return true;

	const Layout current = layout();
	const bool clicked_away = !current.panel.contains(t_point);
	const bool clicked_close = back_badge_rect(current).contains(t_point);

	if (m_mode != Mode::login_progress && (clicked_away || clicked_close)) {
		close();
		return true;
	}

	switch (m_mode) {
		case Mode::account_list:
			if (has_game()) {
				handle_list_click(current, t_point);
			}

			break;

		case Mode::login_progress:
			if (primary_button_rect(current.footer).contains(t_point)) {
				cancel_login();
				m_mode = Mode::account_list;
			}

			break;

		case Mode::edit_account:
			handle_edit_click(current, t_point);
			break;
	}

	return true;
}

void AccountModal::handle_list_click(const Layout &t_layout, Vec2 t_point)
{
	const std::optional<AccountRef> armed = std::exchange(m_armed_delete, std::nullopt);

	const Rect main = t_layout.main_column;
	const Rect search = search_rect(main);

	if (!m_search.value().empty() && search_clear_rect(search).contains(t_point)) {
		clear_search();
		m_search.set_focused(true);
		return;
	}

	if (search.contains(t_point)) return;

	if (add_button_rect(main).contains(t_point)) {
		start_adding();
		return;
	}

	const AccountRows rows = account_rows(t_layout);

	if (rows.accounts.count == 0 && m_search.value().empty() && empty_state(rows.region).button.contains(t_point)) {
		start_adding();
		return;
	}

	if (rows.region.contains(t_point)) {
		const i32 row = row_at(t_layout, rows, t_point);

		if (row < 0) {
			m_selected.reset();
		} else {
			const Rect rect = row_rect(t_layout, rows, static_cast<u32>(row));
			const AccountRef account = rows.accounts.refs[row];

			if (remove_button_rect(rect).contains(t_point)) {
				m_armed_delete = armed;
				arm_or_delete(account);
				return;
			}

			if (edit_button_rect(rect).contains(t_point)) {
				start_editing(account);
				return;
			}

			if (favorite_button_rect(rect).contains(t_point)) {
				toggle_favorite(account);
				return;
			}

			m_selected = account;
		}
	}

	if (selected_row(rows.accounts) >= 0 && primary_button_rect(t_layout.footer).contains(t_point)) {
		request_login(static_cast<u32>(m_game), *m_selected);
	}
}

void AccountModal::handle_edit_click(const Layout &t_layout, Vec2 t_point)
{
	const Rect main = t_layout.main_column;

	if (m_visible_games.is_open()) {
		if (!m_visible_games.on_pointer_down(t_point)) {
			m_visible_games.close();
		}

		return;
	}

	if (is_region_hit(main, t_point)) {
		open_region_list();
		return;
	}

	if (is_show_in_hit(main, t_point)) {
		m_visible_games.open(show_in_rect(main), t_layout.inner, static_cast<u32>(m_game));
		return;
	}

	if (is_reveal_hit(main, t_point)) {
		TextInput &password = field(EditField::password);
		password.set_masked(!password.is_masked());
		return;
	}

	focus_field(-1);

	const Rect save = primary_button_rect(t_layout.footer);

	if (cancel_button_rect(save).contains(t_point)) {
		m_mode = Mode::account_list;
	} else if (has_changes() && save.contains(t_point)) {
		if (can_save()) {
			save_edit();
		} else {
			m_show_required = true;
		}
	}

	m_armed_delete.reset();
}

bool AccountModal::on_right_click(Vec2 t_point)
{
	if (!is_blocking()) return false;
	if (m_open_amount < interactive_open_amount) return true;

	const Layout current = layout();

	if (m_mode == Mode::edit_account) {
		const i32 clicked = field_at(current.main_column, t_point);

		if (clicked >= 0) {
			focus_field(clicked);
			m_fields[clicked].on_right_click(m_fonts.body(), field_text_rect(current.main_column, clicked), t_point.x);
			m_commands.push(
				Command{.type = CommandType::show_text_menu, .position = t_point, .text_input = &m_fields[clicked]});
		}

		return true;
	}

	if (m_mode != Mode::account_list || !has_game() || m_drag.lifted) return true;

	const Rect search = search_rect(current.main_column);

	if (search.contains(t_point)) {
		m_search.set_focused(true);
		m_search.on_right_click(m_fonts.secondary(), search_text_rect(search), t_point.x);
		m_commands.push(Command{.type = CommandType::show_text_menu, .position = t_point, .text_input = &m_search});
		return true;
	}

	const AccountRows rows = account_rows(current);
	const i32 row = row_at(current, rows, t_point);

	if (row >= 0) {
		m_selected = rows.accounts.refs[row];
		m_commands.push(Command{.type = CommandType::show_account_menu, .index = row, .position = t_point});
	}

	return true;
}

bool AccountModal::on_scroll(Vec2, float t_wheel_delta)
{
	if (!is_blocking()) return false;
	if (m_open_amount < interactive_open_amount) return true;

	if (m_region_list.is_open()) {
		m_region_list.on_scroll(t_wheel_delta);
		return true;
	}

	if (m_mode == Mode::account_list) {
		m_rows_scroll.on_scroll(t_wheel_delta, account_rows(layout()).scroll);
	} else if (m_mode == Mode::edit_account && !m_visible_games.is_open()) {
		m_form_scroll.on_scroll(t_wheel_delta, form_scroll(layout().main_column));
	}

	return true;
}

bool AccountModal::handle_list_key(u32 t_key)
{
	if (m_drag.lifted) return true;

	switch (t_key) {
		case VK_UP:
		case VK_DOWN:
			select_step(t_key == VK_DOWN ? 1 : -1);
			return true;

		case VK_RETURN:
			if (selected_row(displayed_accounts()) >= 0) {
				request_login(static_cast<u32>(m_game), *m_selected);
			}

			return true;

		default:
			break;
	}

	const bool control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

	if (control && t_key == 'F' && is_search_visible()) {
		m_search.set_focused(true);
		m_search.apply(TextEdit::select_all);
		return true;
	}

	if (m_search.is_focused()) {
		m_search.on_key_down(t_key);
		return true;
	}

	if (t_key == VK_DELETE) {
		if (selected_row(displayed_accounts()) >= 0) {
			arm_or_delete(*m_selected);
		}

		return true;
	}

	if (t_key == VK_BACK && !m_search.value().empty() && is_search_visible()) {
		m_search.set_focused(true);
		m_search.on_key_down(t_key);
		return true;
	}

	return false;
}

bool AccountModal::on_key_down(u32 t_key)
{
	if (!is_blocking()) return false;

	if (m_region_list.is_open()) {
		if (const std::optional<u32> chosen = m_region_list.on_key_down(t_key)) {
			choose_region(*chosen);
		}

		return true;
	}

	if (t_key == VK_ESCAPE) {
		if (m_armed_delete) {
			m_armed_delete.reset();
		} else if (m_mode == Mode::edit_account) {
			m_mode = Mode::account_list;
		} else if (m_mode == Mode::account_list && m_drag.lifted) {
			cancel_row_drag();
		} else if (m_mode == Mode::account_list && !m_search.value().empty()) {
			clear_search();
		} else if (m_mode == Mode::account_list && m_search.is_focused()) {
			m_search.set_focused(false);
		} else if (m_mode == Mode::account_list && m_selected) {
			m_selected.reset();
		} else if (m_mode == Mode::account_list) {
			close();
		}

		return true;
	}

	if (m_mode == Mode::account_list && has_game()) {
		handle_list_key(t_key);
	} else if (m_mode == Mode::edit_account && t_key == VK_TAB) {
		const auto count = static_cast<i32>(field_count);
		const i32 step = (GetKeyState(VK_SHIFT) & 0x8000) != 0 ? count - 1 : 1;
		const i32 current = focused_field();
		focus_field(current < 0 ? 0 : (current + step) % count);
		reveal_field(focused_field());
	} else if (m_mode == Mode::edit_account) {
		for (TextInput &input : m_fields) {
			input.on_key_down(t_key);
		}
	}

	return true;
}

bool AccountModal::on_char(u32 t_character)
{
	if (!is_blocking()) return false;

	if (m_mode == Mode::edit_account) {
		if (m_region_list.is_open()) {
			m_region_list.on_char(t_character);
			return true;
		}

		for (TextInput &input : m_fields) {
			input.on_char(t_character);
		}
	} else if (m_mode == Mode::account_list && has_game() && !m_drag.lifted) {
		const bool printable = t_character >= 32 && t_character != 127;

		if (printable && !m_search.is_focused() && is_search_visible()) {
			m_search.set_focused(true);
		}

		if (m_search.is_focused()) {
			m_search.on_char(t_character);
		}
	}

	return true;
}

CursorKind AccountModal::list_cursor(const Layout &t_layout) const
{
	if (m_drag.lifted) return CursorKind::drag;

	const Rect main = t_layout.main_column;
	const Rect search = search_rect(main);

	if (!m_search.value().empty() && search_clear_rect(search).contains(m_mouse)) return CursorKind::hand;
	if (search.contains(m_mouse)) return CursorKind::ibeam;
	if (add_button_rect(main).contains(m_mouse)) return CursorKind::hand;

	const AccountRows rows = account_rows(t_layout);
	if (rows.accounts.count == 0 && m_search.value().empty() && empty_state(rows.region).button.contains(m_mouse)) {
		return CursorKind::hand;
	}

	if (row_at(t_layout, rows, m_mouse) >= 0 || m_rows_scroll.is_over_track(m_mouse, rows.scroll)) {
		return CursorKind::hand;
	}

	const bool over_login = primary_button_rect(t_layout.footer).contains(m_mouse);
	const bool can_login = selected_row(rows.accounts) >= 0 && !m_login.is_active();

	return can_login && over_login ? CursorKind::hand : CursorKind::arrow;
}

CursorKind AccountModal::edit_cursor(const Layout &t_layout) const
{
	const Rect main = t_layout.main_column;

	if (m_region_list.is_open()) return m_region_list.cursor(m_mouse);
	if (m_visible_games.is_open()) return m_visible_games.cursor(m_mouse);
	if (is_show_in_hit(main, m_mouse) || is_region_hit(main, m_mouse) || is_reveal_hit(main, m_mouse)) {
		return CursorKind::hand;
	}

	if (field_at(main, m_mouse) >= 0) return CursorKind::ibeam;
	if (m_form_scroll.is_over_track(m_mouse, form_scroll(main))) return CursorKind::hand;

	const Rect save = primary_button_rect(t_layout.footer);
	const bool over_button = cancel_button_rect(save).contains(m_mouse) || (has_changes() && save.contains(m_mouse));

	return over_button ? CursorKind::hand : CursorKind::arrow;
}

CursorKind AccountModal::cursor() const
{
	if (!is_blocking()) return CursorKind::arrow;
	if (m_rows_scroll.is_dragging() || m_form_scroll.is_dragging()) return CursorKind::drag;
	if (m_search.is_selecting()) return CursorKind::ibeam;

	for (const TextInput &input : m_fields) {
		if (input.is_selecting()) return CursorKind::ibeam;
	}

	const Layout current = layout();

	if (m_mode != Mode::login_progress && back_badge_rect(current).contains(m_mouse)) return CursorKind::hand;

	switch (m_mode) {
		case Mode::account_list:
			return has_game() ? list_cursor(current) : CursorKind::arrow;

		case Mode::login_progress:
			return primary_button_rect(current.footer).contains(m_mouse) ? CursorKind::hand : CursorKind::arrow;

		case Mode::edit_account:
			return edit_cursor(current);
	}

	return CursorKind::arrow;
}

void AccountModal::draw_chrome(DrawList &t_draw_list, const Layout &t_layout, bool t_with_art, u8 t_alpha) const
{
	const Game &game = m_library.game(static_cast<u32>(m_game));
	const Rect art = t_layout.art_column;
	const bool docked = is_docked();
	const float radius = docked ? 0.0f : panel_radius;

	if (docked) {
		t_draw_list.add_rect(t_layout.panel, faded(theme().surface, t_alpha));
	} else {
		controls::draw_panel_shadow(t_draw_list, t_layout.panel, panel_radius, m_open_amount);
		t_draw_list.add_bordered_rect(t_layout.panel, rounded(panel_radius), faded(theme().surface, t_alpha),
									  faded(theme().border, t_alpha), panel_border);

		const float highlight_inset = scaled_radius(panel_radius);
		t_draw_list.add_rect(
			Rect{t_layout.inner.x + highlight_inset, t_layout.inner.y, t_layout.inner.w - highlight_inset * 2.0f, 1.0f},
			faded(color_top_highlight, t_alpha));
	}

	const u8 art_alpha = m_art_source ? 255 : t_alpha;

	if (t_with_art && game.banner != nullptr) {
		t_draw_list.add_image(art, game.banner, faded(color_on_art, art_alpha),
							  rounded(std::max(0.0f, radius - panel_border), 0.0f, 0.0f, 0.0f),
							  cover_uv(art.w / art.h, game.banner->aspect()));
	} else if (t_with_art) {
		t_draw_list.add_rect(art, faded(game.accent, art_alpha));
	}

	t_draw_list.add_rect(Rect{art.right(), art.y, art_separator_width, art.h}, faded(theme().border, t_alpha));
}

void AccountModal::draw_back_badge(DrawList &t_draw_list, const Layout &t_layout, u8 t_alpha) const
{
	const Rect badge = back_badge_rect(t_layout);
	const auto badge_alpha = static_cast<u8>(badge.contains(m_mouse) ? 210 : 170);

	t_draw_list.add_rounded_rect(badge, rounded(badge.w * 0.5f),
								 faded(with_alpha(color_art_badge, badge_alpha), t_alpha));
	controls::draw_icon(t_draw_list, badge.centered(close_badge_icon_size, close_badge_icon_size),
						m_assets.get(Asset::icon_arrow_back), faded(color_on_art, t_alpha));
}

void AccountModal::draw_morphing_art(DrawList &t_draw_list, const Layout &t_layout, float t_scale) const
{
	const Game &game = m_library.game(static_cast<u32>(m_game));
	const ArtSource &source = *m_art_source;
	const float amount = 0.5f - 0.5f * std::cos(m_morph_progress * std::numbers::pi_v<float>);

	const Rect target = scaled_about(t_layout.art_column, t_layout.panel.center(), t_scale);
	const Rect art = lerp(source.rect, target, amount);
	const float target_corner = is_docked() ? 0.0f : std::max(0.0f, panel_radius - panel_border) * t_scale;
	const float leading_corner = lerp(source.radius, target_corner, amount);
	const float other_corners = lerp(source.radius, 0.0f, amount);
	const CornerRadii radii = rounded(leading_corner, other_corners, other_corners, other_corners);
	const float banner_share = source.is_icon ? std::clamp(amount / icon_crossfade_share, 0.0f, 1.0f) : 1.0f;
	const float flight = std::sin(amount * std::numbers::pi_v<float>);

	if (flight > 0.01f) {
		controls::draw_panel_shadow(t_draw_list, art, std::max(leading_corner, other_corners), flight);
	}

	const float frame = 1.0f - amount;
	const auto frame_alpha = static_cast<u8>(255.0f * frame);

	if (source.glow > 0.0f && frame_alpha > 0) {
		t_draw_list.add_banner_glow(art, other_corners, source.glow, faded(source.glow_color, frame_alpha));
	}

	if (source.border > 0.0f && frame_alpha > 0) {
		t_draw_list.add_rounded_rect(art, radii, faded(source.border_color, frame_alpha));
	}

	const float inset = source.border * frame;
	const Rect image = art.inset(inset);
	const CornerRadii image_radii =
		rounded(std::max(0.0f, leading_corner - inset), std::max(0.0f, other_corners - inset),
				std::max(0.0f, other_corners - inset), std::max(0.0f, other_corners - inset));

	if (game.banner != nullptr) {
		t_draw_list.add_image(image, game.banner, faded(color_on_art, static_cast<u8>(255.0f * banner_share)),
							  image_radii, cover_uv(image.w / image.h, game.banner->aspect()));
	} else {
		t_draw_list.add_rounded_rect(image, image_radii, faded(game.accent, static_cast<u8>(255.0f * banner_share)));
	}

	if (source.is_icon && game.icon != nullptr && banner_share < 1.0f) {
		t_draw_list.add_image(image, game.icon, faded(color_on_art, static_cast<u8>(255.0f * (1.0f - banner_share))),
							  image_radii, cover_uv(image.w / image.h, game.icon->aspect()));
	}
}

void AccountModal::draw_section_title(DrawList &t_draw_list, Rect t_main, std::string_view t_title, u8 t_alpha) const
{
	const Font &font = m_fonts.body();
	const Rect header{t_main.x, t_main.y, t_main.w, header_height(m_fonts)};

	draw_text(t_draw_list, font, Vec2{t_main.x + row_padding, font.centered_baseline(header)}, t_title,
			  faded(theme().text, t_alpha));
	t_draw_list.add_rect(Rect{t_main.x + row_padding, header.bottom(), t_main.w - row_padding * 2.0f, 1.0f},
						 faded(theme().separator, t_alpha));
}

void AccountModal::draw_search(DrawList &t_draw_list, Rect t_main, u8 t_alpha)
{
	const Rect search = search_rect(t_main);
	if (search.w <= 0.0f) return;

	const Theme &colors = theme();
	const bool focused = m_search.is_focused();
	const bool has_query = !m_search.value().empty();

	controls::draw_field(t_draw_list, search, search.h * 0.5f, focused ? colors.text_dim : colors.control,
						 focused ? colors.row_hover : colors.field, t_alpha);

	const Rect icon{search.x + search_inset, search.center().y - search_icon_size * 0.5f, search_icon_size,
					search_icon_size};
	controls::draw_magnifier(t_draw_list, icon,
							 faded(focused || has_query ? colors.text_dim : colors.text_faint, t_alpha));
	m_search.draw(t_draw_list, m_fonts.secondary(), search_text_rect(search), faded(colors.text, t_alpha),
				  faded(m_settings.accent, t_alpha), search);

	if (!has_query) return;

	const Rect clear = search_clear_rect(search);
	controls::draw_x(t_draw_list, clear.inset(1.5f),
					 faded(clear.contains(m_mouse) ? colors.text : colors.text_faint, t_alpha));
}

void AccountModal::draw_row_details(DrawList &t_draw_list, Rect t_row, float t_baseline, float t_max_width,
									const Account &t_account, u8 t_alpha) const
{
	const Font &secondary = m_fonts.secondary();
	const std::string_view note = t_account.note;

	char relative[32];
	char last_used[48];
	std::string_view when;

	if (t_account.last_used != 0) {
		when = relative_time(t_account.last_used, std::time(nullptr), relative);

		if (note.empty()) {
			const int written = std::snprintf(last_used, sizeof(last_used), "Last used %.*s",
											  static_cast<int>(when.size()), when.data());
			when = std::string_view{last_used, static_cast<usize>(std::max(written, 0))};
		}
	}

	if (note.empty()) {
		draw_text_truncated(t_draw_list, secondary, Vec2{t_row.x, t_baseline}, when, t_max_width,
							faded(theme().text_faint, t_alpha));
		return;
	}

	const float when_width =
		when.empty() ? 0.0f : text_width(secondary, when) + detail_dot_gap * 2.0f + detail_dot_size;
	const float note_width = std::min(text_width(secondary, note), std::max(0.0f, t_max_width - when_width));

	draw_text_truncated(t_draw_list, secondary, Vec2{t_row.x, t_baseline}, note, note_width + 0.5f,
						faded(theme().text_dim, t_alpha));

	if (when.empty()) return;

	const float dot_x = t_row.x + note_width + detail_dot_gap;
	const float dot_y = t_baseline - secondary.ascent() * 0.33f - detail_dot_size * 0.5f;
	const float when_x = dot_x + detail_dot_size + detail_dot_gap;

	t_draw_list.add_rounded_rect(Rect{dot_x, dot_y, detail_dot_size, detail_dot_size}, rounded(detail_dot_size * 0.5f),
								 faded(theme().text_faint, t_alpha));
	draw_text_truncated(t_draw_list, secondary, Vec2{when_x, t_baseline}, when, t_row.x + t_max_width - when_x,
						faded(theme().text_faint, t_alpha));
}

void AccountModal::draw_account_row(DrawList &t_draw_list, Rect t_main, Rect t_row, const Account &t_account,
									bool t_selected, bool t_raised, float t_delete_countdown, u8 t_alpha) const
{
	const Rect highlight = row_highlight(t_row);
	const bool interactive = !m_drag.lifted && !t_raised;
	const bool hovered = interactive && highlight.contains(m_mouse);

	if (t_raised && m_lift_amount > 0.0f) {
		const auto lift_alpha = static_cast<u8>(t_alpha * m_lift_amount);

		controls::draw_popup_shadow(t_draw_list, highlight, row_radius, m_lift_amount * t_alpha / 255.0f);
		t_draw_list.add_bordered_rect(highlight, rounded(row_radius), faded(theme().popup, lift_alpha),
									  faded(theme().border, lift_alpha), 1.0f);
	}

	if (t_selected) {
		t_draw_list.add_rounded_rect(highlight, rounded(row_radius), faded(theme().row_selected, t_alpha));
		t_draw_list.add_rounded_rect(Rect{t_main.x + 8.0f, highlight.y, 3.0f, highlight.h}, rounded(1.5f),
									 faded(m_settings.accent, t_alpha));
	} else if (hovered) {
		t_draw_list.add_rounded_rect(highlight, rounded(row_radius), faded(theme().row_hover, t_alpha));
	}

	const Font &body = m_fonts.body();
	const Font &secondary = m_fonts.secondary();
	const Rect favorite = favorite_button_rect(t_row);
	const float text_limit = favorite.x - row_button_gap - t_row.x;
	const bool has_details = t_account.note[0] != '\0' || t_account.last_used != 0;

	const float block_height = body.line_height() + row_line_gap + secondary.line_height();
	const float block_y = t_row.y + (t_row.h - block_height) * 0.5f;
	const float username_baseline = has_details ? block_y + body.ascent() : body.centered_baseline(t_row);

	const std::string_view region = t_account.region;
	const float chip_space = region.empty() ? 0.0f : region_chip_width(secondary, region) + region_chip_gap;
	const float username_limit = std::max(0.0f, text_limit - chip_space);

	draw_text_truncated(t_draw_list, body, Vec2{t_row.x, username_baseline}, t_account.username, username_limit,
						faded(theme().text, t_alpha));

	if (!region.empty()) {
		const float username_width = std::min(text_width(body, t_account.username), username_limit);
		const float line_center = username_baseline - body.ascent() + body.line_height() * 0.5f;
		draw_region_chip(t_draw_list, secondary, t_row.x + username_width + region_chip_gap, line_center, region,
						 t_alpha);
	}

	if (has_details) {
		const float details_baseline = block_y + body.line_height() + row_line_gap + secondary.ascent();
		draw_row_details(t_draw_list, t_row, details_baseline, text_limit, t_account, t_alpha);
	}

	const auto separator_alpha = static_cast<u8>(t_alpha * (t_raised ? 1.0f - m_lift_amount : 1.0f));
	t_draw_list.add_rect(Rect{t_row.x, t_row.bottom() - 1.0f, t_row.w, 1.0f},
						 faded(theme().separator, separator_alpha));

	const Rect edit = edit_button_rect(t_row);
	const Rect remove = remove_button_rect(t_row);
	const bool favorite_hovered = interactive && favorite.contains(m_mouse);
	const bool edit_hovered = interactive && edit.contains(m_mouse);
	const bool armed = t_delete_countdown > 0.0f;
	const bool pointer_on_remove = interactive && remove.contains(m_mouse);
	const bool remove_hovered = armed || pointer_on_remove;

	if (favorite_hovered) {
		controls::draw_circular_hover(t_draw_list, favorite, m_settings.accent, theme().control_hover, t_alpha);
	}

	if (edit_hovered) {
		controls::draw_circular_hover(t_draw_list, edit, m_settings.accent, theme().control_hover, t_alpha);
	}

	const Color danger = armed ? controls::confirm_red() : theme().error;

	if (remove_hovered) {
		const float tint = armed && pointer_on_remove ? armed_danger_tint : danger_tint;
		controls::draw_circular_hover(t_draw_list, remove, danger, mix(theme().surface, danger, tint), t_alpha);
	}

	if (t_account.favorite) {
		controls::draw_favorite(t_draw_list, m_assets, favorite.inset(row_icon_inset), true,
								faded(m_settings.accent, t_alpha));
	} else if (hovered) {
		controls::draw_favorite(t_draw_list, m_assets, favorite.inset(row_icon_inset), false,
								faded(favorite_hovered ? theme().text : theme().text_faint, t_alpha));
	}

	controls::draw_icon(t_draw_list, edit.inset(row_icon_inset), m_assets.get(Asset::icon_edit),
						faded(edit_hovered ? theme().text : theme().text_dim, t_alpha));
	controls::draw_x(t_draw_list, remove, faded(remove_hovered ? danger : theme().text_dim, t_alpha));

	if (armed) {
		controls::draw_circular_countdown(t_draw_list, remove, t_delete_countdown, faded(danger, t_alpha));
	}
}

AccountModal::EmptyState AccountModal::empty_state(Rect t_region) const
{
	const Font &body = m_fonts.body();
	const Font &secondary = m_fonts.secondary();
	const float button_height = action_button_height(m_fonts);
	const float stack_height = empty_icon_size + empty_gap + body.line_height() + empty_line_gap +
							   secondary.line_height() + empty_gap + button_height;
	const float center_x = t_region.center().x;
	const float top = t_region.center().y - stack_height * 0.5f;

	EmptyState state{};
	state.icon = Rect{center_x - empty_icon_size * 0.5f, top, empty_icon_size, empty_icon_size};
	state.title_baseline = state.icon.bottom() + empty_gap + body.ascent();
	state.hint_baseline = state.icon.bottom() + empty_gap + body.line_height() + empty_line_gap + secondary.ascent();
	state.button = Rect{center_x - empty_button_width * 0.5f,
						state.icon.bottom() + empty_gap + body.line_height() + empty_line_gap +
							secondary.line_height() + empty_gap,
						empty_button_width, button_height};

	return state;
}

void AccountModal::draw_empty_state(DrawList &t_draw_list, Rect t_region, u8 t_alpha) const
{
	const Game &game = m_library.game(static_cast<u32>(m_game));
	const EmptyState state = empty_state(t_region);
	const Font &body = m_fonts.body();
	const Font &secondary = m_fonts.secondary();
	constexpr std::string_view title = "No accounts yet";
	constexpr std::string_view hint = "Add one to log in with a single click.";

	if (game.icon != nullptr) {
		t_draw_list.add_image(state.icon, game.icon, faded(color_on_art, t_alpha), rounded(empty_icon_radius));
	} else {
		t_draw_list.add_rounded_rect(state.icon, rounded(empty_icon_radius), faded(game.accent, t_alpha));
	}

	draw_text(t_draw_list, body, Vec2{t_region.center().x - text_width(body, title) * 0.5f, state.title_baseline},
			  title, faded(theme().text, t_alpha));
	draw_text(t_draw_list, secondary,
			  Vec2{t_region.center().x - text_width(secondary, hint) * 0.5f, state.hint_baseline}, hint,
			  faded(theme().text_dim, t_alpha));
	controls::draw_button(t_draw_list, body, state.button, "Add Account", controls::ButtonStyle::accent,
						  m_settings.accent, true, state.button.contains(m_mouse), t_alpha);
}

void AccountModal::draw_no_matches(DrawList &t_draw_list, Rect t_region, u8 t_alpha) const
{
	const Font &body = m_fonts.body();
	const Font &secondary = m_fonts.secondary();
	constexpr std::string_view title = "No matches";
	constexpr std::string_view hint = "Try a different name or note.";

	const float stack_height = body.line_height() + empty_line_gap + secondary.line_height();
	const float top = t_region.center().y - stack_height * 0.5f;
	const float center_x = t_region.center().x;

	draw_text(t_draw_list, body, Vec2{center_x - text_width(body, title) * 0.5f, top + body.ascent()}, title,
			  faded(theme().text_dim, t_alpha));
	draw_text(t_draw_list, secondary,
			  Vec2{center_x - text_width(secondary, hint) * 0.5f,
				   top + body.line_height() + empty_line_gap + secondary.ascent()},
			  hint, faded(theme().text_faint, t_alpha));
}

void AccountModal::draw_account_list(DrawList &t_draw_list, const Layout &t_layout, u8 t_alpha)
{
	const Rect main = t_layout.main_column;
	draw_section_title(t_draw_list, main, accounts_title, t_alpha);
	draw_search(t_draw_list, main, t_alpha);

	const Rect add = add_button_rect(main);
	const bool add_hovered = add.contains(m_mouse);

	if (add_hovered) {
		controls::draw_circular_hover(t_draw_list, add, m_settings.accent, theme().control_hover, t_alpha);
	}

	controls::draw_icon(t_draw_list, add.centered(24.0f, 24.0f), m_assets.get(Asset::icon_add),
						faded(add_hovered ? theme().text : theme().text_dim, t_alpha));

	const AccountRows rows = account_rows(t_layout);

	if (rows.accounts.count == 0) {
		if (m_search.value().empty()) {
			draw_empty_state(t_draw_list, rows.region, t_alpha);
		} else {
			draw_no_matches(t_draw_list, rows.region, t_alpha);
		}

		return;
	}

	const i32 selected = selected_row(rows.accounts);

	t_draw_list.push_clip(rows.region);

	for (u32 i = 0; i < rows.accounts.count; i += 1) {
		if (m_raised_row == i) continue;

		const Rect row = row_rect(t_layout, rows, i);
		if (!row.overlaps_vertically(rows.region)) continue;

		draw_account_row(t_draw_list, main, row, m_library.account(rows.accounts.refs[i]),
						 static_cast<i32>(i) == selected, false, delete_countdown(rows.accounts.refs[i]), t_alpha);
	}

	if (m_raised_row && *m_raised_row < rows.accounts.count) {
		const u32 raised = *m_raised_row;
		const float top = m_drag.lifted ? lifted_top(rows) : raised * rows.row_height + m_row_offsets[raised];

		draw_account_row(t_draw_list, main, row_rect_at(t_layout, rows, top),
						 m_library.account(rows.accounts.refs[raised]), static_cast<i32>(raised) == selected, true,
						 delete_countdown(rows.accounts.refs[raised]), t_alpha);
	}

	t_draw_list.pop_clip();

	m_rows_scroll.draw_edge_fade(t_draw_list, rows.region, rows.scroll, faded(theme().surface, t_alpha));
	m_rows_scroll.draw(t_draw_list, rows.scroll, m_mouse, t_alpha);
}

std::string_view AccountModal::login_status() const
{
	if (m_queued_login) return "Switching account...";

	const LoginStage stage = m_login.stage();
	if (LoginAttempt::is_terminal(stage) && !m_login.terminal_message().empty()) return m_login.terminal_message();

	return stage_message(stage);
}

void AccountModal::update_login_progress(float t_delta_seconds)
{
	const LoginStage stage = m_queued_login ? LoginStage::idle : m_login.stage();
	const bool finished = !m_queued_login && LoginAttempt::is_terminal(stage);

	if (stage != m_progress_stage) {
		m_progress_stage = stage;
		m_stage_seconds = 0.0f;
	}

	m_stage_seconds += t_delta_seconds;

	const std::string_view status = login_status();
	if (status != std::string_view{m_status_to}) {
		copy_to(m_status_to, m_status_from);
		copy_to(status, m_status_to);
		m_status_change = 0.0f;
	}

	if (const std::optional<StageSpan> span = stage_span(stage)) {
		const float creep = 1.0f - std::exp(-m_stage_seconds / progress_creep_seconds);
		const float target = span->start + (span->end - span->start) * creep;
		m_login_progress = animation::ease_toward(m_login_progress, std::max(target, m_login_progress),
												  progress_ease_rate, t_delta_seconds);
	}

	m_status_change = animation::ease_toward(m_status_change, 1.0f, status_ease_rate, t_delta_seconds);
	m_login_outcome =
		animation::ease_toward(m_login_outcome, finished ? 1.0f : 0.0f, outcome_ease_rate, t_delta_seconds);
}

void AccountModal::draw_login_progress(DrawList &t_draw_list, Rect t_main, u8 t_alpha) const
{
	const LoginStage stage = m_login.stage();
	const bool finished = !m_queued_login && LoginAttempt::is_terminal(stage);
	const Theme &colors = theme();
	const Font &body = m_fonts.body();
	const Font &secondary = m_fonts.secondary();

	const float width = std::min(t_main.w - row_padding * 2.0f, progress_max_width);
	const Rect bar{snapped_to_pixel(t_main.center().x - width * 0.5f), snapped_to_pixel(t_main.center().y), width,
				   progress_bar_height};
	const float status_baseline = bar.y - progress_text_gap;
	const float step_baseline = bar.bottom() + progress_text_gap + secondary.ascent() * cap_height_share;
	const float alpha_scale = t_alpha / 255.0f;

	const auto draw_status = [&](std::string_view t_text, float t_opacity, float t_offset) {
		if (t_text.empty() || t_opacity <= 0.0f) return;

		std::string_view lines[max_message_lines];
		const u32 line_count = wrap_text(body, t_text, width, lines);
		const auto alpha = static_cast<u8>(t_alpha * t_opacity);
		float baseline =
			status_baseline + t_offset - static_cast<float>(line_count > 0 ? line_count - 1 : 0) * body.line_height();

		for (const std::string_view line : std::span{lines, line_count}) {
			draw_text(t_draw_list, body,
					  Vec2{snapped_to_pixel(bar.center().x - text_width(body, line) * 0.5f), baseline}, line,
					  faded(colors.text, alpha));
			baseline += body.line_height();
		}
	};

	draw_status(m_status_from, 1.0f - m_status_change, -progress_text_rise * m_status_change);
	draw_status(m_status_to, m_status_change, progress_text_rise * (1.0f - m_status_change));

	const Color outcome = stage == LoginStage::success ? colors.success : colors.error;
	const Color fill_color = finished ? mix(m_settings.accent, outcome, m_login_outcome) : m_settings.accent;
	const Rect fill{bar.x, bar.y, bar.w * std::clamp(m_login_progress, 0.0f, 1.0f), bar.h};

	t_draw_list.add_rounded_rect(bar, rounded(bar.h * 0.5f), faded(colors.control, t_alpha));

	if (fill.w > 0.5f) {
		t_draw_list.add_shadow(fill, bar.h * 0.5f, progress_glow_blur,
							   with_alpha(fill_color, static_cast<u8>(progress_glow_alpha * alpha_scale)));
		t_draw_list.add_rounded_rect(fill, rounded(bar.h * 0.5f), faded(fill_color, t_alpha));

		if (!finished) {
			const float travel = std::fmod(m_login_seconds * sheen_passes_per_second, 1.0f);
			const float sheen_x = fill.x - sheen_width + travel * (fill.w + sheen_width);
			const float half = sheen_width * 0.5f;
			const Color edge = with_alpha(lightened(fill_color, 80), 0);
			const Color peak = with_alpha(lightened(fill_color, 80), static_cast<u8>(sheen_alpha * alpha_scale));

			t_draw_list.push_clip(fill);
			t_draw_list.add_gradient(Rect{sheen_x, fill.y, half, fill.h}, edge, peak, edge, peak);
			t_draw_list.add_gradient(Rect{sheen_x + half, fill.y, half, fill.h}, peak, edge, peak, edge);
			t_draw_list.pop_clip();
		}
	}

	const u32 step = m_queued_login ? 0 : stage_step(stage);
	if (step == 0) return;

	char label[24];
	const int written = std::snprintf(label, sizeof(label), "Step %u of %u", step, login_step_count);
	const std::string_view step_text{label, static_cast<usize>(std::max(written, 0))};

	draw_text(t_draw_list, secondary,
			  Vec2{snapped_to_pixel(bar.center().x - text_width(secondary, step_text) * 0.5f), step_baseline},
			  step_text, faded(colors.text_faint, static_cast<u8>(t_alpha * (1.0f - m_login_outcome))));
}

void AccountModal::draw_edit_form(DrawList &t_draw_list, Rect t_main, u8 t_alpha)
{
	draw_section_title(t_draw_list, t_main, m_edited ? "Edit Account" : "Add Account", t_alpha);

	const Theme &colors = theme();
	const Font &body = m_fonts.body();
	const Font &label_font = m_fonts.secondary();
	const Color accent = m_settings.accent;
	const Color text = faded(colors.text, t_alpha);
	const Color caret = faded(accent, t_alpha);
	const Color focused_label = mix(colors.text_faint, accent, 0.7f);
	const FormLayout form = form_layout(t_main);
	const ScrollGeometry scroll = form_scroll(t_main);
	const Rect reveal = reveal_button_rect(t_main);

	t_draw_list.push_clip(form.region);

	t_draw_list.add_bordered_rect(form.card, rounded(card_radius), faded(colors.field, t_alpha),
								  faded(colors.control, t_alpha), 1.0f);

	for (u32 slot = 1; slot <= field_count; slot += 1) {
		const float y = form.card.y + slot * (field_input_height(m_fonts) + 1.0f) - 1.0f;
		t_draw_list.add_rect(Rect{form.card.x + field_label_padding, y, form.card.w - field_label_padding * 2.0f, 1.0f},
							 faded(colors.separator, t_alpha));
	}

	for (u32 i = 0; i < field_count; i += 1) {
		TextInput &input = m_fields[i];
		const Rect row = form.rows[i];
		const bool focused = input.is_focused();
		const bool required = i == static_cast<u32>(EditField::username) || i == static_cast<u32>(EditField::password);
		const bool missing = m_show_required && required && input.value().empty();

		if (focused) {
			draw_focus_ring(t_draw_list, row.inset(focus_ring_inset_x, focus_ring_inset_y), focus_ring_radius,
							colors.field, accent, t_alpha);
		}

		Color label = colors.text_faint;
		if (missing) {
			label = colors.error;
		} else if (focused) {
			label = focused_label;
		}

		draw_text(t_draw_list, label_font, Vec2{row.x + field_label_padding, label_font.centered_baseline(row)},
				  field_specs[i].label, faded(label, t_alpha));
		input.draw(t_draw_list, body, field_text_rect(t_main, i), text, caret);

		if (missing) {
			constexpr std::string_view required_text = "Required";
			const float right = i == static_cast<u32>(EditField::password) ? reveal.x - reveal_button_margin
																		   : row.right() - field_label_padding;
			draw_text(t_draw_list, label_font,
					  Vec2{right - text_width(label_font, required_text), label_font.centered_baseline(row)},
					  required_text, faded(with_alpha(colors.error, 200), t_alpha));
		}
	}

	const Rect region = form.rows[region_row];
	const bool region_open = m_region_list.is_open();

	if (region_open) {
		draw_focus_ring(t_draw_list, region.inset(focus_ring_inset_x, focus_ring_inset_y), focus_ring_radius,
						colors.field, accent, t_alpha);
	}

	draw_text(t_draw_list, label_font, Vec2{region.x + field_label_padding, label_font.centered_baseline(region)},
			  region_label, faded(region_open ? focused_label : colors.text_faint, t_alpha));

	std::string_view region_value = m_region;
	for (const RegionOption &option : region_options) {
		if (!option.code.empty() && option.code == region_value) {
			region_value = option.label;
		}
	}

	const bool region_hovered = !region_open && is_region_hit(t_main, m_mouse);
	const Rect region_chevron{region.right() - chevron_margin - chevron_size.x,
							  region.center().y - chevron_size.y * 0.5f, chevron_size.x, chevron_size.y};
	const float region_value_x = region.x + label_column_width(m_fonts) + value_text_inset;

	draw_text_truncated(t_draw_list, body, Vec2{region_value_x, body.centered_baseline(region)},
						region_value.empty() ? std::string_view{"None"} : region_value,
						region_chevron.x - value_text_inset - region_value_x,
						region_value.empty() ? faded(colors.text_faint, t_alpha) : text);
	controls::draw_chevron(t_draw_list, region_chevron, region_open,
						   faded(region_open || region_hovered ? colors.text : colors.text_dim, t_alpha));

	controls::draw_eye(t_draw_list, m_assets, reveal, !field(EditField::password).is_masked(),
					   faded(is_reveal_hit(t_main, m_mouse) ? colors.text : colors.text_dim, t_alpha));

	const Rect show_in = form.rows[show_in_row];
	const bool open = m_visible_games.is_open();
	const bool hovered = !open && is_show_in_hit(t_main, m_mouse);

	if (open) {
		t_draw_list.add_shadow(show_in, card_radius, focus_glow_blur,
							   with_alpha(accent, static_cast<u8>(focus_glow_alpha * t_alpha / 255)));
	}

	t_draw_list.add_bordered_rect(show_in, rounded(card_radius),
								  faded(hovered ? mix(colors.field, colors.text, 0.04f) : colors.field, t_alpha),
								  faded(open ? accent : colors.control, t_alpha), 1.0f);
	draw_text(t_draw_list, label_font, Vec2{show_in.x + field_label_padding, label_font.centered_baseline(show_in)},
			  show_in_label, faded(open ? focused_label : colors.text_faint, t_alpha));

	const Rect chevron{show_in.right() - chevron_margin - chevron_size.x, show_in.center().y - chevron_size.y * 0.5f,
					   chevron_size.x, chevron_size.y};
	const float value_x = show_in.x + label_column_width(m_fonts) + value_text_inset;

	char summary[32];
	draw_text_truncated(t_draw_list, body, Vec2{value_x, body.centered_baseline(show_in)}, visibility_summary(summary),
						chevron.x - value_text_inset - value_x, text);
	controls::draw_chevron(t_draw_list, chevron, open, faded(open || hovered ? colors.text : colors.text_dim, t_alpha));

	t_draw_list.pop_clip();

	m_form_scroll.draw_edge_fade(t_draw_list, form.region, scroll, faded(colors.surface, t_alpha));
	m_form_scroll.draw(t_draw_list, scroll, m_mouse, t_alpha);
}

void AccountModal::draw_edit_footer(DrawList &t_draw_list, Rect t_footer, u8 t_alpha) const
{
	const Font &body = m_fonts.body();
	const Rect save = primary_button_rect(t_footer);
	const Rect cancel = cancel_button_rect(save);

	controls::draw_button(t_draw_list, body, save, "Save", controls::ButtonStyle::accent, m_settings.accent,
						  has_changes(), save.contains(m_mouse), t_alpha);
	controls::draw_button(t_draw_list, body, cancel, "Cancel", controls::ButtonStyle::ghost, m_settings.accent, true,
						  cancel.contains(m_mouse), t_alpha);
}

void AccountModal::draw_footer(DrawList &t_draw_list, Rect t_footer, u8 t_alpha) const
{
	const Font &body = m_fonts.body();
	const Font &secondary = m_fonts.secondary();
	const float hint_baseline = secondary.centered_baseline(t_footer);
	const Rect primary = primary_button_rect(t_footer);
	const float hint_width = primary.x - row_padding * 2.0f - t_footer.x;

	t_draw_list.add_rect(Rect{t_footer.x, t_footer.y, t_footer.w, 1.0f}, faded(theme().separator, t_alpha));

	switch (m_mode) {
		case Mode::edit_account:
			draw_edit_footer(t_draw_list, t_footer, t_alpha);
			break;

		case Mode::login_progress: {
			const bool finished = !m_queued_login && LoginAttempt::is_terminal(m_login.stage());

			draw_text_truncated(t_draw_list, secondary, Vec2{t_footer.x + row_padding, hint_baseline}, "", hint_width,
								faded(theme().text_faint, t_alpha));
			controls::draw_button(t_draw_list, body, primary, finished ? "Back" : "Cancel",
								  controls::ButtonStyle::neutral, m_settings.accent, true, primary.contains(m_mouse),
								  t_alpha);
			break;
		}

		case Mode::account_list: {
			const bool can_login = selected_row(displayed_accounts()) >= 0;

			draw_text_truncated(t_draw_list, secondary, Vec2{t_footer.x + row_padding, hint_baseline},
								"Select an account to log in", hint_width, faded(theme().text_faint, t_alpha));
			controls::draw_button(t_draw_list, body, primary, "Login", controls::ButtonStyle::accent, m_settings.accent,
								  can_login, primary.contains(m_mouse), t_alpha);
			break;
		}
	}
}

void AccountModal::draw(DrawList &t_draw_list)
{
	PULSAR_PROFILE_SCOPE("AccountModal.Draw");

	const bool art_visible = m_art_source && m_morph_progress > 0.0f;
	if ((m_open_amount <= 0.001f && !art_visible) || !has_game()) return;

	const auto alpha = static_cast<u8>(255.0f * m_open_amount);
	const Vec2 window = m_window.size();
	if (!is_docked()) {
		t_draw_list.add_rect(Rect{0.0f, 0.0f, window.x, window.y},
							 faded(theme().scrim, static_cast<u8>(255.0f * m_open_amount)));
	}

	const Layout current = layout();
	const float scale =
		is_docked() || m_art_source ? 1.0f : panel_closed_scale + (1.0f - panel_closed_scale) * m_open_amount;
	const bool morphing = m_art_source && m_morph_progress != 1.0f;

	t_draw_list.push_scale(current.panel.center(), scale);
	draw_chrome(t_draw_list, current, !morphing, alpha);
	t_draw_list.pop_scale();

	if (morphing) {
		draw_morphing_art(t_draw_list, current, scale);
	}

	t_draw_list.push_scale(current.panel.center(), scale);
	draw_back_badge(t_draw_list, current, alpha);

	switch (m_mode) {
		case Mode::account_list:
			draw_account_list(t_draw_list, current, alpha);
			break;

		case Mode::login_progress:
			draw_login_progress(t_draw_list, current.main_column, alpha);
			break;

		case Mode::edit_account:
			draw_edit_form(t_draw_list, current.main_column, alpha);
			break;
	}

	draw_footer(t_draw_list, current.footer, alpha);

	if (m_mode == Mode::edit_account) {
		m_visible_games.draw(t_draw_list, m_mouse);
		m_region_list.draw(t_draw_list, m_mouse);
	}

	m_tooltip.draw(t_draw_list, m_fonts, current.panel, alpha);

	t_draw_list.pop_scale();
}
