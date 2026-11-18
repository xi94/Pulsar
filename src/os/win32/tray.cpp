#include "os/tray.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cwchar>
#include <iterator>
#include <numbers>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <Windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <shellapi.h>
#include <shellscalingapi.h>
#include <timeapi.h>
#include <wrl/client.h>

#include "core/animation.h"
#include "core/app_identity.h"
#include "core/caret.h"
#include "core/debug_log.h"
#include "core/enum_array.h"
#include "core/regions.h"
#include "core/str.h"
#include "core/text_editor.h"
#include "os/win32/win32.h"
#include "stb/stb_image.h"

using Microsoft::WRL::ComPtr;

namespace {
constexpr const char* K_LOG_CATEGORY = "tray";

constexpr UINT     K_TRAY_CALLBACK_MESSAGE      = WM_APP + 1;
constexpr UINT_PTR K_TRAY_ICON_ID               = 1;
constexpr UINT_PTR K_ADD_ICON_RETRY_TIMER       = 1;
constexpr UINT     K_ADD_ICON_RETRY_INTERVAL_MS = 1000;
constexpr u32      K_MAX_ADD_ICON_ATTEMPTS      = 10;

constexpr UINT_PTR K_ANIMATION_TIMER   = 1;
constexpr UINT_PTR K_WATCH_TIMER       = 3;
constexpr UINT_PTR K_TOAST_TIMER       = 4;
constexpr UINT_PTR K_CARET_TIMER       = 5;
constexpr UINT     K_ANIMATION_TICK_MS = 10;
constexpr UINT     K_FINE_TIMER_MS     = 1;
constexpr UINT     K_WATCH_INTERVAL_MS = 50;
constexpr UINT     K_TOAST_TICK_MS     = 10;

constexpr float K_LOCKED_ICON_OPACITY = 0.55f;

constexpr float K_REFERENCE_DPI   = 96.0f;
constexpr float K_SURFACE_RADIUS  = 9.0f;
constexpr float K_SHADOW_MARGIN   = 16.0f;
constexpr float K_SHADOW_DROP     = 3.0f;
constexpr float K_SHADOW_ALPHA    = 0.28f;
constexpr u32   K_SHADOW_LAYERS   = 12;
constexpr float K_HOVER_EASE_RATE = 20.0f;
constexpr float K_SETTLED         = 0.005f;

constexpr float K_PANEL_PADDING         = 5.0f;
constexpr float K_ROW_HEIGHT            = 28.0f;
constexpr float K_ROW_RADIUS            = 6.0f;
constexpr float K_ROW_INSET             = 9.0f;
constexpr float K_SEPARATOR_HEIGHT      = 9.0f;
constexpr float K_SEPARATOR_INSET       = 4.0f;
constexpr float K_ICON_SIZE             = 18.0f;
constexpr float K_ICON_RADIUS           = 4.5f;
constexpr float K_LOGO_SIZE             = 16.0f;
constexpr float K_GLYPH_SIZE            = 15.0f;
constexpr float K_CHEVRON_SIZE          = 13.0f;
constexpr float K_ICON_GAP              = 9.0f;
constexpr float K_COUNT_GAP             = 3.0f;
constexpr float K_CHIP_HEIGHT           = 16.0f;
constexpr float K_CHIP_PADDING          = 6.0f;
constexpr float K_CHIP_GAP              = 7.0f;
constexpr float K_BUTTON_LIFT           = 0.14f;
constexpr float K_GAMES_MIN_WIDTH       = 220.0f;
constexpr float K_MAX_WIDTH             = 360.0f;
constexpr float K_HOVER_ALPHA           = 0.16f;
constexpr float K_QUIT_REST_RED         = 0.65f;
constexpr float K_CHIP_ALPHA            = 0.09f;
constexpr float K_BODY_SIZE             = 13.0f;
constexpr float K_CAPTION_SIZE          = 12.0f;
constexpr float K_CHIP_TEXT_SIZE        = 11.0f;
constexpr float K_MENU_IN_SECONDS       = 0.1f;
constexpr float K_LONGEST_FRAME_SECONDS = 0.05f;

constexpr float K_SIDE_WIDTH            = 330.0f;
constexpr float K_SIDE_PADDING          = 14.0f;
constexpr float K_SIDE_HEADER_HEIGHT    = 52.0f;
constexpr float K_SIDE_OPEN_SECONDS     = 0.3f;
constexpr float K_SIDE_CLOSE_SECONDS    = 0.2f;
constexpr float K_SELECTED_GAME_ALPHA   = 0.12f;
constexpr float K_ACCOUNT_ROW_HEIGHT    = 44.0f;
constexpr float K_ACCOUNT_LINE_GAP      = 2.0f;
constexpr float K_ACCOUNT_TEXT_INSET    = 16.0f;
constexpr float K_NAME_LINE_HEIGHT      = 18.0f;
constexpr float K_DETAIL_LINE_HEIGHT    = 16.0f;
constexpr float K_ACCOUNT_HOVER_ALPHA   = 0.06f;
constexpr float K_ROW_BUTTON_SIZE       = 24.0f;
constexpr float K_ROW_BUTTON_GLYPH_SIZE = 14.0f;
constexpr float K_ROW_BUTTON_HOVER      = 0.08f;
constexpr float K_DETAIL_DOT_SIZE       = 3.0f;
constexpr float K_DETAIL_DOT_GAP        = 6.0f;
constexpr float K_DRAG_THRESHOLD        = 4.0f;

constexpr float K_FORM_HEADER_HEIGHT    = 44.0f;
constexpr float K_FORM_BACK_SIZE        = 26.0f;
constexpr float K_FORM_LABEL_HEIGHT     = 18.0f;
constexpr float K_FORM_FIELD_HEIGHT     = 32.0f;
constexpr float K_FORM_FIELD_RADIUS     = 7.0f;
constexpr float K_FORM_FIELD_PADDING    = 10.0f;
constexpr float K_FORM_FIELD_FILL       = 0.04f;
constexpr float K_FORM_COLUMN_GAP       = 10.0f;
constexpr float K_FORM_ROW_GAP          = 10.0f;
constexpr float K_FORM_REVEAL_SIZE      = 24.0f;
constexpr float K_FORM_CHIP_HEIGHT      = 26.0f;
constexpr float K_FORM_CHIP_PADDING     = 8.0f;
constexpr float K_FORM_CHIP_ICON_SIZE   = 16.0f;
constexpr float K_FORM_CHIP_ICON_GAP    = 5.0f;
constexpr float K_FORM_CHIP_GAP         = 5.0f;
constexpr float K_FORM_CHIP_TINT        = 0.12f;
constexpr float K_FORM_CHIP_FADED_ICON  = 0.45f;
constexpr float K_FORM_CHIP_BORDER_TINT = 0.55f;
constexpr float K_FORM_GHOST_ALPHA      = 0.08f;
constexpr float K_FORM_GHOST_HOVER      = 0.07f;
constexpr float K_FORM_BUTTON_HEIGHT    = 30.0f;
constexpr float K_FORM_BUTTON_PADDING   = 14.0f;
constexpr float K_FORM_BUTTON_GAP       = 8.0f;
constexpr float K_FORM_BUTTON_RADIUS    = 8.0f;
constexpr float K_REGION_ROW_HEIGHT     = 26.0f;
constexpr usize K_REGION_ROWS_SHOWN     = 7;
constexpr float K_REGION_LIST_WIDTH     = 220.0f;
constexpr float K_REGION_LIST_GAP       = 4.0f;

constexpr float             K_TEXT_LINE_HEIGHT        = 18.0f;
constexpr float             K_TEXT_CLIP_BLEED         = 2.0f;
constexpr float             K_SELECTION_ALPHA         = 0.27f;
constexpr float             K_UNDERLINE_CARET_HEIGHT  = 2.0f;
constexpr std::wstring_view K_END_OF_TEXT_CELL        = L"0";
constexpr float             K_KEYCAP_PADDING          = 5.0f;
constexpr float             K_KEYCAP_RADIUS           = 4.0f;
constexpr float             K_KEYCAP_LIP              = 1.0f;
constexpr float             K_KEYCAP_GAP              = 3.0f;
constexpr float             K_KEYCAP_HEIGHT           = 19.0f;
constexpr float             K_KEYCAP_INK_MIX          = 0.5f;
constexpr float             K_KEYCAP_TEXT_SIZE        = 11.0f;
constexpr float             K_DISABLED_KEYCAP_ALPHA   = 0.5f;
constexpr float             K_POPUP_MIN_WIDTH         = 180.0f;
constexpr float             K_POPUP_PADDING           = 4.0f;
constexpr float             K_POPUP_RADIUS            = 10.0f;
constexpr float             K_POPUP_ITEM_HEIGHT       = 28.0f;
constexpr float             K_POPUP_ITEM_RADIUS       = 6.0f;
constexpr float             K_POPUP_LABEL_INSET       = 10.0f;
constexpr float             K_POPUP_SHORTCUT_GAP      = 24.0f;
constexpr float             K_POPUP_HOVER_ALPHA       = 0.08f;
constexpr float             K_POPUP_ICON_SIZE         = 16.0f;
constexpr float             K_POPUP_ICON_GAP          = 10.0f;
constexpr float             K_POPUP_SEPARATOR_SPACE   = 9.0f;
constexpr float             K_POPUP_DANGER_ALPHA      = 0.13f;
constexpr float             K_CONFIRM_SECONDS         = 3.0f;
constexpr float             K_CONFIRM_BAR_DROP        = 8.0f;
constexpr float             K_CONFIRM_BAR_HEIGHT      = 1.5f;
constexpr float             K_ACCOUNT_SEPARATOR_INSET = 12.0f;
constexpr float             K_ADD_BUTTON_HEIGHT       = 26.0f;
constexpr float             K_ADD_BUTTON_PADDING      = 9.0f;
constexpr float             K_ADD_BUTTON_GLYPH_SIZE   = 13.0f;
constexpr float             K_ADD_BUTTON_GAP          = 5.0f;
constexpr float             K_ADD_BUTTON_HOVER_ALPHA  = 0.07f;
constexpr float             K_CHEVRON_TURN_RATE       = 16.0f;
constexpr float             K_CHEVRON_FLIP_DEGREES    = 180.0f;
constexpr float             K_SIDE_AIM_SECONDS        = 0.25f;
constexpr float             K_SIDE_AIM_SLOP           = 8.0f;
constexpr float             K_ROW_SHIFT_EASE_RATE     = 18.0f;
constexpr float             K_ROW_LIFT_EASE_RATE      = 16.0f;

constexpr float K_UPDATE_SIZE       = 22.0f;
constexpr float K_UPDATE_RADIUS     = 6.0f;
constexpr float K_UPDATE_GLYPH_SIZE = 14.0f;
constexpr float K_UPDATE_PADDING    = 8.0f;
constexpr float K_UPDATE_GAP        = 5.0f;
constexpr float K_UPDATE_FILL_ALPHA = 0.16f;
constexpr float K_UPDATE_FROM_SCALE = 0.85f;
constexpr float K_UPDATE_IN_SECONDS = 0.16f;
constexpr float K_SPIN_SECONDS      = 0.9f;

constexpr float K_FIELD_ROW_HEIGHT    = 40.0f;
constexpr float K_FIELD_INSET_Y       = 5.0f;
constexpr float K_FIELD_RADIUS        = 7.0f;
constexpr float K_FIELD_PADDING       = 9.0f;
constexpr float K_FIELD_FILL_ALPHA    = 0.06f;
constexpr float K_FIELD_BORDER_ALPHA  = 0.7f;
constexpr float K_FIELD_SUBMIT_SIZE   = 22.0f;
constexpr float K_FIELD_SUBMIT_RADIUS = 5.0f;
constexpr float K_FIELD_IDLE_SUBMIT   = 0.35f;
constexpr float K_FIELD_MIN_WIDTH     = 210.0f;
constexpr float K_CARET_WIDTH         = 1.5f;
constexpr float K_CARET_BLINK_SECONDS = 0.53f;
constexpr float K_SHAKE_SECONDS       = 0.4f;
constexpr float K_SHAKE_DISTANCE      = 6.0f;
constexpr float K_SHAKE_TURNS         = 3.0f;
constexpr usize K_UTF8_MAX_BYTES      = 4;

constexpr float K_CHEVRON_LEFT_DEGREES = 180.0f;
constexpr float K_CHEVRON_DOWN_DEGREES = 90.0f;
constexpr float K_CHEVRON_UP_DEGREES   = 270.0f;

constexpr float K_SCROLL_GUTTER         = 6.0f;
constexpr float K_SCROLLBAR_INSET       = 2.0f;
constexpr float K_SCROLLBAR_THIN        = 2.0f;
constexpr float K_SCROLLBAR_WIDE        = 6.0f;
constexpr float K_MIN_THUMB_HEIGHT      = 24.0f;
constexpr float K_THUMB_HOVER_STRENGTH  = 0.3f;
constexpr float K_PIXELS_PER_NOTCH      = 48.0f;
constexpr float K_SCROLL_EASE_RATE      = 16.0f;
constexpr float K_SCROLL_SETTLED        = 0.25f;
constexpr float K_SCROLLBAR_GROW_RATE   = 18.0f;
constexpr float K_SCROLLBAR_SHRINK_RATE = 9.0f;
constexpr float K_SCROLL_AWAKE_SECONDS  = 0.9f;
constexpr float K_RELEASE_LINGER        = 0.35f;
constexpr float K_EDGE_SHADOW_HEIGHT    = 14.0f;
constexpr float K_EDGE_SHADOW_ALPHA     = 0.35f;
constexpr float K_EDGE_SHADOW_TRAVEL    = 24.0f;

constexpr float K_TOAST_MARGIN              = 12.0f;
constexpr float K_TOAST_PADDING_LEFT        = 10.0f;
constexpr float K_TOAST_PADDING_RIGHT       = 14.0f;
constexpr float K_TOAST_PADDING_Y           = 9.0f;
constexpr float K_TOAST_LINE_HEIGHT         = 20.0f;
constexpr float K_TOAST_PILL_HEIGHT         = K_TOAST_PADDING_Y * 2.0f + K_TOAST_LINE_HEIGHT;
constexpr float K_TOAST_MAX_WIDTH           = 360.0f;
constexpr float K_TOAST_GROWN_RADIUS        = 13.0f;
constexpr float K_TOAST_ICON_SIZE           = 18.0f;
constexpr float K_TOAST_ICON_RADIUS         = 5.0f;
constexpr float K_TOAST_GAP                 = 8.0f;
constexpr float K_TOAST_TEXT_INSET          = K_TOAST_PADDING_LEFT + K_TOAST_ICON_SIZE + K_TOAST_GAP;
constexpr float K_TOAST_DIVIDER_WIDTH       = 1.0f;
constexpr float K_TOAST_DIVIDER_HEIGHT      = 14.0f;
constexpr float K_TOAST_MARK_SIZE           = 13.0f;
constexpr float K_TOAST_TIME_GAP            = 6.0f;
constexpr float K_TOAST_TIME_MIN_WIDTH      = 26.0f;
constexpr usize K_TOAST_NAME_MAX_CHARACTERS = 14;
constexpr float K_TOAST_DOT_SIZE            = 5.0f;
constexpr float K_TOAST_BADGE_RADIUS_SHARE  = 0.43f;
constexpr float K_TOAST_BADGE_STROKE_SHARE  = 0.28f;
constexpr float K_TOAST_RING_RADIUS_SHARE   = 0.375f;
constexpr float K_TOAST_RING_STROKE         = 1.4f;
constexpr float K_TOAST_RING_TRACK_ALPHA    = 0.22f;
constexpr float K_TOAST_RING_ARC_TURNS      = 0.28f;
constexpr float K_TOAST_RING_TURN_SECONDS   = 1.1f;
constexpr float K_TOAST_MESSAGE_GAP         = 8.0f;
constexpr float K_TOAST_MESSAGE_MAX         = 48.0f;
constexpr float K_TOAST_BUTTON_GAP          = 8.0f;
constexpr float K_TOAST_BUTTON_HEIGHT       = 24.0f;
constexpr float K_TOAST_BUTTON_PADDING      = 11.0f;
constexpr float K_TOAST_BUTTON_SPACING      = 6.0f;
constexpr float K_TOAST_BUTTON_RADIUS       = 7.0f;
constexpr float K_TOAST_TEXT_SIZE           = 12.5f;
constexpr float K_TOAST_COUNT_SIZE          = 11.5f;
constexpr float K_TOAST_MESSAGE_SIZE        = 12.0f;
constexpr float K_TOAST_GHOST_ALPHA         = 0.08f;
constexpr float K_TOAST_GHOST_HOVER         = 0.07f;
constexpr float K_TOAST_ERROR_EDGE          = 0.6f;
constexpr float K_TOAST_SLIDE               = 14.0f;
constexpr float K_TOAST_IN_SECONDS          = 0.22f;
constexpr float K_TOAST_GROW_SECONDS        = 0.22f;
constexpr float K_TOAST_DONE_SECONDS        = 0.25f;
constexpr float K_TOAST_WIDTH_EASE_RATE     = 14.0f;
constexpr float K_TOAST_RAISE_SECONDS       = 0.5f;
constexpr float K_TOAST_SUCCESS_SECONDS     = 2.0f;
constexpr float K_TOAST_ERROR_SECONDS       = 5.0f;

enum class ToastHit : u8 {
	RETRY,
	OPEN,
	BODY,
	COUNT,
};

constexpr const wchar_t* K_FONT_FAMILIES[]{L"Segoe UI Variable Text", L"Segoe UI"};
constexpr const wchar_t* K_RETRY_LABEL    = L"Retry";
constexpr const wchar_t* K_UPDATE_LABEL   = L"Update available";
constexpr const wchar_t* K_UPDATING_LABEL = L"Updating";

constexpr std::wstring_view K_NEW_ACCOUNT_TITLE = L"New account";
constexpr std::wstring_view K_ESCAPE_HINT       = L"Esc to cancel";
constexpr std::wstring_view K_REGION_LABEL      = L"Region";
constexpr std::wstring_view K_SHOW_IN_LABEL     = L"Show in";
constexpr std::wstring_view K_SAVE_LABEL        = L"Save";
constexpr std::wstring_view K_CANCEL_LABEL      = L"Cancel";
constexpr std::wstring_view K_ADD_LABEL         = L"Add";
constexpr std::wstring_view K_NO_ACCOUNTS_LABEL = L"No accounts yet";

constexpr std::wstring_view K_PASSWORD_PLACEHOLDER = L"Master password";
constexpr std::wstring_view K_WRONG_PASSWORD_LABEL = L"Wrong password";

constexpr wchar_t K_ELLIPSIS = 0x2026;

enum class RowKind : u8 {
	BRAND,
	GAME,
	ACCOUNT,
	ACTION,
	OPTION,
	NOTE,
	PASSWORD,
	SEPARATOR,
};

enum class Glyph : u8 {
	NONE,
	OPEN,
	LOCK,
	POWER,
	CHEVRON,
	CHECK_CIRCLE,
	ALERT_CIRCLE,
	REFRESH,
	DOWNLOAD,
	ADD,
	EDIT,
	EYE,
	EYE_OFF,
	TRASH,
	COPY,
	KEY,
};

struct Row {
	RowKind            kind = RowKind::NOTE;
	std::wstring       label;
	std::wstring       detail;
	std::wstring       note;
	std::wstring       when;
	Glyph              glyph = Glyph::NONE;
	std::optional<u32> item;
	std::optional<u32> icon;
	os::TrayEventType  action       = os::TrayEventType::NONE;
	bool               destructive  = false;
	bool               favorite     = false;
	float              turn         = 0.0f;
	u64                id           = 0;
	float              offset       = 0.0f;
	float              label_x      = 0.0f;
	float              detail_width = 0.0f;
	float              top          = 0.0f;
	float              height       = 0.0f;
	float              hover        = 0.0f;
};

struct TextFormats {
	ComPtr<IDWriteTextFormat> body;
	ComPtr<IDWriteTextFormat> strong;
	ComPtr<IDWriteTextFormat> caption;
	ComPtr<IDWriteTextFormat> count;
	ComPtr<IDWriteTextFormat> chip;
	ComPtr<IDWriteTextFormat> button;
	ComPtr<IDWriteTextFormat> message;
	ComPtr<IDWriteTextFormat> toast_text;
	ComPtr<IDWriteTextFormat> toast_title;
	ComPtr<IDWriteTextFormat> toast_count;
	ComPtr<IDWriteTextFormat> badge;
	ComPtr<IDWriteTextFormat> keycap;
};

struct Canvas {
	HDC                         dc       = nullptr;
	HBITMAP                     pixels   = nullptr;
	HGDIOBJ                     replaced = nullptr;
	SIZE                        size{};
	ComPtr<ID2D1DCRenderTarget> target;
};

struct Scroll {
	float                offset    = 0.0f;
	float                target    = 0.0f;
	float                thickness = 0.0f;
	float                awake     = 0.0f;
	std::optional<float> grab;
	bool                 over_bar   = false;
	bool                 over_thumb = false;
	bool                 was_held   = false;
};

enum class RowPart : u8 {
	BODY,
	STAR,
};

struct Panel {
	std::vector<Row>       rows;
	D2D1_RECT_F            frame{};
	float                  width          = 0.0f;
	float                  height         = 0.0f;
	float                  content_height = 0.0f;
	float                  gutter         = 0.0f;
	Scroll                 scroll;
	std::optional<u32>     hot;
	std::optional<RowPart> hot_part;
	std::optional<usize>   footer;
	std::optional<u32>     selected;
	float                  chevron_degrees = 0.0f;
};

[[nodiscard]] auto max_scroll(const Panel& t_panel) -> float
{
	return t_panel.content_height - t_panel.height;
}

auto fit_rows(Panel* t_panel, float t_height) -> void
{
	const float most = std::max(0.0f, t_panel->content_height - t_height);

	t_panel->height        = t_height;
	t_panel->gutter        = most > 0.0f ? K_SCROLL_GUTTER : 0.0f;
	t_panel->scroll.target = std::clamp(t_panel->scroll.target, 0.0f, most);
	t_panel->scroll.offset = std::clamp(t_panel->scroll.offset, 0.0f, most);
}

struct ShownRow {
	u64   id;
	float top;
	float hover;
};

[[nodiscard]] auto shown_rows(const Panel& t_panel) -> std::vector<ShownRow>
{
	std::vector<ShownRow> shown;

	for (const Row& row : t_panel.rows) {
		if (row.kind == RowKind::ACCOUNT) {
			shown.push_back(ShownRow{.id = row.id, .top = row.top + row.offset, .hover = row.hover});
		}
	}

	return shown;
}

auto slide_from(Panel* t_panel, std::span<const ShownRow> t_before) -> void
{
	for (Row& row : t_panel->rows) {
		const auto was = std::ranges::find(t_before, row.id, &ShownRow::id);
		if (row.kind != RowKind::ACCOUNT || was == t_before.end()) continue;

		row.offset = was->top - row.top;
		row.hover  = was->hover;
	}
}

[[nodiscard]] auto track_rect(const Panel& t_panel) -> D2D1_RECT_F
{
	const float right = t_panel.frame.right - t_panel.frame.left - K_SCROLLBAR_INSET;

	return D2D1::RectF(right - K_SCROLLBAR_WIDE, K_PANEL_PADDING, right, t_panel.height - K_PANEL_PADDING);
}

[[nodiscard]] auto thumb_rect(const Panel& t_panel) -> D2D1_RECT_F
{
	const D2D1_RECT_F track  = track_rect(t_panel);
	const float       length = track.bottom - track.top;
	const float       size   = std::max(length * t_panel.height / t_panel.content_height, K_MIN_THUMB_HEIGHT);
	const float       top    = track.top + (length - size) * t_panel.scroll.offset / max_scroll(t_panel);

	return D2D1::RectF(track.left, top, track.right, top + size);
}

auto step_scroll(Scroll* t_scroll, float t_seconds) -> bool
{
	t_scroll->offset += (t_scroll->target - t_scroll->offset) * (1.0f - std::exp(-K_SCROLL_EASE_RATE * t_seconds));

	if (std::abs(t_scroll->target - t_scroll->offset) < K_SCROLL_SETTLED) {
		t_scroll->offset = t_scroll->target;
	}

	const bool held = t_scroll->over_bar || t_scroll->grab;
	if (t_scroll->was_held && !held) {
		t_scroll->awake = std::max(t_scroll->awake, K_RELEASE_LINGER);
	}

	t_scroll->was_held = held;
	t_scroll->awake    = std::max(0.0f, t_scroll->awake - t_seconds);

	const bool  active = held || t_scroll->awake > 0.0f;
	const float wide   = active ? 1.0f : 0.0f;
	const float rate   = active ? K_SCROLLBAR_GROW_RATE : K_SCROLLBAR_SHRINK_RATE;
	t_scroll->thickness += (wide - t_scroll->thickness) * (1.0f - std::exp(-rate * t_seconds));

	if (std::abs(wide - t_scroll->thickness) < K_SETTLED) {
		t_scroll->thickness = wide;
	}

	return t_scroll->offset != t_scroll->target || t_scroll->thickness != wide || t_scroll->awake > 0.0f;
}

struct MenuImages {
	ComPtr<ID2D1Bitmap> logo;
	ComPtr<ID2D1Bitmap> game_icons[os::K_TRAY_MAX_GAMES];
};

enum class SideView : u8 {
	ACCOUNTS,
	FORM,
};

enum class FormField : u8 {
	USERNAME,
	PASSWORD,
	NOTE,
	COUNT,
};

constexpr EnumArray<FormField, std::wstring_view> K_FORM_LABELS{{L"Username", L"Password", L"Note"}};
constexpr EnumArray<FormField, std::wstring_view> K_FORM_PLACEHOLDERS{{L"Riot username", L"", L""}};
constexpr EnumArray<FormField, usize>             K_FORM_CAPACITIES{{sizeof(os::TrayAccountForm::username), sizeof(os::TrayAccountForm::password), sizeof(os::TrayAccountForm::note)}};
constexpr std::wstring_view                       K_OPTIONAL_SUFFIX = L" (optional)";

enum class MenuAction : u8 {
	CUT,
	COPY,
	PASTE,
	SELECT_ALL,
	EDIT_ACCOUNT,
	DELETE_ACCOUNT,
	COPY_USERNAME,
	COPY_PASSWORD,
};

struct MenuItem {
	std::wstring_view label;
	MenuAction        action;
	Glyph             glyph = Glyph::NONE;
	std::wstring_view key;
	bool              separated   = false;
	bool              destructive = false;
};

constexpr MenuItem K_TEXT_MENU_ITEMS[]{
	{.label = L"Cut", .action = MenuAction::CUT, .key = L"X"},
	{.label = L"Copy", .action = MenuAction::COPY, .key = L"C"},
	{.label = L"Paste", .action = MenuAction::PASTE, .key = L"V"},
	{.label = L"Select all", .action = MenuAction::SELECT_ALL, .key = L"A"},
};

constexpr MenuItem K_ACCOUNT_MENU_ITEMS[]{
	{.label = L"Edit", .action = MenuAction::EDIT_ACCOUNT, .glyph = Glyph::EDIT},
	{.label = L"Delete", .action = MenuAction::DELETE_ACCOUNT, .glyph = Glyph::TRASH, .destructive = true},
	{.label = L"Copy username", .action = MenuAction::COPY_USERNAME, .glyph = Glyph::COPY, .separated = true},
	{.label = L"Copy password", .action = MenuAction::COPY_PASSWORD, .glyph = Glyph::KEY},
};

constexpr usize             K_MAX_MENU_ITEMS = 4;
constexpr std::wstring_view K_SHORTCUT_KEY   = L"Ctrl";
constexpr std::wstring_view K_CONFIRM_LABEL  = L"Confirm";

[[nodiscard]] auto text_edit_for(MenuAction t_action) -> std::optional<TextEdit>
{
	switch (t_action) {
		using enum MenuAction;

		case CUT: {
			return TextEdit::CUT;
		}

		case COPY: {
			return TextEdit::COPY;
		}

		case PASTE: {
			return TextEdit::PASTE;
		}

		case SELECT_ALL: {
			return TextEdit::SELECT_ALL;
		}

		default: {
			return std::nullopt;
		}
	}
}

struct PopupMenu {
	std::span<const MenuItem>          items;
	std::array<bool, K_MAX_MENU_ITEMS> enabled{};
	D2D1_RECT_F                        box{};
	std::optional<usize>               hot;
	std::optional<usize>               armed;
	float                              armed_seconds = 0.0f;
	std::optional<u32>                 account_row;
};

struct PendingSide {
	std::optional<u32> row;
	float              seconds = 0.0f;
};

[[nodiscard]] auto inside_triangle(D2D1_POINT_2F t_point, D2D1_POINT_2F t_a, D2D1_POINT_2F t_b, D2D1_POINT_2F t_c) -> bool
{
	const auto side_of = [t_point](D2D1_POINT_2F t_from, D2D1_POINT_2F t_to) { return (t_to.x - t_from.x) * (t_point.y - t_from.y) - (t_to.y - t_from.y) * (t_point.x - t_from.x); };

	const float ab = side_of(t_a, t_b);
	const float bc = side_of(t_b, t_c);
	const float ca = side_of(t_c, t_a);

	return (ab >= 0.0f && bc >= 0.0f && ca >= 0.0f) || (ab <= 0.0f && bc <= 0.0f && ca <= 0.0f);
}

struct FocusedText {
	TextEditor*   editor;
	D2D1_RECT_F   box;
	float*        scroll;
	D2D1_POINT_2F origin;
};

struct TimerResolution {
	bool fine = false;

	auto refine() -> void
	{
		if (fine) return;

		timeBeginPeriod(K_FINE_TIMER_MS);
		fine = true;
	}

	auto restore() -> void
	{
		if (!fine) return;

		timeEndPeriod(K_FINE_TIMER_MS);
		fine = false;
	}
};

struct AccountFormState {
	bool                             editing = false;
	u32                              game    = 0;
	u32                              row     = 0;
	EnumArray<FormField, TextEditor> fields;
	EnumArray<FormField, float>      scroll{};
	FormField                        focus         = FormField::USERNAME;
	usize                            region        = 0;
	u16                              visible_games = 0;
	bool                             show_required = false;
	bool                             regions_open  = false;
	Panel                            regions;
};

enum class FormTarget : u8 {
	BACK,
	FIELD,
	REVEAL,
	REGION,
	REGION_OPTION,
	GAME,
	CANCEL,
	SAVE,
};

struct FormHit {
	FormTarget target;
	FormField  field = FormField::USERNAME;
	usize      index = 0;

