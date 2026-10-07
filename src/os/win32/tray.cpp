#include "os/tray.h"

#include <algorithm>
#include <cwchar>
#include <string>
#include <utility>
#include <vector>

#include <Windows.h>
#include <shellapi.h>

#include "core/app_identity.h"
#include "core/debug_log.h"
#include "os/win32/win32.h"
#include "stb/stb_image.h"

namespace {
constexpr const char* K_LOG_CATEGORY = "tray";

constexpr UINT     K_TRAY_CALLBACK_MESSAGE      = WM_APP + 1;
constexpr UINT_PTR K_TRAY_ICON_ID               = 1;
constexpr UINT_PTR K_ADD_ICON_RETRY_TIMER       = 1;
constexpr UINT     K_ADD_ICON_RETRY_INTERVAL_MS = 1000;
constexpr u32      K_MAX_ADD_ICON_ATTEMPTS      = 10;

constexpr UINT K_SHOW_COMMAND              = 1001;
constexpr UINT K_EXIT_COMMAND              = 1002;
constexpr UINT K_PLACEHOLDER_COMMAND       = 1003;
constexpr UINT K_FIRST_QUICK_LOGIN_COMMAND = 2000;

constexpr int K_ROW_HEIGHT          = 26;
constexpr int K_SEPARATOR_HEIGHT    = 7;
constexpr int K_PADDING_X           = 12;
constexpr int K_SUBMENU_ARROW_WIDTH = 18;
constexpr int K_ICON_GAP            = 8;
constexpr int K_MIN_ROW_WIDTH       = 170;

constexpr float K_LOCKED_ICON_OPACITY = 0.55f;

[[nodiscard]] auto menu_icon_size() -> int
{
	return os::win32::app_icon_pixel_size(os::win32::AppIconSize::SMALL_ICON) * 3 / 2;
}

[[nodiscard]] auto taskbar_created_message() -> UINT
{
	static const UINT MESSAGE = RegisterWindowMessageW(L"TaskbarCreated");

	return MESSAGE;
}

[[nodiscard]] auto to_colorref(Color t_color) -> COLORREF
{
	return RGB(t_color.r, t_color.g, t_color.b);
}

auto utf8_to_wide(const char* t_utf8, wchar_t* t_out, int t_capacity) -> void
{
	if (MultiByteToWideChar(CP_UTF8, 0, t_utf8, -1, t_out, t_capacity) <= 0) {
		t_out[0] = L'\0';
	}
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

[[nodiscard]] auto decode_icon_bitmap(std::span<const u8> t_png, int t_size) -> HBITMAP
{
	int width    = 0;
	int height   = 0;
	int channels = 0;
	u8* pixels   = stbi_load_from_memory(t_png.data(), static_cast<int>(t_png.size()), &width, &height, &channels, 4);
	if (pixels == nullptr) return nullptr;

	BITMAPINFO info{};
	info.bmiHeader.biSize        = sizeof(info.bmiHeader);
	info.bmiHeader.biWidth       = t_size;
	info.bmiHeader.biHeight      = -t_size;
	info.bmiHeader.biPlanes      = 1;
	info.bmiHeader.biBitCount    = 32;
	info.bmiHeader.biCompression = BI_RGB;

	void*         bits   = nullptr;
	const HBITMAP bitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);

	if (bitmap != nullptr && bits != nullptr) {
		auto* out = static_cast<u8*>(bits);

		for (int y = 0; y < t_size; y += 1) {
			const int source_y0 = y * height / t_size;
			const int source_y1 = std::max((y + 1) * height / t_size, source_y0 + 1);

			for (int x = 0; x < t_size; x += 1) {
				const int source_x0 = x * width / t_size;
				const int source_x1 = std::max((x + 1) * width / t_size, source_x0 + 1);

				u32 sum[4]{};
				u32 samples = 0;
				for (int sy = source_y0; sy < std::min(source_y1, height); sy += 1) {
					for (int sx = source_x0; sx < std::min(source_x1, width); sx += 1) {
						const u8* texel = pixels + (static_cast<usize>(sy) * width + sx) * 4;
						for (int channel = 0; channel < 4; channel += 1) {
							sum[channel] += texel[channel];
						}

						samples += 1;
					}
				}

				const u32  alpha         = samples > 0 ? sum[3] / samples : 0;
				const auto premultiplied = [&](int t_channel) { return static_cast<u8>(samples > 0 ? sum[t_channel] / samples * alpha / 255 : 0); };

				u8* destination = out + (static_cast<usize>(y) * t_size + x) * 4;
				destination[0]  = premultiplied(2);
				destination[1]  = premultiplied(1);
				destination[2]  = premultiplied(0);
				destination[3]  = static_cast<u8>(alpha);
			}
		}
	}

	stbi_image_free(pixels);

	return bitmap;
}
}

