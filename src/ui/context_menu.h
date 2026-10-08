#pragma once

#include <optional>
#include <span>
#include <string_view>

#include "gfx/assets.h"
#include "ui/commands.h"
#include "ui/widget.h"

class Assets;
struct Fonts;

struct ContextMenuItem {
	std::string_view label;
	Command          command;
	bool             enabled = true;
	std::string_view shortcut;
	Asset            icon = Asset::COUNT;
	// A line above this item splits it from the group before it.
	bool separated = false;
	// Destructive items are red and only act on a second click, made before the bar under them runs out.
	bool destructive = false;
};

class ContextMenu : public Widget {
  public:
	ContextMenu(const Fonts* t_fonts, const Assets* t_assets, CommandQueue* t_commands);

	auto open(Vec2 t_position, std::span<const ContextMenuItem> t_items, Vec2 t_window_size) -> void;
	auto close() -> void;

	auto update(float t_delta_seconds) -> void override;
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
	const Assets* m_assets;
	CommandQueue* m_commands;

	bool            m_open = false;
	Vec2            m_position{};
	float           m_width = 0.0f;
	ContextMenuItem m_items[K_MAX_ITEMS]{};
	u32             m_item_count = 0;
	bool            m_has_icons  = false;

	std::optional<u32> m_armed;
	float              m_armed_seconds = 0.0f;
};
