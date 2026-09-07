#include "platform/app_icon.h"

#include "platform/resource.h"

namespace {
// System rather than per-monitor DPI: both slots this serves are shell furniture on the primary
// monitor's taskbar, not part of this app's own window, so they scale with the system and not with
// whichever display the window happens to be sitting on.
UINT ShellDpi()
{
	return GetDpiForSystem();
}
} // namespace

int AppIconPixelSize(EAppIconSize size)
{
	// GetSystemMetricsForDpi, not GetSystemMetrics: this process is Per-Monitor-V2, and the
	// unscaled call reports 16 for a small icon however the display is scaled. The shell then
	// stretches that 16 into the 20, 24 or 32 pixel slot it actually draws, which is what made the
	// tray icon look both small and soft.
	const int metric = size == EAppIconSize::Small ? SM_CXSMICON : SM_CXICON;
	const int pixels = GetSystemMetricsForDpi(metric, ShellDpi());

	// A zero would ask LoadImage for the resource's own first image, which is the 16 - the exact
	// thing this exists to avoid.
	return pixels > 0 ? pixels : (size == EAppIconSize::Small ? 16 : 32);
}

// LoadImageW rather than LoadIconW, which only ever returns the large size. Given an explicit
// pixel size it picks the closest image in the .ico, so every scale factor gets a native one.
HICON LoadAppIcon(EAppIconSize size)
{
	const int pixels = AppIconPixelSize(size);

	return static_cast<HICON>(
		LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, pixels, pixels, 0));
}