namespace os {

struct Tray::Native {
	struct MenuRow {
		wchar_t label[96];
		HBITMAP icon;
		bool    indented;
		bool    submenu;
		bool    separator;
		bool    disabled;
	};

	static constexpr u32 K_MAX_MENU_ROWS = K_TRAY_MAX_ACCOUNTS + K_TRAY_MAX_GAMES + 8;

	HWND         window       = nullptr;
	HICON        icon         = nullptr;
	HICON        locked_icon  = nullptr;
	bool         locked       = false;
	bool         icon_added   = false;
	u32          add_attempts = 0;
	std::wstring tooltip;

	HFONT      menu_font        = nullptr;
	bool       owns_menu_font   = false;
	HBRUSH     background_brush = nullptr;
	HBRUSH     hover_brush      = nullptr;
	TrayColors colors{
		.background    = {32, 32, 36, 255},
		.hover         = {68, 60, 124, 255},
		.text          = {232, 232, 236, 255},
		.text_disabled = {108, 108, 116, 255},
		.separator     = {50, 50, 56, 255},
	};

	std::span<const u8> game_icon_sources[K_TRAY_MAX_GAMES]{};
	HBITMAP             game_icons[K_TRAY_MAX_GAMES]{};

	TrayEvent pending_event{};

	std::function<void(TrayMenu*)> fill_menu;
	TrayMenu                       menu{};
	MenuRow                        rows[K_MAX_MENU_ROWS]{};
	u32                            row_count = 0;

	static auto CALLBACK window_proc(HWND t_window, UINT t_message, WPARAM t_wparam, LPARAM t_lparam) -> LRESULT;

	[[nodiscard]] auto handle_message(UINT t_message, WPARAM t_wparam, LPARAM t_lparam) -> LRESULT;
	auto handle_command(UINT t_command) -> void;

	auto add_icon() -> bool;
	auto remove_icon() -> void;
	auto update_icon() -> void;
	[[nodiscard]] auto shown_icon() const -> HICON;
	auto fill_tooltip(wchar_t (&t_tooltip)[128]) const -> void;
	auto rebuild_brushes() -> void;

	auto show_menu() -> void;
	[[nodiscard]] auto build_menu() -> HMENU;
	[[nodiscard]] auto build_game_submenu(const TrayGame& t_game) -> HMENU;
	[[nodiscard]] auto game_icon(i32 t_game) -> HBITMAP;

	auto append_row(HMENU t_menu, UINT t_flags, UINT_PTR t_id, const MenuRow& t_row) -> void;

	auto measure_row(MEASUREITEMSTRUCT* t_measure) const -> void;
	auto draw_row(const DRAWITEMSTRUCT* t_draw) const -> void;
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

	if (native->owns_menu_font) {
		DeleteObject(native->menu_font);
	}

	if (native->locked_icon != nullptr) {
		DestroyIcon(native->locked_icon);
	}

	DeleteObject(native->background_brush);
	DeleteObject(native->hover_brush);

	for (const HBITMAP icon : native->game_icons) {
		if (icon != nullptr) {
			DeleteObject(icon);
		}
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

	NONCLIENTMETRICSW metrics{.cbSize = sizeof(NONCLIENTMETRICSW)};
	if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0)) {
		native->menu_font      = CreateFontIndirectW(&metrics.lfMenuFont);
		native->owns_menu_font = native->menu_font != nullptr;
	}

	if (native->menu_font == nullptr) {
		native->menu_font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
	}

	native->rebuild_brushes();
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

	if (native->locked && native->locked_icon == nullptr) {
		native->locked_icon = greyscale_icon(native->icon);
	}

	native->update_icon();
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
	if (t_colors == m_native->colors) return;

	m_native->colors = t_colors;
	m_native->rebuild_brushes();
}

