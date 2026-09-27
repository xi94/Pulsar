#pragma once

#include <span>
#include <string_view>

#include "ui/commands.h"
#include "ui/widget.h"

class Fonts;
struct Settings;

struct ContextMenuItem {
	std::string_view label;
	Command command;
	bool enabled = true;
};

class ContextMenu : public Widget {
  public:
	ContextMenu(const Settings &t_settings, const Fonts &t_fonts, CommandQueue &t_commands);

	void open(Vec2 t_position, std::span<const ContextMenuItem> t_items, Vec2 t_window_size);
	void close();

	void draw(DrawList &t_draw_list) override;

	bool on_pointer_up(Vec2 t_point) override;
	bool on_right_click(Vec2 t_point) override;
	bool on_key_down(u32 t_key) override;

	bool is_blocking() const override
	{
		return m_open;
	}

	CursorKind cursor() const override;

  private:
	static constexpr u32 max_items = 8;

	Rect menu_rect() const;
	Rect item_rect(u32 t_index) const;
	i32 item_at(Vec2 t_point) const;

	const Settings &m_settings;
	const Fonts &m_fonts;
	CommandQueue &m_commands;

	bool m_open = false;
	Vec2 m_position{};
	ContextMenuItem m_items[max_items]{};
	u32 m_item_count = 0;
};
