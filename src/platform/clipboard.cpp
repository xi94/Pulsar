#include "platform/clipboard.h"

#include <cstring>

#include <Windows.h>

#include "core/str.h"

void set_clipboard_text(std::string_view t_text)
{
	const std::wstring wide = to_wide(t_text);
	const usize bytes = (wide.size() + 1) * sizeof(wchar_t);

	if (!OpenClipboard(GetActiveWindow())) return;

	EmptyClipboard();

	const HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
	void *destination = memory != nullptr ? GlobalLock(memory) : nullptr;

	if (destination != nullptr) {
		std::memcpy(destination, wide.c_str(), bytes);
		GlobalUnlock(memory);

		if (SetClipboardData(CF_UNICODETEXT, memory) == nullptr) {
			GlobalFree(memory);
		}
	} else if (memory != nullptr) {
		GlobalFree(memory);
	}

	CloseClipboard();
}

std::string clipboard_text()
{
	if (!OpenClipboard(GetActiveWindow())) return {};

	std::string text;

	const HANDLE data = GetClipboardData(CF_UNICODETEXT);
	if (const auto *wide = data != nullptr ? static_cast<const wchar_t *>(GlobalLock(data)) : nullptr) {
		text = to_utf8(wide);
		GlobalUnlock(data);
	}

	CloseClipboard();

	return text;
}

bool clipboard_has_text()
{
	return IsClipboardFormatAvailable(CF_UNICODETEXT);
}
