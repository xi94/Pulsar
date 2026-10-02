#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/types.h"

class Renderer;
class Texture;
struct GlyphCache;

struct Glyph {
	float x0;
	float y0;
	float x1;
	float y1;
	float u0;
	float v0;
	float u1;
	float v1;
	float advance;
};

struct Font {
	std::unique_ptr<Texture>    atlas;
	std::unique_ptr<GlyphCache> glyphs;
	float                       bake_scale   = 1.0f;
	float                       pixel_height = 0.0f;
	float                       ascent       = 0.0f;
	float                       descent      = 0.0f;
	float                       line_gap     = 0.0f;

	Font();
	~Font();
	Font(Font&&) noexcept;
	auto operator=(Font&&) noexcept -> Font&;

	[[nodiscard]] auto load(Renderer* t_renderer, const char* t_path, float t_pixel_height, float t_dpi_scale) -> bool;
	[[nodiscard]] auto glyph(u32 t_codepoint) const -> const Glyph*;

	[[nodiscard]] auto line_height() const -> float
	{
		return ascent - descent + line_gap;
	}

	[[nodiscard]] auto centered_baseline(Rect t_box) const -> float
	{
		return t_box.y + t_box.h * 0.5f + (ascent + descent) * 0.5f;
	}
};

struct InstalledFonts {
	std::vector<std::string> names;
	std::vector<std::string> files;

	[[nodiscard]] auto index_of_file(std::string_view t_file) const -> std::optional<u32>;
};

[[nodiscard]] auto installed_fonts() -> InstalledFonts;

struct Fonts {
	Font body;
	Font secondary;
	Font caption;

	[[nodiscard]] auto load(Renderer* t_renderer, std::string_view t_file, float t_body_size, float t_secondary_size, float t_dpi_scale) -> bool;
};
