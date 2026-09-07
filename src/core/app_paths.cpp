#include "core/app_paths.h"

#include "core/app_identity.h"

#include <Windows.h>
#include <shlobj.h>

namespace {
std::wstring LocalAppDataRoot()
{
	wchar_t fromEnvironment[MAX_PATH];
	const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", fromEnvironment, MAX_PATH);
	if (length > 0 && length < MAX_PATH) return std::wstring{fromEnvironment, length};

	PWSTR pKnownFolder = nullptr;
	std::wstring root;

	if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &pKnownFolder))) {
		root = pKnownFolder;
	}

	if (pKnownFolder != nullptr) {
		CoTaskMemFree(pKnownFolder);
	}

	return root;
}
} // namespace

std::wstring AppDataSubdirectory(const wchar_t *pSubfolder)
{
	const std::wstring root = LocalAppDataRoot();
	if (root.empty()) return std::wstring{};

	std::wstring dir = root + L"\\" + kAppNameW + L"\\" + pSubfolder;
	SHCreateDirectoryExW(nullptr, dir.c_str(), nullptr);

	return dir;
}
