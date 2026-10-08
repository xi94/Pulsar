#include "ui/library_view.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <iterator>
#include <utility>

#include <sodium.h>

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
constexpr float K_SUBTITLE_GAP           = 2.0f;
constexpr float K_HEADER_GAP             = 16.0f;
constexpr float K_TITLE_GAP              = 16.0f;
constexpr float K_LABELS_PADDING_Y       = 4.0f;
constexpr float K_LABELS_GAP             = 6.0f;
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
constexpr float K_ROW_GAP                = 2.0f;
constexpr float K_ROW_RADIUS             = 8.0f;
constexpr float K_ROW_INSET              = 10.0f;
constexpr float K_STAR_INSET             = 4.0f;
constexpr float K_STAR_COLUMN            = 26.0f;
constexpr float K_STAR_SIZE              = 24.0f;
constexpr float K_STAR_ICON_INSET        = 4.0f;
constexpr float K_ACCOUNT_GAP            = 6.0f;
constexpr float K_COLUMN_GAP             = 16.0f;
constexpr float K_MIN_ACCOUNT_WIDTH      = 140.0f;
constexpr float K_PILL_PADDING_X         = 12.0f;
constexpr float K_PILL_HEIGHT            = 26.0f;
constexpr float K_PILL_RADIUS            = 7.0f;
constexpr float K_PILL_SLIDE             = 6.0f;
constexpr float K_EDIT_SIZE              = 26.0f;
constexpr float K_EDIT_ICON_INSET        = 6.0f;
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
constexpr float K_SHIFT_EASE_RATE        = 18.0f;
constexpr float K_LIFT_EASE_RATE         = 16.0f;
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
constexpr float K_DELETE_PADDING_X       = 10.0f;
constexpr float K_COUNTDOWN_SIZE         = 14.0f;
constexpr float K_COUNTDOWN_GAP          = 6.0f;
constexpr float K_DANGER_TINT            = 0.22f;
constexpr float K_DELETE_CONFIRM_SECONDS = 3.0f;
constexpr float K_UNDO_SECONDS           = 6.0f;
constexpr float K_SUCCESS_LINGER_SECONDS = 2.5f;
constexpr float K_FAILURE_LINGER_SECONDS = 8.0f;
constexpr float K_FLASH_SECONDS          = 1.2f;
constexpr float K_EMPTY_ICON_SIZE        = 52.0f;
constexpr float K_EMPTY_ICON_RADIUS      = 12.0f;
constexpr float K_EMPTY_GAP              = 16.0f;
constexpr float K_EMPTY_LINE_GAP         = 4.0f;
constexpr float K_EMPTY_BUTTON_WIDTH     = 140.0f;
constexpr u32   K_FILTER_MAX_LENGTH      = 64;
constexpr Color K_COLOR_IMAGE{255, 255, 255, 255};

constexpr float K_CHEVRON_MARGIN = 12.0f;
constexpr Vec2  K_CHEVRON_SIZE{9.0f, 5.0f};

constexpr std::string_view K_LOGIN_LABEL      = "Login";
constexpr std::string_view K_CANCEL_LABEL     = "Cancel";
constexpr std::string_view K_RETRY_LABEL      = "Try again";
constexpr std::string_view K_PERMISSION_LABEL = "Open settings";
constexpr std::string_view K_ADD_LABEL        = "Add account";
constexpr std::string_view K_DELETE_LABEL     = "Delete";
constexpr std::string_view K_DELETE_ARMED     = "Delete for good";
constexpr std::string_view K_OPTIONAL_SUFFIX  = " (optional)";
constexpr std::string_view K_FIELD_LABELS[]{"Username", "Password", "Note"};

[[nodiscard]] auto eased_out(float t_amount) -> float
{
	return 1.0f - std::pow(1.0f - t_amount, 3.0f);
}

[[nodiscard]] auto scaled_alpha(u8 t_alpha, float t_amount) -> u8
{
	return static_cast<u8>(static_cast<float>(t_alpha) * std::clamp(t_amount, 0.0f, 1.0f));
}

[[nodiscard]] auto matches_filter(const Account& t_account, std::string_view t_query) -> bool
{
	return find_ignoring_case(t_account.username, t_query) != std::string_view::npos || find_ignoring_case(t_account.note, t_query) != std::string_view::npos ||
	       find_ignoring_case(t_account.region, t_query) != std::string_view::npos;
}

auto draw_input_box(DrawList* t_draw_list, Rect t_rect, Color t_border, bool t_focused, Color t_accent, u8 t_alpha) -> void
{
	if (t_focused) {
		t_draw_list->add_shadow(t_rect, K_INPUT_RADIUS, K_FOCUS_GLOW_BLUR, with_alpha(t_accent, static_cast<u8>(K_FOCUS_GLOW_ALPHA * t_alpha / 255)));
	}

	t_draw_list->add_bordered_rect(t_rect, rounded(K_INPUT_RADIUS), faded(g_theme.field, t_alpha), faded(t_border, t_alpha),
	                               t_focused ? K_INPUT_FOCUS_BORDER : 1.0f);
}

auto draw_pill(DrawList* t_draw_list, const Font& t_font, Rect t_rect, std::string_view t_label, Color t_fill, Color t_ink, u8 t_alpha) -> void
{
	t_draw_list->add_rounded_rect(t_rect, rounded(K_PILL_RADIUS), faded(t_fill, t_alpha));
	draw_text_centered(t_draw_list, t_font, t_rect, t_label, faded(t_ink, t_alpha));
}

}

