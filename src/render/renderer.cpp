#include "render/renderer.h"

#include <cassert>

Texture::Texture(Renderer* t_renderer, std::span<const TextureLevel> t_levels, bool t_updatable)
	: m_renderer(t_renderer)
	, m_slot(t_renderer->create_texture(t_levels, t_updatable))
	, m_width(t_levels.front().width)
	, m_height(t_levels.front().height)
{
}

Texture::~Texture()
{
	if (is_valid()) {
		m_renderer->destroy_texture(m_slot);
	}
}

auto Texture::is_valid() const -> bool
{
	return m_slot != Renderer::K_INVALID_TEXTURE_SLOT;
}

auto Texture::update(u32 t_x, u32 t_y, u32 t_width, u32 t_height, const u8* t_rgba_pixels) -> void
{
	if (is_valid()) {
		m_renderer->update_texture(m_slot, t_x, t_y, t_width, t_height, t_rgba_pixels);
	}
}

auto Renderer::allocate_texture_slot() -> u32
{
	if (m_free_texture_count > 0) {
		m_free_texture_count -= 1;
		return m_free_texture_slots[m_free_texture_count];
	}

	assert(m_texture_high_water < K_MAX_TEXTURES);
	if (m_texture_high_water >= K_MAX_TEXTURES) return K_INVALID_TEXTURE_SLOT;

	const u32 slot = m_texture_high_water;
	m_texture_high_water += 1;

	return slot;
}

auto Renderer::release_texture_slot(u32 t_slot) -> void
{
	assert(t_slot < m_texture_high_water && m_free_texture_count < K_MAX_TEXTURES);

	m_free_texture_slots[m_free_texture_count] = t_slot;
	m_free_texture_count += 1;
}
