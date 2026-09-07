#include "platform/tray.h"

#include <shellapi.h>
#include <wchar.h>

#include "core/app_identity.h"
#include "core/debug_log.h"
#include "platform/app_icon.h"
#include "platform/resource.h"
#include "stb/stb_image.h"

namespace {
constexpr const char *kLogCategory = "tray";

constexpr UINT kTrayCallbackMessage = WM_APP + 1;
constexpr UINT_PTR kTrayIconId = 1;
constexpr UINT_PTR kRetryTimerId = 1;
constexpr UINT kRetryIntervalMs = 1000;
constexpr u32 kMaxAddAttempts = 10;

constexpr UINT kMenuIdShow = 1001;
constexpr UINT kMenuIdExit = 1002;
constexpr UINT kMenuIdPlaceholder = 1003;
constexpr UINT kMenuIdQuickLoginBase = 2000;

constexpr int kItemHeight = 26;
constexpr int kSeparatorHeight = 7;
constexpr int kPaddingX = 12;
constexpr int kArrowWidth = 18;
constexpr int kIconGap = 8;
constexpr int kMinItemWidth = 170;

constexpr Color kColorBg{30, 30, 34, 255};
constexpr Color kColorText{220, 220, 224, 255};
constexpr Color kColorTextDim{140, 140, 148, 255};
constexpr Color kColorSeparator{60, 60, 66, 255};

// Deliberately larger than the shell would draw a small icon: this menu is owner-drawn, so its row
// height is ours to choose, and a game icon is the only thing in a row that identifies it at a
// glance. Scaled off the app-icon size so it tracks display scaling with everything else.
int MenuIconSize()
{
	constexpr int kMenuIconScaleNumerator = 3;
	constexpr int kMenuIconScaleDenominator = 2;

	return AppIconPixelSize(EAppIconSize::Small) * kMenuIconScaleNumerator / kMenuIconScaleDenominator;
}

// Explorer broadcasts this after a restart; the icon has to be re-added or it is gone for the
// rest of the session.
UINT TaskbarCreatedMessage()
{
	static const UINT message = RegisterWindowMessageW(L"TaskbarCreated");

	return message;
}

COLORREF ToColorRef(Color color)
{
	return RGB(color.R, color.G, color.B);
}

void ToWide(const char *pUtf8, wchar_t *pOut, int outCapacity)
{
	if (MultiByteToWideChar(CP_UTF8, 0, pUtf8, -1, pOut, outCapacity) <= 0) {
		pOut[0] = L'\0';
	}
}

HFONT CreateMenuFont()
{
	NONCLIENTMETRICSW metrics{};
	metrics.cbSize = sizeof(metrics);

	if (!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0)) return nullptr;

	return CreateFontIndirectW(&metrics.lfMenuFont);
}

struct BoxSample {
	u32 R;
	u32 G;
	u32 B;
	u32 A;
};

// Averages the source texels covering one destination pixel.
BoxSample AverageBox(const unsigned char *pPixels, int width, int height, int x0, int x1, int y0, int y1)
{
	BoxSample sum{};
	u32 samples = 0;

	for (int y = y0; y < y1 && y < height; y += 1) {
		for (int x = x0; x < x1 && x < width; x += 1) {
			const unsigned char *pTexel = pPixels + (static_cast<usize>(y) * width + x) * 4;
			sum.R += pTexel[0];
			sum.G += pTexel[1];
			sum.B += pTexel[2];
			sum.A += pTexel[3];
			samples += 1;
		}
	}

	if (samples == 0) return BoxSample{};

	return BoxSample{sum.R / samples, sum.G / samples, sum.B / samples, sum.A / samples};
}

