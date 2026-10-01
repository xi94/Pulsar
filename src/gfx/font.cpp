#include "gfx/font.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <print>

#include <Windows.h>

#include "core/file.h"
#include "core/str.h"
#include "gfx/renderer.h"

namespace {
constexpr float coverage_gamma = 0.8f;
constexpr float setting_to_pixel_scale = 1.5f;
constexpr float caption_size_ratio = 0.85f;
constexpr const wchar_t *registered_fonts_key = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts";

struct FontEntry {
	std::string name;
	std::string file;
};

u32 atlas_size_for(float t_baked_pixel_height)
{
	if (t_baked_pixel_height <= 24.0f) return 512;
	if (t_baked_pixel_height <= 48.0f) return 1024;

	return 2048;
}

std::vector<u8> coverage_to_white_rgba(const std::vector<u8> &t_coverage)
{
	std::vector<u8> rgba(t_coverage.size() * 4);

	for (usize i = 0; i < t_coverage.size(); i += 1) {
		const float boosted = std::pow(t_coverage[i] / 255.0f, coverage_gamma) * 255.0f;

		rgba[i * 4 + 0] = 255;
		rgba[i * 4 + 1] = 255;
		rgba[i * 4 + 2] = 255;
		rgba[i * 4 + 3] = static_cast<u8>(std::min(255.0f, boosted));
	}

	return rgba;
}

bool equals_ignoring_case(std::string_view t_a, std::string_view t_b)
{
	return t_a.size() == t_b.size() && _strnicmp(t_a.data(), t_b.data(), t_a.size()) == 0;
}

bool is_absolute_path(std::string_view t_file)
{
	return t_file.find(':') != std::string_view::npos || t_file.starts_with("\\\\");
}

std::string font_file_path(std::string_view t_file)
{
	if (is_absolute_path(t_file)) return std::string{t_file};

	char windows_directory[MAX_PATH];
	const UINT length = GetWindowsDirectoryA(windows_directory, MAX_PATH);
	if (length == 0 || length >= MAX_PATH) return {};

	return std::string{windows_directory, length} + "\\Fonts\\" + std::string{t_file};
}

bool is_outline_font(std::string_view t_file)
{
	const usize dot = t_file.rfind('.');
	if (dot == std::string_view::npos) return false;

	const std::string_view extension = t_file.substr(dot);

	return equals_ignoring_case(extension, ".ttf") || equals_ignoring_case(extension, ".ttc") || equals_ignoring_case(extension, ".otf");
}

std::string_view display_name(std::string_view t_registered_name)
{
	const usize suffix = t_registered_name.rfind(" (");
	if (suffix != std::string_view::npos && t_registered_name.ends_with(')')) {
		t_registered_name = t_registered_name.substr(0, suffix);
	}

	return t_registered_name.substr(0, t_registered_name.find(" & "));
}

void add_registered_fonts(HKEY t_root, std::vector<FontEntry> *t_entries)
{
	HKEY key = nullptr;
	if (RegOpenKeyExW(t_root, registered_fonts_key, 0, KEY_READ, &key) != ERROR_SUCCESS) return;

	DWORD longest_name = 0;
	DWORD largest_file_bytes = 0;
	RegQueryInfoKeyW(key, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &longest_name, &largest_file_bytes, nullptr, nullptr);

	std::vector<wchar_t> name(longest_name + 1);
	std::vector<wchar_t> file(largest_file_bytes / sizeof(wchar_t) + 1);

	for (DWORD index = 0;; index += 1) {
		DWORD name_length = static_cast<DWORD>(name.size());
		DWORD file_bytes = static_cast<DWORD>(file.size() * sizeof(wchar_t));
		DWORD type = 0;

		const LSTATUS status = RegEnumValueW(key, index, name.data(), &name_length, nullptr, &type, reinterpret_cast<BYTE *>(file.data()), &file_bytes);
		if (status == ERROR_NO_MORE_ITEMS) break;
		if (status != ERROR_SUCCESS || type != REG_SZ) continue;

		std::wstring_view file_view{file.data(), file_bytes / sizeof(wchar_t)};
		while (!file_view.empty() && file_view.back() == L'\0') {
			file_view.remove_suffix(1);
		}

		std::string file_utf8 = to_utf8(file_view);
		if (!is_outline_font(file_utf8)) continue;

		const std::string name_utf8 = to_utf8(std::wstring_view{name.data(), name_length});
		t_entries->push_back(FontEntry{std::string{display_name(name_utf8)}, std::move(file_utf8)});
	}

	RegCloseKey(key);
}
}

