#include "platform/clipboard.h"

#include <cstring>

#include <Windows.h>

void SetClipboardText(void *hOwnerWindow, const char *pText)
{
	if (!OpenClipboard(static_cast<HWND>(hOwnerWindow))) return;

	EmptyClipboard();

	const auto textLength = static_cast<int>(std::strlen(pText));
	const int wideLength = MultiByteToWideChar(CP_UTF8, 0, pText, textLength, nullptr, 0);
	const HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, (static_cast<SIZE_T>(wideLength) + 1) * sizeof(wchar_t));

	if (memory != nullptr) {
		auto *pDestination = static_cast<wchar_t *>(GlobalLock(memory));
		if (pDestination != nullptr) {
			if (wideLength > 0) {
				MultiByteToWideChar(CP_UTF8, 0, pText, textLength, pDestination, wideLength);
			}

			pDestination[wideLength] = L'\0';
			GlobalUnlock(memory);
			SetClipboardData(CF_UNICODETEXT, memory);
		}
	}

	CloseClipboard();
}
