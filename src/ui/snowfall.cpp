#include "ui/snowfall.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include "core/animation.h"
#include "gfx/assets.h"
#include "gfx/draw_list.h"
#include "gfx/renderer.h"
#include "ui/theme.h"

namespace {
constexpr u32   K_SPRITE_SIZE        = 32;
constexpr float K_SPRITE_SOLID_SHARE = 0.72f;
constexpr float K_REFERENCE_AREA     = 680.0f * 312.0f;
constexpr float K_MIN_DENSITY        = 0.6f;
constexpr float K_MAX_DENSITY        = 3.0f;
constexpr u32   K_LAYER_FLAKES[]{150, 90};
constexpr float K_LAYER_DEPTH[]{0.38f, 0.68f};
constexpr float K_DEPTH_SPREAD = 0.7f;

constexpr float K_WIND                    = 8.4f;
constexpr float K_MAX_STEP                = 0.05f;
constexpr float K_UNFOCUSED_FRAME_SECONDS = 1.0f / 30.0f;
constexpr float K_SPAWN_BAND              = 60.0f;
constexpr float K_WRAP_MARGIN             = 20.0f;
constexpr float K_FALL_OUT_MARGIN         = 8.0f;
constexpr float K_TWO_PI                  = std::numbers::pi_v<float> * 2.0f;

constexpr float K_LIGHT_THEME_LUMINANCE = 0.25f;
constexpr Color K_DARK_THEME_SNOW{255, 255, 255, 255};
constexpr Color K_LIGHT_THEME_SNOW{150, 164, 188, 255};

[[nodiscard]] auto dot_pixels() -> std::vector<u8>
{
	constexpr float HALF = K_SPRITE_SIZE * 0.5f;

	std::vector<u8> pixels(static_cast<usize>(K_SPRITE_SIZE) * K_SPRITE_SIZE * 4);

	for (u32 y = 0; y < K_SPRITE_SIZE; y += 1) {
		for (u32 x = 0; x < K_SPRITE_SIZE; x += 1) {
			const float distance = std::hypot(static_cast<float>(x) + 0.5f - HALF, static_cast<float>(y) + 0.5f - HALF) / HALF;
			const float edge     = std::clamp((1.0f - distance) / (1.0f - K_SPRITE_SOLID_SHARE), 0.0f, 1.0f);
			u8*         pixel    = pixels.data() + (static_cast<usize>(y) * K_SPRITE_SIZE + x) * 4;

			pixel[0] = 255;
			pixel[1] = 255;
			pixel[2] = 255;
			pixel[3] = to_alpha(edge * edge * (3.0f - 2.0f * edge));
		}
	}

	return pixels;
}

[[nodiscard]] auto depth_of(u32 t_layer) -> float
{
	return 1.0f - K_DEPTH_SPREAD * (1.0f - K_LAYER_DEPTH[t_layer]);
}

[[nodiscard]] auto snow_color() -> Color
{
	return luminance(g_theme.window) > K_LIGHT_THEME_LUMINANCE ? K_LIGHT_THEME_SNOW : K_DARK_THEME_SNOW;
}
}

Snowfall::~Snowfall() = default;

auto Snowfall::create_texture(Renderer* t_renderer) -> void
{
	const std::vector<u8> pixels = dot_pixels();
	m_sprite                     = Assets::create_texture(t_renderer, pixels.data(), K_SPRITE_SIZE, K_SPRITE_SIZE);
}

auto Snowfall::clear() -> void
{
	for (std::vector<Flake>& flakes : m_flakes) {
		flakes.clear();
	}
}

auto Snowfall::random() -> float
{
	m_random_state ^= m_random_state << 13;
	m_random_state ^= m_random_state >> 17;
	m_random_state ^= m_random_state << 5;

	return static_cast<float>(m_random_state >> 8) * (1.0f / 16777216.0f);
}

auto Snowfall::spawn(u32 t_layer, bool t_anywhere) -> Snowfall::Flake
{
	const float depth = depth_of(t_layer);

	return Flake{
		.x          = m_area.x + random() * m_area.w,
		.y          = t_anywhere ? m_area.y + random() * m_area.h : m_area.y - random() * K_SPAWN_BAND,
		.radius     = t_layer == 0 ? 0.75f + random() * 0.5f : 1.1f + random() * 0.8f,
		.fall_speed = (15.0f + random() * 16.0f) * (0.35f + depth * 0.9f),
		.phase      = random() * K_TWO_PI,
		.frequency  = 0.45f + random() * 1.1f,
		.sway       = (4.0f + random() * 9.0f) * (0.4f + depth),
		.alpha      = t_layer == 0 ? 0.35f + random() * 0.2f : 0.65f + random() * 0.25f,
	};
}

auto Snowfall::update(float t_delta_seconds, Rect t_area, bool t_focused) -> void
{
	m_area = t_area;
	if (m_area.w <= 0.0f || m_area.h <= 0.0f) return;

	const float step    = std::min(t_delta_seconds, K_MAX_STEP);
	const float density = std::clamp(m_area.w * m_area.h / K_REFERENCE_AREA, K_MIN_DENSITY, K_MAX_DENSITY);
	const float left    = m_area.x - K_WRAP_MARGIN;
	const float right   = m_area.right() + K_WRAP_MARGIN;
	m_time += step;

	for (u32 layer = 0; layer < K_LAYER_COUNT; layer += 1) {
		std::vector<Flake>* flakes  = &m_flakes[layer];
		const auto          wanted  = static_cast<usize>(std::lround(static_cast<float>(K_LAYER_FLAKES[layer]) * density));
		const bool          filling = flakes->empty();

		while (flakes->size() < wanted) {
			flakes->push_back(spawn(layer, filling));
		}

		flakes->resize(wanted);

		const float drift = K_WIND * (0.35f + depth_of(layer) * 0.8f);

		for (Flake& flake : *flakes) {
			flake.y += flake.fall_speed * step;
			flake.x += (drift + std::cos(m_time * flake.frequency + flake.phase) * flake.sway) * step;

			if (flake.x < left) {
				flake.x += right - left;
			} else if (flake.x > right) {
				flake.x -= right - left;
			}

			if (flake.y > m_area.bottom() + K_FALL_OUT_MARGIN) {
				flake = spawn(layer, false);
			}
		}
	}

	if (t_focused) {
		animation::request_frame();
	} else {
		animation::request_frame_after(K_UNFOCUSED_FRAME_SECONDS);
	}
}

auto Snowfall::draw(DrawList* t_draw_list) const -> void
{
	if (m_flakes[0].empty()) return;

	const Color color = snow_color();

	t_draw_list->push_clip(m_area);

	for (u32 layer = 0; layer < K_LAYER_COUNT; layer += 1) {
		for (const Flake& flake : m_flakes[layer]) {
			const Rect dot{flake.x - flake.radius, flake.y - flake.radius, flake.radius * 2.0f, flake.radius * 2.0f};
			t_draw_list->add_image(dot, m_sprite.get(), with_alpha(color, to_alpha(flake.alpha)));
		}
	}

	t_draw_list->pop_clip();
}