// Box-filtered rather than letting AlphaBlend stretch: these icons are a few hundred pixels
// square, and a nearest-neighbour drop to 16px looks obviously broken next to the rest of the
// app. The result is premultiplied BGRA so AlphaBlend can draw it straight onto the menu.
HBITMAP DecodePngToPremultipliedDib(const u8 *pBytes, u64 length, int targetSize)
{
	int width = 0;
	int height = 0;
	int channels = 0;
	unsigned char *pPixels = stbi_load_from_memory(pBytes, static_cast<int>(length), &width, &height, &channels, 4);
	if (pPixels == nullptr || width <= 0 || height <= 0) return nullptr;

	BITMAPINFO info{};
	info.bmiHeader.biSize = sizeof(info.bmiHeader);
	info.bmiHeader.biWidth = targetSize;
	info.bmiHeader.biHeight = -targetSize; // top-down
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 32;
	info.bmiHeader.biCompression = BI_RGB;

	void *pDibBits = nullptr;
	const HBITMAP hBitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &pDibBits, nullptr, 0);
	if (hBitmap == nullptr || pDibBits == nullptr) {
		stbi_image_free(pPixels);
		return nullptr;
	}

	auto *pOut = static_cast<u8 *>(pDibBits);

	for (int y = 0; y < targetSize; y += 1) {
		const int srcY0 = y * height / targetSize;
		const int srcY1 = std::max((y + 1) * height / targetSize, srcY0 + 1);

		for (int x = 0; x < targetSize; x += 1) {
			const int srcX0 = x * width / targetSize;
			const int srcX1 = std::max((x + 1) * width / targetSize, srcX0 + 1);

			const BoxSample sample = AverageBox(pPixels, width, height, srcX0, srcX1, srcY0, srcY1);

			u8 *pDest = pOut + (static_cast<usize>(y) * targetSize + x) * 4;
			pDest[0] = static_cast<u8>(sample.B * sample.A / 255);
			pDest[1] = static_cast<u8>(sample.G * sample.A / 255);
			pDest[2] = static_cast<u8>(sample.R * sample.A / 255);
			pDest[3] = static_cast<u8>(sample.A);
		}
	}

	stbi_image_free(pPixels);

	return hBitmap;
}

TrayMenuEntry MakeEntry(const wchar_t *pLabel, HBITMAP hIcon, bool bIndent, bool bSubmenu, bool bSeparator,
						bool bDisabled)
{
	TrayMenuEntry entry{};
	entry.hIcon = hIcon;
	entry.bIndent = bIndent;
	entry.bSubmenu = bSubmenu;
	entry.bSeparator = bSeparator;
	entry.bDisabled = bDisabled;

	if (pLabel != nullptr) {
		wcsncpy_s(entry.szLabel, pLabel, _TRUNCATE);
	}

	return entry;
}
} // namespace

CTray::~CTray()
{
	RemoveIcon();

	if (m_hWnd != nullptr) {
		KillTimer(m_hWnd, kRetryTimerId);
		DestroyWindow(m_hWnd);
	}

	if (m_hMenuFont != nullptr) {
		DeleteObject(m_hMenuFont);
	}

	if (m_hBackBrush != nullptr) {
		DeleteObject(m_hBackBrush);
	}

	if (m_hHoverBrush != nullptr) {
		DeleteObject(m_hHoverBrush);
	}

	for (HBITMAP hIcon : m_gameIcons) {
		if (hIcon != nullptr) {
			DeleteObject(hIcon);
		}
	}
}

bool CTray::Create(const wchar_t *pTooltip)
{
	const HINSTANCE instance = GetModuleHandleW(nullptr);

	WNDCLASSEXW windowClass{};
	windowClass.cbSize = sizeof(WNDCLASSEXW);
	windowClass.lpfnWndProc = WindowProc;
	windowClass.hInstance = instance;
	windowClass.lpszClassName = kTrayWindowClassName;

	if (RegisterClassExW(&windowClass) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
		DebugLog::Write(kLogCategory, "RegisterClassExW failed, err=%lu", GetLastError());
		return false;
	}

	if (CreateWindowExW(0, kTrayWindowClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, instance, this) ==
		nullptr) {
		DebugLog::Write(kLogCategory, "failed to create the tray window, err=%lu", GetLastError());
		m_hWnd = nullptr;
		return false;
	}

	wcsncpy_s(m_szTooltip, pTooltip, _TRUNCATE);

	m_hIcon = LoadAppIcon(EAppIconSize::Small);
	if (m_hIcon == nullptr) {
		m_hIcon = LoadIconW(nullptr, IDI_APPLICATION);
	}

	m_hMenuFont = CreateMenuFont();
	if (m_hMenuFont == nullptr) {
		m_hMenuFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
	}

	RebuildBrushes();
	AddIcon();

	return true;
}

