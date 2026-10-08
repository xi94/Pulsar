#include "core/app_identity.h"
#include "os/win32/win32.h"

namespace os::win32 {

auto app_icon_pixel_size(AppIconSize t_size) -> int
{
	const bool is_small = t_size == AppIconSize::SMALL_ICON;

	// The unscaled GetSystemMetrics reports 16 regardless of display scaling in a per-monitor aware process.
	const int pixels = GetSystemMetricsForDpi(is_small ? SM_CXSMICON : SM_CXICON, GetDpiForSystem());

	return pixels > 0 ? pixels : (is_small ? 16 : 32);
}

auto load_app_icon(AppIconSize t_size) -> HICON
{
	const int pixels = app_icon_pixel_size(t_size);

	return static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(PULSAR_APP_ICON_RESOURCE), IMAGE_ICON, pixels, pixels, 0));
}

}
