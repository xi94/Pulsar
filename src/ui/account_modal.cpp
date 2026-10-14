#include "ui/account_modal.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <numbers>
#include <ranges>
#include <span>
#include <utility>

#include "core/animation.h"
#include "core/profiler.h"
#include "core/regions.h"
#include "core/settings.h"
#include "core/str.h"
#include "gfx/assets.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "os/window.h"
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
constexpr float K_EXACT_ROUNDNESS             = 1.0f;
constexpr float K_PANEL_RADIUS                = 16.0f;

constexpr float K_ART_COLUMN_FRACTION      = 0.35f;
constexpr float K_ART_SEPARATOR_WIDTH      = 2.0f;
constexpr float K_CLOSE_BADGE_SIZE         = 40.0f;
constexpr float K_CLOSE_BADGE_MARGIN       = 12.0f;
constexpr float K_BACK_HEIGHT              = 32.0f;
constexpr float K_BACK_PADDING_X           = 12.0f;
constexpr float K_BACK_ICON_SIZE           = 16.0f;
constexpr float K_BACK_ICON_GAP            = 6.0f;
constexpr float K_ICON_CROSSFADE_SHARE     = 0.35f;
constexpr float K_MORPH_SECONDS            = 0.22f;
constexpr float K_PANEL_REVEAL_PROGRESS    = 0.6f;
constexpr float K_MORPH_RETURN_OPEN_AMOUNT = 0.35f;
constexpr float K_INTERACTIVE_OPEN_AMOUNT  = 0.5f;

constexpr std::string_view K_ACCOUNTS_TITLE         = "Accounts";
constexpr float            K_HEADER_PADDING_TOP     = 16.0f;
constexpr float            K_HEADER_PADDING_BOTTOM  = 12.0f;
constexpr float            K_TITLE_GAP              = 16.0f;
constexpr float            K_CONTROL_HEIGHT         = 30.0f;
constexpr float            K_CONTROL_GAP            = 10.0f;
constexpr float            K_CONTROL_RADIUS         = 8.0f;
constexpr float            K_ADD_PADDING_X          = 12.0f;
constexpr float            K_ADD_ICON_SIZE          = 14.0f;
constexpr float            K_ADD_ICON_GAP           = 6.0f;
constexpr std::string_view K_ADD_LABEL              = "Add";
constexpr float            K_SEARCH_MAX_WIDTH       = 180.0f;
constexpr float            K_SEARCH_MIN_WIDTH       = 90.0f;
constexpr float            K_SEARCH_INSET           = 10.0f;
constexpr u32              K_SEARCH_MAX_LENGTH      = 64;
constexpr float            K_SCROLLED_LINE_DISTANCE = 12.0f;
constexpr std::string_view K_LOGIN_LABEL            = "Login";
constexpr std::string_view K_BACK_LABEL             = "Back";
constexpr u8               K_BACK_RIM_ALPHA         = 46;
constexpr u8               K_BACK_RIM_HOVER_ALPHA   = 90;
constexpr float            K_LOGIN_PADDING_X        = 12.0f;
constexpr float            K_SHADOW_REACH           = 48.0f;

constexpr float K_ROW_PADDING         = 24.0f;
constexpr float K_ROW_TOP_PADDING     = 14.0f;
constexpr float K_ROW_LINE_GAP        = 4.0f;
constexpr float K_ROW_BOTTOM_PADDING  = 10.0f;
constexpr float K_ROW_BUTTON_GAP      = 14.0f;
constexpr float K_ROW_ICON_INSET      = 5.0f;
constexpr float K_ROW_RADIUS          = 10.0f;
constexpr float K_SCROLLBAR_MARGIN    = 4.0f;
constexpr float K_ROW_TEXT_GAP        = 6.0f;
constexpr float K_ROW_HOVER_SHARE     = 0.3f;
constexpr float K_ROW_BLEED           = 8.0f;
constexpr float K_ROW_HIGHLIGHT_INSET = 3.0f;
constexpr float K_SELECTED_ALPHA      = 38.0f;
constexpr float K_LIST_END_PADDING    = 10.0f;
constexpr float K_HOVER_EASE_RATE     = 20.0f;
constexpr float K_PILL_RADIUS         = 7.0f;
constexpr float K_PILL_SLIDE          = 6.0f;

constexpr float K_DRAG_THRESHOLD    = 4.0f;
constexpr float K_AUTO_SCROLL_ZONE  = 0.6f;
constexpr float K_AUTO_SCROLL_SPEED = 540.0f;

constexpr float            K_ACTION_BUTTON_WIDTH     = 108.0f;
constexpr float            K_ACTION_BUTTON_GAP       = 16.0f;
constexpr std::string_view K_PERMISSION_BUTTON_LABEL = "Open settings";

constexpr float            K_EMPTY_LINE_GAP = 4.0f;
constexpr float            K_ZONE_TOP_INSET = 4.0f;
constexpr std::string_view K_ZONE_TITLE     = "Add your first account";
constexpr std::string_view K_ZONE_HINT      = "Save a Riot login once, then sign in with one click.";

constexpr float            K_FORM_TOP_PADDING          = 14.0f;
constexpr float            K_FORM_COLUMN_GAP           = 12.0f;
constexpr float            K_FORM_TWO_COLUMN_MIN_WIDTH = 400.0f;
constexpr float            K_FORM_LABEL_GAP            = 6.0f;
constexpr float            K_FORM_HINT_GAP             = 4.0f;
constexpr float            K_FORM_HINT_INDENT          = 2.0f;
constexpr float            K_INPUT_RADIUS              = 8.0f;
constexpr float            K_INPUT_PADDING_X           = 10.0f;
constexpr float            K_INPUT_FOCUS_BORDER        = 1.5f;
constexpr float            K_FOCUS_GLOW_BLUR           = 8.0f;
constexpr u8               K_FOCUS_GLOW_ALPHA          = 60;
constexpr float            K_ACTIVE_LABEL_TINT         = 0.6f;
constexpr float            K_CHEVRON_MARGIN            = 12.0f;
constexpr Vec2             K_CHEVRON_SIZE{9.0f, 5.0f};
constexpr float            K_EDIT_HEADER_ICON_SIZE         = 28.0f;
constexpr float            K_EDIT_HEADER_ICON_RADIUS       = 7.0f;
constexpr float            K_EDIT_HEADER_GAP               = 10.0f;
constexpr float            K_EDIT_HEADER_LINE_GAP          = 2.0f;
constexpr float            K_EDIT_HEADER_PADDING           = 20.0f;
constexpr float            K_EDIT_HEADER_ICON_PADDING      = 16.0f;
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
constexpr float            K_TILE_LABEL_INSET              = 4.0f;
constexpr float            K_TILE_RADIUS                   = 8.0f;
constexpr float            K_TILE_SELECTED_TINT            = 0.14f;
constexpr u8               K_TILE_UNSELECTED_ICON_ALPHA    = 140;
constexpr float            K_TILES_TOP_GAP                 = 8.0f;
constexpr float            K_TILES_CLIP_BLEED              = 4.0f;
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
constexpr u8    K_SHEEN_LIGHTEN           = 80;
constexpr u32   K_MAX_MESSAGE_LINES       = 3;

constexpr Color K_COLOR_ON_ART{255, 255, 255, 255};
constexpr Color K_COLOR_TOP_HIGHLIGHT{255, 255, 255, 22};
constexpr float K_REGION_CHIP_GAP = 8.0f;

constexpr EnumArray<EditField, std::string_view> K_FIELD_LABELS{{"Note", "Username", "Password"}};