void CTray::SetMenuCallback(TrayMenuCallback callback, void *pUserData)
{
	m_pMenuCallback = callback;
	m_pMenuCallbackUserData = pUserData;
}

ETrayEventType CTray::TakeEvent()
{
	const ETrayEventType event = m_pendingEvent;
	m_pendingEvent = ETrayEventType::None;

	return event;
}

void CTray::RebuildBrushes()
{
	if (m_hBackBrush != nullptr) {
		DeleteObject(m_hBackBrush);
	}

	if (m_hHoverBrush != nullptr) {
		DeleteObject(m_hHoverBrush);
	}

	m_hBackBrush = CreateSolidBrush(ToColorRef(kColorBg));
	m_hHoverBrush = CreateSolidBrush(ToColorRef(ColorLerp(kColorBg, m_accent, 0.42f)));
}

void CTray::SetAccentColor(Color accent)
{
	if (accent.R == m_accent.R && accent.G == m_accent.G && accent.B == m_accent.B) return;

	m_accent = accent;
	RebuildBrushes();
}

// Only the bytes are kept here. Decoding all of them at startup cost more than the tray menu is
// worth, given most launches never open it, so the actual decode happens on first use.
void CTray::SetGameIcon(i32 bannerIndex, const u8 *pPngBytes, u64 length)
{
	if (bannerIndex < 0 || static_cast<u32>(bannerIndex) >= kTrayMaxGames || pPngBytes == nullptr) return;

	m_gameIconSources[bannerIndex] = EmbeddedImageBytes{pPngBytes, length};

	if (m_gameIcons[bannerIndex] != nullptr) {
		DeleteObject(m_gameIcons[bannerIndex]);
		m_gameIcons[bannerIndex] = nullptr;
	}
}

// Decoded once, the first time a menu that shows it is actually built.
HBITMAP CTray::GameIcon(i32 bannerIndex)
{
	if (bannerIndex < 0 || static_cast<u32>(bannerIndex) >= kTrayMaxGames) return nullptr;

	if (m_gameIcons[bannerIndex] == nullptr && m_gameIconSources[bannerIndex].pBytes != nullptr) {
		m_gameIcons[bannerIndex] = DecodePngToPremultipliedDib(m_gameIconSources[bannerIndex].pBytes,
															   m_gameIconSources[bannerIndex].Length, MenuIconSize());
	}

	return m_gameIcons[bannerIndex];
}

TrayMenuEntry *CTray::PushEntry(const TrayMenuEntry &entry)
{
	if (m_entryCount >= kTrayMaxMenuEntries) return nullptr;

	m_entries[m_entryCount] = entry;
	m_entryCount += 1;

	return &m_entries[m_entryCount - 1];
}

void CTray::AppendRow(HMENU target, UINT flags, UINT_PTR id, const TrayMenuEntry &entry)
{
	const TrayMenuEntry *pStored = PushEntry(entry);
	if (pStored == nullptr) return;

	AppendMenuW(target, flags | MF_OWNERDRAW, id, reinterpret_cast<LPCWSTR>(pStored));
}

void CTray::AppendCommand(HMENU target, UINT_PTR id, const wchar_t *pLabel, bool bIndent)
{
	AppendRow(target, MF_STRING, id, MakeEntry(pLabel, nullptr, bIndent, false, false, false));
}

void CTray::AppendSubmenu(HMENU target, HMENU submenu, const wchar_t *pLabel, HBITMAP hIcon)
{
	AppendRow(target, MF_POPUP, reinterpret_cast<UINT_PTR>(submenu),
			  MakeEntry(pLabel, hIcon, true, true, false, false));
}

void CTray::AppendPlaceholder(HMENU target, const wchar_t *pLabel, bool bIndent)
{
	AppendRow(target, MF_DISABLED | MF_GRAYED, kMenuIdPlaceholder,
			  MakeEntry(pLabel, nullptr, bIndent, false, false, true));
}

void CTray::AppendSeparator(HMENU target)
{
	AppendRow(target, MF_DISABLED | MF_GRAYED, 0, MakeEntry(L"", nullptr, false, false, true, true));
}

