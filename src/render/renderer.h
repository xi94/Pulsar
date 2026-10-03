#pragma once

#include <memory>
#include <span>

#include "core/types.h"
#include "gfx/draw_list.h"

class Renderer;

namespace os {
class Window;
}

struct TextureLevel {
	const u8* rgba_pixels;
	u32       width;
	u32       height;
};

class Texture {
  public:
	Texture(Renderer* t_renderer, std::span<const TextureLevel> t_levels, bool t_updatable = false);
	~Texture();

	Texture(const Texture&)                    = delete;
	auto operator=(const Texture&) -> Texture& = delete;

	[[nodiscard]] auto is_valid() const -> bool;
	auto update(u32 t_x, u32 t_y, u32 t_width, u32 t_height, const u8* t_rgba_pixels) -> void;

	[[nodiscard]] auto slot() const -> u32
	{
		return m_slot;
	}

	[[nodiscard]] auto width() const -> u32
	{
		return m_width;
	}

	[[nodiscard]] auto height() const -> u32
	{
		return m_height;
	}

	[[nodiscard]] auto aspect() const -> float
	{
		return m_height > 0 ? static_cast<float>(m_width) / static_cast<float>(m_height) : 1.0f;
	}

  private:
	Renderer* m_renderer;
	u32       m_slot;
	u32       m_width;
	u32       m_height;
};

class Renderer {
  public:
	static constexpr u32 K_INVALID_TEXTURE_SLOT = 0xFFFFFFFFu;
	static constexpr u32 K_MAX_TEXTURES         = 64;

	Renderer();
	~Renderer();

	Renderer(const Renderer&)                    = delete;
	auto operator=(const Renderer&) -> Renderer& = delete;

	[[nodiscard]] auto init(const os::Window* t_window) -> bool;
	auto resize(const os::Window* t_window) -> void;

	auto set_effect_time(float t_seconds) -> void
	{
		m_effect_time_seconds = t_seconds;
	}

	auto set_backdrop(u32 t_style, float t_intensity, float t_light, float t_grain) -> void
	{
		m_backdrop_style     = t_style;
		m_backdrop_intensity = t_intensity;
		m_backdrop_light     = t_light;
		m_backdrop_grain     = t_grain;
	}

	auto render(const DrawList* t_draw_list, Color t_clear_color) -> void;

	[[nodiscard]] auto create_texture(std::span<const TextureLevel> t_levels, bool t_updatable) -> u32;
	auto update_texture(u32 t_slot, u32 t_x, u32 t_y, u32 t_width, u32 t_height, const u8* t_rgba_pixels) -> void;
	auto destroy_texture(u32 t_slot) -> void;

  private:
	struct Backend;

	[[nodiscard]] auto allocate_texture_slot() -> u32;
	auto release_texture_slot(u32 t_slot) -> void;

	std::unique_ptr<Backend> m_backend;

	u32 m_free_texture_slots[K_MAX_TEXTURES]{};
	u32 m_free_texture_count = 0;
	u32 m_texture_high_water = 0;

	u32   m_physical_width      = 0;
	u32   m_physical_height     = 0;
	float m_logical_width       = 0.0f;
	float m_logical_height      = 0.0f;
	float m_effect_time_seconds = 0.0f;
	u32   m_backdrop_style      = 0;
	float m_backdrop_intensity  = 0.0f;
	float m_backdrop_light      = 0.0f;
	float m_backdrop_grain      = 0.0f;
};
