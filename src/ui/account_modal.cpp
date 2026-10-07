#include "ui/account_modal.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <numbers>
#include <span>
#include <string>
#include <utility>

#include <sodium.h>

#include "core/animation.h"
#include "core/profiler.h"
#include "core/settings.h"
#include "core/str.h"
#include "gfx/assets.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "os/window.h"
#include "core/regions.h"
#include "ui/controls.h"
#include "ui/login_session.h"
#include "ui/text.h"
#include "ui/theme.h"
#include "ui/toasts.h"
#include "ui/window_layout.h"

namespace {
constexpr float K_OPEN_EASE_RATE = 14.0f;

constexpr float K_PANEL_WIDTH_FRACTION  = 0.78f;
constexpr float K_PANEL_HEIGHT_FRACTION = 0.71f;
constexpr Vec2  K_PANEL_MAX_SIZE{810.0f, 480.0f};
constexpr Vec2  K_PANEL_MIN_SIZE{560.0f, 380.0f};
constexpr float K_REFERENCE_BODY_PIXEL_HEIGHT = 24.0f;
constexpr float K_PANEL_MARGIN                = 48.0f;
constexpr float K_PANEL_CLOSED_SCALE          = 0.92f;
constexpr float K_PANEL_BORDER                = 1.5f;
constexpr float K_PANEL_RADIUS                = 16.0f;

constexpr float K_ART_COLUMN_FRACTION      = 0.35f;
constexpr float K_ART_SEPARATOR_WIDTH      = 2.0f;
constexpr float K_CLOSE_BADGE_SIZE         = 40.0f;
constexpr float K_CLOSE_BADGE_MARGIN       = 12.0f;
constexpr float K_CLOSE_BADGE_ICON_SIZE    = 18.0f;
constexpr float K_ICON_CROSSFADE_SHARE     = 0.35f;
constexpr float K_MORPH_SECONDS            = 0.22f;
constexpr float K_PANEL_REVEAL_PROGRESS    = 0.6f;
constexpr float K_MORPH_RETURN_OPEN_AMOUNT = 0.35f;
constexpr float K_INTERACTIVE_OPEN_AMOUNT  = 0.5f;

constexpr std::string_view K_ACCOUNTS_TITLE    = "Accounts";
constexpr float            K_SEARCH_MAX_WIDTH  = 170.0f;
constexpr float            K_SEARCH_MIN_WIDTH  = 90.0f;
constexpr float            K_SEARCH_TITLE_GAP  = 16.0f;
constexpr float            K_SEARCH_BUTTON_GAP = 10.0f;
constexpr float            K_SEARCH_INSET      = 10.0f;
constexpr u32              K_SEARCH_MAX_LENGTH = 64;

constexpr float K_ROW_PADDING        = 24.0f;
constexpr float K_ROW_TOP_PADDING    = 14.0f;
constexpr float K_ROW_LINE_GAP       = 4.0f;
constexpr float K_ROW_BOTTOM_PADDING = 10.0f;
constexpr float K_ROW_BUTTON_GAP     = 10.0f;
constexpr float K_ROW_ICON_INSET     = 5.0f;
constexpr float K_ROW_RADIUS         = 10.0f;
constexpr float K_SCROLLBAR_MARGIN   = 4.0f;

constexpr float K_DRAG_THRESHOLD      = 4.0f;
constexpr float K_ROW_SHIFT_EASE_RATE = 18.0f;
constexpr float K_LIFT_EASE_RATE      = 16.0f;
constexpr float K_AUTO_SCROLL_ZONE    = 0.6f;
constexpr float K_AUTO_SCROLL_SPEED   = 540.0f;

constexpr float K_UNDO_SECONDS           = 6.0f;
constexpr float K_DELETE_CONFIRM_SECONDS = 3.0f;

constexpr float            K_ACTION_BUTTON_WIDTH     = 108.0f;
constexpr float            K_ACTION_BUTTON_GAP       = 16.0f;
constexpr std::string_view K_PERMISSION_BUTTON_LABEL = "Open settings";

constexpr float K_EMPTY_ICON_SIZE    = 52.0f;
constexpr float K_EMPTY_ICON_RADIUS  = 12.0f;
constexpr float K_EMPTY_GAP          = 16.0f;
constexpr float K_EMPTY_LINE_GAP     = 4.0f;
constexpr float K_EMPTY_BUTTON_WIDTH = 140.0f;

constexpr float            K_FORM_TOP_PADDING          = 14.0f;
constexpr float            K_FORM_COLUMN_GAP           = 12.0f;
constexpr float            K_FORM_TWO_COLUMN_MIN_WIDTH = 400.0f;
constexpr float            K_FORM_LABEL_GAP            = 6.0f;
constexpr float            K_FORM_HINT_GAP             = 4.0f;
constexpr float            K_INPUT_RADIUS              = 8.0f;
constexpr float            K_INPUT_PADDING_X           = 10.0f;
constexpr float            K_INPUT_FOCUS_BORDER        = 1.5f;
constexpr float            K_FOCUS_GLOW_BLUR           = 8.0f;
constexpr u8               K_FOCUS_GLOW_ALPHA          = 60;
constexpr float            K_CHEVRON_MARGIN            = 12.0f;
constexpr Vec2             K_CHEVRON_SIZE{9.0f, 5.0f};
constexpr float            K_EDIT_HEADER_ICON_SIZE         = 28.0f;
constexpr float            K_EDIT_HEADER_ICON_RADIUS       = 7.0f;
constexpr float            K_EDIT_HEADER_GAP               = 10.0f;
constexpr float            K_EDIT_HEADER_LINE_GAP          = 2.0f;
constexpr float            K_SUMMARY_ICON_SIZE             = 18.0f;
constexpr float            K_SUMMARY_ICON_GAP              = 6.0f;
constexpr float            K_SUMMARY_ICON_RADIUS           = 5.0f;
constexpr u8               K_SUMMARY_UNSELECTED_ICON_ALPHA = 60;
constexpr float            K_SUMMARY_TEXT_GAP              = 8.0f;
constexpr float            K_TILE_MIN_WIDTH                = 64.0f;
constexpr float            K_TILE_GAP                      = 8.0f;
constexpr float            K_TILE_ICON_SIZE                = 24.0f;
constexpr float            K_TILE_ICON_RADIUS              = 6.0f;
constexpr float            K_TILE_PADDING_Y                = 8.0f;
constexpr float            K_TILE_LABEL_GAP                = 5.0f;
constexpr float            K_TILE_RADIUS                   = 8.0f;
constexpr float            K_TILE_SELECTED_TINT            = 0.14f;
constexpr u8               K_TILE_UNSELECTED_ICON_ALPHA    = 140;
constexpr float            K_TILES_TOP_GAP                 = 8.0f;
constexpr float            K_SHOW_IN_EASE_RATE             = 18.0f;
constexpr float            K_HINT_KEY_GAP                  = 5.0f;
constexpr float            K_HINT_GAP                      = 14.0f;
constexpr std::string_view K_REGION_FIELD_LABEL            = "Region";
constexpr std::string_view K_SHOW_IN_FIELD_LABEL           = "Show in";
constexpr std::string_view K_OPTIONAL_SUFFIX               = " (optional)";
constexpr float            K_CANCEL_PADDING                = 32.0f;
constexpr float            K_REVEAL_BUTTON_SIZE            = 24.0f;
constexpr float            K_REVEAL_BUTTON_MARGIN          = 6.0f;

constexpr float K_PROGRESS_MAX_WIDTH      = 320.0f;
constexpr float K_PROGRESS_BAR_HEIGHT     = 6.0f;
constexpr float K_PROGRESS_TEXT_GAP       = 18.0f;
constexpr float K_CAP_HEIGHT_SHARE        = 0.66f;
constexpr float K_PROGRESS_TEXT_RISE      = 8.0f;
constexpr float K_PROGRESS_GLOW_BLUR      = 8.0f;
constexpr u8    K_PROGRESS_GLOW_ALPHA     = 70;
constexpr float K_SHEEN_WIDTH             = 60.0f;
constexpr float K_SHEEN_PASSES_PER_SECOND = 0.7f;
constexpr u8    K_SHEEN_ALPHA             = 90;
constexpr u32   K_MAX_MESSAGE_LINES       = 3;

constexpr Color K_COLOR_ON_ART{255, 255, 255, 255};
constexpr Color K_COLOR_ART_BADGE{20, 20, 22, 255};
constexpr Color K_COLOR_TOP_HIGHLIGHT{255, 255, 255, 22};
constexpr float K_DANGER_TINT       = 0.22f;
constexpr float K_REGION_CHIP_GAP   = 8.0f;
constexpr float K_ARMED_DANGER_TINT = 0.4f;

struct FieldSpec {
	const char* label;
	u32         max_length;
};

constexpr FieldSpec K_FIELD_SPECS[]{
	{"Note", sizeof(Account::note) - 1},
	{"Username", sizeof(Account::username) - 1},
	{"Password", sizeof(Account::password) - 1},
};

constexpr auto K_REGION_LABELS = [] {
	std::array<std::string_view, K_REGION_COUNT> labels{};
	for (usize i = 0; i < K_REGION_COUNT; i += 1) {
		labels[i] = K_REGION_OPTIONS[i].label;
	}

	return labels;
}();

[[nodiscard]] auto scaled_about(Rect t_rect, Vec2 t_origin, float t_scale) -> Rect
{
	return Rect{t_origin.x + (t_rect.x - t_origin.x) * t_scale, t_origin.y + (t_rect.y - t_origin.y) * t_scale, t_rect.w * t_scale, t_rect.h * t_scale};
}

[[nodiscard]] auto panel_size_scale(const Fonts* t_fonts) -> float
{
	return std::max(1.0f, t_fonts->body.pixel_height / K_REFERENCE_BODY_PIXEL_HEIGHT);
}

[[nodiscard]] auto row_height(const Fonts* t_fonts) -> float
{
	return K_ROW_TOP_PADDING + t_fonts->body.line_height() + K_ROW_LINE_GAP + t_fonts->secondary.line_height() + K_ROW_BOTTOM_PADDING;
}

[[nodiscard]] auto art_column_width(float t_content_width) -> float
{
	return std::max(t_content_width * K_ART_COLUMN_FRACTION, K_CLOSE_BADGE_SIZE + K_CLOSE_BADGE_MARGIN * 2.0f);
}

[[nodiscard]] auto header_height(const Fonts* t_fonts) -> float
{
	return t_fonts->body.line_height() + 20.0f;
}

[[nodiscard]] auto footer_height(const Fonts* t_fonts) -> float
{
	return std::max(56.0f, t_fonts->body.line_height() + 20.0f);
}

[[nodiscard]] auto action_button_height(const Fonts* t_fonts) -> float
{
	return std::max(32.0f, t_fonts->body.line_height() + 10.0f);
}

[[nodiscard]] auto field_input_height(const Fonts* t_fonts) -> float
{
	return std::max(34.0f, t_fonts->body.line_height() + 14.0f);
}

auto draw_input_box(DrawList* t_draw_list, Rect t_rect, Color t_border, bool t_focused, Color t_accent, u8 t_alpha) -> void
{
	if (t_focused) {
		t_draw_list->add_shadow(t_rect, K_INPUT_RADIUS, K_FOCUS_GLOW_BLUR, with_alpha(t_accent, static_cast<u8>(K_FOCUS_GLOW_ALPHA * t_alpha / 255)));
	}

	t_draw_list->add_bordered_rect(t_rect, rounded(K_INPUT_RADIUS), faded(g_theme.field, t_alpha), faded(t_border, t_alpha),
	                               t_focused ? K_INPUT_FOCUS_BORDER : 1.0f);
}

[[nodiscard]] auto row_button_size(const Fonts* t_fonts) -> float
{
	return std::max(28.0f, t_fonts->secondary.line_height() + 8.0f);
}

[[nodiscard]] auto search_height(const Fonts* t_fonts) -> float
{
	return std::max(24.0f, t_fonts->secondary.line_height() + 4.0f);
}

[[nodiscard]] auto vertically_centered(Rect t_strip, float t_x, float t_width, float t_height) -> Rect
{
	return Rect{t_x, t_strip.y + (t_strip.h - t_height) * 0.5f, t_width, t_height};
}

[[nodiscard]] auto row_highlight(Rect t_row) -> Rect
{
	return Rect{t_row.x - 8.0f, t_row.y + 3.0f, t_row.w + 16.0f, t_row.h - 6.0f};
}

[[nodiscard]] auto matches_query(const Account* t_account, std::string_view t_query) -> bool
{
	return find_ignoring_case(t_account->username, t_query) != std::string_view::npos ||
	       find_ignoring_case(t_account->note, t_query) != std::string_view::npos || find_ignoring_case(t_account->region, t_query) != std::string_view::npos;
}
}

AccountModal::AccountModal(Library*          t_library,
                           Settings*         t_settings,
                           const Fonts*      t_fonts,
                           const Assets*     t_assets,
                           const os::Window* t_window,
                           Toasts*           t_toasts,
                           LoginSession*     t_session,
                           CommandQueue*     t_commands)
	: m_library(t_library)
	, m_settings(t_settings)
	, m_fonts(t_fonts)
	, m_assets(t_assets)
	, m_window(t_window)
	, m_toasts(t_toasts)
	, m_session(t_session)
	, m_commands(t_commands)
	, m_region_list(t_fonts, t_assets, t_settings, ListPopupOptions{.empty_message = "No regions"})
{
	for (u32 i = 0; i < K_FIELD_COUNT; i += 1) {
		m_fields[i].set_max_length(K_FIELD_SPECS[i].max_length);
	}

	field(EditField::NOTE)->set_placeholder("Main, smurf, ranked...");

	m_search.set_max_length(K_SEARCH_MAX_LENGTH);
	m_search.set_placeholder("Search");
}