[[nodiscard]] auto scaled_about(Rect t_rect, Vec2 t_origin, float t_scale) -> Rect
{
	const float x = t_origin.x + (t_rect.x - t_origin.x) * t_scale;
	const float y = t_origin.y + (t_rect.y - t_origin.y) * t_scale;

	return Rect{x, y, t_rect.w * t_scale, t_rect.h * t_scale};
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

[[nodiscard]] auto list_header_height(const Fonts* t_fonts) -> float
{
	return K_HEADER_PADDING_TOP + std::max(t_fonts->title.line_height(), K_CONTROL_HEIGHT) + K_HEADER_PADDING_BOTTOM;
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

[[nodiscard]] auto row_button_size(const Fonts* t_fonts) -> float
{
	return std::max(28.0f, t_fonts->secondary.line_height() + 8.0f);
}

[[nodiscard]] auto vertically_centered(Rect t_strip, float t_x, float t_width, float t_height) -> Rect
{
	const float y = t_strip.y + (t_strip.h - t_height) * 0.5f;

	return Rect{t_x, y, t_width, t_height};
}

[[nodiscard]] auto row_highlight(Rect t_row) -> Rect
{
	return Rect{t_row.x - K_ROW_BLEED, t_row.y + K_ROW_HIGHLIGHT_INSET, t_row.w + K_ROW_BLEED * 2.0f, t_row.h - K_ROW_HIGHLIGHT_INSET * 2.0f};
}

[[nodiscard]] auto chevron_in(Rect t_box) -> Rect
{
	const float x = t_box.right() - K_CHEVRON_MARGIN - K_CHEVRON_SIZE.x;
	const float y = t_box.center().y - K_CHEVRON_SIZE.y * 0.5f;

	return Rect{x, y, K_CHEVRON_SIZE.x, K_CHEVRON_SIZE.y};
}

[[nodiscard]] auto hover_target(bool t_lit) -> float
{
	return t_lit ? 1.0f : 0.0f;
}

auto draw_input_box(DrawList* t_draw_list, Rect t_rect, Color t_border, bool t_focused, Color t_accent, u8 t_alpha) -> void
{
	if (t_focused) {
		const Color glow = with_alpha(t_accent, static_cast<u8>(K_FOCUS_GLOW_ALPHA * t_alpha / 255));
		t_draw_list->add_shadow(t_rect, K_INPUT_RADIUS, K_FOCUS_GLOW_BLUR, glow);
	}

	const float border_width = t_focused ? K_INPUT_FOCUS_BORDER : 1.0f;
	t_draw_list->add_bordered_rect(t_rect, rounded(K_INPUT_RADIUS), faded(g_theme.field, t_alpha), faded(t_border, t_alpha), border_width);
}

// The popup is glass, which shows whatever is under it, so its shadow is drawn in strips around it and never beneath it.
auto draw_shadow_around(DrawList* t_draw_list, Rect t_panel, float t_amount) -> void
{
	const float wide = t_panel.w + K_SHADOW_REACH * 2.0f;
	const float left = t_panel.x - K_SHADOW_REACH;
	const Rect  strips[]{
		Rect{left, t_panel.y - K_SHADOW_REACH, wide, K_SHADOW_REACH},
		Rect{left, t_panel.bottom(), wide, K_SHADOW_REACH},
		Rect{left, t_panel.y, K_SHADOW_REACH, t_panel.h},
		Rect{t_panel.right(), t_panel.y, K_SHADOW_REACH, t_panel.h},
	};

	for (const Rect strip : strips) {
		t_draw_list->push_clip(strip);
		controls::draw_panel_shadow(t_draw_list, t_panel, K_PANEL_RADIUS, t_amount);
		t_draw_list->pop_clip();
	}
}

auto draw_game_icon(DrawList* t_draw_list, const Game& t_game, Rect t_icon, float t_radius, u8 t_alpha) -> void
{
	if (t_game.icon != nullptr) {
		t_draw_list->add_image(t_icon, t_game.icon, faded(K_COLOR_ON_ART, t_alpha), rounded(t_radius));
	} else {
		t_draw_list->add_rounded_rect(t_icon, rounded(t_radius), faded(t_game.accent, t_alpha));
	}
}
}

AccountModal::AccountModal(Library* t_library, Settings* t_settings, const Fonts* t_fonts, const Assets* t_assets, const os::Window* t_window, Toasts* t_toasts, LoginSession* t_session, CommandQueue* t_commands)
	: m_library(t_library)
	, m_settings(t_settings)
	, m_fonts(t_fonts)
	, m_assets(t_assets)
	, m_window(t_window)
	, m_toasts(t_toasts)
	, m_session(t_session)
	, m_commands(t_commands)
	, m_list(t_library, t_session, t_toasts, CommandType::UNDO_DELETE)
	, m_region_list(t_fonts, t_assets, t_settings, ListPopupOptions{.empty_message = "No regions"})
{
	m_search.set_max_length(K_SEARCH_MAX_LENGTH);
	m_search.m_placeholder = "Search";
}

auto AccountModal::floating_panel_size() const -> Vec2
{
	const float size_scale = panel_size_scale(m_fonts);
	const Vec2  largest{K_PANEL_MAX_SIZE.x * size_scale, K_PANEL_MAX_SIZE.y * size_scale};
	const Vec2  window = m_window->size();
	const float room_x = std::max(0.0f, window.x - K_PANEL_MARGIN * 2.0f);
	const float room_y = std::max(0.0f, window.y - K_PANEL_MARGIN * 2.0f);

	float width  = std::min({window.x * K_PANEL_WIDTH_FRACTION, largest.x, room_x});
	float height = std::min({window.y * K_PANEL_HEIGHT_FRACTION, largest.y, room_y});

	const float aspect = largest.x / largest.y;
	if (width / height > aspect) {
		width = height * aspect;
	} else {
		height = width / aspect;
	}

	const float form_height = std::min(expanded_form_panel_height(width), room_y);

	return Vec2{width, std::max(height, form_height)};
}

auto AccountModal::expanded_form_panel_height(float t_panel_width) const -> float
{
	const float      content_width = t_panel_width - K_PANEL_BORDER * 2.0f;
	const float      main_width    = std::max(0.0f, content_width - art_column_width(content_width) - K_ART_SEPARATOR_WIDTH);
	const Rect       measured{0.0f, 0.0f, main_width, 0.0f};
	const FormLayout form        = form_layout(measured);
	const float      form_height = form.tiles.bottom() + m_form_scroll.offset() - form.area.y + K_FORM_TOP_PADDING;
	const float      chrome      = K_PANEL_BORDER * 2.0f + footer_height(m_fonts) + edit_header_height();

	return std::ceil(chrome + form_height) + 1.0f;
}

auto AccountModal::is_docked() const -> bool
{
	const float size_scale = panel_size_scale(m_fonts);
	const Vec2  floating   = floating_panel_size();

	return floating.x < K_PANEL_MIN_SIZE.x * size_scale || floating.y < K_PANEL_MIN_SIZE.y * size_scale;
}

auto AccountModal::panel_rect() const -> Rect
{
	const Vec2 window = m_window->size();
	if (is_docked()) return content_rect(window).inset(0.0f, 1.0f);

	const Vec2 size = floating_panel_size();

	return Rect{0.0f, 0.0f, window.x, window.y}.centered(size.x, size.y);
}

auto AccountModal::layout() const -> AccountModal::Layout
{
	Layout layout{};
	layout.panel      = panel_rect();
	layout.inner      = layout.panel.inset(K_PANEL_BORDER);
	layout.art_column = Rect{layout.inner.x, layout.inner.y, art_column_width(layout.inner.w), layout.inner.h};

	const float sheet_x = layout.art_column.right() + K_ART_SEPARATOR_WIDTH;
	layout.sheet        = Rect{sheet_x, layout.inner.y, layout.inner.right() - sheet_x, layout.inner.h};

	const float footer = m_mode == Mode::ACCOUNT_LIST ? 0.0f : footer_height(m_fonts);
	Rect        main   = layout.sheet;
	layout.footer      = main.split_bottom(footer);
	layout.main_column = main;

	return layout;
}

auto AccountModal::back_badge_rect(const Layout& t_layout) const -> Rect
{
	const Rect& art   = t_layout.art_column;
	const float width = K_BACK_PADDING_X * 2.0f + K_BACK_ICON_SIZE + K_BACK_ICON_GAP + text_width(m_fonts->secondary, K_BACK_LABEL);

	return Rect{art.x + K_CLOSE_BADGE_MARGIN, art.y + K_CLOSE_BADGE_MARGIN, std::ceil(width), K_BACK_HEIGHT};
}

auto AccountModal::list_layout(const Layout& t_layout) const -> AccountModal::ListLayout
{
	const Rect  main   = t_layout.main_column;
	const float header = list_header_height(m_fonts);

	ListLayout list{};
	list.area       = Rect{main.x, main.y + header, main.w, main.h - header};
	list.row_height = row_height(m_fonts);
	list.accounts   = m_list.rows();

	const float track_x = list.area.right() - K_SCROLLBAR_WIDTH - K_SCROLLBAR_MARGIN;
	const Rect  track{track_x, list.area.y, K_SCROLLBAR_WIDTH, list.area.h - K_SCROLLBAR_MARGIN};
	const float content_height = static_cast<float>(list.accounts.count) * list.row_height + K_LIST_END_PADDING;
	list.scroll                = ScrollGeometry{track, content_height, list.area.h};

	return list;
}

auto AccountModal::list_to_screen(const ListLayout& t_list, float t_list_y) const -> float
{
	return t_list.area.y + t_list_y - m_rows_scroll.offset();
}

auto AccountModal::pointer_in_list(const ListLayout& t_list, float t_y) const -> float
{
	return t_y - list_to_screen(t_list, 0.0f);
}

auto AccountModal::row_rect_at(const Layout& t_layout, const ListLayout& t_list, float t_top_in_list) const -> Rect
{
	const Rect  main = t_layout.main_column;
	const float y    = list_to_screen(t_list, t_top_in_list);

	return Rect{main.x + K_ROW_PADDING, y, main.w - K_ROW_PADDING * 2.0f, t_list.row_height};
}

auto AccountModal::row_rect(const Layout& t_layout, const ListLayout& t_list, u32 t_row) const -> Rect
{
	const float top = static_cast<float>(t_row) * t_list.row_height + m_list.m_offsets[t_row];

	return row_rect_at(t_layout, t_list, top);
}

auto AccountModal::row_at(const Layout& t_layout, const ListLayout& t_list, Vec2 t_point) const -> std::optional<u32>
{
	if (!t_list.area.contains(t_point)) return std::nullopt;

	for (const u32 row : std::views::iota(0u, t_list.accounts.count)) {
		const Rect rect = row_rect(t_layout, t_list, row);
		if (rect.overlaps_vertically(t_list.area) && rect.contains(t_point)) return row;
	}

	return std::nullopt;
}

auto AccountModal::row_account(std::optional<u32> t_row) const -> std::optional<AccountRef>
{
	const VisibleAccounts shown = m_list.rows();
	if (!t_row || *t_row >= shown.count) return std::nullopt;

	return shown.refs[*t_row];
}

auto AccountModal::login_button_rect(Rect t_row) const -> Rect
{
	const float width = std::ceil(text_width(m_fonts->secondary, K_LOGIN_LABEL) + K_LOGIN_PADDING_X * 2.0f);

	return vertically_centered(t_row, t_row.right() - width, width, row_button_size(m_fonts));
}

auto AccountModal::favorite_button_rect(Rect t_row) const -> Rect
{
	const float size  = row_button_size(m_fonts);
	const Rect  login = login_button_rect(t_row);

	return vertically_centered(t_row, login.x - K_ROW_BUTTON_GAP - size, size, size);
}

auto AccountModal::is_row_button_hit(Rect t_row, Vec2 t_point) const -> bool
{
	return login_button_rect(t_row).contains(t_point) || favorite_button_rect(t_row).contains(t_point);
}

auto AccountModal::list_header(Rect t_main) const -> Rect
{
	const float height = std::max(m_fonts->title.line_height(), K_CONTROL_HEIGHT);
	const float width  = std::max(0.0f, t_main.w - K_ROW_PADDING * 2.0f);

	return Rect{t_main.x + K_ROW_PADDING, t_main.y + K_HEADER_PADDING_TOP, width, height};
}

auto AccountModal::add_button_rect(Rect t_main) const -> Rect
{
	const Rect  header = list_header(t_main);
	const float width  = K_ADD_PADDING_X * 2.0f + K_ADD_ICON_SIZE + K_ADD_ICON_GAP + text_width(m_fonts->secondary, K_ADD_LABEL);
	const float x      = snapped_to_pixel(header.right() - width);
	const float y      = snapped_to_pixel(header.center().y - K_CONTROL_HEIGHT * 0.5f);

	return Rect{x, y, width, K_CONTROL_HEIGHT};
}

auto AccountModal::search_rect(Rect t_main) const -> Rect
{
	const Rect  header      = list_header(t_main);
	const Rect  add         = add_button_rect(t_main);
	const float right       = add.x - K_CONTROL_GAP;
	const float title_width = text_width(m_fonts->title, K_ACCOUNTS_TITLE);
	const float width       = std::floor(std::min(K_SEARCH_MAX_WIDTH, right - header.x - title_width - K_TITLE_GAP));
	if (width < K_SEARCH_MIN_WIDTH) return Rect{};

	return Rect{right - width, add.y, width, K_CONTROL_HEIGHT};
}

auto AccountModal::is_search_visible() const -> bool
{
	return search_rect(layout().main_column).w > 0.0f;
}

auto AccountModal::empty_zone(const Layout& t_layout, Rect t_list_area) const -> Rect
{
	const Rect  main   = t_layout.main_column;
	const float left   = snapped_to_pixel(main.x + K_ROW_PADDING - K_ROW_BLEED);
	const float right  = snapped_to_pixel(main.right() - K_ROW_PADDING + K_ROW_BLEED);
	const float top    = snapped_to_pixel(t_list_area.y + K_ZONE_TOP_INSET);
	const float bottom = snapped_to_pixel(t_list_area.bottom() - (K_ROW_PADDING - K_ROW_BLEED));

	return Rect{left, top, right - left, std::max(0.0f, bottom - top)};
}

auto AccountModal::lifted_top(const ListLayout& t_list) const -> float
{
	return m_list.lifted_top(pointer_in_list(t_list, m_mouse.y), t_list.row_height);
}

auto AccountModal::raised_top(const ListLayout& t_list, u32 t_row) const -> float
{
	const float offset = m_list.m_offsets[t_row];
	if (!m_list.m_drag.lifted) return static_cast<float>(t_row) * t_list.row_height + offset;

	return lifted_top(t_list) + m_list.m_drag.rows_from_held(t_row) * t_list.row_height + offset;
}

auto AccountModal::can_select() const -> bool
{
	return m_mode == Mode::ACCOUNT_LIST && m_list.shows_every_account();
}

auto AccountModal::primary_button_rect(Rect t_footer) const -> Rect
{
	std::string_view label;
	if (m_mode == Mode::EDIT_ACCOUNT) {
		label = save_label();
	} else if (asks_for_permission()) {
		label = K_PERMISSION_BUTTON_LABEL;
	}

	const float width = std::max(K_ACTION_BUTTON_WIDTH, text_width(m_fonts->body, label) + K_CANCEL_PADDING);
	const float x     = t_footer.right() - K_ROW_PADDING - width;

	return vertically_centered(t_footer, x, width, action_button_height(m_fonts));
}

auto AccountModal::cancel_button_rect(Rect t_primary) const -> Rect
{
	const float width = text_width(m_fonts->body, "Cancel") + K_CANCEL_PADDING;

	return Rect{t_primary.x - K_ACTION_BUTTON_GAP - width, t_primary.y, width, t_primary.h};
}

auto AccountModal::asks_for_permission() const -> bool
{
	return m_mode == Mode::LOGIN_PROGRESS && m_session->asks_for_permission();
}

auto AccountModal::save_label() const -> std::string_view
{
	return m_form.m_account ? "Save changes" : "Add account";
}

auto AccountModal::edit_header_height() const -> float
{
	const float lines      = m_fonts->body.line_height() + K_EDIT_HEADER_LINE_GAP + m_fonts->secondary.line_height() + K_EDIT_HEADER_PADDING;
	const float icon_block = K_EDIT_HEADER_ICON_SIZE + K_EDIT_HEADER_ICON_PADDING;

	return std::max({header_height(m_fonts), lines, icon_block});
}

auto AccountModal::form_area(Rect t_main) const -> Rect
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
	const float label_height = m_fonts->secondary.line_height();
	const float input_height = field_input_height(m_fonts);
	const float cell_height  = label_height + K_FORM_LABEL_GAP + input_height + label_height + K_FORM_HINT_GAP;
	const float left         = t_main.x + K_ROW_PADDING;
	const float width        = std::max(0.0f, t_main.w - K_ROW_PADDING * 2.0f);
	const bool  two_columns  = width >= K_FORM_TWO_COLUMN_MIN_WIDTH;
	const float column_width = two_columns ? (width - K_FORM_COLUMN_GAP) * 0.5f : width;

	FormLayout form{};
	form.area = form_area(t_main);

	float y = form.area.y + K_FORM_TOP_PADDING - m_form_scroll.offset();

	const auto place = [&](Rect* t_label, Rect* t_input, u32 t_slot) {
		const u32   column = two_columns ? t_slot % 2 : 0;
		const u32   line   = two_columns ? t_slot / 2 : t_slot;
		const float x      = left + static_cast<float>(column) * (column_width + K_FORM_COLUMN_GAP);
		const float top    = y + static_cast<float>(line) * cell_height;

		*t_label = Rect{x, top, column_width, label_height};
		*t_input = Rect{x, top + label_height + K_FORM_LABEL_GAP, column_width, input_height};
	};

	place(&form.field_labels[EditField::USERNAME], &form.field_inputs[EditField::USERNAME], 0);
	place(&form.field_labels[EditField::PASSWORD], &form.field_inputs[EditField::PASSWORD], 1);
	place(&form.region_label, &form.region_input, 2);
	place(&form.field_labels[EditField::NOTE], &form.field_inputs[EditField::NOTE], 3);

	const u32 cell_lines = two_columns ? 2 : 4;
	y += static_cast<float>(cell_lines) * cell_height;

	form.show_in_label = Rect{left, y, width, label_height};
	form.show_in_input = Rect{left, y + label_height + K_FORM_LABEL_GAP, width, input_height};
	form.tile_columns  = show_in_columns(width);

	const u32   tile_rows    = (m_library->game_count + form.tile_columns - 1) / form.tile_columns;
	const u32   tile_gaps    = tile_rows > 0 ? tile_rows - 1 : 0;
	const float tiles_height = static_cast<float>(tile_rows) * show_in_tile_height() + static_cast<float>(tile_gaps) * K_TILE_GAP;
	form.tiles               = Rect{left, form.show_in_input.bottom() + K_TILES_TOP_GAP, width, tiles_height};

	const float tiles_shown = (K_TILES_TOP_GAP + tiles_height) * m_show_in_amount;
	form.content_height     = form.show_in_input.bottom() + tiles_shown + m_form_scroll.offset() - form.area.y + K_FORM_TOP_PADDING;

	const Rect  password = form.field_inputs[EditField::PASSWORD];
	const float reveal_x = password.right() - K_REVEAL_BUTTON_SIZE - K_REVEAL_BUTTON_MARGIN;
	form.reveal          = vertically_centered(password, reveal_x, K_REVEAL_BUTTON_SIZE, K_REVEAL_BUTTON_SIZE);

	return form;
}

