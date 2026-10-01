#pragma once

#include <string_view>

#include "ui/commands.h"
#include "ui/widget.h"

class Assets;
struct Fonts;

class AppMenu : public Widget {
  public:
	AppMenu(const Fonts &t_fonts, const Assets &t_assets, CommandQueue &t_commands);

	void open(bool t_unlocked, std::string_view t_update_status);
	void close();

	bool is_open() const
	{
		return m_open;
	}

	void update(float t_delta_seconds) override;
	void draw(DrawList &t_draw_list) override;

	bool on_pointer_up(Vec2 t_point) override;

	bool is_blocking() const override
	{
		return m_open_amount > 0.01f;
	}

	CursorKind cursor() const override;

  private:
	static constexpr u32 max_items = 8;

	bool is_enabled(u32 t_item) const;

	const Fonts &m_fonts;
	const Assets &m_assets;
	CommandQueue &m_commands;

	bool m_open = false;
	bool m_unlocked = false;
	std::string_view m_update_status;
	float m_open_amount = 0.0f;
	float m_item_hover[max_items]{};
};
