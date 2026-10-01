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

	Snowfall(const Snowfall &) = delete;
	Snowfall &operator=(const Snowfall &) = delete;

	void create_textures(Renderer *t_renderer);
	void update(float t_delta_seconds, Rect t_area, bool t_focused);
	void clear();
	void draw(DrawList *t_draw_list) const;

  private:
	static constexpr u32 layer_count = 2;

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

	float random();
	Flake spawn(u32 t_layer, bool t_anywhere);

	std::unique_ptr<Texture> m_sprites[layer_count];
	std::vector<Flake> m_flakes[layer_count];
	Rect m_area{};
	float m_time = 0.0f;
	u32 m_random_state = 0x9E3779B9u;
};