auto AccountModal::has_game() const -> bool
{
	return m_game >= 0 && static_cast<u32>(m_game) < m_library->game_count;
}

auto AccountModal::floating_panel_size() const -> Vec2
{
	const float size_scale = panel_size_scale(m_fonts);
	const Vec2  max_size{K_PANEL_MAX_SIZE.x * size_scale, K_PANEL_MAX_SIZE.y * size_scale};
	const Vec2  window = m_window->size();

	float width  = std::min({window.x * K_PANEL_WIDTH_FRACTION, max_size.x, std::max(0.0f, window.x - K_PANEL_MARGIN * 2.0f)});
	float height = std::min({window.y * K_PANEL_HEIGHT_FRACTION, max_size.y, std::max(0.0f, window.y - K_PANEL_MARGIN * 2.0f)});

	const float aspect = max_size.x / max_size.y;
	if (width / height > aspect) {
		width = height * aspect;
	} else {
		height = width / aspect;
	}

	const float room = std::max(0.0f, window.y - K_PANEL_MARGIN * 2.0f);

	return Vec2{width, std::max(height, std::min(expanded_form_panel_height(width), room))};
}

auto AccountModal::expanded_form_panel_height(float t_panel_width) const -> float
{
	const float      content_width = t_panel_width - K_PANEL_BORDER * 2.0f;
	const float      main_width    = std::max(0.0f, content_width - art_column_width(content_width) - K_ART_SEPARATOR_WIDTH);
	const FormLayout form          = form_layout(Rect{0.0f, 0.0f, main_width, 0.0f});
	const float      form_height   = form.tiles.bottom() + m_form_scroll.offset() - form.region.y + K_FORM_TOP_PADDING;

	return std::ceil(K_PANEL_BORDER * 2.0f + footer_height(m_fonts) + edit_header_height() + form_height) + 1.0f;
}

auto AccountModal::is_docked() const -> bool
{
	const float size_scale = panel_size_scale(m_fonts);
	const Vec2  floating   = floating_panel_size();

	return floating.x < K_PANEL_MIN_SIZE.x * size_scale || floating.y < K_PANEL_MIN_SIZE.y * size_scale;
}

auto AccountModal::panel_rect() const -> Rect
{
	if (is_docked()) return content_rect(m_window->size()).inset(0.0f, 1.0f);

	const Vec2 size   = floating_panel_size();
	const Vec2 window = m_window->size();

	return Rect{0.0f, 0.0f, window.x, window.y}.centered(size.x, size.y);
}

auto AccountModal::layout() const -> AccountModal::Layout
{
	Layout result{};
	result.panel = panel_rect();
	result.inner = result.panel.inset(K_PANEL_BORDER);

	Rect content      = result.inner;
	result.footer     = content.split_bottom(footer_height(m_fonts));
	result.art_column = Rect{content.x, content.y, art_column_width(content.w), content.h};

	const float main_x = result.art_column.right() + K_ART_SEPARATOR_WIDTH;
	result.main_column = Rect{main_x, content.y, content.right() - main_x, content.h};

	return result;
}

auto AccountModal::displayed_accounts() const -> VisibleAccounts
{
	if (!has_game()) return VisibleAccounts{};

	const VisibleAccounts  visible = m_library->visible_accounts(static_cast<u32>(m_game));
	const std::string_view query   = m_search.value();
	if (query.empty()) return visible;

	VisibleAccounts matching;
	for (const AccountRef ref : visible.view()) {
		if (!matches_query(m_library->account(ref), query)) continue;

		matching.refs[matching.count] = ref;
		matching.count += 1;
	}

	return matching;
}

auto AccountModal::account_rows(const Layout& t_layout) const -> AccountModal::AccountRows
{
	const Rect  main   = t_layout.main_column;
	const float header = header_height(m_fonts);

	AccountRows rows{};
	rows.region     = Rect{main.x, main.y + header, main.w, main.h - header};
	rows.row_height = row_height(m_fonts);
	rows.accounts   = displayed_accounts();

	const Rect track{rows.region.right() - K_SCROLLBAR_WIDTH - K_SCROLLBAR_MARGIN, rows.region.y, K_SCROLLBAR_WIDTH, rows.region.h};
	rows.scroll = ScrollGeometry{track, rows.accounts.count * rows.row_height, rows.region.h};

	return rows;
}

auto AccountModal::selected_row(const VisibleAccounts& t_accounts) const -> i32
{
	if (!m_selected) return -1;

	for (u32 i = 0; i < t_accounts.count; i += 1) {
		if (t_accounts.refs[i] == *m_selected) return static_cast<i32>(i);
	}

	return -1;
}

auto AccountModal::content_to_screen(const AccountRows& t_rows, float t_content_y) const -> float
{
	return t_rows.region.y + t_content_y - m_rows_scroll.offset();
}

auto AccountModal::row_rect_at(const Layout& t_layout, const AccountRows& t_rows, float t_content_top) const -> Rect
{
	const Rect main = t_layout.main_column;

	return Rect{main.x + K_ROW_PADDING, content_to_screen(t_rows, t_content_top), main.w - K_ROW_PADDING * 2.0f, t_rows.row_height};
}

auto AccountModal::row_rect(const Layout& t_layout, const AccountRows& t_rows, u32 t_row) const -> Rect
{
	return row_rect_at(t_layout, t_rows, t_row * t_rows.row_height + m_row_offsets[t_row]);
}

auto AccountModal::row_at(const Layout& t_layout, const AccountRows& t_rows, Vec2 t_point) const -> i32
{
	if (!t_rows.region.contains(t_point)) return -1;

	for (u32 i = 0; i < t_rows.accounts.count; i += 1) {
		const Rect row = row_rect(t_layout, t_rows, i);
		if (row.overlaps_vertically(t_rows.region) && row.contains(t_point)) return static_cast<i32>(i);
	}

	return -1;
}

auto AccountModal::remove_button_rect(Rect t_row) const -> Rect
{
	const float size = row_button_size(m_fonts);

	return vertically_centered(t_row, t_row.right() - size, size, size);
}

auto AccountModal::edit_button_rect(Rect t_row) const -> Rect
{
	const Rect remove = remove_button_rect(t_row);

	return Rect{remove.x - K_ROW_BUTTON_GAP - remove.w, remove.y, remove.w, remove.h};
}

auto AccountModal::favorite_button_rect(Rect t_row) const -> Rect
{
	const Rect edit = edit_button_rect(t_row);

	return Rect{edit.x - K_ROW_BUTTON_GAP - edit.w, edit.y, edit.w, edit.h};
}

auto AccountModal::is_row_button_hit(Rect t_row, Vec2 t_point) const -> bool
{
	return remove_button_rect(t_row).contains(t_point) || edit_button_rect(t_row).contains(t_point) || favorite_button_rect(t_row).contains(t_point);
}

auto AccountModal::add_button_rect(Rect t_main) const -> Rect
{
	const float size = row_button_size(m_fonts);
	const Rect  header{t_main.x, t_main.y, t_main.w, header_height(m_fonts)};

	return vertically_centered(header, t_main.right() - K_ROW_PADDING - size, size, size);
}

auto AccountModal::search_rect(Rect t_main) const -> Rect
{
	const Rect  header{t_main.x, t_main.y, t_main.w, header_height(m_fonts)};
	const float left  = t_main.x + K_ROW_PADDING + text_width(m_fonts->body, K_ACCOUNTS_TITLE) + K_SEARCH_TITLE_GAP;
	const float right = add_button_rect(t_main).x - K_SEARCH_BUTTON_GAP;
	const float width = std::min(K_SEARCH_MAX_WIDTH, right - left);
	if (width < K_SEARCH_MIN_WIDTH) return Rect{};

	return vertically_centered(header, snapped_to_pixel(left + (right - left - width) * 0.5f), width, search_height(m_fonts));
}

auto AccountModal::primary_button_rect(Rect t_footer) const -> Rect
{
	const std::string_view label = m_mode == Mode::EDIT_ACCOUNT ? save_label() : asks_for_permission() ? K_PERMISSION_BUTTON_LABEL : "";
	const float            width = std::max(K_ACTION_BUTTON_WIDTH, text_width(m_fonts->body, label) + K_CANCEL_PADDING);

	return vertically_centered(t_footer, t_footer.right() - K_ROW_PADDING - width, width, action_button_height(m_fonts));
}

auto AccountModal::asks_for_permission() const -> bool
{
	return m_mode == Mode::LOGIN_PROGRESS && m_session->asks_for_permission();
}

auto AccountModal::cancel_button_rect(Rect t_primary) const -> Rect
{
	const float width = text_width(m_fonts->body, "Cancel") + K_CANCEL_PADDING;

	return Rect{t_primary.x - K_ACTION_BUTTON_GAP - width, t_primary.y, width, t_primary.h};
}

auto AccountModal::edit_header_height() const -> float
{
	const float lines = m_fonts->body.line_height() + K_EDIT_HEADER_LINE_GAP + m_fonts->secondary.line_height() + 20.0f;

	return std::max(header_height(m_fonts), std::max(lines, K_EDIT_HEADER_ICON_SIZE + 16.0f));
}

auto AccountModal::save_label() const -> std::string_view
{
	return m_edited ? "Save changes" : "Add account";
}

auto AccountModal::form_region(Rect t_main) const -> Rect
{
	const float header = edit_header_height();

	return Rect{t_main.x, t_main.y + header, t_main.w, std::max(0.0f, t_main.h - header)};
}

auto AccountModal::show_in_columns(float t_width) const -> u32
{
	const u32 count   = std::max(1u, m_library->game_count);
	const u32 fitting = std::max(1u, static_cast<u32>((t_width + K_TILE_GAP) / (K_TILE_MIN_WIDTH + K_TILE_GAP)));
	if (count <= fitting) return count;

	const u32 rows = (count + fitting - 1) / fitting;

	return (count + rows - 1) / rows;
}

auto AccountModal::show_in_tile_height() const -> float
{
	return K_TILE_PADDING_Y * 2.0f + K_TILE_ICON_SIZE + K_TILE_LABEL_GAP + m_fonts->secondary.line_height();
}

auto AccountModal::form_layout(Rect t_main) const -> AccountModal::FormLayout
{
	const Font& secondary    = m_fonts->secondary;
	const float label_height = secondary.line_height();
	const float input_height = field_input_height(m_fonts);
	const float cell_height  = label_height + K_FORM_LABEL_GAP + input_height + label_height + K_FORM_HINT_GAP;
	const float left         = t_main.x + K_ROW_PADDING;
	const float width        = std::max(0.0f, t_main.w - K_ROW_PADDING * 2.0f);
	const bool  two_columns  = width >= K_FORM_TWO_COLUMN_MIN_WIDTH;
	const float column_width = two_columns ? (width - K_FORM_COLUMN_GAP) * 0.5f : width;

	FormLayout form{};
	form.region = form_region(t_main);

	float y = form.region.y + K_FORM_TOP_PADDING - m_form_scroll.offset();

	const auto place = [&](u32 t_row, u32 t_slot) {
		const u32   column = two_columns ? t_slot % 2 : 0;
		const u32   line   = two_columns ? t_slot / 2 : t_slot;
		const float x      = left + static_cast<float>(column) * (column_width + K_FORM_COLUMN_GAP);
		const float top    = y + static_cast<float>(line) * cell_height;

		form.labels[t_row] = Rect{x, top, column_width, label_height};
		form.inputs[t_row] = Rect{x, top + label_height + K_FORM_LABEL_GAP, column_width, input_height};
	};

	place(static_cast<u32>(EditField::USERNAME), 0);
	place(static_cast<u32>(EditField::PASSWORD), 1);
	place(K_REGION_ROW, 2);
	place(static_cast<u32>(EditField::NOTE), 3);
	y += static_cast<float>(two_columns ? 2 : 4) * cell_height;

	form.labels[K_SHOW_IN_ROW] = Rect{left, y, width, label_height};
	form.inputs[K_SHOW_IN_ROW] = Rect{left, y + label_height + K_FORM_LABEL_GAP, width, input_height};
	form.tile_columns          = show_in_columns(width);

	const u32   tile_rows    = (m_library->game_count + form.tile_columns - 1) / form.tile_columns;
	const float tiles_height = static_cast<float>(tile_rows) * show_in_tile_height() + static_cast<float>(tile_rows > 0 ? tile_rows - 1 : 0) * K_TILE_GAP;
	form.tiles               = Rect{left, form.inputs[K_SHOW_IN_ROW].bottom() + K_TILES_TOP_GAP, width, tiles_height};

	const float tiles_shown = (K_TILES_TOP_GAP + tiles_height) * m_show_in_amount;
	form.content_height     = form.inputs[K_SHOW_IN_ROW].bottom() + tiles_shown + m_form_scroll.offset() - form.region.y + K_FORM_TOP_PADDING;

	return form;
}

auto AccountModal::form_scroll(Rect t_main) const -> ScrollGeometry
{
	const FormLayout form = form_layout(t_main);
	const Rect       track{form.region.right() - K_SCROLLBAR_WIDTH - K_SCROLLBAR_MARGIN, form.region.y, K_SCROLLBAR_WIDTH, form.region.h};

	return ScrollGeometry{track, form.content_height, form.region.h};
}

auto AccountModal::field_input_rect(Rect t_main, u32 t_field) const -> Rect
{
	return form_layout(t_main).inputs[t_field];
}

auto AccountModal::field_text_rect(Rect t_main, u32 t_field) const -> Rect
{
	Rect input = field_input_rect(t_main, t_field).inset(K_INPUT_PADDING_X, 0.0f);

	if (t_field == static_cast<u32>(EditField::PASSWORD)) {
		input.w -= K_REVEAL_BUTTON_SIZE;
	}

	return input;
}