HMENU CTray::BuildGameSubmenu(const TrayGameItem &game)
{
	const HMENU submenu = CreatePopupMenu();

	for (u32 i = 0; i < game.AccountCount; i += 1) {
		const u32 accountIndex = game.FirstAccount + i;
		if (accountIndex >= m_model.AccountCount) break;

		wchar_t label[96];
		ToWide(m_model.Accounts[accountIndex].Label, label, ARRAYSIZE(label));
		AppendCommand(submenu, kMenuIdQuickLoginBase + accountIndex, label, false);
	}

	if (game.AccountCount == 0) {
		AppendPlaceholder(submenu, L"No accounts", false);
	}

	return submenu;
}

HMENU CTray::BuildMenu()
{
	const HMENU menu = CreatePopupMenu();

	for (u32 i = 0; i < m_model.GameCount; i += 1) {
		const TrayGameItem &game = m_model.Games[i];

		wchar_t title[96];
		ToWide(game.Title, title, ARRAYSIZE(title));

		AppendSubmenu(menu, BuildGameSubmenu(game), title, GameIcon(game.BannerIndex));
	}

	if (m_model.GameCount == 0) {
		AppendPlaceholder(menu, L"No games", true);
	}

	AppendSeparator(menu);
	AppendCommand(menu, kMenuIdShow, L"Show Application", true);
	AppendCommand(menu, kMenuIdExit, L"Exit Application", true);

	// MIM_APPLYTOSUBMENUS only reaches submenus that already exist, so this runs last.
	MENUINFO menuInfo{};
	menuInfo.cbSize = sizeof(menuInfo);
	menuInfo.fMask = MIM_BACKGROUND | MIM_APPLYTOSUBMENUS;
	menuInfo.hbrBack = m_hBackBrush;
	SetMenuInfo(menu, &menuInfo);

	return menu;
}

void CTray::ShowContextMenu()
{
	POINT cursor;
	GetCursorPos(&cursor);

	m_model = TrayMenuModel{};
	if (m_pMenuCallback != nullptr) {
		m_pMenuCallback(m_pMenuCallbackUserData, m_model);
	}

	m_entryCount = 0;
	const HMENU menu = BuildMenu();

	// Required so the menu dismisses when the user clicks away from it.
	SetForegroundWindow(m_hWnd);

	// TPM_WORKAREA keeps the menu off the taskbar: by default Windows only flips a menu up when
	// it would not fit on the monitor, so one that fits on screen but not above the taskbar gets
	// drawn underneath it. It is only honoured by TrackPopupMenuEx with a TPMPARAMS, and
	// rcExclude is degenerate at the cursor - an all-zero rect names the screen's top-left.
	TPMPARAMS popupParams{};
	popupParams.cbSize = sizeof(popupParams);
	popupParams.rcExclude = RECT{cursor.x, cursor.y, cursor.x, cursor.y};

	TrackPopupMenuEx(menu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON | TPM_WORKAREA, cursor.x, cursor.y, m_hWnd,
					 &popupParams);
	PostMessageW(m_hWnd, WM_NULL, 0, 0);

	DestroyMenu(menu);
	m_entryCount = 0;
}

void CTray::OnMeasureItem(MEASUREITEMSTRUCT *pMeasure) const
{
	const auto *pEntry = reinterpret_cast<const TrayMenuEntry *>(pMeasure->itemData);
	if (pEntry == nullptr) return;

	if (pEntry->bSeparator) {
		pMeasure->itemWidth = kMinItemWidth;
		pMeasure->itemHeight = kSeparatorHeight;
		return;
	}

	int textWidth = 0;
	const HDC hdc = GetDC(m_hWnd);
	if (hdc != nullptr) {
		const HGDIOBJ previousFont = SelectObject(hdc, m_hMenuFont);

		SIZE size{};
		if (GetTextExtentPoint32W(hdc, pEntry->szLabel, static_cast<int>(wcslen(pEntry->szLabel)), &size)) {
			textWidth = size.cx;
		}

		SelectObject(hdc, previousFont);
		ReleaseDC(m_hWnd, hdc);
	}

	const int indent = pEntry->bIndent ? MenuIconSize() + kIconGap : 0;
	const int width = kPaddingX * 2 + indent + textWidth + (pEntry->bSubmenu ? kArrowWidth : 0);

	pMeasure->itemWidth = static_cast<UINT>(width > kMinItemWidth ? width : kMinItemWidth);
	pMeasure->itemHeight = kItemHeight;
}