	auto operator==(const FormHit&) const -> bool = default;
};

struct FormLayout {
	D2D1_RECT_F                       back{};
	D2D1_RECT_F                       header{};
	EnumArray<FormField, D2D1_RECT_F> labels{};
	EnumArray<FormField, D2D1_RECT_F> fields{};
	D2D1_RECT_F                       reveal{};
	D2D1_RECT_F                       region_label{};
	D2D1_RECT_F                       region{};
	D2D1_RECT_F                       regions{};
	D2D1_RECT_F                       games_label{};
	std::vector<D2D1_RECT_F>          games;
	D2D1_RECT_F                       cancel{};
	D2D1_RECT_F                       save{};
	float                             needed_height = 0.0f;
};

struct RowDrag {
	std::optional<u32> pressed;
	float              press_y    = 0.0f;
	float              grab       = 0.0f;
	float              lifted_top = 0.0f;
	bool               lifted     = false;
	u32                insert     = 0;
};

struct RaisedRow {
	u64   id;
	float top;
	float lift;
};

struct UpdateBadge {
	os::TrayUpdate state = os::TrayUpdate::NONE;
	float          spin  = 0.0f;
	float          hover = 0.0f;
	float          shown = 1.0f;
};

struct ToastMeasures {
	float name           = 0.0f;
	float label          = 0.0f;
	float time           = K_TOAST_TIME_MIN_WIDTH;
	float message        = 0.0f;
	float message_height = 0.0f;
	float message_line   = 0.0f;
	float retry          = 0.0f;
	float open           = 0.0f;
};

struct LoginToast {
	HWND                                  window = nullptr;
	Canvas                                canvas;
	ComPtr<ID2D1Bitmap>                   game_icon;
	os::TrayLogin                         login{};
	std::wstring                          account;
	std::wstring                          step_label;
	std::wstring                          message;
	ToastMeasures                         measures;
	RECT                                  work{};
	float                                 scale          = 1.0f;
	bool                                  active         = false;
	bool                                  leaving        = false;
	bool                                  hovered        = false;
	bool                                  tracking_leave = false;
	std::optional<ToastHit>               hot;
	float                                 appear               = 0.0f;
	float                                 details              = 0.0f;
	float                                 finished             = 0.0f;
	float                                 spin                 = 0.0f;
	float                                 width                = 0.0f;
	float                                 linger               = 0.0f;
	float                                 seconds_since_update = 0.0f;
	float                                 seconds_since_raised = 0.0f;
	EnumArray<ToastHit, float>            hover{};
	std::chrono::steady_clock::time_point updated_at;
	std::chrono::steady_clock::time_point last_tick;
};

struct ToastButtons {
	D2D1_RECT_F retry;
	D2D1_RECT_F open;
};

enum class BadgeSymbol : u8 {
	CHECK,
	EXCLAMATION,
};

struct PasswordField {
	const TextEditor*  editor    = nullptr;
	const CaretMotion* caret     = nullptr;
	float              scroll    = 0.0f;
	bool               blinked   = false;
	bool               unlocking = false;
	bool               wrong     = false;
	float              shift     = 0.0f;
	float              spin      = 0.0f;
};

struct Painter {
	ID2D1Factory*      factory;
	IDWriteFactory*    writer;
	const TextFormats* formats;
	ID2D1StrokeStyle*  stroke;
	os::TrayColors     colors;
	float              scale;
	float              update_width;
	float              updating_width;
	PasswordField      password;
};

[[nodiscard]] auto taskbar_created_message() -> UINT
{
	static const UINT MESSAGE = RegisterWindowMessageW(L"TaskbarCreated");

	return MESSAGE;
}

[[nodiscard]] auto to_d2d(Color t_color, float t_opacity = 1.0f) -> D2D1_COLOR_F
{
	return D2D1::ColorF(t_color.r / 255.0f, t_color.g / 255.0f, t_color.b / 255.0f, t_color.a / 255.0f * std::clamp(t_opacity, 0.0f, 1.0f));
}

[[nodiscard]] auto snapped(float t_dips, float t_scale) -> float
{
	return std::round(t_dips * t_scale) / t_scale;
}

[[nodiscard]] auto eased_out(float t_amount) -> float
{
	const float rest = 1.0f - std::clamp(t_amount, 0.0f, 1.0f);

	return 1.0f - rest * rest * rest;
}

[[nodiscard]] auto smoothed(float t_amount) -> float
{
	const float amount = std::clamp(t_amount, 0.0f, 1.0f);

	return amount * amount * (3.0f - 2.0f * amount);
}

[[nodiscard]] auto approached(float t_value, float t_target, float t_step) -> float
{
	return t_value < t_target ? std::min(t_value + t_step, t_target) : std::max(t_value - t_step, t_target);
}

[[nodiscard]] auto contains(const D2D1_RECT_F& t_rect, float t_x, float t_y) -> bool
{
	return t_x >= t_rect.left && t_x < t_rect.right && t_y >= t_rect.top && t_y < t_rect.bottom;
}

[[nodiscard]] auto monitor_scale(HMONITOR t_monitor) -> float
{
	UINT dpi_x = 96;
	UINT dpi_y = 96;
	GetDpiForMonitor(t_monitor, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y);

	return static_cast<float>(dpi_x) / K_REFERENCE_DPI;
}

[[nodiscard]] auto elapsed_label(float t_seconds, bool t_still_counting) -> std::wstring
{
	char buffer[16];

	return os::win32::to_wide(elapsed_time(t_seconds, t_still_counting ? ElapsedStyle::WHOLE_SECONDS : ElapsedStyle::TENTHS_OF_A_SECOND, buffer));
}

[[nodiscard]] auto shown_step(const os::TrayLogin& t_login) -> const os::TrayLoginStep&
{
	for (const os::TrayLoginStep& step : std::span{t_login.steps}.first(t_login.step_count) | std::views::reverse) {
		if (step.state != os::TrayStepState::WAITING) return step;
	}

	return t_login.steps[0];
}

[[nodiscard]] auto shortened_name(std::wstring t_name) -> std::wstring
{
	if (t_name.size() <= K_TOAST_NAME_MAX_CHARACTERS) return t_name;

	t_name.resize(K_TOAST_NAME_MAX_CHARACTERS - 1);
	t_name.push_back(K_ELLIPSIS);

	return t_name;
}

[[nodiscard]] auto is_running(const LoginToast& t_toast) -> bool
{
	return t_toast.login.state == os::TrayLoginState::RUNNING;
}

[[nodiscard]] auto offers_retry(const LoginToast& t_toast) -> bool
{
	return t_toast.login.state == os::TrayLoginState::FAILED;
}

[[nodiscard]] auto toast_time_label(const LoginToast& t_toast) -> std::wstring
{
	if (!is_running(t_toast)) return elapsed_label(t_toast.login.total_seconds, false);

	return elapsed_label(shown_step(t_toast.login).seconds + t_toast.seconds_since_update, true);
}

[[nodiscard]] auto toast_line_width(const ToastMeasures& t_measures) -> float
{
	const float name    = t_measures.name + K_TOAST_GAP;
	const float divider = K_TOAST_DIVIDER_WIDTH + K_TOAST_GAP;
	const float mark    = K_TOAST_MARK_SIZE + K_TOAST_GAP;
	const float label   = t_measures.label + K_TOAST_TIME_GAP;

	return K_TOAST_TEXT_INSET + name + divider + mark + label + t_measures.time + K_TOAST_PADDING_RIGHT;
}

[[nodiscard]] auto toast_details_height(const LoginToast& t_toast) -> float
{
	const float message = t_toast.message.empty() ? 0.0f : K_TOAST_MESSAGE_GAP + t_toast.measures.message_height;
	const float buttons = offers_retry(t_toast) ? K_TOAST_BUTTON_GAP + K_TOAST_BUTTON_HEIGHT : 0.0f;

	return message + buttons;
}

[[nodiscard]] auto toast_target_width(const LoginToast& t_toast) -> float
{
	const ToastMeasures& measures = t_toast.measures;
	const float          line     = toast_line_width(measures);
	if (t_toast.message.empty()) return std::min(line, K_TOAST_MAX_WIDTH);

	const float message = K_TOAST_TEXT_INSET + measures.message + K_TOAST_PADDING_RIGHT;
	const float buttons = offers_retry(t_toast) ? K_TOAST_TEXT_INSET + measures.retry + K_TOAST_BUTTON_SPACING + measures.open + K_TOAST_PADDING_RIGHT : 0.0f;

	return std::min(std::max({line, message, buttons}), K_TOAST_MAX_WIDTH);
}

[[nodiscard]] auto toast_shape(const LoginToast& t_toast, float t_window_width, float t_window_height) -> D2D1_RECT_F
{
	const float height = K_TOAST_PILL_HEIGHT + toast_details_height(t_toast) * smoothed(t_toast.details);
	const float right  = t_window_width - K_SHADOW_MARGIN;
	const float bottom = t_window_height - K_SHADOW_MARGIN;

	return D2D1::RectF(right - t_toast.width, bottom - height, right, bottom);
}

[[nodiscard]] auto toast_radius(const LoginToast& t_toast) -> float
{
	return std::lerp(K_TOAST_PILL_HEIGHT * 0.5f, K_TOAST_GROWN_RADIUS, smoothed(t_toast.details));
}

[[nodiscard]] auto toast_message_top(const D2D1_RECT_F& t_shape) -> float
{
	return t_shape.top + K_TOAST_PADDING_Y + K_TOAST_LINE_HEIGHT + K_TOAST_MESSAGE_GAP;
}

[[nodiscard]] auto toast_buttons(const LoginToast& t_toast, const D2D1_RECT_F& t_shape) -> ToastButtons
{
	const ToastMeasures& measures  = t_toast.measures;
	const float          left      = t_shape.left + K_TOAST_TEXT_INSET;
	const float          top       = toast_message_top(t_shape) + measures.message_height + K_TOAST_BUTTON_GAP;
	const float          open_left = left + measures.retry + K_TOAST_BUTTON_SPACING;

	return ToastButtons{
		.retry = D2D1::RectF(left, top, left + measures.retry, top + K_TOAST_BUTTON_HEIGHT),
		.open  = D2D1::RectF(open_left, top, open_left + measures.open, top + K_TOAST_BUTTON_HEIGHT),
	};
}

[[nodiscard]] auto is_selectable(const Row& t_row) -> bool
{
	return t_row.kind == RowKind::GAME || t_row.kind == RowKind::ACCOUNT || t_row.kind == RowKind::ACTION || t_row.kind == RowKind::OPTION;
}

struct WideText {
	explicit WideText(std::string_view t_utf8)
		: length(MultiByteToWideChar(CP_UTF8, 0, t_utf8.data(), static_cast<int>(t_utf8.size()), text, static_cast<int>(std::size(text))))
	{
	}

	WideText(const WideText&)                    = delete;
	auto operator=(const WideText&) -> WideText& = delete;

	~WideText()
	{
		SecureZeroMemory(text, sizeof(text));
	}

	[[nodiscard]] auto view() const -> std::wstring_view
	{
		return std::wstring_view{text, static_cast<usize>(std::max(length, 0))};
	}

	wchar_t text[K_TEXT_INPUT_CAPACITY]{};
	int     length = 0;
};

[[nodiscard]] auto badge_width(const Painter& t_painter, os::TrayUpdate t_state) -> float
{
	if (t_state == os::TrayUpdate::AVAILABLE) return t_painter.update_width;
	if (t_state == os::TrayUpdate::INSTALLING) return t_painter.updating_width;

	return 0.0f;
}

[[nodiscard]] auto update_rect(const Row& t_row, float t_panel_width, float t_width, float t_scale) -> D2D1_RECT_F
{
	const float right = t_panel_width - K_PANEL_PADDING - (K_ROW_HEIGHT - K_UPDATE_SIZE) * 0.5f;
	const float top   = snapped(t_row.top + (t_row.height - K_UPDATE_SIZE) * 0.5f, t_scale);

	return D2D1::RectF(right - t_width, top, right, top + K_UPDATE_SIZE);
}

[[nodiscard]] auto greyscale_icon(HICON t_icon) -> HICON
{
	ICONINFO info{};
	if (!GetIconInfo(t_icon, &info)) return nullptr;

	HICON  result = nullptr;
	BITMAP source{};

	if (info.hbmColor != nullptr && GetObjectW(info.hbmColor, sizeof(source), &source) != 0) {
		BITMAPINFO format{};
		format.bmiHeader.biSize        = sizeof(format.bmiHeader);
		format.bmiHeader.biWidth       = source.bmWidth;
		format.bmiHeader.biHeight      = -source.bmHeight;
		format.bmiHeader.biPlanes      = 1;
		format.bmiHeader.biBitCount    = 32;
		format.bmiHeader.biCompression = BI_RGB;

		std::vector<u8> pixels(static_cast<usize>(source.bmWidth) * source.bmHeight * 4);
		const HDC       screen = GetDC(nullptr);
		const bool      read   = GetDIBits(screen, info.hbmColor, 0, static_cast<UINT>(source.bmHeight), pixels.data(), &format, DIB_RGB_COLORS) != 0;

		void*         bits = nullptr;
		const HBITMAP grey = read ? CreateDIBSection(screen, &format, DIB_RGB_COLORS, &bits, nullptr, 0) : nullptr;
		ReleaseDC(nullptr, screen);

		if (grey != nullptr && bits != nullptr) {
			for (usize i = 0; i < pixels.size(); i += 4) {
				const auto luma = static_cast<u8>((pixels[i] * 29 + pixels[i + 1] * 150 + pixels[i + 2] * 77) >> 8);
				pixels[i]       = luma;
				pixels[i + 1]   = luma;
				pixels[i + 2]   = luma;
				pixels[i + 3]   = static_cast<u8>(pixels[i + 3] * K_LOCKED_ICON_OPACITY);
			}

			std::copy(pixels.begin(), pixels.end(), static_cast<u8*>(bits));

			ICONINFO grey_info{.fIcon = TRUE, .hbmMask = info.hbmMask, .hbmColor = grey};
			result = CreateIconIndirect(&grey_info);
		}

		if (grey != nullptr) {
			DeleteObject(grey);
		}
	}

	if (info.hbmColor != nullptr) {
		DeleteObject(info.hbmColor);
	}

	if (info.hbmMask != nullptr) {
		DeleteObject(info.hbmMask);
	}

	return result;
}

[[nodiscard]] auto icon_pixels(std::span<const u8> t_png, int t_pixels, float t_radius) -> std::vector<u8>
{
	int width    = 0;
	int height   = 0;
	int channels = 0;
	u8* source   = stbi_load_from_memory(t_png.data(), static_cast<int>(t_png.size()), &width, &height, &channels, 4);
	if (source == nullptr) return {};

	const auto corner = [&](int t_x, int t_y) {
		const float x  = static_cast<float>(t_x) + 0.5f;
		const float y  = static_cast<float>(t_y) + 0.5f;
		const float dx = std::max({t_radius - x, x - (static_cast<float>(t_pixels) - t_radius), 0.0f});
		const float dy = std::max({t_radius - y, y - (static_cast<float>(t_pixels) - t_radius), 0.0f});
		if (dx <= 0.0f || dy <= 0.0f) return 1.0f;

		return std::clamp(t_radius + 0.5f - std::sqrt(dx * dx + dy * dy), 0.0f, 1.0f);
	};

	std::vector<u8> pixels(static_cast<usize>(t_pixels) * t_pixels * 4);

	for (const int y : std::views::iota(0, t_pixels)) {
		const int source_y0 = y * height / t_pixels;
		const int source_y1 = std::max((y + 1) * height / t_pixels, source_y0 + 1);

		for (const int x : std::views::iota(0, t_pixels)) {
			const int source_x0 = x * width / t_pixels;
			const int source_x1 = std::max((x + 1) * width / t_pixels, source_x0 + 1);

			float sum[4]{};
			float samples = 0.0f;

			for (const int sy : std::views::iota(source_y0, std::min(source_y1, height))) {
				for (const int sx : std::views::iota(source_x0, std::min(source_x1, width))) {
					const u8*   texel = source + (static_cast<usize>(sy) * width + sx) * 4;
					const float alpha = texel[3] / 255.0f;

					sum[0] += texel[0] * alpha;
					sum[1] += texel[1] * alpha;
					sum[2] += texel[2] * alpha;
					sum[3] += alpha;
					samples += 1.0f;
				}
			}

			const float coverage = samples > 0.0f ? corner(x, y) / samples : 0.0f;
			u8*         out      = pixels.data() + (static_cast<usize>(y) * t_pixels + x) * 4;

			out[0] = static_cast<u8>(std::lround(sum[2] * coverage));
			out[1] = static_cast<u8>(std::lround(sum[1] * coverage));
			out[2] = static_cast<u8>(std::lround(sum[0] * coverage));
			out[3] = static_cast<u8>(std::lround(sum[3] * coverage * 255.0f));
		}
	}

	stbi_image_free(source);

	return pixels;
}

[[nodiscard]] auto make_bitmap(ID2D1RenderTarget* t_target, std::span<const u8> t_png, float t_size, float t_radius, float t_scale) -> ComPtr<ID2D1Bitmap>
{
	if (t_png.empty()) return nullptr;

	const int             pixels = std::max(1, static_cast<int>(std::lround(t_size * t_scale)));
	const std::vector<u8> data   = icon_pixels(t_png, pixels, t_radius * t_scale);
	if (data.empty()) return nullptr;

	const float                  dpi        = K_REFERENCE_DPI * t_scale;
	const D2D1_BITMAP_PROPERTIES properties = D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), dpi, dpi);
	ComPtr<ID2D1Bitmap>          bitmap;

	t_target->CreateBitmap(D2D1::SizeU(static_cast<UINT32>(pixels), static_cast<UINT32>(pixels)), data.data(), static_cast<UINT32>(pixels) * 4, properties, &bitmap);

	return bitmap;
}

[[nodiscard]] auto prepare_canvas(Canvas* t_canvas, ID2D1Factory* t_factory, SIZE t_size, float t_scale) -> bool
{
	if (t_canvas->dc == nullptr) {
		t_canvas->dc = CreateCompatibleDC(nullptr);
		if (t_canvas->dc == nullptr) return false;
	}

	if (!t_canvas->target) {
		const D2D1_RENDER_TARGET_PROPERTIES properties = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
		if (FAILED(t_factory->CreateDCRenderTarget(&properties, &t_canvas->target))) return false;

		t_canvas->target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
	}

	if (t_canvas->pixels == nullptr || t_size.cx != t_canvas->size.cx || t_size.cy != t_canvas->size.cy) {
		if (t_canvas->pixels != nullptr) {
			SelectObject(t_canvas->dc, t_canvas->replaced);
			DeleteObject(t_canvas->pixels);
			t_canvas->pixels = nullptr;
		}

		BITMAPINFO format{};
		format.bmiHeader = BITMAPINFOHEADER{
			.biSize        = sizeof(BITMAPINFOHEADER),
			.biWidth       = t_size.cx,
			.biHeight      = -t_size.cy,
			.biPlanes      = 1,
			.biBitCount    = 32,
			.biCompression = BI_RGB,
		};

		void* bits       = nullptr;
		t_canvas->pixels = CreateDIBSection(t_canvas->dc, &format, DIB_RGB_COLORS, &bits, nullptr, 0);
		if (t_canvas->pixels == nullptr) return false;

		t_canvas->replaced = SelectObject(t_canvas->dc, t_canvas->pixels);
		t_canvas->size     = t_size;
	}

	const RECT  bounds{0, 0, t_size.cx, t_size.cy};
	const float dpi = K_REFERENCE_DPI * t_scale;
	if (FAILED(t_canvas->target->BindDC(t_canvas->dc, &bounds))) return false;

	t_canvas->target->SetDpi(dpi, dpi);

	return true;
}

auto present_canvas(const Canvas& t_canvas, HWND t_window, POINT t_position, float t_opacity) -> void
{
	POINT         position = t_position;
	POINT         origin{0, 0};
	SIZE          extent = t_canvas.size;
	BLENDFUNCTION blend{AC_SRC_OVER, 0, static_cast<BYTE>(std::lround(std::clamp(t_opacity, 0.0f, 1.0f) * 255.0f)), AC_SRC_ALPHA};

	UpdateLayeredWindow(t_window, nullptr, &position, &extent, t_canvas.dc, &origin, 0, &blend, ULW_ALPHA);
}

auto release_canvas(Canvas* t_canvas) -> void
{
	t_canvas->target.Reset();

	if (t_canvas->pixels != nullptr) {
		SelectObject(t_canvas->dc, t_canvas->replaced);
		DeleteObject(t_canvas->pixels);
	}

	if (t_canvas->dc != nullptr) {
		DeleteDC(t_canvas->dc);
	}

	*t_canvas = Canvas{};
}

[[nodiscard]] auto menu_font_family(IDWriteFactory* t_writer) -> const wchar_t*
{
	ComPtr<IDWriteFontCollection> fonts;

	if (SUCCEEDED(t_writer->GetSystemFontCollection(&fonts))) {
		for (const wchar_t* family : K_FONT_FAMILIES) {
			UINT32 index  = 0;
			BOOL   exists = FALSE;
			if (SUCCEEDED(fonts->FindFamilyName(family, &index, &exists)) && exists) return family;
		}
	}

	return K_FONT_FAMILIES[std::size(K_FONT_FAMILIES) - 1];
}

[[nodiscard]] auto make_format(IDWriteFactory* t_writer, const wchar_t* t_family, float t_size, DWRITE_FONT_WEIGHT t_weight, DWRITE_TEXT_ALIGNMENT t_alignment) -> ComPtr<IDWriteTextFormat>
{
	ComPtr<IDWriteTextFormat> format;
	const HRESULT             created = t_writer->CreateTextFormat(t_family, nullptr, t_weight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, t_size, L"", &format);
	if (FAILED(created)) return nullptr;

	format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
	format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
	format->SetTextAlignment(t_alignment);

	ComPtr<IDWriteInlineObject> ellipsis;
	if (SUCCEEDED(t_writer->CreateEllipsisTrimmingSign(format.Get(), &ellipsis))) {
		const DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
		format->SetTrimming(&trimming, ellipsis.Get());
	}

	return format;
}

[[nodiscard]] auto text_width(IDWriteFactory* t_writer, IDWriteTextFormat* t_format, std::wstring_view t_text) -> float
{
	ComPtr<IDWriteTextLayout> layout;
	if (t_format == nullptr || FAILED(t_writer->CreateTextLayout(t_text.data(), static_cast<UINT32>(t_text.size()), t_format, 4096.0f, 256.0f, &layout))) {
		return 0.0f;
	}

	DWRITE_TEXT_METRICS metrics{};
	layout->GetMetrics(&metrics);

	return std::ceil(metrics.widthIncludingTrailingWhitespace);
}

[[nodiscard]] auto utf8_width(IDWriteFactory* t_writer, IDWriteTextFormat* t_format, std::string_view t_text) -> float
{
	const WideText wide{t_text};

	return text_width(t_writer, t_format, wide.view());
}

[[nodiscard]] auto text_height(IDWriteFactory* t_writer, IDWriteTextFormat* t_format, std::wstring_view t_text, float t_width) -> float
{
	ComPtr<IDWriteTextLayout> layout;
	if (t_format == nullptr || FAILED(t_writer->CreateTextLayout(t_text.data(), static_cast<UINT32>(t_text.size()), t_format, t_width, 1024.0f, &layout))) {
		return 0.0f;
	}

	DWRITE_TEXT_METRICS metrics{};
	layout->GetMetrics(&metrics);

	return std::ceil(metrics.height);
}

