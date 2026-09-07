#pragma once

/// Places UTF-8 text on the system clipboard as CF_UNICODETEXT. `owner` is the window that
/// takes clipboard ownership for the duration.
void SetClipboardText(void *hOwnerWindow, const char *pText);