auto AccountModal::reveal_button_rect(Rect t_main) const -> Rect
{
	const Rect password = field_input_rect(t_main, static_cast<u32>(EditField::PASSWORD));

	return vertically_centered(password, password.right() - K_REVEAL_BUTTON_SIZE - K_REVEAL_BUTTON_MARGIN, K_REVEAL_BUTTON_SIZE, K_REVEAL_BUTTON_SIZE);
}

auto AccountModal::show_in_rect(Rect t_main) const -> Rect
{
	return form_layout(t_main).inputs[K_SHOW_IN_ROW];
}

auto AccountModal::region_rect(Rect t_main) const -> Rect
{
	return form_layout(t_main).inputs[K_REGION_ROW];
}

auto AccountModal::show_in_tile(const FormLayout& t_form, u32 t_game) const -> Rect
{
	const u32   columns = std::max(1u, t_form.tile_columns);
	const float width   = (t_form.tiles.w - K_TILE_GAP * static_cast<float>(columns - 1)) / static_cast<float>(columns);
	const float height  = show_in_tile_height();
	const auto  column  = static_cast<float>(t_game % columns);
	const auto  line    = static_cast<float>(t_game / columns);

	return Rect{snapped_to_pixel(t_form.tiles.x + column * (width + K_TILE_GAP)), snapped_to_pixel(t_form.tiles.y + line * (height + K_TILE_GAP)),
	            std::floor(width), height};
}

auto AccountModal::show_in_tile_at(Rect t_main, Vec2 t_point) const -> std::optional<u32>
{
	if (m_show_in_amount < 0.5f) return std::nullopt;

	const FormLayout form = form_layout(t_main);
	if (!form.region.contains(t_point)) return std::nullopt;

	for (u32 game = 0; game < m_library->game_count; game += 1) {
		if (show_in_tile(form, game).contains(t_point)) return game;
	}

	return std::nullopt;
}

auto AccountModal::toggle_visible_game(u32 t_game) -> void
{
	const auto bit        = static_cast<u16>(1u << t_game);
	const bool last_shown = (m_visible_mask & bit) != 0 && std::popcount(m_visible_mask) == 1;

	if (!last_shown) {
		m_visible_mask ^= bit;
	}
}

auto AccountModal::is_region_hit(Rect t_main, Vec2 t_point) const -> bool
{
	return form_region(t_main).contains(t_point) && region_rect(t_main).contains(t_point);
}

auto AccountModal::open_region_list() -> void
{
	focus_field(-1);

	u32 selected = 0;
	for (u32 i = 0; i < K_REGION_COUNT; i += 1) {
		if (K_REGION_OPTIONS[i].code == std::string_view{m_region}) {
			selected = i;
		}
	}

	m_region_list.open(K_REGION_LABELS, selected);
}

auto AccountModal::choose_region(u32 t_index) -> void
{
	if (t_index < K_REGION_COUNT) {
		copy_to(K_REGION_OPTIONS[t_index].code, m_region);
	}
}

auto AccountModal::is_show_in_hit(Rect t_main, Vec2 t_point) const -> bool
{
	return form_region(t_main).contains(t_point) && show_in_rect(t_main).contains(t_point);
}

auto AccountModal::focused_field() const -> i32
{
	for (u32 i = 0; i < K_FIELD_COUNT; i += 1) {
		if (m_fields[i].is_focused()) return static_cast<i32>(i);
	}

	return -1;
}

auto AccountModal::focus_field(i32 t_field) -> void
{
	for (u32 i = 0; i < K_FIELD_COUNT; i += 1) {
		m_fields[i].set_focused(static_cast<i32>(i) == t_field);
	}
}

auto AccountModal::field_at(Rect t_main, Vec2 t_point) const -> i32
{
	if (!form_region(t_main).contains(t_point) || is_reveal_hit(t_main, t_point)) {
		return -1;
	}

	for (u32 i = 0; i < K_FIELD_COUNT; i += 1) {
		if (field_input_rect(t_main, i).contains(t_point)) return static_cast<i32>(i);
	}

	return -1;
}

auto AccountModal::is_reveal_hit(Rect t_main, Vec2 t_point) const -> bool
{
	return form_region(t_main).contains(t_point) && reveal_button_rect(t_main).contains(t_point);
}

auto AccountModal::reveal_field(i32 t_field) -> void
{
	if (t_field < 0) return;

	const Rect main   = layout().main_column;
	const Rect region = form_region(main);
	const Rect input  = field_input_rect(main, static_cast<u32>(t_field));

	m_form_scroll.reveal(input.y, input.bottom(), region.y, region.bottom(), form_scroll(main));
}

auto AccountModal::open(i32 t_game) -> void
{
	m_open = true;
	m_art_source.reset();
	m_armed_delete.reset();
	m_game        = t_game;
	m_mode        = Mode::ACCOUNT_LIST;
	m_rows_scroll = Scrollable{};
	m_selected.reset();
	m_search.set_focused(false);
	clear_search();
	reset_row_motion();
}

auto AccountModal::detached_game() const -> i32
{
	return m_art_source && m_morph_progress > 0.0f ? m_game : -1;
}

auto AccountModal::close() -> void
{
	m_open = false;
	m_region_list.close();
	m_armed_delete.reset();
	m_search.set_focused(false);
	cancel_row_drag();
}

auto AccountModal::quick_login(u32 t_game, AccountRef t_account) -> void
{
	open(static_cast<i32>(t_game));
	request_login(t_game, t_account);
}

auto AccountModal::edit_account(AccountRef t_account) -> void
{
	open(static_cast<i32>(t_account.game));
	m_selected = t_account;
	start_editing(t_account);
}

auto AccountModal::account_at_row(i32 t_row) const -> const Account*
{
	if (t_row < 0) return nullptr;

	const VisibleAccounts shown = displayed_accounts();
	if (static_cast<u32>(t_row) >= shown.count) return nullptr;

	return m_library->account(shown.refs[t_row]);
}

auto AccountModal::forget_secrets() -> void
{
	cancel_login();
	forget_deleted();
	m_armed_delete.reset();

	for (TextInput& input : m_fields) {
		input.set_value("");
		input.set_focused(false);
	}

	m_search.set_value("");
	m_search.set_focused(false);
	sodium_memzero(m_applied_query, sizeof(m_applied_query));

	m_show_in_open = false;
	m_edited.reset();
	m_selected.reset();
	reset_row_motion();

	m_mode        = Mode::ACCOUNT_LIST;
	m_open        = false;
	m_open_amount = 0.0f;
	m_game        = -1;
}

auto AccountModal::start_adding() -> void
{
	m_show_required = false;
	m_armed_delete.reset();
	m_mode        = Mode::EDIT_ACCOUNT;
	m_form_scroll = Scrollable{};
	m_edited.reset();
	m_region[0] = '\0';
	m_region_list.close();
	m_search.set_focused(false);

	for (TextInput& input : m_fields) {
		input.set_value("");
	}

	focus_field(static_cast<i32>(EditField::USERNAME));
	field(EditField::PASSWORD)->set_masked(true);

	m_visible_mask   = static_cast<u16>(1u << m_game);
	m_show_in_open   = false;
	m_show_in_amount = 0.0f;
}

auto AccountModal::start_editing(AccountRef t_account) -> void
{
	m_show_required = false;
	m_armed_delete.reset();
	m_mode        = Mode::EDIT_ACCOUNT;
	m_form_scroll = Scrollable{};
	m_edited      = t_account;
	m_search.set_focused(false);

	const Account* account = m_library->account(t_account);
	field(EditField::NOTE)->set_value(account->note);
	copy_to(std::string_view{account->region}, m_region);
	m_region_list.close();
	field(EditField::USERNAME)->set_value(account->username);
	field(EditField::PASSWORD)->set_value(account->password);

	focus_field(static_cast<i32>(EditField::USERNAME));
	field(EditField::PASSWORD)->set_masked(true);

	m_visible_mask   = account->visible_games(t_account.game);
	m_show_in_open   = false;
	m_show_in_amount = 0.0f;
}

auto AccountModal::can_save() const -> bool
{
	return !field(EditField::USERNAME)->value().empty() && !field(EditField::PASSWORD)->value().empty();
}

auto AccountModal::has_changes() const -> bool
{
	const std::string_view note     = field(EditField::NOTE)->value();
	const std::string_view region   = m_region;
	const std::string_view username = field(EditField::USERNAME)->value();
	const std::string_view password = field(EditField::PASSWORD)->value();

	if (!m_edited) return !note.empty() || !region.empty() || !username.empty() || !password.empty();

	const Account* account = m_library->account(*m_edited);

	return note != account->note || region != account->region || username != account->username || password != account->password ||
	       m_visible_mask != account->visible_games(m_edited->game);
}

auto AccountModal::save_edit() -> void
{
	m_mode = Mode::ACCOUNT_LIST;
	if (!has_game()) return;

	const std::string_view username = field(EditField::USERNAME)->value();
	const std::string_view note     = field(EditField::NOTE)->value();
	const std::string_view password = field(EditField::PASSWORD)->value();

	if (m_edited) {
		Account* account = m_library->account(*m_edited);
		account->assign(username, note, password);
		copy_to(std::string_view{m_region}, account->region);
		account->visible_game_mask = m_visible_mask;
		return;
	}

	Account account{.visible_game_mask = m_visible_mask};
	account.assign(username, note, password);
	copy_to(std::string_view{m_region}, account.region);

	const std::optional<AccountRef> added = m_library->add_account(static_cast<u32>(m_game), account);
	sodium_memzero(&account, sizeof(account));

	if (!added) {
		notify("This game can't hold any more accounts.");
		return;
	}

	reset_row_motion();
	m_selected = added;
	reveal_selected();
}

auto AccountModal::delete_account(AccountRef t_account) -> void
{
	forget_deleted();

	m_deleted = DeletedAccount{*m_library->account(t_account), t_account};
	m_library->remove_account(t_account);
	follow_removal(t_account);
	reset_row_motion();

	m_toasts->notify(Notification{
		.message     = "Account deleted. Click to undo.",
		.on_click    = Command{.type = CommandType::UNDO_DELETE},
		.seconds     = K_UNDO_SECONDS,
		.always_show = true,
	});
}

auto AccountModal::arm_or_delete(AccountRef t_account) -> void
{
	if (m_armed_delete == t_account) {
		m_armed_delete.reset();
		delete_account(t_account);
		return;
	}

	m_armed_delete  = t_account;
	m_armed_seconds = K_DELETE_CONFIRM_SECONDS;
}

auto AccountModal::delete_countdown(AccountRef t_account) const -> float
{
	return m_armed_delete == t_account ? m_armed_seconds / K_DELETE_CONFIRM_SECONDS : 0.0f;
}

auto AccountModal::forget_deleted() -> void
{
	if (!m_deleted) return;

	sodium_memzero(&m_deleted->account, sizeof(Account));
	m_deleted.reset();
}