auto AccountModal::form_scroll(const FormLayout& t_form) const -> ScrollGeometry
{
	const float track_x = t_form.area.right() - K_SCROLLBAR_WIDTH - K_SCROLLBAR_MARGIN;
	const Rect  track{track_x, t_form.area.y, K_SCROLLBAR_WIDTH, t_form.area.h};

	return ScrollGeometry{track, t_form.content_height, t_form.area.h};
}

auto AccountModal::field_text_rect(const FormLayout& t_form, EditField t_field) const -> Rect
{
	Rect text = t_form.field_inputs[t_field].inset(K_INPUT_PADDING_X, 0.0f);

	if (t_field == EditField::PASSWORD) {
		text.w -= K_REVEAL_BUTTON_SIZE;
	}

	return text;
}

auto AccountModal::show_in_tile(const FormLayout& t_form, u32 t_game) const -> Rect
{
	const u32   columns = std::max(1u, t_form.tile_columns);
	const float gaps    = K_TILE_GAP * static_cast<float>(columns - 1);
	const float width   = (t_form.tiles.w - gaps) / static_cast<float>(columns);
	const float height  = show_in_tile_height();
	const auto  column  = static_cast<float>(t_game % columns);
	const auto  line    = static_cast<float>(t_game / columns);
	const float x       = snapped_to_pixel(t_form.tiles.x + column * (width + K_TILE_GAP));
	const float y       = snapped_to_pixel(t_form.tiles.y + line * (height + K_TILE_GAP));

	return Rect{x, y, std::floor(width), height};
}

auto AccountModal::show_in_tile_at(const FormLayout& t_form, Vec2 t_point) const -> std::optional<u32>
{
	if (m_show_in_amount < 0.5f || !t_form.area.contains(t_point)) return std::nullopt;

	for (const u32 game : std::views::iota(0u, m_library->game_count)) {
		if (show_in_tile(t_form, game).contains(t_point)) return game;
	}

	return std::nullopt;
}

auto AccountModal::is_in_form(const FormLayout& t_form, Rect t_part, Vec2 t_point) const -> bool
{
	return t_form.area.contains(t_point) && t_part.contains(t_point);
}

auto AccountModal::field_at(const FormLayout& t_form, Vec2 t_point) const -> std::optional<EditField>
{
	if (!t_form.area.contains(t_point) || t_form.reveal.contains(t_point)) return std::nullopt;

	for (const EditField field : each<EditField>()) {
		if (t_form.field_inputs[field].contains(t_point)) return field;
	}

	return std::nullopt;
}

auto AccountModal::open(u32 t_game) -> void
{
	m_open = true;
	m_art_source.reset();
	m_list.m_game = t_game;
	m_mode        = Mode::ACCOUNT_LIST;
	m_rows_scroll = Scrollable{};
	m_zone_hover  = 0.0f;
	m_search.set_focused(false);
	clear_search();
	m_list.reset_motion();
	m_list.m_selection.clear();
}

auto AccountModal::detached_game() const -> std::optional<u32>
{
	return m_art_source && m_morph_progress > 0.0f ? m_list.m_game : std::nullopt;
}

auto AccountModal::close() -> void
{
	m_open = false;
	m_region_list.close();
	m_search.set_focused(false);
	cancel_row_drag();
	m_list.m_selection.clear();
}

auto AccountModal::quick_login(u32 t_game, AccountRef t_account) -> void
{
	open(t_game);
	request_login(t_game, t_account);
}

auto AccountModal::edit_account(AccountRef t_account) -> void
{
	open(t_account.game);
	start_form(t_account);
}

auto AccountModal::account_at_row(std::optional<u32> t_row) const -> const Account*
{
	const std::optional<AccountRef> account = row_account(t_row);

	return account ? m_library->account(*account) : nullptr;
}

auto AccountModal::edit_row(std::optional<u32> t_row) -> void
{
	if (const std::optional<AccountRef> account = row_account(t_row)) {
		start_form(account);
	}
}

auto AccountModal::delete_row(std::optional<u32> t_row) -> void
{
	if (const std::optional<AccountRef> account = row_account(t_row)) {
		const AccountRef target = *account;
		delete_accounts(std::span{&target, 1});
	}
}

auto AccountModal::delete_picked() -> void
{
	delete_accounts(m_list.take_picked());
}

auto AccountModal::delete_accounts(std::span<const AccountRef> t_accounts) -> void
{
	if (t_accounts.empty()) return;

	for (const AccountRef removed : m_list.delete_accounts(t_accounts)) {
		shift_after_removal(&m_form.m_account, removed);
	}

	m_list.reset_motion();
}

auto AccountModal::undo_delete() -> void
{
	const std::span<const AccountRef> restored = m_list.undo_delete();

	for (const AccountRef back : restored) {
		shift_after_insert(&m_form.m_account, back);
	}

	if (restored.empty()) return;

	m_list.reset_motion();

	if (restored.size() > 1) {
		m_list.select_accounts(restored);
	}

	reveal_account(restored.front());
}

auto AccountModal::forget_secrets() -> void
{
	m_session->cancel();
	m_list.forget_secrets();
	m_form.clear();
	m_search.set_value("");
	m_search.set_focused(false);

	m_show_in_open = false;
	m_mode         = Mode::ACCOUNT_LIST;
	m_open         = false;
	m_open_amount  = 0.0f;
	m_list.m_game.reset();
}

auto AccountModal::start_form(std::optional<AccountRef> t_account) -> void
{
	if (!m_list.has_game()) return;

	m_mode        = Mode::EDIT_ACCOUNT;
	m_form_scroll = Scrollable{};
	m_search.set_focused(false);
	m_list.m_selection.clear();
	m_region_list.close();
	m_form.start(*m_library, t_account, *m_list.m_game);

	m_show_in_open   = false;
	m_show_in_amount = 0.0f;
}

auto AccountModal::save_form() -> void
{
	if (!m_list.has_game() || !m_form.has_changes(*m_library)) return;

	const bool                                   adding = !m_form.m_account;
	const std::expected<AccountRef, SaveProblem> saved  = m_form.save(m_library, *m_list.m_game);
	if (!saved && saved.error() == SaveProblem::MISSING_FIELDS) return;

	m_mode = Mode::ACCOUNT_LIST;

	if (!saved) {
		m_toasts->notify(Notification{.message = "This game can't hold any more accounts."});
		return;
	}

	if (adding) {
		m_list.reset_motion();
		reveal_account(*saved);
	}
}

auto AccountModal::open_region_list() -> void
{
	m_form.focus(std::nullopt);
	m_region_list.open(K_REGION_LABELS, region_index(m_form.m_region));
}

auto AccountModal::reveal_field(EditField t_field) -> void
{
	const FormLayout form  = form_layout(layout().main_column);
	const Rect       input = form.field_inputs[t_field];

	m_form_scroll.reveal(input.y, input.bottom(), form.area.y, form.area.bottom(), form_scroll(form));
}

auto AccountModal::request_login(u32 t_game, AccountRef t_account) -> void
{
	m_mode = Mode::LOGIN_PROGRESS;
	m_search.set_focused(false);
	m_session->request(t_game, t_account);
}

auto AccountModal::toggle_favorite(AccountRef t_account) -> void
{
	m_list.toggle_favorite(t_account, row_height(m_fonts));
}

auto AccountModal::refresh_search() -> void
{
	if (!m_list.set_query(m_search.value())) return;

	m_list.reset_motion();

	if (!m_search.value().empty()) {
		m_rows_scroll.jump_to(0.0f, list_layout(layout()).scroll);
	}
}

auto AccountModal::clear_search() -> void
{
	m_search.set_value("");
	refresh_search();
}

auto AccountModal::reveal_account(AccountRef t_account) -> void
{
	const Layout             layout = this->layout();
	const ListLayout         list   = list_layout(layout);
	const std::optional<u32> row    = list.accounts.row_of(t_account);
	if (!row) return;

	const Rect rect = row_rect(layout, list, *row);
	m_rows_scroll.reveal(rect.y, rect.bottom(), list.area.y, list.area.bottom(), list.scroll);
}

auto AccountModal::lift_row(const ListLayout& t_list, u32 t_row, Vec2 t_point) -> void
{
	m_list.lift(t_list.accounts, t_row, pointer_in_list(t_list, t_point.y), t_list.row_height);
	m_search.set_focused(false);
}

auto AccountModal::drop_row() -> void
{
	const ListLayout list = list_layout(layout());
	m_list.drop(lifted_top(list), list.row_height);
}

auto AccountModal::cancel_row_drag() -> void
{
	const ListLayout list = list_layout(layout());
	m_list.cancel_drag(lifted_top(list), list.row_height);
}

auto AccountModal::scroll_near_edges(const ListLayout& t_list, float t_delta_seconds) -> void
{
	const float zone  = t_list.row_height * K_AUTO_SCROLL_ZONE;
	const float above = t_list.area.y + zone - m_mouse.y;
	const float below = m_mouse.y - (t_list.area.bottom() - zone);

	if (above > 0.0f) {
		const float speed = K_AUTO_SCROLL_SPEED * std::min(above / zone, 1.0f);
		m_rows_scroll.scroll_by(-speed * t_delta_seconds, t_list.scroll);
	} else if (below > 0.0f) {
		const float speed = K_AUTO_SCROLL_SPEED * std::min(below / zone, 1.0f);
		m_rows_scroll.scroll_by(speed * t_delta_seconds, t_list.scroll);
	}
}

auto AccountModal::update_list(float t_delta_seconds) -> void
{
	m_search.update(t_delta_seconds);

	if (m_list.has_game()) {
		refresh_search();
	}

	const Layout     layout = this->layout();
	const ListLayout list   = list_layout(layout);

	m_list.m_selection.clear_if_rows_changed(list.accounts);
	if (!can_select()) {
		m_list.m_selection.clear();
	}

	m_rows_scroll.update(t_delta_seconds);
	update_marquee(list, t_delta_seconds);

	if (m_list.m_drag.lifted) {
		scroll_near_edges(list, t_delta_seconds);
	}

	m_list.update_motion(list.accounts.count, lifted_top(list), list.row_height, t_delta_seconds);

	const bool               live    = m_open && !m_list.m_drag.lifted && !m_rows_scroll.m_dragging && !m_list.m_selection.m_box;
	const std::optional<u32> hovered = live ? row_at(layout, list, m_mouse) : std::nullopt;
	m_list.ease_hover(hovered, list.accounts.count, t_delta_seconds);

	const bool empty    = live && m_list.has_game() && list.accounts.count == 0 && m_search.value().empty();
	const bool zone_lit = empty && empty_zone(layout, list.area).contains(m_mouse);
	m_zone_hover        = animation::ease_toward(m_zone_hover, hover_target(zone_lit), K_HOVER_EASE_RATE, t_delta_seconds);
}

// The box reaches over more rows as the list scrolls under it, so it follows the pointer every frame and not only when it moves.
auto AccountModal::update_marquee(const ListLayout& t_list, float t_delta_seconds) -> void
{
	RowSelection* selection = &m_list.m_selection;
	if (!selection->m_box) return;

	if (!can_select()) {
		selection->cancel_box();
		return;
	}

	if (selection->box_shown()) {
		scroll_near_edges(t_list, t_delta_seconds);
		animation::request_frame();
	}

	selection->drag_box(m_mouse, list_to_screen(t_list, 0.0f), t_list.row_height, t_list.accounts.count);
}

auto AccountModal::request_tooltip() -> void
{
	if (!is_blocking() || !m_list.has_game()) return;

	const Layout layout = this->layout();

	if (m_mode == Mode::EDIT_ACCOUNT) {
		const FormLayout form = form_layout(layout.main_column);

		if (is_in_form(form, form.reveal, m_mouse)) {
			const bool masked = m_form.m_fields[EditField::PASSWORD].m_masked;
			m_tooltip.request(masked ? "Show password" : "Hide password", form.reveal);
		}

		return;
	}

	if (m_mode == Mode::ACCOUNT_LIST) {
		request_row_tooltip(layout);
	}
}

