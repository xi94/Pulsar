#pragma once

#include <bitset>
#include <optional>

#include "core/library.h"

using RowSet = std::bitset<K_MAX_VISIBLE_ACCOUNTS>;

[[nodiscard]] auto rows_before(const RowSet& t_rows, u32 t_row) -> u32;

// Where t_row lands once the rows in t_moved are taken out and put back together, starting at t_insert among the rows left.
[[nodiscard]] auto row_after_move(u32 t_row, const RowSet& t_moved, u32 t_insert) -> u32;

// A press on a row, which lifts it once the pointer moves far enough. Several picked rows lift together and travel as one block that
// stays in its part of the list, the favourites or the rest. insert is where the block would go among the rows left, and rank is how
// many of its rows come before the one held. Offsets of lifted rows are measured from their place in the block.
struct RowDrag {
	std::optional<u32> pressed_row;
	Vec2               press_point{};
	bool               lifted      = false;
	u32                from_row    = 0;
	u32                insert      = 0;
	u32                rank        = 0;
	u32                first       = 0;
	u32                last_insert = 0;
	float              grab_offset = 0.0f;
	RowSet             rows;

	// Lifts t_row, or every picked row from t_first to t_last when t_row is one of several picked.
	auto lift(u32 t_row, const RowSet& t_picked, u32 t_first, u32 t_last) -> void;
	// Aims the block at the slot nearest to where the held row's top is, given in list space.
	auto aim(float t_top, float t_row_height) -> void;
	[[nodiscard]] auto clamp_top(float t_top, float t_row_height) const -> float;
	// A lifted row's place in the block, in rows from the one held.
	[[nodiscard]] auto place_in_block(u32 t_row) const -> float;
	// How many rows a row that isn't lifted moves to make room for the block.
	[[nodiscard]] auto shift(u32 t_row) const -> float;
};

// Rows picked by dragging a box over them or with Ctrl and Shift clicks. The box keeps its x on screen and its y in the list, so it
// stays on the rows it covers while the list scrolls. The selection is dropped once the rows it was picked from change.
class RowSelection {
  public:
	[[nodiscard]] auto rows() const -> const RowSet&
	{
		return m_rows;
	}

	[[nodiscard]] auto contains(u32 t_row) const -> bool;
	[[nodiscard]] auto count() const -> u32;
	[[nodiscard]] auto is_empty() const -> bool;

	auto clear() -> void;
	auto toggle(u32 t_row) -> void;
	auto extend_to(u32 t_row) -> void;
	auto select_block(u32 t_first, u32 t_count) -> void;

	// Drops the selection if t_rows differ from the rows it was picked from.
	auto follow(const VisibleAccounts& t_rows) -> void;
	// Takes t_rows as the rows the selection was picked from, after a change that kept it.
	auto remember(const VisibleAccounts& t_rows) -> void;

	auto begin_box(Vec2 t_point, float t_content_top, bool t_additive) -> void;
	auto drag_box(Vec2 t_point, float t_content_top, float t_row_height, u32 t_row_count) -> void;
	auto end_box() -> void;
	auto cancel_box() -> void;

	[[nodiscard]] auto is_boxing() const -> bool
	{
		return m_box.has_value();
	}

	[[nodiscard]] auto box_shown() const -> bool;
	[[nodiscard]] auto box_rect(float t_content_top) const -> Rect;

  private:
	struct Box {
		Vec2   anchor;
		Vec2   point;
		bool   additive;
		bool   moved;
		RowSet base;
	};

	RowSet             m_rows;
	std::optional<u32> m_anchor;
	std::optional<Box> m_box;
	VisibleAccounts    m_source;
};