auto AccountModal::undo_delete() -> void
{
	if (!m_deleted) return;

	const std::optional<AccountRef> restored = m_library->insert_account(m_deleted->position, m_deleted->account);
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

auto AccountModal::toggle_favorite(i32 t_row) -> void
{
	if (t_row < 0) return;

	const VisibleAccounts shown = displayed_accounts();
	if (static_cast<u32>(t_row) < shown.count) {
		toggle_favorite(shown.refs[t_row]);
	}
}

auto AccountModal::toggle_favorite(AccountRef t_account) -> void
{
	const VisibleAccounts before = displayed_accounts();

	Account* account  = m_library->account(t_account);
	account->favorite = !account->favorite;

	animate_reorder(before);

	const VisibleAccounts after = displayed_accounts();
	for (u32 i = 0; i < after.count; i += 1) {
		if (after.refs[i] == t_account) {
			m_raised_row = i;
		}
	}
}

auto AccountModal::follow_insert(AccountRef t_inserted) -> void
{
	shift_after_insert(&m_selected, t_inserted);
	shift_after_insert(&m_edited, t_inserted);
	shift_after_insert(&m_armed_delete, t_inserted);
	m_session->follow_insert(t_inserted);
}

auto AccountModal::follow_removal(AccountRef t_removed) -> void
{
	shift_after_removal(&m_selected, t_removed);
	shift_after_removal(&m_edited, t_removed);
	shift_after_removal(&m_armed_delete, t_removed);
	m_session->follow_removal(t_removed);
}

auto AccountModal::notify(std::string_view t_message) -> void
{
	m_toasts->notify(Notification{.message = t_message});
}

auto AccountModal::request_login(u32 t_game, AccountRef t_account) -> void
{
	m_armed_delete.reset();
	m_selected = t_account;
	m_mode     = Mode::LOGIN_PROGRESS;
	m_search.set_focused(false);
	m_session->request(t_game, t_account);
}

auto AccountModal::cancel_login() -> void
{
	m_session->cancel();
}

auto AccountModal::refresh_search() -> void
{
	const std::string_view query = m_search.value();
	if (query == std::string_view{m_applied_query}) return;

	copy_to(query, m_applied_query);
	reset_row_motion();

	const Layout      current = layout();
	const AccountRows rows    = account_rows(current);

	if (query.empty()) {
		reveal_selected();
		return;
	}

	m_rows_scroll.jump_to(0.0f, rows.scroll);
}

auto AccountModal::clear_search() -> void
{
	m_search.set_value("");
	refresh_search();
}

auto AccountModal::reveal_selected() -> void
{
	const Layout      current = layout();
	const AccountRows rows    = account_rows(current);
	const i32         row     = selected_row(rows.accounts);
	if (row < 0) return;

	const Rect rect = row_rect(current, rows, static_cast<u32>(row));
	m_rows_scroll.reveal(rect.y, rect.bottom(), rows.region.y, rows.region.bottom(), rows.scroll);
}

auto AccountModal::select_step(i32 t_step) -> void
{
	const VisibleAccounts shown = displayed_accounts();
	if (shown.count == 0) return;

	const i32 selected = selected_row(shown);
	const i32 last     = static_cast<i32>(shown.count) - 1;
	const i32 row      = selected < 0 ? 0 : std::clamp(selected + t_step, 0, last);

	m_selected = shown.refs[row];
	reveal_selected();
}

auto AccountModal::is_search_visible() const -> bool
{
	return search_rect(layout().main_column).w > 0.0f;
}

auto AccountModal::drag_range(const VisibleAccounts& t_accounts, u32 t_row) const -> AccountModal::RowRange
{
	u32 favorites = 0;
	while (favorites < t_accounts.count && m_library->account(t_accounts.refs[favorites])->favorite) {
		favorites += 1;
	}

	if (t_row < favorites) return RowRange{0, favorites - 1};

	return RowRange{favorites, t_accounts.count - 1};
}

auto AccountModal::lifted_top(const AccountRows& t_rows) const -> float
{
	const RowRange range   = drag_range(t_rows.accounts, m_drag.from_row);
	const float    pointer = m_mouse.y - t_rows.region.y + m_rows_scroll.offset();

	return std::clamp(pointer - m_drag.grab_offset, range.first * t_rows.row_height, range.last * t_rows.row_height);
}

auto AccountModal::lift_row(const AccountRows& t_rows, u32 t_row, Vec2 t_point) -> void
{
	const float top     = t_row * t_rows.row_height + m_row_offsets[t_row];
	const float pointer = t_point.y - t_rows.region.y + m_rows_scroll.offset();

	m_drag.lifted      = true;
	m_drag.from_row    = t_row;
	m_drag.target_row  = t_row;
	m_drag.grab_offset = pointer - top;
	m_raised_row       = t_row;
	m_search.set_focused(false);
}

auto AccountModal::drop_row() -> void
{
	if (!m_drag.lifted) {
		m_drag = RowDrag{};
		return;
	}

	const AccountRows rows = account_rows(layout());
	const u32         from = m_drag.from_row;
	const u32         to   = m_drag.target_row;

	if (from < rows.accounts.count) {
		m_row_offsets[from] = lifted_top(rows) - from * rows.row_height;
	}

	m_drag = RowDrag{};

	if (from >= rows.accounts.count || to >= rows.accounts.count) {
		reset_row_motion();
		return;
	}

	if (from != to) {
		m_library->move_visible_account(static_cast<u32>(m_game), from, to);
		animate_reorder(rows.accounts);
	}

	m_raised_row = to;
}

auto AccountModal::cancel_row_drag() -> void
{
	m_drag.target_row = m_drag.from_row;
	drop_row();
}

auto AccountModal::update_row_drag(float t_delta_seconds) -> void
{
	const AccountRows rows   = account_rows(layout());
	const float       height = rows.row_height;

	if (m_drag.lifted && m_drag.from_row >= rows.accounts.count) {
		reset_row_motion();
	}

	if (m_drag.lifted) {
		const float zone  = height * K_AUTO_SCROLL_ZONE;
		const float above = rows.region.y + zone - m_mouse.y;
		const float below = m_mouse.y - (rows.region.bottom() - zone);

		if (above > 0.0f) {
			m_rows_scroll.scroll_by(-K_AUTO_SCROLL_SPEED * std::min(above / zone, 1.0f) * t_delta_seconds, rows.scroll);
		} else if (below > 0.0f) {
			m_rows_scroll.scroll_by(K_AUTO_SCROLL_SPEED * std::min(below / zone, 1.0f) * t_delta_seconds, rows.scroll);
		}

		const RowRange range = drag_range(rows.accounts, m_drag.from_row);
		const auto     slot  = static_cast<u32>(std::lround(lifted_top(rows) / height));
		m_drag.target_row    = std::clamp(slot, range.first, range.last);
	}

	for (u32 i = 0; i < rows.accounts.count; i += 1) {
		if (m_drag.lifted && i == m_drag.from_row) continue;

		float target = 0.0f;
		if (m_drag.lifted && m_drag.from_row < i && i <= m_drag.target_row) {
			target = -height;
		} else if (m_drag.lifted && m_drag.target_row <= i && i < m_drag.from_row) {
			target = height;
		}

		m_row_offsets[i] = animation::ease_toward(m_row_offsets[i], target, K_ROW_SHIFT_EASE_RATE, t_delta_seconds, animation::K_SETTLED_PIXELS);
	}

	m_lift_amount = animation::ease_toward(m_lift_amount, m_drag.lifted ? 1.0f : 0.0f, K_LIFT_EASE_RATE, t_delta_seconds);

	const bool raised_settled = !m_raised_row || *m_raised_row >= rows.accounts.count || m_row_offsets[*m_raised_row] == 0.0f;
	if (!m_drag.lifted && m_lift_amount == 0.0f && raised_settled) {
		m_raised_row.reset();
	}
}

auto AccountModal::animate_reorder(const VisibleAccounts& t_before) -> void
{
	const VisibleAccounts after  = displayed_accounts();
	const float           height = row_height(m_fonts);
	float                 offsets[K_MAX_VISIBLE_ACCOUNTS]{};

	for (u32 i = 0; i < after.count; i += 1) {
		for (u32 j = 0; j < t_before.count; j += 1) {
			if (t_before.refs[j] != after.refs[i]) continue;

			offsets[i] = (static_cast<float>(j) - static_cast<float>(i)) * height + m_row_offsets[j];
			break;
		}
	}

	std::copy(std::begin(offsets), std::end(offsets), std::begin(m_row_offsets));
}

auto AccountModal::reset_row_motion() -> void
{
	m_drag = RowDrag{};
	std::fill(std::begin(m_row_offsets), std::end(m_row_offsets), 0.0f);
	m_raised_row.reset();
	m_lift_amount = 0.0f;
}

auto AccountModal::back_badge_rect(const Layout& t_layout) const -> Rect
{
	const Rect& art = t_layout.art_column;

	return Rect{art.x + K_CLOSE_BADGE_MARGIN, art.y + K_CLOSE_BADGE_MARGIN, K_CLOSE_BADGE_SIZE, K_CLOSE_BADGE_SIZE};
}

auto AccountModal::request_tooltip() -> void
{
	if (!is_blocking() || !has_game()) return;

	const Layout current = layout();
	const Rect   back    = back_badge_rect(current);

	if (m_mode != Mode::LOGIN_PROGRESS && back.contains(m_mouse)) {
		m_tooltip.request("Back", back);
		return;
	}

	if (m_mode == Mode::EDIT_ACCOUNT) {
		const Rect main = current.main_column;

		if (is_reveal_hit(main, m_mouse)) {
			m_tooltip.request(field(EditField::PASSWORD)->is_masked() ? "Show password" : "Hide password", reveal_button_rect(main));
		}

		return;
	}

	if (m_mode == Mode::ACCOUNT_LIST) {
		request_row_tooltip(current);
	}
}

auto AccountModal::request_row_tooltip(const Layout& t_layout) -> void
{
	if (m_drag.lifted || m_drag.pressed_row) return;

	const Rect add = add_button_rect(t_layout.main_column);
	if (add.contains(m_mouse)) {
		m_tooltip.request("Add account", add);
		return;
	}

	const AccountRows rows        = account_rows(t_layout);
	const i32         hovered_row = row_at(t_layout, rows, m_mouse);
	if (hovered_row < 0) return;

	const Rect row      = row_rect(t_layout, rows, static_cast<u32>(hovered_row));
	const Rect favorite = favorite_button_rect(row);
	const Rect edit     = edit_button_rect(row);
	const Rect remove   = remove_button_rect(row);

	if (favorite.contains(m_mouse)) {
		const bool pinned = m_library->account(rows.accounts.refs[hovered_row])->favorite;
		m_tooltip.request(pinned ? "Unpin" : "Pin to top", favorite);
	} else if (edit.contains(m_mouse)) {
		m_tooltip.request("Edit account", edit);
	} else if (remove.contains(m_mouse)) {
		const bool armed = m_armed_delete == rows.accounts.refs[hovered_row];
		m_tooltip.request(armed ? "Click again to delete" : "Delete account", remove);
	}
}

auto AccountModal::update(float t_delta_seconds) -> void
{
	const bool  morphing     = m_art_source && has_game();
	const float morph_target = m_open ? 1.0f : 0.0f;

	if (!morphing) {
		m_morph_progress = morph_target;
	} else if (m_open || m_open_amount <= K_MORPH_RETURN_OPEN_AMOUNT) {
		m_morph_progress = animation::step_toward(m_morph_progress, morph_target, K_MORPH_SECONDS, t_delta_seconds);
	}

	const bool  panel_shown  = m_open && (!morphing || m_morph_progress >= K_PANEL_REVEAL_PROGRESS);
	const float scale_travel = m_window->size().x * 0.5f * (1.0f - K_PANEL_CLOSED_SCALE);
	m_open_amount =
		animation::ease_toward(m_open_amount, panel_shown ? 1.0f : 0.0f, K_OPEN_EASE_RATE, t_delta_seconds, animation::K_SETTLED_PIXELS / scale_travel);

	if (!m_open && m_open_amount == 0.0f && m_morph_progress == 0.0f) {
		m_game = -1;
	}

	if (m_deleted && !m_toasts->is_offering(CommandType::UNDO_DELETE)) {
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
		using enum Mode;

		case ACCOUNT_LIST: {
			m_search.update(t_delta_seconds);

			if (has_game()) {
				refresh_search();
			}

			m_rows_scroll.update(t_delta_seconds);
			update_row_drag(t_delta_seconds);
			break;
		}

		case LOGIN_PROGRESS: {
			animation::request_frame();
			break;
		}

		case EDIT_ACCOUNT: {
			m_form_scroll.update(t_delta_seconds);

			for (TextInput& input : m_fields) {
				input.update(t_delta_seconds);
			}

			break;
		}
	}

	const float show_in_before = m_show_in_amount;
	m_show_in_amount           = animation::ease_toward(m_show_in_amount, m_show_in_open ? 1.0f : 0.0f, K_SHOW_IN_EASE_RATE, t_delta_seconds);

	if (m_mode == Mode::EDIT_ACCOUNT && m_show_in_open && m_show_in_amount != show_in_before) {
		const Rect       main   = layout().main_column;
		const FormLayout form   = form_layout(main);
		const float      bottom = form.tiles.y + form.tiles.h * m_show_in_amount;

		m_form_scroll.reveal(form.labels[K_SHOW_IN_ROW].y, bottom, form.region.y, form.region.bottom(), form_scroll(main));
	}
	m_region_list.update(t_delta_seconds, region_rect(layout().main_column), layout().inner);
	request_tooltip();
	m_tooltip.update(t_delta_seconds);
}

auto AccountModal::on_pointer_down(Vec2 t_point) -> bool
{
	if (!is_blocking()) return false;

	if (m_open_amount < K_INTERACTIVE_OPEN_AMOUNT) {
		m_press_swallowed = true;
		return true;
	}

	const Layout current = layout();

	if (m_mode == Mode::EDIT_ACCOUNT && m_region_list.is_open()) {
		m_region_list.on_pointer_down(t_point);
		return true;
	}

	if (m_mode == Mode::EDIT_ACCOUNT) {
		if (m_form_scroll.on_pointer_down(t_point, form_scroll(current.main_column))) {
			return true;
		}

		const i32 pressed = field_at(current.main_column, t_point);

		if (pressed >= 0) {
			focus_field(pressed);
			m_fields[pressed].on_pointer_down(m_fonts->body, field_text_rect(current.main_column, pressed), t_point.x);
		}
	} else if (m_mode == Mode::ACCOUNT_LIST && has_game()) {
		handle_list_press(current, t_point);
	}

	return true;
}

auto AccountModal::handle_list_press(const Layout& t_layout, Vec2 t_point) -> void
{
	const AccountRows rows = account_rows(t_layout);
	if (m_rows_scroll.on_pointer_down(t_point, rows.scroll)) return;

	const Rect search     = search_rect(t_layout.main_column);
	const bool over_clear = !m_search.value().empty() && controls::search_clear_rect(search).contains(t_point);

	if (search.contains(t_point) && !over_clear) {
		m_search.set_focused(true);
		m_search.on_pointer_down(m_fonts->secondary, controls::search_text_rect(search, K_SEARCH_INSET), t_point.x);
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

auto AccountModal::on_pointer_move(Vec2 t_point) -> bool
{
	if (!is_blocking()) return false;

	if (m_region_list.is_open()) {
		m_region_list.on_pointer_move(t_point);
		return true;
	}

	const Layout current = layout();

	for (u32 i = 0; i < K_FIELD_COUNT; i += 1) {
		if (m_fields[i].is_selecting()) {
			m_fields[i].on_pointer_move(m_fonts->body, field_text_rect(current.main_column, i), t_point.x);
		}
	}

	if (m_search.is_selecting()) {
		m_search.on_pointer_move(m_fonts->secondary, controls::search_text_rect(search_rect(current.main_column), K_SEARCH_INSET), t_point.x);
	}

	if (m_rows_scroll.is_dragging()) {
		m_rows_scroll.on_pointer_move(t_point.y, account_rows(current).scroll);
	}

	if (m_form_scroll.is_dragging()) {
		m_form_scroll.on_pointer_move(t_point.y, form_scroll(current.main_column));
	}

	if (m_drag.pressed_row && !m_drag.lifted && m_search.value().empty()) {
		const float       dx   = t_point.x - m_drag.press_point.x;
		const float       dy   = t_point.y - m_drag.press_point.y;
		const AccountRows rows = account_rows(current);

		if (dx * dx + dy * dy > K_DRAG_THRESHOLD * K_DRAG_THRESHOLD && *m_drag.pressed_row < rows.accounts.count) {
			lift_row(rows, *m_drag.pressed_row, t_point);
		}
	}

	return true;
}

auto AccountModal::on_pointer_up(Vec2 t_point) -> bool
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

	for (TextInput& input : m_fields) {
		ended_text_selection = ended_text_selection || input.is_selecting();
		input.on_pointer_up();
	}

	if (ended_text_selection) return true;

	const Layout current       = layout();
	const bool   clicked_away  = !current.panel.contains(t_point);
	const bool   clicked_close = back_badge_rect(current).contains(t_point);

	if (m_mode != Mode::LOGIN_PROGRESS && (clicked_away || clicked_close)) {
		close();
		return true;
	}

	switch (m_mode) {
		using enum Mode;

		case ACCOUNT_LIST: {
			if (has_game()) {
				handle_list_click(current, t_point);
			}

			break;
		}

		case LOGIN_PROGRESS: {
			const Rect primary = primary_button_rect(current.footer);
			const Rect back    = asks_for_permission() ? cancel_button_rect(primary) : primary;

			if (asks_for_permission() && primary.contains(t_point)) {
				m_commands->push(Command{.type = CommandType::OPEN_PERMISSION_SETTINGS});
			} else if (back.contains(t_point)) {
				cancel_login();
				m_mode = Mode::ACCOUNT_LIST;
			}

			break;
		}

		case EDIT_ACCOUNT: {
			handle_edit_click(current, t_point);
			break;
		}
	}

	return true;
}

auto AccountModal::handle_list_click(const Layout& t_layout, Vec2 t_point) -> void
{
	const std::optional<AccountRef> armed = std::exchange(m_armed_delete, std::nullopt);

	const Rect main   = t_layout.main_column;
	const Rect search = search_rect(main);

	if (!m_search.value().empty() && controls::search_clear_rect(search).contains(t_point)) {
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
			const Rect       rect    = row_rect(t_layout, rows, static_cast<u32>(row));
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

auto AccountModal::handle_edit_click(const Layout& t_layout, Vec2 t_point) -> void
{
	const Rect main = t_layout.main_column;

	if (is_region_hit(main, t_point)) {
		open_region_list();
		return;
	}

	if (is_show_in_hit(main, t_point)) {
		focus_field(-1);
		m_show_in_open = !m_show_in_open;
		return;
	}

	if (const std::optional<u32> tile = show_in_tile_at(main, t_point)) {
		toggle_visible_game(*tile);
		return;
	}

	if (is_reveal_hit(main, t_point)) {
		TextInput* password = field(EditField::PASSWORD);
		password->set_masked(!password->is_masked());
		return;
	}

	focus_field(-1);

	const Rect save = primary_button_rect(t_layout.footer);

	if (cancel_button_rect(save).contains(t_point)) {
		m_mode = Mode::ACCOUNT_LIST;
	} else if (has_changes() && save.contains(t_point)) {
		if (can_save()) {
			save_edit();
		} else {
			m_show_required = true;
		}
	}

	m_armed_delete.reset();
}

auto AccountModal::on_right_click(Vec2 t_point) -> bool
{
	if (!is_blocking()) return false;
	if (m_open_amount < K_INTERACTIVE_OPEN_AMOUNT) return true;

	const Layout current = layout();

	if (m_mode == Mode::EDIT_ACCOUNT) {
		const i32 clicked = field_at(current.main_column, t_point);

		if (clicked >= 0) {
			focus_field(clicked);
			m_fields[clicked].on_right_click(m_fonts->body, field_text_rect(current.main_column, clicked), t_point.x);
			m_commands->push(Command{.type = CommandType::SHOW_TEXT_MENU, .position = t_point, .text_input = &m_fields[clicked]});
		}

		return true;
	}

	if (m_mode != Mode::ACCOUNT_LIST || !has_game() || m_drag.lifted) return true;

	const Rect search = search_rect(current.main_column);

	if (search.contains(t_point)) {
		m_search.set_focused(true);
		m_search.on_right_click(m_fonts->secondary, controls::search_text_rect(search, K_SEARCH_INSET), t_point.x);
		m_commands->push(Command{.type = CommandType::SHOW_TEXT_MENU, .position = t_point, .text_input = &m_search});
		return true;
	}

	const AccountRows rows = account_rows(current);
	const i32         row  = row_at(current, rows, t_point);

	if (row >= 0) {
		m_selected = rows.accounts.refs[row];
		m_commands->push(Command{.type = CommandType::SHOW_ACCOUNT_MENU, .index = row, .position = t_point});
	}

	return true;
}

auto AccountModal::on_scroll(Vec2, float t_wheel_delta) -> bool
{
	if (!is_blocking()) return false;
	if (m_open_amount < K_INTERACTIVE_OPEN_AMOUNT) return true;

	if (m_region_list.is_open()) {
		m_region_list.on_scroll(t_wheel_delta);
		return true;
	}

	if (m_mode == Mode::ACCOUNT_LIST) {
		m_rows_scroll.on_scroll(t_wheel_delta, account_rows(layout()).scroll);
	} else if (m_mode == Mode::EDIT_ACCOUNT) {
		m_form_scroll.on_scroll(t_wheel_delta, form_scroll(layout().main_column));
	}

	return true;
}

auto AccountModal::handle_list_key(os::Key t_key) -> bool
{
	if (m_drag.lifted) return true;

	switch (t_key) {
		using enum os::Key;

		case UP:
		case DOWN: {
			select_step(t_key == os::Key::DOWN ? 1 : -1);
			return true;
		}

		case ENTER: {
			if (selected_row(displayed_accounts()) >= 0) {
				request_login(static_cast<u32>(m_game), *m_selected);
			}

			return true;
		}

		default: {
			break;
		}
	}

	const bool control = os::modifiers().shortcut;

	if (control && t_key == os::Key::F && is_search_visible()) {
		m_search.set_focused(true);
		m_search.apply(TextEdit::SELECT_ALL);
		return true;
	}

	if (m_search.is_focused()) {
		m_search.on_key_down(t_key);
		return true;
	}

	if (t_key == os::Key::FORWARD_DELETE) {
		if (selected_row(displayed_accounts()) >= 0) {
			arm_or_delete(*m_selected);
		}

		return true;
	}

	if (t_key == os::Key::BACKSPACE && !m_search.value().empty() && is_search_visible()) {
		m_search.set_focused(true);
		m_search.on_key_down(t_key);
		return true;
	}

	return false;
}

auto AccountModal::on_key_down(os::Key t_key) -> bool
{
	if (!is_blocking()) return false;

	if (m_region_list.is_open()) {
		if (const std::optional<u32> chosen = m_region_list.on_key_down(t_key)) {
			choose_region(*chosen);
		}

		return true;
	}

	if (t_key == os::Key::ESCAPE) {
		if (m_armed_delete) {
			m_armed_delete.reset();
		} else if (m_mode == Mode::EDIT_ACCOUNT) {
			m_mode = Mode::ACCOUNT_LIST;
		} else if (m_mode == Mode::ACCOUNT_LIST && m_drag.lifted) {
			cancel_row_drag();
		} else if (m_mode == Mode::ACCOUNT_LIST && !m_search.value().empty()) {
			clear_search();
		} else if (m_mode == Mode::ACCOUNT_LIST && m_search.is_focused()) {
			m_search.set_focused(false);
		} else if (m_mode == Mode::ACCOUNT_LIST && m_selected) {
			m_selected.reset();
		} else if (m_mode == Mode::ACCOUNT_LIST) {
			close();
		}

		return true;
	}

	if (m_mode == Mode::ACCOUNT_LIST && has_game()) {
		handle_list_key(t_key);
	} else if (m_mode == Mode::EDIT_ACCOUNT && t_key == os::Key::TAB) {
		constexpr EditField ORDER[]{EditField::USERNAME, EditField::PASSWORD, EditField::NOTE};
		constexpr auto      COUNT    = static_cast<i32>(std::size(ORDER));
		const i32           step     = os::modifiers().shift ? COUNT - 1 : 1;
		const i32           current  = focused_field();
		i32                 position = -1;

		for (i32 i = 0; i < COUNT; i += 1) {
			if (static_cast<i32>(ORDER[i]) == current) {
				position = i;
			}
		}

		focus_field(static_cast<i32>(ORDER[position < 0 ? 0 : (position + step) % COUNT]));
		reveal_field(focused_field());
	} else if (m_mode == Mode::EDIT_ACCOUNT && t_key == os::Key::ENTER) {
		if (has_changes() && can_save()) {
			save_edit();
		} else if (has_changes()) {
			m_show_required = true;
		}
	} else if (m_mode == Mode::EDIT_ACCOUNT) {
		for (TextInput& input : m_fields) {
			input.on_key_down(t_key);
		}
	}

	return true;
}

auto AccountModal::on_char(u32 t_character) -> bool
{
	if (!is_blocking()) return false;

	if (m_mode == Mode::EDIT_ACCOUNT) {
		if (m_region_list.is_open()) {
			m_region_list.on_char(t_character);
			return true;
		}

		for (TextInput& input : m_fields) {
			input.on_char(t_character);
		}
	} else if (m_mode == Mode::ACCOUNT_LIST && has_game() && !m_drag.lifted) {
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

auto AccountModal::list_cursor(const Layout& t_layout) const -> CursorKind
{
	if (m_drag.lifted) return CursorKind::DRAG;

	const Rect main   = t_layout.main_column;
	const Rect search = search_rect(main);

	if (!m_search.value().empty() && controls::search_clear_rect(search).contains(m_mouse)) return CursorKind::HAND;
	if (search.contains(m_mouse)) return CursorKind::I_BEAM;
	if (add_button_rect(main).contains(m_mouse)) return CursorKind::HAND;

	const AccountRows rows = account_rows(t_layout);
	if (rows.accounts.count == 0 && m_search.value().empty() && empty_state(rows.region).button.contains(m_mouse)) {
		return CursorKind::HAND;
	}

	if (row_at(t_layout, rows, m_mouse) >= 0 || m_rows_scroll.is_over_track(m_mouse, rows.scroll)) {
		return CursorKind::HAND;
	}

	const bool over_login = primary_button_rect(t_layout.footer).contains(m_mouse);
	const bool can_login  = selected_row(rows.accounts) >= 0 && !m_session->is_busy();

	return can_login && over_login ? CursorKind::HAND : CursorKind::ARROW;
}

auto AccountModal::edit_cursor(const Layout& t_layout) const -> CursorKind
{
	const Rect main = t_layout.main_column;

	if (m_region_list.is_open()) return m_region_list.cursor(m_mouse);
	if (is_show_in_hit(main, m_mouse) || is_region_hit(main, m_mouse) || is_reveal_hit(main, m_mouse)) {
		return CursorKind::HAND;
	}

	if (const std::optional<u32> tile = show_in_tile_at(main, m_mouse)) {
		const auto bit    = static_cast<u16>(1u << *tile);
		const bool locked = (m_visible_mask & bit) != 0 && std::popcount(m_visible_mask) == 1;

		return locked ? CursorKind::ARROW : CursorKind::HAND;
	}

	if (field_at(main, m_mouse) >= 0) return CursorKind::I_BEAM;
	if (m_form_scroll.is_over_track(m_mouse, form_scroll(main))) return CursorKind::HAND;

	const Rect save        = primary_button_rect(t_layout.footer);
	const bool over_button = cancel_button_rect(save).contains(m_mouse) || (has_changes() && save.contains(m_mouse));

	return over_button ? CursorKind::HAND : CursorKind::ARROW;
}

auto AccountModal::cursor() const -> CursorKind
{
	if (!is_blocking()) return CursorKind::ARROW;
	if (m_rows_scroll.is_dragging() || m_form_scroll.is_dragging()) return CursorKind::DRAG;
	if (m_search.is_selecting()) return CursorKind::I_BEAM;

	for (const TextInput& input : m_fields) {
		if (input.is_selecting()) return CursorKind::I_BEAM;
	}

	const Layout current = layout();

	if (m_mode != Mode::LOGIN_PROGRESS && back_badge_rect(current).contains(m_mouse)) return CursorKind::HAND;

	switch (m_mode) {
		using enum Mode;

		case ACCOUNT_LIST: {
			return has_game() ? list_cursor(current) : CursorKind::ARROW;
		}

		case LOGIN_PROGRESS: {
			const Rect primary = primary_button_rect(current.footer);
			const bool hovered = primary.contains(m_mouse) || (asks_for_permission() && cancel_button_rect(primary).contains(m_mouse));

			return hovered ? CursorKind::HAND : CursorKind::ARROW;
		}

		case EDIT_ACCOUNT: {
			return edit_cursor(current);
		}
	}

	return CursorKind::ARROW;
}

auto AccountModal::draw_chrome(DrawList* t_draw_list, const Layout& t_layout, bool t_with_art, u8 t_alpha) const -> void
{
	const Game& game   = m_library->games[static_cast<u32>(m_game)];
	const Rect  art    = t_layout.art_column;
	const bool  docked = is_docked();
	const float radius = docked ? 0.0f : K_PANEL_RADIUS;

	if (docked) {
		t_draw_list->add_rect(t_layout.panel, faded(g_theme.surface, t_alpha));
	} else {
		controls::draw_panel_shadow(t_draw_list, t_layout.panel, K_PANEL_RADIUS, m_open_amount);
		t_draw_list->add_bordered_rect(t_layout.panel, rounded(K_PANEL_RADIUS), faded(g_theme.surface, t_alpha), faded(g_theme.border, t_alpha),
		                               K_PANEL_BORDER);

		const float highlight_inset = scaled_radius(K_PANEL_RADIUS);
		t_draw_list->add_rect(Rect{t_layout.inner.x + highlight_inset, t_layout.inner.y, t_layout.inner.w - highlight_inset * 2.0f, 1.0f},
		                      faded(K_COLOR_TOP_HIGHLIGHT, t_alpha));
	}

	const u8 art_alpha = m_art_source ? 255 : t_alpha;

	if (t_with_art && game.banner != nullptr) {
		t_draw_list->add_image(art, game.banner, faded(K_COLOR_ON_ART, art_alpha), rounded(std::max(0.0f, radius - K_PANEL_BORDER), 0.0f, 0.0f, 0.0f),
		                       cover_uv(art.w / art.h, game.banner->aspect()));
	} else if (t_with_art) {
		t_draw_list->add_rect(art, faded(game.accent, art_alpha));
	}

	t_draw_list->add_rect(Rect{art.right(), art.y, K_ART_SEPARATOR_WIDTH, art.h}, faded(g_theme.border, t_alpha));
}

auto AccountModal::draw_back_badge(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) const -> void
{
	const Rect badge       = back_badge_rect(t_layout);
	const auto badge_alpha = static_cast<u8>(badge.contains(m_mouse) ? 210 : 170);

	t_draw_list->add_rounded_rect(badge, rounded(badge.w * 0.5f), faded(with_alpha(K_COLOR_ART_BADGE, badge_alpha), t_alpha));
	t_draw_list->add_image(badge.centered(K_CLOSE_BADGE_ICON_SIZE, K_CLOSE_BADGE_ICON_SIZE), m_assets->get(Asset::ICON_ARROW_BACK),
	                       faded(K_COLOR_ON_ART, t_alpha));
}

auto AccountModal::draw_morphing_art(DrawList* t_draw_list, const Layout& t_layout, float t_scale) const -> void
{
	const Game&     game   = m_library->games[static_cast<u32>(m_game)];
	const ArtSource source = *m_art_source;
	const float     amount = 0.5f - 0.5f * std::cos(m_morph_progress * std::numbers::pi_v<float>);

	const Rect        target         = scaled_about(t_layout.art_column, t_layout.panel.center(), t_scale);
	const Rect        art            = lerp(source.rect, target, amount);
	const float       target_corner  = is_docked() ? 0.0f : std::max(0.0f, K_PANEL_RADIUS - K_PANEL_BORDER) * t_scale;
	const float       leading_corner = lerp(source.radius, target_corner, amount);
	const float       other_corners  = lerp(source.radius, 0.0f, amount);
	const CornerRadii radii          = rounded(leading_corner, other_corners, other_corners, other_corners);
	const float       banner_share   = source.is_icon ? std::clamp(amount / K_ICON_CROSSFADE_SHARE, 0.0f, 1.0f) : 1.0f;
	const float       flight         = std::sin(amount * std::numbers::pi_v<float>);

	if (flight > 0.01f) {
		controls::draw_panel_shadow(t_draw_list, art, std::max(leading_corner, other_corners), flight);
	}

	const float frame       = 1.0f - amount;
	const auto  frame_alpha = to_alpha(frame);

	if (source.glow > 0.0f && frame_alpha > 0) {
		t_draw_list->add_banner_glow(art, other_corners, source.glow, faded(source.glow_color, frame_alpha));
	}

	if (source.border > 0.0f && frame_alpha > 0) {
		t_draw_list->add_rounded_rect(art, radii, faded(source.border_color, frame_alpha));
	}

	const float       inset       = source.border * frame;
	const Rect        image       = art.inset(inset);
	const CornerRadii image_radii = rounded(std::max(0.0f, leading_corner - inset), std::max(0.0f, other_corners - inset),
	                                        std::max(0.0f, other_corners - inset), std::max(0.0f, other_corners - inset));

	if (game.banner != nullptr) {
		t_draw_list->add_image(image, game.banner, faded(K_COLOR_ON_ART, to_alpha(banner_share)), image_radii,
		                       cover_uv(image.w / image.h, game.banner->aspect()));
	} else {
		t_draw_list->add_rounded_rect(image, image_radii, faded(game.accent, to_alpha(banner_share)));
	}

	if (source.is_icon && game.icon != nullptr && banner_share < 1.0f) {
		t_draw_list->add_image(image, game.icon, faded(K_COLOR_ON_ART, to_alpha(1.0f - banner_share)), image_radii,
		                       cover_uv(image.w / image.h, game.icon->aspect()));
	}
}

auto AccountModal::draw_section_title(DrawList* t_draw_list, Rect t_main, std::string_view t_title, u8 t_alpha) const -> void
{
	const Font& font = m_fonts->body;
	const Rect  header{t_main.x, t_main.y, t_main.w, header_height(m_fonts)};

	draw_text(t_draw_list, font, Vec2{t_main.x + K_ROW_PADDING, font.centered_baseline(header)}, t_title, faded(g_theme.text, t_alpha));
	t_draw_list->add_rect(Rect{t_main.x + K_ROW_PADDING, header.bottom(), t_main.w - K_ROW_PADDING * 2.0f, 1.0f}, faded(g_theme.separator, t_alpha));
}

auto AccountModal::draw_search(DrawList* t_draw_list, Rect t_main, u8 t_alpha) -> void
{
	const Rect search = search_rect(t_main);
	if (search.w <= 0.0f) return;

	controls::draw_search_field(t_draw_list, m_fonts->secondary, search, K_SEARCH_INSET, &m_search, m_mouse, m_settings->accent, t_alpha);
}

auto AccountModal::draw_account_row(DrawList*      t_draw_list,
                                    Rect           t_main,
                                    Rect           t_row,
                                    const Account* t_account,
                                    bool           t_selected,
                                    bool           t_raised,
                                    float          t_delete_countdown,
                                    u8             t_alpha) const -> void
{
	const Rect highlight   = row_highlight(t_row);
	const bool interactive = !m_drag.lifted && !t_raised;
	const bool hovered     = interactive && highlight.contains(m_mouse);

	if (t_raised && m_lift_amount > 0.0f) {
		const auto lift_alpha = static_cast<u8>(t_alpha * m_lift_amount);

		controls::draw_popup_shadow(t_draw_list, highlight, K_ROW_RADIUS, m_lift_amount * t_alpha / 255.0f);
		t_draw_list->add_bordered_rect(highlight, rounded(K_ROW_RADIUS), faded(g_theme.popup, lift_alpha), faded(g_theme.border, lift_alpha), 1.0f);
	}

	if (t_selected) {
		t_draw_list->add_rounded_rect(highlight, rounded(K_ROW_RADIUS), faded(g_theme.row_selected, t_alpha));
		t_draw_list->add_rounded_rect(Rect{t_main.x + 8.0f, highlight.y, 3.0f, highlight.h}, rounded(1.5f), faded(m_settings->accent, t_alpha));
	} else if (hovered) {
		t_draw_list->add_rounded_rect(highlight, rounded(K_ROW_RADIUS), faded(g_theme.row_hover, t_alpha));
	}

	const Font& body        = m_fonts->body;
	const Font& secondary   = m_fonts->secondary;
	const Rect  favorite    = favorite_button_rect(t_row);
	const float text_limit  = favorite.x - K_ROW_BUTTON_GAP - t_row.x;
	const bool  has_details = t_account->note[0] != '\0' || t_account->last_used != 0;

	const float block_height      = body.line_height() + K_ROW_LINE_GAP + secondary.line_height();
	const float block_y           = t_row.y + (t_row.h - block_height) * 0.5f;
	const float username_baseline = has_details ? block_y + body.ascent : body.centered_baseline(t_row);

	const std::string_view region         = t_account->region;
	const float            chip_space     = region.empty() ? 0.0f : controls::region_chip_width(secondary, region) + K_REGION_CHIP_GAP;
	const float            username_limit = std::max(0.0f, text_limit - chip_space);

	draw_text_truncated(t_draw_list, body, Vec2{t_row.x, username_baseline}, t_account->username, username_limit, faded(g_theme.text, t_alpha));

	if (!region.empty()) {
		const float username_width = std::min(text_width(body, t_account->username), username_limit);
		const float line_center    = username_baseline - body.ascent + body.line_height() * 0.5f;
		controls::draw_region_chip(t_draw_list, secondary, t_row.x + username_width + K_REGION_CHIP_GAP, line_center, region, t_alpha);
	}

	if (has_details) {
		const float details_baseline = block_y + body.line_height() + K_ROW_LINE_GAP + secondary.ascent;
		controls::draw_account_details(t_draw_list, secondary, Vec2{t_row.x, details_baseline}, text_limit, *t_account, t_alpha);
	}

	const auto separator_alpha = static_cast<u8>(t_alpha * (t_raised ? 1.0f - m_lift_amount : 1.0f));
	t_draw_list->add_rect(Rect{t_row.x, t_row.bottom() - 1.0f, t_row.w, 1.0f}, faded(g_theme.separator, separator_alpha));

	const Rect edit              = edit_button_rect(t_row);
	const Rect remove            = remove_button_rect(t_row);
	const bool favorite_hovered  = interactive && favorite.contains(m_mouse);
	const bool edit_hovered      = interactive && edit.contains(m_mouse);
	const bool armed             = t_delete_countdown > 0.0f;
	const bool pointer_on_remove = interactive && remove.contains(m_mouse);
	const bool remove_hovered    = armed || pointer_on_remove;

	if (favorite_hovered) {
		controls::draw_circular_hover(t_draw_list, favorite, g_theme.shadow, g_theme.control_hover, t_alpha);
	}

	if (edit_hovered) {
		controls::draw_circular_hover(t_draw_list, edit, g_theme.shadow, g_theme.control_hover, t_alpha);
	}

	const Color danger = armed ? controls::confirm_red() : g_theme.error;

	if (remove_hovered) {
		const float tint = armed && pointer_on_remove ? K_ARMED_DANGER_TINT : K_DANGER_TINT;
		controls::draw_circular_hover(t_draw_list, remove, danger, mix(g_theme.surface, danger, tint), t_alpha);
	}

	if (t_account->favorite) {
		controls::draw_favorite(t_draw_list, m_assets, favorite.inset(K_ROW_ICON_INSET), true, faded(m_settings->accent, t_alpha));
	} else if (hovered) {
		controls::draw_favorite(t_draw_list, m_assets, favorite.inset(K_ROW_ICON_INSET), false,
		                        faded(favorite_hovered ? g_theme.text : g_theme.text_faint, t_alpha));
	}

	t_draw_list->add_image(edit.inset(K_ROW_ICON_INSET), m_assets->get(Asset::ICON_EDIT), faded(edit_hovered ? g_theme.text : g_theme.text_dim, t_alpha));
	controls::draw_x(t_draw_list, remove, faded(remove_hovered ? danger : g_theme.text_dim, t_alpha));

	if (armed) {
		controls::draw_circular_countdown(t_draw_list, remove, t_delete_countdown, faded(danger, t_alpha));
	}
}

auto AccountModal::empty_state(Rect t_region) const -> AccountModal::EmptyState
{
	const Font& body          = m_fonts->body;
	const Font& secondary     = m_fonts->secondary;
	const float button_height = action_button_height(m_fonts);
	const float stack_height  = K_EMPTY_ICON_SIZE + K_EMPTY_GAP + body.line_height() + K_EMPTY_LINE_GAP + secondary.line_height() + K_EMPTY_GAP + button_height;
	const float center_x      = t_region.center().x;
	const float top           = t_region.center().y - stack_height * 0.5f;

	EmptyState state{};
	state.icon           = Rect{center_x - K_EMPTY_ICON_SIZE * 0.5f, top, K_EMPTY_ICON_SIZE, K_EMPTY_ICON_SIZE};
	state.title_baseline = state.icon.bottom() + K_EMPTY_GAP + body.ascent;
	state.hint_baseline  = state.icon.bottom() + K_EMPTY_GAP + body.line_height() + K_EMPTY_LINE_GAP + secondary.ascent;
	state.button = Rect{center_x - K_EMPTY_BUTTON_WIDTH * 0.5f,
	                    state.icon.bottom() + K_EMPTY_GAP + body.line_height() + K_EMPTY_LINE_GAP + secondary.line_height() + K_EMPTY_GAP, K_EMPTY_BUTTON_WIDTH,
	                    button_height};

	return state;
}

auto AccountModal::draw_empty_state(DrawList* t_draw_list, Rect t_region, u8 t_alpha) const -> void
{
	const Game&                game      = m_library->games[static_cast<u32>(m_game)];
	const EmptyState           state     = empty_state(t_region);
	const Font&                body      = m_fonts->body;
	const Font&                secondary = m_fonts->secondary;
	constexpr std::string_view TITLE     = "No accounts yet";
	constexpr std::string_view HINT      = "Add one to log in with a single click.";

	if (game.icon != nullptr) {
		t_draw_list->add_image(state.icon, game.icon, faded(K_COLOR_ON_ART, t_alpha), rounded(K_EMPTY_ICON_RADIUS));
	} else {
		t_draw_list->add_rounded_rect(state.icon, rounded(K_EMPTY_ICON_RADIUS), faded(game.accent, t_alpha));
	}

	draw_text(t_draw_list, body, Vec2{t_region.center().x - text_width(body, TITLE) * 0.5f, state.title_baseline}, TITLE, faded(g_theme.text, t_alpha));
	draw_text(t_draw_list, secondary, Vec2{t_region.center().x - text_width(secondary, HINT) * 0.5f, state.hint_baseline}, HINT,
	          faded(g_theme.text_dim, t_alpha));
	controls::draw_button(t_draw_list, body, state.button, "Add account", controls::ButtonStyle::ACCENT, m_settings->accent, true,
	                      state.button.contains(m_mouse), t_alpha);
}

auto AccountModal::draw_no_matches(DrawList* t_draw_list, Rect t_region, u8 t_alpha) const -> void
{
	const Font&                body      = m_fonts->body;
	const Font&                secondary = m_fonts->secondary;
	constexpr std::string_view TITLE     = "No matches";
	constexpr std::string_view HINT      = "Try a different name or note.";

	const float stack_height = body.line_height() + K_EMPTY_LINE_GAP + secondary.line_height();
	const float top          = t_region.center().y - stack_height * 0.5f;
	const float center_x     = t_region.center().x;

	draw_text(t_draw_list, body, Vec2{center_x - text_width(body, TITLE) * 0.5f, top + body.ascent}, TITLE, faded(g_theme.text_dim, t_alpha));
	draw_text(t_draw_list, secondary, Vec2{center_x - text_width(secondary, HINT) * 0.5f, top + body.line_height() + K_EMPTY_LINE_GAP + secondary.ascent}, HINT,
	          faded(g_theme.text_faint, t_alpha));
}

auto AccountModal::draw_account_list(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) -> void
{
	const Rect main = t_layout.main_column;
	draw_section_title(t_draw_list, main, K_ACCOUNTS_TITLE, t_alpha);
	draw_search(t_draw_list, main, t_alpha);

	const Rect add         = add_button_rect(main);
	const bool add_hovered = add.contains(m_mouse);

	if (add_hovered) {
		controls::draw_circular_hover(t_draw_list, add, g_theme.shadow, g_theme.control_hover, t_alpha);
	}

	t_draw_list->add_image(add.centered(24.0f, 24.0f), m_assets->get(Asset::ICON_ADD), faded(add_hovered ? g_theme.text : g_theme.text_dim, t_alpha));

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

	t_draw_list->push_clip(rows.region);

	for (u32 i = 0; i < rows.accounts.count; i += 1) {
		if (m_raised_row == i) continue;

		const Rect row = row_rect(t_layout, rows, i);
		if (!row.overlaps_vertically(rows.region)) continue;

		draw_account_row(t_draw_list, main, row, m_library->account(rows.accounts.refs[i]), static_cast<i32>(i) == selected, false,
		                 delete_countdown(rows.accounts.refs[i]), t_alpha);
	}

	if (m_raised_row && *m_raised_row < rows.accounts.count) {
		const u32   raised = *m_raised_row;
		const float top    = m_drag.lifted ? lifted_top(rows) : raised * rows.row_height + m_row_offsets[raised];

		draw_account_row(t_draw_list, main, row_rect_at(t_layout, rows, top), m_library->account(rows.accounts.refs[raised]),
		                 static_cast<i32>(raised) == selected, true, delete_countdown(rows.accounts.refs[raised]), t_alpha);
	}

	t_draw_list->pop_clip();

	m_rows_scroll.draw_edge_fade(t_draw_list, rows.region, rows.scroll, faded(g_theme.surface, t_alpha));
	m_rows_scroll.draw(t_draw_list, rows.scroll, m_mouse, t_alpha);
}

auto AccountModal::draw_login_progress(DrawList* t_draw_list, Rect t_main, u8 t_alpha) const -> void
{
	const LoginStage stage     = m_session->stage();
	const bool       finished  = m_session->is_finished();
	const Font&      body      = m_fonts->body;
	const Font&      secondary = m_fonts->secondary;

	const float width = std::min(t_main.w - K_ROW_PADDING * 2.0f, K_PROGRESS_MAX_WIDTH);
	const Rect  bar{snapped_to_pixel(t_main.center().x - width * 0.5f), snapped_to_pixel(t_main.center().y), width, K_PROGRESS_BAR_HEIGHT};
	const float status_baseline = bar.y - K_PROGRESS_TEXT_GAP;
	const float step_baseline   = bar.bottom() + K_PROGRESS_TEXT_GAP + secondary.ascent * K_CAP_HEIGHT_SHARE;
	const float alpha_scale     = t_alpha / 255.0f;

	const auto draw_status = [&](std::string_view t_text, float t_opacity, float t_offset) {
		if (t_text.empty() || t_opacity <= 0.0f) return;

		std::string_view lines[K_MAX_MESSAGE_LINES];
		const u32        line_count = wrap_text(body, t_text, width, lines);
		const auto       alpha      = static_cast<u8>(t_alpha * t_opacity);
		float            baseline   = status_baseline + t_offset - static_cast<float>(line_count > 0 ? line_count - 1 : 0) * body.line_height();

		for (const std::string_view line : std::span{lines, line_count}) {
			draw_text(t_draw_list, body, Vec2{snapped_to_pixel(bar.center().x - text_width(body, line) * 0.5f), baseline}, line, faded(g_theme.text, alpha));
			baseline += body.line_height();
		}
	};

	const float change = m_session->status_change();
	draw_status(m_session->previous_status(), 1.0f - change, -K_PROGRESS_TEXT_RISE * change);
	draw_status(m_session->status(), change, K_PROGRESS_TEXT_RISE * (1.0f - change));

	const Color outcome    = stage == LoginStage::SUCCESS ? g_theme.success : g_theme.error;
	const Color fill_color = finished ? mix(m_settings->accent, outcome, m_session->outcome()) : m_settings->accent;
	const Rect  fill{bar.x, bar.y, bar.w * std::clamp(m_session->progress(), 0.0f, 1.0f), bar.h};

	t_draw_list->add_rounded_rect(bar, rounded(bar.h * 0.5f), faded(g_theme.control, t_alpha));

	if (fill.w > 0.5f) {
		t_draw_list->add_shadow(fill, bar.h * 0.5f, K_PROGRESS_GLOW_BLUR, with_alpha(fill_color, static_cast<u8>(K_PROGRESS_GLOW_ALPHA * alpha_scale)));
		t_draw_list->add_rounded_rect(fill, rounded(bar.h * 0.5f), faded(fill_color, t_alpha));

		if (!finished) {
			const float travel  = std::fmod(m_session->seconds() * K_SHEEN_PASSES_PER_SECOND, 1.0f);
			const float sheen_x = fill.x - K_SHEEN_WIDTH + travel * (fill.w + K_SHEEN_WIDTH);
			const float half    = K_SHEEN_WIDTH * 0.5f;
			const Color edge    = with_alpha(lightened(fill_color, 80), 0);
			const Color peak    = with_alpha(lightened(fill_color, 80), static_cast<u8>(K_SHEEN_ALPHA * alpha_scale));

			t_draw_list->push_clip(fill);
			t_draw_list->add_gradient(Rect{sheen_x, fill.y, half, fill.h}, edge, peak, edge, peak);
			t_draw_list->add_gradient(Rect{sheen_x + half, fill.y, half, fill.h}, peak, edge, peak, edge);
			t_draw_list->pop_clip();
		}
	}

	const u32 step = m_session->step();
	if (step == 0) return;

	char                   label[24];
	const int              written = std::snprintf(label, sizeof(label), "Step %u of %u", step, LoginSession::K_STEP_COUNT);
	const std::string_view step_text{label, static_cast<usize>(std::max(written, 0))};

	draw_text(t_draw_list, secondary, Vec2{snapped_to_pixel(bar.center().x - text_width(secondary, step_text) * 0.5f), step_baseline}, step_text,
	          faded(g_theme.text_faint, static_cast<u8>(t_alpha * (1.0f - m_session->outcome()))));
}

auto AccountModal::draw_edit_header(DrawList* t_draw_list, Rect t_main, u8 t_alpha) const -> void
{
	const Font& body      = m_fonts->body;
	const Font& secondary = m_fonts->secondary;
	const Game& game      = m_library->games[static_cast<u32>(m_game)];
	const Rect  header{t_main.x, t_main.y, t_main.w, edit_header_height()};
	const Rect  icon{t_main.x + K_ROW_PADDING, header.center().y - K_EDIT_HEADER_ICON_SIZE * 0.5f, K_EDIT_HEADER_ICON_SIZE, K_EDIT_HEADER_ICON_SIZE};

	if (game.icon != nullptr) {
		t_draw_list->add_image(icon, game.icon, faded(K_COLOR_ON_ART, t_alpha), rounded(K_EDIT_HEADER_ICON_RADIUS));
	} else {
		t_draw_list->add_rounded_rect(icon, rounded(K_EDIT_HEADER_ICON_RADIUS), faded(game.accent, t_alpha));
	}

	const float block  = body.line_height() + K_EDIT_HEADER_LINE_GAP + secondary.line_height();
	const float top    = header.center().y - block * 0.5f;
	const float text_x = icon.right() + K_EDIT_HEADER_GAP;
	const float limit  = header.right() - K_ROW_PADDING - text_x;

	draw_text_truncated(t_draw_list, body, Vec2{text_x, top + body.ascent}, m_edited ? "Edit account" : "Add account", limit, faded(g_theme.text, t_alpha));
	draw_text_truncated(t_draw_list, secondary, Vec2{text_x, top + body.line_height() + K_EDIT_HEADER_LINE_GAP + secondary.ascent}, game.title, limit,
	                    faded(g_theme.text_faint, t_alpha));
	t_draw_list->add_rect(Rect{t_main.x + K_ROW_PADDING, header.bottom(), t_main.w - K_ROW_PADDING * 2.0f, 1.0f}, faded(g_theme.separator, t_alpha));
}

auto AccountModal::draw_show_in(DrawList* t_draw_list, const FormLayout& t_form, u8 t_alpha) const -> void
{
	const Font& body        = m_fonts->body;
	const Font& secondary   = m_fonts->secondary;
	const Color accent      = m_settings->accent;
	const Rect  box         = t_form.inputs[K_SHOW_IN_ROW];
	const bool  live        = !m_region_list.is_open() && t_form.region.contains(m_mouse);
	const bool  box_hovered = live && box.contains(m_mouse);

	draw_text(t_draw_list, secondary, Vec2{t_form.labels[K_SHOW_IN_ROW].x, t_form.labels[K_SHOW_IN_ROW].y + secondary.ascent}, K_SHOW_IN_FIELD_LABEL,
	          faded(m_show_in_open ? mix(g_theme.text_dim, accent, 0.6f) : g_theme.text_dim, t_alpha));

	Color border = g_theme.separator;
	if (m_show_in_open) {
		border = accent;
	} else if (box_hovered) {
		border = g_theme.border;
	}

	draw_input_box(t_draw_list, box, border, false, accent, t_alpha);

	float              x = box.x + K_INPUT_PADDING_X;
	std::optional<u32> only;

	for (u32 index = 0; index < m_library->game_count; index += 1) {
		const Game& game       = m_library->games[index];
		const bool  selected   = (m_visible_mask & (1u << index)) != 0;
		const u8    icon_alpha = selected ? 255 : K_SUMMARY_UNSELECTED_ICON_ALPHA;
		const Rect  icon{x, box.center().y - K_SUMMARY_ICON_SIZE * 0.5f, K_SUMMARY_ICON_SIZE, K_SUMMARY_ICON_SIZE};

		if (selected) only = index;

		if (game.icon != nullptr) {
			t_draw_list->add_image(icon, game.icon, faded(with_alpha(K_COLOR_ON_ART, icon_alpha), t_alpha), rounded(K_SUMMARY_ICON_RADIUS));
		} else {
			t_draw_list->add_rounded_rect(icon, rounded(K_SUMMARY_ICON_RADIUS), faded(with_alpha(game.accent, icon_alpha), t_alpha));
		}

		x += K_SUMMARY_ICON_SIZE + K_SUMMARY_ICON_GAP;
	}

	const auto  count = static_cast<u32>(std::popcount(m_visible_mask));
	const Rect  chevron{box.right() - K_CHEVRON_MARGIN - K_CHEVRON_SIZE.x, box.center().y - K_CHEVRON_SIZE.y * 0.5f, K_CHEVRON_SIZE.x, K_CHEVRON_SIZE.y};
	const float text_left  = x - K_SUMMARY_ICON_GAP + K_SUMMARY_TEXT_GAP;
	const float text_right = chevron.x - K_SUMMARY_TEXT_GAP;
	const float room       = std::max(0.0f, text_right - text_left);
	char        summary[96];
	int         written = 0;

	if (count >= m_library->game_count) {
		written = std::snprintf(summary, sizeof(summary), "All games");
	} else if (count == 1 && only) {
		const Game& game = m_library->games[*only];
		written          = std::snprintf(summary, sizeof(summary), "Only %.*s", static_cast<int>(game.title.size()), game.title.data());

		if (!game.short_title.empty() && text_width(body, std::string_view{summary, static_cast<usize>(std::max(written, 0))}) > room) {
			written = std::snprintf(summary, sizeof(summary), "Only %.*s", static_cast<int>(game.short_title.size()), game.short_title.data());
		}
	} else {
		written = std::snprintf(summary, sizeof(summary), "%u of %u", count, m_library->game_count);
	}

	const std::string_view summary_text{summary, static_cast<usize>(std::max(written, 0))};
	const float            summary_width = std::min(text_width(body, summary_text), room);

	draw_text_truncated(t_draw_list, body, Vec2{snapped_to_pixel(text_right - summary_width), body.centered_baseline(box)}, summary_text, room,
	                    faded(g_theme.text_dim, t_alpha));
	controls::draw_chevron(t_draw_list, chevron, m_show_in_open, faded(m_show_in_open || box_hovered ? g_theme.text : g_theme.text_dim, t_alpha));

	if (m_show_in_amount <= 0.001f) return;

	const float revealed = (K_TILES_TOP_GAP + t_form.tiles.h + 2.0f) * m_show_in_amount;
	t_draw_list->push_clip(Rect{t_form.tiles.x - 4.0f, box.bottom(), t_form.tiles.w + 8.0f, revealed});

	for (u32 index = 0; index < m_library->game_count; index += 1) {
		const Game& game         = m_library->games[index];
		const Rect  tile         = show_in_tile(t_form, index);
		const bool  selected     = (m_visible_mask & (1u << index)) != 0;
		const bool  locked       = selected && count == 1;
		const bool  tile_hovered = live && !locked && tile.contains(m_mouse);

		Color tile_border = g_theme.separator;
		if (selected) {
			tile_border = accent;
		} else if (tile_hovered) {
			tile_border = g_theme.border;
		}

		t_draw_list->add_bordered_rect(tile, rounded(K_TILE_RADIUS),
		                               faded(selected ? mix(g_theme.field, accent, K_TILE_SELECTED_TINT) : g_theme.field, t_alpha), faded(tile_border, t_alpha),
		                               selected ? K_INPUT_FOCUS_BORDER : 1.0f);

		const Rect icon{tile.center().x - K_TILE_ICON_SIZE * 0.5f, tile.y + K_TILE_PADDING_Y, K_TILE_ICON_SIZE, K_TILE_ICON_SIZE};
		const u8   icon_alpha = selected ? 255 : K_TILE_UNSELECTED_ICON_ALPHA;

		if (game.icon != nullptr) {
			t_draw_list->add_image(icon, game.icon, faded(with_alpha(K_COLOR_ON_ART, icon_alpha), t_alpha), rounded(K_TILE_ICON_RADIUS));
		} else {
			t_draw_list->add_rounded_rect(icon, rounded(K_TILE_ICON_RADIUS), faded(with_alpha(game.accent, icon_alpha), t_alpha));
		}

		const std::string_view name       = game.short_title.empty() ? game.title : game.short_title;
		const float            name_width = std::min(text_width(secondary, name), tile.w - 8.0f);
		draw_text_truncated(t_draw_list, secondary,
		                    Vec2{snapped_to_pixel(tile.center().x - name_width * 0.5f), icon.bottom() + K_TILE_LABEL_GAP + secondary.ascent}, name,
		                    tile.w - 8.0f, faded(selected || tile_hovered ? g_theme.text : g_theme.text_dim, t_alpha));
	}

	t_draw_list->pop_clip();
}

auto AccountModal::draw_edit_form(DrawList* t_draw_list, Rect t_main, u8 t_alpha) -> void
{
	draw_edit_header(t_draw_list, t_main, t_alpha);

	const Font&          body         = m_fonts->body;
	const Font&          secondary    = m_fonts->secondary;
	const Color          accent       = m_settings->accent;
	const Color          text         = faded(g_theme.text, t_alpha);
	const Color          caret        = faded(accent, t_alpha);
	const Color          active_label = mix(g_theme.text_dim, accent, 0.6f);
	const FormLayout     form         = form_layout(t_main);
	const ScrollGeometry scroll       = form_scroll(t_main);
	const Rect           reveal       = reveal_button_rect(t_main);
	const bool           live         = !m_region_list.is_open() && form.region.contains(m_mouse);

	t_draw_list->push_clip(form.region);

	const auto draw_label = [&](u32 t_row, std::string_view t_label, Color t_color) {
		const Vec2 at{form.labels[t_row].x, form.labels[t_row].y + secondary.ascent};
		draw_text(t_draw_list, secondary, at, t_label, faded(t_color, t_alpha));
		return at.x + text_width(secondary, t_label);
	};

	const auto draw_hint = [&](u32 t_row, controls::NoticeKind t_kind, std::string_view t_hint, Color t_color) {
		const Rect input = form.inputs[t_row];
		controls::draw_notice(t_draw_list, secondary, Vec2{input.x + 2.0f, input.bottom() + K_FORM_HINT_GAP}, t_kind, t_hint, faded(t_color, t_alpha));
	};

	for (u32 i = 0; i < K_FIELD_COUNT; i += 1) {
		TextInput* input       = &m_fields[i];
		const Rect box         = form.inputs[i];
		const bool focused     = input->is_focused();
		const bool required    = i == static_cast<u32>(EditField::USERNAME) || i == static_cast<u32>(EditField::PASSWORD);
		const bool missing     = m_show_required && required && input->value().empty();
		const bool box_hovered = live && !focused && box.contains(m_mouse);

		Color label  = g_theme.text_dim;
		Color border = g_theme.separator;
		if (missing) {
			label  = g_theme.error;
			border = g_theme.error;
		} else if (focused) {
			label  = active_label;
			border = accent;
		} else if (box_hovered) {
			border = g_theme.border;
		}

		const float label_end = draw_label(i, K_FIELD_SPECS[i].label, label);
		if (i == static_cast<u32>(EditField::NOTE)) {
			draw_text(t_draw_list, secondary, Vec2{label_end, form.labels[i].y + secondary.ascent}, K_OPTIONAL_SUFFIX, faded(g_theme.text_faint, t_alpha));
		}

		draw_input_box(t_draw_list, box, border, focused && !missing, accent, t_alpha);
		input->draw(t_draw_list, body, field_text_rect(t_main, i), text, caret);

		if (missing) {
			draw_hint(i, controls::NoticeKind::ALERT, "Required", g_theme.error);
		} else if (i == static_cast<u32>(EditField::PASSWORD) && focused && os::is_caps_lock_on()) {
			draw_hint(i, controls::NoticeKind::CAPS_LOCK, "Caps Lock is on", controls::caution_color());
		}
	}

	const Rect region         = form.inputs[K_REGION_ROW];
	const bool region_open    = m_region_list.is_open();
	const bool region_hovered = live && region.contains(m_mouse);

	const float region_label_end = draw_label(K_REGION_ROW, K_REGION_FIELD_LABEL, region_open ? active_label : g_theme.text_dim);
	draw_text(t_draw_list, secondary, Vec2{region_label_end, form.labels[K_REGION_ROW].y + secondary.ascent}, K_OPTIONAL_SUFFIX,
	          faded(g_theme.text_faint, t_alpha));

	Color region_border = g_theme.separator;
	if (region_open) {
		region_border = accent;
	} else if (region_hovered) {
		region_border = g_theme.border;
	}

	draw_input_box(t_draw_list, region, region_border, region_open, accent, t_alpha);

	std::string_view region_value = m_region;
	for (const RegionOption& option : K_REGION_OPTIONS) {
		if (!option.code.empty() && option.code == region_value) {
			region_value = option.label;
		}
	}

	const Rect  region_chevron{region.right() - K_CHEVRON_MARGIN - K_CHEVRON_SIZE.x, region.center().y - K_CHEVRON_SIZE.y * 0.5f, K_CHEVRON_SIZE.x,
	                           K_CHEVRON_SIZE.y};
	const float region_value_x = region.x + K_INPUT_PADDING_X;

	draw_text_truncated(t_draw_list, body, Vec2{region_value_x, body.centered_baseline(region)}, region_value.empty() ? std::string_view{"None"} : region_value,
	                    region_chevron.x - K_INPUT_PADDING_X - region_value_x, region_value.empty() ? faded(g_theme.text_faint, t_alpha) : text);
	controls::draw_chevron(t_draw_list, region_chevron, region_open, faded(region_open || region_hovered ? g_theme.text : g_theme.text_dim, t_alpha));

	controls::draw_eye(t_draw_list, m_assets, reveal, !field(EditField::PASSWORD)->is_masked(),
	                   faded(is_reveal_hit(t_main, m_mouse) ? g_theme.text : g_theme.text_dim, t_alpha));

	draw_show_in(t_draw_list, form, t_alpha);

	t_draw_list->pop_clip();

	m_form_scroll.draw_edge_fade(t_draw_list, form.region, scroll, faded(g_theme.surface, t_alpha));
	m_form_scroll.draw(t_draw_list, scroll, m_mouse, t_alpha);
}

auto AccountModal::draw_edit_footer(DrawList* t_draw_list, Rect t_footer, u8 t_alpha) const -> void
{
	const Font& body        = m_fonts->body;
	const Font& secondary   = m_fonts->secondary;
	const Rect  save        = primary_button_rect(t_footer);
	const Rect  cancel      = cancel_button_rect(save);
	const float cap         = controls::keycap_height(secondary);
	const float cap_y       = snapped_to_pixel(t_footer.center().y - cap * 0.5f);
	const float baseline    = secondary.centered_baseline(Rect{t_footer.x, cap_y, t_footer.w, cap});
	const Color hint        = faded(g_theme.text_faint, t_alpha);
	const float hints_width = controls::keycap_width(secondary, "Enter") + K_HINT_KEY_GAP + text_width(secondary, "save") + K_HINT_GAP +
	                          controls::keycap_width(secondary, "Esc") + K_HINT_KEY_GAP + text_width(secondary, "cancel");
	float       x           = t_footer.x + K_ROW_PADDING;

	if (x + hints_width <= cancel.x - K_HINT_GAP) {
		for (const auto& [key, action] : {std::pair{"Enter", "save"}, std::pair{"Esc", "cancel"}}) {
			const float width = controls::keycap_width(secondary, key);

			controls::draw_keycap(t_draw_list, secondary, Rect{snapped_to_pixel(x), cap_y, width, cap}, key, g_theme.surface, t_alpha);
			x += width + K_HINT_KEY_GAP;
			draw_text(t_draw_list, secondary, Vec2{snapped_to_pixel(x), baseline}, action, hint);
			x += text_width(secondary, action) + K_HINT_GAP;
		}
	}

	controls::draw_button(t_draw_list, body, save, save_label(), controls::ButtonStyle::ACCENT, m_settings->accent, has_changes(), save.contains(m_mouse),
	                      t_alpha);
	controls::draw_button(t_draw_list, body, cancel, "Cancel", controls::ButtonStyle::GHOST, m_settings->accent, true, cancel.contains(m_mouse), t_alpha);
}

auto AccountModal::draw_footer(DrawList* t_draw_list, Rect t_footer, u8 t_alpha) const -> void
{
	const Font& body          = m_fonts->body;
	const Font& secondary     = m_fonts->secondary;
	const float hint_baseline = secondary.centered_baseline(t_footer);
	const Rect  primary       = primary_button_rect(t_footer);
	const float hint_width    = primary.x - K_ROW_PADDING * 2.0f - t_footer.x;

	t_draw_list->add_rect(Rect{t_footer.x, t_footer.y, t_footer.w, 1.0f}, faded(g_theme.separator, t_alpha));

	switch (m_mode) {
		using enum Mode;

		case EDIT_ACCOUNT: {
			draw_edit_footer(t_draw_list, t_footer, t_alpha);
			break;
		}

		case LOGIN_PROGRESS: {
			const bool finished = m_session->is_finished();

			draw_text_truncated(t_draw_list, secondary, Vec2{t_footer.x + K_ROW_PADDING, hint_baseline}, "", hint_width, faded(g_theme.text_faint, t_alpha));

			if (asks_for_permission()) {
				const Rect back = cancel_button_rect(primary);
				controls::draw_button(t_draw_list, body, primary, K_PERMISSION_BUTTON_LABEL, controls::ButtonStyle::ACCENT, m_settings->accent, true,
				                      primary.contains(m_mouse), t_alpha);
				controls::draw_button(t_draw_list, body, back, "Back", controls::ButtonStyle::GHOST, m_settings->accent, true, back.contains(m_mouse), t_alpha);
				break;
			}

			controls::draw_button(t_draw_list, body, primary, finished ? "Back" : "Cancel", controls::ButtonStyle::NEUTRAL, m_settings->accent, true,
			                      primary.contains(m_mouse), t_alpha);
			break;
		}

		case ACCOUNT_LIST: {
			const bool can_login = selected_row(displayed_accounts()) >= 0;

			draw_text_truncated(t_draw_list, secondary, Vec2{t_footer.x + K_ROW_PADDING, hint_baseline}, "Select an account to log in", hint_width,
			                    faded(g_theme.text_faint, t_alpha));
			controls::draw_button(t_draw_list, body, primary, "Login", controls::ButtonStyle::ACCENT, m_settings->accent, can_login, primary.contains(m_mouse),
			                      t_alpha);
			break;
		}
	}
}

auto AccountModal::draw(DrawList* t_draw_list) -> void
{
	PULSAR_PROFILE_SCOPE("AccountModal.Draw");

	const bool art_visible = m_art_source && m_morph_progress > 0.0f;
	if ((m_open_amount <= 0.001f && !art_visible) || !has_game()) return;

	const auto alpha  = to_alpha(m_open_amount);
	const Vec2 window = m_window->size();
	if (!is_docked()) {
		t_draw_list->add_rect(Rect{0.0f, 0.0f, window.x, window.y}, faded(g_theme.scrim, to_alpha(m_open_amount)));
	}

	const Layout current  = layout();
	const float  scale    = is_docked() || m_art_source ? 1.0f : K_PANEL_CLOSED_SCALE + (1.0f - K_PANEL_CLOSED_SCALE) * m_open_amount;
	const bool   morphing = m_art_source && m_morph_progress != 1.0f;

	t_draw_list->push_scale(current.panel.center(), scale);
	draw_chrome(t_draw_list, current, !morphing, alpha);
	t_draw_list->pop_scale();

	if (morphing) {
		draw_morphing_art(t_draw_list, current, scale);
	}

	t_draw_list->push_scale(current.panel.center(), scale);
	draw_back_badge(t_draw_list, current, alpha);

	switch (m_mode) {
		using enum Mode;

		case ACCOUNT_LIST: {
			draw_account_list(t_draw_list, current, alpha);
			break;
		}

		case LOGIN_PROGRESS: {
			draw_login_progress(t_draw_list, current.main_column, alpha);
			break;
		}

		case EDIT_ACCOUNT: {
			draw_edit_form(t_draw_list, current.main_column, alpha);
			break;
		}
	}

	draw_footer(t_draw_list, current.footer, alpha);

	if (m_mode == Mode::EDIT_ACCOUNT) {
		m_region_list.draw(t_draw_list, m_mouse);
	}

	m_tooltip.draw(t_draw_list, m_fonts, current.panel, alpha);

	t_draw_list->pop_scale();
}