auto Tray::set_game_icon(u32 t_game, std::span<const u8> t_png) -> void
{
	if (t_game >= K_TRAY_MAX_GAMES) return;

	Native* native                    = m_native.get();
	native->game_icon_sources[t_game] = t_png;

	if (native->game_icons[t_game] != nullptr) {
		DeleteObject(native->game_icons[t_game]);
		native->game_icons[t_game] = nullptr;
	}
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

auto Tray::Native::rebuild_brushes() -> void
{
	DeleteObject(background_brush);
	DeleteObject(hover_brush);

	background_brush = CreateSolidBrush(to_colorref(colors.background));
	hover_brush      = CreateSolidBrush(to_colorref(colors.hover));
}

auto Tray::Native::game_icon(i32 t_game) -> HBITMAP
{
	if (t_game < 0 || static_cast<u32>(t_game) >= K_TRAY_MAX_GAMES) return nullptr;

	HBITMAP* bitmap = &game_icons[t_game];
	if (*bitmap == nullptr && !game_icon_sources[t_game].empty()) {
		*bitmap = decode_icon_bitmap(game_icon_sources[t_game], menu_icon_size());
	}

	return *bitmap;
}

auto Tray::Native::append_row(HMENU t_menu, UINT t_flags, UINT_PTR t_id, const MenuRow& t_row) -> void
{
	if (row_count >= K_MAX_MENU_ROWS) return;

	MenuRow* stored = &rows[row_count];
	*stored         = t_row;
	row_count += 1;

	AppendMenuW(t_menu, t_flags | MF_OWNERDRAW, t_id, reinterpret_cast<LPCWSTR>(stored));
}

auto Tray::Native::build_game_submenu(const TrayGame& t_game) -> HMENU
{
	const HMENU submenu = CreatePopupMenu();

	for (u32 i = 0; i < t_game.account_count; i += 1) {
		const u32 account = t_game.first_account + i;
		if (account >= menu.account_count) break;

		MenuRow row{};
		utf8_to_wide(menu.accounts[account].label, row.label, ARRAYSIZE(row.label));
		append_row(submenu, MF_STRING, K_FIRST_QUICK_LOGIN_COMMAND + account, row);
	}

	if (t_game.account_count == 0) {
		append_row(submenu, MF_DISABLED | MF_GRAYED, K_PLACEHOLDER_COMMAND, MenuRow{.label = L"No accounts", .disabled = true});
	}

	return submenu;
}

auto Tray::Native::build_menu() -> HMENU
{
	const HMENU popup = CreatePopupMenu();

	for (const TrayGame& game : std::span{menu.games, menu.game_count}) {
		MenuRow row{.icon = game_icon(game.game), .indented = true, .submenu = true};
		utf8_to_wide(game.title, row.label, ARRAYSIZE(row.label));

		append_row(popup, MF_POPUP, reinterpret_cast<UINT_PTR>(build_game_submenu(game)), row);
	}

	if (menu.locked) {
		append_row(popup, MF_DISABLED | MF_GRAYED, K_PLACEHOLDER_COMMAND, MenuRow{.label = L"Vault locked", .indented = true, .disabled = true});
	} else if (menu.game_count == 0) {
		append_row(popup, MF_DISABLED | MF_GRAYED, K_PLACEHOLDER_COMMAND, MenuRow{.label = L"No games", .indented = true, .disabled = true});
	}

	append_row(popup, MF_DISABLED | MF_GRAYED, 0, MenuRow{.separator = true, .disabled = true});
	append_row(popup, MF_STRING, K_SHOW_COMMAND, MenuRow{.label = L"Show application", .indented = true});
	append_row(popup, MF_STRING, K_EXIT_COMMAND, MenuRow{.label = L"Exit application", .indented = true});

	const MENUINFO menu_info{
		.cbSize  = sizeof(MENUINFO),
		.fMask   = MIM_BACKGROUND | MIM_APPLYTOSUBMENUS,
		.hbrBack = background_brush,
	};
	SetMenuInfo(popup, &menu_info);

	return popup;
}

auto Tray::Native::show_menu() -> void
{
	POINT cursor;
	GetCursorPos(&cursor);

	menu        = TrayMenu{};
	menu.locked = locked;
	if (fill_menu && !locked) {
		fill_menu(&menu);
	}

	row_count         = 0;
	const HMENU popup = build_menu();

	// Without foreground, the menu does not dismiss when the user clicks elsewhere.
	SetForegroundWindow(window);

	TPMPARAMS placement{.cbSize = sizeof(TPMPARAMS), .rcExclude = RECT{cursor.x, cursor.y, cursor.x, cursor.y}};
	TrackPopupMenuEx(popup, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON | TPM_WORKAREA, cursor.x, cursor.y, window, &placement);
	PostMessageW(window, WM_NULL, 0, 0);

	DestroyMenu(popup);
	row_count = 0;
}

auto Tray::Native::measure_row(MEASUREITEMSTRUCT* t_measure) const -> void
{
	const auto* row = reinterpret_cast<const MenuRow*>(t_measure->itemData);

	if (row->separator) {
		t_measure->itemWidth  = K_MIN_ROW_WIDTH;
		t_measure->itemHeight = K_SEPARATOR_HEIGHT;
		return;
	}

	SIZE text_size{};
	if (const HDC dc = GetDC(window)) {
		const HGDIOBJ previous_font = SelectObject(dc, menu_font);
		GetTextExtentPoint32W(dc, row->label, static_cast<int>(wcslen(row->label)), &text_size);
		SelectObject(dc, previous_font);
		ReleaseDC(window, dc);
	}

	const int indent = row->indented ? menu_icon_size() + K_ICON_GAP : 0;
	const int width  = K_PADDING_X * 2 + indent + text_size.cx + (row->submenu ? K_SUBMENU_ARROW_WIDTH : 0);

	t_measure->itemWidth  = static_cast<UINT>(std::max(width, K_MIN_ROW_WIDTH));
	t_measure->itemHeight = K_ROW_HEIGHT;
}

auto Tray::Native::draw_row(const DRAWITEMSTRUCT* t_draw) const -> void
{
	const auto* row      = reinterpret_cast<const MenuRow*>(t_draw->itemData);
	const HDC   dc       = t_draw->hDC;
	const RECT  rect     = t_draw->rcItem;
	const int   middle_y = (rect.top + rect.bottom) / 2;

	const bool hovered = (t_draw->itemState & ODS_SELECTED) != 0 && !row->separator && !row->disabled;
	FillRect(dc, &rect, hovered ? hover_brush : background_brush);

	if (row->separator) {
		const RECT   line{rect.left + K_PADDING_X, middle_y, rect.right - K_PADDING_X, middle_y + 1};
		const HBRUSH brush = CreateSolidBrush(to_colorref(colors.separator));
		FillRect(dc, &line, brush);
		DeleteObject(brush);
		return;
	}

	const int icon_size = menu_icon_size();

	if (row->icon != nullptr) {
		if (const HDC memory_dc = CreateCompatibleDC(dc)) {
			const HGDIOBJ       previous_bitmap = SelectObject(memory_dc, row->icon);
			const BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};

			AlphaBlend(dc, rect.left + K_PADDING_X, middle_y - icon_size / 2, icon_size, icon_size, memory_dc, 0, 0, icon_size, icon_size, blend);

			SelectObject(memory_dc, previous_bitmap);
			DeleteDC(memory_dc);
		}
	}

	const Color text_color = row->disabled ? colors.text_disabled : colors.text;
	SetBkMode(dc, TRANSPARENT);
	SetTextColor(dc, to_colorref(text_color));
	const HGDIOBJ previous_font = SelectObject(dc, menu_font);

	const int indent = row->indented ? icon_size + K_ICON_GAP : 0;
	RECT      text_rect{rect.left + K_PADDING_X + indent, rect.top, rect.right - K_PADDING_X, rect.bottom};
	DrawTextW(dc, row->label, -1, &text_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

	if (row->submenu) {
		const int   tip_x = rect.right - K_PADDING_X - 4;
		const POINT arrow[3]{{tip_x - 4, middle_y - 4}, {tip_x, middle_y}, {tip_x - 4, middle_y + 4}};

		const HBRUSH  brush          = CreateSolidBrush(to_colorref(text_color));
		const HGDIOBJ previous_brush = SelectObject(dc, brush);
		const HGDIOBJ previous_pen   = SelectObject(dc, GetStockObject(NULL_PEN));

		Polygon(dc, arrow, 3);

		SelectObject(dc, previous_pen);
		SelectObject(dc, previous_brush);
		DeleteObject(brush);
	}

	SelectObject(dc, previous_font);
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

auto Tray::Native::handle_command(UINT t_command) -> void
{
	if (t_command == K_SHOW_COMMAND) {
		pending_event = TrayEvent{.type = TrayEventType::SHOW_WINDOW};
		return;
	}

	if (t_command == K_EXIT_COMMAND) {
		pending_event = TrayEvent{.type = TrayEventType::EXIT};
		return;
	}

	const UINT account = t_command - K_FIRST_QUICK_LOGIN_COMMAND;
	if (t_command < K_FIRST_QUICK_LOGIN_COMMAND || account >= menu.account_count || locked) return;

	pending_event = TrayEvent{
		.type = TrayEventType::QUICK_LOGIN,
		.game = menu.accounts[account].game,
		.row  = menu.accounts[account].row,
	};
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

		case WM_MEASUREITEM: {
			auto* measure = reinterpret_cast<MEASUREITEMSTRUCT*>(t_lparam);
			if (measure->CtlType != ODT_MENU || measure->itemData == 0) break;

			measure_row(measure);
			return TRUE;
		}

		case WM_DRAWITEM: {
			const auto* draw = reinterpret_cast<const DRAWITEMSTRUCT*>(t_lparam);
			if (draw->CtlType != ODT_MENU || draw->itemData == 0) break;

			draw_row(draw);
			return TRUE;
		}

		case WM_COMMAND: {
			handle_command(LOWORD(t_wparam));
			return 0;
		}

		default: {
			break;
		}
	}

	return DefWindowProcW(window, t_message, t_wparam, t_lparam);
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

}
