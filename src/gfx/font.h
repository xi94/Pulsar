#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "stb/stb_truetype.h"

#include "core/types.h"

class Renderer;
class Texture;

struct Font {
	static constexpr u32 first_char = 32;
	static constexpr u32 char_count = 95;

	std::unique_ptr<Texture> atlas;
	stbtt_packedchar packed_chars[char_count]{};
	u32 atlas_size = 0;
	float bake_scale = 1.0f;
	float pixel_height = 0.0f;
	float ascent = 0.0f;
	float descent = 0.0f;
	float line_gap = 0.0f;

	Font();
	~Font();
	Font(Font &&) noexcept;
	Font &operator=(Font &&) noexcept;

	bool load(Renderer *t_renderer, const char *t_path, float t_pixel_height, float t_dpi_scale);

	float line_height() const
	{
		return ascent - descent + line_gap;
	}

	float centered_baseline(Rect t_box) const
	{
		return t_box.y + t_box.h * 0.5f + (ascent + descent) * 0.5f;
	}
};

struct InstalledFonts {
	std::vector<std::string> names;
	std::vector<std::string> files;

	std::optional<u32> index_of_file(std::string_view t_file) const;
};

InstalledFonts installed_fonts();

struct Fonts {
	Font body;
	Font secondary;
	Font caption;

	bool load(Renderer *t_renderer, std::string_view t_file, float t_body_size, float t_secondary_size, float t_dpi_scale);
};
