#include "gfx/font.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <print>
#include <span>
#include <unordered_map>

#include "core/str.h"
#include "os/files.h"
#include "os/fonts.h"
#include "render/renderer.h"
#include "stb/stb_truetype.h"

namespace {
constexpr float K_COVERAGE_GAMMA         = 0.8f;
constexpr float K_SETTING_TO_PIXEL_SCALE = 1.5f;
constexpr float K_CAPTION_SIZE_RATIO     = 0.85f;
constexpr u32   K_ATLAS_PADDING          = 1;

struct FontFace {
	std::string    path;
	const u8*      data = nullptr;
	stbtt_fontinfo info{};
	float          em_scale = 0.0f;
};

std::vector<std::unique_ptr<FontFace>> g_faces;
std::vector<const FontFace*>           g_fallbacks;
bool                                   g_fallbacks_opened = false;

[[nodiscard]] auto atlas_size_for(float t_baked_pixel_height) -> u32
{
	return t_baked_pixel_height <= 32.0f ? 1024 : 2048;
}

[[nodiscard]] auto ascii_lowered(char t_character) -> char
{
	return t_character >= 'A' && t_character <= 'Z' ? static_cast<char>(t_character - 'A' + 'a') : t_character;
}

[[nodiscard]] auto equals_ignoring_case(std::string_view t_a, std::string_view t_b) -> bool
{
	return std::ranges::equal(t_a, t_b, [](char t_left, char t_right) { return ascii_lowered(t_left) == ascii_lowered(t_right); });
}

[[nodiscard]] auto sorts_before_ignoring_case(std::string_view t_a, std::string_view t_b) -> bool
{
	return std::ranges::lexicographical_compare(
		t_a, t_b, [](char t_left, char t_right) { return static_cast<u8>(ascii_lowered(t_left)) < static_cast<u8>(ascii_lowered(t_right)); });
}

// Font files are mapped rather than read, so the large CJK fallbacks only cost the pages a glyph actually touches.
[[nodiscard]] auto open_face(const std::string& t_path) -> const FontFace*
{
	for (const std::unique_ptr<FontFace>& face : g_faces) {
		if (equals_ignoring_case(face->path, t_path)) return face.get();
	}

	const std::span<const u8> mapping = os::map_file(t_path);
	if (mapping.empty()) return nullptr;

	auto face  = std::make_unique<FontFace>();
	face->path = t_path;
	face->data = mapping.data();

	const int offset = stbtt_GetFontOffsetForIndex(face->data, 0);
	if (offset < 0 || !stbtt_InitFont(&face->info, face->data, offset)) {
		os::unmap_file(mapping);
		return nullptr;
	}

	face->em_scale = stbtt_ScaleForMappingEmToPixels(&face->info, 1.0f);
	g_faces.push_back(std::move(face));

	return g_faces.back().get();
}

[[nodiscard]] auto fallback_faces() -> std::span<const FontFace* const>
{
	if (!g_fallbacks_opened) {
		g_fallbacks_opened = true;

		for (const std::string& path : os::fallback_font_paths()) {
			g_fallbacks.push_back(open_face(path));
		}
	}

	return g_fallbacks;
}

[[nodiscard]] auto is_outline_font(std::string_view t_file) -> bool
{
	const usize dot = t_file.rfind('.');
	if (dot == std::string_view::npos) return false;

	const std::string_view extension = t_file.substr(dot);

	return equals_ignoring_case(extension, ".ttf") || equals_ignoring_case(extension, ".ttc") || equals_ignoring_case(extension, ".otf");
}
}

struct GlyphCache {
	Texture*                       atlas      = nullptr;
	u32                            atlas_size = 0;
	const FontFace*                face       = nullptr;
	float                          scale      = 0.0f;
	float                          em_pixels  = 0.0f;
	Glyph                          ascii[128]{};
	bool                           ascii_ready[128]{};
	std::unordered_map<u32, Glyph> others;
	u32                            shelf_x      = K_ATLAS_PADDING;
	u32                            shelf_y      = K_ATLAS_PADDING;
	u32                            shelf_height = 0;
	std::vector<u8>                coverage;
	std::vector<u8>                rgba;
};