auto AccountModal::request_row_tooltip(const Layout& t_layout) -> void
{
	if (m_list.m_drag.lifted || m_list.m_drag.pressed_row || m_list.m_selection.m_box) return;

	const Rect add = add_button_rect(t_layout.main_column);
	if (add.contains(m_mouse)) {
		m_tooltip.request("Add account", add);
		return;
	}

	const ListLayout         list        = list_layout(t_layout);
	const std::optional<u32> hovered_row = row_at(t_layout, list, m_mouse);
	if (!hovered_row) return;

	const Rect favorite = favorite_button_rect(row_rect(t_layout, list, *hovered_row));
	if (!favorite.contains(m_mouse)) return;

	const bool pinned = m_library->account(list.accounts.refs[*hovered_row])->favorite;
	m_tooltip.request(pinned ? "Unpin" : "Pin to top", favorite);
}

auto AccountModal::update(float t_delta_seconds) -> void
{
	const bool  morphing     = m_art_source && m_list.has_game();
	const float morph_target = m_open ? 1.0f : 0.0f;

	if (!morphing) {
		m_morph_progress = morph_target;
	} else if (m_open || m_open_amount <= K_MORPH_RETURN_OPEN_AMOUNT) {
		m_morph_progress = animation::step_toward(m_morph_progress, morph_target, K_MORPH_SECONDS, t_delta_seconds);
	}

	const bool  panel_shown    = m_open && (!morphing || m_morph_progress >= K_PANEL_REVEAL_PROGRESS);
	const float open_target    = panel_shown ? 1.0f : 0.0f;
	const float scale_travel   = m_window->size().x * 0.5f * (1.0f - K_PANEL_CLOSED_SCALE);
	const float settled_amount = animation::K_SETTLED_PIXELS / scale_travel;
	m_open_amount              = animation::ease_toward(m_open_amount, open_target, K_OPEN_EASE_RATE, t_delta_seconds, settled_amount);

	if (!m_open && m_open_amount == 0.0f && m_morph_progress == 0.0f) {
		m_list.m_game.reset();
	}

	m_list.forget_deleted_without_undo();

	switch (m_mode) {
		using enum Mode;

		case ACCOUNT_LIST: {
			update_list(t_delta_seconds);
			break;
		}

		case LOGIN_PROGRESS: {
			animation::request_frame();
			break;
		}

		case EDIT_ACCOUNT: {
			m_form_scroll.update(t_delta_seconds);
			m_form.update(t_delta_seconds);
			break;
		}
	}

	const float show_in_before = m_show_in_amount;
	const float show_in_target = m_show_in_open ? 1.0f : 0.0f;
	m_show_in_amount           = animation::ease_toward(m_show_in_amount, show_in_target, K_SHOW_IN_EASE_RATE, t_delta_seconds);

	const Layout     layout = this->layout();
	const FormLayout form   = form_layout(layout.main_column);

	if (m_mode == Mode::EDIT_ACCOUNT && m_show_in_open && m_show_in_amount != show_in_before) {
		const float tiles_bottom = form.tiles.y + form.tiles.h * m_show_in_amount;
		m_form_scroll.reveal(form.show_in_label.y, tiles_bottom, form.area.y, form.area.bottom(), form_scroll(form));
	}

	m_region_list.update(t_delta_seconds, form.region_input, layout.inner);
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

	const Layout layout = this->layout();

	if (m_mode == Mode::EDIT_ACCOUNT && m_region_list.m_open) {
		m_region_list.on_pointer_down(t_point);
		return true;
	}

	if (m_mode == Mode::EDIT_ACCOUNT) {
		const FormLayout form = form_layout(layout.main_column);
		if (m_form_scroll.on_pointer_down(t_point, form_scroll(form))) return true;

		if (const std::optional<EditField> pressed = field_at(form, t_point)) {
			m_form.focus(pressed);
			m_form.m_fields[*pressed].on_pointer_down(m_fonts->body, field_text_rect(form, *pressed), t_point.x);
		}
	} else if (m_mode == Mode::ACCOUNT_LIST && m_list.has_game()) {
		press_in_list(layout, t_point);
	}

	return true;
}

auto AccountModal::press_in_list(const Layout& t_layout, Vec2 t_point) -> void
{
	const ListLayout list = list_layout(t_layout);
	if (m_rows_scroll.on_pointer_down(t_point, list.scroll)) return;

	const Rect search     = search_rect(t_layout.main_column);
	const bool over_clear = !m_search.value().empty() && controls::search_clear_rect(search).contains(t_point);

	if (search.contains(t_point) && !over_clear) {
		const Rect text_area = controls::search_text_rect(search, K_SEARCH_INSET);

		m_search.set_focused(true);
		m_search.on_pointer_down(m_fonts->secondary, text_area, t_point.x);
		return;
	}

	if (!over_clear) {
		m_search.set_focused(false);
	}

	const std::optional<u32> row = row_at(t_layout, list, t_point);

	if (!row) {
		if (can_select() && list.accounts.count > 0 && list.area.contains(t_point)) {
			const os::Modifiers held     = os::modifiers();
			const bool          additive = held.shortcut || held.shift;
			m_list.m_selection.begin_box(t_point, list_to_screen(list, 0.0f), additive);
		}

		return;
	}

	const bool on_button = is_row_button_hit(row_rect(t_layout, list, *row), t_point);
	if (on_button || (can_select() && m_list.select_with_modifiers(*row))) return;

	m_list.m_drag.pressed_row = *row;
	m_list.m_drag.press_point = t_point;
}

auto AccountModal::on_pointer_move(Vec2 t_point) -> bool
{
	if (!is_blocking()) return false;

	if (m_region_list.m_open) {
		m_region_list.on_pointer_move(t_point);
		return true;
	}

	const Layout layout = this->layout();
	const Rect   main   = layout.main_column;

	if (m_form.is_selecting_text()) {
		const FormLayout form = form_layout(main);

		for (const EditField field : each<EditField>()) {
			TextInput* input = &m_form.m_fields[field];

			if (input->m_selecting) {
				input->on_pointer_move(m_fonts->body, field_text_rect(form, field), t_point.x);
			}
		}
	}

	if (m_search.m_selecting) {
		const Rect text_area = controls::search_text_rect(search_rect(main), K_SEARCH_INSET);
		m_search.on_pointer_move(m_fonts->secondary, text_area, t_point.x);
	}

	if (m_rows_scroll.m_dragging) {
		m_rows_scroll.on_pointer_move(t_point.y, list_layout(layout).scroll);
	}

	if (m_form_scroll.m_dragging) {
		const FormLayout form = form_layout(main);
		m_form_scroll.on_pointer_move(t_point.y, form_scroll(form));
	}

	if (m_list.m_selection.m_box) {
		const ListLayout list = list_layout(layout);
		m_list.m_selection.drag_box(t_point, list_to_screen(list, 0.0f), list.row_height, list.accounts.count);
	}

	const RowDrag& drag = m_list.m_drag;

	if (drag.pressed_row && !drag.lifted && m_search.value().empty()) {
		const float      dx             = t_point.x - drag.press_point.x;
		const float      dy             = t_point.y - drag.press_point.y;
		const bool       past_threshold = dx * dx + dy * dy > K_DRAG_THRESHOLD * K_DRAG_THRESHOLD;
		const ListLayout list           = list_layout(layout);

		if (past_threshold && *drag.pressed_row < list.accounts.count) {
			lift_row(list, *drag.pressed_row, t_point);
		}
	}

	return true;
}

auto AccountModal::on_pointer_up(Vec2 t_point) -> bool
{
	if (std::exchange(m_press_swallowed, false)) return true;

	if (m_region_list.m_open) {
		if (const std::optional<u32> chosen = m_region_list.on_pointer_up(t_point)) {
			m_form.choose_region(*chosen);
		}

		return true;
	}

	if (m_rows_scroll.m_dragging) {
		m_rows_scroll.on_pointer_up();
		return true;
	}

	if (m_form_scroll.m_dragging) {
		m_form_scroll.on_pointer_up();
		return true;
	}

	if (m_list.m_selection.m_box) {
		m_list.m_selection.end_box();
		return true;
	}

	if (m_list.m_drag.lifted) {
		drop_row();
		return true;
	}

	if (m_list.m_drag.pressed_row) {
		m_list.m_selection.clear();
	}

	m_list.m_drag.pressed_row.reset();

	if (!is_blocking()) return false;

	const bool search_was_selecting = m_search.m_selecting;
	m_search.on_pointer_up();

	const bool form_was_selecting = m_form.end_text_selection();
	if (search_was_selecting || form_was_selecting) return true;

	const Layout layout        = this->layout();
	const bool   clicked_away  = !layout.panel.contains(t_point);
	const bool   clicked_close = back_badge_rect(layout).contains(t_point);

	if (m_mode != Mode::LOGIN_PROGRESS && (clicked_away || clicked_close)) {
		close();
		return true;
	}

	switch (m_mode) {
		using enum Mode;

		case ACCOUNT_LIST: {
			if (m_list.has_game()) {
				click_in_list(layout, t_point);
			}

			break;
		}

		case LOGIN_PROGRESS: {
			click_in_login_progress(layout, t_point);
			break;
		}

		case EDIT_ACCOUNT: {
			click_in_form(layout, t_point);
			break;
		}
	}

	return true;
}

auto AccountModal::click_in_login_progress(const Layout& t_layout, Vec2 t_point) -> void
{
	const Rect primary = primary_button_rect(t_layout.footer);
	const Rect back    = asks_for_permission() ? cancel_button_rect(primary) : primary;

	if (asks_for_permission() && primary.contains(t_point)) {
		m_commands->push(Command{.type = CommandType::OPEN_PERMISSION_SETTINGS});
	} else if (back.contains(t_point)) {
		m_session->cancel();
		m_mode = Mode::ACCOUNT_LIST;
	}
}

auto AccountModal::click_in_list(const Layout& t_layout, Vec2 t_point) -> void
{
	const Rect main   = t_layout.main_column;
	const Rect search = search_rect(main);

	if (!m_search.value().empty() && controls::search_clear_rect(search).contains(t_point)) {
		clear_search();
		m_search.set_focused(true);
		return;
	}

	if (search.contains(t_point)) return;

	if (add_button_rect(main).contains(t_point)) {
		start_form(std::nullopt);
		return;
	}

	const ListLayout list    = list_layout(t_layout);
	const bool       no_rows = list.accounts.count == 0 && m_search.value().empty();
	const bool       on_zone = no_rows && empty_zone(t_layout, list.area).contains(t_point);
	if (on_zone) {
		start_form(std::nullopt);
		return;
	}

	const std::optional<u32> row = row_at(t_layout, list, t_point);
	if (!row) return;

	const Rect       rect    = row_rect(t_layout, list, *row);
	const AccountRef account = list.accounts.refs[*row];

	if (login_button_rect(rect).contains(t_point)) {
		request_login(*m_list.m_game, account);
	} else if (favorite_button_rect(rect).contains(t_point)) {
		toggle_favorite(account);
	}
}

auto AccountModal::click_in_form(const Layout& t_layout, Vec2 t_point) -> void
{
	const FormLayout form = form_layout(t_layout.main_column);

	if (is_in_form(form, form.region_input, t_point)) {
		open_region_list();
		return;
	}

	if (is_in_form(form, form.show_in_input, t_point)) {
		m_form.focus(std::nullopt);
		m_show_in_open = !m_show_in_open;
		return;
	}

	if (const std::optional<u32> tile = show_in_tile_at(form, t_point)) {
		m_form.toggle_game(*tile);
		return;
	}

	if (is_in_form(form, form.reveal, t_point)) {
		m_form.toggle_password_shown();
		return;
	}

	m_form.focus(std::nullopt);

	const Rect save = primary_button_rect(t_layout.footer);

	if (cancel_button_rect(save).contains(t_point)) {
		m_mode = Mode::ACCOUNT_LIST;
	} else if (save.contains(t_point)) {
		save_form();
	}
}

