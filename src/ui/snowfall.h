#pragma once

#include <memory>
#include <vector>

#include "core/types.h"

class DrawList;
class Renderer;
class Texture;

class Snowfall {
  public:
	Snowfall() = default;
	~Snowfall();

	Snowfall(const Snowfall&)                    = delete;
	auto operator=(const Snowfall&) -> Snowfall& = delete;

	auto create_texture(Renderer* t_renderer) -> void;
	auto update(float t_delta_seconds, Rect t_area, bool t_focused) -> void;
	auto clear() -> void;
	auto draw(DrawList* t_draw_list) const -> void;

  private:
	static constexpr u32 K_LAYER_COUNT = 2;

	struct Flake {
		float x;
		float y;
		float radius;
		float fall_speed;
		float phase;
		float frequency;
		float sway;
		float alpha;
	};

	[[nodiscard]] auto random() -> float;
	[[nodiscard]] auto spawn(u32 t_layer, bool t_anywhere) -> Flake;

	std::unique_ptr<Texture> m_sprite;
	std::vector<Flake>       m_flakes[K_LAYER_COUNT];
	Rect                     m_area{};
	float                    m_time         = 0.0f;
	u32                      m_random_state = 0x9E3779B9u;
};