auto draw_label(ID2D1RenderTarget* t_target, IDWriteTextFormat* t_format, std::wstring_view t_text, float t_left, float t_right, const D2D1_RECT_F& t_row, ID2D1Brush* t_brush) -> void
{
	if (t_format == nullptr || t_text.empty() || t_right <= t_left) return;

	t_target->DrawText(t_text.data(), static_cast<UINT32>(t_text.size()), t_format, D2D1::RectF(t_left, t_row.top, t_right, t_row.bottom), t_brush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

// Tabler's outline icons, drawn from their 24 unit outlines so they stay sharp at any scale.
auto draw_glyph(ID2D1RenderTarget* t_target, const Painter& t_painter, Glyph t_glyph, const D2D1_RECT_F& t_box, ID2D1Brush* t_brush) -> void
{
	const float unit   = (t_box.right - t_box.left) / 24.0f;
	const float stroke = 2.0f * unit;
	const auto  at     = [&](float t_x, float t_y) { return D2D1::Point2F(t_box.left + t_x * unit, t_box.top + t_y * unit); };

	const auto trace = [&](auto&& t_build) {
		ComPtr<ID2D1PathGeometry> path;
		ComPtr<ID2D1GeometrySink> sink;
		if (FAILED(t_painter.factory->CreatePathGeometry(&path)) || FAILED(path->Open(&sink))) return;

		t_build(sink.Get());
		if (SUCCEEDED(sink->Close())) {
			t_target->DrawGeometry(path.Get(), t_brush, stroke, t_painter.stroke);
		}
	};

	const auto frame = [&](float t_left, float t_top, float t_right, float t_bottom) {
		const D2D1_ROUNDED_RECT shape{D2D1::RectF(at(t_left, t_top).x, at(t_left, t_top).y, at(t_right, t_bottom).x, at(t_right, t_bottom).y), 2.0f * unit, 2.0f * unit};
		t_target->DrawRoundedRectangle(shape, t_brush, stroke, t_painter.stroke);
	};

	const auto dot = [&](float t_x, float t_y, float t_radius) { t_target->FillEllipse(D2D1::Ellipse(at(t_x, t_y), t_radius, t_radius), t_brush); };

	const auto corner = [&](float t_x, float t_y, D2D1_SWEEP_DIRECTION t_direction) { return D2D1::ArcSegment(at(t_x, t_y), D2D1::SizeF(2.0f * unit, 2.0f * unit), 0.0f, t_direction, D2D1_ARC_SIZE_SMALL); };

	switch (t_glyph) {
		using enum Glyph;

		case OPEN: {
			trace([&](ID2D1GeometrySink* t_sink) {
				t_sink->BeginFigure(at(12.0f, 6.0f), D2D1_FIGURE_BEGIN_HOLLOW);
				t_sink->AddLine(at(6.0f, 6.0f));
				t_sink->AddArc(corner(4.0f, 8.0f, D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE));
				t_sink->AddLine(at(4.0f, 18.0f));
				t_sink->AddArc(corner(6.0f, 20.0f, D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE));
				t_sink->AddLine(at(16.0f, 20.0f));
				t_sink->AddArc(corner(18.0f, 18.0f, D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE));
				t_sink->AddLine(at(18.0f, 12.0f));
				t_sink->EndFigure(D2D1_FIGURE_END_OPEN);
				t_sink->BeginFigure(at(11.0f, 13.0f), D2D1_FIGURE_BEGIN_HOLLOW);
				t_sink->AddLine(at(20.0f, 4.0f));
				t_sink->EndFigure(D2D1_FIGURE_END_OPEN);
				t_sink->BeginFigure(at(15.0f, 4.0f), D2D1_FIGURE_BEGIN_HOLLOW);
				t_sink->AddLine(at(20.0f, 4.0f));
				t_sink->AddLine(at(20.0f, 9.0f));
				t_sink->EndFigure(D2D1_FIGURE_END_OPEN);
			});
			break;
		}

		case LOCK: {
			frame(5.0f, 11.0f, 19.0f, 21.0f);
			dot(12.0f, 16.0f, unit + stroke * 0.5f);
			trace([&](ID2D1GeometrySink* t_sink) {
				t_sink->BeginFigure(at(8.0f, 11.0f), D2D1_FIGURE_BEGIN_HOLLOW);
				t_sink->AddLine(at(8.0f, 7.0f));
				t_sink->AddArc(D2D1::ArcSegment(at(16.0f, 7.0f), D2D1::SizeF(4.0f * unit, 4.0f * unit), 0.0f, D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
				t_sink->AddLine(at(16.0f, 11.0f));
				t_sink->EndFigure(D2D1_FIGURE_END_OPEN);
			});
			break;
		}

		case POWER: {
			trace([&](ID2D1GeometrySink* t_sink) {
				t_sink->BeginFigure(at(7.0f, 6.0f), D2D1_FIGURE_BEGIN_HOLLOW);
				t_sink->AddArc(D2D1::ArcSegment(at(17.0f, 6.0f), D2D1::SizeF(7.75f * unit, 7.75f * unit), 0.0f, D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE, D2D1_ARC_SIZE_LARGE));
				t_sink->EndFigure(D2D1_FIGURE_END_OPEN);
				t_sink->BeginFigure(at(12.0f, 4.0f), D2D1_FIGURE_BEGIN_HOLLOW);
				t_sink->AddLine(at(12.0f, 12.0f));
				t_sink->EndFigure(D2D1_FIGURE_END_OPEN);
			});
			break;
		}

		case CHEVRON: {
			trace([&](ID2D1GeometrySink* t_sink) {
				t_sink->BeginFigure(at(9.0f, 6.0f), D2D1_FIGURE_BEGIN_HOLLOW);
				t_sink->AddLine(at(15.0f, 12.0f));
				t_sink->AddLine(at(9.0f, 18.0f));
				t_sink->EndFigure(D2D1_FIGURE_END_OPEN);
			});
			break;
		}

		case CHECK_CIRCLE: {
			t_target->DrawEllipse(D2D1::Ellipse(at(12.0f, 12.0f), 9.0f * unit, 9.0f * unit), t_brush, stroke, t_painter.stroke);
			trace([&](ID2D1GeometrySink* t_sink) {
				t_sink->BeginFigure(at(9.0f, 12.0f), D2D1_FIGURE_BEGIN_HOLLOW);
				t_sink->AddLine(at(11.0f, 14.0f));
				t_sink->AddLine(at(15.0f, 10.0f));
				t_sink->EndFigure(D2D1_FIGURE_END_OPEN);
			});
			break;
		}

		case ALERT_CIRCLE: {
			t_target->DrawEllipse(D2D1::Ellipse(at(12.0f, 12.0f), 9.0f * unit, 9.0f * unit), t_brush, stroke, t_painter.stroke);
			t_target->DrawLine(at(12.0f, 8.0f), at(12.0f, 12.0f), t_brush, stroke, t_painter.stroke);
			dot(12.0f, 16.0f, stroke * 0.5f);
			break;
		}

		case REFRESH: {
			const D2D1_SIZE_F radius = D2D1::SizeF(8.1f * unit, 8.1f * unit);

			trace([&](ID2D1GeometrySink* t_sink) {
				t_sink->BeginFigure(at(20.0f, 11.0f), D2D1_FIGURE_BEGIN_HOLLOW);
				t_sink->AddArc(D2D1::ArcSegment(at(4.5f, 9.0f), radius, 0.0f, D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
				t_sink->EndFigure(D2D1_FIGURE_END_OPEN);
				t_sink->BeginFigure(at(4.0f, 5.0f), D2D1_FIGURE_BEGIN_HOLLOW);
				t_sink->AddLine(at(4.0f, 9.0f));
				t_sink->AddLine(at(8.0f, 9.0f));
				t_sink->EndFigure(D2D1_FIGURE_END_OPEN);
				t_sink->BeginFigure(at(4.0f, 13.0f), D2D1_FIGURE_BEGIN_HOLLOW);
				t_sink->AddArc(D2D1::ArcSegment(at(19.5f, 15.0f), radius, 0.0f, D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
				t_sink->EndFigure(D2D1_FIGURE_END_OPEN);
				t_sink->BeginFigure(at(20.0f, 19.0f), D2D1_FIGURE_BEGIN_HOLLOW);
				t_sink->AddLine(at(20.0f, 15.0f));
				t_sink->AddLine(at(16.0f, 15.0f));
				t_sink->EndFigure(D2D1_FIGURE_END_OPEN);
			});
			break;
		}

		case DOWNLOAD: {
			trace([&](ID2D1GeometrySink* t_sink) {
				t_sink->BeginFigure(at(4.0f, 17.0f), D2D1_FIGURE_BEGIN_HOLLOW);
				t_sink->AddLine(at(4.0f, 19.0f));
				t_sink->AddArc(corner(6.0f, 21.0f, D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE));
				t_sink->AddLine(at(18.0f, 21.0f));
				t_sink->AddArc(corner(20.0f, 19.0f, D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE));
				t_sink->AddLine(at(20.0f, 17.0f));
				t_sink->EndFigure(D2D1_FIGURE_END_OPEN);
				t_sink->BeginFigure(at(7.0f, 11.0f), D2D1_FIGURE_BEGIN_HOLLOW);
				t_sink->AddLine(at(12.0f, 16.0f));
				t_sink->AddLine(at(17.0f, 11.0f));
				t_sink->EndFigure(D2D1_FIGURE_END_OPEN);
				t_sink->BeginFigure(at(12.0f, 4.0f), D2D1_FIGURE_BEGIN_HOLLOW);
				t_sink->AddLine(at(12.0f, 16.0f));
				t_sink->EndFigure(D2D1_FIGURE_END_OPEN);
			});
			break;
		}

		case ADD: {
			trace([&](ID2D1GeometrySink* t_sink) {
				t_sink->BeginFigure(at(12.0f, 5.0f), D2D1_FIGURE_BEGIN_HOLLOW);
				t_sink->AddLine(at(12.0f, 19.0f));
				t_sink->EndFigure(D2D1_FIGURE_END_OPEN);
				t_sink->BeginFigure(at(5.0f, 12.0f), D2D1_FIGURE_BEGIN_HOLLOW);
				t_sink->AddLine(at(19.0f, 12.0f));
				t_sink->EndFigure(D2D1_FIGURE_END_OPEN);
			});
			break;
		}

		case EDIT: {
			const D2D1_SIZE_F tip = D2D1::SizeF(2.828f * unit, 2.828f * unit);

			trace([&](ID2D1GeometrySink* t_sink) {
				t_sink->BeginFigure(at(4.0f, 20.0f), D2D1_FIGURE_BEGIN_HOLLOW);
				t_sink->AddLine(at(8.0f, 20.0f));
				t_sink->AddLine(at(18.5f, 9.5f));
				t_sink->AddArc(D2D1::ArcSegment(at(14.5f, 5.5f), tip, 0.0f, D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE, D2D1_ARC_SIZE_LARGE));
				t_sink->AddLine(at(4.0f, 16.0f));
				t_sink->AddLine(at(4.0f, 20.0f));
				t_sink->EndFigure(D2D1_FIGURE_END_OPEN);
				t_sink->BeginFigure(at(13.5f, 6.5f), D2D1_FIGURE_BEGIN_HOLLOW);
				t_sink->AddLine(at(17.5f, 10.5f));
				t_sink->EndFigure(D2D1_FIGURE_END_OPEN);
			});
			break;
		}

		case EYE:
		case EYE_OFF: {
			const D2D1_SIZE_F lid = D2D1::SizeF(10.6f * unit, 10.6f * unit);

			t_target->DrawEllipse(D2D1::Ellipse(at(12.0f, 12.0f), 2.0f * unit, 2.0f * unit), t_brush, stroke, t_painter.stroke);
			trace([&](ID2D1GeometrySink* t_sink) {
				t_sink->BeginFigure(at(3.0f, 12.0f), D2D1_FIGURE_BEGIN_HOLLOW);
				t_sink->AddArc(D2D1::ArcSegment(at(21.0f, 12.0f), lid, 0.0f, D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
				t_sink->AddArc(D2D1::ArcSegment(at(3.0f, 12.0f), lid, 0.0f, D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
				t_sink->EndFigure(D2D1_FIGURE_END_CLOSED);
			});

			if (t_glyph == EYE_OFF) {
				t_target->DrawLine(at(3.0f, 3.0f), at(21.0f, 21.0f), t_brush, stroke, t_painter.stroke);
			}

			break;
		}

		case TRASH: {
			trace([&](ID2D1GeometrySink* t_sink) {
				t_sink->BeginFigure(at(4.0f, 7.0f), D2D1_FIGURE_BEGIN_HOLLOW);
				t_sink->AddLine(at(20.0f, 7.0f));
				t_sink->EndFigure(D2D1_FIGURE_END_OPEN);
				t_sink->BeginFigure(at(10.0f, 11.0f), D2D1_FIGURE_BEGIN_HOLLOW);
				t_sink->AddLine(at(10.0f, 17.0f));
				t_sink->EndFigure(D2D1_FIGURE_END_OPEN);
				t_sink->BeginFigure(at(14.0f, 11.0f), D2D1_FIGURE_BEGIN_HOLLOW);
				t_sink->AddLine(at(14.0f, 17.0f));
				t_sink->EndFigure(D2D1_FIGURE_END_OPEN);
				t_sink->BeginFigure(at(5.0f, 7.0f), D2D1_FIGURE_BEGIN_HOLLOW);
				t_sink->AddLine(at(6.0f, 19.0f));
				t_sink->AddArc(corner(8.0f, 21.0f, D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE));
				t_sink->AddLine(at(16.0f, 21.0f));
				t_sink->AddArc(corner(18.0f, 19.0f, D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE));
				t_sink->AddLine(at(19.0f, 7.0f));
				t_sink->EndFigure(D2D1_FIGURE_END_OPEN);
				t_sink->BeginFigure(at(9.0f, 7.0f), D2D1_FIGURE_BEGIN_HOLLOW);
				t_sink->AddLine(at(9.0f, 4.0f));
				t_sink->AddLine(at(15.0f, 4.0f));
				t_sink->AddLine(at(15.0f, 7.0f));
				t_sink->EndFigure(D2D1_FIGURE_END_OPEN);
			});
			break;
		}

		case COPY: {
			frame(8.0f, 8.0f, 20.0f, 20.0f);
			trace([&](ID2D1GeometrySink* t_sink) {
				t_sink->BeginFigure(at(16.0f, 8.0f), D2D1_FIGURE_BEGIN_HOLLOW);
				t_sink->AddLine(at(16.0f, 6.0f));
				t_sink->AddArc(corner(14.0f, 4.0f, D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE));
				t_sink->AddLine(at(6.0f, 4.0f));
				t_sink->AddArc(corner(4.0f, 6.0f, D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE));
				t_sink->AddLine(at(4.0f, 14.0f));
				t_sink->AddArc(corner(6.0f, 16.0f, D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE));
				t_sink->AddLine(at(8.0f, 16.0f));
				t_sink->EndFigure(D2D1_FIGURE_END_OPEN);
			});
			break;
		}

		case KEY: {
			t_target->DrawEllipse(D2D1::Ellipse(at(15.0f, 9.0f), 5.0f * unit, 5.0f * unit), t_brush, stroke, t_painter.stroke);
			dot(15.0f, 9.0f, unit);
			trace([&](ID2D1GeometrySink* t_sink) {
				t_sink->BeginFigure(at(11.5f, 12.5f), D2D1_FIGURE_BEGIN_HOLLOW);
				t_sink->AddLine(at(4.0f, 20.0f));
				t_sink->EndFigure(D2D1_FIGURE_END_OPEN);
				t_sink->BeginFigure(at(6.5f, 17.5f), D2D1_FIGURE_BEGIN_HOLLOW);
				t_sink->AddLine(at(8.5f, 19.5f));
				t_sink->EndFigure(D2D1_FIGURE_END_OPEN);
				t_sink->BeginFigure(at(8.5f, 15.5f), D2D1_FIGURE_BEGIN_HOLLOW);
				t_sink->AddLine(at(10.5f, 17.5f));
				t_sink->EndFigure(D2D1_FIGURE_END_OPEN);
			});
			break;
		}

		case NONE: {
			break;
		}
	}
}

auto draw_star(ID2D1RenderTarget* t_target, const Painter& t_painter, const D2D1_RECT_F& t_box, bool t_filled, ID2D1Brush* t_brush) -> void
{
	constexpr D2D1_POINT_2F K_STAR_POINTS[]{{12.0f, 17.75f}, {5.828f, 20.995f}, {7.007f, 14.122f}, {2.007f, 9.255f}, {8.907f, 8.255f}, {11.993f, 2.002f}, {15.079f, 8.255f}, {21.979f, 9.255f}, {16.979f, 14.122f}, {18.158f, 20.995f}};

	const float unit = (t_box.right - t_box.left) / 24.0f;
	const auto  at   = [&](D2D1_POINT_2F t_point) { return D2D1::Point2F(t_box.left + t_point.x * unit, t_box.top + t_point.y * unit); };

	ComPtr<ID2D1PathGeometry> star;
	ComPtr<ID2D1GeometrySink> sink;
	if (FAILED(t_painter.factory->CreatePathGeometry(&star)) || FAILED(star->Open(&sink))) return;

	sink->BeginFigure(at(K_STAR_POINTS[0]), t_filled ? D2D1_FIGURE_BEGIN_FILLED : D2D1_FIGURE_BEGIN_HOLLOW);
	for (const D2D1_POINT_2F point : std::span{K_STAR_POINTS}.subspan(1)) {
		sink->AddLine(at(point));
	}
	sink->EndFigure(D2D1_FIGURE_END_CLOSED);

	if (FAILED(sink->Close())) return;

	if (t_filled) {
		t_target->FillGeometry(star.Get(), t_brush);
	}

	t_target->DrawGeometry(star.Get(), t_brush, 2.0f * unit, t_painter.stroke);
}

auto draw_turned_glyph(ID2D1RenderTarget* t_target, const Painter& t_painter, Glyph t_glyph, const D2D1_RECT_F& t_box, float t_degrees, ID2D1Brush* t_brush) -> void
{
	const D2D1_POINT_2F center = D2D1::Point2F((t_box.left + t_box.right) * 0.5f, (t_box.top + t_box.bottom) * 0.5f);

	D2D1::Matrix3x2F upright;
	t_target->GetTransform(&upright);

	t_target->SetTransform(D2D1::Matrix3x2F::Rotation(t_degrees, center) * upright);
	draw_glyph(t_target, t_painter, t_glyph, t_box, t_brush);
	t_target->SetTransform(upright);
}

[[nodiscard]] auto centered_box(float t_left, float t_middle, float t_size, float t_scale) -> D2D1_RECT_F
{
	const float left = snapped(t_left, t_scale);
	const float top  = snapped(t_middle - t_size * 0.5f, t_scale);

	return D2D1::RectF(left, top, left + t_size, top + t_size);
}

auto set_hand_cursor(bool t_hand) -> void
{
	const LPCWSTR shape = t_hand ? IDC_HAND : IDC_ARROW;
	SetCursor(LoadCursorW(nullptr, shape));
}

auto draw_bitmap(ID2D1RenderTarget* t_target, ID2D1Bitmap* t_bitmap, float t_left, float t_middle, float t_scale, float t_opacity = 1.0f) -> void
{
	const D2D1_SIZE_F size        = t_bitmap->GetSize();
	const float       left        = snapped(t_left, t_scale);
	const float       top         = snapped(t_middle - size.height * 0.5f, t_scale);
	const D2D1_RECT_F destination = D2D1::RectF(left, top, left + size.width, top + size.height);

	t_target->DrawBitmap(t_bitmap, destination, t_opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
}

auto draw_soft_shadow(ID2D1RenderTarget* t_target, ID2D1SolidColorBrush* t_brush, const D2D1_RECT_F& t_shape, float t_radius, float t_strength = 1.0f) -> void
{
	const float step        = (K_SHADOW_MARGIN - K_SHADOW_DROP) / static_cast<float>(K_SHADOW_LAYERS);
	const float layer_alpha = K_SHADOW_ALPHA * t_strength / static_cast<float>(K_SHADOW_LAYERS);
	t_brush->SetColor(D2D1::ColorF(0.0f, 0.0f, 0.0f, layer_alpha));

	for (u32 layer = K_SHADOW_LAYERS; layer > 0; layer -= 1) {
		const float             spread = static_cast<float>(layer) * step;
		const float             radius = t_radius + spread;
		const D2D1_RECT_F       area   = D2D1::RectF(t_shape.left - spread, t_shape.top - spread + K_SHADOW_DROP, t_shape.right + spread, t_shape.bottom + spread + K_SHADOW_DROP);
		const D2D1_ROUNDED_RECT shadow{area, radius, radius};

		t_target->FillRoundedRectangle(shadow, t_brush);
	}
}

auto paint_body(ID2D1RenderTarget* t_target, ID2D1SolidColorBrush* t_brush, const os::TrayColors& t_colors, const D2D1_RECT_F& t_shape, float t_radius, Color t_edge, float t_scale) -> void
{
	const float             half = 0.5f / t_scale;
	const D2D1_ROUNDED_RECT body{t_shape, t_radius, t_radius};
	const D2D1_ROUNDED_RECT edge{D2D1::RectF(t_shape.left + half, t_shape.top + half, t_shape.right - half, t_shape.bottom - half), t_radius, t_radius};

	t_brush->SetColor(to_d2d(t_colors.background));
	t_target->FillRoundedRectangle(body, t_brush);
	t_brush->SetColor(to_d2d(t_edge));
	t_target->DrawRoundedRectangle(edge, t_brush, 1.0f / t_scale);
}

// Filling the body's inner outline keeps the shadow inside the rounded corners.
auto paint_edge_shadows(ID2D1RenderTarget* t_target, const Painter& t_painter, const Panel& t_panel) -> void
{
	const float             inset = 1.0f / t_painter.scale;
	const float             width = t_panel.frame.right - t_panel.frame.left;
	const float             round = K_SURFACE_RADIUS - inset * 0.5f;
	const D2D1_ROUNDED_RECT inside{D2D1::RectF(inset, inset, width - inset, t_panel.height - inset), round, round};

	const auto shade = [&](float t_hidden, float t_edge, float t_fade_to) {
		const float strength = std::min(t_hidden / K_EDGE_SHADOW_TRAVEL, 1.0f);
		if (strength <= 0.0f) return;

		const D2D1_GRADIENT_STOP stops[] = {
			{0.0f, to_d2d(t_painter.colors.shadow, K_EDGE_SHADOW_ALPHA * strength)},
			{1.0f, to_d2d(t_painter.colors.shadow, 0.0f)},
		};
		const D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES line = D2D1::LinearGradientBrushProperties(D2D1::Point2F(0.0f, t_edge), D2D1::Point2F(0.0f, t_fade_to));
		ComPtr<ID2D1GradientStopCollection>         collection;
		ComPtr<ID2D1LinearGradientBrush>            brush;

		if (FAILED(t_target->CreateGradientStopCollection(stops, 2, &collection))) return;
		if (FAILED(t_target->CreateLinearGradientBrush(line, collection.Get(), &brush))) return;

		t_target->FillRoundedRectangle(inside, brush.Get());
	};

	const float hidden_above = t_panel.scroll.offset;
	const float hidden_below = max_scroll(t_panel) - t_panel.scroll.offset;
	const float top_edge     = inset;
	const float bottom_edge  = t_panel.height - inset;

	shade(hidden_above, top_edge, top_edge + K_EDGE_SHADOW_HEIGHT);
	shade(hidden_below, bottom_edge, bottom_edge - K_EDGE_SHADOW_HEIGHT);
}

auto paint_scrollbar(ID2D1RenderTarget* t_target, const Painter& t_painter, ID2D1SolidColorBrush* t_brush, const Panel& t_panel) -> void
{
	const os::TrayColors& colors = t_painter.colors;
	const Scroll&         scroll = t_panel.scroll;
	const float           width  = K_SCROLLBAR_THIN + (K_SCROLLBAR_WIDE - K_SCROLLBAR_THIN) * scroll.thickness;
	const float           inset  = (K_SCROLLBAR_WIDE - width) * 0.5f;
	const float           round  = width * 0.5f;
	const D2D1_RECT_F     track  = track_rect(t_panel);
	const D2D1_RECT_F     thumb  = thumb_rect(t_panel);

	const D2D1_ROUNDED_RECT shown_track{D2D1::RectF(track.left + inset, track.top, track.right - inset, track.bottom), round, round};
	const D2D1_ROUNDED_RECT shown_thumb{D2D1::RectF(thumb.left + inset, thumb.top, thumb.right - inset, thumb.bottom), round, round};

	Color lit = mix(colors.scroll_thumb, colors.text, K_THUMB_HOVER_STRENGTH);
	lit.a     = colors.scroll_thumb.a;

	const bool  thumb_lit   = scroll.grab || scroll.over_thumb;
	const Color thumb_color = thumb_lit ? lit : colors.scroll_thumb;

	if (scroll.thickness > 0.001f) {
		t_brush->SetColor(to_d2d(colors.separator, scroll.thickness));
		t_target->FillRoundedRectangle(shown_track, t_brush);
	}

	t_brush->SetColor(to_d2d(thumb_color));
	t_target->FillRoundedRectangle(shown_thumb, t_brush);
}

auto paint_update(ID2D1RenderTarget* t_target, const Painter& t_painter, ID2D1SolidColorBrush* t_brush, const UpdateBadge& t_badge, const D2D1_RECT_F& t_box) -> void
{
	const os::TrayColors& colors  = t_painter.colors;
	const TextFormats&    formats = *t_painter.formats;
	const float           scale   = t_painter.scale;
	const float           shown   = smoothed(t_badge.shown);
	const float           middle  = (t_box.top + t_box.bottom) * 0.5f;

	const auto paint = [&](Color t_color, float t_opacity = 1.0f) {
		t_brush->SetColor(to_d2d(t_color, t_opacity * shown));
		return t_brush;
	};

	if (t_badge.state == os::TrayUpdate::NONE) return;

	D2D1::Matrix3x2F base;
	t_target->GetTransform(&base);

	const float            size       = lerp(K_UPDATE_FROM_SCALE, 1.0f, eased_out(t_badge.shown));
	const D2D1_POINT_2F    box_center = D2D1::Point2F((t_box.left + t_box.right) * 0.5f, middle);
	const D2D1::Matrix3x2F popping    = D2D1::Matrix3x2F::Scale(size, size, box_center);
	t_target->SetTransform(popping * base);

	switch (t_badge.state) {
		using enum os::TrayUpdate;

		case AVAILABLE: {
			const D2D1_ROUNDED_RECT pill{t_box, K_UPDATE_RADIUS, K_UPDATE_RADIUS};
			const Color             ink   = mix(colors.accent, colors.accent_ink, t_badge.hover);
			const D2D1_RECT_F       glyph = centered_box(t_box.left + K_UPDATE_PADDING, middle, K_UPDATE_GLYPH_SIZE, scale);

			const float fill_alpha = lerp(K_UPDATE_FILL_ALPHA, 1.0f, t_badge.hover);
			t_target->FillRoundedRectangle(pill, paint(colors.accent, fill_alpha));
			draw_glyph(t_target, t_painter, Glyph::DOWNLOAD, glyph, paint(ink));
			draw_label(t_target, formats.badge.Get(), K_UPDATE_LABEL, glyph.right + K_UPDATE_GAP, t_box.right, t_box, paint(ink));
			break;
		}

		case INSTALLING: {
			const D2D1_ROUNDED_RECT pill{t_box, K_UPDATE_RADIUS, K_UPDATE_RADIUS};
			const D2D1_RECT_F       glyph = centered_box(t_box.left + K_UPDATE_PADDING, middle, K_UPDATE_GLYPH_SIZE, scale);

			const D2D1_POINT_2F center     = D2D1::Point2F((glyph.left + glyph.right) * 0.5f, (glyph.top + glyph.bottom) * 0.5f);
			const float         fill_alpha = K_CHIP_ALPHA + K_CHIP_ALPHA * t_badge.hover;
			D2D1::Matrix3x2F    upright;
			t_target->GetTransform(&upright);

			const D2D1::Matrix3x2F spinning = D2D1::Matrix3x2F::Rotation(t_badge.spin * 360.0f, center);

			t_target->FillRoundedRectangle(pill, paint(colors.text, fill_alpha));
			t_target->SetTransform(spinning * upright);
			draw_glyph(t_target, t_painter, Glyph::REFRESH, glyph, paint(colors.accent));
			t_target->SetTransform(upright);
			draw_label(t_target, formats.badge.Get(), K_UPDATING_LABEL, glyph.right + K_UPDATE_GAP, t_box.right, t_box, paint(colors.text_dim));
			break;
		}

		case NONE: {
			break;
		}
	}

	t_target->SetTransform(base);
}

[[nodiscard]] auto caret_size(IDWriteFactory* t_writer, IDWriteTextFormat* t_format, std::string_view t_shown, u32 t_cursor) -> Vec2
{
	if (caret_style() == CaretStyle::BAR) return Vec2{K_CARET_WIDTH, K_TEXT_LINE_HEIGHT};

	const usize next  = next_codepoint(t_shown, t_cursor);
	const float under = next > t_cursor ? utf8_width(t_writer, t_format, t_shown.substr(t_cursor, next - t_cursor)) : text_width(t_writer, t_format, K_END_OF_TEXT_CELL);
	const float width = std::max(under, K_CARET_WIDTH);

	return Vec2{width, caret_style() == CaretStyle::BLOCK ? K_TEXT_LINE_HEIGHT : K_UNDERLINE_CARET_HEIGHT};
}

struct TextFieldLook {
	const TextEditor& editor;
	D2D1_RECT_F       text;
	float             scroll;
	bool              focused;
	std::wstring_view placeholder;
	Color             placeholder_color;
};

struct CaretLook {
	const TextEditor*  editor = nullptr;
	const CaretMotion* motion = nullptr;
	D2D1_RECT_F        text{};
	float              scroll  = 0.0f;
	bool               blinked = false;
};

auto paint_typed_text(ID2D1RenderTarget* t_target, const Painter& t_painter, const TextEditor& t_editor, const D2D1_RECT_F& t_text, float t_scroll, Color t_color, const auto& t_paint) -> void
{
	IDWriteTextFormat* body = t_painter.formats->body.Get();
	char               mask[K_TEXT_INPUT_CAPACITY];
	const WideText     wide{t_editor.shown_text(mask)};
	const float        origin = t_text.left - t_scroll;
	const float        width  = text_width(t_painter.writer, body, wide.view());

	draw_label(t_target, body, wide.view(), origin, origin + width + K_FORM_FIELD_PADDING, t_text, t_paint(t_color));
}

auto paint_text_field(ID2D1RenderTarget* t_target, const Painter& t_painter, const TextFieldLook& t_look, const auto& t_paint) -> void
{
	const os::TrayColors& colors = t_painter.colors;
	IDWriteTextFormat*    body   = t_painter.formats->body.Get();
	const D2D1_RECT_F     text   = t_look.text;

	t_target->PushAxisAlignedClip(D2D1::RectF(text.left, text.top - K_TEXT_CLIP_BLEED, text.right, text.bottom + K_TEXT_CLIP_BLEED), D2D1_ANTIALIAS_MODE_ALIASED);

	if (t_look.focused && t_look.editor.has_selection()) {
		char                   mask[K_TEXT_INPUT_CAPACITY];
		const std::string_view shown  = t_look.editor.shown_text(mask);
		const TextRange        range  = t_look.editor.selection();
		const float            origin = text.left - t_look.scroll;
		const float            start  = origin + utf8_width(t_painter.writer, body, shown.substr(0, range.start));
		const float            end    = origin + utf8_width(t_painter.writer, body, shown.substr(0, range.end));

		t_target->FillRectangle(D2D1::RectF(start, text.top, end, text.bottom), t_paint(colors.accent, K_SELECTION_ALPHA));
	}

	if (t_look.editor.value().empty()) {
		draw_label(t_target, body, t_look.placeholder, text.left, text.right, text, t_paint(t_look.placeholder_color));
	} else {
		paint_typed_text(t_target, t_painter, t_look.editor, text, t_look.scroll, colors.text, t_paint);
	}

	t_target->PopAxisAlignedClip();
}

auto fill_caret_trail(ID2D1RenderTarget* t_target, const Painter& t_painter, const CaretTrail& t_trail, const CaretMotion& t_caret, Vec2 t_size) -> void
{
	ComPtr<ID2D1PathGeometry> outline;
	ComPtr<ID2D1GeometrySink> sink;
	if (FAILED(t_painter.factory->CreatePathGeometry(&outline)) || FAILED(outline->Open(&sink))) return;

	sink->BeginFigure(D2D1::Point2F(t_trail.points[0].x, t_trail.points[0].y), D2D1_FIGURE_BEGIN_FILLED);

	for (const Vec2 point : std::span{t_trail.points}.subspan(1, t_trail.count - 1)) {
		sink->AddLine(D2D1::Point2F(point.x, point.y));
	}

	sink->EndFigure(D2D1_FIGURE_END_CLOSED);
	sink->Close();

	const std::span<const Color> colors{t_trail.colors, t_trail.count};
	const Color                  head  = *std::ranges::max_element(colors, {}, &Color::a);
	const Color                  faint = *std::ranges::min_element(colors, {}, &Color::a);
	const D2D1_GRADIENT_STOP     stops[]{{0.0f, to_d2d(head)}, {1.0f, to_d2d(faint)}};
	const D2D1_POINT_2F          from = D2D1::Point2F(t_caret.head().x + t_size.x * 0.5f, t_caret.head().y + t_size.y * 0.5f);
	const D2D1_POINT_2F          to   = D2D1::Point2F(t_caret.tail().x + t_size.x * 0.5f, t_caret.tail().y + t_size.y * 0.5f);

	ComPtr<ID2D1GradientStopCollection> gradient;
	ComPtr<ID2D1LinearGradientBrush>    brush;
	if (FAILED(t_target->CreateGradientStopCollection(stops, static_cast<UINT32>(std::size(stops)), &gradient))) return;
	if (FAILED(t_target->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(from, to), gradient.Get(), &brush))) return;

	t_target->FillGeometry(outline.Get(), brush.Get());
}

auto paint_caret(ID2D1RenderTarget* t_target, const Painter& t_painter, const CaretLook& t_look, const auto& t_paint) -> void
{
	if (!t_look.blinked || t_look.editor == nullptr || t_look.motion == nullptr) return;

	const CaretMotion&     motion = *t_look.motion;
	const Color            color  = t_painter.colors.accent;
	char                   mask[K_TEXT_INPUT_CAPACITY];
	const std::string_view shown = t_look.editor->shown_text(mask);
	const Vec2             size  = caret_size(t_painter.writer, t_painter.formats->body.Get(), shown, t_look.editor->cursor());
	const CaretTrail       trail = motion.trail(Vec2{}, size, color);
	const D2D1_RECT_F      head  = D2D1::RectF(motion.head().x, motion.head().y, motion.head().x + size.x, motion.head().y + size.y);

	if (trail.count > 0) {
		fill_caret_trail(t_target, t_painter, trail, motion, size);
	}

	t_target->FillRectangle(head, t_paint(color));

	if (caret_style() == CaretStyle::BLOCK) {
		t_target->PushAxisAlignedClip(head, D2D1_ANTIALIAS_MODE_ALIASED);
		paint_typed_text(t_target, t_painter, *t_look.editor, t_look.text, t_look.scroll, t_painter.colors.accent_ink, t_paint);
		t_target->PopAxisAlignedClip();
	}
}

[[nodiscard]] auto password_submit_rect(const D2D1_RECT_F& t_field) -> D2D1_RECT_F
{
	const float middle = (t_field.top + t_field.bottom) * 0.5f;
	const float inset  = (t_field.bottom - t_field.top - K_FIELD_SUBMIT_SIZE) * 0.5f;

	return D2D1::RectF(t_field.right - inset - K_FIELD_SUBMIT_SIZE, middle - K_FIELD_SUBMIT_SIZE * 0.5f, t_field.right - inset, middle + K_FIELD_SUBMIT_SIZE * 0.5f);
}

[[nodiscard]] auto password_field_rect(const D2D1_RECT_F& t_row) -> D2D1_RECT_F
{
	return D2D1::RectF(t_row.left, t_row.top + K_FIELD_INSET_Y, t_row.right, t_row.bottom - K_FIELD_INSET_Y);
}

[[nodiscard]] auto password_text_box(const D2D1_RECT_F& t_row, float t_shift) -> D2D1_RECT_F
{
	const D2D1_RECT_F frame  = password_field_rect(D2D1::RectF(t_row.left + t_shift, t_row.top, t_row.right + t_shift, t_row.bottom));
	const D2D1_RECT_F submit = password_submit_rect(frame);
	const float       middle = (frame.top + frame.bottom) * 0.5f;
	const float       left   = frame.left + K_FIELD_PADDING + K_GLYPH_SIZE + K_ICON_GAP;

	return D2D1::RectF(left, middle - K_TEXT_LINE_HEIGHT * 0.5f, submit.left - K_ICON_GAP, middle + K_TEXT_LINE_HEIGHT * 0.5f);
}

auto paint_password_field(ID2D1RenderTarget* t_target, const Painter& t_painter, ID2D1SolidColorBrush* t_brush, const D2D1_RECT_F& t_row) -> void
{
	const os::TrayColors& colors = t_painter.colors;
	const PasswordField&  field  = t_painter.password;
	const float           scale  = t_painter.scale;

	const auto paint = [&](Color t_color, float t_opacity = 1.0f) {
		t_brush->SetColor(to_d2d(t_color, t_opacity));
		return t_brush;
	};

	if (field.editor == nullptr) return;

	const D2D1_RECT_F       frame  = password_field_rect(D2D1::RectF(t_row.left + field.shift, t_row.top, t_row.right + field.shift, t_row.bottom));
	const D2D1_ROUNDED_RECT shape  = {frame, K_FIELD_RADIUS, K_FIELD_RADIUS};
	const float             middle = (frame.top + frame.bottom) * 0.5f;
	const D2D1_RECT_F       submit = password_submit_rect(frame);
	const D2D1_RECT_F       lock   = centered_box(frame.left + K_FIELD_PADDING, middle, K_GLYPH_SIZE, scale);
	const D2D1_RECT_F       text   = password_text_box(t_row, field.shift);
	const bool              typed  = !field.editor->value().empty();

	const TextFieldLook look{
		.editor            = *field.editor,
		.text              = text,
		.scroll            = field.scroll,
		.focused           = true,
		.placeholder       = field.wrong ? K_WRONG_PASSWORD_LABEL : K_PASSWORD_PLACEHOLDER,
		.placeholder_color = field.wrong ? colors.error : colors.text_disabled,
	};

	t_target->FillRoundedRectangle(shape, paint(colors.text, K_FIELD_FILL_ALPHA));
	t_target->DrawRoundedRectangle(shape, paint(field.wrong ? colors.error : colors.accent, K_FIELD_BORDER_ALPHA), 1.0f);
	draw_glyph(t_target, t_painter, Glyph::LOCK, lock, paint(colors.text_dim));
	paint_text_field(t_target, t_painter, look, paint);

	const D2D1_POINT_2F center = D2D1::Point2F((submit.left + submit.right) * 0.5f, middle);

	if (field.unlocking) {
		D2D1::Matrix3x2F upright;
		t_target->GetTransform(&upright);

		t_target->SetTransform(D2D1::Matrix3x2F::Rotation(field.spin * 360.0f, center) * upright);
		draw_glyph(t_target, t_painter, Glyph::REFRESH, centered_box(center.x - K_GLYPH_SIZE * 0.5f, middle, K_GLYPH_SIZE, scale), paint(colors.accent));
		t_target->SetTransform(upright);
		return;
	}

	const CaretLook caret{.editor = field.editor, .motion = field.caret, .text = text, .scroll = field.scroll, .blinked = field.blinked};
	paint_caret(t_target, t_painter, caret, paint);

	const D2D1_ROUNDED_RECT button{submit, K_FIELD_SUBMIT_RADIUS, K_FIELD_SUBMIT_RADIUS};
	t_target->FillRoundedRectangle(button, paint(colors.accent, typed ? 1.0f : K_FIELD_IDLE_SUBMIT));
	draw_glyph(t_target, t_painter, Glyph::CHEVRON, centered_box(center.x - K_CHEVRON_SIZE * 0.5f, middle, K_CHEVRON_SIZE, scale), paint(colors.accent_ink));
}

struct AccountRowParts {
	D2D1_RECT_F star;
	float       chip_left;
	float       chip_right;
	float       text_right;
};

[[nodiscard]] auto account_row_parts(const Row& t_row, float t_row_right, float t_scale) -> AccountRowParts
{
	const float       middle     = t_row.top + t_row.height * 0.5f;
	const D2D1_RECT_F star       = centered_box(t_row_right - K_ROW_BUTTON_SIZE, middle, K_ROW_BUTTON_SIZE, t_scale);
	const float       chip_right = star.left - K_CHIP_GAP;
	const float       chip_left  = chip_right - t_row.detail_width;
	const float       text_right = (t_row.detail.empty() ? star.left : chip_left) - K_CHIP_GAP;

	return AccountRowParts{.star = star, .chip_left = chip_left, .chip_right = chip_right, .text_right = text_right};
}

[[nodiscard]] auto account_part_at(const AccountRowParts& t_parts, float t_x, float t_y) -> RowPart
{
	return contains(t_parts.star, t_x, t_y) ? RowPart::STAR : RowPart::BODY;
}

[[nodiscard]] auto row_box(const Panel& t_panel, const Row& t_row) -> D2D1_RECT_F
{
	const float width = t_panel.frame.right - t_panel.frame.left;
	const float top   = t_row.top + t_row.offset;

	return D2D1::RectF(K_PANEL_PADDING, top, width - K_PANEL_PADDING - t_panel.gutter, top + t_row.height);
}

auto paint_row_button(ID2D1RenderTarget* t_target, const D2D1_RECT_F& t_box, bool t_lit, float t_hover, const auto& t_paint, const os::TrayColors& t_colors) -> void
{
	if (!t_lit) return;

	const D2D1_POINT_2F center = D2D1::Point2F((t_box.left + t_box.right) * 0.5f, (t_box.top + t_box.bottom) * 0.5f);
	const float         radius = (t_box.right - t_box.left) * 0.5f;

	t_target->FillEllipse(D2D1::Ellipse(center, radius, radius), t_paint(t_colors.text, K_ROW_BUTTON_HOVER * t_hover));
}

auto paint_account_details(ID2D1RenderTarget* t_target, const Painter& t_painter, const Row& t_row, const D2D1_RECT_F& t_line, const auto& t_paint) -> void
{
	const os::TrayColors& colors  = t_painter.colors;
	IDWriteTextFormat*    caption = t_painter.formats->caption.Get();
	const float           room    = t_line.right - t_line.left;

	if (t_row.note.empty()) {
		draw_label(t_target, caption, t_row.when, t_line.left, t_line.right, t_line, t_paint(colors.text_disabled));
		return;
	}

	const float when_width = t_row.when.empty() ? 0.0f : text_width(t_painter.writer, caption, t_row.when) + K_DETAIL_DOT_GAP * 2.0f + K_DETAIL_DOT_SIZE;
	const float note_width = std::min(text_width(t_painter.writer, caption, t_row.note), std::max(0.0f, room - when_width));

	draw_label(t_target, caption, t_row.note, t_line.left, t_line.left + note_width, t_line, t_paint(colors.text_dim));

	if (t_row.when.empty()) return;

	const float         dot_x   = t_line.left + note_width + K_DETAIL_DOT_GAP;
	const float         radius  = K_DETAIL_DOT_SIZE * 0.5f;
	const D2D1_POINT_2F dot     = D2D1::Point2F(dot_x + radius, (t_line.top + t_line.bottom) * 0.5f);
	const float         when_at = dot_x + K_DETAIL_DOT_SIZE + K_DETAIL_DOT_GAP;

	t_target->FillEllipse(D2D1::Ellipse(dot, radius, radius), t_paint(colors.text_disabled));
	draw_label(t_target, caption, t_row.when, when_at, t_line.right, t_line, t_paint(colors.text_disabled));
}

auto paint_account_row(ID2D1RenderTarget* t_target, const Painter& t_painter, const Row& t_row, const D2D1_RECT_F& t_box, std::optional<RowPart> t_hot_part, const auto& t_paint) -> void
{
	const os::TrayColors& colors      = t_painter.colors;
	const TextFormats&    formats     = *t_painter.formats;
	const float           scale       = t_painter.scale;
	const float           hover       = t_row.hover;
	const float           middle      = (t_box.top + t_box.bottom) * 0.5f;
	const AccountRowParts parts       = account_row_parts(t_row, t_box.right - K_ROW_INSET, scale);
	const float           text_left   = t_box.left + K_ACCOUNT_TEXT_INSET;
	const bool            has_details = !t_row.note.empty() || !t_row.when.empty();
	const float           block       = has_details ? K_NAME_LINE_HEIGHT + K_ACCOUNT_LINE_GAP + K_DETAIL_LINE_HEIGHT : K_NAME_LINE_HEIGHT;
	const float           name_top    = middle - block * 0.5f;
	const D2D1_RECT_F     name_line   = D2D1::RectF(text_left, name_top, parts.text_right, name_top + K_NAME_LINE_HEIGHT);
	const float           detail_top  = name_line.bottom + K_ACCOUNT_LINE_GAP;
	const D2D1_RECT_F     detail_line = D2D1::RectF(text_left, detail_top, parts.text_right, detail_top + K_DETAIL_LINE_HEIGHT);
	const D2D1_RECT_F     star_glyph  = centered_box(parts.star.left + (K_ROW_BUTTON_SIZE - K_ROW_BUTTON_GLYPH_SIZE) * 0.5f, middle, K_ROW_BUTTON_GLYPH_SIZE, scale);
	const bool            star_lit    = t_hot_part == RowPart::STAR;

	if (hover > 0.0f) {
		const D2D1_ROUNDED_RECT pill{D2D1::RectF(t_box.left, t_box.top + 1.0f, t_box.right, t_box.bottom - 1.0f), K_ROW_RADIUS, K_ROW_RADIUS};
		t_target->FillRoundedRectangle(pill, t_paint(colors.text, K_ACCOUNT_HOVER_ALPHA * hover));
	}

	draw_label(t_target, formats.strong.Get(), t_row.label, name_line.left, name_line.right, name_line, t_paint(colors.text));

	if (has_details) {
		paint_account_details(t_target, t_painter, t_row, detail_line, t_paint);
	}

	if (!t_row.detail.empty()) {
		const float             top = snapped(middle - K_CHIP_HEIGHT * 0.5f, scale);
		const D2D1_ROUNDED_RECT chip{D2D1::RectF(parts.chip_left, top, parts.chip_right, top + K_CHIP_HEIGHT), K_CHIP_HEIGHT * 0.5f, K_CHIP_HEIGHT * 0.5f};

		t_target->FillRoundedRectangle(chip, t_paint(colors.text, K_CHIP_ALPHA));
		draw_label(t_target, formats.chip.Get(), t_row.detail, chip.rect.left, chip.rect.right, chip.rect, t_paint(colors.text_dim));
	}

	paint_row_button(t_target, parts.star, star_lit, hover, t_paint, colors);

	if (t_row.favorite) {
		draw_star(t_target, t_painter, star_glyph, true, t_paint(colors.accent));
	} else if (hover > 0.0f) {
		draw_star(t_target, t_painter, star_glyph, false, t_paint(star_lit ? colors.text : colors.text_disabled, hover));
	}
}

[[nodiscard]] auto is_raised(const Row& t_row, std::optional<RaisedRow> t_raised) -> bool
{
	return t_raised && t_row.kind == RowKind::ACCOUNT && t_row.id == t_raised->id;
}

auto paint_raised_row(ID2D1RenderTarget* t_target, const Painter& t_painter, ID2D1SolidColorBrush* t_brush, const Panel& t_panel, RaisedRow t_raised, const auto& t_paint) -> void
{
	const auto row = std::ranges::find_if(t_panel.rows, [t_raised](const Row& t_row) { return is_raised(t_row, t_raised); });
	if (row == t_panel.rows.end()) return;

	Row shown    = *row;
	shown.top    = t_raised.top;
	shown.offset = 0.0f;
	shown.hover  = 0.0f;

	const D2D1_RECT_F       box  = row_box(t_panel, shown);
	const float             half = 0.5f / t_painter.scale;
	const D2D1_ROUNDED_RECT body{box, K_ROW_RADIUS, K_ROW_RADIUS};
	const D2D1_ROUNDED_RECT edge{D2D1::RectF(box.left + half, box.top + half, box.right - half, box.bottom - half), K_ROW_RADIUS, K_ROW_RADIUS};

	if (t_raised.lift > 0.0f) {
		draw_soft_shadow(t_target, t_brush, box, K_ROW_RADIUS, t_raised.lift);
		t_target->FillRoundedRectangle(body, t_paint(t_painter.colors.background, t_raised.lift));
		t_target->DrawRoundedRectangle(edge, t_paint(t_painter.colors.border, t_raised.lift), 1.0f / t_painter.scale);
	}

	paint_account_row(t_target, t_painter, shown, box, std::nullopt, t_paint);
}

auto paint_panel(ID2D1RenderTarget* t_target, const Painter& t_painter, ID2D1SolidColorBrush* t_brush, const Panel& t_panel, const MenuImages& t_images, const UpdateBadge* t_badge, std::optional<RaisedRow> t_raised, const D2D1::Matrix3x2F& t_base) -> void
{
	const os::TrayColors& colors  = t_painter.colors;
	const TextFormats&    formats = *t_painter.formats;
	const float           scale   = t_painter.scale;
	const float           width   = t_panel.frame.right - t_panel.frame.left;

	const auto paint = [&](Color t_color, float t_opacity = 1.0f) {
		t_brush->SetColor(to_d2d(t_color, t_opacity));
		return t_brush;
	};

	const bool        scrolls = t_panel.gutter > 0.0f;
	const D2D1_RECT_F rows_window{t_panel.frame.left, t_panel.frame.top + K_PANEL_PADDING, t_panel.frame.right, t_panel.frame.bottom - K_PANEL_PADDING};

	t_target->SetTransform(t_base);
	t_target->PushAxisAlignedClip(scrolls ? rows_window : t_panel.frame, D2D1_ANTIALIAS_MODE_ALIASED);

	const D2D1::Matrix3x2F rows_origin = D2D1::Matrix3x2F::Translation(t_panel.frame.left, t_panel.frame.top - t_panel.scroll.offset);
	t_target->SetTransform(rows_origin * t_base);

	for (const usize index : std::views::iota(usize{0}, t_panel.rows.size())) {
		const Row& row = t_panel.rows[index];
		if (is_raised(row, t_raised)) continue;

		const D2D1_RECT_F box    = row_box(t_panel, row);
		const float       left   = box.left + K_ROW_INSET;
		const float       right  = box.right - K_ROW_INSET;
		const float       middle = (box.top + box.bottom) * 0.5f;
		const Color       ink    = mix(colors.text_dim, colors.text, row.hover);
		const bool        chosen = t_panel.selected == index;

		if ((row.hover > 0.0f || chosen) && is_selectable(row) && row.kind != RowKind::ACCOUNT) {
			const D2D1_ROUNDED_RECT pill{D2D1::RectF(box.left, box.top + 1.0f, box.right, box.bottom - 1.0f), K_ROW_RADIUS, K_ROW_RADIUS};
			const Color             tint = row.destructive ? colors.error : colors.accent;
			const float             fill = std::max(K_HOVER_ALPHA * row.hover, chosen ? K_SELECTED_GAME_ALPHA : 0.0f);
			t_target->FillRoundedRectangle(pill, paint(tint, fill));
		}

		switch (row.kind) {
			using enum RowKind;

			case BRAND: {
				if (t_images.logo) {
					const float logo_x = left + (K_ICON_SIZE - K_LOGO_SIZE) * 0.5f;
					draw_bitmap(t_target, t_images.logo.Get(), logo_x, middle, scale);
				}

				draw_label(t_target, formats.strong.Get(), row.label, row.label_x, right, box, paint(colors.text));

				if (t_badge != nullptr) {
					paint_update(t_target, t_painter, t_brush, *t_badge, update_rect(row, width, badge_width(t_painter, t_badge->state), scale));
				}

				break;
			}

			case GAME: {
				const float chevron = right - K_CHEVRON_SIZE;
				const float count   = chevron - K_COUNT_GAP - row.detail_width;

				if (row.icon && *row.icon < os::K_TRAY_MAX_GAMES && t_images.game_icons[*row.icon]) {
					draw_bitmap(t_target, t_images.game_icons[*row.icon].Get(), left, middle, scale);
				} else {
					const D2D1_RECT_F blank = centered_box(left, middle, K_ICON_SIZE, scale);
					t_target->FillRoundedRectangle(D2D1_ROUNDED_RECT{blank, K_ICON_RADIUS, K_ICON_RADIUS}, paint(colors.text_disabled, 0.4f));
				}

				const float label_end     = (row.detail.empty() ? chevron : count) - K_ICON_GAP;
				const Color chevron_color = chosen ? colors.accent : ink;

				draw_label(t_target, formats.body.Get(), row.label, row.label_x, label_end, box, paint(colors.text));
				draw_label(t_target, formats.count.Get(), row.detail, count, chevron - K_COUNT_GAP, box, paint(colors.text_disabled));
				draw_turned_glyph(t_target, t_painter, Glyph::CHEVRON, centered_box(chevron, middle, K_CHEVRON_SIZE, scale), t_panel.chevron_degrees + K_CHEVRON_FLIP_DEGREES * smoothed(row.turn), paint(chevron_color));
				break;
			}

			case ACCOUNT: {
				const std::optional<RowPart> hot_part = t_panel.hot == index ? t_panel.hot_part : std::nullopt;
				paint_account_row(t_target, t_painter, row, box, hot_part, paint);

				const bool last_account = index + 1 == t_panel.rows.size() || t_panel.rows[index + 1].kind != ACCOUNT;
				if (!last_account) {
					const float y = snapped(box.bottom, scale) - 1.0f / scale;
					t_target->FillRectangle(D2D1::RectF(box.left + K_ACCOUNT_SEPARATOR_INSET, y, box.right - K_ACCOUNT_SEPARATOR_INSET, y + 1.0f / scale), paint(colors.separator));
				}

				break;
			}

			// Quit is red from the start and turns fully red under the pointer, so it never reads like the harmless actions above it.
			case ACTION: {
				const D2D1_RECT_F glyph = centered_box(left + (K_ICON_SIZE - K_GLYPH_SIZE) * 0.5f, middle, K_GLYPH_SIZE, scale);
				const Color       rest  = mix(colors.text_dim, colors.error, K_QUIT_REST_RED);
				const Color       icon  = row.destructive ? mix(rest, colors.error, row.hover) : ink;
				const Color       label = row.destructive ? mix(colors.text, colors.error, row.hover) : colors.text;

				draw_glyph(t_target, t_painter, row.glyph, glyph, paint(icon));
				draw_label(t_target, formats.body.Get(), row.label, row.label_x, right, box, paint(label));
				break;
			}

			case OPTION: {
				draw_label(t_target, formats.body.Get(), row.label, left, right, box, paint(colors.text));
				break;
			}

			case NOTE: {
				if (row.glyph != Glyph::NONE) {
					const D2D1_RECT_F glyph = centered_box(left + (K_ICON_SIZE - K_GLYPH_SIZE) * 0.5f, middle, K_GLYPH_SIZE, scale);
					draw_glyph(t_target, t_painter, row.glyph, glyph, paint(colors.text_disabled));
				}

				draw_label(t_target, formats.body.Get(), row.label, row.label_x, right, box, paint(colors.text_disabled));
				break;
			}

			case PASSWORD: {
				paint_password_field(t_target, t_painter, t_brush, box);
				break;
			}

			case SEPARATOR: {
				const float       y = snapped(middle, scale);
				const D2D1_RECT_F line{box.left + K_SEPARATOR_INSET, y, box.right - K_SEPARATOR_INSET, y + 1.0f / scale};
				t_target->FillRectangle(line, paint(colors.separator));
				break;
			}
		}
	}

	if (t_raised) {
		paint_raised_row(t_target, t_painter, t_brush, t_panel, *t_raised, paint);
	}

	t_target->SetTransform(t_base);
	t_target->PopAxisAlignedClip();

	if (scrolls) {
		t_target->SetTransform(D2D1::Matrix3x2F::Translation(t_panel.frame.left, t_panel.frame.top) * t_base);
		paint_edge_shadows(t_target, t_painter, t_panel);
		paint_scrollbar(t_target, t_painter, t_brush, t_panel);
	}

	t_target->SetTransform(t_base);
}

[[nodiscard]] auto add_button_rect(const D2D1_RECT_F& t_header, float t_label_width, float t_scale) -> D2D1_RECT_F
{
	const float width = K_ADD_BUTTON_PADDING * 2.0f + K_ADD_BUTTON_GLYPH_SIZE + K_ADD_BUTTON_GAP + t_label_width;
	const float right = t_header.right - K_SIDE_PADDING;
	const float top   = snapped((t_header.top + t_header.bottom - K_ADD_BUTTON_HEIGHT) * 0.5f, t_scale);

	return D2D1::RectF(right - width, top, right, top + K_ADD_BUTTON_HEIGHT);
}

struct SideHeader {
	D2D1_RECT_F       box;
	ID2D1Bitmap*      icon;
	std::wstring_view title;
	std::wstring_view subtitle;
	float             add_label_width;
	bool              add_hot;
};

auto paint_side_header(ID2D1RenderTarget* t_target, const Painter& t_painter, const SideHeader& t_header, const auto& t_paint) -> void
{
	const os::TrayColors& colors    = t_painter.colors;
	const TextFormats&    formats   = *t_painter.formats;
	const float           scale     = t_painter.scale;
	const D2D1_RECT_F     box       = t_header.box;
	const D2D1_RECT_F     add       = add_button_rect(box, t_header.add_label_width, scale);
	const float           left      = box.left + K_SIDE_PADDING;
	const float           middle    = (box.top + box.bottom) * 0.5f;
	const float           text_left = left + K_ICON_SIZE + K_ICON_GAP;
	const float           text_end  = add.left - K_ICON_GAP;
	const D2D1_RECT_F     title     = D2D1::RectF(text_left, middle - K_NAME_LINE_HEIGHT, text_end, middle);
	const D2D1_RECT_F     subtitle  = D2D1::RectF(text_left, middle, text_end, middle + K_DETAIL_LINE_HEIGHT);
	const float           line_y    = snapped(box.bottom, scale) - 1.0f / scale;
	const float           half      = 0.5f / scale;
	const D2D1_RECT_F     outline   = D2D1::RectF(add.left + half, add.top + half, add.right - half, add.bottom - half);
	const D2D1_RECT_F     plus      = centered_box(add.left + K_ADD_BUTTON_PADDING, middle, K_ADD_BUTTON_GLYPH_SIZE, scale);
	const Color           ink       = t_header.add_hot ? colors.text : colors.text_dim;

	if (t_header.icon != nullptr) {
		draw_bitmap(t_target, t_header.icon, left, middle, scale);
	}

	draw_label(t_target, formats.strong.Get(), t_header.title, title.left, title.right, title, t_paint(colors.text));
	draw_label(t_target, formats.caption.Get(), t_header.subtitle, subtitle.left, subtitle.right, subtitle, t_paint(colors.text_disabled));
	t_target->FillRectangle(D2D1::RectF(left, line_y, box.right - K_SIDE_PADDING, line_y + 1.0f / scale), t_paint(colors.separator));

	if (t_header.add_hot) {
		t_target->FillRoundedRectangle(D2D1_ROUNDED_RECT{add, K_FORM_BUTTON_RADIUS, K_FORM_BUTTON_RADIUS}, t_paint(colors.text, K_ADD_BUTTON_HOVER_ALPHA));
	}

	t_target->DrawRoundedRectangle(D2D1_ROUNDED_RECT{outline, K_FORM_BUTTON_RADIUS, K_FORM_BUTTON_RADIUS}, t_paint(colors.border), 1.0f / scale);
	draw_glyph(t_target, t_painter, Glyph::ADD, plus, t_paint(ink));
	draw_label(t_target, formats.caption.Get(), K_ADD_LABEL, plus.right + K_ADD_BUTTON_GAP, add.right, add, t_paint(ink));
}

struct FormView {
	const AccountFormState&       state;
	const FormLayout&             layout;
	std::optional<FormHit>        hot;
	std::span<const os::TrayGame> games;
	std::span<const std::wstring> game_labels;
	const MenuImages&             images;
	CaretLook                     caret;
};

[[nodiscard]] auto region_text(usize t_region) -> std::wstring
{
	const RegionOption& option = K_REGION_OPTIONS[t_region];

	return os::win32::to_wide(option.code.empty() ? option.label : option.code);
}

[[nodiscard]] auto is_hot(const FormView& t_view, FormTarget t_target) -> bool
{
	return t_view.hot && t_view.hot->target == t_target;
}

[[nodiscard]] auto field_text_box(const FormLayout& t_layout, FormField t_field) -> D2D1_RECT_F
{
	const D2D1_RECT_F box    = t_layout.fields[t_field];
	const float       right  = t_field == FormField::PASSWORD ? t_layout.reveal.left : box.right - K_FORM_FIELD_PADDING;
	const float       middle = (box.top + box.bottom) * 0.5f;

	return D2D1::RectF(box.left + K_FORM_FIELD_PADDING, middle - K_TEXT_LINE_HEIGHT * 0.5f, right, middle + K_TEXT_LINE_HEIGHT * 0.5f);
}

[[nodiscard]] auto keycap_width(const Painter& t_painter, std::wstring_view t_key) -> float
{
	return std::ceil(text_width(t_painter.writer, t_painter.formats->keycap.Get(), t_key) + K_KEYCAP_PADDING * 2.0f);
}

auto paint_form_label(ID2D1RenderTarget* t_target, const Painter& t_painter, const D2D1_RECT_F& t_box, std::wstring_view t_label, bool t_optional, Color t_color, const auto& t_paint) -> void
{
	IDWriteTextFormat* caption = t_painter.formats->caption.Get();

	draw_label(t_target, caption, t_label, t_box.left, t_box.right, t_box, t_paint(t_color));

	if (t_optional) {
		const float suffix_left = t_box.left + text_width(t_painter.writer, caption, t_label);
		draw_label(t_target, caption, K_OPTIONAL_SUFFIX, suffix_left, t_box.right, t_box, t_paint(t_painter.colors.text_disabled));
	}
}

auto paint_form_box(ID2D1RenderTarget* t_target, const Painter& t_painter, const D2D1_RECT_F& t_box, Color t_fill, float t_fill_alpha, Color t_border, const auto& t_paint) -> void
{
	const float             half = 0.5f / t_painter.scale;
	const D2D1_ROUNDED_RECT fill{t_box, K_FORM_FIELD_RADIUS, K_FORM_FIELD_RADIUS};
	const D2D1_ROUNDED_RECT edge{D2D1::RectF(t_box.left + half, t_box.top + half, t_box.right - half, t_box.bottom - half), K_FORM_FIELD_RADIUS, K_FORM_FIELD_RADIUS};

	t_target->FillRoundedRectangle(fill, t_paint(t_fill, t_fill_alpha));
	t_target->DrawRoundedRectangle(edge, t_paint(t_border), 1.0f / t_painter.scale);
}

auto paint_form_text(ID2D1RenderTarget* t_target, const Painter& t_painter, const FormView& t_view, FormField t_field, const auto& t_paint) -> void
{
	const os::TrayColors& colors   = t_painter.colors;
	const D2D1_RECT_F     box      = t_view.layout.fields[t_field];
	const TextEditor&     editor   = t_view.state.fields[t_field];
	const bool            focused  = t_view.state.focus == t_field;
	const bool            missing  = t_view.state.show_required && t_field != FormField::NOTE && editor.value().empty();
	const bool            hovered  = is_hot(t_view, FormTarget::FIELD) && t_view.hot->field == t_field;
	const Color           resting  = hovered ? colors.border : colors.separator;
	const Color           focus_to = focused ? colors.accent : resting;

	const TextFieldLook look{
		.editor            = editor,
		.text              = field_text_box(t_view.layout, t_field),
		.scroll            = t_view.state.scroll[t_field],
		.focused           = focused,
		.placeholder       = K_FORM_PLACEHOLDERS[t_field],
		.placeholder_color = colors.text_disabled,
	};

	paint_form_box(t_target, t_painter, box, colors.text, K_FORM_FIELD_FILL, missing ? colors.error : focus_to, t_paint);
	paint_text_field(t_target, t_painter, look, t_paint);
}

auto paint_form_button(ID2D1RenderTarget* t_target, const Painter& t_painter, const D2D1_RECT_F& t_box, std::wstring_view t_label, Color t_fill, float t_fill_alpha, Color t_ink, const auto& t_paint) -> void
{
	const D2D1_ROUNDED_RECT button{t_box, K_FORM_BUTTON_RADIUS, K_FORM_BUTTON_RADIUS};

	t_target->FillRoundedRectangle(button, t_paint(t_fill, t_fill_alpha));
	draw_label(t_target, t_painter.formats->button.Get(), t_label, t_box.left, t_box.right, t_box, t_paint(t_ink));
}

auto paint_keycap(ID2D1RenderTarget* t_target, const Painter& t_painter, const D2D1_RECT_F& t_cap, std::wstring_view t_key, Color t_backdrop, float t_alpha, const auto& t_paint) -> void
{
	const os::TrayColors&   colors = t_painter.colors;
	const D2D1_ROUNDED_RECT frame{t_cap, K_KEYCAP_RADIUS, K_KEYCAP_RADIUS};
	const D2D1_RECT_F       inner = D2D1::RectF(t_cap.left + 1.0f, t_cap.top + 1.0f, t_cap.right - 1.0f, t_cap.bottom - 1.0f - K_KEYCAP_LIP);
	const D2D1_ROUNDED_RECT face{inner, K_KEYCAP_RADIUS - 1.0f, K_KEYCAP_RADIUS - 1.0f};
	const D2D1_RECT_F       label = D2D1::RectF(t_cap.left, t_cap.top, t_cap.right, t_cap.bottom - K_KEYCAP_LIP);

	t_target->FillRoundedRectangle(frame, t_paint(colors.border, t_alpha));
	t_target->FillRoundedRectangle(face, t_paint(t_backdrop, t_alpha));
	draw_label(t_target, t_painter.formats->keycap.Get(), t_key, label.left, label.right, label, t_paint(mix(colors.text_disabled, colors.text_dim, K_KEYCAP_INK_MIX), t_alpha));
}

[[nodiscard]] auto popup_height(std::span<const MenuItem> t_items) -> float
{
	const auto separators = static_cast<float>(std::ranges::count_if(t_items.subspan(1), &MenuItem::separated));

	return K_POPUP_PADDING * 2.0f + K_POPUP_ITEM_HEIGHT * static_cast<float>(t_items.size()) + K_POPUP_SEPARATOR_SPACE * separators;
}

[[nodiscard]] auto popup_row(const PopupMenu& t_menu, usize t_index) -> D2D1_RECT_F
{
	const auto  separators = static_cast<float>(std::ranges::count_if(t_menu.items.subspan(1, t_index), &MenuItem::separated));
	const float top        = t_menu.box.top + K_POPUP_PADDING + static_cast<float>(t_index) * K_POPUP_ITEM_HEIGHT + separators * K_POPUP_SEPARATOR_SPACE;

	return D2D1::RectF(t_menu.box.left + K_POPUP_PADDING, top, t_menu.box.right - K_POPUP_PADDING, top + K_POPUP_ITEM_HEIGHT);
}

[[nodiscard]] auto popup_label_inset(std::span<const MenuItem> t_items) -> float
{
	const bool has_icons = std::ranges::any_of(t_items, [](const MenuItem& t_item) { return t_item.glyph != Glyph::NONE; });

	return K_POPUP_LABEL_INSET + (has_icons ? K_POPUP_ICON_SIZE + K_POPUP_ICON_GAP : 0.0f);
}

auto paint_popup_item(ID2D1RenderTarget* t_target, const Painter& t_painter, const PopupMenu& t_menu, usize t_index, const auto& t_paint) -> void
{
	const os::TrayColors&   colors   = t_painter.colors;
	const MenuItem&         item     = t_menu.items[t_index];
	const D2D1_RECT_F       row      = popup_row(t_menu, t_index);
	const bool              enabled  = t_menu.enabled[t_index];
	const bool              hovered  = enabled && t_menu.hot == t_index;
	const bool              armed    = t_menu.armed == t_index;
	const float             middle   = (row.top + row.bottom) * 0.5f;
	const float             label_x  = row.left + popup_label_inset(t_menu.items);
	const std::wstring_view label    = armed ? K_CONFIRM_LABEL : item.label;
	const Color             backdrop = hovered ? mix(colors.background, colors.text, K_POPUP_HOVER_ALPHA) : colors.background;

	Color ink = enabled ? colors.text : colors.text_disabled;
	if (item.destructive && enabled) {
		ink = colors.error;
	}

	if (item.separated && t_index > 0) {
		const float y = snapped(row.top - K_POPUP_SEPARATOR_SPACE * 0.5f, t_painter.scale);
		t_target->FillRectangle(D2D1::RectF(row.left + K_POPUP_LABEL_INSET, y, row.right - K_POPUP_LABEL_INSET, y + 1.0f / t_painter.scale), t_paint(colors.separator));
	}

	if (hovered || armed) {
		const D2D1_ROUNDED_RECT pill{row, K_POPUP_ITEM_RADIUS, K_POPUP_ITEM_RADIUS};
		t_target->FillRoundedRectangle(pill, item.destructive ? t_paint(colors.error, K_POPUP_DANGER_ALPHA) : t_paint(colors.text, K_POPUP_HOVER_ALPHA));
	}

	if (item.glyph != Glyph::NONE) {
		const Color icon = item.destructive ? ink : (enabled ? colors.text_dim : colors.text_disabled);
		draw_glyph(t_target, t_painter, item.glyph, centered_box(row.left + K_POPUP_LABEL_INSET, middle, K_POPUP_ICON_SIZE, t_painter.scale), t_paint(icon));
	}

	draw_label(t_target, t_painter.formats->body.Get(), label, label_x, row.right, row, t_paint(ink));

	if (armed) {
		const float left  = std::clamp(t_menu.armed_seconds / K_CONFIRM_SECONDS, 0.0f, 1.0f);
		const float width = text_width(t_painter.writer, t_painter.formats->body.Get(), label) * left;
		const float top   = middle + K_CONFIRM_BAR_DROP;
		t_target->FillRoundedRectangle(D2D1_ROUNDED_RECT{D2D1::RectF(label_x, top, label_x + width, top + K_CONFIRM_BAR_HEIGHT), K_CONFIRM_BAR_HEIGHT * 0.5f, K_CONFIRM_BAR_HEIGHT * 0.5f}, t_paint(colors.error));
	}

	if (item.key.empty()) return;

	const float alpha     = enabled ? 1.0f : K_DISABLED_KEYCAP_ALPHA;
	const float cap_top   = snapped(middle - K_KEYCAP_HEIGHT * 0.5f, t_painter.scale);
	const float key_left  = row.right - K_POPUP_LABEL_INSET - keycap_width(t_painter, item.key);
	const float ctrl_left = key_left - K_KEYCAP_GAP - keycap_width(t_painter, K_SHORTCUT_KEY);

	paint_keycap(t_target, t_painter, D2D1::RectF(ctrl_left, cap_top, key_left - K_KEYCAP_GAP, cap_top + K_KEYCAP_HEIGHT), K_SHORTCUT_KEY, backdrop, alpha, t_paint);
	paint_keycap(t_target, t_painter, D2D1::RectF(key_left, cap_top, row.right - K_POPUP_LABEL_INSET, cap_top + K_KEYCAP_HEIGHT), item.key, backdrop, alpha, t_paint);
}

auto paint_popup(ID2D1RenderTarget* t_target, const Painter& t_painter, ID2D1SolidColorBrush* t_brush, const PopupMenu& t_menu, const auto& t_paint) -> void
{
	draw_soft_shadow(t_target, t_brush, t_menu.box, K_POPUP_RADIUS);
	paint_body(t_target, t_brush, t_painter.colors, t_menu.box, K_POPUP_RADIUS, t_painter.colors.border, t_painter.scale);

	for (const usize index : std::views::iota(usize{0}, t_menu.items.size())) {
		paint_popup_item(t_target, t_painter, t_menu, index, t_paint);
	}
}

auto paint_region_list(ID2D1RenderTarget* t_target, const Painter& t_painter, ID2D1SolidColorBrush* t_brush, const FormView& t_view) -> void
{
	const Panel& list = t_view.state.regions;

	D2D1::Matrix3x2F base;
	t_target->GetTransform(&base);

	draw_soft_shadow(t_target, t_brush, list.frame, K_FORM_FIELD_RADIUS);
	paint_body(t_target, t_brush, t_painter.colors, list.frame, K_FORM_FIELD_RADIUS, t_painter.colors.border, t_painter.scale);
	paint_panel(t_target, t_painter, t_brush, list, t_view.images, nullptr, std::nullopt, base);
}

auto paint_form(ID2D1RenderTarget* t_target, const Painter& t_painter, ID2D1SolidColorBrush* t_brush, const FormView& t_view, const auto& t_paint) -> void
{
	const os::TrayColors&   colors  = t_painter.colors;
	const TextFormats&      formats = *t_painter.formats;
	const FormLayout&       layout  = t_view.layout;
	const AccountFormState& state   = t_view.state;
	const float             scale   = t_painter.scale;
	const D2D1_RECT_F       back    = centered_box(layout.back.left + (K_FORM_BACK_SIZE - K_CHEVRON_SIZE) * 0.5f, (layout.back.top + layout.back.bottom) * 0.5f, K_CHEVRON_SIZE, scale);
	const WideText          username{state.fields[FormField::USERNAME].value()};
	const std::wstring_view title = state.editing ? username.view() : K_NEW_ACCOUNT_TITLE;

	paint_row_button(t_target, layout.back, is_hot(t_view, FormTarget::BACK), 1.0f, t_paint, colors);
	draw_turned_glyph(t_target, t_painter, Glyph::CHEVRON, back, K_CHEVRON_LEFT_DEGREES, t_paint(colors.text_dim));
	draw_label(t_target, formats.strong.Get(), title, layout.header.left, layout.header.right, layout.header, t_paint(colors.text));
	draw_label(t_target, formats.count.Get(), K_ESCAPE_HINT, layout.header.left, layout.header.right, layout.header, t_paint(colors.text_disabled));

	for (const FormField field : each<FormField>()) {
		const bool missing = state.show_required && field != FormField::NOTE && state.fields[field].value().empty();
		paint_form_label(t_target, t_painter, layout.labels[field], K_FORM_LABELS[field], field == FormField::NOTE, missing ? colors.error : colors.text_dim, t_paint);
		paint_form_text(t_target, t_painter, t_view, field, t_paint);
	}

	const D2D1_RECT_F reveal = centered_box(layout.reveal.left + (K_FORM_REVEAL_SIZE - K_ROW_BUTTON_GLYPH_SIZE) * 0.5f, (layout.reveal.top + layout.reveal.bottom) * 0.5f, K_ROW_BUTTON_GLYPH_SIZE, scale);
	draw_glyph(t_target, t_painter, state.fields[FormField::PASSWORD].m_masked ? Glyph::EYE : Glyph::EYE_OFF, reveal, t_paint(is_hot(t_view, FormTarget::REVEAL) ? colors.text : colors.text_dim));

	const bool        region_lit   = state.regions_open || is_hot(t_view, FormTarget::REGION);
	const Color       region_ink   = state.region == 0 ? colors.text_disabled : colors.text;
	const D2D1_RECT_F region_value = D2D1::RectF(layout.region.left + K_FORM_FIELD_PADDING, layout.region.top, layout.region.right - K_FORM_FIELD_PADDING - K_CHEVRON_SIZE, layout.region.bottom);
	const D2D1_RECT_F region_arrow = centered_box(layout.region.right - K_FORM_FIELD_PADDING - K_CHEVRON_SIZE, (layout.region.top + layout.region.bottom) * 0.5f, K_CHEVRON_SIZE, scale);

	paint_form_label(t_target, t_painter, layout.region_label, K_REGION_LABEL, true, colors.text_dim, t_paint);
	paint_form_box(t_target, t_painter, layout.region, colors.text, K_FORM_FIELD_FILL, state.regions_open ? colors.accent : (region_lit ? colors.border : colors.separator), t_paint);
	draw_label(t_target, formats.body.Get(), region_text(state.region), region_value.left, region_value.right, region_value, t_paint(region_ink));
	draw_turned_glyph(t_target, t_painter, Glyph::CHEVRON, region_arrow, state.regions_open ? K_CHEVRON_UP_DEGREES : K_CHEVRON_DOWN_DEGREES, t_paint(region_lit ? colors.text : colors.text_dim));

	paint_form_label(t_target, t_painter, layout.games_label, K_SHOW_IN_LABEL, false, colors.text_dim, t_paint);

	for (const usize index : std::views::iota(usize{0}, std::min(layout.games.size(), t_view.games.size()))) {
		const os::TrayGame& game     = t_view.games[index];
		const D2D1_RECT_F   chip     = layout.games[index];
		const bool          selected = (state.visible_games & (1u << game.game)) != 0;
		const bool          hovered  = is_hot(t_view, FormTarget::GAME) && t_view.hot->index == index;
		const Color         border   = selected ? colors.accent : (hovered ? colors.border : colors.separator);
		const float         middle   = (chip.top + chip.bottom) * 0.5f;
		const float         text_x   = chip.left + K_FORM_CHIP_PADDING + K_FORM_CHIP_ICON_SIZE + K_FORM_CHIP_ICON_GAP;

		const Color fill      = selected ? colors.accent : colors.text;
		const float fill_tint = selected ? K_FORM_CHIP_TINT : K_FORM_FIELD_FILL;
		const Color edge      = selected ? mix(colors.separator, border, K_FORM_CHIP_BORDER_TINT) : border;

		paint_form_box(t_target, t_painter, chip, fill, fill_tint, edge, t_paint);

		if (game.game < os::K_TRAY_MAX_GAMES && t_view.images.game_icons[game.game]) {
			draw_bitmap(t_target, t_view.images.game_icons[game.game].Get(), chip.left + K_FORM_CHIP_PADDING, middle, scale, selected ? 1.0f : K_FORM_CHIP_FADED_ICON);
		}

		draw_label(t_target, formats.caption.Get(), t_view.game_labels[index], text_x, chip.right, chip, t_paint(selected || hovered ? colors.text : colors.text_dim));
	}

	const float cancel_fill = K_FORM_GHOST_ALPHA + (is_hot(t_view, FormTarget::CANCEL) ? K_FORM_GHOST_HOVER : 0.0f);
	paint_form_button(t_target, t_painter, layout.cancel, K_CANCEL_LABEL, colors.text, cancel_fill, colors.text, t_paint);
	paint_form_button(t_target, t_painter, layout.save, K_SAVE_LABEL, colors.accent, 1.0f, colors.accent_ink, t_paint);

	if (is_hot(t_view, FormTarget::SAVE)) {
		const D2D1_ROUNDED_RECT lift{layout.save, K_FORM_BUTTON_RADIUS, K_FORM_BUTTON_RADIUS};
		t_target->FillRoundedRectangle(lift, t_paint(Color{255, 255, 255, 255}, K_BUTTON_LIFT));
	}

	paint_caret(t_target, t_painter, t_view.caret, t_paint);

	if (state.regions_open) {
		paint_region_list(t_target, t_painter, t_brush, t_view);
	}
}

struct MenuScene {
	const Panel&             games;
	const Panel&             side;
	const MenuImages&        images;
	const UpdateBadge&       badge;
	D2D1_RECT_F              side_area;
	float                    slide;
	bool                     side_left;
	SideView                 view;
	SideHeader               header;
	std::optional<RaisedRow> raised;
	const FormView&          form;
	const PopupMenu*         popup;
};

[[nodiscard]] auto shown_side(const MenuScene& t_scene) -> std::optional<D2D1_RECT_F>
{
	const float       width = K_SIDE_WIDTH * smoothed(t_scene.slide);
	const D2D1_RECT_F area  = t_scene.side_area;
	if (width < K_SETTLED) return std::nullopt;

	return t_scene.side_left ? D2D1::RectF(area.right - width, area.top, area.right, area.bottom) : D2D1::RectF(area.left, area.top, area.left + width, area.bottom);
}

auto paint_menu_body(ID2D1RenderTarget* t_target, const Painter& t_painter, ID2D1SolidColorBrush* t_brush, const MenuScene& t_scene, std::optional<D2D1_RECT_F> t_side) -> void
{
	const D2D1_RECT_F games = t_scene.games.frame;
	const D2D1_RECT_F whole = t_side ? D2D1::RectF(std::min(games.left, t_side->left), games.top, std::max(games.right, t_side->right), games.bottom) : games;

	draw_soft_shadow(t_target, t_brush, whole, K_SURFACE_RADIUS);
	paint_body(t_target, t_brush, t_painter.colors, whole, K_SURFACE_RADIUS, t_painter.colors.border, t_painter.scale);

	if (!t_side) return;

	const float       seam = snapped(t_scene.side_left ? games.left : games.right, t_painter.scale);
	const D2D1_RECT_F line = D2D1::RectF(seam, games.top + 1.0f, seam + 1.0f / t_painter.scale, games.bottom - 1.0f);

	t_brush->SetColor(to_d2d(t_painter.colors.separator));
	t_target->FillRectangle(line, t_brush);
}

auto paint_menu(ID2D1RenderTarget* t_target, const Painter& t_painter, const MenuScene& t_scene) -> void
{
	ComPtr<ID2D1SolidColorBrush> brush;
	if (FAILED(t_target->CreateSolidColorBrush(to_d2d(t_painter.colors.text), &brush))) return;

	const auto paint = [&](Color t_color, float t_opacity = 1.0f) {
		brush->SetColor(to_d2d(t_color, t_opacity));
		return brush.Get();
	};

	const std::optional<D2D1_RECT_F> side = shown_side(t_scene);

	t_target->SetTransform(D2D1::Matrix3x2F::Identity());
	t_target->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
	paint_menu_body(t_target, t_painter, brush.Get(), t_scene, side);
	paint_panel(t_target, t_painter, brush.Get(), t_scene.games, t_scene.images, &t_scene.badge, std::nullopt, D2D1::Matrix3x2F::Identity());

	if (side) {
		const float            hidden = K_SIDE_WIDTH - (side->right - side->left);
		const D2D1::Matrix3x2F slide  = D2D1::Matrix3x2F::Translation(t_scene.side_left ? hidden : -hidden, 0.0f);

		t_target->PushAxisAlignedClip(*side, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
		t_target->SetTransform(slide);

		if (t_scene.view == SideView::ACCOUNTS) {
			paint_side_header(t_target, t_painter, t_scene.header, paint);
			paint_panel(t_target, t_painter, brush.Get(), t_scene.side, t_scene.images, nullptr, t_scene.raised, slide);
		} else {
			paint_form(t_target, t_painter, brush.Get(), t_scene.form, paint);
		}

		t_target->SetTransform(D2D1::Matrix3x2F::Identity());
		t_target->PopAxisAlignedClip();
	}

	if (t_scene.popup != nullptr) {
		paint_popup(t_target, t_painter, brush.Get(), *t_scene.popup, paint);
	}
}

[[nodiscard]] auto middle_of(const D2D1_RECT_F& t_box) -> D2D1_POINT_2F
{
	return D2D1::Point2F((t_box.left + t_box.right) * 0.5f, (t_box.top + t_box.bottom) * 0.5f);
}

auto draw_progress_ring(ID2D1RenderTarget* t_target, const Painter& t_painter, const D2D1_RECT_F& t_box, float t_turns, Color t_color, const auto& t_paint) -> void
{
	const D2D1_POINT_2F center      = middle_of(t_box);
	const float         radius      = (t_box.right - t_box.left) * K_TOAST_RING_RADIUS_SHARE;
	const float         full_turn   = 2.0f * std::numbers::pi_v<float>;
	const float         arc_start   = t_turns * full_turn;
	const float         arc_end     = arc_start + K_TOAST_RING_ARC_TURNS * full_turn;
	const auto          point_at    = [&](float t_angle) { return D2D1::Point2F(center.x + radius * std::cos(t_angle), center.y + radius * std::sin(t_angle)); };
	const D2D1_SIZE_F   ring_radius = D2D1::SizeF(radius, radius);

	t_target->DrawEllipse(D2D1::Ellipse(center, radius, radius), t_paint(t_color, K_TOAST_RING_TRACK_ALPHA), K_TOAST_RING_STROKE);

	ComPtr<ID2D1PathGeometry> arc;
	ComPtr<ID2D1GeometrySink> sink;
	if (FAILED(t_painter.factory->CreatePathGeometry(&arc)) || FAILED(arc->Open(&sink))) return;

	sink->BeginFigure(point_at(arc_start), D2D1_FIGURE_BEGIN_HOLLOW);
	sink->AddArc(D2D1::ArcSegment(point_at(arc_end), ring_radius, 0.0f, D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
	sink->EndFigure(D2D1_FIGURE_END_OPEN);

	if (SUCCEEDED(sink->Close())) {
		t_target->DrawGeometry(arc.Get(), t_paint(t_color), K_TOAST_RING_STROKE, t_painter.stroke);
	}
}

auto draw_badge(ID2D1RenderTarget* t_target, const Painter& t_painter, const D2D1_RECT_F& t_box, BadgeSymbol t_symbol, Color t_fill, const auto& t_paint) -> void
{
	const D2D1_POINT_2F center   = middle_of(t_box);
	const float         radius   = (t_box.right - t_box.left) * K_TOAST_BADGE_RADIUS_SHARE;
	const float         stroke   = radius * K_TOAST_BADGE_STROKE_SHARE;
	const auto          point_at = [&](float t_x, float t_y) { return D2D1::Point2F(center.x + t_x * radius, center.y + t_y * radius); };

	t_target->FillEllipse(D2D1::Ellipse(center, radius, radius), t_paint(t_fill));

	ID2D1Brush* ink = t_paint(t_painter.colors.background);

	if (t_symbol == BadgeSymbol::EXCLAMATION) {
		t_target->DrawLine(point_at(0.0f, -0.46f), point_at(0.0f, 0.08f), ink, stroke, t_painter.stroke);
		t_target->FillEllipse(D2D1::Ellipse(point_at(0.0f, 0.48f), stroke * 0.55f, stroke * 0.55f), ink);
		return;
	}

	ComPtr<ID2D1PathGeometry> tick;
	ComPtr<ID2D1GeometrySink> sink;
	if (FAILED(t_painter.factory->CreatePathGeometry(&tick)) || FAILED(tick->Open(&sink))) return;

	sink->BeginFigure(point_at(-0.44f, 0.04f), D2D1_FIGURE_BEGIN_HOLLOW);
	sink->AddLine(point_at(-0.12f, 0.36f));
	sink->AddLine(point_at(0.46f, -0.28f));
	sink->EndFigure(D2D1_FIGURE_END_OPEN);

	if (SUCCEEDED(sink->Close())) {
		t_target->DrawGeometry(tick.Get(), ink, stroke, t_painter.stroke);
	}
}

auto paint_toast_mark(ID2D1RenderTarget* t_target, const Painter& t_painter, const LoginToast& t_toast, const D2D1_RECT_F& t_box, const auto& t_paint) -> void
{
	const os::TrayColors& colors = t_painter.colors;

	switch (shown_step(t_toast.login).state) {
		using enum os::TrayStepState;

		case WAITING:
		case RUNNING: {
			draw_progress_ring(t_target, t_painter, t_box, t_toast.spin, colors.accent, t_paint);
			break;
		}

		case DONE: {
			draw_badge(t_target, t_painter, t_box, BadgeSymbol::CHECK, colors.success, t_paint);
			break;
		}

		case FAILED: {
			draw_badge(t_target, t_painter, t_box, BadgeSymbol::EXCLAMATION, colors.error, t_paint);
			break;
		}

		case LEFT_TO_USER: {
			draw_badge(t_target, t_painter, t_box, BadgeSymbol::EXCLAMATION, colors.accent, t_paint);
			break;
		}
	}
}

auto paint_toast_line(ID2D1RenderTarget* t_target, const Painter& t_painter, const LoginToast& t_toast, const D2D1_RECT_F& t_shape, const auto& t_paint) -> void
{
	const os::TrayColors& colors       = t_painter.colors;
	const TextFormats&    formats      = *t_painter.formats;
	const ToastMeasures&  measures     = t_toast.measures;
	const float           top          = t_shape.top + K_TOAST_PADDING_Y;
	const float           middle       = top + K_TOAST_LINE_HEIGHT * 0.5f;
	const D2D1_RECT_F     line         = D2D1::RectF(t_shape.left, top, t_shape.right, top + K_TOAST_LINE_HEIGHT);
	const float           name_left    = t_shape.left + K_TOAST_TEXT_INSET;
	const float           divider_left = name_left + measures.name + K_TOAST_GAP;
	const float           mark_left    = divider_left + K_TOAST_DIVIDER_WIDTH + K_TOAST_GAP;
	const float           label_left   = mark_left + K_TOAST_MARK_SIZE + K_TOAST_GAP;
	const float           time_right   = t_shape.right - K_TOAST_PADDING_RIGHT;
	const float           time_left    = time_right - measures.time;
	const float           divider_top  = middle - K_TOAST_DIVIDER_HEIGHT * 0.5f;
	const D2D1_RECT_F     divider      = D2D1::RectF(divider_left, divider_top, divider_left + K_TOAST_DIVIDER_WIDTH, divider_top + K_TOAST_DIVIDER_HEIGHT);
	const D2D1_RECT_F     mark         = centered_box(mark_left, middle, K_TOAST_MARK_SIZE, t_painter.scale);
	const Color           time_color   = is_running(t_toast) ? colors.accent : colors.text_disabled;

	if (t_toast.game_icon) {
		draw_bitmap(t_target, t_toast.game_icon.Get(), t_shape.left + K_TOAST_PADDING_LEFT, middle, t_painter.scale);
	}

	draw_label(t_target, formats.toast_title.Get(), t_toast.account, name_left, divider_left, line, t_paint(colors.text));
	t_target->FillRectangle(divider, t_paint(colors.border));
	paint_toast_mark(t_target, t_painter, t_toast, mark, t_paint);
	t_target->PushAxisAlignedClip(D2D1::RectF(label_left, line.top, time_left - K_TOAST_TIME_GAP, line.bottom), D2D1_ANTIALIAS_MODE_ALIASED);
	draw_label(t_target, formats.toast_text.Get(), t_toast.step_label, label_left, label_left + measures.label + K_TOAST_TIME_GAP, line, t_paint(colors.text_dim));
	t_target->PopAxisAlignedClip();
	draw_label(t_target, formats.toast_count.Get(), toast_time_label(t_toast), time_left, time_right, line, t_paint(time_color));
}

auto paint_toast_details(ID2D1RenderTarget* t_target, const Painter& t_painter, const LoginToast& t_toast, const D2D1_RECT_F& t_shape, const auto& t_paint) -> void
{
	const os::TrayColors& colors      = t_painter.colors;
	const TextFormats&    formats     = *t_painter.formats;
	const ToastMeasures&  measures    = t_toast.measures;
	const float           shown       = smoothed(t_toast.details);
	const float           message_top = toast_message_top(t_shape);
	const D2D1_RECT_F     message     = D2D1::RectF(t_shape.left + K_TOAST_TEXT_INSET, message_top, t_shape.right - K_TOAST_PADDING_RIGHT, message_top + measures.message_height);
	const D2D1_POINT_2F   dot_center  = D2D1::Point2F(t_shape.left + K_TOAST_PADDING_LEFT + K_TOAST_ICON_SIZE * 0.5f, message_top + measures.message_line * 0.5f);
	const float           dot_radius  = K_TOAST_DOT_SIZE * 0.5f;
	const Color           dot_color   = offers_retry(t_toast) ? colors.error : colors.accent;

	t_target->FillEllipse(D2D1::Ellipse(dot_center, dot_radius, dot_radius), t_paint(dot_color, shown));
	t_target->DrawText(t_toast.message.c_str(), static_cast<UINT32>(t_toast.message.size()), formats.message.Get(), message, t_paint(colors.text_dim, shown), D2D1_DRAW_TEXT_OPTIONS_CLIP);

	if (!offers_retry(t_toast)) return;

	const ToastButtons      buttons = toast_buttons(t_toast, t_shape);
	const D2D1_ROUNDED_RECT retry{buttons.retry, K_TOAST_BUTTON_RADIUS, K_TOAST_BUTTON_RADIUS};
	const D2D1_ROUNDED_RECT open{buttons.open, K_TOAST_BUTTON_RADIUS, K_TOAST_BUTTON_RADIUS};

	const float        retry_lift = K_BUTTON_LIFT * t_toast.hover[ToastHit::RETRY] * shown;
	const float        open_fill  = (K_TOAST_GHOST_ALPHA + K_TOAST_GHOST_HOVER * t_toast.hover[ToastHit::OPEN]) * shown;
	const std::wstring open_label = std::wstring{L"Open "} + os::win32::K_APP_NAME_WIDE;

	t_target->FillRoundedRectangle(retry, t_paint(colors.accent, shown));
	t_target->FillRoundedRectangle(retry, t_paint(Color{255, 255, 255, 255}, retry_lift));
	draw_label(t_target, formats.button.Get(), K_RETRY_LABEL, buttons.retry.left, buttons.retry.right, buttons.retry, t_paint(colors.accent_ink, shown));

	t_target->FillRoundedRectangle(open, t_paint(colors.text, open_fill));
	draw_label(t_target, formats.button.Get(), open_label, buttons.open.left, buttons.open.right, buttons.open, t_paint(colors.text, shown));
}

auto paint_toast(ID2D1RenderTarget* t_target, const Painter& t_painter, const LoginToast& t_toast) -> void
{
	const os::TrayColors& colors     = t_painter.colors;
	const D2D1_SIZE_F     size       = t_target->GetSize();
	const D2D1_RECT_F     shape      = toast_shape(t_toast, size.width, size.height);
	const float           radius     = toast_radius(t_toast);
	const float           error_edge = offers_retry(t_toast) ? t_toast.finished * K_TOAST_ERROR_EDGE : 0.0f;

	ComPtr<ID2D1SolidColorBrush> brush;
	if (FAILED(t_target->CreateSolidColorBrush(to_d2d(colors.text), &brush))) return;

	const auto paint = [&](Color t_color, float t_opacity = 1.0f) {
		brush->SetColor(to_d2d(t_color, t_opacity));
		return brush.Get();
	};

	t_target->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
	draw_soft_shadow(t_target, brush.Get(), shape, radius);
	paint_body(t_target, brush.Get(), colors, shape, radius, mix(colors.border, colors.error, error_edge), t_painter.scale);

	t_target->PushAxisAlignedClip(shape, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
	paint_toast_line(t_target, t_painter, t_toast, shape, paint);

	if (t_toast.details > 0.0f && !t_toast.message.empty()) {
		paint_toast_details(t_target, t_painter, t_toast, shape, paint);
	}

	t_target->PopAxisAlignedClip();
}
}

namespace os {

struct Tray::Native {
	using Clock = std::chrono::steady_clock;

	HWND         window       = nullptr;
	HICON        icon         = nullptr;
	HICON        locked_icon  = nullptr;
	bool         locked       = false;
	bool         icon_added   = false;
	u32          add_attempts = 0;
	std::wstring tooltip;

	TrayColors colors{
		.background    = {37, 34, 43, 255},
		.border        = {64, 64, 70, 255},
		.separator     = {50, 50, 56, 255},
		.scroll_thumb  = {120, 120, 128, 190},
		.shadow        = {0, 0, 0, 255},
		.text          = {232, 232, 236, 255},
		.text_dim      = {150, 150, 158, 255},
		.text_disabled = {108, 108, 116, 255},
		.accent        = {203, 166, 247, 255},
		.accent_ink    = {24, 25, 30, 255},
		.success       = {80, 200, 120, 255},
		.error         = {220, 90, 80, 255},
	};

	std::span<const u8> game_icon_sources[K_TRAY_MAX_GAMES]{};
	std::span<const u8> logo_source;

	TrayEvent pending_event{};

	std::function<void(TrayMenu*)> fill_menu;
	TrayMenu                       menu{};

	ComPtr<ID2D1Factory>     factory;
	ComPtr<IDWriteFactory>   writer;
	ComPtr<ID2D1StrokeStyle> stroke;
	TextFormats              formats;
	float                    update_width   = 0.0f;
	float                    updating_width = 0.0f;
	bool                     popup_ready    = false;

	HWND               menu_window = nullptr;
	Canvas             menu_canvas;
	MenuImages         images;
	Panel              games;
	Panel              side;
	bool               open             = false;
	bool               activated        = false;
	bool               tracking_leave   = false;
	bool               side_left        = false;
	bool               side_focused     = false;
	float              scale            = 1.0f;
	float              appear           = 0.0f;
	float              slide            = 0.0f;
	float              games_min_height = 0.0f;
	RECT               work{};
	RECT               games_screen{};
	POINT              menu_origin{};
	SIZE               menu_size{};
	D2D1_RECT_F        side_area{};
	SideView           side_view = SideView::ACCOUNTS;
	std::optional<u32> open_game;
	std::optional<u32> shown_game;
	D2D1_POINT_2F      last_pointer{};
	bool               animating = false;
	Clock::time_point  last_tick;

	RowDrag                                         drag;
	AccountFormState                                form;
	FormLayout                                      form_frame;
	std::optional<FormHit>                          form_hot;
	TrayAccountForm                                 saved_form{};
	std::vector<std::wstring>                       game_labels;
	std::function<void(u32, u32, TrayAccountForm*)> fill_form;
	wchar_t                                         high_surrogate = L'\0';
	std::optional<u64>                              raised;
	float                                           lift = 0.0f;
	std::optional<PopupMenu>                        popup;
	CaretMotion                                     caret;
	float                                           add_label_width = 0.0f;
	bool                                            add_hot         = false;
	std::optional<PendingSide>                      pending_side;
	TimerResolution                                 menu_resolution;

	TrayUpdate  update = TrayUpdate::NONE;
	UpdateBadge badge;
	bool        update_hot = false;

	LoginToast      toast;
	TimerResolution toast_resolution;

	POINT             menu_anchor{};
	TextEditor        password_field;
	float             password_scroll = 0.0f;
	bool              unlocking       = false;
	bool              wrong_password  = false;
	float             shake_seconds   = 0.0f;
	float             spin_seconds    = 0.0f;
	Clock::time_point caret_epoch;

	static auto CALLBACK window_proc(HWND t_window, UINT t_message, WPARAM t_wparam, LPARAM t_lparam) -> LRESULT;
	static auto CALLBACK popup_proc(HWND t_window, UINT t_message, WPARAM t_wparam, LPARAM t_lparam) -> LRESULT;

	[[nodiscard]] auto handle_message(UINT t_message, WPARAM t_wparam, LPARAM t_lparam) -> LRESULT;
	[[nodiscard]] auto handle_menu_message(UINT t_message, WPARAM t_wparam, LPARAM t_lparam) -> LRESULT;
	[[nodiscard]] auto handle_toast_message(UINT t_message, WPARAM t_wparam, LPARAM t_lparam) -> LRESULT;

	auto add_icon() -> bool;
	auto remove_icon() -> void;
	auto update_icon() -> void;
	[[nodiscard]] auto shown_icon() const -> HICON;
	auto fill_tooltip(wchar_t (&t_tooltip)[128]) const -> void;

	[[nodiscard]] auto ensure_popup() -> bool;
	[[nodiscard]] auto popup_window(DWORD t_extra_style) -> HWND;
	[[nodiscard]] auto painter(float t_scale) const -> Painter;
	auto show_menu_at(POINT t_anchor) -> void;
	auto close_menu() -> void;
	auto choose(const Row& t_row) -> void;
	auto reload_menu() -> void;
	auto build_games(Panel* t_panel) -> void;
	auto build_game_labels() -> void;
	auto lay_out_games() -> void;
	auto build_side(u32 t_game_row) -> void;
	auto lay_out_side() -> void;
	[[nodiscard]] auto form_layout(const D2D1_RECT_F& t_area) const -> FormLayout;
	[[nodiscard]] auto pixel_size(const Panel& t_panel) const -> SIZE;
	[[nodiscard]] auto to_menu_space(const RECT& t_screen) const -> D2D1_RECT_F;
	auto open_side(u32 t_row, SideView t_view) -> void;
	auto close_side() -> void;
	auto enter_side() -> void;
	auto point_at_game(std::optional<u32> t_row, D2D1_POINT_2F t_from, D2D1_POINT_2F t_to) -> void;
	[[nodiscard]] auto heads_for_side(D2D1_POINT_2F t_from, D2D1_POINT_2F t_to) const -> bool;
	auto show_side_for(std::optional<u32> t_row) -> void;
	auto leave_side_if_outside(std::optional<D2D1_POINT_2F> t_point) -> void;
	[[nodiscard]] auto side_is_ready() const -> bool;
	[[nodiscard]] auto row_at(const Panel& t_panel, float t_y) const -> std::optional<u32>;
	[[nodiscard]] auto panel_at(D2D1_POINT_2F t_point) -> Panel*;
	[[nodiscard]] auto side_row_right() const -> float;
	[[nodiscard]] auto side_part_at(u32 t_row, float t_x, float t_content_y) const -> RowPart;
	[[nodiscard]] auto side_header() const -> D2D1_RECT_F;
	[[nodiscard]] auto over_add_button(D2D1_POINT_2F t_point) const -> bool;
	[[nodiscard]] auto menu_point(LPARAM t_lparam) const -> D2D1_POINT_2F;
	[[nodiscard]] auto cursor_point() const -> D2D1_POINT_2F;

	auto open_add_form() -> void;
	auto open_edit_form(u32 t_side_row) -> void;
	auto start_form() -> void;
	auto wipe_form() -> void;
	auto close_form() -> void;
	auto submit_form() -> void;
	auto toggle_form_game(usize t_index) -> void;
	auto focus_field(FormField t_field) -> void;
	auto form_key(WPARAM t_key) -> void;
	[[nodiscard]] auto form_hit(D2D1_POINT_2F t_point) const -> std::optional<FormHit>;
	auto click_form(D2D1_POINT_2F t_point) -> void;
	auto build_region_list() -> void;
	auto open_regions() -> void;
	auto toggle_favorite(u32 t_side_row) -> void;

	[[nodiscard]] auto focused_text() -> std::optional<FocusedText>;
	[[nodiscard]] auto text_at(D2D1_POINT_2F t_point) -> bool;
	[[nodiscard]] auto text_index_at(const FocusedText& t_text, float t_x) const -> u32;
	[[nodiscard]] auto follow_caret(float t_seconds) -> bool;
	[[nodiscard]] auto caret_look() -> CaretLook;
	auto edited_text(bool t_changed) -> void;
	auto press_text(D2D1_POINT_2F t_point) -> void;
	auto select_text(D2D1_POINT_2F t_point) -> void;
	auto finish_text_selection() -> void;
	auto type_text(wchar_t t_unit) -> void;
	[[nodiscard]] auto is_selecting_text() const -> bool;

	auto open_popup(std::span<const MenuItem> t_items, D2D1_POINT_2F t_at, std::optional<u32> t_account_row) -> void;
	auto close_popup() -> void;
	[[nodiscard]] auto popup_item_at(D2D1_POINT_2F t_point) const -> std::optional<usize>;
	auto choose_popup_item(D2D1_POINT_2F t_point) -> void;
	auto run_menu_action(MenuAction t_action, std::optional<u32> t_account_row) -> void;
	[[nodiscard]] auto menu_cursor() const -> LPCWSTR;

	auto render_menu() -> void;
	auto on_right_click(LPARAM t_lparam) -> void;
	auto on_pointer(LPARAM t_lparam) -> void;
	auto point_at_side(std::optional<D2D1_POINT_2F> t_point) -> void;
	auto on_leave() -> void;
	auto on_press(LPARAM t_lparam) -> void;
	[[nodiscard]] auto drag_slots() const -> std::pair<u32, u32>;
	auto drag_row(float t_content_y) -> void;
	[[nodiscard]] auto row_shift(u32 t_row) const -> float;
	[[nodiscard]] auto step_rows(float t_seconds) -> bool;
	[[nodiscard]] auto step_games(float t_seconds) -> bool;
	[[nodiscard]] auto raised_row() const -> std::optional<RaisedRow>;
	auto finish_drag() -> void;
	auto on_click(LPARAM t_lparam) -> void;
	[[nodiscard]] auto over_scrollbar(const Panel& t_panel, D2D1_POINT_2F t_point) const -> bool;
	auto scroll_panel(Panel* t_panel, float t_delta) -> void;
	auto drag_thumb(float t_y) -> void;
	[[nodiscard]] auto update_box() const -> D2D1_RECT_F;
	[[nodiscard]] auto over_update(float t_x, float t_y) const -> bool;
	auto press_update() -> void;
	[[nodiscard]] auto step_badge(float t_seconds, float t_blend) -> bool;
	auto on_key(WPARAM t_key) -> void;
	auto step_hot(Panel* t_panel, i32 t_step) -> void;

	[[nodiscard]] auto shows_password_field() const -> bool;
	[[nodiscard]] auto password_row_box() const -> std::optional<D2D1_RECT_F>;
	[[nodiscard]] auto caret_shown() const -> bool;
	[[nodiscard]] auto shake_offset() const -> float;
	[[nodiscard]] auto over_password_submit(const Row& t_row, float t_x, float t_y) const -> bool;
	[[nodiscard]] auto edit_password(WPARAM t_key) -> bool;
	auto on_char(wchar_t t_character) -> void;
	auto clear_password() -> void;
	auto submit_password() -> void;
	auto watch() -> void;
	auto start_animation() -> void;
	auto animate() -> void;

	[[nodiscard]] auto ensure_toast() -> bool;
	auto begin_toast() -> void;
	auto measure_toast_text() -> void;
	auto raise_toast() -> void;
	auto show_login(const TrayLogin& t_login) -> void;
	auto hide_login() -> void;
	[[nodiscard]] auto toast_size() const -> SIZE;
	[[nodiscard]] auto toast_hit(LPARAM t_lparam) const -> std::optional<ToastHit>;
	auto render_toast() -> void;
	auto step_toast() -> void;
};

Tray::Tray()
	: m_native(std::make_unique<Native>())
{
}

Tray::~Tray()
{
	Native* native = m_native.get();
	native->remove_icon();

	if (native->window != nullptr) {
		KillTimer(native->window, K_ADD_ICON_RETRY_TIMER);
		DestroyWindow(native->window);
	}

	if (native->locked_icon != nullptr) {
		DestroyIcon(native->locked_icon);
	}

	native->wipe_form();
	native->clear_password();
	SecureZeroMemory(&native->saved_form, sizeof(native->saved_form));
	native->images = MenuImages{};
	native->toast.game_icon.Reset();
	release_canvas(&native->menu_canvas);
	release_canvas(&native->toast.canvas);

	if (native->menu_window != nullptr) {
		DestroyWindow(native->menu_window);
	}

	if (native->toast.window != nullptr) {
		DestroyWindow(native->toast.window);
	}
}

auto Tray::create(std::string_view t_tooltip) -> bool
{
	Native*         native   = m_native.get();
	const HINSTANCE instance = GetModuleHandleW(nullptr);

	const WNDCLASSEXW window_class{
		.cbSize        = sizeof(WNDCLASSEXW),
		.lpfnWndProc   = Native::window_proc,
		.hInstance     = instance,
		.lpszClassName = os::win32::K_TRAY_WINDOW_CLASS_NAME,
	};

	if (RegisterClassExW(&window_class) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
		debug_log::write(K_LOG_CATEGORY, "RegisterClassExW failed, err=%lu", GetLastError());
		return false;
	}

	if (CreateWindowExW(0, os::win32::K_TRAY_WINDOW_CLASS_NAME, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, instance, native) == nullptr) {
		debug_log::write(K_LOG_CATEGORY, "failed to create the tray window, err=%lu", GetLastError());
		native->window = nullptr;
		return false;
	}

	native->tooltip = win32::to_wide(t_tooltip);

	native->icon = win32::load_app_icon(win32::AppIconSize::SMALL_ICON);
	if (native->icon == nullptr) {
		native->icon = LoadIconW(nullptr, IDI_APPLICATION);
	}

	native->add_icon();

	return true;
}

auto Tray::on_menu_open(std::function<void(TrayMenu*)> t_fill_menu) -> void
{
	m_native->fill_menu = std::move(t_fill_menu);
}

auto Tray::on_account_edit(std::function<void(u32 t_game, u32 t_row, TrayAccountForm*)> t_fill_form) -> void
{
	m_native->fill_form = std::move(t_fill_form);
}

auto Tray::take_account_form(TrayAccountForm* t_form) -> void
{
	Native* native = m_native.get();

	*t_form = native->saved_form;
	SecureZeroMemory(&native->saved_form, sizeof(native->saved_form));
}

auto Tray::reload_menu() -> void
{
	m_native->reload_menu();
}

auto Tray::set_locked(bool t_locked) -> void
{
	Native* native = m_native.get();
	if (t_locked == native->locked) return;

	const bool reopen = native->open && !t_locked;

	native->locked         = t_locked;
	native->unlocking      = false;
	native->wrong_password = false;
	native->clear_password();
	native->close_menu();

	if (native->locked && native->locked_icon == nullptr) {
		native->locked_icon = greyscale_icon(native->icon);
	}

	native->update_icon();

	if (reopen) {
		native->show_menu_at(native->menu_anchor);
	}
}

auto Tray::take_unlock_password(std::span<char> t_out) -> std::string_view
{
	Native* native = m_native.get();
	if (t_out.empty()) return {};

	const std::string_view typed  = native->password_field.value();
	const usize            length = std::min(typed.size(), t_out.size() - 1);

	std::ranges::copy(typed.substr(0, length), t_out.begin());
	native->clear_password();

	t_out[length] = '\0';

	return std::string_view{t_out.data(), length};
}

auto Tray::show_unlock_failure() -> void
{
	Native* native = m_native.get();

	native->unlocking      = false;
	native->wrong_password = native->open;
	native->shake_seconds  = native->open ? K_SHAKE_SECONDS : 0.0f;
	native->clear_password();
	native->start_animation();
}

auto Tray::show_login(const TrayLogin& t_login) -> void
{
	m_native->show_login(t_login);
}

auto Tray::hide_login() -> void
{
	m_native->hide_login();
}

auto Tray::set_update(TrayUpdate t_update) -> void
{
	Native* native = m_native.get();
	if (t_update == native->update) return;

	native->update = t_update;
	native->start_animation();
}

auto Tray::is_icon_visible() const -> bool
{
	return m_native->icon_added;
}

auto Tray::take_event() -> TrayEvent
{
	return std::exchange(m_native->pending_event, TrayEvent{});
}

auto Tray::set_colors(const TrayColors& t_colors) -> void
{
	m_native->colors = t_colors;
}

auto Tray::set_game_icon(u32 t_game, std::span<const u8> t_png) -> void
{
	if (t_game >= K_TRAY_MAX_GAMES) return;

	m_native->game_icon_sources[t_game] = t_png;
	m_native->images.game_icons[t_game].Reset();
}

auto Tray::set_logo(std::span<const u8> t_png) -> void
{
	m_native->logo_source = t_png;
	m_native->images.logo.Reset();
}

auto Tray::Native::shown_icon() const -> HICON
{
	return locked && locked_icon != nullptr ? locked_icon : icon;
}

auto Tray::Native::fill_tooltip(wchar_t (&t_tooltip)[128]) const -> void
{
	wcsncpy_s(t_tooltip, tooltip.c_str(), _TRUNCATE);

	if (locked) {
		wcsncat_s(t_tooltip, L" (locked)", _TRUNCATE);
	}
}

auto Tray::Native::update_icon() -> void
{
	if (!icon_added) return;

	NOTIFYICONDATAW data{
		.cbSize = sizeof(NOTIFYICONDATAW),
		.hWnd   = window,
		.uID    = K_TRAY_ICON_ID,
		.uFlags = NIF_ICON | NIF_TIP,
		.hIcon  = shown_icon(),
	};
	fill_tooltip(data.szTip);

	Shell_NotifyIconW(NIM_MODIFY, &data);
}

auto Tray::Native::ensure_popup() -> bool
{
	if (popup_ready) return true;

	clear_password();

	if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory.GetAddressOf())) || FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(writer.GetAddressOf())))) {
		debug_log::write(K_LOG_CATEGORY, "Direct2D or DirectWrite is unavailable, so the tray has no menu");
		return false;
	}

	const D2D1_STROKE_STYLE_PROPERTIES round = D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_ROUND);
	factory->CreateStrokeStyle(round, nullptr, 0, &stroke);

	const wchar_t* family = menu_font_family(writer.Get());
	formats.body          = make_format(writer.Get(), family, K_BODY_SIZE, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_LEADING);
	formats.strong        = make_format(writer.Get(), family, K_BODY_SIZE, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_LEADING);
	formats.caption       = make_format(writer.Get(), family, K_CAPTION_SIZE, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_LEADING);
	formats.count         = make_format(writer.Get(), family, K_CAPTION_SIZE, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_TRAILING);
	formats.chip          = make_format(writer.Get(), family, K_CHIP_TEXT_SIZE, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
	formats.button        = make_format(writer.Get(), family, K_CAPTION_SIZE, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER);
	formats.message       = make_format(writer.Get(), family, K_TOAST_MESSAGE_SIZE, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_LEADING);
	formats.toast_text    = make_format(writer.Get(), family, K_TOAST_TEXT_SIZE, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_LEADING);
	formats.toast_title   = make_format(writer.Get(), family, K_TOAST_TEXT_SIZE, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_LEADING);
	formats.toast_count   = make_format(writer.Get(), family, K_TOAST_COUNT_SIZE, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_TRAILING);
	formats.badge         = make_format(writer.Get(), family, K_CAPTION_SIZE, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_LEADING);
	formats.keycap        = make_format(writer.Get(), family, K_KEYCAP_TEXT_SIZE, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);

	if (formats.message) {
		formats.message->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
		formats.message->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
	}

	const float update_inside = K_UPDATE_PADDING * 2.0f + K_UPDATE_GLYPH_SIZE + K_UPDATE_GAP;

	update_width    = text_width(writer.Get(), formats.badge.Get(), K_UPDATE_LABEL) + update_inside;
	updating_width  = text_width(writer.Get(), formats.badge.Get(), K_UPDATING_LABEL) + update_inside;
	add_label_width = text_width(writer.Get(), formats.caption.Get(), K_ADD_LABEL);

	const WNDCLASSEXW popup_class{
		.cbSize        = sizeof(WNDCLASSEXW),
		.lpfnWndProc   = popup_proc,
		.hInstance     = GetModuleHandleW(nullptr),
		.hCursor       = LoadCursorW(nullptr, IDC_ARROW),
		.lpszClassName = os::win32::K_TRAY_MENU_CLASS_NAME,
	};

	if (RegisterClassExW(&popup_class) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
		debug_log::write(K_LOG_CATEGORY, "RegisterClassExW for the tray menu failed, err=%lu", GetLastError());
		return false;
	}

	menu_window = popup_window(0);
	if (menu_window == nullptr) {
		debug_log::write(K_LOG_CATEGORY, "failed to create the tray menu window, err=%lu", GetLastError());
		return false;
	}

	popup_ready = true;

	return true;
}

auto Tray::Native::popup_window(DWORD t_extra_style) -> HWND
{
	return CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | t_extra_style, os::win32::K_TRAY_MENU_CLASS_NAME, L"", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, GetModuleHandleW(nullptr), this);
}

auto Tray::Native::painter(float t_scale) const -> Painter
{
	return Painter{
		.factory        = factory.Get(),
		.writer         = writer.Get(),
		.formats        = &formats,
		.stroke         = stroke.Get(),
		.colors         = colors,
		.scale          = t_scale,
		.update_width   = update_width,
		.updating_width = updating_width,
		.password =
			PasswordField{
				.editor    = &password_field,
				.caret     = &caret,
				.scroll    = password_scroll,
				.blinked   = caret_shown(),
				.unlocking = unlocking,
				.wrong     = wrong_password,
				.shift     = shake_offset(),
				.spin      = spin_seconds / K_SPIN_SECONDS,
			},
	};
}

auto Tray::Native::build_games(Panel* t_panel) -> void
{
	std::vector<Row>* rows = &t_panel->rows;
	rows->clear();
	rows->push_back(Row{.kind = RowKind::BRAND, .label = win32::K_APP_NAME_WIDE});
	rows->push_back(Row{.kind = RowKind::SEPARATOR});

	if (menu.locked && menu.can_unlock) {
		rows->push_back(Row{.kind = RowKind::PASSWORD});
	} else if (menu.locked) {
		rows->push_back(Row{.kind = RowKind::NOTE, .label = L"Vault locked", .glyph = Glyph::LOCK});
	} else if (menu.game_count == 0) {
		rows->push_back(Row{.kind = RowKind::NOTE, .label = L"No games"});
	}

	for (u32 i = 0; i < menu.game_count && !menu.locked; i += 1) {
		const TrayGame& game = menu.games[i];

		rows->push_back(Row{
			.kind   = RowKind::GAME,
			.label  = win32::to_wide(game.title),
			.detail = game.account_count > 0 ? std::to_wstring(game.account_count) : std::wstring{},
			.item   = i,
			.icon   = game.game,
		});
	}

	t_panel->footer = rows->size();
	rows->push_back(Row{.kind = RowKind::SEPARATOR});
	rows->push_back(Row{
		.kind   = RowKind::ACTION,
		.label  = std::wstring{L"Open "} + win32::K_APP_NAME_WIDE,
		.glyph  = Glyph::OPEN,
		.action = TrayEventType::SHOW_WINDOW,
	});

	if (menu.can_lock && !menu.locked) {
		rows->push_back(Row{.kind = RowKind::ACTION, .label = L"Lock vault", .glyph = Glyph::LOCK, .action = TrayEventType::LOCK});
	}

	rows->push_back(Row{.kind = RowKind::ACTION, .label = L"Quit", .glyph = Glyph::POWER, .action = TrayEventType::EXIT, .destructive = true});
}

auto Tray::Native::build_game_labels() -> void
{
	game_labels.clear();

	for (const TrayGame& game : std::span{menu.games, menu.game_count}) {
		game_labels.push_back(win32::to_wide(game.short_title));
	}
}

auto Tray::Native::lay_out_games() -> void
{
	float y      = K_PANEL_PADDING;
	float widest = 0.0f;

	for (Row& row : games.rows) {
		row.top     = y;
		row.height  = row.kind == RowKind::SEPARATOR ? K_SEPARATOR_HEIGHT : (row.kind == RowKind::PASSWORD ? K_FIELD_ROW_HEIGHT : K_ROW_HEIGHT);
		row.label_x = K_PANEL_PADDING + K_ROW_INSET + K_ICON_SIZE + K_ICON_GAP;
		y += row.height;

		if (row.kind == RowKind::PASSWORD) {
			widest = std::max(widest, K_FIELD_MIN_WIDTH + K_PANEL_PADDING * 2.0f);
			continue;
		}

		const float label = text_width(writer.Get(), (row.kind == RowKind::BRAND ? formats.strong : formats.body).Get(), row.label);
		float       need  = row.label_x + label + K_ROW_INSET + K_PANEL_PADDING;

		if (row.kind == RowKind::GAME) {
			row.detail_width = text_width(writer.Get(), formats.count.Get(), row.detail);
			need += K_ICON_GAP + row.detail_width + K_COUNT_GAP + K_CHEVRON_SIZE;
		} else if (row.kind == RowKind::BRAND) {
			need += K_ICON_GAP + std::max(update_width, updating_width);
		}

		widest = std::max(widest, need);
	}

	const float content = y + K_PANEL_PADDING;
	const float stretch = std::max(0.0f, games_min_height - content);

	if (stretch > 0.0f && games.footer) {
		for (Row& row : std::span{games.rows}.subspan(*games.footer)) {
			row.top += stretch;
		}
	}

	games.content_height = content + stretch;
	games.height         = games.content_height;
	games.width          = std::ceil(std::clamp(widest, K_GAMES_MIN_WIDTH, K_MAX_WIDTH));
}

auto Tray::Native::build_side(u32 t_game_row) -> void
{
	const std::vector<ShownRow> before = shown_rows(side);

	side.rows.clear();
	side.hot.reset();
	side.hot_part.reset();

	const TrayGame& game = menu.games[*games.rows[t_game_row].item];

	for (const u32 index : std::views::iota(game.first_account, std::min(game.first_account + game.account_count, menu.account_count))) {
		const TrayAccount& account = menu.accounts[index];

		side.rows.push_back(Row{
			.kind     = RowKind::ACCOUNT,
			.label    = win32::to_wide(account.username),
			.detail   = win32::to_wide(account.region),
			.note     = win32::to_wide(account.note),
			.when     = win32::to_wide(account.last_used),
			.item     = index,
			.favorite = account.favorite,
			.id       = account.id,
		});
	}

	if (side.rows.empty()) {
		side.rows.push_back(Row{.kind = RowKind::NOTE, .label = std::wstring{K_NO_ACCOUNTS_LABEL}});
	}

	for (Row& row : side.rows) {
		row.height       = row.kind == RowKind::ACCOUNT ? K_ACCOUNT_ROW_HEIGHT : K_ROW_HEIGHT;
		row.label_x      = K_PANEL_PADDING + K_ROW_INSET;
		row.detail_width = row.detail.empty() ? 0.0f : text_width(writer.Get(), formats.chip.Get(), row.detail) + K_CHIP_PADDING * 2.0f;
	}

	lay_out_side();
	slide_from(&side, before);

	side.frame = D2D1::RectF(side_area.left, side_area.top + K_SIDE_HEADER_HEIGHT, side_area.right, side_area.bottom);
	fit_rows(&side, side.frame.bottom - side.frame.top);
}

auto Tray::Native::lay_out_side() -> void
{
	float y = K_PANEL_PADDING;

	for (Row& row : side.rows) {
		row.top = y;
		y += row.height;
	}

	side.content_height = y + K_PANEL_PADDING;
}

auto Tray::Native::form_layout(const D2D1_RECT_F& t_area) const -> FormLayout
{
	FormLayout  layout;
	const float left   = t_area.left + K_SIDE_PADDING;
	const float right  = t_area.right - K_SIDE_PADDING;
	const float column = (right - left - K_FORM_COLUMN_GAP) * 0.5f;
	const float second = left + column + K_FORM_COLUMN_GAP;
	const float middle = t_area.top + K_FORM_HEADER_HEIGHT * 0.5f;

	const auto place = [column](float t_left, float t_top, D2D1_RECT_F* t_label, D2D1_RECT_F* t_field) {
		*t_label = D2D1::RectF(t_left, t_top, t_left + column, t_top + K_FORM_LABEL_HEIGHT);
		*t_field = D2D1::RectF(t_left, t_label->bottom, t_left + column, t_label->bottom + K_FORM_FIELD_HEIGHT);
	};

	layout.back   = D2D1::RectF(left, middle - K_FORM_BACK_SIZE * 0.5f, left + K_FORM_BACK_SIZE, middle + K_FORM_BACK_SIZE * 0.5f);
	layout.header = D2D1::RectF(layout.back.right + K_ICON_GAP, t_area.top, right, t_area.top + K_FORM_HEADER_HEIGHT);

	const float first_row = t_area.top + K_FORM_HEADER_HEIGHT;
	place(left, first_row, &layout.labels[FormField::USERNAME], &layout.fields[FormField::USERNAME]);
	place(second, first_row, &layout.labels[FormField::PASSWORD], &layout.fields[FormField::PASSWORD]);

	const float second_row = layout.fields[FormField::USERNAME].bottom + K_FORM_ROW_GAP;
	place(left, second_row, &layout.labels[FormField::NOTE], &layout.fields[FormField::NOTE]);
	place(second, second_row, &layout.region_label, &layout.region);

	const D2D1_RECT_F& password    = layout.fields[FormField::PASSWORD];
	const float        reveal_edge = (K_FORM_FIELD_HEIGHT - K_FORM_REVEAL_SIZE) * 0.5f;
	layout.reveal                  = D2D1::RectF(password.right - reveal_edge - K_FORM_REVEAL_SIZE, password.top + reveal_edge, password.right - reveal_edge, password.bottom - reveal_edge);

	float chips_top    = layout.region.bottom + K_FORM_ROW_GAP + K_FORM_LABEL_HEIGHT;
	float chip_left    = left;
	layout.games_label = D2D1::RectF(left, chips_top - K_FORM_LABEL_HEIGHT, right, chips_top);

	for (const std::wstring& label : game_labels) {
		const float width = K_FORM_CHIP_PADDING * 2.0f + K_FORM_CHIP_ICON_SIZE + K_FORM_CHIP_ICON_GAP + text_width(writer.Get(), formats.caption.Get(), label);

		if (chip_left > left && chip_left + width > right) {
			chip_left = left;
			chips_top += K_FORM_CHIP_HEIGHT + K_FORM_CHIP_GAP;
		}

		layout.games.push_back(D2D1::RectF(chip_left, chips_top, chip_left + width, chips_top + K_FORM_CHIP_HEIGHT));
		chip_left += width + K_FORM_CHIP_GAP;
	}

	const float chips_bottom = chips_top + K_FORM_CHIP_HEIGHT;
	layout.needed_height     = chips_bottom + K_FORM_ROW_GAP + K_FORM_BUTTON_HEIGHT + K_SIDE_PADDING - t_area.top;

	const auto  button_width = [this](std::wstring_view t_label) { return text_width(writer.Get(), formats.button.Get(), t_label) + K_FORM_BUTTON_PADDING * 2.0f; };
	const float bottom       = t_area.bottom - K_SIDE_PADDING;
	const float top          = bottom - K_FORM_BUTTON_HEIGHT;
	const float save_left    = right - button_width(K_SAVE_LABEL);
	const float cancel_left  = save_left - K_FORM_BUTTON_GAP - button_width(K_CANCEL_LABEL);

	layout.save   = D2D1::RectF(save_left, top, right, bottom);
	layout.cancel = D2D1::RectF(cancel_left, top, save_left - K_FORM_BUTTON_GAP, bottom);

	const float list_height = K_PANEL_PADDING * 2.0f + K_REGION_ROW_HEIGHT * static_cast<float>(K_REGION_ROWS_SHOWN);
	const float below       = layout.region.bottom + K_REGION_LIST_GAP;
	const float above       = layout.region.top - K_REGION_LIST_GAP - list_height;
	const bool  fits_below  = below + list_height <= t_area.bottom;
	const float list_top    = fits_below ? below : std::max(above, t_area.top + K_PANEL_PADDING);
	const float list_right  = layout.region.right;
	layout.regions          = D2D1::RectF(list_right - K_REGION_LIST_WIDTH, list_top, list_right, list_top + list_height);

	return layout;
}

auto Tray::Native::pixel_size(const Panel& t_panel) const -> SIZE
{
	return SIZE{static_cast<LONG>(std::ceil(t_panel.width * scale)), static_cast<LONG>(std::ceil(t_panel.height * scale))};
}

auto Tray::Native::to_menu_space(const RECT& t_screen) const -> D2D1_RECT_F
{
	const float left   = static_cast<float>(t_screen.left - menu_origin.x) / scale;
	const float top    = static_cast<float>(t_screen.top - menu_origin.y) / scale;
	const float right  = static_cast<float>(t_screen.right - menu_origin.x) / scale;
	const float bottom = static_cast<float>(t_screen.bottom - menu_origin.y) / scale;

	return D2D1::RectF(left, top, right, bottom);
}

auto Tray::Native::show_menu_at(POINT t_anchor) -> void
{
	if (!ensure_popup()) return;

	close_menu();

	menu_anchor = t_anchor;
	menu        = TrayMenu{};
	menu.locked = locked;
	if (fill_menu) {
		fill_menu(&menu);
	}

	const HMONITOR monitor = MonitorFromPoint(t_anchor, MONITOR_DEFAULTTONEAREST);
	MONITORINFO    info{.cbSize = sizeof(MONITORINFO)};
	GetMonitorInfoW(monitor, &info);
	work = info.rcWork;

	const float new_scale = monitor_scale(monitor);
	if (new_scale != scale) {
		scale  = new_scale;
		images = MenuImages{};
	}

	build_game_labels();
	build_games(&games);

	const bool can_manage_accounts = !menu.locked && menu.game_count > 0;
	games_min_height               = can_manage_accounts ? form_layout(D2D1::RectF(0.0f, 0.0f, K_SIDE_WIDTH, 0.0f)).needed_height : 0.0f;
	lay_out_games();

	SIZE games_size = pixel_size(games);
	CalculatePopupWindowPosition(&t_anchor, &games_size, TPM_LEFTALIGN | TPM_BOTTOMALIGN | TPM_WORKAREA, nullptr, &games_screen);

	const auto side_width  = static_cast<LONG>(std::lround(K_SIDE_WIDTH * scale));
	side_left              = games_screen.right + side_width > work.right;
	const LONG side_start  = side_left ? games_screen.left - side_width : games_screen.right;
	const RECT side_screen = RECT{side_start, games_screen.top, side_start + side_width, games_screen.bottom};

	RECT bounds;
	UnionRect(&bounds, &games_screen, &side_screen);

	const auto margin = static_cast<LONG>(std::ceil(K_SHADOW_MARGIN * scale));
	InflateRect(&bounds, margin, margin);

	update     = menu.update;
	badge      = UpdateBadge{.state = update};
	update_hot = false;

	menu_origin           = POINT{bounds.left, bounds.top};
	menu_size             = SIZE{bounds.right - bounds.left, bounds.bottom - bounds.top};
	games.frame           = to_menu_space(games_screen);
	side_area             = to_menu_space(side_screen);
	form_frame            = form_layout(side_area);
	games.chevron_degrees = side_left ? K_CHEVRON_LEFT_DEGREES : 0.0f;
	games.hot.reset();
	games.selected.reset();
	open           = true;
	activated      = false;
	tracking_leave = false;
	side_focused   = false;
	side_view      = SideView::ACCOUNTS;
	slide          = 0.0f;
	drag           = RowDrag{};
	open_game.reset();
	shown_game.reset();
	pending_side.reset();
	form_hot.reset();
	popup.reset();
	caret.restart();
	appear      = 0.0f;
	caret_epoch = Clock::now();

	render_menu();
	ShowWindow(menu_window, SW_SHOW);
	SetForegroundWindow(menu_window);
	SetTimer(menu_window, K_WATCH_TIMER, K_WATCH_INTERVAL_MS, nullptr);
	start_animation();
}

auto Tray::Native::reload_menu() -> void
{
	if (!open) return;

	const D2D1_RECT_F frame = games.frame;

	menu        = TrayMenu{};
	menu.locked = locked;
	if (fill_menu) {
		fill_menu(&menu);
	}

	build_game_labels();
	build_games(&games);
	lay_out_games();
	games.frame = frame;
	form_frame  = form_layout(side_area);
	drag        = RowDrag{};
	popup.reset();

	const bool still_a_game = open_game && *open_game < games.rows.size() && games.rows[*open_game].kind == RowKind::GAME;

	if (still_a_game && side_view == SideView::ACCOUNTS) {
		build_side(*open_game);
	} else if (!still_a_game) {
		close_side();
	}

	start_animation();
}

auto Tray::Native::close_menu() -> void
{
	if (!open) return;

	open      = false;
	animating = false;
	open_game.reset();
	shown_game.reset();
	pending_side.reset();
	side_focused = false;
	slide        = 0.0f;
	drag         = RowDrag{};
	games.hot.reset();
	games.selected.reset();
	side.hot.reset();
	popup.reset();
	wipe_form();
	wrong_password = false;
	shake_seconds  = 0.0f;

	if (!unlocking) {
		clear_password();
	}

	KillTimer(menu_window, K_ANIMATION_TIMER);
	KillTimer(menu_window, K_WATCH_TIMER);
	KillTimer(menu_window, K_CARET_TIMER);
	menu_resolution.restore();
	ShowWindow(menu_window, SW_HIDE);
}

auto Tray::Native::choose(const Row& t_row) -> void
{
	TrayEvent event{.type = t_row.action};

	if (t_row.kind == RowKind::ACCOUNT) {
		if (locked || !t_row.item || *t_row.item >= menu.account_count) return;

		const TrayAccount& account = menu.accounts[*t_row.item];
		event                      = TrayEvent{.type = TrayEventType::QUICK_LOGIN, .game = account.game, .row = account.row};
	}

	close_menu();
	pending_event = event;
}

auto Tray::Native::open_side(u32 t_row, SideView t_view) -> void
{
	if (open_game != t_row) {
		side.scroll = Scroll{};
		side.rows.clear();
		raised.reset();
	}

	open_game      = t_row;
	shown_game     = t_row;
	games.selected = t_row;
	side_view      = t_view;
	drag           = RowDrag{};
	form_hot.reset();
	pending_side.reset();

	build_side(t_row);
	start_animation();
}

auto Tray::Native::close_side() -> void
{
	open_game.reset();
	games.selected.reset();
	pending_side.reset();
	side_focused = false;
	side_view    = SideView::ACCOUNTS;
	drag         = RowDrag{};
	side.hot.reset();
	side.hot_part.reset();
	form_hot.reset();
	popup.reset();
	wipe_form();
	start_animation();
}

auto Tray::Native::point_at_game(std::optional<u32> t_row, D2D1_POINT_2F t_from, D2D1_POINT_2F t_to) -> void
{
	if (side_view == SideView::FORM) {
		pending_side.reset();
		return;
	}

	const bool               is_game = t_row && games.rows[*t_row].kind == RowKind::GAME;
	const std::optional<u32> wanted  = is_game ? t_row : std::nullopt;

	if (wanted == open_game) {
		pending_side.reset();
		return;
	}

	if (open_game && heads_for_side(t_from, t_to)) {
		pending_side = PendingSide{.row = wanted};
		return;
	}

	pending_side.reset();
	show_side_for(wanted);
}

auto Tray::Native::heads_for_side(D2D1_POINT_2F t_from, D2D1_POINT_2F t_to) const -> bool
{
	const float edge = side_left ? side_area.right : side_area.left;

	return inside_triangle(t_to, t_from, D2D1::Point2F(edge, side.frame.top - K_SIDE_AIM_SLOP), D2D1::Point2F(edge, side.frame.bottom + K_SIDE_AIM_SLOP));
}

auto Tray::Native::show_side_for(std::optional<u32> t_row) -> void
{
	if (t_row) {
		open_side(*t_row, SideView::ACCOUNTS);
	} else {
		close_side();
	}
}

auto Tray::Native::leave_side_if_outside(std::optional<D2D1_POINT_2F> t_point) -> void
{
	const bool inside_side = t_point && contains(side_area, t_point->x, t_point->y);
	const bool keep_open   = side_view == SideView::FORM || popup || drag.pressed || side.scroll.grab;

	if (inside_side || keep_open || !open_game) {
		pending_side.reset();
		return;
	}

	if (!pending_side || pending_side->row) {
		pending_side = PendingSide{};
		start_animation();
	}
}

auto Tray::Native::open_add_form() -> void
{
	if (!open_game) return;

	start_form();
	form.game          = menu.games[*games.rows[*open_game].item].game;
	form.visible_games = static_cast<u16>(1u << form.game);

	open_side(*open_game, SideView::FORM);
}

auto Tray::Native::open_edit_form(u32 t_side_row) -> void
{
	const Row& row = side.rows[t_side_row];
	if (!row.item || !fill_form) return;

	const TrayAccount& account = menu.accounts[*row.item];
	TrayAccountForm    filled{};
	fill_form(account.game, account.row, &filled);

	if (filled.editing) {
		start_form();
		form.editing       = true;
		form.game          = filled.game;
		form.row           = filled.row;
		form.region        = region_index(filled.region);
		form.visible_games = filled.visible_games;
		form.fields[FormField::USERNAME].set_value(filled.username);
		form.fields[FormField::PASSWORD].set_value(filled.password);
		form.fields[FormField::NOTE].set_value(filled.note);
		form.regions.selected = static_cast<u32>(form.region);

		side_view = SideView::FORM;
		side.hot.reset();
	}

	SecureZeroMemory(&filled, sizeof(filled));
	start_animation();
}

auto Tray::Native::start_form() -> void
{
	wipe_form();

	for (const FormField field : each<FormField>()) {
		form.fields[field].set_max_length(static_cast<u32>(K_FORM_CAPACITIES[field] - 1));
	}

	form.fields[FormField::PASSWORD].m_masked = true;
	build_region_list();
	caret.restart();
	form_hot.reset();
	caret_epoch = Clock::now();
}

auto Tray::Native::wipe_form() -> void
{
	for (TextEditor& field : form.fields) {
		field.set_value({});
	}

	form = AccountFormState{};
}

auto Tray::Native::close_form() -> void
{
	wipe_form();
	form_hot.reset();
	popup.reset();
	side_view = SideView::ACCOUNTS;

	if (open_game) {
		build_side(*open_game);
	}

	start_animation();
}

auto Tray::Native::submit_form() -> void
{
	const bool missing_username = form.fields[FormField::USERNAME].value().empty();
	const bool missing_password = form.fields[FormField::PASSWORD].value().empty();

	if (missing_username || missing_password) {
		form.show_required = true;
		focus_field(missing_username ? FormField::USERNAME : FormField::PASSWORD);
		return;
	}

	SecureZeroMemory(&saved_form, sizeof(saved_form));
	saved_form = TrayAccountForm{.editing = form.editing, .game = form.game, .row = form.row, .visible_games = form.visible_games};
	copy_to(form.fields[FormField::USERNAME].value(), saved_form.username);
	copy_to(form.fields[FormField::PASSWORD].value(), saved_form.password);
	copy_to(form.fields[FormField::NOTE].value(), saved_form.note);
	copy_to(K_REGION_OPTIONS[form.region].code, saved_form.region);

	pending_event = TrayEvent{.type = TrayEventType::SAVE_ACCOUNT};
	close_form();
}

auto Tray::Native::toggle_favorite(u32 t_side_row) -> void
{
	Row& row = side.rows[t_side_row];
	if (!row.item) return;

	const TrayAccount& account = menu.accounts[*row.item];
	row.favorite               = !row.favorite;
	pending_event              = TrayEvent{.type = TrayEventType::TOGGLE_FAVORITE, .game = account.game, .row = account.row};
	start_animation();
}

auto Tray::Native::toggle_form_game(usize t_index) -> void
{
	if (t_index >= menu.game_count) return;

	const auto bit    = static_cast<u16>(1u << menu.games[t_index].game);
	const bool chosen = (form.visible_games & bit) != 0;

	if (chosen && form.visible_games == bit) return;

	form.visible_games = static_cast<u16>(form.visible_games ^ bit);
}

auto Tray::Native::focus_field(FormField t_field) -> void
{
	if (t_field != form.focus) {
		form.fields[form.focus].release();
		form.focus = t_field;
	}

	edited_text(true);
}

auto Tray::Native::form_key(WPARAM t_key) -> void
{
	const os::Modifiers held = os::modifiers();

	switch (t_key) {
		case VK_ESCAPE: {
			if (form.regions_open) {
				form.regions_open = false;
				start_animation();
			} else {
				close_form();
			}

			break;
		}

		case VK_TAB: {
			focus_field(stepped(form.focus, held.shift ? -1 : 1));
			break;
		}

		case VK_RETURN: {
			submit_form();
			break;
		}

		default: {
			edited_text(form.fields[form.focus].press_key(win32::key_from_virtual_key(t_key), held));
			break;
		}
	}
}

auto Tray::Native::build_region_list() -> void
{
	Panel& list = form.regions;
	list.rows.clear();

	for (const RegionOption& option : K_REGION_OPTIONS) {
		list.rows.push_back(Row{.kind = RowKind::OPTION, .label = win32::to_wide(option.label), .height = K_REGION_ROW_HEIGHT});
	}

	float y = K_PANEL_PADDING;
	for (Row& row : list.rows) {
		row.top = y;
		y += row.height;
	}

	list.content_height = y + K_PANEL_PADDING;
	list.frame          = form_frame.regions;
	list.width          = list.frame.right - list.frame.left;
	list.selected       = static_cast<u32>(form.region);
	fit_rows(&list, list.frame.bottom - list.frame.top);
}

auto Tray::Native::open_regions() -> void
{
	Panel&      list   = form.regions;
	const Row&  chosen = list.rows[form.region];
	const float middle = chosen.top + chosen.height * 0.5f - list.height * 0.5f;

	list.selected      = static_cast<u32>(form.region);
	list.scroll.target = std::clamp(middle, 0.0f, max_scroll(list));
	list.scroll.offset = list.scroll.target;
	list.hot.reset();
	form.regions_open = true;
	start_animation();
}

auto Tray::Native::form_hit(D2D1_POINT_2F t_point) const -> std::optional<FormHit>
{
	const FormLayout& layout = form_frame;
	const float       x      = t_point.x;
	const float       y      = t_point.y;

	if (form.regions_open && contains(form.regions.frame, x, y)) {
		const Panel&             list   = form.regions;
		const std::optional<u32> option = over_scrollbar(list, t_point) ? std::nullopt : row_at(list, y - list.frame.top + list.scroll.offset);
		if (option) return FormHit{.target = FormTarget::REGION_OPTION, .index = *option};

		return std::nullopt;
	}

	if (contains(layout.back, x, y)) return FormHit{.target = FormTarget::BACK};
	if (contains(layout.reveal, x, y)) return FormHit{.target = FormTarget::REVEAL};
	if (contains(layout.region, x, y)) return FormHit{.target = FormTarget::REGION};
	if (contains(layout.cancel, x, y)) return FormHit{.target = FormTarget::CANCEL};
	if (contains(layout.save, x, y)) return FormHit{.target = FormTarget::SAVE};

	for (const FormField field : each<FormField>()) {
		if (contains(layout.fields[field], x, y)) return FormHit{.target = FormTarget::FIELD, .field = field};
	}

	for (const usize index : std::views::iota(usize{0}, layout.games.size())) {
		if (contains(layout.games[index], x, y)) return FormHit{.target = FormTarget::GAME, .index = index};
	}

	return std::nullopt;
}

auto Tray::Native::click_form(D2D1_POINT_2F t_point) -> void
{
	const std::optional<FormHit> hit = form_hit(t_point);

	if (form.regions_open && (!hit || hit->target != FormTarget::REGION_OPTION)) {
		form.regions_open = false;
		start_animation();
		return;
	}

	if (!hit) return;

	switch (hit->target) {
		case FormTarget::BACK:
		case FormTarget::CANCEL: {
			close_form();
			return;
		}

		case FormTarget::FIELD: {
			break;
		}

		case FormTarget::REVEAL: {
			TextEditor& password = form.fields[FormField::PASSWORD];
			password.m_masked    = !password.m_masked;
			break;
		}

		case FormTarget::REGION: {
			open_regions();
			break;
		}

		case FormTarget::REGION_OPTION: {
			form.region           = hit->index;
			form.regions.selected = static_cast<u32>(hit->index);
			form.regions_open     = false;
			break;
		}

		case FormTarget::GAME: {
			toggle_form_game(hit->index);
			break;
		}

		case FormTarget::SAVE: {
			submit_form();
			return;
		}
	}

	start_animation();
}

auto Tray::Native::password_row_box() const -> std::optional<D2D1_RECT_F>
{
	const auto row = std::ranges::find(games.rows, RowKind::PASSWORD, &Row::kind);
	if (!shows_password_field() || row == games.rows.end()) return std::nullopt;

	return D2D1::RectF(K_PANEL_PADDING, row->top, games.frame.right - games.frame.left - K_PANEL_PADDING, row->top + row->height);
}

auto Tray::Native::focused_text() -> std::optional<FocusedText>
{
	if (const std::optional<D2D1_RECT_F> row = password_row_box()) {
		const D2D1_POINT_2F origin = D2D1::Point2F(games.frame.left, games.frame.top);
		return FocusedText{.editor = &password_field, .box = password_text_box(*row, shake_offset()), .scroll = &password_scroll, .origin = origin};
	}

	if (side_view != SideView::FORM || !open_game) return std::nullopt;

	return FocusedText{.editor = &form.fields[form.focus], .box = field_text_box(form_frame, form.focus), .scroll = &form.scroll[form.focus], .origin = D2D1::Point2F()};
}

auto Tray::Native::text_at(D2D1_POINT_2F t_point) -> bool
{
	if (const std::optional<D2D1_RECT_F> row = password_row_box()) {
		const D2D1_RECT_F field = password_field_rect(*row);
		const float       x     = t_point.x - games.frame.left;
		const float       y     = t_point.y - games.frame.top;

		return contains(field, x, y) && !contains(password_submit_rect(field), x, y);
	}

	if (side_view != SideView::FORM || !side_is_ready() || form.regions_open) return false;

	const std::optional<FormHit> hit = form_hit(t_point);
	if (!hit || hit->target != FormTarget::FIELD) return false;

	focus_field(hit->field);
	return true;
}

auto Tray::Native::text_index_at(const FocusedText& t_text, float t_x) const -> u32
{
	IDWriteTextFormat*     body = formats.body.Get();
	char                   mask[K_TEXT_INPUT_CAPACITY];
	const std::string_view shown  = t_text.editor->shown_text(mask);
	const float            origin = t_text.origin.x + t_text.box.left - *t_text.scroll;

	for (usize index = 0; index < shown.size();) {
		const usize next   = next_codepoint(shown, index);
		const float before = utf8_width(writer.Get(), body, shown.substr(0, index));
		const float after  = utf8_width(writer.Get(), body, shown.substr(0, next));

		if (t_x < origin + (before + after) * 0.5f) return static_cast<u32>(index);

		index = next;
	}

	return static_cast<u32>(shown.size());
}

auto Tray::Native::follow_caret(float t_seconds) -> bool
{
	const std::optional<FocusedText> text = focused_text();
	if (!text) return false;

	IDWriteTextFormat*     body = formats.body.Get();
	char                   mask[K_TEXT_INPUT_CAPACITY];
	const std::string_view shown   = text->editor->shown_text(mask);
	const float            visible = std::max(0.0f, text->box.right - text->box.left - K_CARET_WIDTH);
	const float            caret_x = utf8_width(writer.Get(), body, shown.substr(0, text->editor->cursor()));
	const Vec2             size    = caret_size(writer.Get(), body, shown, text->editor->cursor());
	float&                 scroll  = *text->scroll;

	scroll = std::clamp(scroll, caret_x - visible, caret_x);
	scroll = std::clamp(scroll, 0.0f, std::max(0.0f, utf8_width(writer.Get(), body, shown) - visible));

	const Vec2 target{text->box.left + caret_x - scroll, text->box.bottom - size.y};
	caret.follow(target, Vec2{}, K_TEXT_LINE_HEIGHT, t_seconds, true);

	return !caret.is_settled();
}

auto Tray::Native::caret_look() -> CaretLook
{
	const std::optional<FocusedText> text = focused_text();
	if (!text || shows_password_field()) return CaretLook{};

	return CaretLook{.editor = text->editor, .motion = &caret, .text = text->box, .scroll = *text->scroll, .blinked = caret_shown()};
}

auto Tray::Native::edited_text(bool t_changed) -> void
{
	if (!t_changed) return;

	wrong_password = false;
	caret_epoch    = Clock::now();
	start_animation();
}

auto Tray::Native::press_text(D2D1_POINT_2F t_point) -> void
{
	if (unlocking || !text_at(t_point)) return;

	const std::optional<FocusedText> text = focused_text();
	if (!text) return;

	text->editor->press_at(text_index_at(*text, t_point.x), t_point.x);
	SetCapture(menu_window);
	edited_text(true);
}

auto Tray::Native::select_text(D2D1_POINT_2F t_point) -> void
{
	if (const std::optional<FocusedText> text = focused_text()) {
		edited_text(text->editor->drag_to(text_index_at(*text, t_point.x)));
	}
}

auto Tray::Native::finish_text_selection() -> void
{
	if (const std::optional<FocusedText> text = focused_text()) {
		text->editor->release();
	}

	ReleaseCapture();
}

auto Tray::Native::is_selecting_text() const -> bool
{
	if (shows_password_field()) return password_field.m_selecting;

	return side_view == SideView::FORM && form.fields[form.focus].m_selecting;
}

auto Tray::Native::type_text(wchar_t t_unit) -> void
{
	const std::optional<FocusedText> text = focused_text();
	if (!text || unlocking) return;

	if (IS_HIGH_SURROGATE(t_unit)) {
		high_surrogate = t_unit;
		return;
	}

	const wchar_t pair[]{std::exchange(high_surrogate, L'\0'), t_unit};
	const auto    units = IS_LOW_SURROGATE(t_unit) ? std::wstring_view{pair, std::size(pair)} : std::wstring_view{&t_unit, 1};

	char      utf8[K_UTF8_MAX_BYTES]{};
	const int length = WideCharToMultiByte(CP_UTF8, 0, units.data(), static_cast<int>(units.size()), utf8, static_cast<int>(std::size(utf8)), nullptr, nullptr);
	if (length <= 0) return;

	edited_text(text->editor->type(decode_utf8(std::string_view{utf8, static_cast<usize>(length)}, 0).value));
}

auto Tray::Native::open_popup(std::span<const MenuItem> t_items, D2D1_POINT_2F t_at, std::optional<u32> t_account_row) -> void
{
	const Painter                    popup_painter = painter(scale);
	const std::optional<FocusedText> text          = focused_text();
	const float                      ctrl          = keycap_width(popup_painter, K_SHORTCUT_KEY);
	float                            widest        = 0.0f;

	PopupMenu opened{.items = t_items, .account_row = t_account_row};

	for (const usize index : std::views::iota(usize{0}, t_items.size())) {
		const MenuItem&               item     = t_items[index];
		const std::optional<TextEdit> edit     = text_edit_for(item.action);
		const float                   label    = text_width(writer.Get(), formats.body.Get(), item.label);
		const float                   confirm  = item.destructive ? text_width(writer.Get(), formats.body.Get(), K_CONFIRM_LABEL) : 0.0f;
		const float                   shortcut = item.key.empty() ? 0.0f : K_POPUP_SHORTCUT_GAP + ctrl + K_KEYCAP_GAP + keycap_width(popup_painter, item.key);

		opened.enabled[index] = !edit || (text && text->editor->can_apply(*edit));
		widest                = std::max(widest, std::max(label, confirm) + shortcut);
	}

	const D2D1_RECT_F bounds = shown_game ? D2D1::RectF(std::min(games.frame.left, side_area.left), games.frame.top, std::max(games.frame.right, side_area.right), games.frame.bottom) : games.frame;
	const float       width  = std::ceil(std::max(K_POPUP_MIN_WIDTH, widest + popup_label_inset(t_items) + K_POPUP_LABEL_INSET + K_POPUP_PADDING * 2.0f));
	const float       height = popup_height(t_items);
	const float       left   = std::clamp(t_at.x, bounds.left + K_PANEL_PADDING, std::max(bounds.left + K_PANEL_PADDING, bounds.right - K_PANEL_PADDING - width));
	const float       top    = std::clamp(t_at.y, bounds.top + K_PANEL_PADDING, std::max(bounds.top + K_PANEL_PADDING, bounds.bottom - K_PANEL_PADDING - height));

	opened.box = D2D1::RectF(left, top, left + width, top + height);
	popup      = opened;
	start_animation();
}

auto Tray::Native::close_popup() -> void
{
	popup.reset();
	start_animation();
}

auto Tray::Native::popup_item_at(D2D1_POINT_2F t_point) const -> std::optional<usize>
{
	if (!popup) return std::nullopt;

	for (const usize index : std::views::iota(usize{0}, popup->items.size())) {
		if (popup->enabled[index] && contains(popup_row(*popup, index), t_point.x, t_point.y)) return index;
	}

	return std::nullopt;
}

auto Tray::Native::choose_popup_item(D2D1_POINT_2F t_point) -> void
{
	const std::optional<usize> item = popup_item_at(t_point);

	if (item && popup->items[*item].destructive && popup->armed != item) {
		popup->armed         = item;
		popup->armed_seconds = K_CONFIRM_SECONDS;
		start_animation();
		return;
	}

	const PopupMenu chosen = *popup;
	close_popup();

	if (item) {
		run_menu_action(chosen.items[*item].action, chosen.account_row);
	}
}

auto Tray::Native::run_menu_action(MenuAction t_action, std::optional<u32> t_account_row) -> void
{
	if (const std::optional<TextEdit> edit = text_edit_for(t_action)) {
		if (const std::optional<FocusedText> text = focused_text()) {
			edited_text(text->editor->edit(*edit));
		}

		return;
	}

	if (!t_account_row || *t_account_row >= side.rows.size() || !side.rows[*t_account_row].item) return;

	const TrayAccount& account = menu.accounts[*side.rows[*t_account_row].item];

	switch (t_action) {
		case MenuAction::EDIT_ACCOUNT: {
			open_edit_form(*t_account_row);
			break;
		}

		case MenuAction::DELETE_ACCOUNT: {
			pending_event = TrayEvent{.type = TrayEventType::REMOVE_ACCOUNT, .game = account.game, .row = account.row};
			break;
		}

		case MenuAction::COPY_USERNAME: {
			pending_event = TrayEvent{.type = TrayEventType::COPY_USERNAME, .game = account.game, .row = account.row};
			break;
		}

		case MenuAction::COPY_PASSWORD: {
			pending_event = TrayEvent{.type = TrayEventType::COPY_PASSWORD, .game = account.game, .row = account.row};
			break;
		}

		default: {
			break;
		}
	}
}

auto Tray::Native::row_at(const Panel& t_panel, float t_y) const -> std::optional<u32>
{
	for (const u32 i : std::views::iota(0u, static_cast<u32>(t_panel.rows.size()))) {
		const Row& row = t_panel.rows[i];
		if (t_y >= row.top && t_y < row.top + row.height) return i;
	}

	return std::nullopt;
}

auto Tray::Native::side_is_ready() const -> bool
{
	return open_game.has_value() && slide >= 1.0f;
}

auto Tray::Native::panel_at(D2D1_POINT_2F t_point) -> Panel*
{
	if (side_is_ready() && side_view == SideView::ACCOUNTS && contains(side.frame, t_point.x, t_point.y)) return &side;
	if (contains(games.frame, t_point.x, t_point.y)) return &games;

	return nullptr;
}

auto Tray::Native::side_row_right() const -> float
{
	return side.frame.right - side.frame.left - K_PANEL_PADDING - side.gutter - K_ROW_INSET;
}

auto Tray::Native::side_part_at(u32 t_row, float t_x, float t_content_y) const -> RowPart
{
	return account_part_at(account_row_parts(side.rows[t_row], side_row_right(), scale), t_x, t_content_y);
}

auto Tray::Native::side_header() const -> D2D1_RECT_F
{
	return D2D1::RectF(side_area.left, side_area.top, side_area.right, side_area.top + K_SIDE_HEADER_HEIGHT);
}

auto Tray::Native::over_add_button(D2D1_POINT_2F t_point) const -> bool
{
	return side_is_ready() && side_view == SideView::ACCOUNTS && contains(add_button_rect(side_header(), add_label_width, scale), t_point.x, t_point.y);
}

auto Tray::Native::menu_point(LPARAM t_lparam) const -> D2D1_POINT_2F
{
	return D2D1::Point2F(static_cast<float>(static_cast<short>(LOWORD(t_lparam))) / scale, static_cast<float>(static_cast<short>(HIWORD(t_lparam))) / scale);
}

auto Tray::Native::cursor_point() const -> D2D1_POINT_2F
{
	POINT cursor;
	GetCursorPos(&cursor);
	ScreenToClient(menu_window, &cursor);

	return D2D1::Point2F(static_cast<float>(cursor.x) / scale, static_cast<float>(cursor.y) / scale);
}

auto Tray::Native::render_menu() -> void
{
	if (!prepare_canvas(&menu_canvas, factory.Get(), menu_size, scale)) return;

	ID2D1DCRenderTarget* target = menu_canvas.target.Get();

	if (!images.logo) {
		images.logo = make_bitmap(target, logo_source, K_LOGO_SIZE, 0.0f, scale);
	}

	for (const Row& row : games.rows) {
		if (!row.icon || *row.icon >= K_TRAY_MAX_GAMES || images.game_icons[*row.icon]) continue;

		images.game_icons[*row.icon] = make_bitmap(target, game_icon_sources[*row.icon], K_ICON_SIZE, K_ICON_RADIUS, scale);
	}

	std::wstring title;
	std::wstring subtitle;
	ID2D1Bitmap* icon = nullptr;

	if (shown_game) {
		const TrayGame& game = menu.games[*games.rows[*shown_game].item];
		title                = win32::to_wide(game.title);
		subtitle             = game.account_count == 1 ? std::wstring{L"1 account"} : std::to_wstring(game.account_count) + L" accounts";
		icon                 = game.game < K_TRAY_MAX_GAMES ? images.game_icons[game.game].Get() : nullptr;
	}

	const FormView form_view{
		.state       = form,
		.layout      = form_frame,
		.hot         = form_hot,
		.games       = std::span{menu.games, menu.game_count},
		.game_labels = game_labels,
		.images      = images,
		.caret       = caret_look(),
	};
	const MenuScene scene{
		.games     = games,
		.side      = side,
		.images    = images,
		.badge     = badge,
		.side_area = side_area,
		.slide     = slide,
		.side_left = side_left,
		.view      = side_view,
		.header    = SideHeader{.box = side_header(), .icon = icon, .title = title, .subtitle = subtitle, .add_label_width = add_label_width, .add_hot = add_hot},
		.raised    = raised_row(),
		.form      = form_view,
		.popup     = popup ? &*popup : nullptr,
	};

	target->BeginDraw();
	paint_menu(target, painter(scale), scene);

	if (target->EndDraw() == D2DERR_RECREATE_TARGET) {
		menu_canvas.target.Reset();
		images = MenuImages{};
		return;
	}

	present_canvas(menu_canvas, menu_window, menu_origin, eased_out(appear));
}

auto Tray::Native::menu_cursor() const -> LPCWSTR
{
	if (popup) return popup->hot ? IDC_HAND : IDC_ARROW;

	const bool over_field = side_view == SideView::FORM && form_hot && form_hot->target == FormTarget::FIELD;
	if (is_selecting_text() || over_field) return IDC_IBEAM;

	const bool over_row  = side.hot.has_value() && side.hot_part.has_value();
	const bool form_part = side_view == SideView::FORM && form_hot.has_value();

	return update_hot || over_row || form_part || add_hot ? IDC_HAND : IDC_ARROW;
}

auto Tray::Native::on_pointer(LPARAM t_lparam) -> void
{
	if (!tracking_leave) {
		TRACKMOUSEEVENT track{.cbSize = sizeof(TRACKMOUSEEVENT), .dwFlags = TME_LEAVE, .hwndTrack = menu_window};
		tracking_leave = TrackMouseEvent(&track) != FALSE;
	}

	const D2D1_POINT_2F point    = menu_point(t_lparam);
	const D2D1_POINT_2F previous = last_pointer;
	last_pointer                 = point;

	if (side.scroll.grab) {
		drag_thumb(point.y - side.frame.top);
		return;
	}

	if (drag.pressed) {
		drag_row(point.y - side.frame.top + side.scroll.offset);
		return;
	}

	if (is_selecting_text()) {
		select_text(point);
		return;
	}

	if (popup) {
		popup->hot = popup_item_at(point);
		SetCursor(LoadCursorW(nullptr, menu_cursor()));
		start_animation();
		return;
	}

	const Panel* panel = panel_at(point);
	update_hot         = panel == &games && over_update(point.x - games.frame.left, point.y - games.frame.top);
	add_hot            = over_add_button(point);

	if (panel == &games) {
		const std::optional<u32> row = row_at(games, point.y - games.frame.top);
		games.hot                    = row && is_selectable(games.rows[*row]) ? row : std::nullopt;
		side_focused                 = false;
		point_at_game(row, previous, point);
	} else {
		leave_side_if_outside(point);

		if (!side_focused) {
			games.hot.reset();
		}
	}

	point_at_side(panel == &side ? std::optional{point} : std::nullopt);
	form_hot = side_view == SideView::FORM && side_is_ready() ? form_hit(point) : std::nullopt;

	if (form.regions_open) {
		const std::optional<FormHit> option = form_hot && form_hot->target == FormTarget::REGION_OPTION ? form_hot : std::nullopt;
		form.regions.hot                    = option ? std::optional{static_cast<u32>(option->index)} : std::nullopt;
		form.regions.scroll.over_bar        = contains(form.regions.frame, point.x, point.y) && over_scrollbar(form.regions, point);
	}

	SetCursor(LoadCursorW(nullptr, menu_cursor()));
	start_animation();
}

auto Tray::Native::point_at_side(std::optional<D2D1_POINT_2F> t_point) -> void
{
	side.scroll.over_bar   = false;
	side.scroll.over_thumb = false;

	if (!t_point) {
		if (!side_focused) {
			side.hot.reset();
		}

		side.hot_part.reset();
		return;
	}

	const bool               bar   = over_scrollbar(side, *t_point);
	const float              x     = t_point->x - side.frame.left;
	const float              y     = t_point->y - side.frame.top + side.scroll.offset;
	const std::optional<u32> row   = bar ? std::nullopt : row_at(side, y);
	const D2D1_RECT_F        thumb = bar ? thumb_rect(side) : D2D1_RECT_F{};
	const bool               lit   = row && is_selectable(side.rows[*row]);

	side.hot               = lit ? row : std::nullopt;
	side.hot_part          = lit ? std::optional{side_part_at(*row, x, y)} : std::nullopt;
	side.scroll.over_bar   = bar;
	side.scroll.over_thumb = bar && t_point->y - side.frame.top >= thumb.top && t_point->y - side.frame.top < thumb.bottom;
	side_focused           = false;
}

auto Tray::Native::on_leave() -> void
{
	tracking_leave = false;
	update_hot     = false;
	add_hot        = false;
	leave_side_if_outside(std::nullopt);
	form_hot.reset();
	point_at_side(std::nullopt);

	if (!side_focused) {
		games.hot.reset();
	}

	start_animation();
}

auto Tray::Native::on_press(LPARAM t_lparam) -> void
{
	const D2D1_POINT_2F point = menu_point(t_lparam);
	if (popup) return;

	press_text(point);
	if (is_selecting_text() || panel_at(point) != &side) return;

	if (over_scrollbar(side, point)) {
		const D2D1_RECT_F thumb = thumb_rect(side);
		const float       y     = point.y - side.frame.top;
		const bool        held  = y >= thumb.top && y < thumb.bottom;

		side.scroll.grab = held ? y - thumb.top : (thumb.bottom - thumb.top) * 0.5f;
		SetCapture(menu_window);
		drag_thumb(y);
		return;
	}

	const float              x   = point.x - side.frame.left;
	const float              y   = point.y - side.frame.top + side.scroll.offset;
	const std::optional<u32> row = row_at(side, y);
	if (!row || side.rows[*row].kind != RowKind::ACCOUNT || side_part_at(*row, x, y) != RowPart::BODY) return;

	const Row& pressed = side.rows[*row];
	drag               = RowDrag{.pressed = row, .press_y = y, .grab = y - pressed.top - pressed.offset, .insert = *row};
	SetCapture(menu_window);
}

auto Tray::Native::on_right_click(LPARAM t_lparam) -> void
{
	const D2D1_POINT_2F point = menu_point(t_lparam);
	popup.reset();

	if (text_at(point)) {
		if (const std::optional<FocusedText> text = focused_text()) {
			text->editor->right_click_at(text_index_at(*text, point.x));
			open_popup(K_TEXT_MENU_ITEMS, point, std::nullopt);
		}

		return;
	}

	if (panel_at(point) != &side) {
		start_animation();
		return;
	}

	const std::optional<u32> row = row_at(side, point.y - side.frame.top + side.scroll.offset);
	if (!row || side.rows[*row].kind != RowKind::ACCOUNT) {
		start_animation();
		return;
	}

	open_popup(K_ACCOUNT_MENU_ITEMS, point, row);
}

auto Tray::Native::drag_slots() const -> std::pair<u32, u32>
{
	const auto favorites = static_cast<u32>(std::ranges::count_if(side.rows, &Row::favorite));
	const auto count     = static_cast<u32>(side.rows.size());

	if (side.rows[*drag.pressed].favorite) return {0, favorites - 1};

	return {favorites, count - 1};
}

auto Tray::Native::drag_row(float t_content_y) -> void
{
	if (!drag.lifted && std::abs(t_content_y - drag.press_y) < K_DRAG_THRESHOLD) return;

	if (!drag.lifted) {
		drag.lifted = true;
		raised      = side.rows[*drag.pressed].id;
		side.hot.reset();
		side.hot_part.reset();
	}

	const auto [first, last] = drag_slots();
	drag.lifted_top          = std::clamp(t_content_y - drag.grab, side.rows[first].top, side.rows[last].top);

	const float slot = std::round((drag.lifted_top - side.rows.front().top) / K_ACCOUNT_ROW_HEIGHT);
	drag.insert      = static_cast<u32>(std::clamp(slot, static_cast<float>(first), static_cast<float>(last)));
	start_animation();
}

auto Tray::Native::row_shift(u32 t_row) const -> float
{
	if (!drag.lifted || t_row == drag.pressed) return 0.0f;

	const u32 from = *drag.pressed;
	const u32 to   = drag.insert;

	if (from < t_row && t_row <= to) return -K_ACCOUNT_ROW_HEIGHT;
	if (to <= t_row && t_row < from) return K_ACCOUNT_ROW_HEIGHT;

	return 0.0f;
}

auto Tray::Native::step_rows(float t_seconds) -> bool
{
	bool moving = false;

	for (const u32 index : std::views::iota(0u, static_cast<u32>(side.rows.size()))) {
		Row&        row    = side.rows[index];
		const float target = row_shift(index);

		row.offset = animation::ease_toward(row.offset, target, K_ROW_SHIFT_EASE_RATE, t_seconds, animation::K_SETTLED_PIXELS);
		moving     = moving || row.offset != target;
	}

	const float lift_target = drag.lifted ? 1.0f : 0.0f;
	lift                    = animation::ease_toward(lift, lift_target, K_ROW_LIFT_EASE_RATE, t_seconds);
	moving                  = moving || lift != lift_target;

	if (raised && !drag.lifted && lift == 0.0f) {
		const auto row = std::ranges::find(side.rows, *raised, &Row::id);

		if (row == side.rows.end() || row->offset == 0.0f) {
			raised.reset();
		}
	}

	return moving;
}

auto Tray::Native::raised_row() const -> std::optional<RaisedRow>
{
	if (!raised) return std::nullopt;

	const auto row = std::ranges::find(side.rows, *raised, &Row::id);
	if (row == side.rows.end()) return std::nullopt;

	const float top = drag.lifted ? drag.lifted_top : row->top + row->offset;

	return RaisedRow{.id = *raised, .top = top, .lift = lift};
}

auto Tray::Native::finish_drag() -> void
{
	const RowDrag dropped = drag;
	drag                  = RowDrag{};
	ReleaseCapture();

	if (!dropped.lifted) {
		choose(side.rows[*dropped.pressed]);
		return;
	}

	const u32 from  = *dropped.pressed;
	const u32 to    = dropped.insert;
	Row&      moved = side.rows[from];
	moved.offset    = dropped.lifted_top - moved.top;

	if (to != from && moved.item) {
		const TrayAccount& account = menu.accounts[*moved.item];
		pending_event              = TrayEvent{.type = TrayEventType::MOVE_ACCOUNT, .game = account.game, .row = account.row, .to_row = to};

		const std::vector<ShownRow> before = shown_rows(side);
		const auto                  rows   = side.rows.begin();

		if (from < to) {
			std::rotate(rows + from, rows + from + 1, rows + to + 1);
		} else {
			std::rotate(rows + to, rows + from, rows + from + 1);
		}

		lay_out_side();
		slide_from(&side, before);
	}

	start_animation();
}

auto Tray::Native::on_click(LPARAM t_lparam) -> void
{
	const D2D1_POINT_2F point = menu_point(t_lparam);

	if (popup) {
		if (contains(popup->box, point.x, point.y)) {
			choose_popup_item(point);
		} else {
			close_popup();
		}

		return;
	}

	if (over_add_button(point)) {
		open_add_form();
		return;
	}

	if (side_view == SideView::FORM && side_is_ready() && contains(side_area, point.x, point.y)) {
		click_form(point);
		return;
	}

	Panel* panel = panel_at(point);
	if (panel == nullptr) return;

	const float              x   = point.x - panel->frame.left;
	const float              y   = point.y - panel->frame.top + panel->scroll.offset;
	const std::optional<u32> row = row_at(*panel, y);
	if (!row) return;

	if (panel == &side) {
		if (side.rows[*row].kind == RowKind::ACCOUNT && side_part_at(*row, x, y) == RowPart::STAR) {
			toggle_favorite(*row);
		}

		return;
	}

	const Row& clicked = games.rows[*row];

	if (clicked.kind == RowKind::BRAND && over_update(x, y)) {
		press_update();
	} else if (clicked.kind == RowKind::PASSWORD && over_password_submit(clicked, x, y)) {
		submit_password();
	} else if (clicked.kind == RowKind::GAME) {
		open_side(*row, SideView::ACCOUNTS);
	} else if (clicked.kind == RowKind::ACTION) {
		choose(clicked);
	}
}

auto Tray::Native::over_scrollbar(const Panel& t_panel, D2D1_POINT_2F t_point) const -> bool
{
	return t_panel.gutter > 0.0f && t_point.x >= t_panel.frame.right - K_PANEL_PADDING - t_panel.gutter;
}

auto Tray::Native::scroll_panel(Panel* t_panel, float t_delta) -> void
{
	if (t_panel->gutter <= 0.0f || t_panel->scroll.grab) return;

	t_panel->scroll.target = std::clamp(t_panel->scroll.target + t_delta, 0.0f, max_scroll(*t_panel));
	t_panel->scroll.awake  = K_SCROLL_AWAKE_SECONDS;
	start_animation();
}

auto Tray::Native::drag_thumb(float t_y) -> void
{
	const D2D1_RECT_F thumb  = thumb_rect(side);
	const float       travel = side.height - K_PANEL_PADDING * 2.0f - (thumb.bottom - thumb.top);
	const float       top    = t_y - *side.scroll.grab - K_PANEL_PADDING;

	side.scroll.offset = std::clamp(top / travel * max_scroll(side), 0.0f, max_scroll(side));
	side.scroll.target = side.scroll.offset;
	start_animation();
}

auto Tray::Native::update_box() const -> D2D1_RECT_F
{
	return update_rect(games.rows.front(), games.frame.right - games.frame.left, badge_width(painter(scale), badge.state), scale);
}

auto Tray::Native::over_update(float t_x, float t_y) const -> bool
{
	return badge.state != TrayUpdate::NONE && !games.rows.empty() && games.rows.front().kind == RowKind::BRAND && contains(update_box(), t_x, t_y);
}

auto Tray::Native::press_update() -> void
{
	close_menu();
	pending_event = TrayEvent{.type = TrayEventType::SHOW_UPDATES};
}

auto Tray::Native::step_badge(float t_seconds, float t_blend) -> bool
{
	const bool  installing = update == TrayUpdate::INSTALLING;
	const float hover      = update_hot ? 1.0f : 0.0f;

	if (update != badge.state) {
		badge.state = update;
		badge.shown = 0.0f;
	}

	if (installing) {
		const float turns = badge.spin + t_seconds / K_SPIN_SECONDS;
		badge.spin        = turns - std::floor(turns);
	}

	badge.shown = approached(badge.shown, 1.0f, t_seconds / K_UPDATE_IN_SECONDS);
	badge.hover += (hover - badge.hover) * t_blend;

	const bool settled = std::abs(hover - badge.hover) < K_SETTLED;
	if (settled) {
		badge.hover = hover;
	}

	return installing || badge.shown < 1.0f || !settled;
}

auto Tray::Native::on_key(WPARAM t_key) -> void
{
	if (popup) {
		if (t_key == VK_ESCAPE) {
			close_popup();
		}

		return;
	}

	if (shows_password_field() && edit_password(t_key)) return;

	if (side_view == SideView::FORM && open_game) {
		form_key(t_key);
		return;
	}

	Panel* panel = side_focused ? &side : &games;

	switch (t_key) {
		case VK_ESCAPE: {
			if (open_game) {
				close_side();
			} else {
				close_menu();
			}

			break;
		}

		case VK_LEFT:
		case VK_RIGHT: {
			const bool toward_side = (t_key == VK_LEFT) == side_left;

			if (toward_side && !side_focused) {
				enter_side();
			} else if (!toward_side && side_focused) {
				side_focused = false;
				side.hot.reset();
				start_animation();
			}

			break;
		}

		case VK_UP:
		case VK_DOWN: {
			step_hot(panel, t_key == VK_DOWN ? 1 : -1);
			break;
		}

		case VK_RETURN:
		case VK_SPACE: {
			if (!panel->hot) break;

			const Row& row = panel->rows[*panel->hot];

			if (row.kind == RowKind::GAME) {
				enter_side();
			} else if (is_selectable(row)) {
				choose(row);
			}

			break;
		}

		default: {
			break;
		}
	}
}

auto Tray::Native::enter_side() -> void
{
	if (!games.hot || games.rows[*games.hot].kind != RowKind::GAME) return;

	if (open_game != games.hot || side_view != SideView::ACCOUNTS) {
		open_side(*games.hot, SideView::ACCOUNTS);
	}

	side_focused = true;
	side.hot.reset();
	step_hot(&side, 1);
}

auto Tray::Native::step_hot(Panel* t_panel, i32 t_step) -> void
{
	const auto count = static_cast<u32>(t_panel->rows.size());
	if (count == 0) return;

	const u32 step = t_step > 0 ? 1 : count - 1;
	u32       row  = t_panel->hot ? *t_panel->hot : (t_step > 0 ? count - 1 : 0);

	for (u32 tries = 0; tries < count; tries += 1) {
		row = (row + step) % count;

		if (is_selectable(t_panel->rows[row])) {
			t_panel->hot = row;
			break;
		}
	}

	if (t_panel->hot && t_panel->gutter > 0.0f) {
		const Row&  lit    = t_panel->rows[*t_panel->hot];
		const float view   = t_panel->height - K_PANEL_PADDING * 2.0f;
		float       target = t_panel->scroll.target;

		target                 = std::min(target, lit.top - K_PANEL_PADDING);
		target                 = std::max(target, lit.top + lit.height - K_PANEL_PADDING - view);
		t_panel->scroll.target = std::clamp(target, 0.0f, max_scroll(*t_panel));
	}

	start_animation();
}

// The menu normally closes when it loses focus. If Windows didn't let it take focus when it opened, a click anywhere else closes it instead.
auto Tray::Native::watch() -> void
{
	if (GetForegroundWindow() == menu_window || side.scroll.grab || drag.pressed || is_selecting_text()) return;

	if (activated) {
		close_menu();
		return;
	}

	const bool pressed = ((GetAsyncKeyState(VK_LBUTTON) | GetAsyncKeyState(VK_RBUTTON) | GetAsyncKeyState(VK_MBUTTON)) & 0x8000) != 0;
	if (!pressed) return;

	POINT cursor;
	GetCursorPos(&cursor);

	if (WindowFromPoint(cursor) != menu_window) {
		close_menu();
	}
}

// Windows only sends timer messages while no input is waiting, so a pointer moving over the menu would hold the timer back and freeze the animations. Every change steps them by the time that has passed, whether the timer or the pointer asks.
auto Tray::Native::start_animation() -> void
{
	if (!open) return;

	if (!animating) {
		animating = true;
		last_tick = Clock::now();
		menu_resolution.refine();
		SetTimer(menu_window, K_ANIMATION_TIMER, K_ANIMATION_TICK_MS, nullptr);
	}

	animate();
}

auto Tray::Native::step_games(float t_seconds) -> bool
{
	bool moving = false;

	for (const u32 index : std::views::iota(0u, static_cast<u32>(games.rows.size()))) {
		Row&        row    = games.rows[index];
		const float target = games.selected == index ? 1.0f : 0.0f;

		row.turn = animation::ease_toward(row.turn, target, K_CHEVRON_TURN_RATE, t_seconds);
		moving   = moving || row.turn != target;
	}

	if (pending_side) {
		pending_side->seconds += t_seconds;
		moving = true;

		if (pending_side->seconds >= K_SIDE_AIM_SECONDS) {
			const std::optional<u32> row = pending_side->row;
			pending_side.reset();
			show_side_for(row);
		}
	}

	return moving;
}

auto Tray::Native::animate() -> void
{
	const Clock::time_point now     = Clock::now();
	const float             seconds = std::min(std::chrono::duration<float>(now - last_tick).count(), K_LONGEST_FRAME_SECONDS);
	const float             blend   = 1.0f - std::exp(-K_HOVER_EASE_RATE * seconds);
	const bool              showing = open_game.has_value();
	const float             target  = showing ? 1.0f : 0.0f;
	bool                    moving  = false;

	last_tick = now;
	appear    = approached(appear, 1.0f, seconds / K_MENU_IN_SECONDS);
	slide     = approached(slide, target, seconds / (showing ? K_SIDE_OPEN_SECONDS : K_SIDE_CLOSE_SECONDS));
	moving    = appear < 1.0f || slide != target;
	moving    = step_badge(seconds, blend) || moving;

	if (!showing && slide <= 0.0f) {
		shown_game.reset();
	}

	moving = step_scroll(&side.scroll, seconds) || moving;
	moving = step_scroll(&form.regions.scroll, seconds) || moving;

	const auto ease = [&](Panel* t_panel) {
		for (const u32 i : std::views::iota(0u, static_cast<u32>(t_panel->rows.size()))) {
			Row&        row     = t_panel->rows[i];
			const float lit     = i == t_panel->hot && is_selectable(row) ? 1.0f : 0.0f;
			const bool  settled = std::abs(lit - row.hover) < K_SETTLED;

			row.hover = settled ? lit : row.hover + (lit - row.hover) * blend;
			moving    = moving || !settled;
		}
	};

	ease(&games);
	ease(&side);
	ease(&form.regions);

	moving = step_games(seconds) || moving;
	moving = step_rows(seconds) || moving;
	moving = follow_caret(seconds) || moving;

	if (popup && popup->armed) {
		popup->armed_seconds -= seconds;
		moving = true;

		if (popup->armed_seconds <= 0.0f) {
			popup->armed.reset();
		}
	}

	spin_seconds += unlocking ? seconds : 0.0f;
	shake_seconds = std::max(0.0f, shake_seconds - seconds);
	moving        = moving || unlocking || shake_seconds > 0.0f;

	render_menu();

	if (!moving) {
		animating = false;
		KillTimer(menu_window, K_ANIMATION_TIMER);
		menu_resolution.restore();
	}

	if (focused_text()) {
		SetTimer(menu_window, K_CARET_TIMER, static_cast<UINT>(K_CARET_BLINK_SECONDS * 1000.0f), nullptr);
	}
}

auto Tray::Native::shows_password_field() const -> bool
{
	return open && locked && menu.can_unlock;
}

auto Tray::Native::caret_shown() const -> bool
{
	const bool typing = shows_password_field() ? !unlocking : open && side_view == SideView::FORM;
	if (!typing) return false;

	const float since_typing = std::chrono::duration<float>(Clock::now() - caret_epoch).count();

	return std::fmod(since_typing, K_CARET_BLINK_SECONDS * 2.0f) < K_CARET_BLINK_SECONDS;
}

auto Tray::Native::shake_offset() const -> float
{
	if (shake_seconds <= 0.0f) return 0.0f;

	const float remaining = shake_seconds / K_SHAKE_SECONDS;

	return std::sin((1.0f - remaining) * K_SHAKE_TURNS * 2.0f * std::numbers::pi_v<float>) * K_SHAKE_DISTANCE * remaining;
}

auto Tray::Native::over_password_submit(const Row& t_row, float t_x, float t_y) const -> bool
{
	const D2D1_RECT_F row{K_PANEL_PADDING, t_row.top, games.frame.right - games.frame.left - K_PANEL_PADDING, t_row.top + t_row.height};

	return contains(password_submit_rect(password_field_rect(row)), t_x, t_y);
}

auto Tray::Native::edit_password(WPARAM t_key) -> bool
{
	if (unlocking) return true;

	if (t_key == VK_RETURN) {
		submit_password();
		return true;
	}

	if (t_key == VK_ESCAPE) return false;

	edited_text(password_field.press_key(win32::key_from_virtual_key(t_key), os::modifiers()));
	return true;
}

auto Tray::Native::on_char(wchar_t t_character) -> void
{
	if (!popup) {
		type_text(t_character);
	}
}

auto Tray::Native::clear_password() -> void
{
	password_field.set_value({});
	password_field.m_masked = true;
	password_field.set_max_length(K_TRAY_PASSWORD_LENGTH);
	password_scroll = 0.0f;
}

auto Tray::Native::submit_password() -> void
{
	if (unlocking || password_field.value().empty()) return;

	unlocking      = true;
	wrong_password = false;
	spin_seconds   = 0.0f;
	pending_event  = TrayEvent{.type = TrayEventType::UNLOCK};
	start_animation();
}

auto Tray::Native::add_icon() -> bool
{
	if (icon_added) return true;

	NOTIFYICONDATAW data{
		.cbSize           = sizeof(NOTIFYICONDATAW),
		.hWnd             = window,
		.uID              = K_TRAY_ICON_ID,
		.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP,
		.uCallbackMessage = K_TRAY_CALLBACK_MESSAGE,
		.hIcon            = shown_icon(),
	};
	fill_tooltip(data.szTip);

	add_attempts += 1;

	if (Shell_NotifyIconW(NIM_ADD, &data)) {
		icon_added = true;
		KillTimer(window, K_ADD_ICON_RETRY_TIMER);
		debug_log::write(K_LOG_CATEGORY, "tray icon added on attempt %u", add_attempts);
		return true;
	}

	debug_log::write(K_LOG_CATEGORY, "Shell_NotifyIcon(NIM_ADD) failed on attempt %u, err=%lu", add_attempts, GetLastError());

	if (add_attempts < K_MAX_ADD_ICON_ATTEMPTS) {
		SetTimer(window, K_ADD_ICON_RETRY_TIMER, K_ADD_ICON_RETRY_INTERVAL_MS, nullptr);
	} else {
		KillTimer(window, K_ADD_ICON_RETRY_TIMER);
		debug_log::write(K_LOG_CATEGORY, "giving up on the tray icon after %u attempts", add_attempts);
	}

	return false;
}

auto Tray::Native::remove_icon() -> void
{
	if (!icon_added) return;

	NOTIFYICONDATAW data{.cbSize = sizeof(NOTIFYICONDATAW), .hWnd = window, .uID = K_TRAY_ICON_ID};
	Shell_NotifyIconW(NIM_DELETE, &data);
	icon_added = false;
}

auto Tray::Native::handle_message(UINT t_message, WPARAM t_wparam, LPARAM t_lparam) -> LRESULT
{
	if (t_message == taskbar_created_message()) {
		debug_log::write(K_LOG_CATEGORY, "Explorer restarted - re-adding the tray icon");
		icon_added   = false;
		add_attempts = 0;
		add_icon();
		return 0;
	}

	switch (t_message) {
		case K_TRAY_CALLBACK_MESSAGE: {
			if (LOWORD(t_lparam) == WM_LBUTTONUP) {
				close_menu();
				pending_event = TrayEvent{.type = TrayEventType::SHOW_WINDOW};
			} else if (LOWORD(t_lparam) == WM_RBUTTONUP || LOWORD(t_lparam) == WM_CONTEXTMENU) {
				POINT cursor;
				GetCursorPos(&cursor);
				show_menu_at(cursor);
			}

			return 0;
		}

		case WM_TIMER: {
			if (t_wparam == K_ADD_ICON_RETRY_TIMER) {
				add_icon();
			}

			return 0;
		}

		default: {
			break;
		}
	}

	return DefWindowProcW(window, t_message, t_wparam, t_lparam);
}

auto Tray::Native::handle_menu_message(UINT t_message, WPARAM t_wparam, LPARAM t_lparam) -> LRESULT
{
	switch (t_message) {
		case WM_ACTIVATE: {
			if (LOWORD(t_wparam) != WA_INACTIVE) {
				activated = true;
			} else {
				close_menu();
			}

			break;
		}

		case WM_MOUSEMOVE: {
			on_pointer(t_lparam);
			return 0;
		}

		case WM_MOUSELEAVE: {
			on_leave();
			return 0;
		}

		case WM_LBUTTONDOWN: {
			on_press(t_lparam);
			return 0;
		}

		case WM_LBUTTONUP: {
			if (side.scroll.grab) {
				ReleaseCapture();
			} else if (drag.pressed) {
				finish_drag();
			} else if (is_selecting_text()) {
				finish_text_selection();
			} else {
				on_click(t_lparam);
			}

			return 0;
		}

		case WM_RBUTTONUP: {
			on_right_click(t_lparam);
			return 0;
		}

		case WM_CAPTURECHANGED: {
			side.scroll.grab.reset();
			drag = RowDrag{};
			password_field.release();
			form.fields[form.focus].release();
			start_animation();
			return 0;
		}

		case WM_MOUSEWHEEL: {
			const float notches = static_cast<float>(GET_WHEEL_DELTA_WPARAM(t_wparam)) / static_cast<float>(WHEEL_DELTA);

			const float delta = -notches * K_PIXELS_PER_NOTCH;

			if (form.regions_open) {
				scroll_panel(&form.regions, delta);
			} else if (panel_at(cursor_point()) == &side) {
				scroll_panel(&side, delta);
			}

			return 0;
		}

		case WM_SETCURSOR: {
			SetCursor(LoadCursorW(nullptr, menu_cursor()));
			return TRUE;
		}

		case WM_KEYDOWN: {
			on_key(t_wparam);
			return 0;
		}

		case WM_CHAR: {
			on_char(static_cast<wchar_t>(t_wparam));
			return 0;
		}

		case WM_TIMER: {
			if (t_wparam == K_ANIMATION_TIMER) {
				animate();
			} else if (t_wparam == K_CARET_TIMER) {
				render_menu();
			} else if (t_wparam == K_WATCH_TIMER) {
				watch();
			}

			return 0;
		}

		default: {
			break;
		}
	}

	return DefWindowProcW(menu_window, t_message, t_wparam, t_lparam);
}

auto Tray::Native::ensure_toast() -> bool
{
	if (toast.window != nullptr) return true;
	if (!ensure_popup()) return false;

	// The toast never takes focus, which would pull the game or the Riot Client out of the front.
	toast.window = popup_window(WS_EX_NOACTIVATE);
	if (toast.window == nullptr) {
		debug_log::write(K_LOG_CATEGORY, "failed to create the login toast, err=%lu", GetLastError());
		return false;
	}

	return true;
}

auto Tray::Native::show_login(const TrayLogin& t_login) -> void
{
	if (!ensure_toast()) return;

	const bool starting     = !toast.active;
	const bool has_finished = t_login.state != TrayLoginState::RUNNING;
	const bool just_ended   = has_finished && (starting || t_login.state != toast.login.state);

	if (starting) {
		begin_toast();
	}

	if (starting || t_login.game != toast.login.game) {
		toast.game_icon.Reset();
	}

	std::wstring account    = shortened_name(win32::to_wide(t_login.account));
	std::wstring step_label = win32::to_wide(shown_step(t_login).label);
	std::wstring message    = win32::to_wide(t_login.message);
	const bool   new_text   = account != toast.account || step_label != toast.step_label || message != toast.message;

	toast.login                = t_login;
	toast.account              = std::move(account);
	toast.step_label           = std::move(step_label);
	toast.message              = std::move(message);
	toast.updated_at           = Clock::now();
	toast.seconds_since_update = 0.0f;
	toast.leaving              = false;

	if (starting || new_text) {
		measure_toast_text();
	}

	if (just_ended) {
		toast.linger = offers_retry(toast) || !toast.message.empty() ? K_TOAST_ERROR_SECONDS : K_TOAST_SUCCESS_SECONDS;
	}

	if (starting) {
		toast.details  = has_finished && !toast.message.empty() ? 1.0f : 0.0f;
		toast.finished = has_finished ? 1.0f : 0.0f;
		toast.width    = toast_target_width(toast);

		render_toast();
		ShowWindow(toast.window, SW_SHOWNOACTIVATE);
		raise_toast();
		SetTimer(toast.window, K_TOAST_TIMER, K_TOAST_TICK_MS, nullptr);
	}
}

auto Tray::Native::begin_toast() -> void
{
	const HMONITOR monitor = MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
	MONITORINFO    info{.cbSize = sizeof(MONITORINFO)};
	GetMonitorInfoW(monitor, &info);

	toast.scale     = monitor_scale(monitor);
	toast.work      = info.rcWork;
	toast.appear    = 0.0f;
	toast.spin      = 0.0f;
	toast.hovered   = false;
	toast.hot       = std::nullopt;
	toast.hover     = {};
	toast.active    = true;
	toast.last_tick = Clock::now();
}

auto Tray::Native::measure_toast_text() -> void
{
	const std::wstring open_label = std::wstring{L"Open "} + win32::K_APP_NAME_WIDE;
	ToastMeasures&     measures   = toast.measures;

	measures.name         = text_width(writer.Get(), formats.toast_title.Get(), toast.account);
	measures.label        = text_width(writer.Get(), formats.toast_text.Get(), toast.step_label);
	measures.message      = text_width(writer.Get(), formats.message.Get(), toast.message);
	measures.message_line = text_height(writer.Get(), formats.message.Get(), L" ", K_TOAST_MAX_WIDTH);
	measures.retry        = text_width(writer.Get(), formats.button.Get(), K_RETRY_LABEL) + K_TOAST_BUTTON_PADDING * 2.0f;
	measures.open         = text_width(writer.Get(), formats.button.Get(), open_label) + K_TOAST_BUTTON_PADDING * 2.0f;

	const float message_room = toast_target_width(toast) - K_TOAST_TEXT_INSET - K_TOAST_PADDING_RIGHT;
	measures.message_height  = toast.message.empty() ? 0.0f : std::min(text_height(writer.Get(), formats.message.Get(), toast.message, message_room), K_TOAST_MESSAGE_MAX);
}

auto Tray::Native::raise_toast() -> void
{
	SetWindowPos(toast.window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
	toast.seconds_since_raised = 0.0f;
}

auto Tray::Native::hide_login() -> void
{
	if (toast.active) {
		toast.leaving = true;
	}
}

auto Tray::Native::toast_size() const -> SIZE
{
	const float width  = K_TOAST_MAX_WIDTH + K_SHADOW_MARGIN * 2.0f;
	const float height = K_TOAST_PILL_HEIGHT + toast_details_height(toast) + K_SHADOW_MARGIN * 2.0f;

	return SIZE{static_cast<LONG>(std::ceil(width * toast.scale)), static_cast<LONG>(std::ceil(height * toast.scale))};
}

auto Tray::Native::toast_hit(LPARAM t_lparam) const -> std::optional<ToastHit>
{
	const float       x     = static_cast<float>(static_cast<short>(LOWORD(t_lparam))) / toast.scale;
	const float       y     = static_cast<float>(static_cast<short>(HIWORD(t_lparam))) / toast.scale;
	const float       width = static_cast<float>(toast.canvas.size.cx) / toast.scale;
	const D2D1_RECT_F shape = toast_shape(toast, width, static_cast<float>(toast.canvas.size.cy) / toast.scale);

	if (!contains(shape, x, y)) return std::nullopt;
	if (!offers_retry(toast)) return ToastHit::BODY;
	if (toast.details < 1.0f) return std::nullopt;

	const ToastButtons buttons = toast_buttons(toast, shape);
	if (contains(buttons.retry, x, y)) return ToastHit::RETRY;
	if (contains(buttons.open, x, y)) return ToastHit::OPEN;

	return std::nullopt;
}

auto Tray::Native::render_toast() -> void
{
	const SIZE size = toast_size();
	if (!prepare_canvas(&toast.canvas, factory.Get(), size, toast.scale)) return;

	ID2D1DCRenderTarget* target = toast.canvas.target.Get();

	if (!toast.game_icon && toast.login.game < K_TRAY_MAX_GAMES) {
		toast.game_icon = make_bitmap(target, game_icon_sources[toast.login.game], K_TOAST_ICON_SIZE, K_TOAST_ICON_RADIUS, toast.scale);
	}

	const float shown = eased_out(toast.appear);
	const auto  inset = static_cast<LONG>(std::lround((K_SHADOW_MARGIN - K_TOAST_MARGIN) * toast.scale));
	const auto  slide = static_cast<LONG>(std::lround((1.0f - shown) * K_TOAST_SLIDE * toast.scale));
	const POINT origin{toast.work.right + inset - size.cx, toast.work.bottom + inset - size.cy + slide};

	target->BeginDraw();
	paint_toast(target, painter(toast.scale), toast);

	if (target->EndDraw() == D2DERR_RECREATE_TARGET) {
		toast.canvas.target.Reset();
		toast.game_icon.Reset();
		return;
	}

	present_canvas(toast.canvas, toast.window, origin, shown);
}

auto Tray::Native::step_toast() -> void
{
	const Clock::time_point now           = Clock::now();
	const float             seconds       = std::min(std::chrono::duration<float>(now - toast.last_tick).count(), 0.05f);
	const bool              has_finished  = !is_running(toast);
	const bool              shows_details = has_finished && !toast.message.empty();
	const float             hover         = 1.0f - std::exp(-K_HOVER_EASE_RATE * seconds);
	const float             widen         = 1.0f - std::exp(-K_TOAST_WIDTH_EASE_RATE * seconds);
	const float             turns         = toast.spin + seconds / K_TOAST_RING_TURN_SECONDS;

	toast.last_tick            = now;
	toast.spin                 = turns - std::floor(turns);
	toast.seconds_since_update = std::chrono::duration<float>(now - toast.updated_at).count();
	toast.measures.time        = std::max(text_width(writer.Get(), formats.toast_count.Get(), toast_time_label(toast)), K_TOAST_TIME_MIN_WIDTH);
	toast.width += (toast_target_width(toast) - toast.width) * widen;
	toast.seconds_since_raised += seconds;

	if (toast.seconds_since_raised >= K_TOAST_RAISE_SECONDS) {
		raise_toast();
	}

	if (has_finished && !toast.hovered && !toast.leaving) {
		toast.linger -= seconds;

		if (toast.linger <= 0.0f) {
			toast.leaving = true;
		}
	}

	toast.appear   = approached(toast.appear, toast.leaving ? 0.0f : 1.0f, seconds / K_TOAST_IN_SECONDS);
	toast.details  = approached(toast.details, shows_details ? 1.0f : 0.0f, seconds / K_TOAST_GROW_SECONDS);
	toast.finished = approached(toast.finished, has_finished ? 1.0f : 0.0f, seconds / K_TOAST_DONE_SECONDS);

	for (const ToastHit hit : each<ToastHit>()) {
		toast.hover[hit] += ((toast.hot == hit ? 1.0f : 0.0f) - toast.hover[hit]) * hover;
	}

	if (toast.leaving && toast.appear <= 0.0f) {
		toast.active = false;
		KillTimer(toast.window, K_TOAST_TIMER);
		ShowWindow(toast.window, SW_HIDE);
		return;
	}

	render_toast();
}

auto Tray::Native::handle_toast_message(UINT t_message, WPARAM t_wparam, LPARAM t_lparam) -> LRESULT
{
	switch (t_message) {
		case WM_MOUSEACTIVATE: {
			return MA_NOACTIVATE;
		}

		case WM_MOUSEMOVE: {
			if (!toast.tracking_leave) {
				TRACKMOUSEEVENT track{.cbSize = sizeof(TRACKMOUSEEVENT), .dwFlags = TME_LEAVE, .hwndTrack = toast.window};
				toast.tracking_leave = TrackMouseEvent(&track) != FALSE;
			}

			const float x     = static_cast<float>(static_cast<short>(LOWORD(t_lparam))) / toast.scale;
			const float y     = static_cast<float>(static_cast<short>(HIWORD(t_lparam))) / toast.scale;
			const float width = static_cast<float>(toast.canvas.size.cx) / toast.scale;

			toast.hot     = toast_hit(t_lparam);
			toast.hovered = contains(toast_shape(toast, width, static_cast<float>(toast.canvas.size.cy) / toast.scale), x, y);
			set_hand_cursor(toast.hot.has_value());
			return 0;
		}

		case WM_MOUSELEAVE: {
			toast.tracking_leave = false;
			toast.hovered        = false;
			toast.hot            = std::nullopt;
			return 0;
		}

		case WM_SETCURSOR: {
			set_hand_cursor(toast.hot.has_value());
			return TRUE;
		}

		case WM_LBUTTONUP: {
			const std::optional<ToastHit> hit = toast_hit(t_lparam);

			if (hit == ToastHit::RETRY) {
				pending_event = TrayEvent{.type = TrayEventType::RETRY_LOGIN};
				toast.linger  = K_TOAST_ERROR_SECONDS;
			} else if (hit == ToastHit::OPEN || hit == ToastHit::BODY) {
				pending_event = TrayEvent{.type = TrayEventType::SHOW_WINDOW};
				toast.leaving = true;
			}

			return 0;
		}

		case WM_TIMER: {
			if (t_wparam == K_TOAST_TIMER) {
				step_toast();
			}

			return 0;
		}

		default: {
			break;
		}
	}

	return DefWindowProcW(toast.window, t_message, t_wparam, t_lparam);
}

auto CALLBACK Tray::Native::window_proc(HWND t_window, UINT t_message, WPARAM t_wparam, LPARAM t_lparam) -> LRESULT
{
	if (t_message == WM_NCCREATE) {
		const auto* create = reinterpret_cast<const CREATESTRUCTW*>(t_lparam);
		SetWindowLongPtrW(t_window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
	}

	auto* native = reinterpret_cast<Native*>(GetWindowLongPtrW(t_window, GWLP_USERDATA));
	if (native == nullptr) return DefWindowProcW(t_window, t_message, t_wparam, t_lparam);

	native->window = t_window;

	return native->handle_message(t_message, t_wparam, t_lparam);
}

auto CALLBACK Tray::Native::popup_proc(HWND t_window, UINT t_message, WPARAM t_wparam, LPARAM t_lparam) -> LRESULT
{
	if (t_message == WM_NCCREATE) {
		const auto* create = reinterpret_cast<const CREATESTRUCTW*>(t_lparam);
		SetWindowLongPtrW(t_window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
	}

	auto* native = reinterpret_cast<Native*>(GetWindowLongPtrW(t_window, GWLP_USERDATA));
	if (native == nullptr) return DefWindowProcW(t_window, t_message, t_wparam, t_lparam);

	if (t_window == native->menu_window) return native->handle_menu_message(t_message, t_wparam, t_lparam);
	if (t_window == native->toast.window) return native->handle_toast_message(t_message, t_wparam, t_lparam);

	return DefWindowProcW(t_window, t_message, t_wparam, t_lparam);
}

}
