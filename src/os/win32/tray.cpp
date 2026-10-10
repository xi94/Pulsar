#include "os/tray.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cwchar>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include <Windows.h>
#include <d2d1.h>
#include <dwmapi.h>
#include <dwrite.h>
#include <shellapi.h>
#include <shellscalingapi.h>
#include <wrl/client.h>

#include "core/app_identity.h"
#include "core/debug_log.h"
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
constexpr UINT_PTR K_AIM_TIMER         = 2;
constexpr UINT_PTR K_WATCH_TIMER       = 3;
constexpr UINT     K_ANIMATION_TICK_MS = 8;
constexpr UINT     K_AIM_DELAY_MS      = 250;
constexpr UINT     K_WATCH_INTERVAL_MS = 50;

constexpr float K_LOCKED_ICON_OPACITY = 0.55f;

constexpr float K_REFERENCE_DPI    = 96.0f;
constexpr float K_PANEL_PADDING    = 5.0f;
constexpr float K_ROW_HEIGHT       = 28.0f;
constexpr float K_ROW_RADIUS       = 6.0f;
constexpr float K_ROW_INSET        = 9.0f;
constexpr float K_SEPARATOR_HEIGHT = 9.0f;
constexpr float K_SEPARATOR_INSET  = 4.0f;
constexpr float K_ICON_SIZE        = 18.0f;
constexpr float K_ICON_RADIUS      = 4.5f;
constexpr float K_LOGO_SIZE        = 16.0f;
constexpr float K_GLYPH_SIZE       = 15.0f;
constexpr float K_CHEVRON_SIZE     = 13.0f;
constexpr float K_ICON_GAP         = 9.0f;
constexpr float K_COUNT_GAP        = 3.0f;
constexpr float K_CHIP_HEIGHT      = 16.0f;
constexpr float K_CHIP_PADDING     = 6.0f;
constexpr float K_CHIP_GAP         = 7.0f;
constexpr float K_LOGIN_HEIGHT     = 22.0f;
constexpr float K_LOGIN_PADDING    = 9.0f;
constexpr float K_LOGIN_RADIUS     = 6.0f;
constexpr float K_LOGIN_SLIDE      = 6.0f;
constexpr float K_LOGIN_LIFT       = 0.14f;
constexpr float K_GAMES_MIN_WIDTH  = 220.0f;
constexpr float K_ACCOUNTS_WIDTH   = 190.0f;
constexpr float K_MAX_WIDTH        = 360.0f;
constexpr float K_ACCOUNTS_OVERLAP = 4.0f;
constexpr float K_AIM_SLOP         = 12.0f;
constexpr float K_HOVER_EASE_RATE  = 20.0f;
constexpr float K_HOVER_ALPHA      = 0.16f;
constexpr float K_QUIT_REST_RED    = 0.65f;
constexpr float K_CHIP_ALPHA       = 0.09f;
constexpr float K_SETTLED          = 0.005f;
constexpr float K_BODY_SIZE        = 13.0f;
constexpr float K_CAPTION_SIZE     = 12.0f;
constexpr float K_CHIP_TEXT_SIZE   = 11.0f;

constexpr UINT_PTR K_TOAST_TIMER   = 4;
constexpr UINT     K_TOAST_TICK_MS = 15;

constexpr float K_TOAST_MARGIN          = 12.0f;
constexpr float K_TOAST_SHADOW          = 16.0f;
constexpr float K_TOAST_SHADOW_DROP     = 3.0f;
constexpr float K_TOAST_SHADOW_ALPHA    = 0.28f;
constexpr u32   K_TOAST_SHADOW_LAYERS   = 12;
constexpr float K_TOAST_RADIUS          = 9.0f;
constexpr float K_TOAST_PADDING         = 11.0f;
constexpr float K_TOAST_PILL_WIDTH      = 224.0f;
constexpr float K_TOAST_PILL_HEIGHT     = 38.0f;
constexpr float K_TOAST_CARD_WIDTH      = 262.0f;
constexpr float K_TOAST_LINE_CENTER     = 15.5f;
constexpr float K_TOAST_BAR_TOP         = 27.0f;
constexpr float K_TOAST_BAR_HEIGHT      = 3.0f;
constexpr float K_TOAST_TRACK_ALPHA     = 0.1f;
constexpr float K_TOAST_LOGO_SIZE       = 15.0f;
constexpr float K_TOAST_ICON_GAP        = 8.0f;
constexpr float K_TOAST_GLYPH_SIZE      = 14.0f;
constexpr float K_TOAST_COUNT_WIDTH     = 26.0f;
constexpr float K_TOAST_HEADER_HEIGHT   = 18.0f;
constexpr float K_TOAST_MESSAGE_GAP     = 3.0f;
constexpr float K_TOAST_MESSAGE_MAX     = 48.0f;
constexpr float K_TOAST_BUTTON_GAP      = 10.0f;
constexpr float K_TOAST_BUTTON_HEIGHT   = 24.0f;
constexpr float K_TOAST_BUTTON_PADDING  = 10.0f;
constexpr float K_TOAST_BUTTON_SPACING  = 6.0f;
constexpr float K_TOAST_BUTTON_RADIUS   = 6.0f;
constexpr float K_TOAST_TEXT_SIZE       = 12.0f;
constexpr float K_TOAST_COUNT_SIZE      = 11.0f;
constexpr float K_TOAST_MESSAGE_SIZE    = 11.5f;
constexpr float K_TOAST_GHOST_ALPHA     = 0.08f;
constexpr float K_TOAST_GHOST_HOVER     = 0.07f;
constexpr float K_TOAST_ERROR_EDGE      = 0.6f;
constexpr float K_TOAST_SLIDE           = 14.0f;
constexpr float K_TOAST_IN_SECONDS      = 0.22f;
constexpr float K_TOAST_GROW_SECONDS    = 0.22f;
constexpr float K_TOAST_DONE_SECONDS    = 0.25f;
constexpr float K_TOAST_BAR_EASE_RATE   = 9.0f;
constexpr float K_TOAST_SUCCESS_SECONDS = 2.0f;
constexpr float K_TOAST_ERROR_SECONDS   = 5.0f;
constexpr float K_TOAST_FADE_SHARE      = 0.45f;

constexpr i32 K_TOAST_RETRY = 0;
constexpr i32 K_TOAST_OPEN  = 1;
constexpr i32 K_TOAST_BODY  = 2;

constexpr const wchar_t* K_FONT_FAMILIES[]{L"Segoe UI Variable Text", L"Segoe UI"};
constexpr const wchar_t* K_LOGIN_LABEL = L"Login";
constexpr const wchar_t* K_RETRY_LABEL = L"Retry";
constexpr wchar_t        K_ELLIPSIS    = 0x2026;

enum class RowKind : u8 {
	BRAND,
	GAME,
	ACCOUNT,
	ACTION,
	NOTE,
	SEPARATOR,
};

enum class Glyph : u8 {
	NONE,
	APP_WINDOW,
	LOCK,
	POWER,
	CHEVRON,
	CHECK_CIRCLE,
	ALERT_CIRCLE,
};

struct Row {
	RowKind           kind = RowKind::NOTE;
	std::wstring      label;
	std::wstring      detail;
	Glyph             glyph        = Glyph::NONE;
	i32               item         = -1;
	i32               icon         = -1;
	os::TrayEventType action       = os::TrayEventType::NONE;
	bool              destructive  = false;
	float             label_x      = 0.0f;
	float             detail_width = 0.0f;
	float             top          = 0.0f;
	float             height       = 0.0f;
	float             hover        = 0.0f;
};

struct TextFormats {
	ComPtr<IDWriteTextFormat> body;
	ComPtr<IDWriteTextFormat> strong;
	ComPtr<IDWriteTextFormat> count;
	ComPtr<IDWriteTextFormat> chip;
	ComPtr<IDWriteTextFormat> button;
	ComPtr<IDWriteTextFormat> message;
	ComPtr<IDWriteTextFormat> toast_text;
	ComPtr<IDWriteTextFormat> toast_title;
	ComPtr<IDWriteTextFormat> toast_count;
};

// One window of the menu: the games with the app's actions under them, or the accounts of the game being pointed at.
struct Panel {
	HWND                          window = nullptr;
	ComPtr<ID2D1HwndRenderTarget> target;
	std::vector<Row>              rows;
	float                         width          = 0.0f;
	float                         height         = 0.0f;
	i32                           hot            = -1;
	bool                          login_hot      = false;
	bool                          tracking_leave = false;
	ComPtr<ID2D1Bitmap>           game_icons[os::K_TRAY_MAX_GAMES];
	ComPtr<ID2D1Bitmap>           logo;
};

// The small window by the tray that follows a login: a slim pill with the login's progress, which grows into a card when it fails.
struct LoginToast {
	HWND                                  window   = nullptr;
	HDC                                   canvas   = nullptr;
	HBITMAP                               pixels   = nullptr;
	HGDIOBJ                               replaced = nullptr;
	SIZE                                  size{};
	ComPtr<ID2D1DCRenderTarget>           target;
	ComPtr<ID2D1Bitmap>                   logo;
	os::TrayLogin                         login{};
	std::wstring                          status;
	std::wstring                          account;
	RECT                                  work{};
	float                                 scale          = 1.0f;
	bool                                  active         = false;
	bool                                  leaving        = false;
	bool                                  hovered        = false;
	bool                                  tracking_leave = false;
	i32                                   hot            = -1;
	float                                 appear         = 0.0f;
	float                                 card           = 0.0f;
	float                                 done           = 0.0f;
	float                                 bar            = 0.0f;
	float                                 linger         = 0.0f;
	float                                 message_height = 0.0f;
	float                                 card_height    = 0.0f;
	float                                 retry_width    = 0.0f;
	float                                 open_width     = 0.0f;
	float                                 button_hover[2]{};
	std::chrono::steady_clock::time_point last_tick;
};

