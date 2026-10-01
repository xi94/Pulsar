#include "ui/snowfall.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <span>

#include "core/animation.h"
#include "gfx/assets.h"
#include "gfx/draw_list.h"
#include "gfx/renderer.h"
#include "ui/theme.h"

namespace {
constexpr u32 sprite_size = 64;
constexpr float reference_area = 680.0f * 312.0f;
constexpr float min_density = 0.6f;
constexpr float max_density = 3.0f;
constexpr u32 layer_flakes[]{150, 90};
constexpr float layer_depth[]{0.38f, 0.68f};
constexpr float depth_spread = 0.7f;

constexpr float wind = 8.4f;
constexpr float max_step = 0.05f;
constexpr float unfocused_frame_seconds = 1.0f / 30.0f;
constexpr float spawn_band = 60.0f;
constexpr float wrap_margin = 20.0f;
constexpr float fall_out_margin = 8.0f;
constexpr float sprite_scale = 3.2f;
constexpr float two_pi = std::numbers::pi_v<float> * 2.0f;

constexpr float light_theme_luminance = 0.25f;
constexpr Color dark_theme_snow{255, 255, 255, 255};
constexpr Color light_theme_snow{150, 164, 188, 255};

struct GradientStop {
	float at;
	float alpha;
};

constexpr GradientStop far_stops[]{{0.0f, 0.8f}, {0.3f, 0.45f}, {0.7f, 0.08f}, {1.0f, 0.0f}};
constexpr GradientStop near_stops[]{{0.0f, 1.0f}, {0.32f, 0.9f}, {0.55f, 0.35f}, {0.8f, 0.06f}, {1.0f, 0.0f}};

float gradient_alpha(std::span<const GradientStop> t_stops, float t_distance)
{
	for (usize i = 1; i < t_stops.size(); i += 1) {
		if (t_distance <= t_stops[i].at) {
			const GradientStop &from = t_stops[i - 1];
			const GradientStop &to = t_stops[i];

			return lerp(from.alpha, to.alpha, (t_distance - from.at) / (to.at - from.at));
		}
	}

	return 0.0f;
}

std::vector<u8> sprite_pixels(std::span<const GradientStop> t_stops)
{
	constexpr float half = sprite_size * 0.5f;

	std::vector<u8> pixels(static_cast<usize>(sprite_size) * sprite_size * 4);

	for (u32 y = 0; y < sprite_size; y += 1) {
		for (u32 x = 0; x < sprite_size; x += 1) {
			const float distance = std::hypot(static_cast<float>(x) + 0.5f - half, static_cast<float>(y) + 0.5f - half) / half;
			u8 *pixel = pixels.data() + (static_cast<usize>(y) * sprite_size + x) * 4;

			pixel[0] = 255;
			pixel[1] = 255;
			pixel[2] = 255;
			pixel[3] = to_alpha(gradient_alpha(t_stops, std::min(distance, 1.0f)));
		}
	}

	return pixels;
}

float depth_of(u32 t_layer)
{
	return 1.0f - depth_spread * (1.0f - layer_depth[t_layer]);
}

Color snow_color()
{
	return luminance(theme().window) > light_theme_luminance ? light_theme_snow : dark_theme_snow;
}
}

Snowfall::~Snowfall() = default;

void Snowfall::create_textures(Renderer *t_renderer)
{
	const std::span<const GradientStop> stops[layer_count]{far_stops, near_stops};

	for (u32 layer = 0; layer < layer_count; layer += 1) {
		const std::vector<u8> pixels = sprite_pixels(stops[layer]);
		m_sprites[layer] = Assets::create_texture(t_renderer, pixels.data(), sprite_size, sprite_size);
	}
}

void Snowfall::clear()
{
	for (std::vector<Flake> &flakes : m_flakes) {
		flakes.clear();
	}
}

float Snowfall::random()
{
	m_random_state ^= m_random_state << 13;
	m_random_state ^= m_random_state >> 17;
	m_random_state ^= m_random_state << 5;

	return static_cast<float>(m_random_state >> 8) * (1.0f / 16777216.0f);
}

Snowfall::Flake Snowfall::spawn(u32 t_layer, bool t_anywhere)
{
	const float depth = depth_of(t_layer);

	return Flake{
		.x = m_area.x + random() * m_area.w,
		.y = t_anywhere ? m_area.y + random() * m_area.h : m_area.y - random() * spawn_band,
		.radius = (0.7f + random() * 1.3f) * (0.55f + depth * 0.6f),
		.fall_speed = (15.0f + random() * 16.0f) * (0.35f + depth * 0.9f),
		.phase = random() * two_pi,
		.frequency = 0.45f + random() * 1.1f,
		.sway = (4.0f + random() * 9.0f) * (0.4f + depth),
		.alpha = t_layer == 0 ? 0.28f + random() * 0.22f : 0.62f + random() * 0.3f,
	};
}

void Snowfall::update(float t_delta_seconds, Rect t_area, bool t_focused)
{
	m_area = t_area;
	if (m_area.w <= 0.0f || m_area.h <= 0.0f) return;

	const float step = std::min(t_delta_seconds, max_step);
	const float density = std::clamp(m_area.w * m_area.h / reference_area, min_density, max_density);
	const float left = m_area.x - wrap_margin;
	const float right = m_area.right() + wrap_margin;
	m_time += step;

	for (u32 layer = 0; layer < layer_count; layer += 1) {
		std::vector<Flake> *flakes = &m_flakes[layer];
		const auto wanted = static_cast<usize>(std::lround(static_cast<float>(layer_flakes[layer]) * density));
		const bool filling = flakes->empty();

		while (flakes->size() < wanted) {
			flakes->push_back(spawn(layer, filling));
		}

		flakes->resize(wanted);

		const float drift = wind * (0.35f + depth_of(layer) * 0.8f);

		for (Flake &flake : *flakes) {
			flake.y += flake.fall_speed * step;
			flake.x += (drift + std::cos(m_time * flake.frequency + flake.phase) * flake.sway) * step;

			if (flake.x < left) {
				flake.x += right - left;
			} else if (flake.x > right) {
				flake.x -= right - left;
			}

			if (flake.y > m_area.bottom() + fall_out_margin) {
				flake = spawn(layer, false);
			}
		}
	}

	if (t_focused) {
		animation::request_frame();
	} else {
		animation::request_frame_after(unfocused_frame_seconds);
	}
}

void Snowfall::draw(DrawList *t_draw_list) const
{
	if (m_flakes[0].empty()) return;

	const Color color = snow_color();

	t_draw_list->push_clip(m_area);

	for (u32 layer = 0; layer < layer_count; layer += 1) {
		for (const Flake &flake : m_flakes[layer]) {
			const float size = flake.radius * sprite_scale;
			t_draw_list->add_image(Rect{flake.x - size, flake.y - size, size * 2.0f, size * 2.0f}, m_sprites[layer].get(),
								   with_alpha(color, to_alpha(flake.alpha)));
		}
	}

	t_draw_list->pop_clip();
}
