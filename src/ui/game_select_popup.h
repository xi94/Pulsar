#pragma once

#include "core/types.h"

class DrawList;
class Fonts;
class Library;
struct Settings;

class GameSelectPopup {
  public:
	GameSelectPopup(const Library &t_library, const Settings &t_settings, const Fonts &t_fonts);

	void open(Rect t_anchor, Rect t_bounds);
	void close();

	bool is_open() const
	{
		return m_open_amount > 0.01f;
	}

	u16 mask() const
	{
		return m_mask;
	}

	void set_mask(u16 t_mask)
	{
		m_mask = t_mask;
	}

	void update(float t_delta_seconds);
	bool on_pointer_down(Vec2 t_point);
	CursorKind cursor(Vec2 t_mouse) const;
	void draw(DrawList &t_draw_list, Vec2 t_mouse) const;

  private:
	struct Placement {
		Rect full;
		bool below;
	};

	Placement placement() const;
	Rect shown_rect() const;
	Rect row_rect(u32 t_game) const;
	bool is_last_checked(u32 t_game) const;

	const Library &m_library;
	const Settings &m_settings;
	const Fonts &m_fonts;

	bool m_open = false;
	float m_open_amount = 0.0f;
	u16 m_mask = 0;
	Rect m_anchor{};
	Rect m_bounds{};
};
