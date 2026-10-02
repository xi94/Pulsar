#include "platform/tray.h"

#include <algorithm>
#include <cwchar>
#include <utility>
#include <vector>

#include <shellapi.h>

#include "core/app_identity.h"
#include "core/debug_log.h"
#include "platform/app_icon.h"
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
	return app_icon_pixel_size(AppIconSize::SmallIcon) * 3 / 2;
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

Tray::~Tray()
{
	remove_icon();

	if (m_window != nullptr) {
		KillTimer(m_window, K_ADD_ICON_RETRY_TIMER);
		DestroyWindow(m_window);
	}

	if (m_owns_menu_font) {
		DeleteObject(m_menu_font);
	}

	if (m_locked_icon != nullptr) {
		DestroyIcon(m_locked_icon);
	}

	DeleteObject(m_background_brush);
	DeleteObject(m_hover_brush);

	for (const HBITMAP icon : m_game_icons) {
		if (icon != nullptr) {
			DeleteObject(icon);
		}
	}
}

auto Tray::create(const wchar_t* t_tooltip) -> bool
{
	const HINSTANCE instance = GetModuleHandleW(nullptr);

	const WNDCLASSEXW window_class{
		.cbSize        = sizeof(WNDCLASSEXW),
		.lpfnWndProc   = window_proc,
		.hInstance     = instance,
		.lpszClassName = K_TRAY_WINDOW_CLASS_NAME,
	};

	if (RegisterClassExW(&window_class) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
		debug_log::write(K_LOG_CATEGORY, "RegisterClassExW failed, err=%lu", GetLastError());
		return false;
	}

	if (CreateWindowExW(0, K_TRAY_WINDOW_CLASS_NAME, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, instance, this) == nullptr) {
		debug_log::write(K_LOG_CATEGORY, "failed to create the tray window, err=%lu", GetLastError());
		m_window = nullptr;
		return false;
	}

	wcsncpy_s(m_tooltip, t_tooltip, _TRUNCATE);

	m_icon = load_app_icon(AppIconSize::SmallIcon);
	if (m_icon == nullptr) {
		m_icon = LoadIconW(nullptr, IDI_APPLICATION);
	}

	NONCLIENTMETRICSW metrics{.cbSize = sizeof(NONCLIENTMETRICSW)};
	if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0)) {
		m_menu_font      = CreateFontIndirectW(&metrics.lfMenuFont);
		m_owns_menu_font = m_menu_font != nullptr;
	}

	if (m_menu_font == nullptr) {
		m_menu_font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
	}

	rebuild_brushes();
	add_icon();

	return true;
}

auto Tray::on_menu_open(std::function<void(TrayMenu*)> t_fill_menu) -> void
{
	m_fill_menu = std::move(t_fill_menu);
}

auto Tray::set_locked(bool t_locked) -> void
{
	if (t_locked == m_locked) return;

	m_locked = t_locked;

	if (m_locked && m_locked_icon == nullptr) {
		m_locked_icon = greyscale_icon(m_icon);
	}

	update_icon();
}

auto Tray::shown_icon() const -> HICON
{
	return m_locked && m_locked_icon != nullptr ? m_locked_icon : m_icon;
}

auto Tray::fill_tooltip(wchar_t (&t_tooltip)[128]) const -> void
{
	wcsncpy_s(t_tooltip, m_tooltip, _TRUNCATE);

	if (m_locked) {
		wcsncat_s(t_tooltip, L" (locked)", _TRUNCATE);
	}
}

auto Tray::update_icon() -> void
{
	if (!m_icon_added) return;

	NOTIFYICONDATAW icon{
		.cbSize = sizeof(NOTIFYICONDATAW),
		.hWnd   = m_window,
		.uID    = K_TRAY_ICON_ID,
		.uFlags = NIF_ICON | NIF_TIP,
		.hIcon  = shown_icon(),
	};
	fill_tooltip(icon.szTip);

	Shell_NotifyIconW(NIM_MODIFY, &icon);
}