struct ToastCard {
	D2D1_RECT_F header;
	D2D1_RECT_F message;
	D2D1_RECT_F retry;
	D2D1_RECT_F open;
};

struct Painter {
	ID2D1Factory*      factory;
	const TextFormats* formats;
	ID2D1StrokeStyle*  stroke;
	os::TrayColors     colors;
	float              scale;
	float              login_width;
	bool               see_through;
	bool               framed;
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

// The toast's shape inside its window, which keeps room around it for the shadow. It grows from the pill into the card with its bottom right
// corner, the one nearest the tray, held still.
[[nodiscard]] auto toast_shape(const LoginToast& t_toast, float t_window_width, float t_window_height) -> D2D1_RECT_F
{
	const float grow   = smoothed(t_toast.card);
	const float width  = lerp(K_TOAST_PILL_WIDTH, K_TOAST_CARD_WIDTH, grow);
	const float height = lerp(K_TOAST_PILL_HEIGHT, t_toast.card_height, grow);
	const float right  = t_window_width - K_TOAST_SHADOW;
	const float bottom = t_window_height - K_TOAST_SHADOW;

	return D2D1::RectF(right - width, bottom - height, right, bottom);
}

[[nodiscard]] auto toast_card(const LoginToast& t_toast, const D2D1_RECT_F& t_shape) -> ToastCard
{
	const float left      = t_shape.left + K_TOAST_PADDING;
	const float text_left = left + K_TOAST_LOGO_SIZE + K_TOAST_ICON_GAP;
	const float top       = t_shape.top + K_TOAST_PADDING;
	const float message   = top + K_TOAST_HEADER_HEIGHT + K_TOAST_MESSAGE_GAP;
	const float buttons   = message + t_toast.message_height + K_TOAST_BUTTON_GAP;
	const float open_left = text_left + t_toast.retry_width + K_TOAST_BUTTON_SPACING;

	return ToastCard{
		.header  = D2D1::RectF(left, top, t_shape.right - K_TOAST_PADDING, top + K_TOAST_HEADER_HEIGHT),
		.message = D2D1::RectF(text_left, message, t_shape.right - K_TOAST_PADDING, message + t_toast.message_height),
		.retry   = D2D1::RectF(text_left, buttons, text_left + t_toast.retry_width, buttons + K_TOAST_BUTTON_HEIGHT),
		.open    = D2D1::RectF(open_left, buttons, open_left + t_toast.open_width, buttons + K_TOAST_BUTTON_HEIGHT),
	};
}

[[nodiscard]] auto is_selectable(const Row& t_row) -> bool
{
	return t_row.kind == RowKind::GAME || t_row.kind == RowKind::ACCOUNT || t_row.kind == RowKind::ACTION;
}

// An account logs in from its Login button alone, which sits at the end of its row as far in from the side as from the top.
[[nodiscard]] auto login_rect(const Row& t_row, float t_panel_width, float t_login_width, float t_scale) -> D2D1_RECT_F
{
	const float right = t_panel_width - K_PANEL_PADDING - (K_ROW_HEIGHT - K_LOGIN_HEIGHT) * 0.5f;
	const float top   = snapped(t_row.top + (t_row.height - K_LOGIN_HEIGHT) * 0.5f, t_scale);

	return D2D1::RectF(right - t_login_width, top, right, top + K_LOGIN_HEIGHT);
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

// Scales a PNG to a square of t_pixels with a box filter and rounds its corners, as premultiplied BGRA ready for Direct2D.
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

	for (int y = 0; y < t_pixels; y += 1) {
		const int source_y0 = y * height / t_pixels;
		const int source_y1 = std::max((y + 1) * height / t_pixels, source_y0 + 1);

		for (int x = 0; x < t_pixels; x += 1) {
			const int source_x0 = x * width / t_pixels;
			const int source_x1 = std::max((x + 1) * width / t_pixels, source_x0 + 1);

			float sum[4]{};
			float samples = 0.0f;

			for (int sy = source_y0; sy < std::min(source_y1, height); sy += 1) {
				for (int sx = source_x0; sx < std::min(source_x1, width); sx += 1) {
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

	t_target->CreateBitmap(D2D1::SizeU(static_cast<UINT32>(pixels), static_cast<UINT32>(pixels)), data.data(), static_cast<UINT32>(pixels) * 4, properties,
	                       &bitmap);

	return bitmap;
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

[[nodiscard]] auto make_format(IDWriteFactory* t_writer, const wchar_t* t_family, float t_size, DWRITE_FONT_WEIGHT t_weight, DWRITE_TEXT_ALIGNMENT t_alignment)
	-> ComPtr<IDWriteTextFormat>
{
	ComPtr<IDWriteTextFormat> format;
	const HRESULT created = t_writer->CreateTextFormat(t_family, nullptr, t_weight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, t_size, L"", &format);
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

auto draw_label(ID2D1RenderTarget* t_target,
                IDWriteTextFormat* t_format,
                std::wstring_view  t_text,
                float              t_left,
                float              t_right,
                const D2D1_RECT_F& t_row,
                ID2D1Brush*        t_brush) -> void
{
	if (t_format == nullptr || t_text.empty() || t_right <= t_left) return;

	t_target->DrawText(t_text.data(), static_cast<UINT32>(t_text.size()), t_format, D2D1::RectF(t_left, t_row.top, t_right, t_row.bottom), t_brush,
	                   D2D1_DRAW_TEXT_OPTIONS_CLIP);
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
		const D2D1_ROUNDED_RECT shape{D2D1::RectF(at(t_left, t_top).x, at(t_left, t_top).y, at(t_right, t_bottom).x, at(t_right, t_bottom).y), 2.0f * unit,
		                              2.0f * unit};
		t_target->DrawRoundedRectangle(shape, t_brush, stroke, t_painter.stroke);
	};

	const auto dot = [&](float t_x, float t_y, float t_radius) { t_target->FillEllipse(D2D1::Ellipse(at(t_x, t_y), t_radius, t_radius), t_brush); };

	switch (t_glyph) {
		using enum Glyph;

		case APP_WINDOW: {
			frame(3.0f, 5.0f, 21.0f, 19.0f);
			dot(6.0f, 8.0f, stroke * 0.5f);
			dot(9.0f, 8.0f, stroke * 0.5f);
			break;
		}

		case LOCK: {
			frame(5.0f, 11.0f, 19.0f, 21.0f);
			dot(12.0f, 16.0f, unit + stroke * 0.5f);
			trace([&](ID2D1GeometrySink* t_sink) {
				t_sink->BeginFigure(at(8.0f, 11.0f), D2D1_FIGURE_BEGIN_HOLLOW);
				t_sink->AddLine(at(8.0f, 7.0f));
				t_sink->AddArc(
					D2D1::ArcSegment(at(16.0f, 7.0f), D2D1::SizeF(4.0f * unit, 4.0f * unit), 0.0f, D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
				t_sink->AddLine(at(16.0f, 11.0f));
				t_sink->EndFigure(D2D1_FIGURE_END_OPEN);
			});
			break;
		}

		case POWER: {
			trace([&](ID2D1GeometrySink* t_sink) {
				t_sink->BeginFigure(at(7.0f, 6.0f), D2D1_FIGURE_BEGIN_HOLLOW);
				t_sink->AddArc(D2D1::ArcSegment(at(17.0f, 6.0f), D2D1::SizeF(7.75f * unit, 7.75f * unit), 0.0f, D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,
				                                D2D1_ARC_SIZE_LARGE));
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

		case NONE: {
			break;
		}
	}
}

[[nodiscard]] auto centered_box(float t_left, float t_middle, float t_size, float t_scale) -> D2D1_RECT_F
{
	const float left = snapped(t_left, t_scale);
	const float top  = snapped(t_middle - t_size * 0.5f, t_scale);

	return D2D1::RectF(left, top, left + t_size, top + t_size);
}

auto draw_bitmap(ID2D1RenderTarget* t_target, ID2D1Bitmap* t_bitmap, float t_left, float t_middle, float t_scale, float t_opacity = 1.0f) -> void
{
	const D2D1_SIZE_F size = t_bitmap->GetSize();
	const float       left = snapped(t_left, t_scale);
	const float       top  = snapped(t_middle - size.height * 0.5f, t_scale);

	t_target->DrawBitmap(t_bitmap, D2D1::RectF(left, top, left + size.width, top + size.height), t_opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
}

auto paint_panel(ID2D1RenderTarget* t_target, const Painter& t_painter, const Panel& t_panel) -> void
{
	const os::TrayColors& colors  = t_painter.colors;
	const TextFormats&    formats = *t_painter.formats;
	const float           scale   = t_painter.scale;
	const D2D1_SIZE_F     size    = t_target->GetSize();

	ComPtr<ID2D1SolidColorBrush> brush;
	if (FAILED(t_target->CreateSolidColorBrush(to_d2d(colors.text), &brush))) return;

	const auto paint = [&](Color t_color, float t_opacity = 1.0f) {
		brush->SetColor(to_d2d(t_color, t_opacity));
		return brush.Get();
	};

	if (t_painter.see_through) {
		t_target->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
		t_target->FillRectangle(D2D1::RectF(0.0f, 0.0f, size.width, size.height), paint(colors.background));
	} else {
		t_target->Clear(to_d2d(with_alpha(colors.background, 255)));
	}

	if (t_painter.framed) {
		const float half = 0.5f / scale;
		t_target->DrawRectangle(D2D1::RectF(half, half, size.width - half, size.height - half), paint(colors.border), 1.0f / scale);
	}

	for (usize index = 0; index < t_panel.rows.size(); index += 1) {
		const Row&        row = t_panel.rows[index];
		const D2D1_RECT_F box{K_PANEL_PADDING, row.top, size.width - K_PANEL_PADDING, row.top + row.height};
		const float       left   = box.left + K_ROW_INSET;
		const float       right  = box.right - K_ROW_INSET;
		const float       middle = (box.top + box.bottom) * 0.5f;
		const Color       ink    = mix(colors.text_dim, colors.text, row.hover);

		if (row.hover > 0.0f && is_selectable(row) && row.kind != RowKind::ACCOUNT) {
			const D2D1_ROUNDED_RECT pill{D2D1::RectF(box.left, box.top + 1.0f, box.right, box.bottom - 1.0f), K_ROW_RADIUS, K_ROW_RADIUS};
			t_target->FillRoundedRectangle(pill, paint(row.destructive ? colors.error : colors.accent, K_HOVER_ALPHA * row.hover));
		}

		switch (row.kind) {
			using enum RowKind;

			case BRAND: {
				if (t_panel.logo) {
					draw_bitmap(t_target, t_panel.logo.Get(), left + (K_ICON_SIZE - K_LOGO_SIZE) * 0.5f, middle, scale);
				}

				draw_label(t_target, formats.strong.Get(), row.label, row.label_x, right, box, paint(colors.text));
				break;
			}

			case GAME: {
				const float chevron = right - K_CHEVRON_SIZE;
				const float count   = chevron - K_COUNT_GAP - row.detail_width;

				if (row.icon >= 0 && static_cast<u32>(row.icon) < os::K_TRAY_MAX_GAMES && t_panel.game_icons[row.icon]) {
					draw_bitmap(t_target, t_panel.game_icons[row.icon].Get(), left, middle, scale);
				} else {
					const D2D1_RECT_F       blank = centered_box(left, middle, K_ICON_SIZE, scale);
					const D2D1_ROUNDED_RECT shape{blank, K_ICON_RADIUS, K_ICON_RADIUS};
					t_target->FillRoundedRectangle(shape, paint(colors.text_disabled, 0.4f));
				}

				draw_label(t_target, formats.body.Get(), row.label, row.label_x, (row.detail.empty() ? chevron : count) - K_ICON_GAP, box, paint(colors.text));
				draw_label(t_target, formats.count.Get(), row.detail, count, chevron - K_COUNT_GAP, box, paint(colors.text_disabled));
				draw_glyph(t_target, t_painter, Glyph::CHEVRON, centered_box(chevron, middle, K_CHEVRON_SIZE, scale), paint(ink));
				break;
			}

			case ACCOUNT: {
				const D2D1_RECT_F login      = login_rect(row, size.width, t_painter.login_width, scale);
				const float       chip_right = login.left - K_CHIP_GAP;
				const float       chip_left  = chip_right - row.detail_width;
				const bool        followed   = index + 1 < t_panel.rows.size() && t_panel.rows[index + 1].kind == RowKind::ACCOUNT;

				if (followed) {
					const float y = snapped(box.bottom - 0.5f, scale);
					t_target->FillRectangle(D2D1::RectF(left, y, right, y + 1.0f / scale), paint(colors.separator));
				}

				if (!row.detail.empty()) {
					const float             top = snapped(middle - K_CHIP_HEIGHT * 0.5f, scale);
					const D2D1_ROUNDED_RECT chip{D2D1::RectF(chip_left, top, chip_right, top + K_CHIP_HEIGHT), K_CHIP_HEIGHT * 0.5f, K_CHIP_HEIGHT * 0.5f};

					t_target->FillRoundedRectangle(chip, paint(colors.text, K_CHIP_ALPHA));
					draw_label(t_target, formats.chip.Get(), row.detail, chip_left, chip_right, chip.rect, paint(colors.text_dim));
				}

				draw_label(t_target, formats.body.Get(), row.label, row.label_x, (row.detail.empty() ? login.left : chip_left) - K_CHIP_GAP, box,
				           paint(colors.text));

				if (row.hover > 0.0f) {
					const float             slide = (1.0f - row.hover) * K_LOGIN_SLIDE;
					const D2D1_ROUNDED_RECT pill{D2D1::RectF(login.left + slide, login.top, login.right + slide, login.bottom), K_LOGIN_RADIUS, K_LOGIN_RADIUS};
					const bool              lit = t_panel.login_hot && static_cast<i32>(index) == t_panel.hot;

					t_target->FillRoundedRectangle(pill, paint(colors.accent, row.hover));

					if (lit) {
						t_target->FillRoundedRectangle(pill, paint(Color{255, 255, 255, 255}, K_LOGIN_LIFT * row.hover));
					}

					draw_label(t_target, formats.button.Get(), K_LOGIN_LABEL, pill.rect.left, pill.rect.right, pill.rect, paint(colors.accent_ink, row.hover));
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

			case NOTE: {
				if (row.glyph != Glyph::NONE) {
					const D2D1_RECT_F glyph = centered_box(left + (K_ICON_SIZE - K_GLYPH_SIZE) * 0.5f, middle, K_GLYPH_SIZE, scale);
					draw_glyph(t_target, t_painter, row.glyph, glyph, paint(colors.text_disabled));
				}

				draw_label(t_target, formats.body.Get(), row.label, row.label_x, right, box, paint(colors.text_disabled));
				break;
			}

			case SEPARATOR: {
				const float y = snapped(middle, scale);
				t_target->FillRectangle(D2D1::RectF(box.left + K_SEPARATOR_INSET, y, box.right - K_SEPARATOR_INSET, y + 1.0f / scale), paint(colors.separator));
				break;
			}
		}
	}
}

auto paint_toast(ID2D1RenderTarget* t_target, const Painter& t_painter, const LoginToast& t_toast) -> void
{
	const os::TrayColors& colors     = t_painter.colors;
	const TextFormats&    formats    = *t_painter.formats;
	const float           scale      = t_painter.scale;
	const D2D1_SIZE_F     size       = t_target->GetSize();
	const D2D1_RECT_F     shape      = toast_shape(t_toast, size.width, size.height);
	const float           grow       = smoothed(t_toast.card);
	const float           pill_alpha = std::clamp(1.0f - grow / K_TOAST_FADE_SHARE, 0.0f, 1.0f);
	const float           card_alpha = std::clamp((grow - (1.0f - K_TOAST_FADE_SHARE)) / K_TOAST_FADE_SHARE, 0.0f, 1.0f);

	ComPtr<ID2D1SolidColorBrush> brush;
	if (FAILED(t_target->CreateSolidColorBrush(to_d2d(colors.text), &brush))) return;

	const auto paint = [&](Color t_color, float t_opacity = 1.0f) {
		brush->SetColor(to_d2d(t_color, t_opacity));
		return brush.Get();
	};

	t_target->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

	// Stacked layers of faint black make a soft shadow without a blur pass.
	for (u32 layer = K_TOAST_SHADOW_LAYERS; layer > 0; layer -= 1) {
		const float       spread = static_cast<float>(layer) * (K_TOAST_SHADOW - K_TOAST_SHADOW_DROP) / static_cast<float>(K_TOAST_SHADOW_LAYERS);
		const D2D1_RECT_F area =
			D2D1::RectF(shape.left - spread, shape.top - spread + K_TOAST_SHADOW_DROP, shape.right + spread, shape.bottom + spread + K_TOAST_SHADOW_DROP);
		const D2D1_ROUNDED_RECT shadow{area, K_TOAST_RADIUS + spread, K_TOAST_RADIUS + spread};

		t_target->FillRoundedRectangle(shadow, paint(Color{0, 0, 0, 255}, K_TOAST_SHADOW_ALPHA / static_cast<float>(K_TOAST_SHADOW_LAYERS)));
	}

	const float             half = 0.5f / scale;
	const D2D1_ROUNDED_RECT body{shape, K_TOAST_RADIUS, K_TOAST_RADIUS};
	const D2D1_ROUNDED_RECT edge{D2D1::RectF(shape.left + half, shape.top + half, shape.right - half, shape.bottom - half), K_TOAST_RADIUS, K_TOAST_RADIUS};

	t_target->FillRoundedRectangle(body, paint(with_alpha(colors.background, 255)));
	t_target->DrawRoundedRectangle(edge, paint(mix(colors.border, colors.error, grow * K_TOAST_ERROR_EDGE)), 1.0f / scale);

	const float left      = shape.left + K_TOAST_PADDING;
	const float right     = shape.right - K_TOAST_PADDING;
	const float text_left = left + K_TOAST_LOGO_SIZE + K_TOAST_ICON_GAP;

	// While the pill grows into the card, what's inside stays inside the outline.
	t_target->PushAxisAlignedClip(shape, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

	if (pill_alpha > 0.0f) {
		const D2D1_RECT_F line{left, shape.top + K_TOAST_LINE_CENTER - K_TOAST_HEADER_HEIGHT * 0.5f, right,
		                       shape.top + K_TOAST_LINE_CENTER + K_TOAST_HEADER_HEIGHT * 0.5f};
		const bool        succeeded = t_toast.login.state == os::TrayLoginState::SUCCEEDED;
		const float       middle    = (line.top + line.bottom) * 0.5f;

		if (t_toast.logo) {
			draw_bitmap(t_target, t_toast.logo.Get(), left, middle, scale, pill_alpha);
		}

		const std::wstring status = succeeded ? L"Logged in as " + t_toast.account : t_toast.status;
		draw_label(t_target, formats.toast_text.Get(), status, text_left, right - K_TOAST_COUNT_WIDTH, line, paint(colors.text, pill_alpha));

		if (t_toast.login.step > 0 && t_toast.done < 1.0f) {
			const std::wstring count = std::to_wstring(t_toast.login.step) + L"/" + std::to_wstring(t_toast.login.step_count);
			draw_label(t_target, formats.toast_count.Get(), count, right - K_TOAST_COUNT_WIDTH, right, line,
			           paint(colors.text_disabled, pill_alpha * (1.0f - t_toast.done)));
		}

		if (t_toast.done > 0.0f) {
			const D2D1_RECT_F check = centered_box(right - K_TOAST_GLYPH_SIZE, middle, K_TOAST_GLYPH_SIZE, scale);
			draw_glyph(t_target, t_painter, Glyph::CHECK_CIRCLE, check, paint(colors.success, pill_alpha * t_toast.done));
		}

		const float             top    = snapped(shape.top + K_TOAST_BAR_TOP, scale);
		const float             radius = K_TOAST_BAR_HEIGHT * 0.5f;
		const D2D1_ROUNDED_RECT track{D2D1::RectF(left, top, right, top + K_TOAST_BAR_HEIGHT), radius, radius};

		t_target->FillRoundedRectangle(track, paint(colors.text, K_TOAST_TRACK_ALPHA * pill_alpha));

		if (t_toast.bar > 0.0f) {
			const float             reach = std::max((right - left) * std::clamp(t_toast.bar, 0.0f, 1.0f), K_TOAST_BAR_HEIGHT);
			const D2D1_ROUNDED_RECT filled{D2D1::RectF(left, top, left + reach, top + K_TOAST_BAR_HEIGHT), radius, radius};
			t_target->FillRoundedRectangle(filled, paint(mix(colors.accent, colors.success, t_toast.done), pill_alpha));
		}
	}

	if (card_alpha > 0.0f) {
		const ToastCard card   = toast_card(t_toast, shape);
		const float     middle = (card.header.top + card.header.bottom) * 0.5f;

		if (t_toast.logo) {
			draw_bitmap(t_target, t_toast.logo.Get(), card.header.left, middle, scale, card_alpha);
		}

		const D2D1_RECT_F alert = centered_box(card.header.right - K_TOAST_GLYPH_SIZE, middle, K_TOAST_GLYPH_SIZE, scale);
		draw_glyph(t_target, t_painter, Glyph::ALERT_CIRCLE, alert, paint(colors.error, card_alpha));
		draw_label(t_target, formats.toast_title.Get(), L"Couldn't log in to " + t_toast.account, text_left, alert.left - K_TOAST_ICON_GAP, card.header,
		           paint(colors.text, card_alpha));

		t_target->DrawText(t_toast.status.c_str(), static_cast<UINT32>(t_toast.status.size()), formats.message.Get(), card.message,
		                   paint(colors.text_dim, card_alpha), D2D1_DRAW_TEXT_OPTIONS_CLIP);

		const D2D1_ROUNDED_RECT retry{card.retry, K_TOAST_BUTTON_RADIUS, K_TOAST_BUTTON_RADIUS};
		const D2D1_ROUNDED_RECT open{card.open, K_TOAST_BUTTON_RADIUS, K_TOAST_BUTTON_RADIUS};

		t_target->FillRoundedRectangle(retry, paint(colors.accent, card_alpha));
		t_target->FillRoundedRectangle(retry, paint(Color{255, 255, 255, 255}, K_LOGIN_LIFT * t_toast.button_hover[0] * card_alpha));
		draw_label(t_target, formats.button.Get(), K_RETRY_LABEL, card.retry.left, card.retry.right, card.retry, paint(colors.accent_ink, card_alpha));

		t_target->FillRoundedRectangle(open, paint(colors.text, (K_TOAST_GHOST_ALPHA + K_TOAST_GHOST_HOVER * t_toast.button_hover[1]) * card_alpha));
		draw_label(t_target, formats.button.Get(), std::wstring{L"Open "} + os::win32::K_APP_NAME_WIDE, card.open.left, card.open.right, card.open,
		           paint(colors.text, card_alpha));
	}

	t_target->PopAxisAlignedClip();
}

[[nodiscard]] auto inside_triangle(POINT t_point, POINT t_a, POINT t_b, POINT t_c) -> bool
{
	const auto side = [&](POINT t_from, POINT t_to) {
		return static_cast<i64>(t_to.x - t_from.x) * (t_point.y - t_from.y) - static_cast<i64>(t_to.y - t_from.y) * (t_point.x - t_from.x);
	};

	const i64 ab = side(t_a, t_b);
	const i64 bc = side(t_b, t_c);
	const i64 ca = side(t_c, t_a);

	return (ab > 0 && bc > 0 && ca > 0) || (ab < 0 && bc < 0 && ca < 0);
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
		.background    = {32, 32, 36, 255},
		.border        = {64, 64, 70, 255},
		.separator     = {50, 50, 56, 255},
		.text          = {232, 232, 236, 255},
		.text_dim      = {150, 150, 158, 255},
		.text_disabled = {108, 108, 116, 255},
		.accent        = {203, 166, 247, 255},
		.accent_ink    = {24, 25, 30, 255},
		.success       = {80, 200, 120, 255},
		.error         = {220, 90, 80, 255},
		.dark          = true,
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
	float                    login_width = 0.0f;
	bool                     popup_ready = false;

	Panel             games;
	Panel             accounts;
	bool              open        = false;
	bool              activated   = false;
	bool              see_through = false;
	bool              framed      = false;
	float             scale       = 1.0f;
	RECT              work{};
	i32               open_row         = -1;
	bool              accounts_focused = false;
	POINT             last_pointer{};
	bool              animating = false;
	Clock::time_point last_tick;

	LoginToast toast;

	static auto CALLBACK window_proc(HWND t_window, UINT t_message, WPARAM t_wparam, LPARAM t_lparam) -> LRESULT;
	static auto CALLBACK popup_proc(HWND t_window, UINT t_message, WPARAM t_wparam, LPARAM t_lparam) -> LRESULT;

	[[nodiscard]] auto handle_message(UINT t_message, WPARAM t_wparam, LPARAM t_lparam) -> LRESULT;
	[[nodiscard]] auto handle_popup_message(HWND t_window, UINT t_message, WPARAM t_wparam, LPARAM t_lparam) -> LRESULT;
	[[nodiscard]] auto handle_toast_message(UINT t_message, WPARAM t_wparam, LPARAM t_lparam) -> LRESULT;

	auto add_icon() -> bool;
	auto remove_icon() -> void;
	auto update_icon() -> void;
	[[nodiscard]] auto shown_icon() const -> HICON;
	auto fill_tooltip(wchar_t (&t_tooltip)[128]) const -> void;

	[[nodiscard]] auto ensure_popup() -> bool;
	[[nodiscard]] auto painter() const -> Painter;
	auto show_menu() -> void;
	auto close_menu() -> void;
	auto choose(const Row& t_row) -> void;
	auto build_games() -> void;
	auto build_accounts(i32 t_game) -> void;
	auto lay_out(Panel* t_panel, float t_min_width, bool t_icon_column) -> void;
	auto apply_window_look(HWND t_window) -> void;
	auto open_accounts(i32 t_row) -> void;
	auto close_accounts() -> void;
	auto enter_accounts() -> void;
	auto back_to_games() -> void;
	auto point_at_game(i32 t_row) -> void;
	[[nodiscard]] auto aims_at_accounts(POINT t_point) const -> bool;
	[[nodiscard]] auto row_at(const Panel& t_panel, float t_y) const -> i32;
	[[nodiscard]] auto pixel_size(const Panel& t_panel) const -> SIZE;

	auto paint(Panel* t_panel) -> void;
	auto on_pointer(Panel* t_panel, LPARAM t_lparam) -> void;
	auto on_leave(Panel* t_panel) -> void;
	auto on_click(Panel* t_panel, LPARAM t_lparam) -> void;
	[[nodiscard]] auto over_login(const Row& t_row, float t_x, float t_y) const -> bool;
	auto on_key(WPARAM t_key) -> void;
	auto step_hot(Panel* t_panel, i32 t_step) -> void;
	auto watch() -> void;
	auto start_animation() -> void;
	auto animate() -> void;

	[[nodiscard]] auto ensure_toast() -> bool;
	auto create_toast_target() -> bool;
	auto show_login(const TrayLogin& t_login) -> void;
	auto hide_login() -> void;
	[[nodiscard]] auto toast_size() const -> SIZE;
	[[nodiscard]] auto toast_hit(LPARAM t_lparam) const -> i32;
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

	native->accounts.target.Reset();
	native->games.target.Reset();

	if (native->accounts.window != nullptr) {
		DestroyWindow(native->accounts.window);
	}

	if (native->games.window != nullptr) {
		DestroyWindow(native->games.window);
	}

	if (native->locked_icon != nullptr) {
		DestroyIcon(native->locked_icon);
	}

	LoginToast* toast = &native->toast;
	toast->target.Reset();

	if (toast->window != nullptr) {
		KillTimer(toast->window, K_TOAST_TIMER);
		DestroyWindow(toast->window);
	}

	if (toast->pixels != nullptr) {
		SelectObject(toast->canvas, toast->replaced);
		DeleteObject(toast->pixels);
	}

	if (toast->canvas != nullptr) {
		DeleteDC(toast->canvas);
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

auto Tray::set_locked(bool t_locked) -> void
{
	Native* native = m_native.get();
	if (t_locked == native->locked) return;

	native->locked = t_locked;
	native->close_menu();

	if (native->locked && native->locked_icon == nullptr) {
		native->locked_icon = greyscale_icon(native->icon);
	}

	native->update_icon();
}

auto Tray::show_login(const TrayLogin& t_login) -> void
{
	m_native->show_login(t_login);
}

auto Tray::hide_login() -> void
{
	m_native->hide_login();
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
	m_native->games.game_icons[t_game].Reset();
}

auto Tray::set_logo(std::span<const u8> t_png) -> void
{
	m_native->logo_source = t_png;
	m_native->games.logo.Reset();
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

	if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory.GetAddressOf())) ||
	    FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(writer.GetAddressOf())))) {
		debug_log::write(K_LOG_CATEGORY, "Direct2D or DirectWrite is unavailable, so the tray has no menu");
		return false;
	}

	const D2D1_STROKE_STYLE_PROPERTIES round =
		D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_ROUND);
	factory->CreateStrokeStyle(round, nullptr, 0, &stroke);

	const wchar_t* family = menu_font_family(writer.Get());
	formats.body          = make_format(writer.Get(), family, K_BODY_SIZE, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_LEADING);
	formats.strong        = make_format(writer.Get(), family, K_BODY_SIZE, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_LEADING);
	formats.count         = make_format(writer.Get(), family, K_CAPTION_SIZE, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_TRAILING);
	formats.chip          = make_format(writer.Get(), family, K_CHIP_TEXT_SIZE, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
	formats.button        = make_format(writer.Get(), family, K_CAPTION_SIZE, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER);
	formats.message       = make_format(writer.Get(), family, K_TOAST_MESSAGE_SIZE, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_LEADING);
	formats.toast_text    = make_format(writer.Get(), family, K_TOAST_TEXT_SIZE, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_LEADING);
	formats.toast_title   = make_format(writer.Get(), family, K_TOAST_TEXT_SIZE, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_LEADING);
	formats.toast_count   = make_format(writer.Get(), family, K_TOAST_COUNT_SIZE, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_TRAILING);

	if (formats.message) {
		formats.message->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
		formats.message->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
	}

	login_width = text_width(writer.Get(), formats.button.Get(), K_LOGIN_LABEL) + K_LOGIN_PADDING * 2.0f;

	const HINSTANCE instance = GetModuleHandleW(nullptr);

	// The drop shadow class style gives the menu the same soft shadow as the system's own menus.
	const WNDCLASSEXW popup_class{
		.cbSize        = sizeof(WNDCLASSEXW),
		.style         = CS_DROPSHADOW,
		.lpfnWndProc   = popup_proc,
		.hInstance     = instance,
		.hCursor       = LoadCursorW(nullptr, IDC_ARROW),
		.lpszClassName = os::win32::K_TRAY_MENU_CLASS_NAME,
	};

	if (RegisterClassExW(&popup_class) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
		debug_log::write(K_LOG_CATEGORY, "RegisterClassExW for the tray menu failed, err=%lu", GetLastError());
		return false;
	}

	// The accounts panel never takes focus, so the games panel stays the active window and keeps the keyboard.
	games.window =
		CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, os::win32::K_TRAY_MENU_CLASS_NAME, L"", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, instance, this);
	accounts.window = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE, os::win32::K_TRAY_MENU_CLASS_NAME, L"", WS_POPUP, 0, 0, 1, 1,
	                                  games.window, nullptr, instance, this);

	if (games.window == nullptr || accounts.window == nullptr) {
		debug_log::write(K_LOG_CATEGORY, "failed to create the tray menu windows, err=%lu", GetLastError());
		return false;
	}

	popup_ready = true;

	return true;
}

auto Tray::Native::painter() const -> Painter
{
	return Painter{
		.factory     = factory.Get(),
		.formats     = &formats,
		.stroke      = stroke.Get(),
		.colors      = colors,
		.scale       = scale,
		.login_width = login_width,
		.see_through = see_through,
		.framed      = framed,
	};
}

auto Tray::Native::build_games() -> void
{
	games.rows.clear();
	games.rows.push_back(Row{.kind = RowKind::BRAND, .label = win32::K_APP_NAME_WIDE});
	games.rows.push_back(Row{.kind = RowKind::SEPARATOR});

	if (menu.locked) {
		games.rows.push_back(Row{.kind = RowKind::NOTE, .label = L"Vault locked", .glyph = Glyph::LOCK});
	} else if (menu.game_count == 0) {
		games.rows.push_back(Row{.kind = RowKind::NOTE, .label = L"No games"});
	}

	for (u32 i = 0; i < menu.game_count && !menu.locked; i += 1) {
		const TrayGame& game = menu.games[i];

		games.rows.push_back(Row{
			.kind   = RowKind::GAME,
			.label  = win32::to_wide(game.title),
			.detail = game.account_count > 0 ? std::to_wstring(game.account_count) : std::wstring{},
			.item   = static_cast<i32>(i),
			.icon   = game.game,
		});
	}

	games.rows.push_back(Row{.kind = RowKind::SEPARATOR});
	games.rows.push_back(Row{
		.kind   = RowKind::ACTION,
		.label  = std::wstring{L"Open "} + win32::K_APP_NAME_WIDE,
		.glyph  = Glyph::APP_WINDOW,
		.action = TrayEventType::SHOW_WINDOW,
	});

	if (menu.can_lock && !menu.locked) {
		games.rows.push_back(Row{.kind = RowKind::ACTION, .label = L"Lock vault", .glyph = Glyph::LOCK, .action = TrayEventType::LOCK});
	}

	games.rows.push_back(Row{.kind = RowKind::ACTION, .label = L"Quit", .glyph = Glyph::POWER, .action = TrayEventType::EXIT, .destructive = true});
}

auto Tray::Native::build_accounts(i32 t_game) -> void
{
	accounts.rows.clear();
	accounts.hot       = -1;
	accounts.login_hot = false;

	const TrayGame& game = menu.games[t_game];

	for (u32 i = 0; i < game.account_count && game.first_account + i < menu.account_count; i += 1) {
		const u32          index   = game.first_account + i;
		const TrayAccount& account = menu.accounts[index];

		accounts.rows.push_back(Row{
			.kind   = RowKind::ACCOUNT,
			.label  = win32::to_wide(account.label),
			.detail = win32::to_wide(account.region),
			.item   = static_cast<i32>(index),
		});
	}

	if (accounts.rows.empty()) {
		accounts.rows.push_back(Row{.kind = RowKind::NOTE, .label = L"No accounts"});
	}
}

auto Tray::Native::lay_out(Panel* t_panel, float t_min_width, bool t_icon_column) -> void
{
	float y      = K_PANEL_PADDING;
	float widest = 0.0f;

	for (Row& row : t_panel->rows) {
		row.top     = y;
		row.height  = row.kind == RowKind::SEPARATOR ? K_SEPARATOR_HEIGHT : K_ROW_HEIGHT;
		row.label_x = K_PANEL_PADDING + K_ROW_INSET + (t_icon_column ? K_ICON_SIZE + K_ICON_GAP : 0.0f);
		y += row.height;

		const float label = text_width(writer.Get(), (row.kind == RowKind::BRAND ? formats.strong : formats.body).Get(), row.label);
		float       need  = row.label_x + label + K_ROW_INSET + K_PANEL_PADDING;

		if (row.kind == RowKind::GAME) {
			row.detail_width = text_width(writer.Get(), formats.count.Get(), row.detail);
			need += K_ICON_GAP + row.detail_width + K_COUNT_GAP + K_CHEVRON_SIZE;
		} else if (row.kind == RowKind::ACCOUNT) {
			row.detail_width = row.detail.empty() ? 0.0f : text_width(writer.Get(), formats.chip.Get(), row.detail) + K_CHIP_PADDING * 2.0f;
			need += K_CHIP_GAP + row.detail_width + K_CHIP_GAP + login_width;
		}

		widest = std::max(widest, need);
	}

	t_panel->height = y + K_PANEL_PADDING;
	t_panel->width  = std::ceil(std::clamp(widest, t_min_width, K_MAX_WIDTH));
}

auto Tray::Native::pixel_size(const Panel& t_panel) const -> SIZE
{
	return SIZE{static_cast<LONG>(std::ceil(t_panel.width * scale)), static_cast<LONG>(std::ceil(t_panel.height * scale))};
}

// The theme's colours on Windows 11's frosted glass with rounded corners. Older Windows has neither, so the menu is drawn solid with its own border.
auto Tray::Native::apply_window_look(HWND t_window) -> void
{
	const BOOL dark = colors.dark ? TRUE : FALSE;
	DwmSetWindowAttribute(t_window, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));

	const DWM_WINDOW_CORNER_PREFERENCE corners = DWMWCP_ROUND;
	framed                                     = FAILED(DwmSetWindowAttribute(t_window, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof(corners)));

	const COLORREF border = RGB(colors.border.r, colors.border.g, colors.border.b);
	DwmSetWindowAttribute(t_window, DWMWA_BORDER_COLOR, &border, sizeof(border));

	const bool                    wants_glass = colors.background.a < 255;
	const DWM_SYSTEMBACKDROP_TYPE backdrop    = wants_glass ? DWMSBT_TRANSIENTWINDOW : DWMSBT_NONE;
	see_through = SUCCEEDED(DwmSetWindowAttribute(t_window, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop))) && wants_glass;

	const MARGINS margins = see_through ? MARGINS{-1, -1, -1, -1} : MARGINS{0, 0, 0, 0};
	DwmExtendFrameIntoClientArea(t_window, &margins);
}

auto Tray::Native::show_menu() -> void
{
	if (!ensure_popup()) return;

	close_menu();

	POINT cursor;
	GetCursorPos(&cursor);

	menu        = TrayMenu{};
	menu.locked = locked;
	if (fill_menu && !locked) {
		fill_menu(&menu);
	}

	const HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
	MONITORINFO    info{.cbSize = sizeof(MONITORINFO)};
	GetMonitorInfoW(monitor, &info);
	work = info.rcWork;

	UINT dpi_x = 96;
	UINT dpi_y = 96;
	GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y);

	const float new_scale = static_cast<float>(dpi_x) / K_REFERENCE_DPI;
	if (new_scale != scale) {
		scale = new_scale;
		games.logo.Reset();

		for (ComPtr<ID2D1Bitmap>& game_icon : games.game_icons) {
			game_icon.Reset();
		}
	}

	build_games();
	lay_out(&games, K_GAMES_MIN_WIDTH, true);
	apply_window_look(games.window);
	apply_window_look(accounts.window);

	SIZE size = pixel_size(games);
	RECT placed{};
	CalculatePopupWindowPosition(&cursor, &size, TPM_LEFTALIGN | TPM_BOTTOMALIGN | TPM_WORKAREA, nullptr, &placed);

	open      = true;
	activated = false;
	open_row  = -1;
	games.hot = -1;

	SetWindowPos(games.window, HWND_TOPMOST, placed.left, placed.top, size.cx, size.cy, SWP_SHOWWINDOW);
	SetForegroundWindow(games.window);
	InvalidateRect(games.window, nullptr, FALSE);
	SetTimer(games.window, K_WATCH_TIMER, K_WATCH_INTERVAL_MS, nullptr);
}

auto Tray::Native::close_menu() -> void
{
	if (!open) return;

	open             = false;
	animating        = false;
	open_row         = -1;
	accounts_focused = false;
	games.hot        = -1;
	accounts.hot     = -1;

	KillTimer(games.window, K_ANIMATION_TIMER);
	KillTimer(games.window, K_AIM_TIMER);
	KillTimer(games.window, K_WATCH_TIMER);

	ShowWindow(accounts.window, SW_HIDE);
	ShowWindow(games.window, SW_HIDE);
}

auto Tray::Native::choose(const Row& t_row) -> void
{
	TrayEvent event{.type = t_row.action};

	if (t_row.kind == RowKind::ACCOUNT) {
		if (locked || t_row.item < 0 || static_cast<u32>(t_row.item) >= menu.account_count) return;

		const TrayAccount& account = menu.accounts[t_row.item];
		event                      = TrayEvent{.type = TrayEventType::QUICK_LOGIN, .game = account.game, .row = account.row};
	}

	close_menu();
	pending_event = event;
}

// The accounts open beside the games and overlap them a little, so the pointer never crosses a gap on its way over, with the first account level
// with the game it belongs to.
auto Tray::Native::open_accounts(i32 t_row) -> void
{
	open_row         = t_row;
	accounts_focused = false;

	build_accounts(games.rows[t_row].item);
	lay_out(&accounts, K_ACCOUNTS_WIDTH, false);

	RECT games_rect;
	GetWindowRect(games.window, &games_rect);

	const SIZE size    = pixel_size(accounts);
	const auto overlap = static_cast<LONG>(std::lround(K_ACCOUNTS_OVERLAP * scale));
	LONG       x       = games_rect.right - overlap;
	LONG       y       = games_rect.top + static_cast<LONG>(std::lround((games.rows[t_row].top - K_PANEL_PADDING) * scale));

	if (x + size.cx > work.right) {
		x = games_rect.left + overlap - size.cx;
	}

	x = std::max(x, work.left);
	y = std::clamp(y, work.top, std::max(work.top, work.bottom - size.cy));

	SetWindowPos(accounts.window, HWND_TOPMOST, x, y, size.cx, size.cy, SWP_SHOWWINDOW | SWP_NOACTIVATE);
	InvalidateRect(accounts.window, nullptr, FALSE);
	start_animation();
}

auto Tray::Native::close_accounts() -> void
{
	if (open_row < 0) return;

	open_row         = -1;
	accounts_focused = false;
	accounts.hot     = -1;
	ShowWindow(accounts.window, SW_HIDE);
	start_animation();
}

auto Tray::Native::enter_accounts() -> void
{
	if (games.hot < 0 || games.rows[games.hot].kind != RowKind::GAME) return;

	if (open_row != games.hot) {
		open_accounts(games.hot);
	}

	accounts_focused = true;
	accounts.hot     = -1;
	step_hot(&accounts, 1);
}

auto Tray::Native::back_to_games() -> void
{
	const i32 row = open_row;

	close_accounts();
	games.hot = row;
	start_animation();
}

auto Tray::Native::point_at_game(i32 t_row) -> void
{
	accounts_focused = false;

	if (t_row >= 0 && games.rows[t_row].kind == RowKind::GAME) {
		if (t_row != open_row) {
			open_accounts(t_row);
		}

		games.hot = t_row;
	} else {
		close_accounts();
		games.hot = t_row >= 0 && is_selectable(games.rows[t_row]) ? t_row : -1;
	}

	start_animation();
}

// While the pointer heads from a game toward its accounts it may cut across other games, which shouldn't steal the open list from under it.
auto Tray::Native::aims_at_accounts(POINT t_point) const -> bool
{
	if (open_row < 0) return false;

	RECT games_rect;
	RECT accounts_rect;
	GetWindowRect(games.window, &games_rect);
	GetWindowRect(accounts.window, &accounts_rect);

	const LONG  edge = accounts_rect.left >= games_rect.left ? accounts_rect.left : accounts_rect.right;
	const auto  slop = static_cast<LONG>(std::lround(K_AIM_SLOP * scale));
	const POINT top{edge, accounts_rect.top - slop};
	const POINT bottom{edge, accounts_rect.bottom + slop};

	return inside_triangle(t_point, last_pointer, top, bottom);
}

auto Tray::Native::row_at(const Panel& t_panel, float t_y) const -> i32
{
	for (usize i = 0; i < t_panel.rows.size(); i += 1) {
		const Row& row = t_panel.rows[i];
		if (t_y >= row.top && t_y < row.top + row.height) return static_cast<i32>(i);
	}

	return -1;
}

auto Tray::Native::paint(Panel* t_panel) -> void
{
	PAINTSTRUCT paint_info;
	BeginPaint(t_panel->window, &paint_info);
	EndPaint(t_panel->window, &paint_info);

	RECT client;
	GetClientRect(t_panel->window, &client);

	const D2D1_SIZE_U size = D2D1::SizeU(static_cast<UINT32>(client.right), static_cast<UINT32>(client.bottom));
	const float       dpi  = K_REFERENCE_DPI * scale;

	if (!t_panel->target) {
		const D2D1_RENDER_TARGET_PROPERTIES properties = D2D1::RenderTargetProperties(
			D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), dpi, dpi);
		if (FAILED(factory->CreateHwndRenderTarget(properties, D2D1::HwndRenderTargetProperties(t_panel->window, size), &t_panel->target))) return;

		t_panel->target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
	} else {
		const D2D1_SIZE_U current = t_panel->target->GetPixelSize();
		if (current.width != size.width || current.height != size.height) {
			t_panel->target->Resize(size);
		}

		t_panel->target->SetDpi(dpi, dpi);
	}

	ID2D1HwndRenderTarget* target = t_panel->target.Get();

	if (t_panel == &games) {
		if (!games.logo) {
			games.logo = make_bitmap(target, logo_source, K_LOGO_SIZE, 0.0f, scale);
		}

		for (const Row& row : games.rows) {
			if (row.icon < 0 || static_cast<u32>(row.icon) >= K_TRAY_MAX_GAMES || games.game_icons[row.icon]) continue;

			games.game_icons[row.icon] = make_bitmap(target, game_icon_sources[row.icon], K_ICON_SIZE, K_ICON_RADIUS, scale);
		}
	}

	target->BeginDraw();
	paint_panel(target, painter(), *t_panel);

	if (target->EndDraw() == D2DERR_RECREATE_TARGET) {
		t_panel->target.Reset();
		t_panel->logo.Reset();

		for (ComPtr<ID2D1Bitmap>& game_icon : t_panel->game_icons) {
			game_icon.Reset();
		}

		InvalidateRect(t_panel->window, nullptr, FALSE);
	}
}

auto Tray::Native::on_pointer(Panel* t_panel, LPARAM t_lparam) -> void
{
	if (!t_panel->tracking_leave) {
		TRACKMOUSEEVENT track{.cbSize = sizeof(TRACKMOUSEEVENT), .dwFlags = TME_LEAVE, .hwndTrack = t_panel->window};
		t_panel->tracking_leave = TrackMouseEvent(&track) != FALSE;
	}

	const float x   = static_cast<float>(static_cast<short>(LOWORD(t_lparam))) / scale;
	const float y   = static_cast<float>(static_cast<short>(HIWORD(t_lparam))) / scale;
	const i32   row = row_at(*t_panel, y);

	if (t_panel == &accounts) {
		KillTimer(games.window, K_AIM_TIMER);

		const bool login_hot = row >= 0 && over_login(accounts.rows[row], x, y);
		if (login_hot != accounts.login_hot) {
			SetCursor(LoadCursorW(nullptr, login_hot ? IDC_HAND : IDC_ARROW));
		}

		accounts_focused   = false;
		accounts.hot       = row >= 0 && is_selectable(accounts.rows[row]) ? row : -1;
		accounts.login_hot = login_hot;
		games.hot          = open_row;
		start_animation();
		return;
	}

	POINT screen;
	GetCursorPos(&screen);

	const bool aiming = row != open_row && aims_at_accounts(screen);
	last_pointer      = screen;

	if (aiming) {
		SetTimer(games.window, K_AIM_TIMER, K_AIM_DELAY_MS, nullptr);
		return;
	}

	KillTimer(games.window, K_AIM_TIMER);
	point_at_game(row);
}

auto Tray::Native::on_leave(Panel* t_panel) -> void
{
	t_panel->tracking_leave = false;

	if (t_panel == &accounts) {
		accounts.login_hot = false;

		if (!accounts_focused) {
			accounts.hot = -1;
		}
	} else {
		games.hot = open_row;
	}

	start_animation();
}

auto Tray::Native::on_click(Panel* t_panel, LPARAM t_lparam) -> void
{
	const float x   = static_cast<float>(static_cast<short>(LOWORD(t_lparam))) / scale;
	const float y   = static_cast<float>(static_cast<short>(HIWORD(t_lparam))) / scale;
	const i32   row = row_at(*t_panel, y);
	if (row < 0) return;

	const Row& clicked = t_panel->rows[row];

	if (clicked.kind == RowKind::GAME) {
		KillTimer(games.window, K_AIM_TIMER);
		point_at_game(row);
	} else if (clicked.kind == RowKind::ACTION || (clicked.kind == RowKind::ACCOUNT && over_login(clicked, x, y))) {
		choose(clicked);
	}
}

auto Tray::Native::over_login(const Row& t_row, float t_x, float t_y) const -> bool
{
	if (t_row.kind != RowKind::ACCOUNT) return false;

	RECT client;
	GetClientRect(accounts.window, &client);

	const D2D1_RECT_F login = login_rect(t_row, static_cast<float>(client.right) / scale, login_width, scale);

	return t_x >= login.left && t_x < login.right && t_y >= login.top && t_y < login.bottom;
}

auto Tray::Native::on_key(WPARAM t_key) -> void
{
	Panel* panel = accounts_focused ? &accounts : &games;

	switch (t_key) {
		case VK_ESCAPE: {
			if (accounts_focused) {
				back_to_games();
			} else {
				close_menu();
			}

			break;
		}

		case VK_LEFT: {
			if (accounts_focused) {
				back_to_games();
			}

			break;
		}

		case VK_RIGHT: {
			if (!accounts_focused) {
				enter_accounts();
			}

			break;
		}

		case VK_UP:
		case VK_DOWN: {
			step_hot(panel, t_key == VK_DOWN ? 1 : -1);

			if (!accounts_focused && open_row >= 0 && games.hot != open_row) {
				close_accounts();
			}

			break;
		}

		case VK_RETURN:
		case VK_SPACE: {
			if (panel->hot < 0) break;

			const Row& row = panel->rows[panel->hot];

			if (row.kind == RowKind::GAME) {
				enter_accounts();
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

auto Tray::Native::step_hot(Panel* t_panel, i32 t_step) -> void
{
	const auto count = static_cast<i32>(t_panel->rows.size());
	if (count == 0) return;

	i32 row = t_panel->hot < 0 ? (t_step > 0 ? -1 : count) : t_panel->hot;

	for (i32 tries = 0; tries < count; tries += 1) {
		row = (row + t_step + count) % count;

		if (is_selectable(t_panel->rows[row])) {
			t_panel->hot = row;
			break;
		}
	}

	start_animation();
}

// The games panel normally closes when it loses focus. If Windows didn't let it take focus when it opened, a click anywhere else closes it instead.
auto Tray::Native::watch() -> void
{
	const HWND foreground = GetForegroundWindow();
	if (foreground == games.window || foreground == accounts.window) return;

	if (activated) {
		close_menu();
		return;
	}

	const bool pressed = ((GetAsyncKeyState(VK_LBUTTON) | GetAsyncKeyState(VK_RBUTTON) | GetAsyncKeyState(VK_MBUTTON)) & 0x8000) != 0;
	if (!pressed) return;

	POINT cursor;
	GetCursorPos(&cursor);

	const HWND under = WindowFromPoint(cursor);
	if (under != games.window && under != accounts.window) {
		close_menu();
	}
}

auto Tray::Native::start_animation() -> void
{
	if (!open) return;

	if (!animating) {
		animating = true;
		last_tick = Clock::now();
		SetTimer(games.window, K_ANIMATION_TIMER, K_ANIMATION_TICK_MS, nullptr);
	}

	InvalidateRect(games.window, nullptr, FALSE);
	InvalidateRect(accounts.window, nullptr, FALSE);
}

auto Tray::Native::animate() -> void
{
	const Clock::time_point now     = Clock::now();
	const float             seconds = std::min(std::chrono::duration<float>(now - last_tick).count(), 0.05f);
	const float             blend   = 1.0f - std::exp(-K_HOVER_EASE_RATE * seconds);
	bool                    moving  = false;

	last_tick = now;

	const auto ease = [&](Panel* t_panel, i32 t_lit_row) {
		for (usize i = 0; i < t_panel->rows.size(); i += 1) {
			Row&        row    = t_panel->rows[i];
			const bool  lit    = (static_cast<i32>(i) == t_panel->hot || static_cast<i32>(i) == t_lit_row) && is_selectable(row);
			const float target = lit ? 1.0f : 0.0f;

			row.hover += (target - row.hover) * blend;

			if (std::abs(target - row.hover) < K_SETTLED) {
				row.hover = target;
			} else {
				moving = true;
			}
		}
	};

	ease(&games, open_row);
	ease(&accounts, -1);

	InvalidateRect(games.window, nullptr, FALSE);
	InvalidateRect(accounts.window, nullptr, FALSE);

	if (!moving) {
		animating = false;
		KillTimer(games.window, K_ANIMATION_TIMER);
	}
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
				show_menu();
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

auto Tray::Native::handle_popup_message(HWND t_window, UINT t_message, WPARAM t_wparam, LPARAM t_lparam) -> LRESULT
{
	if (t_window == toast.window && toast.window != nullptr) return handle_toast_message(t_message, t_wparam, t_lparam);

	Panel* panel = nullptr;
	if (t_window == games.window) {
		panel = &games;
	} else if (t_window == accounts.window) {
		panel = &accounts;
	}

	if (panel == nullptr) return DefWindowProcW(t_window, t_message, t_wparam, t_lparam);

	switch (t_message) {
		case WM_PAINT: {
			paint(panel);
			return 0;
		}

		case WM_ERASEBKGND: {
			return 1;
		}

		case WM_MOUSEACTIVATE: {
			return panel == &accounts ? MA_NOACTIVATE : MA_ACTIVATE;
		}

		// Losing focus to anything but the accounts panel closes the menu, the same as clicking outside a system menu.
		case WM_ACTIVATE: {
			if (panel == &games && LOWORD(t_wparam) != WA_INACTIVE) {
				activated = true;
			} else if (panel == &games && reinterpret_cast<HWND>(t_lparam) != accounts.window) {
				close_menu();
			}

			break;
		}

		case WM_MOUSEMOVE: {
			on_pointer(panel, t_lparam);
			return 0;
		}

		case WM_MOUSELEAVE: {
			on_leave(panel);
			return 0;
		}

		case WM_LBUTTONUP: {
			on_click(panel, t_lparam);
			return 0;
		}

		case WM_SETCURSOR: {
			if (panel == &accounts && accounts.login_hot) {
				SetCursor(LoadCursorW(nullptr, IDC_HAND));
				return TRUE;
			}

			break;
		}

		case WM_KEYDOWN: {
			on_key(t_wparam);
			return 0;
		}

		case WM_TIMER: {
			if (t_wparam == K_ANIMATION_TIMER) {
				animate();
			} else if (t_wparam == K_WATCH_TIMER) {
				watch();
			} else if (t_wparam == K_AIM_TIMER) {
				KillTimer(games.window, K_AIM_TIMER);

				POINT cursor;
				GetCursorPos(&cursor);

				if (WindowFromPoint(cursor) == games.window) {
					ScreenToClient(games.window, &cursor);
					point_at_game(row_at(games, static_cast<float>(cursor.y) / scale));
				}
			}

			return 0;
		}

		default: {
			break;
		}
	}

	return DefWindowProcW(t_window, t_message, t_wparam, t_lparam);
}

auto Tray::Native::ensure_toast() -> bool
{
	if (toast.target) return true;
	if (!ensure_popup()) return false;

	if (toast.window == nullptr) {
		const HINSTANCE   instance = GetModuleHandleW(nullptr);
		const WNDCLASSEXW toast_class{
			.cbSize        = sizeof(WNDCLASSEXW),
			.lpfnWndProc   = popup_proc,
			.hInstance     = instance,
			.hCursor       = LoadCursorW(nullptr, IDC_ARROW),
			.lpszClassName = os::win32::K_LOGIN_TOAST_CLASS_NAME,
		};

		if (RegisterClassExW(&toast_class) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
			debug_log::write(K_LOG_CATEGORY, "RegisterClassExW for the login toast failed, err=%lu", GetLastError());
			return false;
		}

		// Layered so it can fade and carry its own soft shadow. It never takes focus, which would pull the game or the Riot Client out of the front.
		toast.window = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE, os::win32::K_LOGIN_TOAST_CLASS_NAME, L"", WS_POPUP,
		                               0, 0, 1, 1, nullptr, nullptr, instance, this);
		toast.canvas = CreateCompatibleDC(nullptr);
	}

	if (toast.window == nullptr || toast.canvas == nullptr || !create_toast_target()) {
		debug_log::write(K_LOG_CATEGORY, "failed to create the login toast, err=%lu", GetLastError());
		return false;
	}

	return true;
}

auto Tray::Native::create_toast_target() -> bool
{
	const D2D1_RENDER_TARGET_PROPERTIES properties =
		D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));

	toast.logo.Reset();
	if (FAILED(factory->CreateDCRenderTarget(&properties, &toast.target))) return false;

	toast.target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

	return true;
}

auto Tray::Native::show_login(const TrayLogin& t_login) -> void
{
	if (!ensure_toast()) return;

	const bool starting = !toast.active;
	const bool changed  = starting || t_login.state != toast.login.state;

	toast.login   = t_login;
	toast.status  = win32::to_wide(t_login.status);
	toast.account = win32::to_wide(t_login.account);
	toast.leaving = false;

	if (toast.status.ends_with(L"...")) {
		toast.status.replace(toast.status.size() - 3, 3, 1, K_ELLIPSIS);
	}

	if (starting) {
		const HMONITOR monitor = MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
		MONITORINFO    info{.cbSize = sizeof(MONITORINFO)};
		GetMonitorInfoW(monitor, &info);

		UINT dpi_x = 96;
		UINT dpi_y = 96;
		GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y);

		const float new_scale = static_cast<float>(dpi_x) / K_REFERENCE_DPI;
		if (new_scale != toast.scale) {
			toast.logo.Reset();
		}

		toast.scale           = new_scale;
		toast.work            = info.rcWork;
		toast.appear          = 0.0f;
		toast.card            = t_login.state == TrayLoginState::FAILED ? 1.0f : 0.0f;
		toast.done            = t_login.state == TrayLoginState::SUCCEEDED ? 1.0f : 0.0f;
		toast.bar             = t_login.progress;
		toast.hovered         = false;
		toast.hot             = -1;
		toast.button_hover[0] = 0.0f;
		toast.button_hover[1] = 0.0f;
		toast.active          = true;
		toast.last_tick       = Clock::now();
	}

	if (t_login.state == TrayLoginState::FAILED) {
		const float        width      = K_TOAST_CARD_WIDTH - K_TOAST_PADDING * 2.0f - K_TOAST_LOGO_SIZE - K_TOAST_ICON_GAP;
		const std::wstring open_label = std::wstring{L"Open "} + win32::K_APP_NAME_WIDE;
		toast.message_height          = std::min(text_height(writer.Get(), formats.message.Get(), toast.status, width), K_TOAST_MESSAGE_MAX);
		toast.retry_width             = text_width(writer.Get(), formats.button.Get(), K_RETRY_LABEL) + K_TOAST_BUTTON_PADDING * 2.0f;
		toast.open_width              = text_width(writer.Get(), formats.button.Get(), open_label) + K_TOAST_BUTTON_PADDING * 2.0f;
		toast.card_height =
			K_TOAST_PADDING * 2.0f + K_TOAST_HEADER_HEIGHT + K_TOAST_MESSAGE_GAP + toast.message_height + K_TOAST_BUTTON_GAP + K_TOAST_BUTTON_HEIGHT;
	}

	if (changed) {
		toast.linger = t_login.state == TrayLoginState::SUCCEEDED ? K_TOAST_SUCCESS_SECONDS : K_TOAST_ERROR_SECONDS;
	}

	if (starting) {
		render_toast();
		ShowWindow(toast.window, SW_SHOWNOACTIVATE);
		SetTimer(toast.window, K_TOAST_TIMER, K_TOAST_TICK_MS, nullptr);
	}
}

auto Tray::Native::hide_login() -> void
{
	if (toast.active) {
		toast.leaving = true;
	}
}

// Room for the card when the login failed or the pill is still growing back from it, and for the shadow all round.
auto Tray::Native::toast_size() const -> SIZE
{
	const bool  card   = toast.card > 0.0f || toast.login.state == TrayLoginState::FAILED;
	const float width  = (card ? K_TOAST_CARD_WIDTH : K_TOAST_PILL_WIDTH) + K_TOAST_SHADOW * 2.0f;
	const float height = (card ? std::max(K_TOAST_PILL_HEIGHT, toast.card_height) : K_TOAST_PILL_HEIGHT) + K_TOAST_SHADOW * 2.0f;

	return SIZE{static_cast<LONG>(std::ceil(width * toast.scale)), static_cast<LONG>(std::ceil(height * toast.scale))};
}

auto Tray::Native::toast_hit(LPARAM t_lparam) const -> i32
{
	const float       x     = static_cast<float>(static_cast<short>(LOWORD(t_lparam))) / toast.scale;
	const float       y     = static_cast<float>(static_cast<short>(HIWORD(t_lparam))) / toast.scale;
	const D2D1_RECT_F shape = toast_shape(toast, static_cast<float>(toast.size.cx) / toast.scale, static_cast<float>(toast.size.cy) / toast.scale);

	if (!contains(shape, x, y)) return -1;
	if (toast.login.state != TrayLoginState::FAILED) return K_TOAST_BODY;
	if (toast.card < 1.0f) return -1;

	const ToastCard card = toast_card(toast, shape);
	if (contains(card.retry, x, y)) return K_TOAST_RETRY;
	if (contains(card.open, x, y)) return K_TOAST_OPEN;

	return -1;
}

auto Tray::Native::render_toast() -> void
{
	if (!toast.target) return;

	const SIZE size = toast_size();

	if (toast.pixels == nullptr || size.cx != toast.size.cx || size.cy != toast.size.cy) {
		if (toast.pixels != nullptr) {
			SelectObject(toast.canvas, toast.replaced);
			DeleteObject(toast.pixels);
		}

		BITMAPINFO format{};
		format.bmiHeader = BITMAPINFOHEADER{
			.biSize        = sizeof(BITMAPINFOHEADER),
			.biWidth       = size.cx,
			.biHeight      = -size.cy,
			.biPlanes      = 1,
			.biBitCount    = 32,
			.biCompression = BI_RGB,
		};

		void* bits   = nullptr;
		toast.pixels = CreateDIBSection(toast.canvas, &format, DIB_RGB_COLORS, &bits, nullptr, 0);
		if (toast.pixels == nullptr) return;

		toast.replaced = SelectObject(toast.canvas, toast.pixels);
		toast.size     = size;
	}

	const RECT  bounds{0, 0, size.cx, size.cy};
	const float dpi = K_REFERENCE_DPI * toast.scale;

	toast.target->BindDC(toast.canvas, &bounds);
	toast.target->SetDpi(dpi, dpi);

	if (!toast.logo) {
		toast.logo = make_bitmap(toast.target.Get(), logo_source, K_TOAST_LOGO_SIZE, 0.0f, toast.scale);
	}

	Painter look = painter();
	look.scale   = toast.scale;

	toast.target->BeginDraw();
	paint_toast(toast.target.Get(), look, toast);

	if (toast.target->EndDraw() == D2DERR_RECREATE_TARGET) {
		toast.target.Reset();
		create_toast_target();
		return;
	}

	const float   shown = eased_out(toast.appear);
	const auto    inset = static_cast<LONG>(std::lround((K_TOAST_SHADOW - K_TOAST_MARGIN) * toast.scale));
	const auto    slide = static_cast<LONG>(std::lround((1.0f - shown) * K_TOAST_SLIDE * toast.scale));
	POINT         position{toast.work.right + inset - size.cx, toast.work.bottom + inset - size.cy + slide};
	POINT         origin{0, 0};
	SIZE          extent = size;
	BLENDFUNCTION blend{AC_SRC_OVER, 0, static_cast<BYTE>(std::lround(shown * 255.0f)), AC_SRC_ALPHA};

	UpdateLayeredWindow(toast.window, nullptr, &position, &extent, toast.canvas, &origin, 0, &blend, ULW_ALPHA);
}

// Slides and fades the toast in, grows it into the card when the login fails, and lets it go a while after the login ends. The pointer resting
// on it holds it open.
auto Tray::Native::step_toast() -> void
{
	const Clock::time_point now     = Clock::now();
	const float             seconds = std::min(std::chrono::duration<float>(now - toast.last_tick).count(), 0.05f);
	const TrayLoginState    state   = toast.login.state;
	const float             blend   = 1.0f - std::exp(-K_TOAST_BAR_EASE_RATE * seconds);
	const float             hover   = 1.0f - std::exp(-K_HOVER_EASE_RATE * seconds);

	toast.last_tick = now;

	if (state != TrayLoginState::RUNNING && !toast.hovered && !toast.leaving) {
		toast.linger -= seconds;

		if (toast.linger <= 0.0f) {
			toast.leaving = true;
		}
	}

	toast.appear = approached(toast.appear, toast.leaving ? 0.0f : 1.0f, seconds / K_TOAST_IN_SECONDS);
	toast.card   = approached(toast.card, state == TrayLoginState::FAILED ? 1.0f : 0.0f, seconds / K_TOAST_GROW_SECONDS);
	toast.done   = approached(toast.done, state == TrayLoginState::SUCCEEDED ? 1.0f : 0.0f, seconds / K_TOAST_DONE_SECONDS);
	toast.bar += ((state == TrayLoginState::SUCCEEDED ? 1.0f : toast.login.progress) - toast.bar) * blend;

	for (i32 button = 0; button < 2; button += 1) {
		toast.button_hover[button] += ((toast.hot == button ? 1.0f : 0.0f) - toast.button_hover[button]) * hover;
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

			const float x = static_cast<float>(static_cast<short>(LOWORD(t_lparam))) / toast.scale;
			const float y = static_cast<float>(static_cast<short>(HIWORD(t_lparam))) / toast.scale;

			toast.hot = toast_hit(t_lparam);
			toast.hovered =
				contains(toast_shape(toast, static_cast<float>(toast.size.cx) / toast.scale, static_cast<float>(toast.size.cy) / toast.scale), x, y);
			SetCursor(LoadCursorW(nullptr, toast.hot >= 0 ? IDC_HAND : IDC_ARROW));
			return 0;
		}

		case WM_MOUSELEAVE: {
			toast.tracking_leave = false;
			toast.hovered        = false;
			toast.hot            = -1;
			return 0;
		}

		case WM_SETCURSOR: {
			SetCursor(LoadCursorW(nullptr, toast.hot >= 0 ? IDC_HAND : IDC_ARROW));
			return TRUE;
		}

		case WM_LBUTTONUP: {
			const i32 hit = toast_hit(t_lparam);

			if (hit == K_TOAST_RETRY) {
				pending_event = TrayEvent{.type = TrayEventType::RETRY_LOGIN};
				toast.linger  = K_TOAST_ERROR_SECONDS;
			} else if (hit == K_TOAST_OPEN || hit == K_TOAST_BODY) {
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

	return native->handle_popup_message(t_window, t_message, t_wparam, t_lparam);
}

}