void CTray::OnDrawItem(const DRAWITEMSTRUCT *pDraw) const
{
	const auto *pEntry = reinterpret_cast<const TrayMenuEntry *>(pDraw->itemData);
	if (pEntry == nullptr) return;

	RECT rect = pDraw->rcItem;
	const bool bSelected = (pDraw->itemState & ODS_SELECTED) != 0 && !pEntry->bSeparator && !pEntry->bDisabled;
	FillRect(pDraw->hDC, &rect, bSelected ? m_hHoverBrush : m_hBackBrush);

	if (pEntry->bSeparator) {
		const int middle = (rect.top + rect.bottom) / 2;
		RECT line{rect.left + kPaddingX, middle, rect.right - kPaddingX, middle + 1};

		const HBRUSH hBrush = CreateSolidBrush(ToColorRef(kColorSeparator));
		FillRect(pDraw->hDC, &line, hBrush);
		DeleteObject(hBrush);

		return;
	}

	const int iconSize = MenuIconSize();

	if (pEntry->hIcon != nullptr) {
		const HDC hdcMem = CreateCompatibleDC(pDraw->hDC);
		if (hdcMem != nullptr) {
			const HGDIOBJ previousBitmap = SelectObject(hdcMem, pEntry->hIcon);
			const BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};

			AlphaBlend(pDraw->hDC, rect.left + kPaddingX, (rect.top + rect.bottom - iconSize) / 2, iconSize, iconSize,
					   hdcMem, 0, 0, iconSize, iconSize, blend);

			SelectObject(hdcMem, previousBitmap);
			DeleteDC(hdcMem);
		}
	}

	SetBkMode(pDraw->hDC, TRANSPARENT);
	SetTextColor(pDraw->hDC, ToColorRef(pEntry->bDisabled ? kColorTextDim : kColorText));
	const HGDIOBJ previousFont = SelectObject(pDraw->hDC, m_hMenuFont);

	const int indent = pEntry->bIndent ? iconSize + kIconGap : 0;
	RECT textRect{rect.left + kPaddingX + indent, rect.top, rect.right - kPaddingX, rect.bottom};
	DrawTextW(pDraw->hDC, pEntry->szLabel, -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

	if (pEntry->bSubmenu) {
		const int cx = rect.right - kPaddingX - 4;
		const int cy = (rect.top + rect.bottom) / 2;
		const POINT arrow[3]{{cx - 4, cy - 4}, {cx, cy}, {cx - 4, cy + 4}};

		const HBRUSH hBrush = CreateSolidBrush(ToColorRef(pEntry->bDisabled ? kColorTextDim : kColorText));
		const HGDIOBJ previousBrush = SelectObject(pDraw->hDC, hBrush);
		const HGDIOBJ previousPen = SelectObject(pDraw->hDC, GetStockObject(NULL_PEN));

		Polygon(pDraw->hDC, arrow, 3);

		SelectObject(pDraw->hDC, previousPen);
		SelectObject(pDraw->hDC, previousBrush);
		DeleteObject(hBrush);
	}

	SelectObject(pDraw->hDC, previousFont);
}

bool CTray::AddIcon()
{
	if (m_bIconAdded) return true;

	NOTIFYICONDATAW iconData{};
	iconData.cbSize = sizeof(NOTIFYICONDATAW);
	iconData.hWnd = m_hWnd;
	iconData.uID = kTrayIconId;
	iconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	iconData.uCallbackMessage = kTrayCallbackMessage;
	iconData.hIcon = m_hIcon;
	wcsncpy_s(iconData.szTip, m_szTooltip, _TRUNCATE);

	m_addAttempts += 1;

	if (Shell_NotifyIconW(NIM_ADD, &iconData) == TRUE) {
		m_bIconAdded = true;
		KillTimer(m_hWnd, kRetryTimerId);
		DebugLog::Write(kLogCategory, "tray icon added on attempt %u", m_addAttempts);

		return true;
	}

	// This legitimately fails while the shell is still starting up, so it is a retry rather
	// than an error until the attempts run out.
	DebugLog::Write(kLogCategory, "Shell_NotifyIcon(NIM_ADD) failed on attempt %u, err=%lu", m_addAttempts,
					GetLastError());

	if (m_addAttempts < kMaxAddAttempts) {
		SetTimer(m_hWnd, kRetryTimerId, kRetryIntervalMs, nullptr);
	} else {
		KillTimer(m_hWnd, kRetryTimerId);
		DebugLog::Write(kLogCategory, "giving up on the tray icon after %u attempts", m_addAttempts);
	}

	return false;
}

