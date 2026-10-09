#include "ui/row_selection.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float K_BOX_THRESHOLD = 4.0f;
}

auto rows_before(const RowSet& t_rows, u32 t_row) -> u32
{
	const u32 kept = std::min(t_row, K_MAX_VISIBLE_ACCOUNTS);
	if (kept == 0) return 0;

	return static_cast<u32>((t_rows << (K_MAX_VISIBLE_ACCOUNTS - kept)).count());
}

auto row_after_move(u32 t_row, const RowSet& t_moved, u32 t_insert) -> u32
{
	const u32 before = rows_before(t_moved, t_row);
	if (t_row < K_MAX_VISIBLE_ACCOUNTS && t_moved[t_row]) return t_insert + before;

	const u32 rest = t_row - before;

	return rest < t_insert ? rest : rest + static_cast<u32>(t_moved.count());
}

auto RowSelection::contains(u32 t_row) const -> bool
{
	return t_row < K_MAX_VISIBLE_ACCOUNTS && m_rows[t_row];
}

auto RowSelection::count() const -> u32
{
	return static_cast<u32>(m_rows.count());
}

auto RowSelection::is_empty() const -> bool
{
	return m_rows.none();
}

auto RowSelection::clear() -> void
{
	m_rows.reset();
	m_anchor.reset();
	m_box.reset();
}

auto RowSelection::toggle(u32 t_row) -> void
{
	if (t_row >= K_MAX_VISIBLE_ACCOUNTS) return;

	m_rows.flip(t_row);
	m_anchor = t_row;
}

// Picks the rows from the last one toggled to this one, in place of whatever was picked.
auto RowSelection::extend_to(u32 t_row) -> void
{
	if (t_row >= K_MAX_VISIBLE_ACCOUNTS) return;

	const u32 anchor = m_anchor.value_or(t_row);

	m_rows.reset();
	select_block(std::min(anchor, t_row), std::max(anchor, t_row) - std::min(anchor, t_row) + 1);
	m_anchor = anchor;
}

auto RowSelection::select_block(u32 t_first, u32 t_count) -> void
{
	for (u32 row = t_first; row < std::min(t_first + t_count, K_MAX_VISIBLE_ACCOUNTS); row += 1) {
		m_rows.set(row);
	}

	m_anchor = t_first;
}

auto RowSelection::follow(const VisibleAccounts& t_rows) -> void
{
	const bool same = t_rows.count == m_source.count && std::ranges::equal(t_rows.view(), m_source.view());
	if (same) return;

	if (!m_box) {
		clear();
	}

	remember(t_rows);
}

auto RowSelection::remember(const VisibleAccounts& t_rows) -> void
{
	std::copy(t_rows.refs, t_rows.refs + t_rows.count, m_source.refs);
	m_source.count = t_rows.count;
}

auto RowSelection::begin_box(Vec2 t_point, float t_content_top, bool t_additive) -> void
{
	const Vec2 anchor{t_point.x, t_point.y - t_content_top};

	m_box = Box{.anchor = anchor, .point = anchor, .additive = t_additive, .moved = false, .base = t_additive ? m_rows : RowSet{}};
}

// Picks every row the box reaches over, going by height alone since rows run the whole width of the list.
auto RowSelection::drag_box(Vec2 t_point, float t_content_top, float t_row_height, u32 t_row_count) -> void
{
	if (!m_box) return;

	m_box->point = Vec2{t_point.x, t_point.y - t_content_top};

	const float dx = m_box->point.x - m_box->anchor.x;
	const float dy = m_box->point.y - m_box->anchor.y;
	m_box->moved   = m_box->moved || dx * dx + dy * dy > K_BOX_THRESHOLD * K_BOX_THRESHOLD;

	if (!m_box->moved || t_row_height <= 0.0f) return;

	const float top    = std::min(m_box->anchor.y, m_box->point.y);
	const float bottom = std::max(m_box->anchor.y, m_box->point.y);

	m_rows = m_box->base;

	for (u32 row = 0; row < std::min(t_row_count, K_MAX_VISIBLE_ACCOUNTS); row += 1) {
		const float row_top = static_cast<float>(row) * t_row_height;

		if (row_top <= bottom && row_top + t_row_height > top) {
			m_rows.set(row);
		}
	}
}

// A click on an empty part of the list, without dragging, clears the selection unless a modifier was held to add to it.
auto RowSelection::end_box() -> void
{
	if (!m_box) return;

	if (!m_box->moved && !m_box->additive) {
		m_rows.reset();
		m_anchor.reset();
	}

	m_box.reset();
}

auto RowSelection::cancel_box() -> void
{
	if (!m_box) return;

	m_rows = m_box->base;
	m_box.reset();
}

auto RowSelection::box_shown() const -> bool
{
	return m_box && m_box->moved;
}

auto RowSelection::box_rect(float t_content_top) const -> Rect
{
	if (!m_box) return Rect{};

	const float left   = std::min(m_box->anchor.x, m_box->point.x);
	const float top    = std::min(m_box->anchor.y, m_box->point.y);
	const float right  = std::max(m_box->anchor.x, m_box->point.x);
	const float bottom = std::max(m_box->anchor.y, m_box->point.y);

	return Rect{left, t_content_top + top, right - left, bottom - top};
}

auto RowDrag::lift(u32 t_row, const RowSet& t_picked, u32 t_first, u32 t_last) -> void
{
	rows.reset();

	if (t_row < K_MAX_VISIBLE_ACCOUNTS && t_picked[t_row] && t_picked.count() > 1) {
		for (u32 row = t_first; row <= std::min(t_last, K_MAX_VISIBLE_ACCOUNTS - 1); row += 1) {
			rows[row] = t_picked[row];
		}
	} else {
		rows.set(t_row);
	}

	lifted      = true;
	from_row    = t_row;
	rank        = rows_before(rows, t_row);
	first       = t_first;
	last_insert = t_last + 1 - static_cast<u32>(rows.count());
	insert      = t_row - rank;
}

auto RowDrag::aim(float t_top, float t_row_height) -> void
{
	const float slot = std::round(t_top / t_row_height) - static_cast<float>(rank);

	insert = static_cast<u32>(std::clamp(slot, static_cast<float>(first), static_cast<float>(last_insert)));
}

auto RowDrag::clamp_top(float t_top, float t_row_height) const -> float
{
	const float lowest  = (static_cast<float>(first) + static_cast<float>(rank)) * t_row_height;
	const float highest = (static_cast<float>(last_insert) + static_cast<float>(rank)) * t_row_height;

	return std::clamp(t_top, lowest, highest);
}

auto RowDrag::place_in_block(u32 t_row) const -> float
{
	return static_cast<float>(rows_before(rows, t_row)) - static_cast<float>(rank);
}

auto RowDrag::shift(u32 t_row) const -> float
{
	return static_cast<float>(row_after_move(t_row, rows, insert)) - static_cast<float>(t_row);
}
