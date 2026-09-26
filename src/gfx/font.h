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

class Font {
  public:
	static constexpr u32 first_char = 32;
	static constexpr u32 char_count = 95;

	Font();
	~Font();
	Font(Font &&) noexcept;
	Font &operator=(Font &&) noexcept;

	bool load(Renderer &t_renderer, const char *t_path, float t_pixel_height, float t_dpi_scale);

	float pixel_height() const
	{
		return m_pixel_height;
	}

	float ascent() const
	{
		return m_ascent;
	}

	float descent() const
	{
		return m_descent;
	}

	float line_height() const
	{
		return m_ascent - m_descent + m_line_gap;
	}

	float centered_baseline(Rect t_box) const
	{
		return t_box.y + t_box.h * 0.5f + (m_ascent + m_descent) * 0.5f;
	}

	const Texture *atlas() const
	{
		return m_atlas.get();
	}

	const stbtt_packedchar *packed_chars() const
	{
		return m_packed_chars;
	}

	u32 atlas_size() const
	{
		return m_atlas_size;
	}

	float bake_scale() const
	{
		return m_bake_scale;
	}

  private:
	std::unique_ptr<Texture> m_atlas;
	stbtt_packedchar m_packed_chars[char_count]{};
	u32 m_atlas_size = 0;
	float m_bake_scale = 1.0f;
	float m_pixel_height = 0.0f;
	float m_ascent = 0.0f;
	float m_descent = 0.0f;
	float m_line_gap = 0.0f;
};

struct InstalledFonts {
	std::vector<std::string> names;
	std::vector<std::string> files;

	std::optional<u32> index_of_file(std::string_view t_file) const;
};

InstalledFonts installed_fonts();

class Fonts {
  public:
	bool load(Renderer &t_renderer, std::string_view t_file, float t_body_size, float t_secondary_size,
			  float t_dpi_scale);

	const Font &body() const
	{
		return m_body;
	}

	const Font &secondary() const
	{
		return m_secondary;
	}

  private:
	Font m_body;
	Font m_secondary;
};
