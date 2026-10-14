#pragma once

#include <bitset>
#include <optional>

#include "core/library.h"

using RowSet = std::bitset<K_MAX_VISIBLE_ACCOUNTS>;

[[nodiscard]] auto rows_before(const RowSet& t_rows, u32 t_row) -> u32;

// Where t_row lands once the rows in t_moved are taken out and put back together, starting at t_insert among the rows left.
[[nodiscard]] auto row_after_move(u32 t_row, const RowSet& t_moved, u32 t_insert) -> u32;

// Several picked rows lift together and travel as one block that stays in its part of the list, the favourites or the rest. insert is where the block would go among the rows left, and rank is how many of its rows come before the one held.
struct RowDrag {
	std::optional<u32> pressed_row;
	Vec2               press_point{};
	bool               lifted       = false;
	u32                from_row     = 0;
	u32                insert       = 0;
	u32                rank         = 0;
	u32                first_insert = 0;
	u32                last_insert  = 0;
	float              grab_offset  = 0.0f;
	RowSet             rows;

	auto lift(u32 t_row, const RowSet& t_picked, u32 t_section_first, u32 t_section_last) -> void;
	auto aim(float t_held_top_in_list, float t_row_height) -> void;
	[[nodiscard]] auto clamp_top(float t_top, float t_row_height) const -> float;
	[[nodiscard]] auto rows_from_held(u32 t_lifted_row) const -> float;
	[[nodiscard]] auto rows_moved_aside(u32 t_resting_row) const -> float;
};

// The box keeps its x on screen and its y in the list, so it stays on the rows it covers while the list scrolls.
class RowSelection {
  public:
	struct Box {
		Vec2   anchor;
		Vec2   point;
		bool   additive;
		bool   moved;
		RowSet base;
	};

	[[nodiscard]] auto contains(u32 t_row) const -> bool;
	[[nodiscard]] auto count() const -> u32;

	auto clear() -> void;
	auto toggle(u32 t_row) -> void;
	auto extend_to(u32 t_row) -> void;
	auto select_block(u32 t_first, u32 t_count) -> void;

	auto clear_if_rows_changed(const VisibleAccounts& t_rows) -> void;
	auto set_source_rows(const VisibleAccounts& t_rows) -> void;

	auto begin_box(Vec2 t_point, float t_content_top, bool t_additive) -> void;
	auto drag_box(Vec2 t_point, float t_content_top, float t_row_height, u32 t_row_count) -> void;
	auto end_box() -> void;
	auto cancel_box() -> void;

	[[nodiscard]] auto box_shown() const -> bool;
	[[nodiscard]] auto box_rect(float t_content_top) const -> Rect;

	RowSet             m_rows;
	std::optional<Box> m_box;

  private:
	std::optional<u32> m_anchor;
	VisibleAccounts    m_source;
};
