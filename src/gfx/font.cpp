#include "gfx/font.h"

#include <algorithm>
#include <cmath>
#include <print>
#include <vector>

#include <Windows.h>

#include "core/file.h"
#include "gfx/renderer.h"

namespace {
constexpr float coverage_gamma = 0.8f;
constexpr float setting_to_pixel_scale = 1.5f;
constexpr float default_body_size = 16.0f;
constexpr float default_secondary_size = 12.0f;
constexpr const char *default_font_file = "segoeui.ttf";

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

std::string system_font_path(std::string_view t_file_name)
{
	char windows_directory[MAX_PATH];
	const UINT length = GetWindowsDirectoryA(windows_directory, MAX_PATH);
	if (length == 0 || length >= MAX_PATH) return {};

	return std::string{windows_directory, length} + "\\Fonts\\" + std::string{t_file_name};
}
}

Font::Font() = default;
Font::~Font() = default;
Font::Font(Font &&) noexcept = default;
Font &Font::operator=(Font &&) noexcept = default;

bool Font::load(Renderer &t_renderer, const char *t_path, float t_pixel_height, float t_dpi_scale)
{
	std::vector<u8> font_file;
	if (!read_whole_file(t_path, font_file)) {
		std::println("Failed to read font file: {}", t_path);
		return false;
	}

	const float baked_pixel_height = t_pixel_height * t_dpi_scale;
	const u32 atlas_size = atlas_size_for(baked_pixel_height);
	std::vector<u8> coverage(static_cast<usize>(atlas_size) * atlas_size);

	stbtt_pack_context pack;
	stbtt_PackBegin(&pack, coverage.data(), static_cast<int>(atlas_size), static_cast<int>(atlas_size), 0, 1, nullptr);
	stbtt_PackSetOversampling(&pack, 1, 1);
	const bool packed = stbtt_PackFontRange(&pack, font_file.data(), 0, baked_pixel_height, first_char, char_count,
											m_packed_chars) != 0;
	stbtt_PackEnd(&pack);

	if (!packed) {
		std::println("Failed to pack glyph atlas for font: {}", t_path);
		return false;
	}

	const std::vector<u8> rgba = coverage_to_white_rgba(coverage);
	m_atlas = std::make_unique<Texture>(t_renderer, rgba.data(), atlas_size, atlas_size);

	stbtt_fontinfo info;
	stbtt_InitFont(&info, font_file.data(), 0);

	int ascent = 0;
	int descent = 0;
	int line_gap = 0;
	stbtt_GetFontVMetrics(&info, &ascent, &descent, &line_gap);

	const float logical_scale = stbtt_ScaleForPixelHeight(&info, baked_pixel_height) / t_dpi_scale;

	m_atlas_size = atlas_size;
	m_bake_scale = t_dpi_scale;
	m_pixel_height = t_pixel_height;
	m_ascent = ascent * logical_scale;
	m_descent = descent * logical_scale;
	m_line_gap = line_gap * logical_scale;

	return m_atlas->is_valid();
}

bool Fonts::load_defaults(Renderer &t_renderer, float t_dpi_scale)
{
	return load(t_renderer, default_font_file, default_body_size, default_secondary_size, t_dpi_scale);
}

bool Fonts::load(Renderer &t_renderer, std::string_view t_file_name, float t_body_size, float t_secondary_size,
				 float t_dpi_scale)
{
	const std::string path = system_font_path(t_file_name);
	if (t_file_name.empty() || path.empty()) return false;

	Font body;
	Font secondary;
	if (!body.load(t_renderer, path.c_str(), t_body_size * setting_to_pixel_scale, t_dpi_scale) ||
		!secondary.load(t_renderer, path.c_str(), t_secondary_size * setting_to_pixel_scale, t_dpi_scale)) {
		return false;
	}

	m_body = std::move(body);
	m_secondary = std::move(secondary);

	return true;
}
