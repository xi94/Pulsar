#pragma once

#include <Windows.h>

// Loading the app icon at the size Windows is actually going to draw it.
//
// The .ico carries 16, 32, 64, 128 and 256, so there is a native image for every slot on every
// scale factor - but only if the pixel size asked for matches one. Ask for the wrong size and
// LoadImage resamples a neighbour, which is what makes an icon look soft.

/// Which slot the icon is for. The two differ by more than a factor of two, so one handle cannot
/// serve both without visibly resampling in one of them.
enum class EAppIconSize : u8 {
	/// The notification area, and the window's own small icon - title bar and Alt+Tab's small slot.
	Small,

	/// The taskbar button, Alt+Tab, and the window class's large icon.
	Large,
};

/// Sized for this machine's scale factor, not for 96 DPI. The caller owns the returned handle and
/// destroys it with DestroyIcon; null if the resource is missing, which callers fall back from.
HICON LoadAppIcon(EAppIconSize size);

/// The pixel size LoadAppIcon would use, for a caller that has to lay out around the icon rather
/// than just draw it - the tray's owner-drawn menu measures its rows against this.
int AppIconPixelSize(EAppIconSize size);
