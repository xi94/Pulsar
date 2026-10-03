#include "os/fonts.h"

#include "os/win32/win32.h"

namespace {
constexpr const wchar_t* K_REGISTERED_FONTS_KEY = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts";

// Tried in order when the chosen font lacks a character. Japanese comes before Korean because Malgun Gothic also carries kana and kanji.
constexpr const char* K_FALLBACK_FONT_FILES[]{
	"segoeui.ttf", "LeelawUI.ttf", "YuGothM.ttc", "meiryo.ttc", "malgun.ttf", "msjh.ttc", "msyh.ttc", "seguisym.ttf", "seguiemj.ttf",
};

[[nodiscard]] auto fonts_folder() -> std::string
{
	wchar_t    windows_directory[MAX_PATH];
	const UINT length = GetWindowsDirectoryW(windows_directory, MAX_PATH);
	if (length == 0 || length >= MAX_PATH) return {};

	return os::win32::to_utf8(std::wstring_view{windows_directory, length}) + "\\Fonts\\";
}

[[nodiscard]] auto is_absolute_path(std::string_view t_file) -> bool
{
	return t_file.find(':') != std::string_view::npos || t_file.starts_with("\\\\");
}

[[nodiscard]] auto display_name(std::string_view t_registered_name) -> std::string_view
{
	const usize suffix = t_registered_name.rfind(" (");
	if (suffix != std::string_view::npos && t_registered_name.ends_with(')')) {
		t_registered_name = t_registered_name.substr(0, suffix);
	}

	return t_registered_name.substr(0, t_registered_name.find(" & "));
}

auto add_registered_fonts(HKEY t_root, std::vector<os::SystemFont>* t_fonts) -> void
{
	HKEY key = nullptr;
	if (RegOpenKeyExW(t_root, K_REGISTERED_FONTS_KEY, 0, KEY_READ, &key) != ERROR_SUCCESS) return;

	DWORD longest_name       = 0;
	DWORD largest_file_bytes = 0;
	RegQueryInfoKeyW(key, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &longest_name, &largest_file_bytes, nullptr, nullptr);

	std::vector<wchar_t> name(longest_name + 1);
	std::vector<wchar_t> file(largest_file_bytes / sizeof(wchar_t) + 1);

	for (DWORD index = 0;; index += 1) {
		DWORD name_length = static_cast<DWORD>(name.size());
		DWORD file_bytes  = static_cast<DWORD>(file.size() * sizeof(wchar_t));
		DWORD type        = 0;

		const LSTATUS status = RegEnumValueW(key, index, name.data(), &name_length, nullptr, &type, reinterpret_cast<BYTE*>(file.data()), &file_bytes);
		if (status == ERROR_NO_MORE_ITEMS) break;
		if (status != ERROR_SUCCESS || type != REG_SZ) continue;

		std::wstring_view file_view{file.data(), file_bytes / sizeof(wchar_t)};
		while (!file_view.empty() && file_view.back() == L'\0') {
			file_view.remove_suffix(1);
		}

		const std::string name_utf8 = os::win32::to_utf8(std::wstring_view{name.data(), name_length});
		t_fonts->push_back(os::SystemFont{std::string{display_name(name_utf8)}, os::win32::to_utf8(file_view)});
	}

	RegCloseKey(key);
}
}

namespace os {

auto system_fonts() -> std::vector<SystemFont>
{
	std::vector<SystemFont> fonts;
	fonts.reserve(512);

	add_registered_fonts(HKEY_LOCAL_MACHINE, &fonts);
	add_registered_fonts(HKEY_CURRENT_USER, &fonts);

	return fonts;
}

auto system_font_path(std::string_view t_file) -> std::string
{
	if (is_absolute_path(t_file)) return std::string{t_file};

	const std::string folder = fonts_folder();

	return folder.empty() ? std::string{} : folder + std::string{t_file};
}

auto fallback_font_paths() -> std::vector<std::string>
{
	const std::string folder = fonts_folder();
	if (folder.empty()) return {};

	std::vector<std::string> paths;
	for (const char* file : K_FALLBACK_FONT_FILES) {
		paths.push_back(folder + file);
	}

	return paths;
}

}