auto Tray::take_event() -> TrayEvent
{
	return std::exchange(m_pending_event, TrayEvent{});
}

auto Tray::rebuild_brushes() -> void
{
	DeleteObject(m_background_brush);
	DeleteObject(m_hover_brush);

	m_background_brush = CreateSolidBrush(to_colorref(m_colors.background));
	m_hover_brush      = CreateSolidBrush(to_colorref(m_colors.hover));
}

auto Tray::set_colors(const TrayColors& t_colors) -> void
{
	if (t_colors == m_colors) return;

	m_colors = t_colors;
	rebuild_brushes();
}

auto Tray::set_game_icon(u32 t_game, std::span<const u8> t_png) -> void
{
	if (t_game >= K_TRAY_MAX_GAMES) return;

	m_game_icon_sources[t_game] = t_png;

	if (m_game_icons[t_game] != nullptr) {
		DeleteObject(m_game_icons[t_game]);
		m_game_icons[t_game] = nullptr;
	}
}

auto Tray::game_icon(i32 t_game) -> HBITMAP
{
	if (t_game < 0 || static_cast<u32>(t_game) >= K_TRAY_MAX_GAMES) return nullptr;

	HBITMAP* icon = &m_game_icons[t_game];
	if (*icon == nullptr && !m_game_icon_sources[t_game].empty()) {
		*icon = decode_icon_bitmap(m_game_icon_sources[t_game], menu_icon_size());
	}

	return *icon;
}

auto Tray::append_row(HMENU t_menu, UINT t_flags, UINT_PTR t_id, const MenuRow& t_row) -> void
{
	if (m_row_count >= K_MAX_MENU_ROWS) return;

	MenuRow* stored = &m_rows[m_row_count];
	*stored         = t_row;
	m_row_count += 1;

	AppendMenuW(t_menu, t_flags | MF_OWNERDRAW, t_id, reinterpret_cast<LPCWSTR>(stored));
}

auto Tray::build_game_submenu(const TrayGame& t_game) -> HMENU
{
	const HMENU submenu = CreatePopupMenu();

	for (u32 i = 0; i < t_game.account_count; i += 1) {
		const u32 account = t_game.first_account + i;
		if (account >= m_menu.account_count) break;

		MenuRow row{};
		utf8_to_wide(m_menu.accounts[account].label, row.label, ARRAYSIZE(row.label));
		append_row(submenu, MF_STRING, K_FIRST_QUICK_LOGIN_COMMAND + account, row);
	}

	if (t_game.account_count == 0) {
		append_row(submenu, MF_DISABLED | MF_GRAYED, K_PLACEHOLDER_COMMAND, MenuRow{.label = L"No accounts", .disabled = true});
	}

	return submenu;
}

auto Tray::build_menu() -> HMENU
{
	const HMENU menu = CreatePopupMenu();

	for (const TrayGame& game : std::span{m_menu.games, m_menu.game_count}) {
		MenuRow row{.icon = game_icon(game.game), .indented = true, .submenu = true};
		utf8_to_wide(game.title, row.label, ARRAYSIZE(row.label));

		append_row(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(build_game_submenu(game)), row);
	}

	if (m_menu.locked) {
		append_row(menu, MF_DISABLED | MF_GRAYED, K_PLACEHOLDER_COMMAND, MenuRow{.label = L"Vault locked", .indented = true, .disabled = true});
	} else if (m_menu.game_count == 0) {
		append_row(menu, MF_DISABLED | MF_GRAYED, K_PLACEHOLDER_COMMAND, MenuRow{.label = L"No games", .indented = true, .disabled = true});
	}

	append_row(menu, MF_DISABLED | MF_GRAYED, 0, MenuRow{.separator = true, .disabled = true});
	append_row(menu, MF_STRING, K_SHOW_COMMAND, MenuRow{.label = L"Show application", .indented = true});
	append_row(menu, MF_STRING, K_EXIT_COMMAND, MenuRow{.label = L"Exit application", .indented = true});

	const MENUINFO menu_info{
		.cbSize  = sizeof(MENUINFO),
		.fMask   = MIM_BACKGROUND | MIM_APPLYTOSUBMENUS,
		.hbrBack = m_background_brush,
	};
	SetMenuInfo(menu, &menu_info);

	return menu;
}

