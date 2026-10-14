#include "ui/row_selection.h"

#include <algorithm>
#include <cmath>
#include <ranges>

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

auto RowSelection::extend_to(u32 t_row) -> void
{
	if (t_row >= K_MAX_VISIBLE_ACCOUNTS) return;

	const u32 anchor = m_anchor.value_or(t_row);
	const u32 first  = std::min(anchor, t_row);
	const u32 last   = std::max(anchor, t_row);

	m_rows.reset();
	select_block(first, last - first + 1);
	m_anchor = anchor;
}

auto RowSelection::select_block(u32 t_first, u32 t_count) -> void
{
	const u32 last  = std::min(t_first + t_count, K_MAX_VISIBLE_ACCOUNTS);
	const u32 first = std::min(t_first, last);

	for (const u32 row : std::views::iota(first, last)) {
		m_rows.set(row);
	}

	m_anchor = t_first;
}

auto RowSelection::clear_if_rows_changed(const VisibleAccounts& t_rows) -> void
{
	const bool same = std::ranges::equal(t_rows.view(), m_source.view());
	if (same) return;

	if (!m_box) {
		clear();
	}

	set_source_rows(t_rows);
}

auto RowSelection::set_source_rows(const VisibleAccounts& t_rows) -> void
{
	std::ranges::copy(t_rows.view(), m_source.refs);
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

	for (const u32 row : std::views::iota(0u, std::min(t_row_count, K_MAX_VISIBLE_ACCOUNTS))) {
		const float row_top = static_cast<float>(row) * t_row_height;

		if (row_top <= bottom && row_top + t_row_height > top) {
			m_rows.set(row);
		}
	}
}

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

auto RowDrag::lift(u32 t_row, const RowSet& t_picked, u32 t_section_first, u32 t_section_last) -> void
{
	rows.reset();

	if (t_row < K_MAX_VISIBLE_ACCOUNTS && t_picked[t_row] && t_picked.count() > 1) {
		const u32 last_row = std::min(t_section_last, K_MAX_VISIBLE_ACCOUNTS - 1);
		for (const u32 row : std::views::iota(t_section_first, last_row + 1)) {
			rows[row] = t_picked[row];
		}
	} else {
		rows.set(t_row);
	}

	lifted       = true;
	from_row     = t_row;
	rank         = rows_before(rows, t_row);
	first_insert = t_section_first;
	last_insert  = t_section_last + 1 - static_cast<u32>(rows.count());
	insert       = t_row - rank;
}

auto RowDrag::aim(float t_held_top_in_list, float t_row_height) -> void
{
	const float slot    = std::round(t_held_top_in_list / t_row_height) - static_cast<float>(rank);
	const float lowest  = static_cast<float>(first_insert);
	const float highest = static_cast<float>(last_insert);

	insert = static_cast<u32>(std::clamp(slot, lowest, highest));
}

auto RowDrag::clamp_top(float t_top, float t_row_height) const -> float
{
	const float lowest  = (static_cast<float>(first_insert) + static_cast<float>(rank)) * t_row_height;
	const float highest = (static_cast<float>(last_insert) + static_cast<float>(rank)) * t_row_height;

	return std::clamp(t_top, lowest, highest);
}

auto RowDrag::rows_from_held(u32 t_lifted_row) const -> float
{
	return static_cast<float>(rows_before(rows, t_lifted_row)) - static_cast<float>(rank);
}

auto RowDrag::rows_moved_aside(u32 t_resting_row) const -> float
{
	return static_cast<float>(row_after_move(t_resting_row, rows, insert)) - static_cast<float>(t_resting_row);
}
