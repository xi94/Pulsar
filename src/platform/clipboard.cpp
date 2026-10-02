#include "platform/clipboard.h"

#include <cstring>

#include <Windows.h>

#include "core/str.h"

namespace {
auto put_global(UINT t_format, const void* t_bytes, usize t_size) -> bool
{
	const HGLOBAL memory      = GlobalAlloc(GMEM_MOVEABLE, t_size);
	void*         destination = memory != nullptr ? GlobalLock(memory) : nullptr;

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

auto put_text(std::string_view t_text, bool t_keep_out_of_history) -> void
{
	std::wstring wide = to_wide(t_text);

	if (OpenClipboard(GetActiveWindow())) {
		EmptyClipboard();
		put_global(CF_UNICODETEXT, wide.c_str(), (wide.size() + 1) * sizeof(wchar_t));

		if (t_keep_out_of_history) {
			static const UINT EXCLUDE_FROM_MONITORS  = RegisterClipboardFormatW(L"ExcludeClipboardContentFromMonitorProcessing");
			static const UINT CAN_INCLUDE_IN_HISTORY = RegisterClipboardFormatW(L"CanIncludeInClipboardHistory");
			static const UINT CAN_UPLOAD_TO_CLOUD    = RegisterClipboardFormatW(L"CanUploadToCloudClipboard");
			const DWORD       disallowed             = 0;

			put_global(EXCLUDE_FROM_MONITORS, &disallowed, sizeof(disallowed));
			put_global(CAN_INCLUDE_IN_HISTORY, &disallowed, sizeof(disallowed));
			put_global(CAN_UPLOAD_TO_CLOUD, &disallowed, sizeof(disallowed));
		}

		CloseClipboard();
	}

	SecureZeroMemory(wide.data(), wide.size() * sizeof(wchar_t));
}
}

auto set_clipboard_text(std::string_view t_text) -> void
{
	put_text(t_text, false);
}

auto set_clipboard_secret(std::string_view t_secret) -> void
{
	put_text(t_secret, true);
}

[[nodiscard]] auto clipboard_text() -> std::string
{
	if (!OpenClipboard(GetActiveWindow())) return {};

	std::string text;

	const HANDLE data = GetClipboardData(CF_UNICODETEXT);
	if (const auto* wide = data != nullptr ? static_cast<const wchar_t*>(GlobalLock(data)) : nullptr) {
		text = to_utf8(wide);
		GlobalUnlock(data);
	}

	CloseClipboard();

	return text;
}

[[nodiscard]] auto clipboard_has_text() -> bool
{
	return IsClipboardFormatAvailable(CF_UNICODETEXT);
}

[[nodiscard]] auto clipboard_sequence() -> u32
{
	return GetClipboardSequenceNumber();
}

auto clear_clipboard_if_unchanged(u32 t_sequence) -> void
{
	if (GetClipboardSequenceNumber() != t_sequence || !OpenClipboard(nullptr)) return;

	EmptyClipboard();
	CloseClipboard();
}
