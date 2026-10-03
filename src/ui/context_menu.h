#pragma once

#include <span>
#include <string_view>

#include "ui/commands.h"
#include "ui/widget.h"

struct Fonts;

struct ContextMenuItem {
	std::string_view label;
	Command          command;
	bool             enabled = true;
	std::string_view shortcut;
};

class ContextMenu : public Widget {
  public:
	ContextMenu(const Fonts* t_fonts, CommandQueue* t_commands);

	auto open(Vec2 t_position, std::span<const ContextMenuItem> t_items, Vec2 t_window_size) -> void;
	auto close() -> void;

	auto draw(DrawList* t_draw_list) -> void override;

	auto on_pointer_up(Vec2 t_point) -> bool override;
	auto on_right_click(Vec2 t_point) -> bool override;
	auto on_key_down(os::Key t_key) -> bool override;

	[[nodiscard]] auto is_blocking() const -> bool override
	{
		return m_open;
	}

	[[nodiscard]] auto cursor() const -> CursorKind override;

  private:
	static constexpr u32 K_MAX_ITEMS = 8;

	[[nodiscard]] auto menu_rect() const -> Rect;
	[[nodiscard]] auto item_rect(u32 t_index) const -> Rect;
	[[nodiscard]] auto item_at(Vec2 t_point) const -> i32;

	const Fonts*  m_fonts;
	CommandQueue* m_commands;

	bool            m_open = false;
	Vec2            m_position{};
	float           m_width = 0.0f;
	ContextMenuItem m_items[K_MAX_ITEMS]{};
	u32             m_item_count = 0;
};
