#include "platform/app_icon.h"

#include <utility>

#include "core/app_identity.h"

[[nodiscard]] auto app_icon_pixel_size(AppIconSize t_size) -> int
{
	const bool is_small = t_size == AppIconSize::SmallIcon;

	// The unscaled GetSystemMetrics reports 16 regardless of display scaling in a per-monitor aware process.
	const int pixels = GetSystemMetricsForDpi(is_small ? SM_CXSMICON : SM_CXICON, GetDpiForSystem());

	return pixels > 0 ? pixels : (is_small ? 16 : 32);
}

[[nodiscard]] auto load_app_icon(AppIconSize t_size) -> HICON
{
	const int pixels = app_icon_pixel_size(t_size);

	return static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(PULSAR_APP_ICON_RESOURCE), IMAGE_ICON, pixels, pixels, 0));
}

[[nodiscard]] auto app_icon_pixels(u32 t_size) -> std::vector<u8>
{
	const auto size = static_cast<int>(t_size);
	const auto icon = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(PULSAR_APP_ICON_RESOURCE), IMAGE_ICON, size, size, 0));
	if (icon == nullptr) return {};

	ICONINFO        info{};
	std::vector<u8> pixels(static_cast<usize>(t_size) * t_size * 4);
	bool            read = false;

	if (GetIconInfo(icon, &info) && info.hbmColor != nullptr) {
		BITMAPINFO format{};
		format.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
		format.bmiHeader.biWidth       = size;
		format.bmiHeader.biHeight      = -size;
		format.bmiHeader.biPlanes      = 1;
		format.bmiHeader.biBitCount    = 32;
		format.bmiHeader.biCompression = BI_RGB;

		const HDC screen = GetDC(nullptr);
		read             = GetDIBits(screen, info.hbmColor, 0, t_size, pixels.data(), &format, DIB_RGB_COLORS) != 0;
		ReleaseDC(nullptr, screen);
	}

	if (info.hbmColor != nullptr) DeleteObject(info.hbmColor);
	if (info.hbmMask != nullptr) DeleteObject(info.hbmMask);
	DestroyIcon(icon);

	if (!read) return {};

	for (usize i = 0; i < pixels.size(); i += 4) {
		std::swap(pixels[i], pixels[i + 2]);
	}

	return pixels;
}