void CTray::RemoveIcon()
{
	if (!m_bIconAdded) return;

	NOTIFYICONDATAW iconData{};
	iconData.cbSize = sizeof(NOTIFYICONDATAW);
	iconData.hWnd = m_hWnd;
	iconData.uID = kTrayIconId;

	Shell_NotifyIconW(NIM_DELETE, &iconData);
	m_bIconAdded = false;
}

void CTray::HandleCommand(UINT commandId)
{
	if (commandId == kMenuIdShow) {
		m_pendingEvent = ETrayEventType::ShowWindow;
		return;
	}

	if (commandId == kMenuIdExit) {
		m_pendingEvent = ETrayEventType::ExitRequested;
		return;
	}

	if (commandId < kMenuIdQuickLoginBase || commandId >= kMenuIdQuickLoginBase + kTrayMaxAccountItems) return;

	const UINT index = commandId - kMenuIdQuickLoginBase;
	if (index >= m_model.AccountCount) return;

	m_pendingEvent = ETrayEventType::QuickLogin;
	m_nPendingBannerIndex = m_model.Accounts[index].BannerIndex;
	m_nPendingAccountIndex = m_model.Accounts[index].QueryIndex;
}

LRESULT CTray::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == TaskbarCreatedMessage()) {
		DebugLog::Write(kLogCategory, "Explorer restarted - re-adding the tray icon");
		m_bIconAdded = false;
		m_addAttempts = 0;
		AddIcon();

		return 0;
	}

	switch (message) {
		case kTrayCallbackMessage: {
			const auto mouseMessage = static_cast<UINT>(LOWORD(lParam));

			if (mouseMessage == WM_LBUTTONUP) {
				m_pendingEvent = ETrayEventType::ShowWindow;
			} else if (mouseMessage == WM_RBUTTONUP || mouseMessage == WM_CONTEXTMENU) {
				ShowContextMenu();
			}

			return 0;
		}

		case WM_TIMER:
			if (wParam == kRetryTimerId) {
				AddIcon();
			}

			return 0;

		case WM_MEASUREITEM: {
			auto *pMeasure = reinterpret_cast<MEASUREITEMSTRUCT *>(lParam);
			if (pMeasure == nullptr || pMeasure->CtlType != ODT_MENU) break;

			OnMeasureItem(pMeasure);

			return TRUE;
		}

		case WM_DRAWITEM: {
			const auto *pDraw = reinterpret_cast<const DRAWITEMSTRUCT *>(lParam);
			if (pDraw == nullptr || pDraw->CtlType != ODT_MENU) break;

			OnDrawItem(pDraw);

			return TRUE;
		}

		case WM_COMMAND:
			HandleCommand(LOWORD(wParam));
			return 0;

		default:
			break;
	}

	return DefWindowProcW(m_hWnd, message, wParam, lParam);
}

LRESULT CALLBACK CTray::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WM_NCCREATE) {
		const auto *pCreate = reinterpret_cast<const CREATESTRUCTW *>(lParam);
		SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreate->lpCreateParams));
	}

	auto *pTray = reinterpret_cast<CTray *>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
	if (pTray == nullptr) return DefWindowProcW(hWnd, message, wParam, lParam);

	// Assigned before dispatching, not after CreateWindowExW returns: HandleMessage passes this
	// to DefWindowProcW, and WM_NCCREATE arrives while creation is still in flight - a null
	// handle there makes DefWindowProcW return 0, which aborts the whole creation.
	pTray->m_hWnd = hWnd;

	return pTray->HandleMessage(message, wParam, lParam);
}
