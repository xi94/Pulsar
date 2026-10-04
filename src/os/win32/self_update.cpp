#include "os/self_update.h"

#include <optional>

#include "core/version.h"
#include "os/process.h"
#include "os/win32/win32.h"

namespace {
[[nodiscard]] auto file_version(const std::wstring& t_executable) -> std::optional<Version>
{
	const DWORD info_size = GetFileVersionInfoSizeW(t_executable.c_str(), nullptr);
	if (info_size == 0) return std::nullopt;

	std::vector<u8> info(info_size);
	if (!GetFileVersionInfoW(t_executable.c_str(), 0, info_size, info.data())) return std::nullopt;

	VS_FIXEDFILEINFO* fixed      = nullptr;
	UINT              fixed_size = 0;
	if (!VerQueryValueW(info.data(), L"\\", reinterpret_cast<void**>(&fixed), &fixed_size) || fixed == nullptr || fixed_size < sizeof(VS_FIXEDFILEINFO)) {
		return std::nullopt;
	}

	return Version{HIWORD(fixed->dwFileVersionMS), LOWORD(fixed->dwFileVersionMS), HIWORD(fixed->dwFileVersionLS)};
}

[[nodiscard]] auto write_bytes(const std::wstring& t_path, const std::vector<u8>& t_bytes) -> bool
{
	const HANDLE file = CreateFileW(t_path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE) return false;

	DWORD      written = 0;
	const bool ok      = WriteFile(file, t_bytes.data(), static_cast<DWORD>(t_bytes.size()), &written, nullptr) && written == t_bytes.size();
	CloseHandle(file);

	return ok;
}

auto delete_stale_backup(const std::wstring& t_backup_path) -> void
{
	constexpr int ATTEMPTS = 20;

	for (int attempt = 0; attempt < ATTEMPTS; attempt += 1) {
		if (DeleteFileW(t_backup_path.c_str()) || GetLastError() == ERROR_FILE_NOT_FOUND) return;

		Sleep(50);
	}
}
}

namespace os {

auto can_self_update() -> bool
{
	return true;
}

auto install_update(const std::vector<u8>& t_new_build, std::string_view t_running_version, std::string* t_out_error) -> bool
{
	const std::wstring self_path = win32::to_wide(executable_path());
	if (self_path.empty()) {
		*t_out_error = "could not resolve this program's path";
		return false;
	}

	const std::wstring update_path = self_path + L".update";
	const std::wstring backup_path = self_path + L".old";

	if (!write_bytes(update_path, t_new_build)) {
		DeleteFileW(update_path.c_str());
		*t_out_error = "could not write the downloaded update to disk (disk full?)";
		return false;
	}

	const std::optional<Version> downloaded = file_version(update_path);
	if (!downloaded || *downloaded <= parse_version(t_running_version).value_or(Version{})) {
		DeleteFileW(update_path.c_str());
		*t_out_error = "the downloaded build is not newer than this one - refusing to downgrade";
		return false;
	}

	if (MoveFileExW(update_path.c_str(), self_path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
		DeleteFileW(backup_path.c_str());
		return true;
	}

	// A crash between these two renames leaves only the .old copy, which handed_off_to_repaired_copy restores.
	if (!MoveFileExW(self_path.c_str(), backup_path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
		DeleteFileW(update_path.c_str());
		*t_out_error = "could not replace the running executable (it may be locked by another program)";
		return false;
	}

	if (!MoveFileExW(update_path.c_str(), self_path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
		MoveFileExW(backup_path.c_str(), self_path.c_str(), MOVEFILE_REPLACE_EXISTING);
		*t_out_error = "could not move the new build into place";
		return false;
	}

	return true;
}

auto handed_off_to_repaired_copy() -> bool
{
	const std::wstring self_path = win32::to_wide(executable_path());
	if (self_path.empty()) return false;

	constexpr std::wstring_view BACKUP_SUFFIX = L".old";
	if (!self_path.ends_with(BACKUP_SUFFIX)) {
		delete_stale_backup(self_path + std::wstring{BACKUP_SUFFIX});
		return false;
	}

	const std::wstring canonical_path = self_path.substr(0, self_path.size() - BACKUP_SUFFIX.size());
	if (GetFileAttributesW(canonical_path.c_str()) == INVALID_FILE_ATTRIBUTES) {
		CopyFileW(self_path.c_str(), canonical_path.c_str(), FALSE);
	}

	launch_process(win32::to_utf8(canonical_path));

	return true;
}

}
