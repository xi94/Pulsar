#include "core/file.h"

#include <cstdio>
#include <cstring>

#include <Windows.h>
#include <shlobj.h>

#include "core/app_identity.h"

namespace {
bool file_already_holds(const std::string &t_path, std::string_view t_contents)
{
	std::vector<u8> existing;
	if (!read_whole_file(t_path.c_str(), existing)) return false;

	return existing.size() == t_contents.size() &&
		   std::memcmp(existing.data(), t_contents.data(), existing.size()) == 0;
}

bool write_and_flush(const std::string &t_path, std::string_view t_contents)
{
	const HANDLE file =
		CreateFileA(t_path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE) return false;

	DWORD written = 0;
	const bool ok = WriteFile(file, t_contents.data(), static_cast<DWORD>(t_contents.size()), &written, nullptr) &&
					written == t_contents.size() && FlushFileBuffers(file);
	CloseHandle(file);

	return ok;
}
}

bool read_whole_file(const char *t_path, std::vector<u8> &t_out_bytes)
{
	FILE *file = nullptr;
	if (fopen_s(&file, t_path, "rb") != 0 || file == nullptr) return false;

	std::fseek(file, 0, SEEK_END);
	const long size = std::ftell(file);
	std::fseek(file, 0, SEEK_SET);

	bool ok = false;
	if (size > 0) {
		t_out_bytes.resize(static_cast<usize>(size));
		ok = std::fread(t_out_bytes.data(), 1, t_out_bytes.size(), file) == t_out_bytes.size();
	}

	std::fclose(file);

	return ok;
}

bool write_file_atomic(const std::string &t_path, std::string_view t_contents)
{
	// Saves happen on nearly every click, so rewriting unchanged content would rotate the last good .bak away.
	if (file_already_holds(t_path, t_contents)) return true;

	const std::string temporary_path = t_path + ".tmp";
	if (!write_and_flush(temporary_path, t_contents)) {
		DeleteFileA(temporary_path.c_str());
		return false;
	}

	MoveFileExA(t_path.c_str(), backup_path_for(t_path).c_str(), MOVEFILE_REPLACE_EXISTING);

	if (!MoveFileExA(temporary_path.c_str(), t_path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
		DeleteFileA(temporary_path.c_str());
		return false;
	}

	return true;
}

std::string backup_path_for(const std::string &t_path)
{
	return t_path + ".bak";
}

std::wstring local_app_data_folder()
{
	wchar_t from_environment[MAX_PATH];
	const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", from_environment, MAX_PATH);
	if (length > 0 && length < MAX_PATH) return std::wstring{from_environment, length};

	PWSTR known_folder = nullptr;
	std::wstring root;

	if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &known_folder))) {
		root = known_folder;
	}

	CoTaskMemFree(known_folder);

	return root;
}

std::wstring app_data_subdirectory(const wchar_t *t_subfolder)
{
	const std::wstring root = local_app_data_folder();
	if (root.empty()) return {};

	const std::wstring directory = root + L"\\" + app_name_wide + L"\\" + t_subfolder;
	SHCreateDirectoryExW(nullptr, directory.c_str(), nullptr);

	return directory;
}
