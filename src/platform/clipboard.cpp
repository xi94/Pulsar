#include "platform/clipboard.h"

#include <cstring>

#include <Windows.h>

#include "core/str.h"

namespace {
bool put_global(UINT t_format, const void *t_bytes, usize t_size)
{
	const HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, t_size);
	void *destination = memory != nullptr ? GlobalLock(memory) : nullptr;

	if (destination == nullptr) {
		if (memory != nullptr) GlobalFree(memory);
		return false;
	}

	std::memcpy(destination, t_bytes, t_size);
	GlobalUnlock(memory);

	if (SetClipboardData(t_format, memory) == nullptr) {
		GlobalFree(memory);
		return false;
	}

	return true;
}

void put_text(std::string_view t_text, bool t_keep_out_of_history)
{
	std::wstring wide = to_wide(t_text);

	if (OpenClipboard(GetActiveWindow())) {
		EmptyClipboard();
		put_global(CF_UNICODETEXT, wide.c_str(), (wide.size() + 1) * sizeof(wchar_t));

		if (t_keep_out_of_history) {
			static const UINT exclude_from_monitors = RegisterClipboardFormatW(L"ExcludeClipboardContentFromMonitorProcessing");
			static const UINT can_include_in_history = RegisterClipboardFormatW(L"CanIncludeInClipboardHistory");
			static const UINT can_upload_to_cloud = RegisterClipboardFormatW(L"CanUploadToCloudClipboard");
			const DWORD disallowed = 0;

			put_global(exclude_from_monitors, &disallowed, sizeof(disallowed));
			put_global(can_include_in_history, &disallowed, sizeof(disallowed));
			put_global(can_upload_to_cloud, &disallowed, sizeof(disallowed));
		}

		CloseClipboard();
	}

	SecureZeroMemory(wide.data(), wide.size() * sizeof(wchar_t));
}
}

void set_clipboard_text(std::string_view t_text)
{
	put_text(t_text, false);
}

void set_clipboard_secret(std::string_view t_secret)
{
	put_text(t_secret, true);
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

u32 clipboard_sequence()
{
	return GetClipboardSequenceNumber();
}

void clear_clipboard_if_unchanged(u32 t_sequence)
{
	if (GetClipboardSequenceNumber() != t_sequence || !OpenClipboard(nullptr)) return;

	EmptyClipboard();
	CloseClipboard();
}