auto AccountModal::on_right_click(Vec2 t_point) -> bool
{
	if (!is_blocking()) return false;
	if (m_open_amount < K_INTERACTIVE_OPEN_AMOUNT) return true;

	const Layout layout = this->layout();
	const Rect   main   = layout.main_column;

	if (m_mode == Mode::EDIT_ACCOUNT) {
		const FormLayout form = form_layout(main);

		if (const std::optional<EditField> clicked = field_at(form, t_point)) {
			TextInput* input = &m_form.m_fields[*clicked];

			m_form.focus(clicked);
			input->on_right_click(m_fonts->body, field_text_rect(form, *clicked), t_point.x);
			m_commands->push(Command{.type = CommandType::SHOW_TEXT_MENU, .position = t_point, .text_input = input});
		}

		return true;
	}

	if (m_mode != Mode::ACCOUNT_LIST || !m_list.has_game() || m_list.m_drag.lifted) return true;

	const Rect search = search_rect(main);

	if (search.contains(t_point)) {
		const Rect text_area = controls::search_text_rect(search, K_SEARCH_INSET);

		m_search.set_focused(true);
		m_search.on_right_click(m_fonts->secondary, text_area, t_point.x);
		m_commands->push(Command{.type = CommandType::SHOW_TEXT_MENU, .position = t_point, .text_input = &m_search});
		return true;
	}

	const std::optional<u32> row = row_at(layout, list_layout(layout), t_point);
	if (!row) return true;

	RowSelection* selection = &m_list.m_selection;
	if (!selection->contains(*row)) {
		selection->clear();
	}

	m_commands->push(Command{.type = CommandType::SHOW_ACCOUNT_MENU, .row = row, .position = t_point, .picked = selection->count()});
	return true;
}

auto AccountModal::on_scroll(Vec2, float t_wheel_delta) -> bool
{
	if (!is_blocking()) return false;
	if (m_open_amount < K_INTERACTIVE_OPEN_AMOUNT) return true;

	if (m_region_list.m_open) {
		m_region_list.on_scroll(t_wheel_delta);
		return true;
	}

	if (m_mode == Mode::ACCOUNT_LIST) {
		const ScrollGeometry geometry = list_layout(layout()).scroll;
		m_rows_scroll.on_scroll(t_wheel_delta, geometry);
	} else if (m_mode == Mode::EDIT_ACCOUNT) {
		const ScrollGeometry geometry = form_scroll(form_layout(layout().main_column));
		m_form_scroll.on_scroll(t_wheel_delta, geometry);
	}

	return true;
}

auto AccountModal::key_in_list(os::Key t_key) -> void
{
	if (m_list.m_drag.lifted || m_list.m_selection.m_box) return;

	const bool shortcut = os::modifiers().shortcut;

	if (shortcut && t_key == os::Key::F && is_search_visible()) {
		m_search.set_focused(true);
		m_search.apply(TextEdit::SELECT_ALL);
	} else if (shortcut && t_key == os::Key::A && !m_search.m_focused && can_select()) {
		m_list.m_selection.select_block(0, m_list.rows().count);
	} else if (m_search.m_focused) {
		m_search.on_key_down(t_key);
	} else if (t_key == os::Key::BACKSPACE && !m_search.value().empty() && is_search_visible()) {
		m_search.set_focused(true);
		m_search.on_key_down(t_key);
	}
}

auto AccountModal::key_in_form(os::Key t_key) -> void
{
	if (t_key == os::Key::TAB) {
		const EditField next = m_form.next_field(os::modifiers().shift);
		m_form.focus(next);
		reveal_field(next);
	} else if (t_key == os::Key::ENTER) {
		save_form();
	} else {
		for (TextInput& field : m_form.m_fields) {
			field.on_key_down(t_key);
		}
	}
}

auto AccountModal::escape() -> void
{
	if (m_mode == Mode::EDIT_ACCOUNT) {
		m_mode = Mode::ACCOUNT_LIST;
		return;
	}

	if (m_mode != Mode::ACCOUNT_LIST) return;

	if (m_list.m_drag.lifted) {
		cancel_row_drag();
	} else if (m_list.m_selection.m_box) {
		m_list.m_selection.cancel_box();
	} else if (m_list.m_selection.m_rows.any()) {
		m_list.m_selection.clear();
	} else if (!m_search.value().empty()) {
		clear_search();
	} else if (m_search.m_focused) {
		m_search.set_focused(false);
	} else {
		close();
	}
}

auto AccountModal::on_key_down(os::Key t_key) -> bool
{
	if (!is_blocking()) return false;

	if (m_region_list.m_open) {
		if (const std::optional<u32> chosen = m_region_list.on_key_down(t_key)) {
			m_form.choose_region(*chosen);
		}

		return true;
	}

	if (t_key == os::Key::ESCAPE) {
		escape();
	} else if (m_mode == Mode::ACCOUNT_LIST && m_list.has_game()) {
		key_in_list(t_key);
	} else if (m_mode == Mode::EDIT_ACCOUNT) {
		key_in_form(t_key);
	}

	return true;
}

auto AccountModal::on_char(u32 t_character) -> bool
{
	if (!is_blocking()) return false;

	if (m_mode == Mode::EDIT_ACCOUNT) {
		if (m_region_list.m_open) {
			m_region_list.on_char(t_character);
			return true;
		}

		for (TextInput& field : m_form.m_fields) {
			field.on_char(t_character);
		}
	} else if (m_mode == Mode::ACCOUNT_LIST && m_list.has_game() && !m_list.m_drag.lifted) {
		const bool printable = t_character >= 32 && t_character != 127;

		if (printable && !m_search.m_focused && is_search_visible()) {
			m_search.set_focused(true);
		}

		if (m_search.m_focused) {
			m_search.on_char(t_character);
		}
	}

	return true;
}

auto AccountModal::list_cursor(const Layout& t_layout) const -> CursorKind
{
	if (m_list.m_drag.lifted) return CursorKind::MOVE;
	if (m_list.m_selection.m_box) return CursorKind::ARROW;

	const Rect main   = t_layout.main_column;
	const Rect search = search_rect(main);

	if (!m_search.value().empty() && controls::search_clear_rect(search).contains(m_mouse)) return CursorKind::HAND;
	if (search.contains(m_mouse)) return CursorKind::I_BEAM;
	if (add_button_rect(main).contains(m_mouse)) return CursorKind::HAND;

	const ListLayout list    = list_layout(t_layout);
	const bool       no_rows = list.accounts.count == 0 && m_search.value().empty();

	if (no_rows && empty_zone(t_layout, list.area).contains(m_mouse)) return CursorKind::HAND;
	if (m_rows_scroll.is_over_track(m_mouse, list.scroll)) return CursorKind::HAND;

	const std::optional<u32> row = row_at(t_layout, list, m_mouse);
	if (!row) return CursorKind::ARROW;

	const bool on_button = is_row_button_hit(row_rect(t_layout, list, *row), m_mouse);

	return on_button ? CursorKind::HAND : CursorKind::ARROW;
}

auto AccountModal::form_cursor(const Layout& t_layout) const -> CursorKind
{
	if (m_region_list.m_open) return m_region_list.cursor(m_mouse);

	const FormLayout form       = form_layout(t_layout.main_column);
	const bool       on_show_in = is_in_form(form, form.show_in_input, m_mouse);
	const bool       on_region  = is_in_form(form, form.region_input, m_mouse);
	const bool       on_reveal  = is_in_form(form, form.reveal, m_mouse);
	if (on_show_in || on_region || on_reveal) return CursorKind::HAND;

	if (const std::optional<u32> tile = show_in_tile_at(form, m_mouse)) {
		return m_form.is_only_game(*tile) ? CursorKind::ARROW : CursorKind::HAND;
	}

	if (field_at(form, m_mouse)) return CursorKind::I_BEAM;
	if (m_form_scroll.is_over_track(m_mouse, form_scroll(form))) return CursorKind::HAND;

	const Rect save      = primary_button_rect(t_layout.footer);
	const bool on_cancel = cancel_button_rect(save).contains(m_mouse);
	const bool on_save   = m_form.has_changes(*m_library) && save.contains(m_mouse);

	return on_cancel || on_save ? CursorKind::HAND : CursorKind::ARROW;
}

auto AccountModal::cursor() const -> CursorKind
{
	if (!is_blocking()) return CursorKind::ARROW;
	if (m_rows_scroll.m_dragging || m_form_scroll.m_dragging) return CursorKind::DRAG;
	if (m_search.m_selecting || m_form.is_selecting_text()) return CursorKind::I_BEAM;

	const Layout layout = this->layout();

	if (m_mode != Mode::LOGIN_PROGRESS && back_badge_rect(layout).contains(m_mouse)) return CursorKind::HAND;

	switch (m_mode) {
		using enum Mode;

		case ACCOUNT_LIST: {
			return m_list.has_game() ? list_cursor(layout) : CursorKind::ARROW;
		}

		case LOGIN_PROGRESS: {
			const Rect primary   = primary_button_rect(layout.footer);
			const bool on_back   = asks_for_permission() && cancel_button_rect(primary).contains(m_mouse);
			const bool on_button = primary.contains(m_mouse) || on_back;

			return on_button ? CursorKind::HAND : CursorKind::ARROW;
		}

		case EDIT_ACCOUNT: {
			return form_cursor(layout);
		}
	}

	return CursorKind::ARROW;
}

auto AccountModal::draw_chrome(DrawList* t_draw_list, const Layout& t_layout, bool t_with_art, u8 t_alpha) const -> void
{
	const RoundnessScope corners{controls::K_POPUP_ROUNDNESS};

	const Game&       game        = m_library->games[*m_list.m_game];
	const Rect        art         = t_layout.art_column;
	const Rect        sheet       = t_layout.sheet;
	const bool        docked      = is_docked();
	const float       radius      = docked ? 0.0f : K_PANEL_RADIUS;
	const float       inner       = docked ? 0.0f : std::max(0.0f, K_PANEL_RADIUS - K_PANEL_BORDER);
	const CornerRadii art_radii   = rounded(inner, 0.0f, 0.0f, inner);
	const CornerRadii sheet_radii = rounded(0.0f, inner, inner, 0.0f);
	const u8          art_alpha   = m_art_source ? 255 : t_alpha;
	const float       top_width   = std::max(0.0f, sheet.w - scaled_radius(inner));
	const Rect        top_line{sheet.x, sheet.y, top_width, 1.0f};
	const Rect        separator{art.right(), art.y, K_ART_SEPARATOR_WIDTH, art.h};

	if (!docked) {
		draw_shadow_around(t_draw_list, t_layout.panel, m_open_amount);
	}

	controls::draw_glass(t_draw_list, sheet, sheet_radii, controls::GlassSurface::SHEET, t_alpha, false);
	t_draw_list->add_rect(top_line, faded(K_COLOR_TOP_HIGHLIGHT, t_alpha));

	if (t_with_art && game.banner != nullptr) {
		const UvRect banner_crop = cover_uv(art.w / art.h, game.banner->aspect());
		t_draw_list->add_image(art, game.banner, faded(K_COLOR_ON_ART, art_alpha), art_radii, banner_crop);
	} else if (t_with_art) {
		t_draw_list->add_rounded_rect(art, art_radii, faded(game.accent, art_alpha));
	} else {
		t_draw_list->add_rounded_rect(art, art_radii, faded(g_theme.surface, t_alpha));
	}

	t_draw_list->add_rect(separator, faded(g_theme.border, t_alpha));
	t_draw_list->add_rounded_outline(t_layout.panel, rounded(radius), K_PANEL_BORDER, faded(g_theme.border, t_alpha));
}

auto AccountModal::draw_back_badge(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) const -> void
{
	if (m_mode == Mode::LOGIN_PROGRESS) return;

	const RoundnessScope pill{K_EXACT_ROUNDNESS};

	const Font&       secondary = m_fonts->secondary;
	const Rect        badge     = back_badge_rect(t_layout);
	const CornerRadii radii     = rounded(badge.h * 0.5f);
	const bool        lit       = badge.contains(m_mouse);
	const float       icon_y    = snapped_to_pixel(badge.center().y - K_BACK_ICON_SIZE * 0.5f);
	const Rect        icon{badge.x + K_BACK_PADDING_X, icon_y, K_BACK_ICON_SIZE, K_BACK_ICON_SIZE};
	const Vec2        label_baseline{icon.right() + K_BACK_ICON_GAP, secondary.centered_baseline(badge)};
	const Color       rim = faded(with_alpha(g_theme.text, lit ? K_BACK_RIM_HOVER_ALPHA : K_BACK_RIM_ALPHA), t_alpha);

	controls::draw_popup_shadow(t_draw_list, badge, badge.h * 0.5f, t_alpha / 255.0f);
	controls::draw_glass(t_draw_list, badge, radii, controls::GlassSurface::SHEET, t_alpha, false);

	if (lit) {
		const Color highlight = faded(controls::glass_highlight(), t_alpha);
		t_draw_list->add_rounded_rect(badge, radii, highlight);
	}

	t_draw_list->add_rounded_outline(badge, radii, 1.0f, rim);
	t_draw_list->add_image(icon, m_assets->texture(Asset::ICON_ARROW_BACK), faded(g_theme.text, t_alpha));
	draw_text(t_draw_list, secondary, label_baseline, K_BACK_LABEL, faded(g_theme.text, t_alpha));
}

