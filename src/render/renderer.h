#pragma once

#include <memory>
#include <span>
#include <string_view>

#include "core/types.h"
#include "gfx/draw_list.h"

class Renderer;
class RenderBackend;

namespace os {
class Window;
}

struct TextureLevel {
	const u8* rgba_pixels;
	u32       width;
	u32       height;
};

struct Backdrop {
	BackgroundStyle style     = BackgroundStyle::NONE;
	float           intensity = 0.0f;
	float           light     = 0.0f;
	float           grain     = 0.0f;
};

class Texture {
  public:
	Texture(Renderer* t_renderer, std::span<const TextureLevel> t_levels, bool t_updatable = false);
	~Texture();

	Texture(const Texture&)                    = delete;
	auto operator=(const Texture&) -> Texture& = delete;

	[[nodiscard]] auto is_valid() const -> bool;
	auto update(u32 t_x, u32 t_y, u32 t_width, u32 t_height, const u8* t_rgba_pixels) -> void;
	auto restore(std::span<const TextureLevel> t_levels) -> void;

	[[nodiscard]] auto aspect() const -> float
	{
		return m_height > 0 ? static_cast<float>(m_width) / static_cast<float>(m_height) : 1.0f;
	}

	u32 m_slot;

	u32 m_width;
	u32 m_height;

  private:
	Renderer* m_renderer;
};

[[nodiscard]] auto graphics_api_name(GraphicsApi t_api) noexcept -> std::string_view;

class Renderer {
  public:
	static constexpr u32 K_INVALID_TEXTURE_SLOT = 0xFFFFFFFFu;
	static constexpr u32 K_MAX_TEXTURES         = 64;

	Renderer();
	~Renderer();

	Renderer(const Renderer&)                    = delete;
	auto operator=(const Renderer&) -> Renderer& = delete;

	[[nodiscard]] auto init(const os::Window* t_window, GraphicsApi t_preferred_api) -> bool;
	auto resize(const os::Window* t_window) -> void;

	auto render(const DrawList* t_draw_list, Color t_clear_color) -> void;
	[[nodiscard]] auto supports_backdrop_blur() const -> bool;
	[[nodiscard]] auto is_device_lost() const -> bool;
	[[nodiscard]] auto recover(const os::Window* t_window) -> bool;

	[[nodiscard]] auto create_texture(std::span<const TextureLevel> t_levels, bool t_updatable) -> u32;
	auto update_texture(u32 t_slot, u32 t_x, u32 t_y, u32 t_width, u32 t_height, const u8* t_rgba_pixels) -> void;
	auto destroy_texture(u32 t_slot) -> void;
	auto restore_texture(u32 t_slot, std::span<const TextureLevel> t_levels) -> void;

	GraphicsApi m_api                 = GraphicsApi::NATIVE;
	GraphicsApi m_requested_api       = GraphicsApi::NATIVE;
	float       m_effect_time_seconds = 0.0f;
	Backdrop    m_backdrop;

  private:
	[[nodiscard]] auto allocate_texture_slot() -> u32;
	auto release_texture_slot(u32 t_slot) -> void;
	auto remember_size(const os::Window* t_window) -> void;

	std::unique_ptr<RenderBackend> m_backend;

	u32 m_free_texture_slots[K_MAX_TEXTURES]{};
	u32 m_free_texture_count = 0;
	u32 m_texture_high_water = 0;

	u32   m_physical_width  = 0;
	u32   m_physical_height = 0;
	float m_logical_width   = 0.0f;
	float m_logical_height  = 0.0f;
};