auto Tray::show_menu() -> void
{
	POINT cursor;
	GetCursorPos(&cursor);

	m_menu        = TrayMenu{};
	m_menu.locked = m_locked;
	if (m_fill_menu && !m_locked) {
		m_fill_menu(&m_menu);
	}

	m_row_count      = 0;
	const HMENU menu = build_menu();

	// Without foreground, the menu does not dismiss when the user clicks elsewhere.
	SetForegroundWindow(m_window);

	TPMPARAMS placement{.cbSize = sizeof(TPMPARAMS), .rcExclude = RECT{cursor.x, cursor.y, cursor.x, cursor.y}};
	TrackPopupMenuEx(menu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON | TPM_WORKAREA, cursor.x, cursor.y, m_window, &placement);
	PostMessageW(m_window, WM_NULL, 0, 0);

	DestroyMenu(menu);
	m_row_count = 0;
}

auto Tray::measure_row(MEASUREITEMSTRUCT* t_measure) const -> void
{
	const auto* row = reinterpret_cast<const MenuRow*>(t_measure->itemData);

	if (row->separator) {
		t_measure->itemWidth  = K_MIN_ROW_WIDTH;
		t_measure->itemHeight = K_SEPARATOR_HEIGHT;
		return;
	}

	SIZE text_size{};
	if (const HDC dc = GetDC(m_window)) {
		const HGDIOBJ previous_font = SelectObject(dc, m_menu_font);
		GetTextExtentPoint32W(dc, row->label, static_cast<int>(wcslen(row->label)), &text_size);
		SelectObject(dc, previous_font);
		ReleaseDC(m_window, dc);
	}

	const int indent = row->indented ? menu_icon_size() + K_ICON_GAP : 0;
	const int width  = K_PADDING_X * 2 + indent + text_size.cx + (row->submenu ? K_SUBMENU_ARROW_WIDTH : 0);

	t_measure->itemWidth  = static_cast<UINT>(std::max(width, K_MIN_ROW_WIDTH));
	t_measure->itemHeight = K_ROW_HEIGHT;
}