auto LibraryView::is_row_target(Target t_target) -> bool
{
	switch (t_target) {
		using enum Target;

		case ROW:
		case STAR:
		case EDIT:
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

LibraryView::LibraryView(Library*        t_library,
                         const Settings* t_settings,
                         const Fonts*    t_fonts,
                         const Assets*   t_assets,
                         Toasts*         t_toasts,
                         LoginSession*   t_session,
                         CommandQueue*   t_commands)
	: m_library(t_library)
	, m_settings(t_settings)
	, m_fonts(t_fonts)
	, m_assets(t_assets)
	, m_toasts(t_toasts)
	, m_session(t_session)
	, m_commands(t_commands)
	, m_region_list(t_fonts, t_assets, t_settings, ListPopupOptions{.empty_message = "No regions"})
{
	constexpr u32 LENGTHS[K_FIELD_COUNT]{sizeof(Account::username) - 1, sizeof(Account::password) - 1, sizeof(Account::note) - 1};

	for (u32 i = 0; i < K_FIELD_COUNT; i += 1) {
		m_fields[i].set_max_length(LENGTHS[i]);
	}

	m_fields[K_NOTE].set_placeholder("Main, smurf, ranked...");
	m_filter.set_max_length(K_FILTER_MAX_LENGTH);
	m_filter.set_placeholder("Filter accounts");
}

auto LibraryView::show_game(i32 t_game) -> void
{
	if (t_game == m_game) return;

	close_form(false);
	reset_row_motion();
	blur();

	m_game    = t_game;
	m_appear  = 0.0f;
	m_scroll  = Scrollable{};
	m_pressed = Hit{};
	m_hovered = Hit{};
	m_filter.set_value("");
	m_applied_filter[0] = '\0';
	m_flash_account.reset();
	std::fill(std::begin(m_row_hover), std::end(m_row_hover), 0.0f);
}

auto LibraryView::restart() -> void
{
	show_game(-1);
}

auto LibraryView::has_game() const -> bool
{
	return m_game >= 0 && static_cast<u32>(m_game) < m_library->game_count;
}

auto LibraryView::shown_accounts() const -> VisibleAccounts
{
	VisibleAccounts accounts = has_game() ? m_library->visible_accounts(static_cast<u32>(m_game)) : VisibleAccounts{};

	const std::string_view query = m_filter.value();
	if (query.empty()) return accounts;

	u32 kept = 0;
	for (u32 i = 0; i < accounts.count; i += 1) {
		if (matches_filter(*m_library->account(accounts.refs[i]), query)) {
			accounts.refs[kept] = accounts.refs[i];
			kept += 1;
		}
	}

	accounts.count = kept;

	return accounts;
}

auto LibraryView::row_of(const VisibleAccounts& t_accounts, AccountRef t_account) const -> std::optional<u32>
{
	for (u32 i = 0; i < t_accounts.count; i += 1) {
		if (t_accounts.refs[i] == t_account) return i;
	}

	return std::nullopt;
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
	layout.header            = Rect{left, m_bounds.y + K_PADDING_TOP, width, title.line_height() + K_SUBTITLE_GAP + secondary.line_height()};
	layout.title_baseline    = layout.header.y + title.ascent;
	layout.subtitle_baseline = layout.header.y + title.line_height() + K_SUBTITLE_GAP + secondary.ascent;

	const float add_width    = K_ADD_PADDING_X * 2.0f + K_ADD_ICON_SIZE + K_ADD_ICON_GAP + text_width(secondary, K_ADD_LABEL);
	const float filter_width = std::clamp(width * K_FILTER_SHARE, K_FILTER_MIN_WIDTH, K_FILTER_MAX_WIDTH);
	const float control_y    = snapped_to_pixel(layout.header.center().y - K_CONTROL_HEIGHT * 0.5f);

	layout.add    = Rect{layout.header.right() - add_width, control_y, add_width, K_CONTROL_HEIGHT};
	layout.filter = Rect{layout.add.x - K_CONTROL_GAP - filter_width, control_y, filter_width, K_CONTROL_HEIGHT};

	layout.labels =
		Rect{left - K_ROWS_BLEED, layout.header.bottom() + K_HEADER_GAP, width + K_ROWS_BLEED * 2.0f, caption.line_height() + K_LABELS_PADDING_Y * 2.0f};

	const float rows_top = layout.labels.bottom() + K_LABELS_GAP;
	layout.rows          = Rect{layout.labels.x, rows_top, layout.labels.w, std::max(0.0f, m_bounds.bottom() - rows_top)};
	layout.row_height    = snapped_to_pixel(std::max(body.line_height() + K_ROW_LINE_GAP + secondary.line_height(), K_PILL_HEIGHT) + K_ROW_PADDING_Y * 2.0f);
	layout.columns       = columns(layout.rows.w);

	return layout;
}

// Columns drop out from the right when the pane is narrow: last played first, then the region.
auto LibraryView::columns(float t_row_width) const -> LibraryView::Columns
{
	const Font& secondary = m_fonts->secondary;
	const Font& caption   = m_fonts->caption;
	const float login     = text_width(secondary, K_LOGIN_LABEL) + K_PILL_PADDING_X * 2.0f;

	Columns columns{};
	columns.star         = K_STAR_INSET;
	columns.account      = columns.star + K_STAR_COLUMN + K_ACCOUNT_GAP;
	columns.actions      = t_row_width - K_ROW_INSET - (K_EDIT_SIZE + K_ACTION_GAP + login);
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

auto LibraryView::pitch(const Layout& t_layout) const -> float
{
	return t_layout.row_height + K_ROW_GAP;
}

auto LibraryView::form_shown() const -> bool
{
	return m_form_open || m_form_amount > 0.0f;
}

auto LibraryView::form_row(const VisibleAccounts& t_accounts) const -> std::optional<u32>
{
	if (!form_shown() || !m_form_account) return std::nullopt;

	return row_of(t_accounts, *m_form_account);
}

auto LibraryView::form_extra(const Layout& t_layout) const -> float
{
	if (!form_shown()) return 0.0f;
	if (!m_form_account) return (m_form_height + K_ROW_GAP) * m_form_amount;

	return std::max(0.0f, m_form_height - t_layout.row_height) * m_form_amount;
}

// A new account's form opens above the rows; editing grows the account's own row into the form.
auto LibraryView::form_card(const Layout& t_layout, const VisibleAccounts& t_accounts) const -> Rect
{
	const float top = t_layout.rows.y - m_scroll.offset();
	if (!m_form_account) return Rect{t_layout.rows.x, top, t_layout.rows.w, m_form_height * m_form_amount};

	const std::optional<u32> row = form_row(t_accounts);
	if (!row) return Rect{};

	const float height = t_layout.row_height + std::max(0.0f, m_form_height - t_layout.row_height) * m_form_amount;

	return Rect{t_layout.rows.x, top + *row * pitch(t_layout) + m_row_offsets[*row], t_layout.rows.w, height};
}

auto LibraryView::form_layout(Rect t_card) const -> LibraryView::FormLayout
{
	const Font& body      = m_fonts->body;
	const Font& secondary = m_fonts->secondary;
	const Rect  inner{t_card.x + K_FORM_PADDING, t_card.y + K_FORM_PADDING, std::max(0.0f, t_card.w - K_FORM_PADDING * 2.0f), 0.0f};

	FormLayout form{};
	form.title = Rect{inner.x, inner.y, inner.w, body.line_height()};

	const bool  two_columns = inner.w >= K_FORM_TWO_COLUMNS;
	const float column      = two_columns ? (inner.w - K_FORM_COLUMN_GAP) * 0.5f : inner.w;
	const float second_x    = two_columns ? inner.x + column + K_FORM_COLUMN_GAP : inner.x;
	const float label_h     = secondary.line_height();
	const float input_h     = std::max(K_INPUT_MIN_HEIGHT, body.line_height() + 12.0f);
	const float block       = label_h + K_FORM_LABEL_GAP + input_h;

	const auto place = [&](u32 t_slot, float t_x, float t_y) {
		form.labels[t_slot] = Rect{t_x, t_y, column, label_h};

		if (t_slot < K_FIELD_COUNT) {
			form.fields[t_slot] = Rect{t_x, t_y + label_h + K_FORM_LABEL_GAP, column, input_h};
		}
	};

	float y = form.title.bottom() + K_FORM_TITLE_GAP;

	place(K_USERNAME, inner.x, y);
	if (!two_columns) {
		y += block + K_FORM_ROW_GAP;
	}

	place(K_PASSWORD, second_x, y);
	y += block + K_FORM_ROW_GAP;

	place(K_NOTE, inner.x, y);
	if (!two_columns) {
		y += block + K_FORM_ROW_GAP;
	}

	place(K_FIELD_COUNT, second_x, y);
	form.region = Rect{second_x, y + label_h + K_FORM_LABEL_GAP, column, input_h};

	const float chip_h = input_h - K_CHIP_SHRINK;
	y                  = std::max(form.fields[K_NOTE].bottom(), form.region.bottom()) + K_FORM_ROW_GAP;

	form.has_games = m_library->game_count > 1;
	if (form.has_games) {
		form.labels[K_FIELD_COUNT + 1] = Rect{inner.x, y, inner.w, label_h};

		float game_x = inner.x;
		float game_y = y + label_h + K_FORM_LABEL_GAP;

		for (u32 game = 0; game < m_library->game_count; game += 1) {
			const float width = K_CHIP_PADDING_X * 2.0f + K_GAME_CHIP_ICON + K_GAME_CHIP_ICON_GAP + text_width(secondary, m_library->games[game].short_title);
			if (game_x > inner.x && game_x + width > inner.right()) {
				game_x = inner.x;
				game_y += chip_h + K_CHIP_GAP;
			}

			form.games[game] = Rect{game_x, game_y, width, chip_h};
			game_x += width + K_CHIP_GAP;
		}

		y = game_y + chip_h + K_FORM_ROW_GAP;
	}

	y += K_FORM_FOOTER_GAP - K_FORM_ROW_GAP;

	const float save_width   = std::max(K_BUTTON_MIN_WIDTH, text_width(body, "Save") + K_BUTTON_PADDING_X * 2.0f);
	const float cancel_width = std::max(K_BUTTON_MIN_WIDTH, text_width(body, "Cancel") + K_BUTTON_PADDING_X * 2.0f);
	form.save                = Rect{inner.right() - save_width, y, save_width, K_BUTTON_HEIGHT};
	form.cancel              = Rect{form.save.x - K_BUTTON_GAP - cancel_width, y, cancel_width, K_BUTTON_HEIGHT};

	if (m_form_account) {
		const std::string_view label     = m_delete_armed ? K_DELETE_ARMED : K_DELETE_LABEL;
		const float            countdown = m_delete_armed ? K_COUNTDOWN_SIZE + K_COUNTDOWN_GAP : 0.0f;
		form.remove = Rect{inner.x - K_DELETE_PADDING_X, y, text_width(secondary, label) + countdown + K_DELETE_PADDING_X * 2.0f, K_BUTTON_HEIGHT};
	}

	const Rect password = form.fields[K_PASSWORD];
	form.reveal =
		Rect{password.right() - K_REVEAL_SIZE - K_REVEAL_MARGIN, snapped_to_pixel(password.center().y - K_REVEAL_SIZE * 0.5f), K_REVEAL_SIZE, K_REVEAL_SIZE};
	form.height = form.save.bottom() + K_FORM_PADDING - t_card.y;

	return form;
}

auto LibraryView::field_text_rect(const FormLayout& t_form, u32 t_field) const -> Rect
{
	Rect text = t_form.fields[t_field].inset(K_INPUT_PADDING_X, 0.0f);

	if (t_field == K_PASSWORD) {
		text.w = std::max(0.0f, text.w - K_REVEAL_SIZE);
	}

	return text;
}

auto LibraryView::row_rect(const Layout& t_layout, const VisibleAccounts& t_accounts, u32 t_row) const -> Rect
{
	const float y = t_layout.rows.y - m_scroll.offset() + t_row * pitch(t_layout) + m_row_offsets[t_row];

	// A new account's form sits above every row; an edited row's form only pushes down the rows after it.
	const std::optional<u32> edited = form_row(t_accounts);
	const bool               pushed = form_shown() && (!m_form_account || (edited && t_row > *edited));

	return Rect{t_layout.rows.x, pushed ? y + form_extra(t_layout) : y, t_layout.rows.w, t_layout.row_height};
}

auto LibraryView::lifted_rect(const Layout& t_layout) const -> Rect
{
	return Rect{t_layout.rows.x, t_layout.rows.y - m_scroll.offset() + lifted_top(t_layout, shown_accounts()), t_layout.rows.w, t_layout.row_height};
}

auto LibraryView::star_rect(const Layout& t_layout, Rect t_row) const -> Rect
{
	const float x = t_row.x + t_layout.columns.star + (K_STAR_COLUMN - K_STAR_SIZE) * 0.5f;

	return Rect{x, snapped_to_pixel(t_row.center().y - K_STAR_SIZE * 0.5f), K_STAR_SIZE, K_STAR_SIZE};
}

auto LibraryView::login_rect(const Layout&, Rect t_row, std::string_view t_label) const -> Rect
{
	const float width = text_width(m_fonts->secondary, t_label) + K_PILL_PADDING_X * 2.0f;

	return Rect{t_row.right() - K_ROW_INSET - width, snapped_to_pixel(t_row.center().y - K_PILL_HEIGHT * 0.5f), width, K_PILL_HEIGHT};
}

auto LibraryView::edit_rect(const Layout& t_layout, Rect t_row) const -> Rect
{
	const Rect login = login_rect(t_layout, t_row, K_LOGIN_LABEL);

	return Rect{login.x - K_ACTION_GAP - K_EDIT_SIZE, snapped_to_pixel(t_row.center().y - K_EDIT_SIZE * 0.5f), K_EDIT_SIZE, K_EDIT_SIZE};
}

auto LibraryView::content_height(const Layout& t_layout, const VisibleAccounts& t_accounts) const -> float
{
	const float rows = t_accounts.count == 0 ? 0.0f : t_accounts.count * pitch(t_layout) - K_ROW_GAP;

	return rows + form_extra(t_layout) + K_PADDING_BOTTOM;
}

auto LibraryView::scroll_geometry(const Layout& t_layout, const VisibleAccounts& t_accounts) const -> ScrollGeometry
{
	const Rect track{m_bounds.right() - K_SCROLLBAR_WIDTH - K_SCROLLBAR_MARGIN, t_layout.rows.y + K_SCROLLBAR_MARGIN, K_SCROLLBAR_WIDTH,
	                 std::max(0.0f, t_layout.rows.h - K_SCROLLBAR_MARGIN * 2.0f)};

	return ScrollGeometry{track, content_height(t_layout, t_accounts), t_layout.rows.h};
}

auto LibraryView::empty_state(const Layout& t_layout) const -> LibraryView::EmptyState
{
	const Font& body      = m_fonts->body;
	const Font& secondary = m_fonts->secondary;
	const float button    = std::max(30.0f, body.line_height() + 12.0f);
	const float text      = body.line_height() + K_EMPTY_LINE_GAP + secondary.line_height();
	const float stack     = K_EMPTY_ICON_SIZE + K_EMPTY_GAP + text + K_EMPTY_GAP + button;
	const Rect  area{m_bounds.x, t_layout.labels.y, m_bounds.w, std::max(0.0f, m_bounds.bottom() - t_layout.labels.y)};
	const float center_x = area.center().x;
	const float top      = snapped_to_pixel(std::max(area.y, area.center().y - stack * 0.5f - K_HEADER_GAP));

	EmptyState state{};
	state.icon           = Rect{snapped_to_pixel(center_x - K_EMPTY_ICON_SIZE * 0.5f), top, K_EMPTY_ICON_SIZE, K_EMPTY_ICON_SIZE};
	state.title_baseline = state.icon.bottom() + K_EMPTY_GAP + body.ascent;
	state.hint_baseline  = state.icon.bottom() + K_EMPTY_GAP + body.line_height() + K_EMPTY_LINE_GAP + secondary.ascent;
	state.button =
		Rect{snapped_to_pixel(center_x - K_EMPTY_BUTTON_WIDTH * 0.5f), state.icon.bottom() + K_EMPTY_GAP + text + K_EMPTY_GAP, K_EMPTY_BUTTON_WIDTH, button};

	return state;
}

auto LibraryView::shows_login(AccountRef t_account) const -> bool
{
	if (!has_game() || !m_session->shows(static_cast<u32>(m_game), t_account)) return false;
	if (!m_session->is_finished() || m_session->asks_for_permission()) return true;

	const float linger = m_session->stage() == LoginStage::SUCCESS ? K_SUCCESS_LINGER_SECONDS : K_FAILURE_LINGER_SECONDS;

	return m_session->finished_seconds() < linger;
}

auto LibraryView::login_action(AccountRef t_account) const -> std::string_view
{
	if (!shows_login(t_account)) return "";
	if (!m_session->is_finished()) return K_CANCEL_LABEL;
	if (m_session->asks_for_permission()) return K_PERMISSION_LABEL;
	if (m_session->stage() == LoginStage::SUCCESS) return "";

	return K_RETRY_LABEL;
}

auto LibraryView::hit_at(Vec2 t_point) const -> LibraryView::Hit
{
	if (!has_game() || !m_bounds.contains(t_point)) return Hit{};

	const Layout layout = this->layout();

	if (!m_filter.value().empty() && controls::search_clear_rect(layout.filter).contains(t_point)) return Hit{Target::FILTER_CLEAR};
	if (layout.filter.contains(t_point)) return Hit{Target::FILTER};
	if (layout.add.contains(t_point)) return Hit{Target::ADD};

	const VisibleAccounts accounts = shown_accounts();

	if (accounts.count == 0 && !form_shown()) {
		const bool empty = m_filter.value().empty();
		return empty && empty_state(layout).button.contains(t_point) ? Hit{Target::ADD} : Hit{};
	}

	if (!layout.rows.contains(t_point) || m_scroll.is_over_track(t_point, scroll_geometry(layout, accounts))) return Hit{};

	if (form_shown()) {
		const Rect card = form_card(layout, accounts);
		if (card.contains(t_point)) return m_form_open && m_form_amount > 0.5f ? form_hit(card, t_point) : Hit{Target::FORM};
	}

	const std::optional<u32> edited = form_row(accounts);

	for (u32 row = 0; row < accounts.count; row += 1) {
		if (edited == row) continue;

		const Rect rect = row_rect(layout, accounts, row);
		if (rect.contains(t_point)) return row_hit(layout, rect, accounts.refs[row], row, t_point);
	}

	return Hit{};
}

auto LibraryView::row_hit(const Layout& t_layout, Rect t_row, AccountRef t_account, u32 t_index, Vec2 t_point) const -> LibraryView::Hit
{
	if (star_rect(t_layout, t_row).contains(t_point)) return Hit{Target::STAR, t_index};

	if (shows_login(t_account)) {
		const std::string_view action = login_action(t_account);
		if (action.empty() || !login_rect(t_layout, t_row, action).contains(t_point)) return Hit{Target::ROW, t_index};
		if (action == K_CANCEL_LABEL) return Hit{Target::CANCEL_LOGIN, t_index};
		if (action == K_PERMISSION_LABEL) return Hit{Target::PERMISSION, t_index};

		return Hit{Target::LOGIN, t_index};
	}

	if (edit_rect(t_layout, t_row).contains(t_point)) return Hit{Target::EDIT, t_index};
	if (login_rect(t_layout, t_row, K_LOGIN_LABEL).contains(t_point)) return Hit{Target::LOGIN, t_index};

	return Hit{Target::ROW, t_index};
}

auto LibraryView::form_hit(Rect t_card, Vec2 t_point) const -> LibraryView::Hit
{
	const FormLayout form = form_layout(t_card);

	if (form.reveal.contains(t_point)) return Hit{Target::REVEAL};

	for (u32 i = 0; i < K_FIELD_COUNT; i += 1) {
		if (form.fields[i].contains(t_point)) return Hit{Target::FIELD, i};
	}

	if (form.region.contains(t_point)) return Hit{Target::REGION};

	if (form.has_games) {
		for (u32 game = 0; game < m_library->game_count; game += 1) {
			if (form.games[game].contains(t_point)) return Hit{Target::SHOW_IN, game};
		}
	}

	if (m_form_account && form.remove.contains(t_point)) return Hit{Target::DELETE};
	if (form.cancel.contains(t_point)) return Hit{Target::CANCEL_FORM};
	if (form.save.contains(t_point)) return Hit{Target::SAVE};

	return Hit{Target::FORM};
}

auto LibraryView::row_entrance(u32 t_row) const -> float
{
	constexpr float SPAN = 1.0f - K_ROW_STAGGER * K_STAGGERED_ROWS;

	const float delay = static_cast<float>(std::min(t_row, K_STAGGERED_ROWS)) * K_ROW_STAGGER;

	return eased_out(std::clamp((m_appear - delay) / SPAN, 0.0f, 1.0f));
}

// Favourites and the rest are reordered separately, the same as in the account list.
auto LibraryView::drag_range(const VisibleAccounts& t_accounts, u32 t_row) const -> LibraryView::RowRange
{
	u32 favorites = 0;
	while (favorites < t_accounts.count && m_library->account(t_accounts.refs[favorites])->favorite) {
		favorites += 1;
	}

	if (t_row < favorites) return RowRange{0, favorites - 1};

	return RowRange{favorites, t_accounts.count - 1};
}

auto LibraryView::lifted_top(const Layout& t_layout, const VisibleAccounts& t_accounts) const -> float
{
	const RowRange range   = drag_range(t_accounts, m_drag.from_row);
	const float    pointer = m_mouse.y - t_layout.rows.y + m_scroll.offset();
	const float    step    = pitch(t_layout);

	return std::clamp(pointer - m_drag.grab_offset, range.first * step, range.last * step);
}

auto LibraryView::focused_field() const -> std::optional<u32>
{
	for (u32 i = 0; i < K_FIELD_COUNT; i += 1) {
		if (m_fields[i].is_focused()) return i;
	}

	return std::nullopt;
}

auto LibraryView::activate(Hit t_hit) -> void
{
	const VisibleAccounts accounts = shown_accounts();
	const bool            on_row   = is_row_target(t_hit.target) && t_hit.index < accounts.count;
	const AccountRef      account  = on_row ? accounts.refs[t_hit.index] : AccountRef{};

	switch (t_hit.target) {
		using enum Target;

		case ROW:
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

		case EDIT: {
			if (on_row) {
				edit(account);
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
			m_fields[K_PASSWORD].set_masked(!m_fields[K_PASSWORD].is_masked());
			break;
		}

		case REGION: {
			open_region_list();
			break;
		}

		case SHOW_IN: {
			toggle_shown_game(t_hit.index);
			break;
		}

		case DELETE: {
			if (!m_form_account) break;

			if (m_delete_armed) {
				delete_account(*m_form_account);
			} else {
				m_delete_armed  = true;
				m_armed_seconds = K_DELETE_CONFIRM_SECONDS;
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
	if (!has_game()) return;

	blur();
	m_session->request(static_cast<u32>(m_game), t_account);
}

auto LibraryView::toggle_favorite(AccountRef t_account) -> void
{
	if (t_account.game >= m_library->game_count || t_account.index >= m_library->games[t_account.game].account_count) return;

	const VisibleAccounts before = shown_accounts();

	Account* account  = m_library->account(t_account);
	account->favorite = !account->favorite;

	animate_reorder(before);

	if (const std::optional<u32> row = row_of(shown_accounts(), t_account)) {
		m_raised_row = *row;
	}
}

auto LibraryView::edit(AccountRef t_account) -> void
{
	if (t_account.game >= m_library->game_count || t_account.index >= m_library->games[t_account.game].account_count) return;

	if (!row_of(shown_accounts(), t_account)) {
		m_filter.set_value("");
		refresh_filter();
	}

	open_form(t_account);
}

auto LibraryView::open_form(std::optional<AccountRef> t_account) -> void
{
	if (!has_game()) return;

	if (m_form_open && m_form_account == t_account) {
		focus_field(K_USERNAME);
		return;
	}

	cancel_drag();

	if (form_shown()) {
		close_form(false);
	}

	m_form_open     = true;
	m_form_account  = t_account;
	m_form_amount   = 0.0f;
	m_show_required = false;
	m_delete_armed  = false;
	m_region_list.close();

	if (t_account) {
		const Account* account = m_library->account(*t_account);
		m_fields[K_USERNAME].set_value(account->username);
		m_fields[K_PASSWORD].set_value(account->password);
		m_fields[K_NOTE].set_value(account->note);
		copy_to(std::string_view{account->region}, m_region);
		m_visible_mask = account->visible_games(t_account->game);
	} else {
		for (TextInput& field : m_fields) {
			field.set_value("");
		}

		m_region[0]    = '\0';
		m_visible_mask = static_cast<u16>(1u << static_cast<u32>(m_game));
	}

	m_fields[K_PASSWORD].set_masked(true);
	focus_filter(false);
	focus_field(K_USERNAME);

	m_form_height = form_layout(Rect{0.0f, 0.0f, layout().rows.w, 0.0f}).height;
}

auto LibraryView::close_form(bool t_animated) -> void
{
	m_form_open = false;
	focus_field(std::nullopt);
	m_region_list.close();

	if (!t_animated) {
		m_form_amount = 0.0f;
		clear_form();
	}
}

auto LibraryView::clear_form() -> void
{
	for (TextInput& field : m_fields) {
		field.set_value("");
	}

	sodium_memzero(m_region, sizeof(m_region));
	m_form_account.reset();
	m_show_required = false;
	m_delete_armed  = false;
}

auto LibraryView::save_form() -> void
{
	if (!has_game() || !m_form_open) return;

	const std::string_view username = m_fields[K_USERNAME].value();
	const std::string_view password = m_fields[K_PASSWORD].value();
	const std::string_view note     = m_fields[K_NOTE].value();

	if (username.empty() || password.empty()) {
		m_show_required = true;
		focus_field(username.empty() ? K_USERNAME : K_PASSWORD);
		return;
	}

	if (m_form_account) {
		Account* account = m_library->account(*m_form_account);
		account->assign(username, note, password);
		copy_to(std::string_view{m_region}, account->region);
		account->visible_game_mask = m_visible_mask;

		m_flash_account = m_form_account;
		m_flash         = 1.0f;
		close_form(true);
		return;
	}

	Account account{.visible_game_mask = m_visible_mask};
	account.assign(username, note, password);
	copy_to(std::string_view{m_region}, account.region);

	const std::optional<AccountRef> added = m_library->add_account(static_cast<u32>(m_game), account);
	sodium_memzero(&account, sizeof(account));

	if (!added) {
		m_toasts->notify(Notification{.message = "This game can't hold any more accounts."});
		return;
	}

	m_session->follow_insert(*added);
	shift_after_insert(&m_flash_account, *added);

	m_flash_account = added;
	m_flash         = 1.0f;
	close_form(true);
	reveal_row(*added);
}

auto LibraryView::delete_account(AccountRef t_account) -> void
{
	forget_deleted();

	const Layout              layout = this->layout();
	const VisibleAccounts     before = shown_accounts();
	const std::optional<u32>  edited = form_row(before);
	const float               extra  = form_extra(layout);
	const std::optional<u32>  gone   = row_of(before, t_account);
	std::optional<AccountRef> flash  = m_flash_account;

	m_deleted.emplace();
	m_deleted->account  = *m_library->account(t_account);
	m_deleted->position = t_account;

	m_library->remove_account(t_account);
	m_session->follow_removal(t_account);
	shift_after_removal(&flash, t_account);
	m_flash_account = flash;
	close_form(false);

	animate_reorder(before);

	// The rows under the form were pushed down by it, so they start from there and slide up.
	if (edited && gone) {
		const VisibleAccounts after = shown_accounts();

		for (u32 i = 0; i < after.count; i += 1) {
			if (i >= *gone) {
				m_row_offsets[i] += extra;
			}
		}
	}

	m_toasts->notify(Notification{
		.message     = "Account deleted. Click to undo.",
		.on_click    = Command{.type = CommandType::UNDO_LIBRARY_DELETE},
		.seconds     = K_UNDO_SECONDS,
		.always_show = true,
	});
}

auto LibraryView::undo_delete() -> void
{
	if (!m_deleted) return;

	const VisibleAccounts           before   = shown_accounts();
	const std::optional<AccountRef> restored = m_library->insert_account(m_deleted->position, m_deleted->account);
	forget_deleted();

	if (!restored) {
		m_toasts->notify(Notification{.message = "There's no room left to restore that account."});
		return;
	}

	m_session->follow_insert(*restored);
	shift_after_insert(&m_flash_account, *restored);
	animate_reorder(before);

	m_flash_account = restored;
	m_flash         = 1.0f;
	reveal_row(*restored);
}

auto LibraryView::forget_deleted() -> void
{
	if (!m_deleted) return;

	sodium_memzero(&m_deleted->account, sizeof(Account));
	m_deleted.reset();
}

auto LibraryView::forget_secrets() -> void
{
	close_form(false);
	reset_row_motion();
	blur();
	forget_deleted();

	m_filter.set_value("");
	sodium_memzero(m_applied_filter, sizeof(m_applied_filter));
	m_flash_account.reset();
	m_pressed = Hit{};
}

auto LibraryView::focus_field(std::optional<u32> t_field) -> void
{
	for (u32 i = 0; i < K_FIELD_COUNT; i += 1) {
		m_fields[i].set_focused(t_field == i);
	}
}

auto LibraryView::focus_filter(bool t_focused) -> void
{
	m_filter.set_focused(t_focused);

	if (t_focused) {
		focus_field(std::nullopt);
	}
}

auto LibraryView::blur() -> void
{
	focus_filter(false);
	focus_field(std::nullopt);
	m_region_list.close();
}

auto LibraryView::open_region_list() -> void
{
	focus_field(std::nullopt);
	m_region_list.open(K_REGION_LABELS, region_index(m_region));
}

auto LibraryView::toggle_shown_game(u32 t_game) -> void
{
	if (t_game >= m_library->game_count) return;

	const auto bit        = static_cast<u16>(1u << t_game);
	const bool last_shown = (m_visible_mask & bit) != 0 && std::popcount(m_visible_mask) == 1;

	if (!last_shown) {
		m_visible_mask ^= bit;
	}
}

auto LibraryView::refresh_filter() -> void
{
	const std::string_view query = m_filter.value();
	if (query == std::string_view{m_applied_filter}) return;

	copy_to(query, m_applied_filter);
	reset_row_motion();
	m_scroll  = Scrollable{};
	m_pressed = Hit{};
}

auto LibraryView::reveal_row(AccountRef t_account) -> void
{
	const Layout             layout   = this->layout();
	const VisibleAccounts    accounts = shown_accounts();
	const std::optional<u32> row      = row_of(accounts, t_account);
	if (!row) return;

	const Rect rect = row_rect(layout, accounts, *row);
	m_scroll.reveal(rect.y, rect.bottom(), layout.rows.y, layout.rows.bottom(), scroll_geometry(layout, accounts));
}

auto LibraryView::lift_row(u32 t_row, Vec2 t_point) -> void
{
	const Layout layout  = this->layout();
	const float  top     = t_row * pitch(layout) + m_row_offsets[t_row];
	const float  pointer = t_point.y - layout.rows.y + m_scroll.offset();

	m_drag.lifted      = true;
	m_drag.from_row    = t_row;
	m_drag.target_row  = t_row;
	m_drag.grab_offset = pointer - top;
	m_raised_row       = t_row;
	m_pressed          = Hit{};
	blur();
}

auto LibraryView::drop_row() -> void
{
	if (!m_drag.lifted) {
		m_drag = RowDrag{};
		return;
	}

	const Layout          layout   = this->layout();
	const VisibleAccounts accounts = shown_accounts();
	const u32             from     = m_drag.from_row;
	const u32             to       = m_drag.target_row;

	if (from < accounts.count) {
		m_row_offsets[from] = lifted_top(layout, accounts) - from * pitch(layout);
	}

	m_drag = RowDrag{};

	if (from >= accounts.count || to >= accounts.count || !has_game()) {
		reset_row_motion();
		return;
	}

	if (from != to) {
		m_library->move_visible_account(static_cast<u32>(m_game), from, to);
		animate_reorder(accounts);
	}

	m_raised_row = to;
}

auto LibraryView::cancel_drag() -> void
{
	m_drag.target_row = m_drag.from_row;
	drop_row();
}

auto LibraryView::update_drag(float t_delta_seconds) -> void
{
	const Layout          layout   = this->layout();
	const VisibleAccounts accounts = shown_accounts();
	const float           step     = pitch(layout);

	if (m_drag.lifted && m_drag.from_row >= accounts.count) {
		reset_row_motion();
	}

	if (m_drag.lifted) {
		const float zone  = layout.row_height * K_AUTO_SCROLL_ZONE;
		const float above = layout.rows.y + zone - m_mouse.y;
		const float below = m_mouse.y - (layout.rows.bottom() - zone);
		const auto  geom  = scroll_geometry(layout, accounts);

		if (above > 0.0f) {
			m_scroll.scroll_by(-K_AUTO_SCROLL_SPEED * std::min(above / zone, 1.0f) * t_delta_seconds, geom);
		} else if (below > 0.0f) {
			m_scroll.scroll_by(K_AUTO_SCROLL_SPEED * std::min(below / zone, 1.0f) * t_delta_seconds, geom);
		}

		const RowRange range = drag_range(accounts, m_drag.from_row);
		const auto     slot  = static_cast<u32>(std::lround(lifted_top(layout, accounts) / step));
		m_drag.target_row    = std::clamp(slot, range.first, range.last);
		animation::request_frame();
	}

	for (u32 i = 0; i < accounts.count; i += 1) {
		if (m_drag.lifted && i == m_drag.from_row) continue;

		float target = 0.0f;
		if (m_drag.lifted && m_drag.from_row < i && i <= m_drag.target_row) {
			target = -step;
		} else if (m_drag.lifted && m_drag.target_row <= i && i < m_drag.from_row) {
			target = step;
		}

		m_row_offsets[i] = animation::ease_toward(m_row_offsets[i], target, K_SHIFT_EASE_RATE, t_delta_seconds, animation::K_SETTLED_PIXELS);
	}

	m_lift_amount = animation::ease_toward(m_lift_amount, m_drag.lifted ? 1.0f : 0.0f, K_LIFT_EASE_RATE, t_delta_seconds);

	const bool raised_settled = !m_raised_row || *m_raised_row >= accounts.count || m_row_offsets[*m_raised_row] == 0.0f;
	if (!m_drag.lifted && m_lift_amount == 0.0f && raised_settled) {
		m_raised_row.reset();
	}
}

// Rows that changed places start where they were and slide to where they are now.
auto LibraryView::animate_reorder(const VisibleAccounts& t_before) -> void
{
	const VisibleAccounts after = shown_accounts();
	const float           step  = pitch(layout());
	float                 offsets[K_MAX_VISIBLE_ACCOUNTS]{};

	for (u32 i = 0; i < after.count; i += 1) {
		for (u32 j = 0; j < t_before.count; j += 1) {
			if (t_before.refs[j] != after.refs[i]) continue;

			offsets[i] = (static_cast<float>(j) - static_cast<float>(i)) * step + m_row_offsets[j];
			break;
		}
	}

	std::copy(std::begin(offsets), std::end(offsets), std::begin(m_row_offsets));
}

auto LibraryView::reset_row_motion() -> void
{
	m_drag = RowDrag{};
	std::fill(std::begin(m_row_offsets), std::end(m_row_offsets), 0.0f);
	m_raised_row.reset();
	m_lift_amount = 0.0f;
}

auto LibraryView::update(float t_delta_seconds) -> void
{
	m_appear = animation::step_toward(m_appear, 1.0f, K_APPEAR_SECONDS, t_delta_seconds);
	m_scroll.update(t_delta_seconds);
	m_filter.update(t_delta_seconds);

	for (TextInput& field : m_fields) {
		field.update(t_delta_seconds);
	}

	refresh_filter();

	if (m_deleted && !m_toasts->is_offering(CommandType::UNDO_LIBRARY_DELETE)) {
		forget_deleted();
	}

	const Layout          layout   = this->layout();
	const VisibleAccounts accounts = shown_accounts();

	if (m_form_open && m_form_account && !row_of(accounts, *m_form_account)) {
		close_form(false);
	}

	if (form_shown()) {
		const float target = form_layout(Rect{layout.rows.x, 0.0f, layout.rows.w, 0.0f}).height;
		m_form_height      = animation::ease_toward(m_form_height, target, K_FORM_EASE_RATE, t_delta_seconds, animation::K_SETTLED_PIXELS);
		m_form_amount      = animation::ease_toward(m_form_amount, m_form_open ? 1.0f : 0.0f, K_FORM_EASE_RATE, t_delta_seconds,
		                                            animation::K_SETTLED_PIXELS / std::max(m_form_height, 1.0f));

		if (m_form_open && (m_form_amount < 1.0f || m_form_height != target)) {
			const Rect card = form_card(layout, accounts);
			m_scroll.reveal(card.y, card.y + m_form_height, layout.rows.y, layout.rows.bottom(), scroll_geometry(layout, accounts));
		}

		if (!m_form_open && m_form_amount == 0.0f) {
			clear_form();
		}
	}

	if (m_delete_armed) {
		m_armed_seconds -= t_delta_seconds;
		animation::request_frame();

		if (m_armed_seconds <= 0.0f) {
			m_delete_armed = false;
		}
	}

	if (m_flash_account) {
		m_flash = animation::step_toward(m_flash, 0.0f, K_FLASH_SECONDS, t_delta_seconds);
		if (m_flash == 0.0f) {
			m_flash_account.reset();
		}
	}

	if (m_session->is_finished() && has_game()) {
		const float linger = m_session->stage() == LoginStage::SUCCESS ? K_SUCCESS_LINGER_SECONDS : K_FAILURE_LINGER_SECONDS;
		if (m_session->finished_seconds() < linger) {
			animation::request_frame_after(linger - m_session->finished_seconds());
		}
	}

	m_hovered = m_scroll.is_dragging() || m_drag.lifted ? Hit{} : hit_at(m_mouse);

	const bool add_lit = m_hovered.target == Target::ADD;
	m_add_hover        = animation::ease_toward(m_add_hover, add_lit ? 1.0f : 0.0f, K_HOVER_EASE_RATE, t_delta_seconds);

	for (u32 row = 0; row < accounts.count; row += 1) {
		const bool lit   = is_row_target(m_hovered.target) && m_hovered.index == row;
		m_row_hover[row] = animation::ease_toward(m_row_hover[row], lit ? 1.0f : 0.0f, K_HOVER_EASE_RATE, t_delta_seconds);
	}

	update_drag(t_delta_seconds);

	const Rect region = form_shown() ? form_layout(form_card(layout, accounts)).region : Rect{};
	m_region_list.update(t_delta_seconds, region, m_bounds);
}

auto LibraryView::on_pointer_down(Vec2 t_point) -> void
{
	if (m_region_list.is_open()) {
		m_region_list.on_pointer_down(t_point);
		return;
	}

	const Layout          layout   = this->layout();
	const VisibleAccounts accounts = shown_accounts();
	if (m_scroll.on_pointer_down(t_point, scroll_geometry(layout, accounts))) return;

	const Hit hit = hit_at(t_point);
	m_pressed     = hit;

	if (hit.target != Target::FILTER && hit.target != Target::FILTER_CLEAR) {
		focus_filter(false);
	}

	if (hit.target != Target::FIELD && hit.target != Target::REVEAL && hit.target != Target::FORM) {
		focus_field(std::nullopt);
	}

	switch (hit.target) {
		using enum Target;

		case FILTER: {
			focus_filter(true);
			m_filter.on_pointer_down(m_fonts->secondary, controls::search_text_rect(layout.filter, K_FILTER_INSET), t_point.x);
			break;
		}

		case FIELD: {
			const FormLayout form = form_layout(form_card(layout, accounts));

			focus_field(hit.index);
			m_fields[hit.index].on_pointer_down(m_fonts->body, field_text_rect(form, hit.index), t_point.x);
			break;
		}

		case ROW: {
			m_drag.pressed_row = hit.index;
			m_drag.press_point = t_point;
			break;
		}

		default: {
			break;
		}
	}
}

auto LibraryView::on_pointer_move(Vec2 t_point) -> bool
{
	if (m_region_list.is_open()) {
		m_region_list.on_pointer_move(t_point);
		return true;
	}

	const Layout          layout   = this->layout();
	const VisibleAccounts accounts = shown_accounts();
	bool                  handled  = false;

	if (m_filter.is_selecting()) {
		m_filter.on_pointer_move(m_fonts->secondary, controls::search_text_rect(layout.filter, K_FILTER_INSET), t_point.x);
		handled = true;
	}

	if (form_shown()) {
		const FormLayout form = form_layout(form_card(layout, accounts));

		for (u32 i = 0; i < K_FIELD_COUNT; i += 1) {
			if (m_fields[i].is_selecting()) {
				m_fields[i].on_pointer_move(m_fonts->body, field_text_rect(form, i), t_point.x);
				handled = true;
			}
		}
	}

	if (m_scroll.is_dragging()) {
		m_scroll.on_pointer_move(t_point.y, scroll_geometry(layout, accounts));
		return true;
	}

	const bool can_reorder = m_filter.value().empty() && !form_shown();

	if (m_drag.pressed_row && !m_drag.lifted && can_reorder) {
		const float dx = t_point.x - m_drag.press_point.x;
		const float dy = t_point.y - m_drag.press_point.y;

		if (dx * dx + dy * dy > K_DRAG_THRESHOLD * K_DRAG_THRESHOLD && *m_drag.pressed_row < accounts.count) {
			lift_row(*m_drag.pressed_row, t_point);
		}
	}

	return handled || m_drag.lifted;
}

auto LibraryView::on_pointer_up(Vec2 t_point) -> bool
{
	if (m_region_list.is_open()) {
		if (const std::optional<u32> chosen = m_region_list.on_pointer_up(t_point)) {
			copy_to(K_REGION_OPTIONS[std::min<usize>(*chosen, K_REGION_COUNT - 1)].code, m_region);
		}

		m_pressed = Hit{};
		return true;
	}

	m_filter.on_pointer_up();

	for (TextInput& field : m_fields) {
		field.on_pointer_up();
	}

	if (m_scroll.is_dragging()) {
		m_scroll.on_pointer_up();
		m_pressed = Hit{};
		m_drag    = RowDrag{};
		return true;
	}

	if (m_drag.lifted) {
		drop_row();
		m_pressed = Hit{};
		return true;
	}

	m_drag.pressed_row.reset();

	const Hit pressed = std::exchange(m_pressed, Hit{});
	if (pressed.target == Target::NONE) return false;

	if (hit_at(t_point) == pressed) {
		activate(pressed);
	}

	return true;
}

auto LibraryView::on_right_click(Vec2 t_point) -> void
{
	const Hit             hit      = hit_at(t_point);
	const Layout          layout   = this->layout();
	const VisibleAccounts accounts = shown_accounts();

	if (hit.target == Target::FILTER) {
		focus_filter(true);
		m_filter.on_right_click(m_fonts->secondary, controls::search_text_rect(layout.filter, K_FILTER_INSET), t_point.x);
		m_commands->push(Command{.type = CommandType::SHOW_TEXT_MENU, .position = t_point, .text_input = &m_filter});
		return;
	}

	if (hit.target == Target::FIELD) {
		const FormLayout form = form_layout(form_card(layout, accounts));

		focus_field(hit.index);
		m_fields[hit.index].on_right_click(m_fonts->body, field_text_rect(form, hit.index), t_point.x);
		m_commands->push(Command{.type = CommandType::SHOW_TEXT_MENU, .position = t_point, .text_input = &m_fields[hit.index]});
		return;
	}

	if (is_row_target(hit.target) && hit.index < accounts.count && !m_drag.lifted) {
		m_commands->push(Command{.type = CommandType::SHOW_ACCOUNT_MENU, .index = -1, .position = t_point, .account = accounts.refs[hit.index]});
	}
}

auto LibraryView::on_scroll(float t_wheel_delta) -> void
{
	if (m_region_list.is_open()) {
		m_region_list.on_scroll(t_wheel_delta);
		return;
	}

	m_scroll.on_scroll(t_wheel_delta, scroll_geometry(layout(), shown_accounts()));
}

auto LibraryView::on_key_down(os::Key t_key) -> bool
{
	if (m_region_list.is_open()) {
		if (const std::optional<u32> chosen = m_region_list.on_key_down(t_key)) {
			copy_to(K_REGION_OPTIONS[std::min<usize>(*chosen, K_REGION_COUNT - 1)].code, m_region);
		}

		return true;
	}

	if (m_drag.lifted) {
		if (t_key == os::Key::ESCAPE) {
			cancel_drag();
		}

		return true;
	}

	if (const std::optional<u32> field = focused_field()) {
		switch (t_key) {
			using enum os::Key;

			case TAB: {
				const u32 step = os::modifiers().shift ? K_FIELD_COUNT - 1 : 1;
				focus_field((*field + step) % K_FIELD_COUNT);
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
				m_fields[*field].on_key_down(t_key);
				return true;
			}
		}
	}

	if (m_filter.is_focused()) {
		if (t_key == os::Key::ESCAPE && !m_filter.value().empty()) {
			m_filter.set_value("");
			refresh_filter();
		} else if (t_key == os::Key::ESCAPE || t_key == os::Key::ENTER) {
			focus_filter(false);
		} else {
			m_filter.on_key_down(t_key);
		}

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
				focus_field(K_USERNAME);
				return true;
			}

			default: {
				break;
			}
		}
	}

	return false;
}

// Typing anywhere in the library starts filtering.
auto LibraryView::on_char(u32 t_character) -> bool
{
	if (m_region_list.is_open()) {
		m_region_list.on_char(t_character);
		return true;
	}

	if (const std::optional<u32> field = focused_field()) {
		m_fields[*field].on_char(t_character);
		return true;
	}

	if (m_filter.is_focused()) {
		m_filter.on_char(t_character);
		return true;
	}

	if (!has_game() || m_form_open || m_drag.lifted || t_character <= ' ' || t_character == 0x7F) return false;

	focus_filter(true);
	m_filter.on_char(t_character);

	return true;
}

auto LibraryView::drop_press() -> void
{
	m_pressed = Hit{};

	if (m_drag.lifted) {
		drop_row();
	} else {
		m_drag = RowDrag{};
	}

	if (m_scroll.is_dragging()) {
		m_scroll.on_pointer_up();
	}
}

auto LibraryView::cursor() const -> CursorKind
{
	if (m_region_list.is_open()) return m_region_list.cursor(m_mouse);
	if (m_drag.lifted) return CursorKind::MOVE;
	if (m_scroll.is_dragging()) return CursorKind::DRAG;
	if (m_filter.is_selecting() || std::ranges::any_of(m_fields, [](const TextInput& t_field) { return t_field.is_selecting(); })) return CursorKind::I_BEAM;

	const Hit hit = hit_at(m_mouse);

	switch (hit.target) {
		using enum Target;

		case FILTER:
		case FIELD: {
			return CursorKind::I_BEAM;
		}

		case NONE:
		case FORM: {
			const bool over_track = m_scroll.is_over_track(m_mouse, scroll_geometry(layout(), shown_accounts()));
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
	const Game& game      = m_library->games[static_cast<u32>(m_game)];
	const Color accent    = m_settings->accent;

	draw_text_truncated(t_draw_list, title, Vec2{t_layout.header.x, t_layout.title_baseline}, game.title,
	                    std::max(0.0f, t_layout.filter.x - K_TITLE_GAP - t_layout.header.x), faded(g_theme.text, t_alpha));

	const VisibleAccounts all  = m_library->visible_accounts(static_cast<u32>(m_game));
	i64                   last = 0;
	for (const AccountRef ref : all.view()) {
		last = std::max(last, m_library->account(ref)->last_used);
	}

	char       count[32];
	char       played[64];
	char       relative[32];
	const auto total   = static_cast<unsigned>(all.count);
	const int  counted = all.count == 0 ? std::snprintf(count, sizeof(count), "No accounts yet")
	                                    : std::snprintf(count, sizeof(count), "%u account%s", total, total == 1 ? "" : "s");
	int        written = 0;
	if (last != 0) {
		const std::string_view when = relative_time(last, std::time(nullptr), relative);
		written                     = std::snprintf(played, sizeof(played), "last played %.*s", static_cast<int>(when.size()), when.data());
	}

	controls::draw_dotted(t_draw_list, secondary, Vec2{t_layout.header.x, t_layout.subtitle_baseline}, {count, static_cast<usize>(std::max(counted, 0))},
	                      {played, static_cast<usize>(std::max(written, 0))}, std::max(0.0f, t_layout.filter.x - K_TITLE_GAP - t_layout.header.x),
	                      faded(g_theme.text_faint, t_alpha));

	controls::draw_search_field(t_draw_list, secondary, t_layout.filter, K_FILTER_INSET, &m_filter, m_mouse, accent, t_alpha);

	const Rect  add    = t_layout.add;
	const Color border = mix(g_theme.separator, g_theme.border, m_add_hover);
	const Rect  plus{add.x + K_ADD_PADDING_X, snapped_to_pixel(add.center().y - K_ADD_ICON_SIZE * 0.5f), K_ADD_ICON_SIZE, K_ADD_ICON_SIZE};
	const Color ink = mix(g_theme.text_dim, g_theme.text, m_add_hover);

	t_draw_list->add_bordered_rect(add, rounded(K_CONTROL_RADIUS), faded(g_theme.row_hover, scaled_alpha(t_alpha, m_add_hover)), faded(border, t_alpha), 1.0f);
	t_draw_list->add_image(plus, m_assets->get(Asset::ICON_ADD), faded(ink, t_alpha));
	draw_text(t_draw_list, secondary, Vec2{plus.right() + K_ADD_ICON_GAP, secondary.centered_baseline(add)}, K_ADD_LABEL, faded(ink, t_alpha));
}

auto LibraryView::draw_labels(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) const -> void
{
	const Font&    caption  = m_fonts->caption;
	const Rect     labels   = t_layout.labels;
	const Columns& columns  = t_layout.columns;
	const float    baseline = caption.centered_baseline(labels);
	const Color    color    = faded(g_theme.text_faint, t_alpha);

	draw_text(t_draw_list, caption, Vec2{labels.x + columns.account, baseline}, "Account", color);

	if (columns.show_region) {
		draw_text(t_draw_list, caption, Vec2{labels.x + columns.region, baseline}, "Region", color);
	}

	if (columns.show_played) {
		draw_text(t_draw_list, caption, Vec2{labels.x + columns.played, baseline}, "Last played", color);
	}

	t_draw_list->add_rect(Rect{labels.x + K_ROW_INSET, labels.bottom() - 1.0f, labels.w - K_ROW_INSET * 2.0f, 1.0f}, faded(g_theme.separator, t_alpha));
}

auto LibraryView::draw_row(DrawList* t_draw_list, const Layout& t_layout, Rect t_row, AccountRef t_account, u32 t_index, bool t_raised, u8 t_alpha) const
	-> void
{
	const Font&    body      = m_fonts->body;
	const Font&    secondary = m_fonts->secondary;
	const Columns& columns   = t_layout.columns;
	const Account& account   = *m_library->account(t_account);
	const Color    accent    = m_settings->accent;
	const bool     logging   = shows_login(t_account);
	const float    hover     = t_raised ? 0.0f : m_row_hover[t_index];
	const bool     pressed   = !t_raised && m_pressed == Hit{Target::ROW, t_index} && m_hovered == m_pressed;

	if (t_raised && m_lift_amount > 0.0f) {
		const u8 lift = scaled_alpha(t_alpha, m_lift_amount);

		controls::draw_popup_shadow(t_draw_list, t_row, K_ROW_RADIUS, m_lift_amount * t_alpha / 255.0f);
		t_draw_list->add_bordered_rect(t_row, rounded(K_ROW_RADIUS), faded(g_theme.popup, lift), faded(g_theme.border, lift), 1.0f);
	}

	if (pressed) {
		t_draw_list->add_rounded_rect(t_row, rounded(K_ROW_RADIUS), faded(g_theme.row_selected, t_alpha));
	} else if (hover > 0.0f || logging) {
		t_draw_list->add_rounded_rect(t_row, rounded(K_ROW_RADIUS), faded(g_theme.row_hover, scaled_alpha(t_alpha, logging ? 1.0f : hover)));
	}

	if (m_flash_account == t_account && m_flash > 0.0f) {
		t_draw_list->add_rounded_rect(t_row, rounded(K_ROW_RADIUS), with_alpha(accent, static_cast<u8>(K_FLASH_ALPHA * m_flash * t_alpha / 255.0f)));
	}

	const Rect star         = star_rect(t_layout, t_row);
	const bool star_hovered = !t_raised && m_hovered == Hit{Target::STAR, t_index};

	if (star_hovered) {
		controls::draw_circular_hover(t_draw_list, star, g_theme.shadow, g_theme.control_hover, t_alpha);
	}

	if (account.favorite) {
		controls::draw_favorite(t_draw_list, m_assets, star.inset(K_STAR_ICON_INSET), true, faded(accent, t_alpha));
	} else if (hover > 0.0f) {
		controls::draw_favorite(t_draw_list, m_assets, star.inset(K_STAR_ICON_INSET), false,
		                        faded(star_hovered ? g_theme.text : g_theme.text_faint, scaled_alpha(t_alpha, hover)));
	}

	const float x           = t_row.x + columns.account;
	const bool  second_line = account.note[0] != '\0' || logging;
	const float block       = body.line_height() + K_ROW_LINE_GAP + secondary.line_height();
	const float block_y     = t_row.y + (t_row.h - block) * 0.5f;
	const float username_y  = second_line ? block_y + body.ascent : body.centered_baseline(t_row);
	const float details_y   = block_y + body.line_height() + K_ROW_LINE_GAP + secondary.ascent;

	draw_text_truncated(t_draw_list, body, Vec2{x, username_y}, account.username, columns.account_width, faded(g_theme.text, t_alpha));

	if (logging) {
		Color status = mix(g_theme.text_dim, accent, K_STATUS_TINT);
		if (m_session->is_finished()) {
			status = m_session->stage() == LoginStage::SUCCESS ? g_theme.success : g_theme.error;
		}

		const float change = m_session->status_change();

		draw_text_truncated(t_draw_list, secondary, Vec2{x, details_y - K_STATUS_RISE * change}, m_session->previous_status(), columns.account_width,
		                    faded(status, scaled_alpha(t_alpha, 1.0f - change)));
		draw_text_truncated(t_draw_list, secondary, Vec2{x, details_y + K_STATUS_RISE * (1.0f - change)}, m_session->status(), columns.account_width,
		                    faded(status, scaled_alpha(t_alpha, change)));
	} else if (account.note[0] != '\0') {
		draw_text_truncated(t_draw_list, secondary, Vec2{x, details_y}, account.note, columns.account_width, faded(g_theme.text_dim, t_alpha));
	}

	if (columns.show_region && account.region[0] != '\0') {
		controls::draw_region_chip(t_draw_list, secondary, t_row.x + columns.region, t_row.center().y, account.region, t_alpha);
	}

	const std::string_view action  = logging ? login_action(t_account) : std::string_view{};
	const Rect             button  = action.empty() ? Rect{} : login_rect(t_layout, t_row, action);
	const bool             covered = !action.empty() && button.x < t_row.x + columns.played + columns.played_width;

	if (columns.show_played && !covered) {
		char             relative[32];
		std::string_view played = "Never";
		Color            color  = g_theme.text_faint;

		if (account.last_used != 0) {
			played = relative_time(account.last_used, std::time(nullptr), relative);
			color  = g_theme.text_dim;
		}

		draw_text_truncated(t_draw_list, secondary, Vec2{t_row.x + columns.played, secondary.centered_baseline(t_row)}, played, columns.played_width,
		                    faded(color, t_alpha));
	}

	if (logging) {
		draw_login_state(t_draw_list, t_layout, t_row, t_account, t_index, t_alpha);
		return;
	}

	if (hover <= 0.0f) return;

	const Rect edit         = edit_rect(t_layout, t_row);
	const bool edit_hovered = m_hovered == Hit{Target::EDIT, t_index};
	const u8   shown        = scaled_alpha(t_alpha, hover);

	if (edit_hovered) {
		controls::draw_circular_hover(t_draw_list, edit, g_theme.shadow, g_theme.control_hover, t_alpha);
	}

	t_draw_list->add_image(edit.inset(K_EDIT_ICON_INSET), m_assets->get(Asset::ICON_EDIT), faded(edit_hovered ? g_theme.text : g_theme.text_dim, shown));

	const Rect login = login_rect(t_layout, t_row, K_LOGIN_LABEL).moved(Vec2{snapped_to_pixel((1.0f - hover) * K_PILL_SLIDE), 0.0f});
	draw_pill(t_draw_list, secondary, login, K_LOGIN_LABEL, accent, controls::ink_on(accent), shown);
}

auto LibraryView::draw_login_state(DrawList* t_draw_list, const Layout& t_layout, Rect t_row, AccountRef t_account, u32 t_index, u8 t_alpha) const -> void
{
	const Font&      secondary = m_fonts->secondary;
	const Color      accent    = m_settings->accent;
	const bool       finished  = m_session->is_finished();
	const LoginStage stage     = m_session->stage();
	const bool       succeeded = finished && stage == LoginStage::SUCCESS;
	const Color      outcome   = succeeded ? g_theme.success : g_theme.error;
	const Color      fill      = finished ? mix(accent, outcome, m_session->outcome()) : accent;

	const Rect track{t_row.x + K_PROGRESS_INSET, t_row.bottom() - K_PROGRESS_BOTTOM - K_PROGRESS_HEIGHT, t_row.w - K_PROGRESS_INSET * 2.0f, K_PROGRESS_HEIGHT};
	const Rect bar{track.x, track.y, track.w * std::clamp(m_session->progress(), 0.0f, 1.0f), track.h};

	t_draw_list->add_rounded_rect(track, rounded(track.h * 0.5f), faded(g_theme.control, t_alpha));
	if (bar.w > 0.5f) {
		t_draw_list->add_rounded_rect(bar, rounded(bar.h * 0.5f), faded(fill, t_alpha));
	}

	if (succeeded) {
		const Rect check{t_row.right() - K_ROW_INSET - K_CHECK_SIZE, snapped_to_pixel(t_row.center().y - K_CHECK_SIZE * 0.5f), K_CHECK_SIZE, K_CHECK_SIZE};
		controls::draw_check(t_draw_list, m_assets, check, faded(g_theme.success, t_alpha));
		return;
	}

	const std::string_view action = login_action(t_account);
	if (action.empty()) return;

	const Rect button    = login_rect(t_layout, t_row, action);
	const bool on_button = is_row_target(m_hovered.target) && m_hovered.target != Target::ROW && m_hovered.target != Target::STAR && m_hovered.index == t_index;

	if (action == K_CANCEL_LABEL) {
		const Color border = on_button ? g_theme.border : g_theme.separator;
		t_draw_list->add_bordered_rect(button, rounded(K_PILL_RADIUS), faded(on_button ? g_theme.row_selected : g_theme.row_hover, t_alpha),
		                               faded(border, t_alpha), 1.0f);
		draw_text_centered(t_draw_list, secondary, button, action, faded(on_button ? g_theme.text : g_theme.text_dim, t_alpha));
		return;
	}

	draw_pill(t_draw_list, secondary, button, action, on_button ? hovered(accent) : accent, controls::ink_on(accent), t_alpha);
}

auto LibraryView::draw_form(DrawList* t_draw_list, const Layout& t_layout, const VisibleAccounts& t_accounts, Rect t_card, u8 t_alpha) -> void
{
	if (t_card.h <= 0.5f) return;

	const Font&      body      = m_fonts->body;
	const Font&      secondary = m_fonts->secondary;
	const Font&      caption   = m_fonts->caption;
	const Color      accent    = m_settings->accent;
	const FormLayout form      = form_layout(t_card);
	const u8         content   = scaled_alpha(t_alpha, m_form_amount);
	const bool       live      = m_form_open && m_form_amount > 0.5f;

	t_draw_list->add_bordered_rect(t_card, rounded(K_FORM_RADIUS), faded(g_theme.popup, t_alpha), faded(g_theme.control_hover, t_alpha), 1.0f);

	if (m_form_account && m_form_amount < 1.0f) {
		if (const std::optional<u32> row = form_row(t_accounts)) {
			const Rect top{t_card.x, t_card.y, t_card.w, t_layout.row_height};
			draw_row(t_draw_list, t_layout, top, *m_form_account, *row, true, scaled_alpha(t_alpha, 1.0f - m_form_amount));
		}
	}

	t_draw_list->push_clip(t_card);

	const Rect                 icon{form.title.x, snapped_to_pixel(form.title.center().y - K_FORM_ICON_SIZE * 0.5f), K_FORM_ICON_SIZE, K_FORM_ICON_SIZE};
	const std::string_view     heading     = m_form_account ? std::string_view{m_library->account(*m_form_account)->username} : std::string_view{"New account"};
	constexpr std::string_view ESCAPE_HINT = "Esc to cancel";
	const float                hint_width  = text_width(caption, ESCAPE_HINT);

	t_draw_list->add_image(icon, m_assets->get(m_form_account ? Asset::ICON_EDIT : Asset::ICON_ADD), faded(g_theme.text_dim, content));
	draw_text_truncated(t_draw_list, body, Vec2{icon.right() + K_FORM_ICON_GAP, body.centered_baseline(form.title)}, heading,
	                    std::max(0.0f, form.title.right() - hint_width - K_TITLE_GAP - icon.right() - K_FORM_ICON_GAP), faded(g_theme.text, content));
	draw_text(t_draw_list, caption, Vec2{form.title.right() - hint_width, caption.centered_baseline(form.title)}, ESCAPE_HINT,
	          faded(g_theme.text_faint, content));

	const Color active_label = mix(g_theme.text_dim, accent, K_ACTIVE_LABEL_TINT);

	for (u32 i = 0; i < K_FIELD_COUNT; i += 1) {
		TextInput* input    = &m_fields[i];
		const Rect box      = form.fields[i];
		const Rect label    = form.labels[i];
		const bool focused  = input->is_focused();
		const bool required = i != K_NOTE;
		const bool missing  = m_show_required && required && input->value().empty();
		const bool hovered  = live && !focused && box.contains(m_mouse);

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

		const float label_baseline = label.y + secondary.ascent;
		draw_text(t_draw_list, secondary, Vec2{label.x, label_baseline}, K_FIELD_LABELS[i], faded(label_color, content));

		if (!required) {
			draw_text(t_draw_list, secondary, Vec2{label.x + text_width(secondary, K_FIELD_LABELS[i]), label_baseline}, K_OPTIONAL_SUFFIX,
			          faded(g_theme.text_faint, content));
		}

		const auto notice = [&](controls::NoticeKind t_kind, std::string_view t_text, Color t_color) {
			const float width = controls::notice_width(secondary, t_text);
			controls::draw_notice(t_draw_list, secondary, Vec2{label.right() - width, label.y}, t_kind, t_text, faded(t_color, content));
		};

		if (missing) {
			notice(controls::NoticeKind::ALERT, "Required", g_theme.error);
		} else if (i == K_PASSWORD && focused && os::is_caps_lock_on()) {
			notice(controls::NoticeKind::CAPS_LOCK, "Caps Lock is on", controls::caution_color());
		}

		draw_input_box(t_draw_list, box, border, focused && !missing, accent, content);
		input->draw(t_draw_list, body, field_text_rect(form, i), faded(g_theme.text, content), faded(accent, content));
	}

	controls::draw_eye(t_draw_list, m_assets, form.reveal, !m_fields[K_PASSWORD].is_masked(),
	                   faded(live && form.reveal.contains(m_mouse) ? g_theme.text : g_theme.text_dim, content));

	const Rect region_label = form.labels[K_FIELD_COUNT];
	draw_text(t_draw_list, secondary, Vec2{region_label.x, region_label.y + secondary.ascent}, "Region", faded(g_theme.text_dim, content));
	draw_text(t_draw_list, secondary, Vec2{region_label.x + text_width(secondary, "Region"), region_label.y + secondary.ascent}, K_OPTIONAL_SUFFIX,
	          faded(g_theme.text_faint, content));

	const Rect  region         = form.region;
	const bool  region_open    = m_region_list.is_open();
	const bool  region_hovered = live && region.contains(m_mouse);
	const u32   region_choice  = region_index(m_region);
	const Rect  chevron{region.right() - K_CHEVRON_MARGIN - K_CHEVRON_SIZE.x, region.center().y - K_CHEVRON_SIZE.y * 0.5f, K_CHEVRON_SIZE.x, K_CHEVRON_SIZE.y};
	const float value_x = region.x + K_INPUT_PADDING_X;

	Color region_border = g_theme.separator;
	if (region_open) {
		region_border = accent;
	} else if (region_hovered) {
		region_border = g_theme.border;
	}

	draw_input_box(t_draw_list, region, region_border, region_open, accent, content);
	draw_text_truncated(t_draw_list, body, Vec2{value_x, body.centered_baseline(region)}, K_REGION_OPTIONS[region_choice].label,
	                    chevron.x - K_INPUT_PADDING_X - value_x, faded(region_choice == 0 ? g_theme.text_faint : g_theme.text, content));
	controls::draw_chevron(t_draw_list, chevron, region_open, faded(region_open || region_hovered ? g_theme.text : g_theme.text_dim, content));

	if (form.has_games) {
		const Rect shown_label = form.labels[K_FIELD_COUNT + 1];
		draw_text(t_draw_list, secondary, Vec2{shown_label.x, shown_label.y + secondary.ascent}, "Show in", faded(g_theme.text_dim, content));

		for (u32 game = 0; game < m_library->game_count; game += 1) {
			const Game& info     = m_library->games[game];
			const Rect  chip     = form.games[game];
			const bool  selected = (m_visible_mask & (1u << game)) != 0;
			const bool  hovered  = live && chip.contains(m_mouse);
			const Color fill     = selected ? mix(g_theme.field, accent, K_SHOWN_CHIP_TINT) : g_theme.field;
			const Color border   = selected ? mix(g_theme.border, accent, K_SHOWN_BORDER_TINT) : (hovered ? g_theme.border : g_theme.separator);
			const Rect  game_icon{chip.x + K_CHIP_PADDING_X, snapped_to_pixel(chip.center().y - K_GAME_CHIP_ICON * 0.5f), K_GAME_CHIP_ICON, K_GAME_CHIP_ICON};
			const u8    icon_alpha = selected ? content : static_cast<u8>(content * K_HIDDEN_ICON_ALPHA / 255);

			t_draw_list->add_bordered_rect(chip, rounded(K_CHIP_RADIUS), faded(fill, content), faded(border, content), 1.0f);

			if (info.icon != nullptr) {
				t_draw_list->add_image(game_icon, info.icon, faded(K_COLOR_IMAGE, icon_alpha), rounded(K_GAME_CHIP_ICON_RADIUS));
			} else {
				t_draw_list->add_rounded_rect(game_icon, rounded(K_GAME_CHIP_ICON_RADIUS), faded(info.accent, icon_alpha));
			}

			draw_text(t_draw_list, secondary, Vec2{game_icon.right() + K_GAME_CHIP_ICON_GAP, secondary.centered_baseline(chip)}, info.short_title,
			          faded(selected || hovered ? g_theme.text : g_theme.text_dim, content));
		}
	}

	if (m_form_account) {
		const Rect  remove  = form.remove;
		const bool  hovered = live && remove.contains(m_mouse);
		const Color danger  = m_delete_armed ? controls::confirm_red() : g_theme.error;
		const float text_x  = remove.x + K_DELETE_PADDING_X;

		if (hovered || m_delete_armed) {
			t_draw_list->add_rounded_rect(remove, rounded(K_PILL_RADIUS), faded(mix(g_theme.popup, danger, K_DANGER_TINT), content));
		}

		draw_text(t_draw_list, secondary, Vec2{text_x, secondary.centered_baseline(remove)}, m_delete_armed ? K_DELETE_ARMED : K_DELETE_LABEL,
		          faded(danger, content));

		if (m_delete_armed) {
			const Rect ring{remove.right() - K_DELETE_PADDING_X - K_COUNTDOWN_SIZE, snapped_to_pixel(remove.center().y - K_COUNTDOWN_SIZE * 0.5f),
			                K_COUNTDOWN_SIZE, K_COUNTDOWN_SIZE};
			controls::draw_circular_countdown(t_draw_list, ring, std::max(0.0f, m_armed_seconds / K_DELETE_CONFIRM_SECONDS), faded(danger, content));
		}
	}

	controls::draw_button(t_draw_list, body, form.cancel, "Cancel", controls::ButtonStyle::GHOST, accent, true, live && form.cancel.contains(m_mouse), content);
	controls::draw_button(t_draw_list, body, form.save, "Save", controls::ButtonStyle::ACCENT, accent, true, live && form.save.contains(m_mouse), content);

	t_draw_list->pop_clip();
}

auto LibraryView::draw_empty_state(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) const -> void
{
	const Game&                game      = m_library->games[static_cast<u32>(m_game)];
	const EmptyState           state     = empty_state(t_layout);
	const Font&                body      = m_fonts->body;
	const Font&                secondary = m_fonts->secondary;
	const float                center_x  = state.icon.center().x;
	constexpr std::string_view TITLE     = "No accounts yet";
	constexpr std::string_view HINT      = "Add one to log in with a single click.";

	if (game.icon != nullptr) {
		t_draw_list->add_image(state.icon, game.icon, faded(K_COLOR_IMAGE, t_alpha), rounded(K_EMPTY_ICON_RADIUS));
	} else {
		t_draw_list->add_rounded_rect(state.icon, rounded(K_EMPTY_ICON_RADIUS), faded(game.accent, t_alpha));
	}

	draw_text(t_draw_list, body, Vec2{snapped_to_pixel(center_x - text_width(body, TITLE) * 0.5f), state.title_baseline}, TITLE, faded(g_theme.text, t_alpha));
	draw_text(t_draw_list, secondary, Vec2{snapped_to_pixel(center_x - text_width(secondary, HINT) * 0.5f), state.hint_baseline}, HINT,
	          faded(g_theme.text_dim, t_alpha));
	controls::draw_button(t_draw_list, body, state.button, K_ADD_LABEL, controls::ButtonStyle::ACCENT, m_settings->accent, true,
	                      m_hovered.target == Target::ADD && state.button.contains(m_mouse), t_alpha);
}

auto LibraryView::draw_no_matches(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) const -> void
{
	const Font&                body      = m_fonts->body;
	const Font&                secondary = m_fonts->secondary;
	constexpr std::string_view TITLE     = "No matches";
	constexpr std::string_view HINT      = "Try a different name, note or region.";

	const Rect  area{m_bounds.x, t_layout.labels.y, m_bounds.w, std::max(0.0f, m_bounds.bottom() - t_layout.labels.y)};
	const float stack    = body.line_height() + K_EMPTY_LINE_GAP + secondary.line_height();
	const float top      = snapped_to_pixel(area.center().y - stack * 0.5f - K_HEADER_GAP);
	const float center_x = area.center().x;

	draw_text(t_draw_list, body, Vec2{snapped_to_pixel(center_x - text_width(body, TITLE) * 0.5f), top + body.ascent}, TITLE, faded(g_theme.text_dim, t_alpha));
	draw_text(t_draw_list, secondary,
	          Vec2{snapped_to_pixel(center_x - text_width(secondary, HINT) * 0.5f), top + body.line_height() + K_EMPTY_LINE_GAP + secondary.ascent}, HINT,
	          faded(g_theme.text_faint, t_alpha));
}

auto LibraryView::draw(DrawList* t_draw_list, u8 t_alpha) -> void
{
	if (!has_game() || t_alpha == 0) return;

	const Layout          layout   = this->layout();
	const VisibleAccounts accounts = shown_accounts();
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

	const std::optional<u32> edited = form_row(accounts);
	const std::optional<u32> raised = m_drag.lifted ? std::optional<u32>{m_drag.from_row} : m_raised_row;

	t_draw_list->push_clip(layout.rows);

	for (u32 row = 0; row < accounts.count; row += 1) {
		if (edited == row || raised == row) continue;

		const float entrance = row_entrance(row);
		const Rect  rect     = row_rect(layout, accounts, row).moved(Vec2{0.0f, snapped_to_pixel((1.0f - entrance) * K_ROW_RISE)});
		if (entrance <= 0.0f || !rect.overlaps_vertically(layout.rows)) continue;

		draw_row(t_draw_list, layout, rect, accounts.refs[row], row, false, scaled_alpha(t_alpha, entrance));
	}

	if (form_shown()) {
		draw_form(t_draw_list, layout, accounts, form_card(layout, accounts), t_alpha);
	}

	if (raised && *raised < accounts.count && edited != raised) {
		const Rect rect = m_drag.lifted ? lifted_rect(layout) : row_rect(layout, accounts, *raised);
		draw_row(t_draw_list, layout, rect, accounts.refs[*raised], *raised, true, t_alpha);
	}

	t_draw_list->pop_clip();

	const ScrollGeometry geometry = scroll_geometry(layout, accounts);
	m_scroll.draw_edge_fade(t_draw_list, layout.rows, geometry, faded(g_theme.window, t_alpha));
	m_scroll.draw(t_draw_list, geometry, m_mouse, t_alpha);
	m_region_list.draw(t_draw_list, m_mouse);
}
