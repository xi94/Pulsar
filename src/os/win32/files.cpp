#include "os/files.h"

#include <Windows.h>
#include <share.h>

#include <ShlObj.h>

#include "os/win32/win32.h"

namespace os {

auto user_data_folder() -> std::string
{
	wchar_t     from_environment[MAX_PATH];
	const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", from_environment, MAX_PATH);
	if (length > 0 && length < MAX_PATH) return win32::to_utf8(std::wstring_view{from_environment, length});

	PWSTR       known_folder = nullptr;
	std::string root;

	if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &known_folder))) {
		root = win32::to_utf8(known_folder);
	}

	CoTaskMemFree(known_folder);

	return root;
}

auto write_file_durably(const std::string& t_path, std::string_view t_contents) -> bool
{
	const std::wstring path = win32::to_wide(t_path);
	const HANDLE       file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE) return false;

	DWORD      written = 0;
	const bool ok =
		WriteFile(file, t_contents.data(), static_cast<DWORD>(t_contents.size()), &written, nullptr) && written == t_contents.size() && FlushFileBuffers(file);
	CloseHandle(file);

	return ok;
}

auto replace_file(const std::string& t_from, const std::string& t_to) -> bool
{
	return MoveFileExW(win32::to_wide(t_from).c_str(), win32::to_wide(t_to).c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
}

// Shared for reading so the log of a run that is hung right now can still be opened.
auto open_log_file(const std::string& t_path) -> std::FILE*
{
	return _wfsopen(win32::to_wide(t_path).c_str(), L"wb", _SH_DENYWR);
}

auto map_file(const std::string& t_path) -> std::span<const u8>
{
	const std::wstring path = win32::to_wide(t_path);
	const HANDLE       file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE) return {};

	LARGE_INTEGER size{};
	const bool    sized = GetFileSizeEx(file, &size) && size.QuadPart > 0;

	const HANDLE mapping = sized ? CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr) : nullptr;
	CloseHandle(file);
	if (mapping == nullptr) return {};

	const void* view = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
	CloseHandle(mapping);
	if (view == nullptr) return {};

	return std::span{static_cast<const u8*>(view), static_cast<usize>(size.QuadPart)};
}

auto unmap_file(std::span<const u8> t_mapping) -> void
{
	if (!t_mapping.empty()) {
		UnmapViewOfFile(t_mapping.data());
	}
}

}