auto AccountModal::draw_morphing_art(DrawList* t_draw_list, const Layout& t_layout, float t_scale) const -> void
{
	const RoundnessScope exact{K_EXACT_ROUNDNESS};

	const Game&     game   = m_library->games[*m_list.m_game];
	const ArtSource source = *m_art_source;
	const float     amount = 0.5f - 0.5f * std::cos(m_morph_progress * std::numbers::pi_v<float>);

	const Rect        target         = scaled_about(t_layout.art_column, t_layout.panel.center(), t_scale);
	const Rect        art            = lerp(source.rect, target, amount);
	const float       panel_corner   = is_docked() ? 0.0f : std::max(0.0f, K_PANEL_RADIUS - K_PANEL_BORDER) * controls::K_POPUP_ROUNDNESS * t_scale;
	const float       source_corner  = source.radius * user_roundness();
	const float       leading_corner = lerp(source_corner, panel_corner, amount);
	const float       other_corners  = lerp(source_corner, 0.0f, amount);
	const CornerRadii radii          = rounded(leading_corner, other_corners, other_corners, other_corners);
	const float       banner_share   = source.is_icon ? std::clamp(amount / K_ICON_CROSSFADE_SHARE, 0.0f, 1.0f) : 1.0f;
	const float       flight         = std::sin(amount * std::numbers::pi_v<float>);

	if (flight > 0.01f) {
		controls::draw_panel_shadow(t_draw_list, art, std::max(leading_corner, other_corners), flight);
	}

	const float frame       = 1.0f - amount;
	const u8    frame_alpha = to_alpha(frame);

	if (source.glow > 0.0f && frame_alpha > 0) {
		t_draw_list->add_banner_glow(art, other_corners, source.glow, faded(source.glow_color, frame_alpha));
	}

	if (source.border > 0.0f && frame_alpha > 0) {
		t_draw_list->add_rounded_rect(art, radii, faded(source.border_color, frame_alpha));
	}

	const float       inset         = source.border * frame;
	const Rect        image         = art.inset(inset);
	const float       inner_leading = std::max(0.0f, leading_corner - inset);
	const float       inner_other   = std::max(0.0f, other_corners - inset);
	const CornerRadii image_radii   = rounded(inner_leading, inner_other, inner_other, inner_other);
	const float       image_aspect  = image.w / image.h;

	const u8 banner_alpha = to_alpha(banner_share);
	const u8 icon_alpha   = to_alpha(1.0f - banner_share);

	if (game.banner != nullptr) {
		const UvRect banner_crop = cover_uv(image_aspect, game.banner->aspect());
		t_draw_list->add_image(image, game.banner, faded(K_COLOR_ON_ART, banner_alpha), image_radii, banner_crop);
	} else {
		t_draw_list->add_rounded_rect(image, image_radii, faded(game.accent, banner_alpha));
	}

	if (source.is_icon && game.icon != nullptr && banner_share < 1.0f) {
		const UvRect icon_crop = cover_uv(image_aspect, game.icon->aspect());
		t_draw_list->add_image(image, game.icon, faded(K_COLOR_ON_ART, icon_alpha), image_radii, icon_crop);
	}
}

auto AccountModal::draw_list_header(DrawList* t_draw_list, Rect t_main, float t_rows_top, u8 t_alpha) -> void
{
	const Font& title      = m_fonts->title;
	const Font& secondary  = m_fonts->secondary;
	const Rect  header     = list_header(t_main);
	const Rect  search     = search_rect(t_main);
	const Rect  add        = add_button_rect(t_main);
	const float title_end  = search.w > 0.0f ? search.x : add.x;
	const float title_room = std::max(0.0f, title_end - K_TITLE_GAP - header.x);
	const Vec2  title_baseline{header.x, title.centered_baseline(header)};

	draw_text_truncated(t_draw_list, title, title_baseline, K_ACCOUNTS_TITLE, title_room, faded(g_theme.text, t_alpha));

	if (search.w > 0.0f) {
		controls::draw_search_field(t_draw_list, secondary, search, K_SEARCH_INSET, &m_search, m_mouse, m_settings->accent, t_alpha);
	}

	const bool  add_hovered = add.contains(m_mouse);
	const Color ink         = add_hovered ? g_theme.text : g_theme.text_dim;
	const Color outline     = add_hovered ? g_theme.border : g_theme.separator;
	const float plus_y      = snapped_to_pixel(add.center().y - K_ADD_ICON_SIZE * 0.5f);
	const Rect  plus{add.x + K_ADD_PADDING_X, plus_y, K_ADD_ICON_SIZE, K_ADD_ICON_SIZE};
	const Vec2  label_baseline{plus.right() + K_ADD_ICON_GAP, secondary.centered_baseline(add)};

	if (add_hovered) {
		const Color highlight = faded(controls::glass_highlight(), t_alpha);
		t_draw_list->add_rounded_rect(add, rounded(K_CONTROL_RADIUS), highlight);
	}

	t_draw_list->add_rounded_outline(add, rounded(K_CONTROL_RADIUS), 1.0f, faded(outline, t_alpha));
	t_draw_list->add_image(plus, m_assets->texture(Asset::ICON_ADD), faded(ink, t_alpha));
	draw_text(t_draw_list, secondary, label_baseline, K_ADD_LABEL, faded(ink, t_alpha));

	const float scrolled = std::clamp(m_rows_scroll.offset() / K_SCROLLED_LINE_DISTANCE, 0.0f, 1.0f);
	if (scrolled > 0.0f) {
		const Rect hairline{t_main.x, t_rows_top - 1.0f, t_main.w, 1.0f};
		t_draw_list->add_rect(hairline, faded(g_theme.separator, static_cast<u8>(t_alpha * scrolled)));
	}
}

auto AccountModal::draw_account_row(DrawList* t_draw_list, Rect t_row, const Account& t_account, bool t_raised, bool t_selected, float t_hover, u8 t_alpha) const -> void
{
	const Rect  highlight   = row_highlight(t_row);
	const bool  interactive = !m_list.m_drag.lifted && !t_raised;
	const float hover       = t_raised ? 0.0f : t_hover;
	const Color accent      = m_settings->accent;

	if (t_raised && m_list.m_lift > 0.0f) {
		const auto lift_alpha = static_cast<u8>(t_alpha * m_list.m_lift);
		t_draw_list->add_bordered_rect(highlight, rounded(K_ROW_RADIUS), faded(g_theme.popup, lift_alpha), faded(g_theme.border, lift_alpha), 1.0f);
	}

	if (t_selected) {
		const Color picked = with_alpha(accent, static_cast<u8>(K_SELECTED_ALPHA * t_alpha / 255.0f));
		t_draw_list->add_rounded_rect(highlight, rounded(K_ROW_RADIUS), picked);
	}

	if (hover > 0.0f) {
		const Color glow = faded(controls::glass_highlight(hover * K_ROW_HOVER_SHARE), t_alpha);
		t_draw_list->add_rounded_rect(highlight, rounded(K_ROW_RADIUS), glow);
	}

	const Font& body        = m_fonts->body;
	const Font& secondary   = m_fonts->secondary;
	const Rect  favorite    = favorite_button_rect(t_row);
	const Rect  login       = login_button_rect(t_row);
	const bool  has_details = t_account.note[0] != '\0' || t_account.last_used != 0;

	const std::string_view region     = t_account.region;
	const float            chip_width = region.empty() ? 0.0f : controls::region_chip_width(secondary, region);
	const float            chip_x     = favorite.x - K_REGION_CHIP_GAP - chip_width;
	const float            text_end   = region.empty() ? favorite.x : chip_x;
	const float            text_room  = std::max(0.0f, text_end - K_ROW_TEXT_GAP - t_row.x);

	const float block_height = body.line_height() + K_ROW_LINE_GAP + secondary.line_height();
	const float block_y      = t_row.y + (t_row.h - block_height) * 0.5f;
	const float username_y   = has_details ? block_y + body.ascent : body.centered_baseline(t_row);
	const Vec2  username_baseline{t_row.x, username_y};

	draw_text_truncated(t_draw_list, body, username_baseline, t_account.username, text_room, faded(g_theme.text, t_alpha));

	if (has_details) {
		const Vec2 details_baseline{t_row.x, block_y + body.line_height() + K_ROW_LINE_GAP + secondary.ascent};
		controls::draw_account_details(t_draw_list, secondary, details_baseline, text_room, t_account, t_alpha);
	}

	if (!region.empty()) {
		controls::draw_region_chip(t_draw_list, secondary, chip_x, t_row.center().y, region, t_alpha);
	}

	const bool favorite_hovered = interactive && favorite.contains(m_mouse);
	const Rect favorite_icon    = favorite.inset(K_ROW_ICON_INSET);
	const auto shown            = static_cast<u8>(t_alpha * hover);

	if (favorite_hovered) {
		controls::draw_circular_hover(t_draw_list, favorite, g_theme.shadow, g_theme.control_hover, t_alpha);
	}

	if (t_account.favorite) {
		controls::draw_favorite(t_draw_list, m_assets, favorite_icon, true, faded(accent, t_alpha));
	} else if (hover > 0.0f) {
		const Color outline = favorite_hovered ? g_theme.text : g_theme.text_faint;
		controls::draw_favorite(t_draw_list, m_assets, favorite_icon, false, faded(outline, shown));
	}

	if (hover <= 0.0f) return;

	const Vec2  slide{snapped_to_pixel((1.0f - hover) * K_PILL_SLIDE), 0.0f};
	const Rect  pill    = login.moved(slide);
	const bool  on_pill = interactive && pill.contains(m_mouse);
	const Color fill    = on_pill ? hovered(accent) : accent;

	const Color ink = faded(controls::contrasting_theme_text(accent), shown);

	t_draw_list->add_rounded_rect(pill, rounded(K_PILL_RADIUS), faded(fill, shown));
	draw_text_centered(t_draw_list, secondary, pill, K_LOGIN_LABEL, ink);
}

auto AccountModal::draw_empty_state(DrawList* t_draw_list, const Layout& t_layout, Rect t_list_area, u8 t_alpha) const -> void
{
	const Rect zone = empty_zone(t_layout, t_list_area);
	controls::draw_drop_zone(t_draw_list, m_assets, m_fonts->body, m_fonts->secondary, zone, K_ZONE_TITLE, K_ZONE_HINT, m_settings->accent, m_zone_hover, t_alpha);
}

auto AccountModal::draw_no_matches(DrawList* t_draw_list, Rect t_list_area, u8 t_alpha) const -> void
{
	const Font&                body      = m_fonts->body;
	const Font&                secondary = m_fonts->secondary;
	constexpr std::string_view TITLE     = "No matches";
	constexpr std::string_view HINT      = "Try a different name or note.";

	const float stack_height = body.line_height() + K_EMPTY_LINE_GAP + secondary.line_height();
	const float top          = t_list_area.center().y - stack_height * 0.5f;
	const float center_x     = t_list_area.center().x;
	const Vec2  title_baseline{center_x - text_width(body, TITLE) * 0.5f, top + body.ascent};
	const Vec2  hint_baseline{center_x - text_width(secondary, HINT) * 0.5f, top + body.line_height() + K_EMPTY_LINE_GAP + secondary.ascent};

	draw_text(t_draw_list, body, title_baseline, TITLE, faded(g_theme.text_dim, t_alpha));
	draw_text(t_draw_list, secondary, hint_baseline, HINT, faded(g_theme.text_faint, t_alpha));
}

auto AccountModal::draw_account_list(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) -> void
{
	const ListLayout list = list_layout(t_layout);

	if (list.accounts.count > 0) {
		draw_account_rows(t_draw_list, t_layout, list, t_alpha);
	} else if (m_search.value().empty()) {
		draw_empty_state(t_draw_list, t_layout, list.area, t_alpha);
	} else {
		draw_no_matches(t_draw_list, list.area, t_alpha);
	}

	draw_list_header(t_draw_list, t_layout.main_column, list.area.y, t_alpha);
}

