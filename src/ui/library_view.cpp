#include "ui/library_view.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <ranges>
#include <span>
#include <utility>

#include "core/animation.h"
#include "core/regions.h"
#include "core/settings.h"
#include "core/str.h"
#include "gfx/assets.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "ui/controls.h"
#include "ui/login_session.h"
#include "ui/text.h"
#include "ui/theme.h"
#include "ui/toasts.h"

namespace {
constexpr float K_PADDING_X              = 28.0f;
constexpr float K_PADDING_TOP            = 20.0f;
constexpr float K_PADDING_BOTTOM         = 16.0f;
constexpr float K_HEADER_GAP             = 16.0f;
constexpr float K_TITLE_GAP              = 16.0f;
constexpr float K_LABELS_PADDING_Y       = 4.0f;
constexpr float K_ROWS_BLEED             = 8.0f;
constexpr float K_FILTER_SHARE           = 0.28f;
constexpr float K_FILTER_MIN_WIDTH       = 120.0f;
constexpr float K_FILTER_MAX_WIDTH       = 200.0f;
constexpr float K_FILTER_INSET           = 10.0f;
constexpr float K_CONTROL_HEIGHT         = 30.0f;
constexpr float K_CONTROL_GAP            = 8.0f;
constexpr float K_CONTROL_RADIUS         = 8.0f;
constexpr float K_ADD_PADDING_X          = 12.0f;
constexpr float K_ADD_ICON_SIZE          = 14.0f;
constexpr float K_ADD_ICON_GAP           = 6.0f;
constexpr float K_ROW_PADDING_Y          = 9.0f;
constexpr float K_ROW_LINE_GAP           = 3.0f;
constexpr float K_ROW_RADIUS             = 8.0f;
constexpr float K_ROW_INSET              = 10.0f;
constexpr float K_STAR_SIZE              = 24.0f;
constexpr float K_STAR_ICON_INSET        = 4.0f;
constexpr float K_ROW_HOVER_SHARE        = 0.3f;
constexpr float K_HIGHLIGHT_INSET_Y      = 2.0f;
constexpr float K_SELECTED_ALPHA         = 38.0f;
constexpr float K_COLUMN_GAP             = 16.0f;
constexpr float K_MIN_ACCOUNT_WIDTH      = 140.0f;
constexpr float K_PILL_PADDING_X         = 12.0f;
constexpr float K_PILL_HEIGHT            = 26.0f;
constexpr float K_PILL_RADIUS            = 7.0f;
constexpr float K_PILL_SLIDE             = 6.0f;
constexpr float K_ACTION_GAP             = 6.0f;
constexpr float K_CHECK_SIZE             = 18.0f;
constexpr float K_PROGRESS_HEIGHT        = 2.0f;
constexpr float K_PROGRESS_INSET         = 12.0f;
constexpr float K_PROGRESS_BOTTOM        = 3.0f;
constexpr float K_STATUS_RISE            = 4.0f;
constexpr float K_STATUS_TINT            = 0.55f;
constexpr u8    K_FLASH_ALPHA            = 46;
constexpr float K_SCROLLBAR_MARGIN       = 4.0f;
constexpr float K_HOVER_EASE_RATE        = 20.0f;
constexpr float K_APPEAR_SECONDS         = 0.32f;
constexpr float K_ROW_STAGGER            = 0.06f;
constexpr u32   K_STAGGERED_ROWS         = 6;
constexpr float K_ROW_RISE               = 6.0f;
constexpr float K_DRAG_THRESHOLD         = 4.0f;
constexpr float K_AUTO_SCROLL_ZONE       = 0.6f;
constexpr float K_AUTO_SCROLL_SPEED      = 540.0f;
constexpr float K_FORM_EASE_RATE         = 16.0f;
constexpr float K_FORM_PADDING           = 14.0f;
constexpr float K_FORM_RADIUS            = 10.0f;
constexpr float K_FORM_TITLE_GAP         = 12.0f;
constexpr float K_FORM_COLUMN_GAP        = 12.0f;
constexpr float K_FORM_ROW_GAP           = 10.0f;
constexpr float K_FORM_LABEL_GAP         = 4.0f;
constexpr float K_FORM_FOOTER_GAP        = 14.0f;
constexpr float K_FORM_TWO_COLUMNS       = 380.0f;
constexpr float K_FORM_ICON_SIZE         = 14.0f;
constexpr float K_FORM_ICON_GAP          = 8.0f;
constexpr float K_INPUT_MIN_HEIGHT       = 32.0f;
constexpr float K_INPUT_TEXT_ROOM        = 12.0f;
constexpr float K_INPUT_RADIUS           = 7.0f;
constexpr float K_INPUT_PADDING_X        = 10.0f;
constexpr float K_INPUT_FOCUS_BORDER     = 1.5f;
constexpr float K_FOCUS_GLOW_BLUR        = 8.0f;
constexpr u8    K_FOCUS_GLOW_ALPHA       = 60;
constexpr float K_ACTIVE_LABEL_TINT      = 0.6f;
constexpr float K_REVEAL_SIZE            = 22.0f;
constexpr float K_REVEAL_MARGIN          = 6.0f;
constexpr float K_CHIP_SHRINK            = 6.0f;
constexpr float K_CHIP_PADDING_X         = 10.0f;
constexpr float K_CHIP_GAP               = 6.0f;
constexpr float K_CHIP_RADIUS            = 7.0f;
constexpr float K_SHOWN_CHIP_TINT        = 0.18f;
constexpr float K_SHOWN_BORDER_TINT      = 0.6f;
constexpr u8    K_HIDDEN_ICON_ALPHA      = 130;
constexpr float K_GAME_CHIP_ICON         = 16.0f;
constexpr float K_GAME_CHIP_ICON_GAP     = 6.0f;
constexpr float K_GAME_CHIP_ICON_RADIUS  = 4.0f;
constexpr float K_BUTTON_HEIGHT          = 28.0f;
constexpr float K_BUTTON_PADDING_X       = 16.0f;
constexpr float K_BUTTON_MIN_WIDTH       = 72.0f;
constexpr float K_BUTTON_GAP             = 8.0f;
constexpr float K_SUCCESS_LINGER_SECONDS = 2.5f;
constexpr float K_FAILURE_LINGER_SECONDS = 8.0f;
constexpr float K_FLASH_SECONDS          = 1.2f;
constexpr float K_EMPTY_LINE_GAP         = 4.0f;
constexpr float K_ZONE_MIN_HEIGHT        = 160.0f;
constexpr u32   K_FILTER_MAX_LENGTH      = 64;
constexpr Color K_COLOR_IMAGE{255, 255, 255, 255};

constexpr float K_CHEVRON_MARGIN = 12.0f;
constexpr Vec2  K_CHEVRON_SIZE{9.0f, 5.0f};

constexpr std::string_view                       K_LOGIN_LABEL      = "Login";
constexpr std::string_view                       K_CANCEL_LABEL     = "Cancel";
constexpr std::string_view                       K_RETRY_LABEL      = "Try again";
constexpr std::string_view                       K_PERMISSION_LABEL = "Open settings";
constexpr std::string_view                       K_ADD_LABEL        = "Add account";
constexpr std::string_view                       K_OPTIONAL_SUFFIX  = " (optional)";
constexpr std::string_view                       K_ZONE_TITLE       = "Add your first account";
constexpr std::string_view                       K_ZONE_HINT        = "Save a Riot login once, then sign in with one click.";
constexpr std::string_view                       K_ESCAPE_HINT      = "Esc to cancel";
constexpr EnumArray<EditField, std::string_view> K_FIELD_LABELS{{"Note", "Username", "Password"}};

constexpr EditField K_FIELD_ORDER[]{EditField::USERNAME, EditField::PASSWORD, EditField::NOTE};

[[nodiscard]] auto eased_out(float t_amount) -> float
{
	return 1.0f - std::pow(1.0f - t_amount, 3.0f);
}

[[nodiscard]] auto scaled_alpha(u8 t_alpha, float t_amount) -> u8
{
	return static_cast<u8>(static_cast<float>(t_alpha) * std::clamp(t_amount, 0.0f, 1.0f));
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

auto draw_pill(DrawList* t_draw_list, const Font& t_font, Rect t_rect, std::string_view t_label, Color t_fill, Color t_ink, u8 t_alpha) -> void
{
	t_draw_list->add_rounded_rect(t_rect, rounded(K_PILL_RADIUS), faded(t_fill, t_alpha));
	draw_text_centered(t_draw_list, t_font, t_rect, t_label, faded(t_ink, t_alpha));
}

auto draw_optional_label(DrawList* t_draw_list, const Font& t_font, Rect t_label, std::string_view t_text, Color t_color, u8 t_alpha) -> void
{
	const Vec2 text_at{t_label.x, t_label.y + t_font.ascent};
	const Vec2 suffix_at{text_at.x + text_width(t_font, t_text), text_at.y};

	draw_text(t_draw_list, t_font, text_at, t_text, faded(t_color, t_alpha));
	draw_text(t_draw_list, t_font, suffix_at, K_OPTIONAL_SUFFIX, faded(g_theme.text_faint, t_alpha));
}
}

auto LibraryView::is_row_target(Target t_target) -> bool
{
	switch (t_target) {
		using enum Target;

		case ROW:
		case STAR:
		case LOGIN:
		case CANCEL_LOGIN:
		case PERMISSION: {
			return true;
		}

		default: {
			return false;
		}
	}
}

LibraryView::LibraryView(Library* t_library, const Settings* t_settings, const Fonts* t_fonts, const Assets* t_assets, Toasts* t_toasts, LoginSession* t_session, CommandQueue* t_commands)
	: m_library(t_library)
	, m_settings(t_settings)
	, m_fonts(t_fonts)
	, m_assets(t_assets)
	, m_toasts(t_toasts)
	, m_session(t_session)
	, m_commands(t_commands)
	, m_list(t_library, t_session, t_toasts, CommandType::UNDO_LIBRARY_DELETE)
	, m_region_list(t_fonts, t_assets, t_settings, ListPopupOptions{.empty_message = "No regions"})
{
	m_filter.set_max_length(K_FILTER_MAX_LENGTH);
	m_filter.m_placeholder = "Filter accounts";
}

auto LibraryView::show_game(std::optional<u32> t_game) -> void
{
	if (t_game == m_list.m_game) return;

	close_form(false);
	m_list.reset_motion();
	unfocus();

	m_list.m_game = t_game;
	m_list.m_selection.clear();
	m_list.set_query("");
	m_filter.set_value("");

	m_appear     = 0.0f;
	m_scroll     = Scrollable{};
	m_pressed    = Hit{};
	m_hovered    = Hit{};
	m_zone_hover = 0.0f;
	m_flash_account.reset();
}

auto LibraryView::layout() const -> LibraryView::Layout
{
	const Font& title     = m_fonts->title;
	const Font& body      = m_fonts->body;
	const Font& secondary = m_fonts->secondary;
	const Font& caption   = m_fonts->caption;
	const float left      = m_bounds.x + K_PADDING_X;
	const float width     = std::max(0.0f, m_bounds.w - K_PADDING_X * 2.0f);

	Layout layout{};

	const float header_height = std::max(title.line_height(), K_CONTROL_HEIGHT);
	layout.header             = Rect{left, m_bounds.y + K_PADDING_TOP, width, header_height};
	layout.title_baseline     = title.centered_baseline(layout.header);

	const float add_width    = K_ADD_PADDING_X * 2.0f + K_ADD_ICON_SIZE + K_ADD_ICON_GAP + text_width(secondary, K_ADD_LABEL);
	const float filter_width = std::clamp(width * K_FILTER_SHARE, K_FILTER_MIN_WIDTH, K_FILTER_MAX_WIDTH);
	const float control_y    = snapped_to_pixel(layout.header.center().y - K_CONTROL_HEIGHT * 0.5f);

	layout.add    = Rect{layout.header.right() - add_width, control_y, add_width, K_CONTROL_HEIGHT};
	layout.filter = Rect{layout.add.x - K_CONTROL_GAP - filter_width, control_y, filter_width, K_CONTROL_HEIGHT};

	const float labels_top    = layout.header.bottom() + K_HEADER_GAP;
	const float labels_height = caption.line_height() + K_LABELS_PADDING_Y * 2.0f;
	layout.labels             = Rect{left - K_ROWS_BLEED, labels_top, width + K_ROWS_BLEED * 2.0f, labels_height};

	const float rows_top    = layout.labels.bottom();
	const float rows_height = std::max(0.0f, m_bounds.bottom() - rows_top);
	const float text_height = body.line_height() + K_ROW_LINE_GAP + secondary.line_height();
	const float content     = std::max(text_height, K_PILL_HEIGHT);
	layout.rows_area        = Rect{layout.labels.x, rows_top, layout.labels.w, rows_height};
	layout.row_height       = snapped_to_pixel(content + K_ROW_PADDING_Y * 2.0f);
	layout.columns          = columns(layout.rows_area.w);

	return layout;
}

auto LibraryView::columns(float t_row_width) const -> LibraryView::Columns
{
	const Font& secondary   = m_fonts->secondary;
	const Font& caption     = m_fonts->caption;
	const float login_width = text_width(secondary, K_LOGIN_LABEL) + K_PILL_PADDING_X * 2.0f;

	Columns columns{};
	columns.account      = K_ROW_INSET;
	columns.star         = t_row_width - K_ROW_INSET - login_width - K_ACTION_GAP - K_STAR_SIZE;
	columns.actions      = columns.star;
	columns.played_width = std::max(text_width(caption, "Last played"), text_width(secondary, "11 months ago"));
	columns.region_width = std::max(text_width(caption, "Region"), controls::region_chip_width(secondary, "EUNE"));
	columns.show_region  = true;
	columns.show_played  = true;

	columns.played        = columns.actions - K_COLUMN_GAP - columns.played_width;
	columns.region        = columns.played - K_COLUMN_GAP - columns.region_width;
	columns.account_width = columns.region - K_COLUMN_GAP - columns.account;

	if (columns.account_width < K_MIN_ACCOUNT_WIDTH) {
		columns.show_played   = false;
		columns.region        = columns.actions - K_COLUMN_GAP - columns.region_width;
		columns.account_width = columns.region - K_COLUMN_GAP - columns.account;
	}

	if (columns.account_width < K_MIN_ACCOUNT_WIDTH) {
		columns.show_region   = false;
		columns.account_width = columns.actions - K_COLUMN_GAP - columns.account;
	}

	columns.account_width = std::max(0.0f, columns.account_width);

	return columns;
}

auto LibraryView::first_row_y(const Layout& t_layout) const -> float
{
	return t_layout.rows_area.y - m_scroll.offset();
}

auto LibraryView::pointer_in_list(const Layout& t_layout, float t_y) const -> float
{
	return t_y - first_row_y(t_layout);
}

auto LibraryView::lifted_top(const Layout& t_layout) const -> float
{
	return m_list.lifted_top(pointer_in_list(t_layout, m_mouse.y), t_layout.row_height);
}

auto LibraryView::form_shown() const -> bool
{
	return m_form_open || m_form_amount > 0.0f;
}

auto LibraryView::form_row(const VisibleAccounts& t_accounts) const -> std::optional<u32>
{
	if (!form_shown() || !m_form.m_account) return std::nullopt;

	return t_accounts.row_of(*m_form.m_account);
}

auto LibraryView::form_growth(const Layout& t_layout) const -> float
{
	if (!form_shown()) return 0.0f;
	if (!m_form.m_account) return m_form_height * m_form_amount;

	return std::max(0.0f, m_form_height - t_layout.row_height) * m_form_amount;
}

auto LibraryView::form_card(const Layout& t_layout, const VisibleAccounts& t_accounts) const -> Rect
{
	const Rect  area = t_layout.rows_area;
	const float top  = first_row_y(t_layout);

	if (!m_form.m_account) {
		const float opened_height = m_form_height * m_form_amount;
		return Rect{area.x, top, area.w, opened_height};
	}

	const std::optional<u32> row = form_row(t_accounts);
	if (!row) return Rect{};

	const float row_y         = top + static_cast<float>(*row) * t_layout.row_height + m_list.m_offsets[*row];
	const float growth        = std::max(0.0f, m_form_height - t_layout.row_height) * m_form_amount;
	const float opened_height = t_layout.row_height + growth;

	return Rect{area.x, row_y, area.w, opened_height};
}

auto LibraryView::reveal_form(const Layout& t_layout, const VisibleAccounts& t_accounts) -> void
{
	const Rect  card        = form_card(t_layout, t_accounts);
	const float open_growth = m_form.m_account ? std::max(0.0f, m_form_height - t_layout.row_height) : m_form_height;
	const float card_bottom = card.y + std::min(m_form_height, t_layout.rows_area.h);

	ScrollGeometry geometry = scroll_geometry(t_layout, t_accounts);
	geometry.content_height += open_growth * (1.0f - m_form_amount);

	m_scroll.reveal(card.y, card_bottom, t_layout.rows_area.y, t_layout.rows_area.bottom(), geometry);
}

auto LibraryView::form_layout(Rect t_card) const -> LibraryView::FormLayout
{
	const Font& body      = m_fonts->body;
	const Font& secondary = m_fonts->secondary;
	const float width     = std::max(0.0f, t_card.w - K_FORM_PADDING * 2.0f);
	const Rect  inner{t_card.x + K_FORM_PADDING, t_card.y + K_FORM_PADDING, width, 0.0f};

	FormLayout form{};
	form.title = Rect{inner.x, inner.y, inner.w, body.line_height()};

	const bool  two_columns  = inner.w >= K_FORM_TWO_COLUMNS;
	const float column       = two_columns ? (inner.w - K_FORM_COLUMN_GAP) * 0.5f : inner.w;
	const float second_x     = two_columns ? inner.x + column + K_FORM_COLUMN_GAP : inner.x;
	const float label_height = secondary.line_height();
	const float input_height = std::max(K_INPUT_MIN_HEIGHT, body.line_height() + K_INPUT_TEXT_ROOM);
	const float block        = label_height + K_FORM_LABEL_GAP + input_height;

	const auto place = [&](EditField t_field, float t_x, float t_y) {
		form.labels[t_field] = Rect{t_x, t_y, column, label_height};
		form.fields[t_field] = Rect{t_x, t_y + label_height + K_FORM_LABEL_GAP, column, input_height};
	};

	float y = form.title.bottom() + K_FORM_TITLE_GAP;

	place(EditField::USERNAME, inner.x, y);
	if (!two_columns) {
		y += block + K_FORM_ROW_GAP;
	}

	place(EditField::PASSWORD, second_x, y);
	y += block + K_FORM_ROW_GAP;

	place(EditField::NOTE, inner.x, y);
	if (!two_columns) {
		y += block + K_FORM_ROW_GAP;
	}

	form.region_label = Rect{second_x, y, column, label_height};
	form.region       = Rect{second_x, y + label_height + K_FORM_LABEL_GAP, column, input_height};

	const float chip_height = input_height - K_CHIP_SHRINK;
	y                       = std::max(form.fields[EditField::NOTE].bottom(), form.region.bottom()) + K_FORM_ROW_GAP;

	form.has_games = m_library->game_count > 1;
	if (form.has_games) {
		form.show_in_label = Rect{inner.x, y, inner.w, label_height};

		float chip_x = inner.x;
		float chip_y = y + label_height + K_FORM_LABEL_GAP;

		for (const u32 game : std::views::iota(0u, m_library->game_count)) {
			const float title_width = text_width(secondary, m_library->games[game].short_title);
			const float chip_width  = K_CHIP_PADDING_X * 2.0f + K_GAME_CHIP_ICON + K_GAME_CHIP_ICON_GAP + title_width;

			if (chip_x > inner.x && chip_x + chip_width > inner.right()) {
				chip_x = inner.x;
				chip_y += chip_height + K_CHIP_GAP;
			}

			form.games[game] = Rect{chip_x, chip_y, chip_width, chip_height};
			chip_x += chip_width + K_CHIP_GAP;
		}

		y = chip_y + chip_height + K_FORM_ROW_GAP;
	}

	y += K_FORM_FOOTER_GAP - K_FORM_ROW_GAP;

	const float save_width   = std::max(K_BUTTON_MIN_WIDTH, text_width(body, "Save") + K_BUTTON_PADDING_X * 2.0f);
	const float cancel_width = std::max(K_BUTTON_MIN_WIDTH, text_width(body, "Cancel") + K_BUTTON_PADDING_X * 2.0f);
	form.save                = Rect{inner.right() - save_width, y, save_width, K_BUTTON_HEIGHT};
	form.cancel              = Rect{form.save.x - K_BUTTON_GAP - cancel_width, y, cancel_width, K_BUTTON_HEIGHT};

	const Rect  password = form.fields[EditField::PASSWORD];
	const float reveal_y = snapped_to_pixel(password.center().y - K_REVEAL_SIZE * 0.5f);
	form.reveal          = Rect{password.right() - K_REVEAL_SIZE - K_REVEAL_MARGIN, reveal_y, K_REVEAL_SIZE, K_REVEAL_SIZE};
	form.height          = form.save.bottom() + K_FORM_PADDING - t_card.y;

	return form;
}

auto LibraryView::field_text_rect(const FormLayout& t_form, EditField t_field) const -> Rect
{
	Rect text = t_form.fields[t_field].inset(K_INPUT_PADDING_X, 0.0f);

	if (t_field == EditField::PASSWORD) {
		text.w = std::max(0.0f, text.w - K_REVEAL_SIZE);
	}

	return text;
}

auto LibraryView::row_rect(const Layout& t_layout, const VisibleAccounts& t_accounts, u32 t_row) const -> Rect
{
	const float own_y = first_row_y(t_layout) + static_cast<float>(t_row) * t_layout.row_height + m_list.m_offsets[t_row];

	const std::optional<u32> edited = form_row(t_accounts);
	const bool               pushed = form_shown() && (!m_form.m_account || (edited && t_row > *edited));
	const float              y      = pushed ? own_y + form_growth(t_layout) : own_y;

	return Rect{t_layout.rows_area.x, y, t_layout.rows_area.w, t_layout.row_height};
}

auto LibraryView::dragged_rect(const Layout& t_layout, u32 t_row) const -> Rect
{
	const float offset_in_block = m_list.m_drag.rows_from_held(t_row) * t_layout.row_height;
	const float top_in_list     = lifted_top(t_layout) + offset_in_block + m_list.m_offsets[t_row];

	return Rect{t_layout.rows_area.x, first_row_y(t_layout) + top_in_list, t_layout.rows_area.w, t_layout.row_height};
}

auto LibraryView::star_rect(const Layout& t_layout, Rect t_row) const -> Rect
{
	const float x = t_row.x + t_layout.columns.star;
	const float y = snapped_to_pixel(t_row.center().y - K_STAR_SIZE * 0.5f);

	return Rect{x, y, K_STAR_SIZE, K_STAR_SIZE};
}

auto LibraryView::login_rect(Rect t_row, std::string_view t_label) const -> Rect
{
	const float width = text_width(m_fonts->secondary, t_label) + K_PILL_PADDING_X * 2.0f;
	const float x     = t_row.right() - K_ROW_INSET - width;
	const float y     = snapped_to_pixel(t_row.center().y - K_PILL_HEIGHT * 0.5f);

	return Rect{x, y, width, K_PILL_HEIGHT};
}

auto LibraryView::content_height(const Layout& t_layout, const VisibleAccounts& t_accounts) const -> float
{
	const float rows_height = static_cast<float>(t_accounts.count) * t_layout.row_height;

	return rows_height + form_growth(t_layout) + K_PADDING_BOTTOM;
}

auto LibraryView::scroll_geometry(const Layout& t_layout, const VisibleAccounts& t_accounts) const -> ScrollGeometry
{
	const Rect  area         = t_layout.rows_area;
	const float track_x      = m_bounds.right() - K_SCROLLBAR_WIDTH - K_SCROLLBAR_MARGIN;
	const float track_height = std::max(0.0f, area.h - K_SCROLLBAR_MARGIN * 2.0f);
	const Rect  track{track_x, area.y + K_SCROLLBAR_MARGIN, K_SCROLLBAR_WIDTH, track_height};

	return ScrollGeometry{track, content_height(t_layout, t_accounts), area.h};
}

auto LibraryView::empty_zone(const Layout& t_layout) const -> Rect
{
	const float lowest = std::max(t_layout.labels.y + K_ZONE_MIN_HEIGHT, m_bounds.bottom() - K_PADDING_BOTTOM);
	const float left   = snapped_to_pixel(t_layout.rows_area.x);
	const float top    = snapped_to_pixel(t_layout.labels.y);
	const float right  = snapped_to_pixel(t_layout.rows_area.right());
	const float bottom = snapped_to_pixel(lowest);

	return Rect{left, top, right - left, bottom - top};
}

auto LibraryView::marquee_area(const Layout& t_layout) const -> Rect
{
	return Rect{m_bounds.x, t_layout.rows_area.y, m_bounds.w, t_layout.rows_area.h};
}

auto LibraryView::shows_login(AccountRef t_account) const -> bool
{
	if (!m_list.has_game() || !m_session->shows(*m_list.m_game, t_account)) return false;
	if (!m_session->is_finished() || m_session->asks_for_permission()) return true;

	const float linger = m_session->stage() == LoginStage::SUCCESS ? K_SUCCESS_LINGER_SECONDS : K_FAILURE_LINGER_SECONDS;

	return m_session->m_finished_seconds < linger;
}

auto LibraryView::login_button_label(AccountRef t_account) const -> std::string_view
{
	if (!shows_login(t_account)) return "";
	if (!m_session->is_finished()) return K_CANCEL_LABEL;
	if (m_session->asks_for_permission()) return K_PERMISSION_LABEL;
	if (m_session->stage() == LoginStage::SUCCESS) return "";

	return K_RETRY_LABEL;
}

auto LibraryView::hit_at(Vec2 t_point) const -> LibraryView::Hit
{
	if (!m_list.has_game() || !m_bounds.contains(t_point)) return Hit{};

	const Layout layout    = this->layout();
	const bool   filtering = !m_filter.value().empty();

	if (filtering && controls::search_clear_rect(layout.filter).contains(t_point)) return Hit{Target::FILTER_CLEAR};
	if (layout.filter.contains(t_point)) return Hit{Target::FILTER};
	if (layout.add.contains(t_point)) return Hit{Target::ADD};

	const VisibleAccounts accounts = m_list.rows();

	if (accounts.count == 0 && !form_shown()) {
		const bool on_zone = !filtering && empty_zone(layout).contains(t_point);
		return on_zone ? Hit{Target::ADD} : Hit{};
	}

	const ScrollGeometry geometry = scroll_geometry(layout, accounts);
	if (!layout.rows_area.contains(t_point) || m_scroll.is_over_track(t_point, geometry)) return Hit{};

	if (form_shown()) {
		const Rect card       = form_card(layout, accounts);
		const bool form_ready = m_form_open && m_form_amount > 0.5f;
		if (card.contains(t_point)) return form_ready ? form_hit(card, t_point) : Hit{Target::FORM};
	}

	const std::optional<u32> edited = form_row(accounts);

	for (const u32 row : std::views::iota(0u, accounts.count)) {
		if (row == edited) continue;

		const Rect rect = row_rect(layout, accounts, row);
		if (rect.contains(t_point)) return row_hit(layout, rect, accounts.refs[row], row, t_point);
	}

	return Hit{};
}

auto LibraryView::row_hit(const Layout& t_layout, Rect t_row, AccountRef t_account, u32 t_index, Vec2 t_point) const -> LibraryView::Hit
{
	if (shows_login(t_account)) {
		const std::string_view label = login_button_label(t_account);
		if (label.empty() || !login_rect(t_row, label).contains(t_point)) return Hit{Target::ROW, t_index};
		if (label == K_CANCEL_LABEL) return Hit{Target::CANCEL_LOGIN, t_index};
		if (label == K_PERMISSION_LABEL) return Hit{Target::PERMISSION, t_index};

		return Hit{Target::LOGIN, t_index};
	}

	if (star_rect(t_layout, t_row).contains(t_point)) return Hit{Target::STAR, t_index};
	if (login_rect(t_row, K_LOGIN_LABEL).contains(t_point)) return Hit{Target::LOGIN, t_index};

	return Hit{Target::ROW, t_index};
}

auto LibraryView::form_hit(Rect t_card, Vec2 t_point) const -> LibraryView::Hit
{
	const FormLayout form = form_layout(t_card);

	if (form.reveal.contains(t_point)) return Hit{Target::REVEAL};

	for (const EditField field : each<EditField>()) {
		if (form.fields[field].contains(t_point)) return Hit{.target = Target::FIELD, .field = field};
	}

	if (form.region.contains(t_point)) return Hit{Target::REGION};

	if (form.has_games) {
		for (const u32 game : std::views::iota(0u, m_library->game_count)) {
			if (form.games[game].contains(t_point)) return Hit{Target::SHOW_IN, game};
		}
	}

	if (form.cancel.contains(t_point)) return Hit{Target::CANCEL_FORM};
	if (form.save.contains(t_point)) return Hit{Target::SAVE};

	return Hit{Target::FORM};
}

auto LibraryView::row_entrance(u32 t_row) const -> float
{
	constexpr float SPAN = 1.0f - K_ROW_STAGGER * K_STAGGERED_ROWS;

	const float delay = static_cast<float>(std::min(t_row, K_STAGGERED_ROWS)) * K_ROW_STAGGER;
	const float shown = std::clamp((m_appear - delay) / SPAN, 0.0f, 1.0f);

	return eased_out(shown);
}

auto LibraryView::can_select() const -> bool
{
	return m_list.shows_every_account() && !form_shown();
}

auto LibraryView::click_target(Hit t_hit) -> void
{
	const VisibleAccounts accounts = m_list.rows();
	const bool            on_row   = is_row_target(t_hit.target) && t_hit.index < accounts.count;
	const AccountRef      account  = on_row ? accounts.refs[t_hit.index] : AccountRef{};

	switch (t_hit.target) {
		using enum Target;

		case LOGIN: {
			if (on_row) {
				request_login(account);
			}

			break;
		}

		case STAR: {
			if (on_row) {
				toggle_favorite(account);
			}

			break;
		}

		case CANCEL_LOGIN: {
			m_session->cancel();
			break;
		}

		case PERMISSION: {
			m_commands->push(Command{.type = CommandType::OPEN_PERMISSION_SETTINGS});
			break;
		}

		case ADD: {
			open_form(std::nullopt);
			break;
		}

		case FILTER_CLEAR: {
			m_filter.set_value("");
			refresh_filter();
			break;
		}

		case REVEAL: {
			m_form.toggle_password_shown();
			break;
		}

		case REGION: {
			open_region_list();
			break;
		}

		case SHOW_IN: {
			if (t_hit.index < m_library->game_count) {
				m_form.toggle_game(t_hit.index);
			}

			break;
		}

		case CANCEL_FORM: {
			close_form(true);
			break;
		}

		case SAVE: {
			save_form();
			break;
		}

		case ROW: {
			m_list.m_selection.clear();
			break;
		}

		case NONE:
		case FILTER:
		case FIELD:
		case FORM: {
			break;
		}
	}
}

auto LibraryView::request_login(AccountRef t_account) -> void
{
	if (!m_list.has_game()) return;

	unfocus();
	m_session->request(*m_list.m_game, t_account);
}

auto LibraryView::toggle_favorite(AccountRef t_account) -> void
{
	m_list.toggle_favorite(t_account, layout().row_height);
}

auto LibraryView::edit(AccountRef t_account) -> void
{
	if (m_library->find_account(t_account) == nullptr) return;

	if (!m_list.rows().row_of(t_account)) {
		m_filter.set_value("");
		refresh_filter();
	}

	open_form(t_account);
}

auto LibraryView::open_form(std::optional<AccountRef> t_account) -> void
{
	if (!m_list.has_game()) return;

	if (m_form_open && m_form.m_account == t_account) {
		m_form.focus(EditField::USERNAME);
		return;
	}

	cancel_drag();
	m_list.m_selection.clear();

	if (form_shown()) {
		close_form(false);
	}

	m_form_open         = true;
	m_form_amount       = 0.0f;
	m_keep_form_in_view = true;
	m_region_list.close();
	m_form.start(*m_library, t_account, *m_list.m_game);
	focus_filter(false);

	const Rect measured{0.0f, 0.0f, layout().rows_area.w, 0.0f};
	m_form_height = form_layout(measured).height;
}

auto LibraryView::close_form(bool t_animated) -> void
{
	m_form_open = false;
	m_form.focus(std::nullopt);
	m_region_list.close();

	if (!t_animated) {
		m_form_amount = 0.0f;
		m_form.clear();
	}
}

auto LibraryView::save_form() -> void
{
	if (!m_list.has_game() || !m_form_open) return;

	const bool                                   adding = !m_form.m_account;
	const std::expected<AccountRef, SaveProblem> saved  = m_form.save(m_library, *m_list.m_game);

	if (!saved) {
		if (saved.error() == SaveProblem::NO_ROOM) {
			m_toasts->notify(Notification{.message = "This game can't hold any more accounts."});
		}

		return;
	}

	if (adding) {
		m_session->follow_insert(*saved);
	}

	m_flash_account = *saved;
	m_flash         = 1.0f;
	close_form(true);

	if (adding) {
		reveal_row(*saved);
	}
}

auto LibraryView::delete_account(AccountRef t_account) -> void
{
	delete_accounts(std::span{&t_account, 1});
}

auto LibraryView::delete_picked() -> void
{
	delete_accounts(m_list.take_picked());
}

auto LibraryView::delete_accounts(std::span<const AccountRef> t_accounts) -> void
{
	if (t_accounts.empty()) return;

	const Layout             layout     = this->layout();
	const VisibleAccounts    before     = m_list.rows();
	const std::optional<u32> edited     = form_row(before);
	const float              growth     = form_growth(layout);
	const std::optional<u32> first_gone = before.row_of(t_accounts.front());

	for (const AccountRef removed : m_list.delete_accounts(t_accounts)) {
		shift_after_removal(&m_flash_account, removed);
	}

	close_form(false);
	m_list.slide_from(before, layout.row_height);

	if (edited && first_gone) {
		const u32 shown = m_list.rows().count;

		for (const u32 row : std::views::iota(std::min(*first_gone, shown), shown)) {
			m_list.m_offsets[row] += growth;
		}
	}
}

auto LibraryView::undo_delete() -> void
{
	const float                       row_height = layout().row_height;
	const VisibleAccounts             before     = m_list.rows();
	const std::span<const AccountRef> restored   = m_list.undo_delete();

	for (const AccountRef back : restored) {
		shift_after_insert(&m_flash_account, back);
	}

	if (restored.empty()) return;

	m_list.slide_from(before, row_height);

	if (restored.size() == 1) {
		m_flash_account = restored.front();
		m_flash         = 1.0f;
	} else {
		m_list.select_accounts(restored);
	}

	reveal_row(restored.front());
}

auto LibraryView::forget_secrets() -> void
{
	close_form(false);
	unfocus();
	m_list.forget_secrets();
	m_filter.set_value("");
	m_flash_account.reset();
	m_pressed = Hit{};
}

auto LibraryView::focus_filter(bool t_focused) -> void
{
	m_filter.set_focused(t_focused);

	if (t_focused) {
		m_form.focus(std::nullopt);
	}
}

auto LibraryView::unfocus() -> void
{
	focus_filter(false);
	m_form.focus(std::nullopt);
	m_region_list.close();
}

auto LibraryView::open_region_list() -> void
{
	m_form.focus(std::nullopt);
	m_region_list.open(K_REGION_LABELS, region_index(m_form.m_region));
}

auto LibraryView::refresh_filter() -> void
{
	if (!m_list.set_query(m_filter.value())) return;

	m_list.reset_motion();
	m_scroll  = Scrollable{};
	m_pressed = Hit{};
}

auto LibraryView::reveal_row(AccountRef t_account) -> void
{
	const Layout             layout   = this->layout();
	const VisibleAccounts    accounts = m_list.rows();
	const std::optional<u32> row      = accounts.row_of(t_account);
	if (!row) return;

	const Rect           rect     = row_rect(layout, accounts, *row);
	const ScrollGeometry geometry = scroll_geometry(layout, accounts);
	m_scroll.reveal(rect.y, rect.bottom(), layout.rows_area.y, layout.rows_area.bottom(), geometry);
}

auto LibraryView::lift_row(u32 t_row, Vec2 t_point) -> void
{
	const Layout layout = this->layout();

	m_list.lift(m_list.rows(), t_row, pointer_in_list(layout, t_point.y), layout.row_height);
	m_pressed = Hit{};
	unfocus();
}

auto LibraryView::drop_row() -> void
{
	const Layout layout = this->layout();
	m_list.drop(lifted_top(layout), layout.row_height);
}

auto LibraryView::cancel_drag() -> void
{
	const Layout layout = this->layout();
	m_list.cancel_drag(lifted_top(layout), layout.row_height);
}

auto LibraryView::scroll_near_edges(const Layout& t_layout, const VisibleAccounts& t_accounts, float t_delta_seconds) -> void
{
	const float          zone     = t_layout.row_height * K_AUTO_SCROLL_ZONE;
	const float          above    = t_layout.rows_area.y + zone - m_mouse.y;
	const float          below    = m_mouse.y - (t_layout.rows_area.bottom() - zone);
	const ScrollGeometry geometry = scroll_geometry(t_layout, t_accounts);

	if (above > 0.0f) {
		const float speed = K_AUTO_SCROLL_SPEED * std::min(above / zone, 1.0f);
		m_scroll.scroll_by(-speed * t_delta_seconds, geometry);
	} else if (below > 0.0f) {
		const float speed = K_AUTO_SCROLL_SPEED * std::min(below / zone, 1.0f);
		m_scroll.scroll_by(speed * t_delta_seconds, geometry);
	}
}

auto LibraryView::update_drag(float t_delta_seconds) -> void
{
	const Layout          layout   = this->layout();
	const VisibleAccounts accounts = m_list.rows();

	if (m_list.m_drag.lifted) {
		scroll_near_edges(layout, accounts, t_delta_seconds);
	}

	m_list.update_motion(accounts.count, lifted_top(layout), layout.row_height, t_delta_seconds);
}

// The box reaches over more rows as the list scrolls under it, so it follows the pointer every frame and not only when it moves.
auto LibraryView::update_marquee(float t_delta_seconds) -> void
{
	RowSelection* selection = &m_list.m_selection;
	if (!selection->m_box) return;

	if (!can_select()) {
		selection->cancel_box();
		return;
	}

	const Layout          layout   = this->layout();
	const VisibleAccounts accounts = m_list.rows();

	if (selection->box_shown()) {
		scroll_near_edges(layout, accounts, t_delta_seconds);
		animation::request_frame();
	}

	selection->drag_box(m_mouse, first_row_y(layout), layout.row_height, accounts.count);
}

auto LibraryView::update(float t_delta_seconds) -> void
{
	m_appear = animation::step_toward(m_appear, 1.0f, K_APPEAR_SECONDS, t_delta_seconds);
	m_scroll.update(t_delta_seconds);
	m_filter.update(t_delta_seconds);
	m_form.update(t_delta_seconds);

	refresh_filter();
	m_list.forget_deleted_without_undo();

	const Layout          layout   = this->layout();
	const VisibleAccounts accounts = m_list.rows();

	m_list.m_selection.clear_if_rows_changed(accounts);
	if (!can_select()) {
		m_list.m_selection.clear();
	}

	if (m_form_open && m_form.m_account && !accounts.row_of(*m_form.m_account)) {
		close_form(false);
	}

	if (form_shown()) {
		const Rect  measured      = Rect{layout.rows_area.x, 0.0f, layout.rows_area.w, 0.0f};
		const float target_height = form_layout(measured).height;
		const float target_amount = m_form_open ? 1.0f : 0.0f;

		m_form_height = animation::ease_toward(m_form_height, target_height, K_FORM_EASE_RATE, t_delta_seconds, animation::K_SETTLED_PIXELS);

		const float settled_amount = animation::K_SETTLED_PIXELS / std::max(m_form_height, 1.0f);
		m_form_amount              = animation::ease_toward(m_form_amount, target_amount, K_FORM_EASE_RATE, t_delta_seconds, settled_amount);

		const bool settling = m_form_amount < 1.0f || m_form_height != target_height;
		if (m_form_open && (settling || m_keep_form_in_view)) {
			reveal_form(layout, accounts);
			m_keep_form_in_view = settling;
		}

		if (!m_form_open && m_form_amount == 0.0f) {
			m_form.clear();
		}
	}

	if (m_flash_account) {
		m_flash = animation::step_toward(m_flash, 0.0f, K_FLASH_SECONDS, t_delta_seconds);

		if (m_flash == 0.0f) {
			m_flash_account.reset();
		}
	}

	if (m_session->is_finished() && m_list.has_game()) {
		const float linger = m_session->stage() == LoginStage::SUCCESS ? K_SUCCESS_LINGER_SECONDS : K_FAILURE_LINGER_SECONDS;

		if (m_session->m_finished_seconds < linger) {
			animation::request_frame_after(linger - m_session->m_finished_seconds);
		}
	}

	update_marquee(t_delta_seconds);

	const bool busy = m_scroll.m_dragging || m_list.m_drag.lifted || m_list.m_selection.m_box.has_value();
	m_hovered       = busy ? Hit{} : hit_at(m_mouse);

	const bool on_add   = m_hovered.target == Target::ADD;
	const bool add_lit  = on_add && layout.add.contains(m_mouse);
	const bool zone_lit = on_add && !add_lit;
	m_add_hover         = animation::ease_toward(m_add_hover, hover_target(add_lit), K_HOVER_EASE_RATE, t_delta_seconds);
	m_zone_hover        = animation::ease_toward(m_zone_hover, hover_target(zone_lit), K_HOVER_EASE_RATE, t_delta_seconds);

	const std::optional<u32> hovered_row = is_row_target(m_hovered.target) ? std::optional{m_hovered.index} : std::nullopt;
	m_list.ease_hover(hovered_row, accounts.count, t_delta_seconds);

	update_drag(t_delta_seconds);

	const Rect region_box = form_shown() ? form_layout(form_card(layout, accounts)).region : Rect{};
	m_region_list.update(t_delta_seconds, region_box, m_bounds);
}

auto LibraryView::on_pointer_down(Vec2 t_point) -> void
{
	if (m_region_list.m_open) {
		m_region_list.on_pointer_down(t_point);
		return;
	}

	const Layout          layout   = this->layout();
	const VisibleAccounts accounts = m_list.rows();
	if (m_scroll.on_pointer_down(t_point, scroll_geometry(layout, accounts))) return;

	const Hit hit = hit_at(t_point);
	m_pressed     = hit;

	const bool on_filter = hit.target == Target::FILTER || hit.target == Target::FILTER_CLEAR;
	const bool on_form   = hit.target == Target::FIELD || hit.target == Target::REVEAL || hit.target == Target::FORM;

	if (!on_filter) {
		focus_filter(false);
	}

	if (!on_form) {
		m_form.focus(std::nullopt);
	}

	switch (hit.target) {
		using enum Target;

		case FILTER: {
			const Rect text_area = controls::search_text_rect(layout.filter, K_FILTER_INSET);

			focus_filter(true);
			m_filter.on_pointer_down(m_fonts->secondary, text_area, t_point.x);
			break;
		}

		case FIELD: {
			const FormLayout form      = form_layout(form_card(layout, accounts));
			const Rect       text_area = field_text_rect(form, hit.field);

			m_form.focus(hit.field);
			m_form.m_fields[hit.field].on_pointer_down(m_fonts->body, text_area, t_point.x);
			break;
		}

		case ROW: {
			if (can_select() && m_list.select_with_modifiers(hit.index)) {
				m_pressed = Hit{};
				break;
			}

			m_list.m_drag.pressed_row = hit.index;
			m_list.m_drag.press_point = t_point;
			break;
		}

		case NONE: {
			if (can_select() && accounts.count > 0 && marquee_area(layout).contains(t_point)) {
				const os::Modifiers held     = os::modifiers();
				const bool          additive = held.shortcut || held.shift;
				m_list.m_selection.begin_box(t_point, first_row_y(layout), additive);
			}

			break;
		}

		default: {
			break;
		}
	}
}

auto LibraryView::on_pointer_move(Vec2 t_point) -> bool
{
	if (m_region_list.m_open) {
		m_region_list.on_pointer_move(t_point);
		return true;
	}

	const Layout          layout   = this->layout();
	const VisibleAccounts accounts = m_list.rows();
	bool                  handled  = false;

	if (m_filter.m_selecting) {
		const Rect text_area = controls::search_text_rect(layout.filter, K_FILTER_INSET);
		m_filter.on_pointer_move(m_fonts->secondary, text_area, t_point.x);
		handled = true;
	}

	if (form_shown() && m_form.is_selecting_text()) {
		const FormLayout form = form_layout(form_card(layout, accounts));

		for (const EditField field : each<EditField>()) {
			TextInput* input = &m_form.m_fields[field];
			if (!input->m_selecting) continue;

			input->on_pointer_move(m_fonts->body, field_text_rect(form, field), t_point.x);
			handled = true;
		}
	}

	if (m_scroll.m_dragging) {
		m_scroll.on_pointer_move(t_point.y, scroll_geometry(layout, accounts));
		return true;
	}

	if (m_list.m_selection.m_box) {
		m_list.m_selection.drag_box(t_point, first_row_y(layout), layout.row_height, accounts.count);
		return true;
	}

	const RowDrag& drag = m_list.m_drag;

	if (drag.pressed_row && !drag.lifted && can_select()) {
		const float dx             = t_point.x - drag.press_point.x;
		const float dy             = t_point.y - drag.press_point.y;
		const bool  past_threshold = dx * dx + dy * dy > K_DRAG_THRESHOLD * K_DRAG_THRESHOLD;

		if (past_threshold && *drag.pressed_row < accounts.count) {
			lift_row(*drag.pressed_row, t_point);
		}
	}

	return handled || m_list.m_drag.lifted;
}

auto LibraryView::on_pointer_up(Vec2 t_point) -> bool
{
	if (m_region_list.m_open) {
		if (const std::optional<u32> chosen = m_region_list.on_pointer_up(t_point)) {
			m_form.choose_region(*chosen);
		}

		m_pressed = Hit{};
		return true;
	}

	m_filter.on_pointer_up();
	m_form.end_text_selection();

	if (m_scroll.m_dragging) {
		m_scroll.on_pointer_up();
		m_pressed     = Hit{};
		m_list.m_drag = RowDrag{};
		return true;
	}

	if (m_list.m_selection.m_box) {
		m_list.m_selection.end_box();
		m_pressed = Hit{};
		return true;
	}

	if (m_list.m_drag.lifted) {
		drop_row();
		m_pressed = Hit{};
		return true;
	}

	m_list.m_drag.pressed_row.reset();

	const Hit pressed = std::exchange(m_pressed, Hit{});
	if (pressed.target == Target::NONE) return false;

	if (hit_at(t_point) == pressed) {
		click_target(pressed);
	}

	return true;
}

auto LibraryView::on_right_click(Vec2 t_point) -> void
{
	const Hit             hit      = hit_at(t_point);
	const Layout          layout   = this->layout();
	const VisibleAccounts accounts = m_list.rows();

	if (hit.target == Target::FILTER) {
		const Rect text_area = controls::search_text_rect(layout.filter, K_FILTER_INSET);

		focus_filter(true);
		m_filter.on_right_click(m_fonts->secondary, text_area, t_point.x);
		m_commands->push(Command{.type = CommandType::SHOW_TEXT_MENU, .position = t_point, .text_input = &m_filter});
		return;
	}

	if (hit.target == Target::FIELD) {
		const FormLayout form      = form_layout(form_card(layout, accounts));
		const Rect       text_area = field_text_rect(form, hit.field);
		TextInput*       input     = &m_form.m_fields[hit.field];

		m_form.focus(hit.field);
		input->on_right_click(m_fonts->body, text_area, t_point.x);
		m_commands->push(Command{.type = CommandType::SHOW_TEXT_MENU, .position = t_point, .text_input = input});
		return;
	}

	if (is_row_target(hit.target) && hit.index < accounts.count && !m_list.m_drag.lifted) {
		RowSelection* selection = &m_list.m_selection;

		if (!selection->contains(hit.index)) {
			selection->clear();
		}

		const Command menu{.type = CommandType::SHOW_ACCOUNT_MENU, .position = t_point, .account = accounts.refs[hit.index], .picked = selection->count()};
		m_commands->push(menu);
	}
}

auto LibraryView::on_scroll(float t_wheel_delta) -> void
{
	if (m_region_list.m_open) {
		m_region_list.on_scroll(t_wheel_delta);
		return;
	}

	const ScrollGeometry geometry = scroll_geometry(layout(), m_list.rows());
	m_scroll.on_scroll(t_wheel_delta, geometry);
}

auto LibraryView::on_key_down(os::Key t_key) -> bool
{
	if (m_region_list.m_open) {
		if (const std::optional<u32> chosen = m_region_list.on_key_down(t_key)) {
			m_form.choose_region(*chosen);
		}

		return true;
	}

	if (m_list.m_drag.lifted) {
		if (t_key == os::Key::ESCAPE) {
			cancel_drag();
		}

		return true;
	}

	if (m_list.m_selection.m_box) {
		if (t_key == os::Key::ESCAPE) {
			m_list.m_selection.cancel_box();
		}

		return true;
	}

	if (const std::optional<EditField> field = m_form.focused_field()) {
		switch (t_key) {
			using enum os::Key;

			case TAB: {
				m_form.focus(m_form.next_field(os::modifiers().shift));
				return true;
			}

			case ENTER: {
				save_form();
				return true;
			}

			case ESCAPE: {
				close_form(true);
				return true;
			}

			default: {
				m_form.m_fields[*field].on_key_down(t_key);
				return true;
			}
		}
	}

	if (m_filter.m_focused) {
		const bool escape = t_key == os::Key::ESCAPE;

		if (escape && !m_filter.value().empty()) {
			m_filter.set_value("");
		} else if (escape || t_key == os::Key::ENTER) {
			focus_filter(false);
		} else {
			m_filter.on_key_down(t_key);
		}

		refresh_filter();
		return true;
	}

	if (m_form_open) {
		switch (t_key) {
			using enum os::Key;

			case ESCAPE: {
				close_form(true);
				return true;
			}

			case ENTER: {
				save_form();
				return true;
			}

			case TAB: {
				m_form.focus(EditField::USERNAME);
				return true;
			}

			default: {
				break;
			}
		}
	}

	if (t_key == os::Key::ESCAPE && m_list.m_selection.m_rows.any()) {
		m_list.m_selection.clear();
		return true;
	}

	if (t_key == os::Key::A && os::modifiers().shortcut && can_select()) {
		m_list.m_selection.select_block(0, m_list.rows().count);
		return true;
	}

	return false;
}

auto LibraryView::on_char(u32 t_character) -> bool
{
	if (m_region_list.m_open) {
		m_region_list.on_char(t_character);
		return true;
	}

	if (const std::optional<EditField> field = m_form.focused_field()) {
		m_form.m_fields[*field].on_char(t_character);
		return true;
	}

	if (!m_filter.m_focused) {
		const bool printable = t_character > ' ' && t_character != 0x7F;
		if (!m_list.has_game() || m_form_open || m_list.m_drag.lifted || !printable) return false;

		focus_filter(true);
	}

	m_filter.on_char(t_character);
	refresh_filter();

	return true;
}

auto LibraryView::cancel_press() -> void
{
	m_pressed = Hit{};
	m_list.m_selection.end_box();

	if (m_list.m_drag.lifted) {
		drop_row();
	} else {
		m_list.m_drag = RowDrag{};
	}

	if (m_scroll.m_dragging) {
		m_scroll.on_pointer_up();
	}
}

auto LibraryView::cursor() const -> CursorKind
{
	if (m_region_list.m_open) return m_region_list.cursor(m_mouse);
	if (m_list.m_drag.lifted) return CursorKind::MOVE;
	if (m_list.m_selection.m_box) return CursorKind::ARROW;
	if (m_scroll.m_dragging) return CursorKind::DRAG;
	if (m_filter.m_selecting || m_form.is_selecting_text()) return CursorKind::I_BEAM;

	switch (hit_at(m_mouse).target) {
		using enum Target;

		case FILTER:
		case FIELD: {
			return CursorKind::I_BEAM;
		}

		case NONE:
		case ROW:
		case FORM: {
			const ScrollGeometry geometry   = scroll_geometry(layout(), m_list.rows());
			const bool           over_track = m_scroll.is_over_track(m_mouse, geometry);
			return over_track ? CursorKind::HAND : CursorKind::ARROW;
		}

		default: {
			return CursorKind::HAND;
		}
	}
}

auto LibraryView::draw_header(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) -> void
{
	const Font& title     = m_fonts->title;
	const Font& secondary = m_fonts->secondary;
	const Game& game      = m_library->games[*m_list.m_game];
	const Color accent    = m_settings->accent;

	const Vec2  title_baseline{t_layout.header.x, t_layout.title_baseline};
	const float title_room = std::max(0.0f, t_layout.filter.x - K_TITLE_GAP - t_layout.header.x);

	draw_text_truncated(t_draw_list, title, title_baseline, game.title, title_room, faded(g_theme.text, t_alpha));
	controls::draw_search_field(t_draw_list, secondary, t_layout.filter, K_FILTER_INSET, &m_filter, m_mouse, accent, t_alpha);

	const Rect  add    = t_layout.add;
	const Color border = mix(g_theme.separator, g_theme.border, m_add_hover);
	const Color ink    = mix(g_theme.text_dim, g_theme.text, m_add_hover);
	const Color fill   = faded(g_theme.row_hover, scaled_alpha(t_alpha, m_add_hover));
	const float plus_y = snapped_to_pixel(add.center().y - K_ADD_ICON_SIZE * 0.5f);
	const Rect  plus{add.x + K_ADD_PADDING_X, plus_y, K_ADD_ICON_SIZE, K_ADD_ICON_SIZE};
	const Vec2  label_baseline{plus.right() + K_ADD_ICON_GAP, secondary.centered_baseline(add)};

	t_draw_list->add_bordered_rect(add, rounded(K_CONTROL_RADIUS), fill, faded(border, t_alpha), 1.0f);
	t_draw_list->add_image(plus, m_assets->texture(Asset::ICON_ADD), faded(ink, t_alpha));
	draw_text(t_draw_list, secondary, label_baseline, K_ADD_LABEL, faded(ink, t_alpha));
}

auto LibraryView::draw_labels(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) const -> void
{
	const Font&    caption  = m_fonts->caption;
	const Rect     labels   = t_layout.labels;
	const Columns& columns  = t_layout.columns;
	const float    baseline = caption.centered_baseline(labels);
	const Color    color    = faded(g_theme.text_faint, t_alpha);

	const Vec2 account_label{labels.x + columns.account, baseline};
	draw_text(t_draw_list, caption, account_label, "Account", color);

	if (columns.show_region) {
		const Vec2 region_label{labels.x + columns.region, baseline};
		draw_text(t_draw_list, caption, region_label, "Region", color);
	}

	if (columns.show_played) {
		const Vec2 played_label{labels.x + columns.played, baseline};
		draw_text(t_draw_list, caption, played_label, "Last played", color);
	}

	const Rect top_edge{labels.x, labels.bottom() - 1.0f, labels.w, 1.0f};
	t_draw_list->add_rect(top_edge, faded(g_theme.separator, t_alpha));
}

auto LibraryView::draw_row(DrawList* t_draw_list, const Layout& t_layout, Rect t_row, AccountRef t_account, u32 t_index, bool t_raised, u8 t_alpha) const -> void
{
	const Font&    body      = m_fonts->body;
	const Font&    secondary = m_fonts->secondary;
	const Columns& columns   = t_layout.columns;
	const Account& account   = *m_library->account(t_account);
	const Color    accent    = m_settings->accent;
	const bool     logging   = shows_login(t_account);
	const float    hover     = t_raised ? 0.0f : m_list.m_hover[t_index];
	const Rect     highlight = t_row.inset(0.0f, K_HIGHLIGHT_INSET_Y);

	if (t_raised && m_list.m_lift > 0.0f) {
		const u8 lift_alpha = scaled_alpha(t_alpha, m_list.m_lift);
		t_draw_list->add_bordered_rect(t_row, rounded(K_ROW_RADIUS), faded(g_theme.popup, lift_alpha), faded(g_theme.border, lift_alpha), 1.0f);
	}

	if (m_list.m_selection.contains(t_index)) {
		const Color picked = with_alpha(accent, static_cast<u8>(K_SELECTED_ALPHA * t_alpha / 255.0f));
		t_draw_list->add_rounded_rect(highlight, rounded(K_ROW_RADIUS), picked);
	}

	const float lit = logging ? 1.0f : hover * K_ROW_HOVER_SHARE;
	if (lit > 0.0f) {
		const Color glow = faded(controls::glass_highlight(lit), t_alpha);
		t_draw_list->add_rounded_rect(highlight, rounded(K_ROW_RADIUS), glow);
	}

	if (m_flash_account == t_account && m_flash > 0.0f) {
		const Color flash = with_alpha(accent, static_cast<u8>(K_FLASH_ALPHA * m_flash * t_alpha / 255.0f));
		t_draw_list->add_rounded_rect(t_row, rounded(K_ROW_RADIUS), flash);
	}

	const float x           = t_row.x + columns.account;
	const bool  second_line = account.note[0] != '\0' || logging;
	const float block       = body.line_height() + K_ROW_LINE_GAP + secondary.line_height();
	const float block_y     = t_row.y + (t_row.h - block) * 0.5f;
	const float username_y  = second_line ? block_y + body.ascent : body.centered_baseline(t_row);
	const float details_y   = block_y + body.line_height() + K_ROW_LINE_GAP + secondary.ascent;

	const Vec2 username_baseline{x, username_y};
	draw_text_truncated(t_draw_list, body, username_baseline, account.username, columns.account_width, faded(g_theme.text, t_alpha));

	if (logging) {
		Color status = mix(g_theme.text_dim, accent, K_STATUS_TINT);
		if (m_session->is_finished()) {
			status = m_session->stage() == LoginStage::SUCCESS ? g_theme.success : g_theme.error;
		}

		const float change = m_session->m_status_change;
		const Vec2  leaving_baseline{x, details_y - K_STATUS_RISE * change};
		const Vec2  arriving_baseline{x, details_y + K_STATUS_RISE * (1.0f - change)};
		const Color leaving_color  = faded(status, scaled_alpha(t_alpha, 1.0f - change));
		const Color arriving_color = faded(status, scaled_alpha(t_alpha, change));

		draw_text_truncated(t_draw_list, secondary, leaving_baseline, m_session->m_previous_status, columns.account_width, leaving_color);
		draw_text_truncated(t_draw_list, secondary, arriving_baseline, m_session->m_status, columns.account_width, arriving_color);
	} else if (account.note[0] != '\0') {
		const Vec2 note_baseline{x, details_y};
		draw_text_truncated(t_draw_list, secondary, note_baseline, account.note, columns.account_width, faded(g_theme.text_dim, t_alpha));
	}

	if (columns.show_region && account.region[0] != '\0') {
		const float chip_x = t_row.x + columns.region;
		controls::draw_region_chip(t_draw_list, secondary, chip_x, t_row.center().y, account.region, t_alpha);
	}

	const std::string_view button_label = logging ? login_button_label(t_account) : std::string_view{};
	const Rect             button       = button_label.empty() ? Rect{} : login_rect(t_row, button_label);
	const float            played_right = t_row.x + columns.played + columns.played_width;
	const bool             covered      = !button_label.empty() && button.x < played_right;

	if (columns.show_played && !covered) {
		char             relative[32];
		std::string_view played = "Never";
		Color            color  = g_theme.text_faint;

		if (account.last_used != 0) {
			played = relative_time(account.last_used, std::time(nullptr), relative);
			color  = g_theme.text_dim;
		}

		const Vec2 played_baseline{t_row.x + columns.played, secondary.centered_baseline(t_row)};
		draw_text_truncated(t_draw_list, secondary, played_baseline, played, columns.played_width, faded(color, t_alpha));
	}

	const Rect star         = star_rect(t_layout, t_row);
	const Rect star_icon    = star.inset(K_STAR_ICON_INSET);
	const bool star_clear   = button_label.empty() || button.x > star.right();
	const bool star_hovered = !t_raised && m_hovered == Hit{Target::STAR, t_index};

	if (star_hovered) {
		controls::draw_circular_hover(t_draw_list, star, g_theme.shadow, g_theme.control_hover, t_alpha);
	}

	if (account.favorite && star_clear) {
		controls::draw_favorite(t_draw_list, m_assets, star_icon, true, faded(accent, t_alpha));
	} else if (hover > 0.0f && !logging) {
		const Color outline    = star_hovered ? g_theme.text : g_theme.text_faint;
		const Color star_color = faded(outline, scaled_alpha(t_alpha, hover));
		controls::draw_favorite(t_draw_list, m_assets, star_icon, false, star_color);
	}

	if (logging) {
		draw_login_state(t_draw_list, t_row, t_account, t_index, t_alpha);
		return;
	}

	if (hover <= 0.0f) return;

	const u8   shown = scaled_alpha(t_alpha, hover);
	const Vec2 slide{snapped_to_pixel((1.0f - hover) * K_PILL_SLIDE), 0.0f};
	const Rect login = login_rect(t_row, K_LOGIN_LABEL).moved(slide);
	draw_pill(t_draw_list, secondary, login, K_LOGIN_LABEL, accent, controls::contrasting_theme_text(accent), shown);
}

auto LibraryView::draw_login_state(DrawList* t_draw_list, Rect t_row, AccountRef t_account, u32 t_index, u8 t_alpha) const -> void
{
	const Font&      secondary = m_fonts->secondary;
	const Color      accent    = m_settings->accent;
	const bool       finished  = m_session->is_finished();
	const LoginStage stage     = m_session->stage();
	const bool       succeeded = finished && stage == LoginStage::SUCCESS;
	const Color      outcome   = succeeded ? g_theme.success : g_theme.error;
	const Color      fill      = finished ? mix(accent, outcome, m_session->m_outcome) : accent;

	const float progress    = std::clamp(m_session->m_progress, 0.0f, 1.0f);
	const float track_y     = t_row.bottom() - K_PROGRESS_BOTTOM - K_PROGRESS_HEIGHT;
	const float track_width = t_row.w - K_PROGRESS_INSET * 2.0f;
	const Rect  track{t_row.x + K_PROGRESS_INSET, track_y, track_width, K_PROGRESS_HEIGHT};
	const Rect  bar{track.x, track.y, track.w * progress, track.h};

	t_draw_list->add_rounded_rect(track, rounded(track.h * 0.5f), faded(g_theme.control, t_alpha));
	if (bar.w > 0.5f) {
		t_draw_list->add_rounded_rect(bar, rounded(bar.h * 0.5f), faded(fill, t_alpha));
	}

	if (succeeded) {
		const float check_x = t_row.right() - K_ROW_INSET - K_CHECK_SIZE;
		const float check_y = snapped_to_pixel(t_row.center().y - K_CHECK_SIZE * 0.5f);
		const Rect  check{check_x, check_y, K_CHECK_SIZE, K_CHECK_SIZE};

		controls::draw_check(t_draw_list, m_assets, check, faded(g_theme.success, t_alpha));
		return;
	}

	const std::string_view label = login_button_label(t_account);
	if (label.empty()) return;

	const Rect button        = login_rect(t_row, label);
	const bool on_row_button = is_row_target(m_hovered.target) && m_hovered.target != Target::ROW && m_hovered.target != Target::STAR;
	const bool on_button     = on_row_button && m_hovered.index == t_index;

	if (label == K_CANCEL_LABEL) {
		const Color border = on_button ? g_theme.border : g_theme.separator;
		const Color back   = on_button ? g_theme.row_selected : g_theme.row_hover;
		const Color ink    = on_button ? g_theme.text : g_theme.text_dim;

		t_draw_list->add_bordered_rect(button, rounded(K_PILL_RADIUS), faded(back, t_alpha), faded(border, t_alpha), 1.0f);
		draw_text_centered(t_draw_list, secondary, button, label, faded(ink, t_alpha));
		return;
	}

	const Color pill = on_button ? hovered(accent) : accent;
	draw_pill(t_draw_list, secondary, button, label, pill, controls::contrasting_theme_text(accent), t_alpha);
}

auto LibraryView::draw_form(DrawList* t_draw_list, const Layout& t_layout, const VisibleAccounts& t_accounts, Rect t_card, u8 t_alpha) -> void
{
	if (t_card.h <= 0.5f) return;

	const Font&      body    = m_fonts->body;
	const Font&      caption = m_fonts->caption;
	const FormLayout form    = form_layout(t_card);
	const u8         content = scaled_alpha(t_alpha, m_form_amount);
	const bool       live    = m_form_open && m_form_amount > 0.5f;

	t_draw_list->add_bordered_rect(t_card, rounded(K_FORM_RADIUS), faded(g_theme.popup, t_alpha), faded(g_theme.control_hover, t_alpha), 1.0f);

	if (m_form.m_account && m_form_amount < 1.0f) {
		if (const std::optional<u32> row = form_row(t_accounts)) {
			const Rect top{t_card.x, t_card.y, t_card.w, t_layout.row_height};
			const u8   fading = scaled_alpha(t_alpha, 1.0f - m_form_amount);
			draw_row(t_draw_list, t_layout, top, *m_form.m_account, *row, true, fading);
		}
	}

	t_draw_list->push_clip(t_card);

	const float            icon_y = snapped_to_pixel(form.title.center().y - K_FORM_ICON_SIZE * 0.5f);
	const Rect             icon{form.title.x, icon_y, K_FORM_ICON_SIZE, K_FORM_ICON_SIZE};
	const Asset            icon_image = m_form.m_account ? Asset::ICON_EDIT : Asset::ICON_ADD;
	const std::string_view heading    = m_form.m_account ? std::string_view{m_library->account(*m_form.m_account)->username} : std::string_view{"New account"};
	const float            hint_width = text_width(caption, K_ESCAPE_HINT);
	const float            heading_x  = icon.right() + K_FORM_ICON_GAP;
	const float            room       = std::max(0.0f, form.title.right() - hint_width - K_TITLE_GAP - heading_x);
	const Vec2             heading_baseline{heading_x, body.centered_baseline(form.title)};
	const Vec2             hint_baseline{form.title.right() - hint_width, caption.centered_baseline(form.title)};

	t_draw_list->add_image(icon, m_assets->texture(icon_image), faded(g_theme.text_dim, content));
	draw_text_truncated(t_draw_list, body, heading_baseline, heading, room, faded(g_theme.text, content));
	draw_text(t_draw_list, caption, hint_baseline, K_ESCAPE_HINT, faded(g_theme.text_faint, content));

	draw_form_fields(t_draw_list, form, live, content);
	draw_form_region(t_draw_list, form, live, content);

	if (form.has_games) {
		draw_form_games(t_draw_list, form, live, content);
	}

	const Color accent         = m_settings->accent;
	const bool  cancel_hovered = live && form.cancel.contains(m_mouse);
	const bool  save_hovered   = live && form.save.contains(m_mouse);

	controls::draw_button(t_draw_list, body, form.cancel, "Cancel", controls::ButtonStyle::GHOST, accent, true, cancel_hovered, content);
	controls::draw_button(t_draw_list, body, form.save, "Save", controls::ButtonStyle::ACCENT, accent, true, save_hovered, content);

	t_draw_list->pop_clip();
}

auto LibraryView::draw_form_fields(DrawList* t_draw_list, const FormLayout& t_form, bool t_live, u8 t_alpha) -> void
{
	const Font& body         = m_fonts->body;
	const Font& secondary    = m_fonts->secondary;
	const Color accent       = m_settings->accent;
	const Color active_label = mix(g_theme.text_dim, accent, K_ACTIVE_LABEL_TINT);

	for (const EditField field : K_FIELD_ORDER) {
		TextInput* input    = &m_form.m_fields[field];
		const Rect box      = t_form.fields[field];
		const Rect label    = t_form.labels[field];
		const bool focused  = input->m_focused;
		const bool required = field != EditField::NOTE;
		const bool missing  = m_form.m_show_required && required && input->value().empty();
		const bool hovered  = t_live && !focused && box.contains(m_mouse);

		Color label_color = g_theme.text_dim;
		Color border      = g_theme.separator;
		if (missing) {
			label_color = g_theme.error;
			border      = g_theme.error;
		} else if (focused) {
			label_color = active_label;
			border      = accent;
		} else if (hovered) {
			border = g_theme.border;
		}

		if (required) {
			const Vec2 label_baseline{label.x, label.y + secondary.ascent};
			draw_text(t_draw_list, secondary, label_baseline, K_FIELD_LABELS[field], faded(label_color, t_alpha));
		} else {
			draw_optional_label(t_draw_list, secondary, label, K_FIELD_LABELS[field], label_color, t_alpha);
		}

		const auto notice = [&](controls::NoticeKind t_kind, std::string_view t_text, Color t_color) {
			const float width = controls::notice_width(secondary, t_text);
			const Vec2  notice_at{label.right() - width, label.y};
			controls::draw_notice(t_draw_list, secondary, notice_at, t_kind, t_text, faded(t_color, t_alpha));
		};

		if (missing) {
			notice(controls::NoticeKind::ALERT, "Required", g_theme.error);
		} else if (field == EditField::PASSWORD && focused && os::is_caps_lock_on()) {
			notice(controls::NoticeKind::CAPS_LOCK, "Caps Lock is on", controls::caution_color());
		}

		draw_input_box(t_draw_list, box, border, focused && !missing, accent, t_alpha);
		input->draw(t_draw_list, body, field_text_rect(t_form, field), faded(g_theme.text, t_alpha), faded(accent, t_alpha));
	}

	const bool  shown_password = !m_form.m_fields[EditField::PASSWORD].m_masked;
	const bool  reveal_hovered = t_live && t_form.reveal.contains(m_mouse);
	const Color eye            = reveal_hovered ? g_theme.text : g_theme.text_dim;
	controls::draw_eye(t_draw_list, m_assets, t_form.reveal, shown_password, faded(eye, t_alpha));
}

auto LibraryView::draw_form_region(DrawList* t_draw_list, const FormLayout& t_form, bool t_live, u8 t_alpha) const -> void
{
	const Font& body      = m_fonts->body;
	const Font& secondary = m_fonts->secondary;
	const Color accent    = m_settings->accent;
	const Rect  region    = t_form.region;
	const bool  open      = m_region_list.m_open;
	const bool  hovered   = t_live && region.contains(m_mouse);
	const u32   choice    = region_index(m_form.m_region);

	draw_optional_label(t_draw_list, secondary, t_form.region_label, "Region", g_theme.text_dim, t_alpha);

	Color border = g_theme.separator;
	if (open) {
		border = accent;
	} else if (hovered) {
		border = g_theme.border;
	}

	const float chevron_x = region.right() - K_CHEVRON_MARGIN - K_CHEVRON_SIZE.x;
	const float chevron_y = region.center().y - K_CHEVRON_SIZE.y * 0.5f;
	const Rect  chevron{chevron_x, chevron_y, K_CHEVRON_SIZE.x, K_CHEVRON_SIZE.y};
	const float value_x    = region.x + K_INPUT_PADDING_X;
	const float value_room = chevron.x - K_INPUT_PADDING_X - value_x;
	const Vec2  value_baseline{value_x, body.centered_baseline(region)};
	const Color value_color   = choice == 0 ? g_theme.text_faint : g_theme.text;
	const Color chevron_color = open || hovered ? g_theme.text : g_theme.text_dim;

	draw_input_box(t_draw_list, region, border, open, accent, t_alpha);
	draw_text_truncated(t_draw_list, body, value_baseline, K_REGION_OPTIONS[choice].label, value_room, faded(value_color, t_alpha));
	controls::draw_chevron(t_draw_list, chevron, open, faded(chevron_color, t_alpha));
}

auto LibraryView::draw_form_games(DrawList* t_draw_list, const FormLayout& t_form, bool t_live, u8 t_alpha) const -> void
{
	const Font& secondary = m_fonts->secondary;
	const Color accent    = m_settings->accent;
	const Rect  label     = t_form.show_in_label;
	const Vec2  label_baseline{label.x, label.y + secondary.ascent};

	draw_text(t_draw_list, secondary, label_baseline, "Show in", faded(g_theme.text_dim, t_alpha));

	for (const u32 game_index : std::views::iota(0u, m_library->game_count)) {
		const Game& game     = m_library->games[game_index];
		const Rect  chip     = t_form.games[game_index];
		const bool  selected = (m_form.m_visible_mask & (1u << game_index)) != 0;
		const bool  hovered  = t_live && chip.contains(m_mouse);

		Color border = selected ? mix(g_theme.border, accent, K_SHOWN_BORDER_TINT) : g_theme.separator;
		if (!selected && hovered) {
			border = g_theme.border;
		}

		const Color fill       = selected ? mix(g_theme.field, accent, K_SHOWN_CHIP_TINT) : g_theme.field;
		const Color title      = selected || hovered ? g_theme.text : g_theme.text_dim;
		const u8    icon_alpha = selected ? t_alpha : static_cast<u8>(t_alpha * K_HIDDEN_ICON_ALPHA / 255);
		const float icon_y     = snapped_to_pixel(chip.center().y - K_GAME_CHIP_ICON * 0.5f);
		const Rect  icon{chip.x + K_CHIP_PADDING_X, icon_y, K_GAME_CHIP_ICON, K_GAME_CHIP_ICON};
		const Vec2  title_baseline{icon.right() + K_GAME_CHIP_ICON_GAP, secondary.centered_baseline(chip)};

		t_draw_list->add_bordered_rect(chip, rounded(K_CHIP_RADIUS), faded(fill, t_alpha), faded(border, t_alpha), 1.0f);

		if (game.icon != nullptr) {
			t_draw_list->add_image(icon, game.icon, faded(K_COLOR_IMAGE, icon_alpha), rounded(K_GAME_CHIP_ICON_RADIUS));
		} else {
			t_draw_list->add_rounded_rect(icon, rounded(K_GAME_CHIP_ICON_RADIUS), faded(game.accent, icon_alpha));
		}

		draw_text(t_draw_list, secondary, title_baseline, game.short_title, faded(title, t_alpha));
	}
}

auto LibraryView::draw_empty_state(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) const -> void
{
	const Rect zone = empty_zone(t_layout);
	controls::draw_drop_zone(t_draw_list, m_assets, m_fonts->body, m_fonts->secondary, zone, K_ZONE_TITLE, K_ZONE_HINT, m_settings->accent, m_zone_hover, t_alpha);
}

auto LibraryView::draw_no_matches(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) const -> void
{
	const Font&                body      = m_fonts->body;
	const Font&                secondary = m_fonts->secondary;
	constexpr std::string_view TITLE     = "No matches";
	constexpr std::string_view HINT      = "Try a different name, note or region.";

	const float area_height = std::max(0.0f, m_bounds.bottom() - t_layout.labels.y);
	const Rect  area{m_bounds.x, t_layout.labels.y, m_bounds.w, area_height};
	const float stack    = body.line_height() + K_EMPTY_LINE_GAP + secondary.line_height();
	const float top      = snapped_to_pixel(area.center().y - stack * 0.5f - K_HEADER_GAP);
	const float center_x = area.center().x;

	const Vec2 title_baseline{snapped_to_pixel(center_x - text_width(body, TITLE) * 0.5f), top + body.ascent};
	const Vec2 hint_baseline{snapped_to_pixel(center_x - text_width(secondary, HINT) * 0.5f), top + body.line_height() + K_EMPTY_LINE_GAP + secondary.ascent};

	draw_text(t_draw_list, body, title_baseline, TITLE, faded(g_theme.text_dim, t_alpha));
	draw_text(t_draw_list, secondary, hint_baseline, HINT, faded(g_theme.text_faint, t_alpha));
}

auto LibraryView::draw(DrawList* t_draw_list, u8 t_alpha) -> void
{
	if (!m_list.has_game() || t_alpha == 0) return;

	const Layout          layout   = this->layout();
	const VisibleAccounts accounts = m_list.rows();
	const u8              header   = scaled_alpha(t_alpha, eased_out(m_appear));

	draw_header(t_draw_list, layout, header);

	if (accounts.count == 0 && !form_shown()) {
		if (m_filter.value().empty()) {
			draw_empty_state(t_draw_list, layout, header);
		} else {
			draw_no_matches(t_draw_list, layout, header);
		}

		return;
	}

	draw_labels(t_draw_list, layout, header);

	const std::optional<u32> edited    = form_row(accounts);
	const RowSet&            raised    = m_list.m_drag.lifted ? m_list.m_drag.rows : m_list.m_raised;
	const Color              separator = g_theme.separator;

	const auto separator_under = [&](Rect t_row, u8 t_line_alpha) {
		const Rect line{t_row.x, t_row.bottom() - 1.0f, t_row.w, 1.0f};
		t_draw_list->add_rect(line, faded(separator, t_line_alpha));
	};

	t_draw_list->push_clip(layout.rows_area);

	for (const u32 row : std::views::iota(0u, accounts.count)) {
		if (row == edited || raised[row]) continue;

		const float entrance = row_entrance(row);
		const Vec2  rise{0.0f, snapped_to_pixel((1.0f - entrance) * K_ROW_RISE)};
		const Rect  rect = row_rect(layout, accounts, row).moved(rise);
		if (entrance <= 0.0f || !rect.overlaps_vertically(layout.rows_area)) continue;

		const u8 row_alpha = scaled_alpha(t_alpha, entrance);
		separator_under(rect, row_alpha);
		draw_row(t_draw_list, layout, rect, accounts.refs[row], row, false, row_alpha);
	}

	if (form_shown()) {
		draw_form(t_draw_list, layout, accounts, form_card(layout, accounts), t_alpha);
	}

	// Lifted rows go on top, with all their shadows first so none falls across another lifted row.
	const auto  raised_rect = [&](u32 t_row) { return m_list.m_drag.lifted ? dragged_rect(layout, t_row) : row_rect(layout, accounts, t_row); };
	const float shadow      = m_list.m_lift * t_alpha / 255.0f;

	if (m_list.m_lift > 0.0f) {
		for (const u32 row : std::views::iota(0u, accounts.count)) {
			if (raised[row] && row != edited) {
				controls::draw_popup_shadow(t_draw_list, raised_rect(row), K_ROW_RADIUS, shadow);
			}
		}
	}

	const u8 lifted_separator = scaled_alpha(t_alpha, 1.0f - m_list.m_lift);

	for (const u32 row : std::views::iota(0u, accounts.count)) {
		if (!raised[row] || row == edited) continue;

		const Rect rect = raised_rect(row);
		separator_under(rect, lifted_separator);
		draw_row(t_draw_list, layout, rect, accounts.refs[row], row, true, t_alpha);
	}

	t_draw_list->pop_clip();

	const ScrollGeometry geometry = scroll_geometry(layout, accounts);
	m_scroll.draw_edge_fade(t_draw_list, layout.rows_area, geometry, faded(g_theme.window, t_alpha));
	m_scroll.draw(t_draw_list, geometry, m_mouse, t_alpha);

	if (m_list.m_selection.box_shown()) {
		const Rect box = m_list.m_selection.box_rect(first_row_y(layout));

		t_draw_list->push_clip(marquee_area(layout));
		controls::draw_marquee(t_draw_list, box, m_settings->accent, t_alpha);
		t_draw_list->pop_clip();
	}

	m_region_list.draw(t_draw_list, m_mouse);
}