namespace {
auto forget_glyphs(GlyphCache* t_cache) -> void
{
	t_cache->others.clear();
	std::ranges::fill(t_cache->ascii_ready, false);
	t_cache->shelf_x      = K_ATLAS_PADDING;
	t_cache->shelf_y      = K_ATLAS_PADDING;
	t_cache->shelf_height = 0;
}

[[nodiscard]] auto reserve_space(GlyphCache* t_cache, u32 t_width, u32 t_height, u32* t_x, u32* t_y) -> bool
{
	const u32 size = t_cache->atlas_size;

	if (t_cache->shelf_x + t_width + K_ATLAS_PADDING > size) {
		t_cache->shelf_x = K_ATLAS_PADDING;
		t_cache->shelf_y += t_cache->shelf_height + K_ATLAS_PADDING;
		t_cache->shelf_height = 0;
	}

	if (t_width + K_ATLAS_PADDING * 2 > size || t_cache->shelf_y + t_height + K_ATLAS_PADDING > size) return false;

	*t_x = t_cache->shelf_x;
	*t_y = t_cache->shelf_y;
	t_cache->shelf_x += t_width + K_ATLAS_PADDING;
	t_cache->shelf_height = std::max(t_cache->shelf_height, t_height);

	return true;
}

[[nodiscard]] auto face_with_glyph(const GlyphCache* t_cache, u32 t_codepoint, int* t_index) -> const FontFace*
{
	const auto codepoint = static_cast<int>(t_codepoint);

	*t_index = stbtt_FindGlyphIndex(&t_cache->face->info, codepoint);
	if (*t_index != 0) return t_cache->face;

	for (const FontFace* fallback : fallback_faces()) {
		if (fallback == nullptr) continue;

		*t_index = stbtt_FindGlyphIndex(&fallback->info, codepoint);
		if (*t_index != 0) return fallback;
	}

	*t_index = 0;

	return t_cache->face;
}

[[nodiscard]] auto rasterize(GlyphCache* t_cache, u32 t_codepoint) -> Glyph
{
	int             index = 0;
	const FontFace* face  = face_with_glyph(t_cache, t_codepoint, &index);
	const float     scale = face == t_cache->face ? t_cache->scale : t_cache->em_pixels * face->em_scale;

	int advance = 0;
	int bearing = 0;
	stbtt_GetGlyphHMetrics(&face->info, index, &advance, &bearing);

	int x0 = 0;
	int y0 = 0;
	int x1 = 0;
	int y1 = 0;
	stbtt_GetGlyphBitmapBox(&face->info, index, scale, scale, &x0, &y0, &x1, &y1);

	Glyph glyph{
		.x0      = static_cast<float>(x0),
		.y0      = static_cast<float>(y0),
		.x1      = static_cast<float>(x0),
		.y1      = static_cast<float>(y0),
		.u0      = 0.0f,
		.v0      = 0.0f,
		.u1      = 0.0f,
		.v1      = 0.0f,
		.advance = static_cast<float>(advance) * scale,
	};

	const auto width  = static_cast<u32>(std::max(0, x1 - x0));
	const auto height = static_cast<u32>(std::max(0, y1 - y0));
	if (width == 0 || height == 0) return glyph;

	u32 x = 0;
	u32 y = 0;
	if (!reserve_space(t_cache, width, height, &x, &y)) {
		forget_glyphs(t_cache);
		if (!reserve_space(t_cache, width, height, &x, &y)) return glyph;
	}

	t_cache->coverage.resize(static_cast<usize>(width) * height);
	t_cache->rgba.resize(t_cache->coverage.size() * 4);
	stbtt_MakeGlyphBitmap(&face->info, t_cache->coverage.data(), static_cast<int>(width), static_cast<int>(height), static_cast<int>(width), scale, scale,
	                      index);

	for (usize i = 0; i < t_cache->coverage.size(); i += 1) {
		const float boosted = std::pow(t_cache->coverage[i] / 255.0f, K_COVERAGE_GAMMA) * 255.0f;

		t_cache->rgba[i * 4 + 0] = 255;
		t_cache->rgba[i * 4 + 1] = 255;
		t_cache->rgba[i * 4 + 2] = 255;
		t_cache->rgba[i * 4 + 3] = static_cast<u8>(std::min(255.0f, boosted));
	}

	t_cache->atlas->update(x, y, width, height, t_cache->rgba.data());

	const float size = static_cast<float>(t_cache->atlas_size);
	glyph.x1         = static_cast<float>(x1);
	glyph.y1         = static_cast<float>(y1);
	glyph.u0         = static_cast<float>(x) / size;
	glyph.v0         = static_cast<float>(y) / size;
	glyph.u1         = static_cast<float>(x + width) / size;
	glyph.v1         = static_cast<float>(y + height) / size;

	return glyph;
}
}

Font::Font()                                   = default;
Font::~Font()                                  = default;
Font::Font(Font&&) noexcept                    = default;
auto Font::operator=(Font&&) noexcept -> Font& = default;