auto Tray::draw_row(const DRAWITEMSTRUCT* t_draw) const -> void
{
	const auto* row      = reinterpret_cast<const MenuRow*>(t_draw->itemData);
	const HDC   dc       = t_draw->hDC;
	const RECT  rect     = t_draw->rcItem;
	const int   middle_y = (rect.top + rect.bottom) / 2;

	const bool hovered = (t_draw->itemState & ODS_SELECTED) != 0 && !row->separator && !row->disabled;
	FillRect(dc, &rect, hovered ? m_hover_brush : m_background_brush);

	if (row->separator) {
		const RECT   line{rect.left + K_PADDING_X, middle_y, rect.right - K_PADDING_X, middle_y + 1};
		const HBRUSH brush = CreateSolidBrush(to_colorref(m_colors.separator));
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

	const Color text_color = row->disabled ? m_colors.text_disabled : m_colors.text;
	SetBkMode(dc, TRANSPARENT);
	SetTextColor(dc, to_colorref(text_color));
	const HGDIOBJ previous_font = SelectObject(dc, m_menu_font);

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

auto Tray::add_icon() -> bool
{
	if (m_icon_added) return true;

	NOTIFYICONDATAW icon{
		.cbSize           = sizeof(NOTIFYICONDATAW),
		.hWnd             = m_window,
		.uID              = K_TRAY_ICON_ID,
		.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP,
		.uCallbackMessage = K_TRAY_CALLBACK_MESSAGE,
		.hIcon            = shown_icon(),
	};
	fill_tooltip(icon.szTip);

	m_add_attempts += 1;

	if (Shell_NotifyIconW(NIM_ADD, &icon)) {
		m_icon_added = true;
		KillTimer(m_window, K_ADD_ICON_RETRY_TIMER);
		debug_log::write(K_LOG_CATEGORY, "tray icon added on attempt %u", m_add_attempts);
		return true;
	}

	debug_log::write(K_LOG_CATEGORY, "Shell_NotifyIcon(NIM_ADD) failed on attempt %u, err=%lu", m_add_attempts, GetLastError());

	if (m_add_attempts < K_MAX_ADD_ICON_ATTEMPTS) {
		SetTimer(m_window, K_ADD_ICON_RETRY_TIMER, K_ADD_ICON_RETRY_INTERVAL_MS, nullptr);
	} else {
		KillTimer(m_window, K_ADD_ICON_RETRY_TIMER);
		debug_log::write(K_LOG_CATEGORY, "giving up on the tray icon after %u attempts", m_add_attempts);
	}

	return false;
}

auto Tray::remove_icon() -> void
{
	if (!m_icon_added) return;

	NOTIFYICONDATAW icon{.cbSize = sizeof(NOTIFYICONDATAW), .hWnd = m_window, .uID = K_TRAY_ICON_ID};
	Shell_NotifyIconW(NIM_DELETE, &icon);
	m_icon_added = false;
}

auto Tray::handle_command(UINT t_command) -> void
{
	if (t_command == K_SHOW_COMMAND) {
		m_pending_event = TrayEvent{.type = TrayEventType::ShowWindow};
		return;
	}

	if (t_command == K_EXIT_COMMAND) {
		m_pending_event = TrayEvent{.type = TrayEventType::Exit};
		return;
	}

	const UINT account = t_command - K_FIRST_QUICK_LOGIN_COMMAND;
	if (t_command < K_FIRST_QUICK_LOGIN_COMMAND || account >= m_menu.account_count || m_locked) return;

	m_pending_event = TrayEvent{
		.type = TrayEventType::QuickLogin,
		.game = m_menu.accounts[account].game,
		.row  = m_menu.accounts[account].row,
	};
}

auto Tray::handle_message(UINT t_message, WPARAM t_wparam, LPARAM t_lparam) -> LRESULT
{
	if (t_message == taskbar_created_message()) {
		debug_log::write(K_LOG_CATEGORY, "Explorer restarted - re-adding the tray icon");
		m_icon_added   = false;
		m_add_attempts = 0;
		add_icon();
		return 0;
	}

	switch (t_message) {
		case K_TRAY_CALLBACK_MESSAGE:
			if (LOWORD(t_lparam) == WM_LBUTTONUP) {
				m_pending_event = TrayEvent{.type = TrayEventType::ShowWindow};
			} else if (LOWORD(t_lparam) == WM_RBUTTONUP || LOWORD(t_lparam) == WM_CONTEXTMENU) {
				show_menu();
			}

			return 0;

		case WM_TIMER:
			if (t_wparam == K_ADD_ICON_RETRY_TIMER) {
				add_icon();
			}

			return 0;

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

		case WM_COMMAND:
			handle_command(LOWORD(t_wparam));
			return 0;

		default:
			break;
	}

	return DefWindowProcW(m_window, t_message, t_wparam, t_lparam);
}

auto CALLBACK Tray::window_proc(HWND t_window, UINT t_message, WPARAM t_wparam, LPARAM t_lparam) -> LRESULT
{
	if (t_message == WM_NCCREATE) {
		const auto* create = reinterpret_cast<const CREATESTRUCTW*>(t_lparam);
		SetWindowLongPtrW(t_window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
	}

	auto* tray = reinterpret_cast<Tray*>(GetWindowLongPtrW(t_window, GWLP_USERDATA));
	if (tray == nullptr) return DefWindowProcW(t_window, t_message, t_wparam, t_lparam);

	tray->m_window = t_window;

	return tray->handle_message(t_message, t_wparam, t_lparam);
}