Font::Font() = default;
Font::~Font() = default;
Font::Font(Font &&) noexcept = default;
Font &Font::operator=(Font &&) noexcept = default;

bool Font::load(Renderer *t_renderer, const char *t_path, float t_pixel_height, float t_dpi_scale)
{
	std::vector<u8> font_file;
	if (!read_whole_file(t_path, &font_file)) {
		std::println("Failed to read font file: {}", t_path);
		return false;
	}

	const int font_offset = stbtt_GetFontOffsetForIndex(font_file.data(), 0);
	stbtt_fontinfo info;
	if (font_offset < 0 || !stbtt_InitFont(&info, font_file.data(), font_offset)) {
		std::println("Unsupported font file: {}", t_path);
		return false;
	}

	const float baked_pixel_height = t_pixel_height * t_dpi_scale;
	const u32 size = atlas_size_for(baked_pixel_height);
	std::vector<u8> coverage(static_cast<usize>(size) * size);

	stbtt_pack_context pack;
	stbtt_PackBegin(&pack, coverage.data(), static_cast<int>(size), static_cast<int>(size), 0, 1, nullptr);
	stbtt_PackSetOversampling(&pack, 1, 1);
	const bool packed = stbtt_PackFontRange(&pack, font_file.data(), 0, baked_pixel_height, first_char, char_count, packed_chars) != 0;
	stbtt_PackEnd(&pack);

	if (!packed) {
		std::println("Failed to pack glyph atlas for font: {}", t_path);
		return false;
	}

	const std::vector<u8> rgba = coverage_to_white_rgba(coverage);
	const TextureLevel atlas_level{rgba.data(), size, size};
	atlas = std::make_unique<Texture>(t_renderer, std::span{&atlas_level, 1});

	int ascent_units = 0;
	int descent_units = 0;
	int line_gap_units = 0;
	stbtt_GetFontVMetrics(&info, &ascent_units, &descent_units, &line_gap_units);

	const float logical_scale = stbtt_ScaleForPixelHeight(&info, baked_pixel_height) / t_dpi_scale;

	atlas_size = size;
	bake_scale = t_dpi_scale;
	pixel_height = t_pixel_height;
	ascent = ascent_units * logical_scale;
	descent = descent_units * logical_scale;
	line_gap = line_gap_units * logical_scale;

	return atlas->is_valid();
}

std::optional<u32> InstalledFonts::index_of_file(std::string_view t_file) const
{
	for (u32 i = 0; i < files.size(); i += 1) {
		if (equals_ignoring_case(files[i], t_file)) return i;
	}

	return std::nullopt;
}

InstalledFonts installed_fonts()
{
	std::vector<FontEntry> entries;
	entries.reserve(512);

	add_registered_fonts(HKEY_LOCAL_MACHINE, &entries);
	add_registered_fonts(HKEY_CURRENT_USER, &entries);

	std::ranges::stable_sort(entries, [](const FontEntry &t_a, const FontEntry &t_b) { return _stricmp(t_a.name.c_str(), t_b.name.c_str()) < 0; });

	const auto duplicates = std::ranges::unique(entries, [](const FontEntry &t_a, const FontEntry &t_b) { return equals_ignoring_case(t_a.name, t_b.name); });
	entries.erase(duplicates.begin(), duplicates.end());

	InstalledFonts fonts;
	fonts.names.reserve(entries.size());
	fonts.files.reserve(entries.size());

	for (FontEntry &entry : entries) {
		fonts.names.push_back(std::move(entry.name));
		fonts.files.push_back(std::move(entry.file));
	}

	return fonts;
}

bool Fonts::load(Renderer *t_renderer, std::string_view t_file, float t_body_size, float t_secondary_size, float t_dpi_scale)
{
	if (t_file.empty()) return false;

	const std::string path = font_file_path(t_file);
	if (path.empty()) return false;

	Font loaded_body;
	Font loaded_secondary;
	Font loaded_caption;
	if (!loaded_body.load(t_renderer, path.c_str(), t_body_size * setting_to_pixel_scale, t_dpi_scale) ||
		!loaded_secondary.load(t_renderer, path.c_str(), t_secondary_size * setting_to_pixel_scale, t_dpi_scale) ||
		!loaded_caption.load(t_renderer, path.c_str(), t_secondary_size * caption_size_ratio * setting_to_pixel_scale, t_dpi_scale)) {
		return false;
	}

	body = std::move(loaded_body);
	secondary = std::move(loaded_secondary);
	caption = std::move(loaded_caption);

	return true;
}