auto AccountModal::draw_account_rows(DrawList* t_draw_list, const Layout& t_layout, const ListLayout& t_list, u8 t_alpha) -> void
{
	const RowSet& raised = m_list.m_drag.lifted ? m_list.m_drag.rows : m_list.m_raised;
	const Rect    first  = row_highlight(row_rect_at(t_layout, t_list, 0.0f));
	const Color   line   = faded(g_theme.separator, t_alpha);

	const auto separator_at = [&](float t_y, Color t_color) {
		const Rect separator{first.x, t_y, first.w, 1.0f};
		t_draw_list->add_rect(separator, t_color);
	};

	t_draw_list->push_clip(t_list.area);
	separator_at(list_to_screen(t_list, 0.0f), line);

	for (const u32 row : std::views::iota(0u, t_list.accounts.count)) {
		if (raised[row]) continue;

		const Rect rect = row_rect(t_layout, t_list, row);
		if (!rect.overlaps_vertically(t_list.area)) continue;

		const Account& account = *m_library->account(t_list.accounts.refs[row]);
		separator_at(rect.bottom() - 1.0f, line);
		draw_account_row(t_draw_list, rect, account, false, m_list.m_selection.contains(row), m_list.m_hover[row], t_alpha);
	}

	// Lifted rows go on top, with all their shadows first so none falls across another lifted row.
	const float shadow = m_list.m_lift * t_alpha / 255.0f;

	if (m_list.m_lift > 0.0f) {
		for (const u32 row : std::views::iota(0u, t_list.accounts.count)) {
			if (!raised[row]) continue;

			const Rect rect = row_rect_at(t_layout, t_list, raised_top(t_list, row));
			controls::draw_popup_shadow(t_draw_list, row_highlight(rect), K_ROW_RADIUS, shadow);
		}
	}

	const Color lifted_line = faded(line, static_cast<u8>(255.0f * (1.0f - m_list.m_lift)));

	for (const u32 row : std::views::iota(0u, t_list.accounts.count)) {
		if (!raised[row]) continue;

		const Rect     rect    = row_rect_at(t_layout, t_list, raised_top(t_list, row));
		const Account& account = *m_library->account(t_list.accounts.refs[row]);
		separator_at(rect.bottom() - 1.0f, lifted_line);
		draw_account_row(t_draw_list, rect, account, true, m_list.m_selection.contains(row), 0.0f, t_alpha);
	}

	t_draw_list->pop_clip();

	m_rows_scroll.draw(t_draw_list, t_list.scroll, m_mouse, t_alpha);

	if (m_list.m_selection.box_shown()) {
		const Rect box = m_list.m_selection.box_rect(list_to_screen(t_list, 0.0f));

		t_draw_list->push_clip(t_list.area);
		controls::draw_marquee(t_draw_list, box, m_settings->accent, t_alpha);
		t_draw_list->pop_clip();
	}
}

auto AccountModal::draw_login_progress(DrawList* t_draw_list, Rect t_main, u8 t_alpha) const -> void
{
	const LoginStage stage     = m_session->stage();
	const bool       finished  = m_session->is_finished();
	const Font&      body      = m_fonts->body;
	const Font&      secondary = m_fonts->secondary;
	const Color      accent    = m_settings->accent;

	const float width = std::min(t_main.w - K_ROW_PADDING * 2.0f, K_PROGRESS_MAX_WIDTH);
	const float bar_x = snapped_to_pixel(t_main.center().x - width * 0.5f);
	const Rect  bar{bar_x, snapped_to_pixel(t_main.center().y), width, K_PROGRESS_BAR_HEIGHT};
	const float status_baseline = bar.y - K_PROGRESS_TEXT_GAP;
	const float step_baseline   = bar.bottom() + K_PROGRESS_TEXT_GAP + secondary.ascent * K_CAP_HEIGHT_SHARE;
	const float alpha_scale     = t_alpha / 255.0f;

	const auto draw_status = [&](std::string_view t_text, float t_opacity, float t_offset) {
		if (t_text.empty() || t_opacity <= 0.0f) return;

		std::string_view lines[K_MAX_MESSAGE_LINES];
		const u32        line_count  = wrap_text(body, t_text, width, lines);
		const u32        lines_above = line_count > 0 ? line_count - 1 : 0;
		const Color      color       = faded(g_theme.text, static_cast<u8>(t_alpha * t_opacity));
		float            baseline    = status_baseline + t_offset - static_cast<float>(lines_above) * body.line_height();

		for (const std::string_view line : std::span{lines, line_count}) {
			const Vec2 line_baseline{snapped_to_pixel(bar.center().x - text_width(body, line) * 0.5f), baseline};
			draw_text(t_draw_list, body, line_baseline, line, color);
			baseline += body.line_height();
		}
	};

	const float change        = m_session->m_status_change;
	const float leaving_rise  = -K_PROGRESS_TEXT_RISE * change;
	const float arriving_rise = K_PROGRESS_TEXT_RISE * (1.0f - change);
	draw_status(m_session->m_previous_status, 1.0f - change, leaving_rise);
	draw_status(m_session->m_status, change, arriving_rise);

	const Color outcome    = stage == LoginStage::SUCCESS ? g_theme.success : g_theme.error;
	const Color fill_color = finished ? mix(accent, outcome, m_session->m_outcome) : accent;
	const float progress   = std::clamp(m_session->m_progress, 0.0f, 1.0f);
	const Rect  fill{bar.x, bar.y, bar.w * progress, bar.h};

	t_draw_list->add_rounded_rect(bar, rounded(bar.h * 0.5f), faded(g_theme.control, t_alpha));

	if (fill.w > 0.5f) {
		const Color glow = with_alpha(fill_color, static_cast<u8>(K_PROGRESS_GLOW_ALPHA * alpha_scale));

		t_draw_list->add_shadow(fill, bar.h * 0.5f, K_PROGRESS_GLOW_BLUR, glow);
		t_draw_list->add_rounded_rect(fill, rounded(bar.h * 0.5f), faded(fill_color, t_alpha));

		if (!finished) {
			const float travel  = std::fmod(m_session->m_seconds * K_SHEEN_PASSES_PER_SECOND, 1.0f);
			const float sheen_x = fill.x - K_SHEEN_WIDTH + travel * (fill.w + K_SHEEN_WIDTH);
			const float half    = K_SHEEN_WIDTH * 0.5f;
			const Color sheen   = lightened(fill_color, K_SHEEN_LIGHTEN);
			const Color edge    = with_alpha(sheen, 0);
			const Color peak    = with_alpha(sheen, static_cast<u8>(K_SHEEN_ALPHA * alpha_scale));
			const Rect  rising{sheen_x, fill.y, half, fill.h};
			const Rect  falling{sheen_x + half, fill.y, half, fill.h};

			t_draw_list->push_clip(fill);
			t_draw_list->add_gradient(rising, edge, peak, edge, peak);
			t_draw_list->add_gradient(falling, peak, edge, peak, edge);
			t_draw_list->pop_clip();
		}
	}

	const u32 step = m_session->step();
	if (step == 0) return;

	char                   label[24];
	const std::string_view step_text = format_into(label, "Step {} of {}", step, LoginSession::K_STEP_COUNT);
	const Vec2             step_at{snapped_to_pixel(bar.center().x - text_width(secondary, step_text) * 0.5f), step_baseline};
	const Color            step_color = faded(g_theme.text_faint, static_cast<u8>(t_alpha * (1.0f - m_session->m_outcome)));

	draw_text(t_draw_list, secondary, step_at, step_text, step_color);
}

auto AccountModal::draw_form_header(DrawList* t_draw_list, Rect t_main, u8 t_alpha) const -> void
{
	const Font& body      = m_fonts->body;
	const Font& secondary = m_fonts->secondary;
	const Game& game      = m_library->games[*m_list.m_game];
	const Rect  header{t_main.x, t_main.y, t_main.w, edit_header_height()};
	const float icon_y = header.center().y - K_EDIT_HEADER_ICON_SIZE * 0.5f;
	const Rect  icon{t_main.x + K_ROW_PADDING, icon_y, K_EDIT_HEADER_ICON_SIZE, K_EDIT_HEADER_ICON_SIZE};

	draw_game_icon(t_draw_list, game, icon, K_EDIT_HEADER_ICON_RADIUS, t_alpha);

	const float            block   = body.line_height() + K_EDIT_HEADER_LINE_GAP + secondary.line_height();
	const float            top     = header.center().y - block * 0.5f;
	const float            text_x  = icon.right() + K_EDIT_HEADER_GAP;
	const float            room    = header.right() - K_ROW_PADDING - text_x;
	const std::string_view heading = m_form.m_account ? "Edit account" : "Add account";
	const Vec2             heading_baseline{text_x, top + body.ascent};
	const Vec2             game_baseline{text_x, top + body.line_height() + K_EDIT_HEADER_LINE_GAP + secondary.ascent};
	const Rect             underline{t_main.x + K_ROW_PADDING, header.bottom(), t_main.w - K_ROW_PADDING * 2.0f, 1.0f};

	draw_text_truncated(t_draw_list, body, heading_baseline, heading, room, faded(g_theme.text, t_alpha));
	draw_text_truncated(t_draw_list, secondary, game_baseline, game.title, room, faded(g_theme.text_faint, t_alpha));
	t_draw_list->add_rect(underline, faded(g_theme.separator, t_alpha));
}

auto AccountModal::draw_form(DrawList* t_draw_list, Rect t_main, u8 t_alpha) -> void
{
	const FormLayout form = form_layout(t_main);
	const bool       live = !m_region_list.m_open && form.area.contains(m_mouse);

	t_draw_list->push_clip(form.area);
	draw_form_fields(t_draw_list, form, live, t_alpha);
	draw_form_region(t_draw_list, form, live, t_alpha);
	draw_show_in(t_draw_list, form, t_alpha);
	t_draw_list->pop_clip();

	m_form_scroll.draw(t_draw_list, form_scroll(form), m_mouse, t_alpha);
	draw_form_header(t_draw_list, t_main, t_alpha);
}

auto AccountModal::draw_form_fields(DrawList* t_draw_list, const FormLayout& t_form, bool t_live, u8 t_alpha) -> void
{
	const Font& body         = m_fonts->body;
	const Font& secondary    = m_fonts->secondary;
	const Color accent       = m_settings->accent;
	const Color text         = faded(g_theme.text, t_alpha);
	const Color caret        = faded(accent, t_alpha);
	const Color active_label = mix(g_theme.text_dim, accent, K_ACTIVE_LABEL_TINT);

	for (const EditField field : each<EditField>()) {
		TextInput* input       = &m_form.m_fields[field];
		const Rect box         = t_form.field_inputs[field];
		const Rect label       = t_form.field_labels[field];
		const bool focused     = input->m_focused;
		const bool required    = field != EditField::NOTE;
		const bool missing     = m_form.m_show_required && required && input->value().empty();
		const bool box_hovered = t_live && !focused && box.contains(m_mouse);

		Color label_color = g_theme.text_dim;
		Color border      = g_theme.separator;
		if (missing) {
			label_color = g_theme.error;
			border      = g_theme.error;
		} else if (focused) {
			label_color = active_label;
			border      = accent;
		} else if (box_hovered) {
			border = g_theme.border;
		}

		const Vec2 label_baseline{label.x, label.y + secondary.ascent};
		draw_text(t_draw_list, secondary, label_baseline, K_FIELD_LABELS[field], faded(label_color, t_alpha));

		if (!required) {
			const Vec2 suffix_baseline{label_baseline.x + text_width(secondary, K_FIELD_LABELS[field]), label_baseline.y};
			draw_text(t_draw_list, secondary, suffix_baseline, K_OPTIONAL_SUFFIX, faded(g_theme.text_faint, t_alpha));
		}

		draw_input_box(t_draw_list, box, border, focused && !missing, accent, t_alpha);
		input->draw(t_draw_list, body, field_text_rect(t_form, field), text, caret);

		const Vec2 hint_at{box.x + K_FORM_HINT_INDENT, box.bottom() + K_FORM_HINT_GAP};

		if (missing) {
			controls::draw_notice(t_draw_list, secondary, hint_at, controls::NoticeKind::ALERT, "Required", faded(g_theme.error, t_alpha));
		} else if (field == EditField::PASSWORD && focused && os::is_caps_lock_on()) {
			const Color caution = faded(controls::caution_color(), t_alpha);
			controls::draw_notice(t_draw_list, secondary, hint_at, controls::NoticeKind::CAPS_LOCK, "Caps Lock is on", caution);
		}
	}

	const bool  shown_password = !m_form.m_fields[EditField::PASSWORD].m_masked;
	const bool  reveal_hovered = is_in_form(t_form, t_form.reveal, m_mouse);
	const Color eye            = reveal_hovered ? g_theme.text : g_theme.text_dim;
	controls::draw_eye(t_draw_list, m_assets, t_form.reveal, shown_password, faded(eye, t_alpha));
}

auto AccountModal::draw_form_region(DrawList* t_draw_list, const FormLayout& t_form, bool t_live, u8 t_alpha) const -> void
{
	const Font& body      = m_fonts->body;
	const Font& secondary = m_fonts->secondary;
	const Color accent    = m_settings->accent;
	const Rect  region    = t_form.region_input;
	const bool  open      = m_region_list.m_open;
	const bool  hovered   = t_live && region.contains(m_mouse);
	const Color label     = open ? mix(g_theme.text_dim, accent, K_ACTIVE_LABEL_TINT) : g_theme.text_dim;

	const Vec2 label_baseline{t_form.region_label.x, t_form.region_label.y + secondary.ascent};
	const Vec2 suffix_baseline{label_baseline.x + text_width(secondary, K_REGION_FIELD_LABEL), label_baseline.y};
	draw_text(t_draw_list, secondary, label_baseline, K_REGION_FIELD_LABEL, faded(label, t_alpha));
	draw_text(t_draw_list, secondary, suffix_baseline, K_OPTIONAL_SUFFIX, faded(g_theme.text_faint, t_alpha));

	Color border = g_theme.separator;
	if (open) {
		border = accent;
	} else if (hovered) {
		border = g_theme.border;
	}

	draw_input_box(t_draw_list, region, border, open, accent, t_alpha);

	const u32              choice      = region_index(m_form.m_region);
	const bool             chosen      = choice != 0;
	const std::string_view value       = chosen ? K_REGION_OPTIONS[choice].label : std::string_view{"None"};
	const Color            value_color = chosen ? g_theme.text : g_theme.text_faint;
	const Rect             chevron     = chevron_in(region);
	const float            value_x     = region.x + K_INPUT_PADDING_X;
	const float            value_room  = chevron.x - K_INPUT_PADDING_X - value_x;
	const Vec2             value_baseline{value_x, body.centered_baseline(region)};
	const Color            chevron_color = open || hovered ? g_theme.text : g_theme.text_dim;

	draw_text_truncated(t_draw_list, body, value_baseline, value, value_room, faded(value_color, t_alpha));
	controls::draw_chevron(t_draw_list, chevron, open, faded(chevron_color, t_alpha));
}