auto Font::load(Renderer* t_renderer, const char* t_path, float t_pixel_height, float t_dpi_scale) -> bool
{
	const FontFace* face = open_face(t_path);
	if (face == nullptr) {
		std::println("Unsupported or unreadable font file: {}", t_path);
		return false;
	}

	const float           baked_pixel_height = t_pixel_height * t_dpi_scale;
	const u32             size               = atlas_size_for(baked_pixel_height);
	const std::vector<u8> empty(static_cast<usize>(size) * size * 4);
	const TextureLevel    level{empty.data(), size, size};

	atlas = std::make_unique<Texture>(t_renderer, std::span{&level, 1}, true);
	if (!atlas->is_valid()) return false;

	const float scale = stbtt_ScaleForPixelHeight(&face->info, baked_pixel_height);

	glyphs             = std::make_unique<GlyphCache>();
	glyphs->atlas      = atlas.get();
	glyphs->atlas_size = size;
	glyphs->face       = face;
	glyphs->scale      = scale;
	glyphs->em_pixels  = scale / face->em_scale;

	int ascent_units   = 0;
	int descent_units  = 0;
	int line_gap_units = 0;
	stbtt_GetFontVMetrics(&face->info, &ascent_units, &descent_units, &line_gap_units);

	const float logical_scale = scale / t_dpi_scale;

	bake_scale   = t_dpi_scale;
	pixel_height = t_pixel_height;
	ascent       = ascent_units * logical_scale;
	descent      = descent_units * logical_scale;
	line_gap     = line_gap_units * logical_scale;

	return true;
}

auto Font::glyph(u32 t_codepoint) const -> const Glyph*
{
	GlyphCache* cache = glyphs.get();

	if (t_codepoint < std::size(cache->ascii)) [[likely]] {
		if (!cache->ascii_ready[t_codepoint]) {
			cache->ascii[t_codepoint]       = rasterize(cache, t_codepoint);
			cache->ascii_ready[t_codepoint] = true;
		}

		return &cache->ascii[t_codepoint];
	}

	if (const auto found = cache->others.find(t_codepoint); found != cache->others.end()) return &found->second;

	const Glyph glyph = rasterize(cache, t_codepoint);

	return &cache->others.emplace(t_codepoint, glyph).first->second;
}

auto InstalledFonts::index_of_file(std::string_view t_file) const -> std::optional<u32>
{
	for (u32 i = 0; i < files.size(); i += 1) {
		if (equals_ignoring_case(files[i], t_file)) return i;
	}

	return std::nullopt;
}

auto installed_fonts() -> InstalledFonts
{
	std::vector<os::SystemFont> entries = os::system_fonts();
	std::erase_if(entries, [](const os::SystemFont& t_font) { return !is_outline_font(t_font.file); });

	std::ranges::stable_sort(entries, [](const os::SystemFont& t_a, const os::SystemFont& t_b) { return sorts_before_ignoring_case(t_a.name, t_b.name); });

	const auto duplicates =
		std::ranges::unique(entries, [](const os::SystemFont& t_a, const os::SystemFont& t_b) { return equals_ignoring_case(t_a.name, t_b.name); });
	entries.erase(duplicates.begin(), duplicates.end());

	InstalledFonts fonts;
	fonts.names.reserve(entries.size());
	fonts.files.reserve(entries.size());

	for (os::SystemFont& entry : entries) {
		fonts.names.push_back(std::move(entry.name));
		fonts.files.push_back(std::move(entry.file));
	}

	return fonts;
}

auto Fonts::load(Renderer* t_renderer, std::string_view t_file, float t_body_size, float t_secondary_size, float t_dpi_scale) -> bool
{
	if (t_file.empty()) return false;

	const std::string path = os::system_font_path(t_file);
	if (path.empty()) return false;

	Font loaded_body;
	Font loaded_secondary;
	Font loaded_caption;
	if (!loaded_body.load(t_renderer, path.c_str(), t_body_size * K_SETTING_TO_PIXEL_SCALE, t_dpi_scale) ||
	    !loaded_secondary.load(t_renderer, path.c_str(), t_secondary_size * K_SETTING_TO_PIXEL_SCALE, t_dpi_scale) ||
	    !loaded_caption.load(t_renderer, path.c_str(), t_secondary_size * K_CAPTION_SIZE_RATIO * K_SETTING_TO_PIXEL_SCALE, t_dpi_scale)) {
		return false;
	}

	body      = std::move(loaded_body);
	secondary = std::move(loaded_secondary);
	caption   = std::move(loaded_caption);

	return true;
}