auto AccountModal::draw_show_in(DrawList* t_draw_list, const FormLayout& t_form, u8 t_alpha) const -> void
{
	const Font& body        = m_fonts->body;
	const Font& secondary   = m_fonts->secondary;
	const Color accent      = m_settings->accent;
	const Rect  box         = t_form.show_in_input;
	const bool  live        = !m_region_list.m_open && t_form.area.contains(m_mouse);
	const bool  box_hovered = live && box.contains(m_mouse);
	const Color label       = m_show_in_open ? mix(g_theme.text_dim, accent, K_ACTIVE_LABEL_TINT) : g_theme.text_dim;
	const Vec2  label_baseline{t_form.show_in_label.x, t_form.show_in_label.y + secondary.ascent};

	draw_text(t_draw_list, secondary, label_baseline, K_SHOW_IN_FIELD_LABEL, faded(label, t_alpha));

	Color border = g_theme.separator;
	if (m_show_in_open) {
		border = accent;
	} else if (box_hovered) {
		border = g_theme.border;
	}

	draw_input_box(t_draw_list, box, border, false, accent, t_alpha);

	float              icon_x = box.x + K_INPUT_PADDING_X;
	std::optional<u32> only_game;

	for (const u32 game : std::views::iota(0u, m_library->game_count)) {
		const bool  selected   = (m_form.m_visible_mask & (1u << game)) != 0;
		const u8    icon_alpha = selected ? t_alpha : static_cast<u8>(t_alpha * K_SUMMARY_UNSELECTED_ICON_ALPHA / 255);
		const float icon_y     = box.center().y - K_SUMMARY_ICON_SIZE * 0.5f;
		const Rect  icon{icon_x, icon_y, K_SUMMARY_ICON_SIZE, K_SUMMARY_ICON_SIZE};

		if (selected) {
			only_game = game;
		}

		draw_game_icon(t_draw_list, m_library->games[game], icon, K_SUMMARY_ICON_RADIUS, icon_alpha);
		icon_x += K_SUMMARY_ICON_SIZE + K_SUMMARY_ICON_GAP;
	}

	const auto       shown_count = static_cast<u32>(std::popcount(m_form.m_visible_mask));
	const Rect       chevron     = chevron_in(box);
	const float      text_left   = icon_x - K_SUMMARY_ICON_GAP + K_SUMMARY_TEXT_GAP;
	const float      text_right  = chevron.x - K_SUMMARY_TEXT_GAP;
	const float      room        = std::max(0.0f, text_right - text_left);
	char             summary[96];
	std::string_view summary_text = "All games";

	if (shown_count < m_library->game_count && shown_count == 1 && only_game) {
		const Game& game = m_library->games[*only_game];
		summary_text     = format_into(summary, "Only {}", game.title);

		if (!game.short_title.empty() && text_width(body, summary_text) > room) {
			summary_text = format_into(summary, "Only {}", game.short_title);
		}
	} else if (shown_count < m_library->game_count) {
		summary_text = format_into(summary, "{} of {}", shown_count, m_library->game_count);
	}

	const float summary_width = std::min(text_width(body, summary_text), room);
	const Vec2  summary_baseline{snapped_to_pixel(text_right - summary_width), body.centered_baseline(box)};
	const Color chevron_color = m_show_in_open || box_hovered ? g_theme.text : g_theme.text_dim;

	draw_text_truncated(t_draw_list, body, summary_baseline, summary_text, room, faded(g_theme.text_dim, t_alpha));
	controls::draw_chevron(t_draw_list, chevron, m_show_in_open, faded(chevron_color, t_alpha));

	if (m_show_in_amount > 0.001f) {
		draw_show_in_tiles(t_draw_list, t_form, live, t_alpha);
	}
}

auto AccountModal::draw_show_in_tiles(DrawList* t_draw_list, const FormLayout& t_form, bool t_live, u8 t_alpha) const -> void
{
	const Font& secondary   = m_fonts->secondary;
	const Color accent      = m_settings->accent;
	const auto  shown_count = static_cast<u32>(std::popcount(m_form.m_visible_mask));
	const float revealed    = (K_TILES_TOP_GAP + t_form.tiles.h + 2.0f) * m_show_in_amount;
	const Rect  clip{t_form.tiles.x - K_TILES_CLIP_BLEED, t_form.show_in_input.bottom(), t_form.tiles.w + K_TILES_CLIP_BLEED * 2.0f, revealed};

	t_draw_list->push_clip(clip);

	for (const u32 index : std::views::iota(0u, m_library->game_count)) {
		const Game& game         = m_library->games[index];
		const Rect  tile         = show_in_tile(t_form, index);
		const bool  selected     = (m_form.m_visible_mask & (1u << index)) != 0;
		const bool  locked       = selected && shown_count == 1;
		const bool  tile_hovered = t_live && !locked && tile.contains(m_mouse);

		Color border = g_theme.separator;
		if (selected) {
			border = accent;
		} else if (tile_hovered) {
			border = g_theme.border;
		}

		const Color fill         = selected ? mix(g_theme.field, accent, K_TILE_SELECTED_TINT) : g_theme.field;
		const float border_width = selected ? K_INPUT_FOCUS_BORDER : 1.0f;
		t_draw_list->add_bordered_rect(tile, rounded(K_TILE_RADIUS), faded(fill, t_alpha), faded(border, t_alpha), border_width);

		const u8    icon_alpha = selected ? t_alpha : static_cast<u8>(t_alpha * K_TILE_UNSELECTED_ICON_ALPHA / 255);
		const float icon_x     = tile.center().x - K_TILE_ICON_SIZE * 0.5f;
		const Rect  icon{icon_x, tile.y + K_TILE_PADDING_Y, K_TILE_ICON_SIZE, K_TILE_ICON_SIZE};
		draw_game_icon(t_draw_list, game, icon, K_TILE_ICON_RADIUS, icon_alpha);

		const std::string_view name       = game.short_title.empty() ? game.title : game.short_title;
		const float            name_room  = tile.w - K_TILE_LABEL_INSET * 2.0f;
		const float            name_width = std::min(text_width(secondary, name), name_room);
		const Vec2             name_baseline{snapped_to_pixel(tile.center().x - name_width * 0.5f), icon.bottom() + K_TILE_LABEL_GAP + secondary.ascent};
		const Color            name_color = selected || tile_hovered ? g_theme.text : g_theme.text_dim;

		draw_text_truncated(t_draw_list, secondary, name_baseline, name, name_room, faded(name_color, t_alpha));
	}

	t_draw_list->pop_clip();
}

auto AccountModal::draw_form_footer(DrawList* t_draw_list, Rect t_footer, u8 t_alpha) const -> void
{
	const Font& body      = m_fonts->body;
	const Font& secondary = m_fonts->secondary;
	const Rect  save      = primary_button_rect(t_footer);
	const Rect  cancel    = cancel_button_rect(save);
	const float cap       = controls::keycap_height(secondary);
	const float cap_y     = snapped_to_pixel(t_footer.center().y - cap * 0.5f);
	const Rect  cap_row{t_footer.x, cap_y, t_footer.w, cap};
	const float baseline    = secondary.centered_baseline(cap_row);
	const Color hint        = faded(g_theme.text_faint, t_alpha);
	const float enter_hint  = controls::keycap_width(secondary, "Enter") + K_HINT_KEY_GAP + text_width(secondary, "save");
	const float escape_hint = controls::keycap_width(secondary, "Esc") + K_HINT_KEY_GAP + text_width(secondary, "cancel");
	float       x           = t_footer.x + K_ROW_PADDING;

	if (x + enter_hint + K_HINT_GAP + escape_hint <= cancel.x - K_HINT_GAP) {
		for (const auto& [key, action] : {std::pair{"Enter", "save"}, std::pair{"Esc", "cancel"}}) {
			const float width = controls::keycap_width(secondary, key);
			const Rect  keycap{snapped_to_pixel(x), cap_y, width, cap};

			controls::draw_keycap(t_draw_list, secondary, keycap, key, g_theme.surface, t_alpha);
			x += width + K_HINT_KEY_GAP;

			const Vec2 action_baseline{snapped_to_pixel(x), baseline};
			draw_text(t_draw_list, secondary, action_baseline, action, hint);
			x += text_width(secondary, action) + K_HINT_GAP;
		}
	}

	const Color accent         = m_settings->accent;
	const bool  can_save       = m_form.has_changes(*m_library);
	const bool  save_hovered   = save.contains(m_mouse);
	const bool  cancel_hovered = cancel.contains(m_mouse);

	controls::draw_button(t_draw_list, body, save, save_label(), controls::ButtonStyle::ACCENT, accent, can_save, save_hovered, t_alpha);
	controls::draw_button(t_draw_list, body, cancel, "Cancel", controls::ButtonStyle::GHOST, accent, true, cancel_hovered, t_alpha);
}

auto AccountModal::draw_footer(DrawList* t_draw_list, Rect t_footer, u8 t_alpha) const -> void
{
	if (m_mode == Mode::ACCOUNT_LIST) return;

	const Rect top_line{t_footer.x, t_footer.y, t_footer.w, 1.0f};
	t_draw_list->add_rect(top_line, faded(g_theme.separator, t_alpha));

	if (m_mode == Mode::EDIT_ACCOUNT) {
		draw_form_footer(t_draw_list, t_footer, t_alpha);
		return;
	}

	const Font& body            = m_fonts->body;
	const Color accent          = m_settings->accent;
	const Rect  primary         = primary_button_rect(t_footer);
	const bool  primary_hovered = primary.contains(m_mouse);

	if (asks_for_permission()) {
		const Rect back         = cancel_button_rect(primary);
		const bool back_hovered = back.contains(m_mouse);

		controls::draw_button(t_draw_list, body, primary, K_PERMISSION_BUTTON_LABEL, controls::ButtonStyle::ACCENT, accent, true, primary_hovered, t_alpha);
		controls::draw_button(t_draw_list, body, back, "Back", controls::ButtonStyle::GHOST, accent, true, back_hovered, t_alpha);
		return;
	}

	const std::string_view label = m_session->is_finished() ? "Back" : "Cancel";
	controls::draw_button(t_draw_list, body, primary, label, controls::ButtonStyle::NEUTRAL, accent, true, primary_hovered, t_alpha);
}

auto AccountModal::draw(DrawList* t_draw_list) -> void
{
	PULSAR_PROFILE_SCOPE("AccountModal.Draw");

	const bool art_visible = m_art_source && m_morph_progress > 0.0f;
	if ((m_open_amount <= 0.001f && !art_visible) || !m_list.has_game()) return;

	const u8   alpha  = to_alpha(m_open_amount);
	const Vec2 window = m_window->size();
	const bool docked = is_docked();

	if (!docked) {
		const Rect whole_window{0.0f, 0.0f, window.x, window.y};
		controls::draw_popup_backdrop(t_draw_list, whole_window, m_open_amount);
	}

	const Layout layout   = this->layout();
	const float  scale    = docked || m_art_source ? 1.0f : K_PANEL_CLOSED_SCALE + (1.0f - K_PANEL_CLOSED_SCALE) * m_open_amount;
	const bool   morphing = m_art_source && m_morph_progress != 1.0f;
	const Vec2   center   = layout.panel.center();

	t_draw_list->push_scale(center, scale);
	draw_chrome(t_draw_list, layout, !morphing, alpha);
	t_draw_list->pop_scale();

	if (morphing) {
		draw_morphing_art(t_draw_list, layout, scale);
	}

	t_draw_list->push_scale(center, scale);
	draw_back_badge(t_draw_list, layout, alpha);

	switch (m_mode) {
		using enum Mode;

		case ACCOUNT_LIST: {
			draw_account_list(t_draw_list, layout, alpha);
			break;
		}

		case LOGIN_PROGRESS: {
			draw_login_progress(t_draw_list, layout.main_column, alpha);
			break;
		}

		case EDIT_ACCOUNT: {
			draw_form(t_draw_list, layout.main_column, alpha);
			break;
		}
	}

	draw_footer(t_draw_list, layout.footer, alpha);

	if (m_mode == Mode::EDIT_ACCOUNT) {
		m_region_list.draw(t_draw_list, m_mouse);
	}

	m_tooltip.draw(t_draw_list, m_fonts, layout.panel, alpha);
	t_draw_list->pop_scale();
}
