#include "ui/list_popup.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "core/animation.h"
#include "core/settings.h"
#include "core/str.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "ui/controls.h"
#include "ui/text.h"
#include "ui/theme.h"

namespace {
constexpr float            K_POPUP_RADIUS        = 10.0f;
constexpr float            K_ANCHOR_GAP          = 6.0f;
constexpr float            K_BOUNDS_MARGIN       = 10.0f;
constexpr float            K_LIST_PADDING        = 6.0f;
constexpr float            K_SEARCH_FIELD_MARGIN = 6.0f;
constexpr float            K_SEARCH_FIELD_RADIUS = 7.0f;
constexpr float            K_SEARCH_ICON_INSET   = 9.0f;
constexpr float            K_SEARCH_ICON_SIZE    = 14.0f;
constexpr float            K_SEARCH_ICON_GAP     = 7.0f;
constexpr float            K_CLEAR_SIZE          = 20.0f;
constexpr float            K_CLEAR_MARGIN        = 4.0f;
constexpr float            K_ROW_INSET           = 10.0f;
constexpr float            K_ROW_RADIUS          = 6.0f;
constexpr float            K_ROW_GAP             = 10.0f;
constexpr float            K_CHECK_SIZE          = 14.0f;
constexpr float            K_SCROLLBAR_GAP       = 4.0f;
constexpr float            K_FOOTER_PADDING_X    = 10.0f;
constexpr float            K_FOOTER_PADDING_Y    = 5.0f;
constexpr float            K_HINT_GAP            = 14.0f;
constexpr float            K_HINT_KEY_GAP        = 5.0f;
constexpr float            K_HINT_ARROW_GAP      = 3.0f;
constexpr float            K_HINT_LINE_GAP       = 4.0f;
constexpr Vec2             K_HINT_ARROW_SIZE{7.0f, 4.0f};
constexpr std::string_view K_HINT_KEYS[]{"", "Enter", "Esc", ""};
constexpr std::string_view K_HINT_LABELS[]{"navigate", "select", "close", "preview"};
constexpr u32              K_HOVER_HINT       = 3;
constexpr float            K_MATCH_CONTRAST   = 0.3f;
constexpr u32              K_MAX_VISIBLE_ROWS = 8;

constexpr float K_OPEN_EASE_RATE   = 22.0f;
constexpr float K_RESIZE_EASE_RATE = 20.0f;
constexpr float K_SLIDE_DISTANCE   = 6.0f;

[[nodiscard]] auto search_height(const Fonts* t_fonts) -> float
{
	return std::max(40.0f, t_fonts->body.line_height() + 16.0f);
}

[[nodiscard]] auto row_height(const Fonts* t_fonts) -> float
{
	return std::max(30.0f, t_fonts->body.line_height() + 10.0f);
}
}

ListPopup::ListPopup(const Fonts* t_fonts, const Assets* t_assets, const Settings* t_settings, ListPopupOptions t_options)
	: m_fonts(t_fonts)
	, m_assets(t_assets)
	, m_settings(t_settings)
	, m_options(std::move(t_options))
{
	m_search.set_placeholder(m_options.search_placeholder);
}

auto ListPopup::open(std::span<const std::string_view> t_items, std::optional<u32> t_selected) -> void
{
	m_items    = t_items;
	m_selected = t_selected;
	m_search.set_value("");
	m_search.set_focused(is_searchable());
	rebuild_matches();

	const u32 last   = m_matches.empty() ? 0 : static_cast<u32>(m_matches.size()) - 1;
	m_highlighted    = std::min(t_selected.value_or(0), last);
	m_previewed_item = t_selected;
	m_hover_seconds  = 0.0f;
	m_shown_rows     = static_cast<float>(shown_row_target());
	m_press          = Press::None;
	m_open           = true;

	const Layout current            = layout();
	const float  highlighted_center = (m_highlighted + 0.5f) * row_height(m_fonts);
	m_scroll.jump_to(highlighted_center - current.list.h * 0.5f, list_scroll(current));
}

auto ListPopup::close() -> void
{
	m_open  = false;
	m_press = Press::None;
	m_search.set_focused(false);
	m_search.on_pointer_up();
	m_scroll.on_pointer_up();
}

auto ListPopup::previewed_item() const -> std::optional<u32>
{
	return m_open ? m_previewed_item : std::nullopt;
}

auto ListPopup::popup_width() const -> float
{
	const float width = std::max(m_options.min_width, m_anchor.w);
	if (!is_searchable()) return width;

	float hints = K_FOOTER_PADDING_X * 2.0f + 2.0f;
	for (u32 i = 0; i < hint_count(); i += 1) {
		hints += hint_width(i) + (i > 0 ? K_HINT_GAP : 0.0f);
	}

	return std::max(width, std::min(std::ceil(hints) + 1.0f, m_bounds.w - K_BOUNDS_MARGIN * 2.0f));
}

auto ListPopup::hint_count() const -> u32
{
	return m_options.hover_preview_seconds > 0.0f ? 4 : 3;
}

auto ListPopup::hint_width(u32 t_hint) const -> float
{
	const Font& font = m_fonts->secondary;
	float       keys = 0.0f;

	if (t_hint == 0) {
		keys = controls::keycap_height(font) * 2.0f + K_HINT_ARROW_GAP + K_HINT_KEY_GAP;
	} else if (t_hint == K_HOVER_HINT) {
		keys = controls::keycap_height(font) + K_HINT_KEY_GAP;
	} else if (!K_HINT_KEYS[t_hint].empty()) {
		keys = controls::keycap_width(font, K_HINT_KEYS[t_hint]) + K_HINT_KEY_GAP;
	}

	return keys + text_width(font, K_HINT_LABELS[t_hint]);
}

auto ListPopup::hint_lines() const -> u32
{
	const float room  = popup_width() - 2.0f - K_FOOTER_PADDING_X * 2.0f;
	u32         lines = 1;
	float       x     = 0.0f;

	for (u32 i = 0; i < hint_count(); i += 1) {
		const float width = hint_width(i);

		if (x > 0.0f && x + width > room) {
			lines += 1;
			x = 0.0f;
		}

		x += width + K_HINT_GAP;
	}

	return lines;
}

auto ListPopup::footer_height() const -> float
{
	const auto lines = static_cast<float>(hint_lines());

	return lines * controls::keycap_height(m_fonts->secondary) + (lines - 1.0f) * K_HINT_LINE_GAP + K_FOOTER_PADDING_Y * 2.0f;
}

auto ListPopup::chrome_height() const -> float
{
	const float search = is_searchable() ? search_height(m_fonts) + 1.0f : 0.0f;
	const float footer = is_searchable() ? footer_height() + 2.0f : 0.0f;

	return search + footer + K_LIST_PADDING * 2.0f;
}

auto ListPopup::placement() const -> ListPopup::Placement
{
	const Placement below{m_anchor.bottom() + K_ANCHOR_GAP, m_bounds.bottom() - K_BOUNDS_MARGIN, true};
	const Placement above{m_bounds.y + K_BOUNDS_MARGIN, m_anchor.y - K_ANCHOR_GAP, false};

	const float room_below = below.bottom - below.top;
	const float room_above = above.bottom - above.top;
	const float tallest    = chrome_height() + K_MAX_VISIBLE_ROWS * row_height(m_fonts);

	return room_below >= tallest || room_below >= room_above ? below : above;
}

auto ListPopup::row_capacity() const -> u32
{
	const Placement space = placement();
	const float     rows  = (space.bottom - space.top - chrome_height()) / row_height(m_fonts);

	return std::clamp(static_cast<u32>(std::max(rows, 0.0f)), 1u, K_MAX_VISIBLE_ROWS);
}

auto ListPopup::shown_row_target() const -> u32
{
	return std::clamp(static_cast<u32>(m_matches.size()), 1u, row_capacity());
}

auto ListPopup::layout() const -> ListPopup::Layout
{
	const Placement space  = placement();
	const float     width  = popup_width();
	const float     height = chrome_height() + m_shown_rows * row_height(m_fonts);
	const float     slide  = (1.0f - m_open_amount) * K_SLIDE_DISTANCE;
	const float     y      = space.opens_below ? space.top - slide : space.bottom - height + slide;

	const float min_x = m_bounds.x + K_BOUNDS_MARGIN;
	const float max_x = std::max(min_x, m_bounds.right() - K_BOUNDS_MARGIN - width);

	Layout result{};
	result.popup = Rect{std::clamp(m_anchor.right() - width, min_x, max_x), snapped_to_pixel(std::max(y, m_bounds.y + K_BOUNDS_MARGIN)), width, height};

	Rect remaining = result.popup;
	if (is_searchable()) {
		result.search    = remaining.split_top(search_height(m_fonts));
		result.separator = remaining.split_top(1.0f).inset(1.0f, 0.0f);
		result.footer    = remaining.split_bottom(footer_height() + 1.0f).inset(1.0f, 0.0f);
		result.footer.h -= 1.0f;
		remaining.split_bottom(1.0f);
	}

	result.list = remaining.inset(K_LIST_PADDING);

	return result;
}

auto ListPopup::list_scroll(const Layout& t_layout) const -> ScrollGeometry
{
	const Rect& list = t_layout.list;
	const Rect  track{list.right() - K_SCROLLBAR_WIDTH, list.y, K_SCROLLBAR_WIDTH, list.h};

	return ScrollGeometry{track, static_cast<float>(m_matches.size()) * row_height(m_fonts), list.h};
}

auto ListPopup::search_field_rect(const Layout& t_layout) const -> Rect
{
	const float left  = t_layout.search.x + K_SEARCH_FIELD_MARGIN + K_SEARCH_ICON_INSET + K_SEARCH_ICON_SIZE + K_SEARCH_ICON_GAP;
	const float right = clear_button_rect(t_layout).x - K_SEARCH_ICON_GAP;

	return Rect{left, t_layout.search.y, std::max(0.0f, right - left), t_layout.search.h};
}

auto ListPopup::clear_button_rect(const Layout& t_layout) const -> Rect
{
	const Rect& search = t_layout.search;

	return Rect{search.right() - K_SEARCH_FIELD_MARGIN - K_CLEAR_MARGIN - K_CLEAR_SIZE, search.center().y - K_CLEAR_SIZE * 0.5f, K_CLEAR_SIZE, K_CLEAR_SIZE};
}

auto ListPopup::row_rect(const Layout& t_layout, u32 t_match) const -> Rect
{
	const float height  = row_height(m_fonts);
	const bool  scrolls = Scrollable::is_needed(list_scroll(t_layout));
	const float width   = t_layout.list.w - (scrolls ? K_SCROLLBAR_WIDTH + K_SCROLLBAR_GAP : 0.0f);

	return Rect{t_layout.list.x, t_layout.list.y + t_match * height - m_scroll.offset(), width, height};
}

auto ListPopup::match_at(const Layout& t_layout, Vec2 t_point) const -> std::optional<u32>
{
	if (!t_layout.list.contains(t_point)) return std::nullopt;

	const float row   = (t_point.y - t_layout.list.y + m_scroll.offset()) / row_height(m_fonts);
	const auto  match = static_cast<u32>(std::max(0.0f, row));
	if (match >= m_matches.size() || !row_rect(t_layout, match).contains(t_point)) return std::nullopt;

	return match;
}

auto ListPopup::rebuild_matches() -> void
{
	const std::string_view query = m_search.value();
	m_matched_query              = query;
	m_matches.clear();

	for (u32 i = 0; i < m_items.size(); i += 1) {
		if (find_ignoring_case(m_items[i], query) == 0) {
			m_matches.push_back(i);
		}
	}

	for (u32 i = 0; i < m_items.size(); i += 1) {
		const usize found = find_ignoring_case(m_items[i], query);
		if (found != 0 && found != std::string_view::npos) {
			m_matches.push_back(i);
		}
	}
}

auto ListPopup::refresh_matches() -> void
{
	if (m_search.value() == m_matched_query) return;

	rebuild_matches();
	m_highlighted = 0;
	m_scroll.jump_to(0.0f, list_scroll(layout()));
}

auto ListPopup::move_highlight(i32 t_rows) -> void
{
	if (m_matches.empty()) return;

	const i32 last   = static_cast<i32>(m_matches.size()) - 1;
	m_highlighted    = static_cast<u32>(std::clamp(static_cast<i32>(m_highlighted) + t_rows, 0, last));
	m_previewed_item = m_matches[m_highlighted];

	const Layout current = layout();
	const Rect   row     = row_rect(current, m_highlighted);
	m_scroll.reveal(row.y, row.bottom(), current.list.y, current.list.bottom(), list_scroll(current));
}

auto ListPopup::choose_highlighted() -> std::optional<u32>
{
	if (m_highlighted >= m_matches.size()) return std::nullopt;

	const u32 chosen = m_matches[m_highlighted];
	close();

	return chosen;
}

auto ListPopup::update(float t_delta_seconds, Rect t_anchor, Rect t_bounds) -> void
{
	m_anchor = t_anchor;
	m_bounds = t_bounds;

	if (m_open) {
		refresh_matches();
	}

	if (m_open && m_highlighted < m_matches.size()) {
		m_hover_seconds += t_delta_seconds;

		if (m_hover_seconds < m_options.hover_preview_seconds) {
			animation::request_frame_after(m_options.hover_preview_seconds - m_hover_seconds);
		}

		if (m_hover_seconds >= m_options.hover_preview_seconds) {
			m_previewed_item = m_matches[m_highlighted];
		}
	}

	m_open_amount = animation::ease_toward(m_open_amount, m_open ? 1.0f : 0.0f, K_OPEN_EASE_RATE, t_delta_seconds);
	m_shown_rows  = animation::ease_toward(m_shown_rows, static_cast<float>(shown_row_target()), K_RESIZE_EASE_RATE, t_delta_seconds,
	                                       animation::K_SETTLED_PIXELS / row_height(m_fonts));
	m_scroll.update(t_delta_seconds);
	m_search.update(t_delta_seconds);
}

auto ListPopup::on_pointer_down(Vec2 t_point) -> void
{
	if (!m_open) return;

	const Layout current = layout();
	m_pressed_match.reset();

	if (!current.popup.contains(t_point)) {
		m_press = Press::Outside;
	} else if (m_scroll.on_pointer_down(t_point, list_scroll(current))) {
		m_press = Press::Scrollbar;
	} else if (!m_search.value().empty() && clear_button_rect(current).contains(t_point)) {
		m_press = Press::Clear;
	} else if (current.search.contains(t_point)) {
		m_press = Press::Search;
		m_search.on_pointer_down(m_fonts->body, search_field_rect(current), t_point.x);
	} else {
		m_press         = Press::Row;
		m_pressed_match = match_at(current, t_point);
	}
}

auto ListPopup::on_pointer_move(Vec2 t_point) -> void
{
	if (!m_open) return;

	const Layout current = layout();
	m_scroll.on_pointer_move(t_point.y, list_scroll(current));

	if (m_search.is_selecting()) {
		m_search.on_pointer_move(m_fonts->body, search_field_rect(current), t_point.x);
	}

	if (m_press == Press::None || m_press == Press::Row) {
		const std::optional<u32> hovered = match_at(current, t_point);
		if (hovered && *hovered != m_highlighted) {
			m_highlighted   = *hovered;
			m_hover_seconds = 0.0f;
		}
	}
}

auto ListPopup::on_pointer_up(Vec2 t_point) -> std::optional<u32>
{
	const Press press = std::exchange(m_press, Press::None);
	m_scroll.on_pointer_up();
	m_search.on_pointer_up();

	if (!m_open) return std::nullopt;

	const Layout current = layout();

	switch (press) {
		case Press::Outside: {
			if (!current.popup.contains(t_point)) {
				close();
			}
			break;
		}

		case Press::Clear: {
			if (clear_button_rect(current).contains(t_point)) {
				m_search.set_value("");
				refresh_matches();
			}
			break;
		}

		case Press::Row: {
			if (m_pressed_match && match_at(current, t_point) == m_pressed_match) {
				m_highlighted = *m_pressed_match;
				return choose_highlighted();
			}
			break;
		}

		case Press::None:
		case Press::Search:
		case Press::Scrollbar: {
			break;
		}
	}

	return std::nullopt;
}

auto ListPopup::on_right_click(Vec2 t_point) -> TextInput*
{
	if (!m_open || !is_searchable()) return nullptr;

	const Layout current = layout();
	if (!current.search.contains(t_point)) return nullptr;

	m_search.on_right_click(m_fonts->body, search_field_rect(current), t_point.x);

	return &m_search;
}

auto ListPopup::on_scroll(float t_wheel_delta) -> void
{
	if (!m_open) return;

	m_scroll.on_scroll(t_wheel_delta, list_scroll(layout()));
}

auto ListPopup::on_key_down(os::Key t_key) -> std::optional<u32>
{
	if (!m_open) return std::nullopt;

	const auto page = static_cast<i32>(row_capacity()) - 1;
	refresh_matches();

	switch (t_key) {
		case os::Key::Escape: {
			close();
			break;
		}

		case os::Key::Enter: {
			return choose_highlighted();
		}

		case os::Key::Up: {
			move_highlight(-1);
			break;
		}

		case os::Key::Down: {
			move_highlight(1);
			break;
		}

		case os::Key::PageUp: {
			move_highlight(-page);
			break;
		}

		case os::Key::PageDown: {
			move_highlight(page);
			break;
		}

		default: {
			m_search.on_key_down(t_key);
			refresh_matches();
			break;
		}
	}

	return std::nullopt;
}

auto ListPopup::on_char(u32 t_character) -> void
{
	if (!m_open) return;

	m_search.on_char(t_character);
	refresh_matches();
}

auto ListPopup::cursor(Vec2 t_mouse) const -> CursorKind
{
	if (!m_open) return CursorKind::Arrow;
	if (m_scroll.is_dragging()) return CursorKind::Drag;
	if (m_search.is_selecting()) return CursorKind::IBeam;

	const Layout current = layout();
	if (!current.popup.contains(t_mouse)) return CursorKind::Arrow;
	if (!m_search.value().empty() && clear_button_rect(current).contains(t_mouse)) return CursorKind::Hand;
	if (current.search.contains(t_mouse)) return CursorKind::IBeam;
	if (m_scroll.is_over_track(t_mouse, list_scroll(current))) return CursorKind::Hand;

	return match_at(current, t_mouse) ? CursorKind::Hand : CursorKind::Arrow;
}

auto ListPopup::draw_search(DrawList* t_draw_list, const Layout& t_layout, Vec2 t_mouse, u8 t_alpha) -> void
{
	const Rect& search    = t_layout.search;
	const bool  has_query = !m_search.value().empty();
	const Rect  field     = search.inset(K_SEARCH_FIELD_MARGIN);
	const Rect  icon{field.x + K_SEARCH_ICON_INSET, search.center().y - K_SEARCH_ICON_SIZE * 0.5f, K_SEARCH_ICON_SIZE, K_SEARCH_ICON_SIZE};

	t_draw_list->add_bordered_rect(field, rounded(K_SEARCH_FIELD_RADIUS), faded(g_theme.field, t_alpha), faded(g_theme.separator, t_alpha), 1.0f);
	controls::draw_magnifier(t_draw_list, icon, faded(has_query ? g_theme.text_dim : g_theme.text_faint, t_alpha));
	m_search.draw(t_draw_list, m_fonts->body, search_field_rect(t_layout), faded(g_theme.text, t_alpha), faded(m_settings->accent, t_alpha),
	              search_field_rect(t_layout));

	if (!has_query) return;

	const Rect clear   = clear_button_rect(t_layout);
	const bool hovered = clear.contains(t_mouse);

	if (hovered) {
		t_draw_list->add_rounded_rect(clear, rounded(clear.w * 0.5f), faded(g_theme.control_hover, t_alpha));
	}

	controls::draw_x(t_draw_list, clear.inset(2.0f), faded(hovered ? g_theme.text : g_theme.text_faint, t_alpha));
}

auto ListPopup::draw_label(DrawList* t_draw_list, Rect t_row, float t_left, float t_max_width, std::string_view t_label, u8 t_alpha) const -> void
{
	const Font& font = m_fonts->body;
	const Vec2  baseline{t_left, font.centered_baseline(t_row)};
	const Color text  = faded(g_theme.text, t_alpha);
	const usize found = find_ignoring_case(t_label, m_matched_query);

	if (m_matched_query.empty() || found == std::string_view::npos || text_width(font, t_label) > t_max_width) {
		draw_text_truncated(t_draw_list, font, baseline, t_label, t_max_width, text);
		return;
	}

	const std::string_view before    = t_label.substr(0, found);
	const std::string_view matched   = t_label.substr(found, m_matched_query.size());
	const std::string_view after     = t_label.substr(found + matched.size());
	const float            matched_x = baseline.x + text_width(font, before);
	const float            after_x   = matched_x + text_width(font, matched);
	const Color            match     = faded(mix(m_settings->accent, g_theme.text, K_MATCH_CONTRAST), t_alpha);

	draw_text(t_draw_list, font, baseline, before, text);
	draw_text(t_draw_list, font, Vec2{matched_x, baseline.y}, matched, match);
	draw_text(t_draw_list, font, Vec2{after_x, baseline.y}, after, text);
}

auto ListPopup::draw_row(DrawList* t_draw_list, Rect t_row, u32 t_item, bool t_highlighted, u8 t_alpha) const -> void
{
	const Color backdrop = t_highlighted ? hovered(g_theme.popup) : g_theme.popup;

	if (t_highlighted) {
		t_draw_list->add_rounded_rect(t_row, rounded(K_ROW_RADIUS), faded(backdrop, t_alpha));
	}

	float left = t_row.x + K_ROW_INSET;

	if (m_options.draw_preview) {
		const Vec2 size = m_options.preview_size;
		const Rect preview{left, t_row.center().y - size.y * 0.5f, size.x, size.y};

		m_options.draw_preview(t_draw_list, preview, t_item, backdrop, t_alpha);
		left = preview.right() + K_ROW_GAP;
	}

	const Rect check{t_row.right() - K_ROW_INSET - K_CHECK_SIZE, t_row.center().y - K_CHECK_SIZE * 0.5f, K_CHECK_SIZE, K_CHECK_SIZE};
	if (m_selected == t_item) {
		controls::draw_check(t_draw_list, m_assets, check, faded(m_settings->accent, t_alpha));
	}

	draw_label(t_draw_list, t_row, left, check.x - K_ROW_GAP - left, m_items[t_item], t_alpha);
}

auto ListPopup::draw_rows(DrawList* t_draw_list, const Layout& t_layout, Vec2 t_mouse, u8 t_alpha) const -> void
{
	if (m_matches.empty()) {
		draw_text_centered(t_draw_list, m_fonts->secondary, t_layout.list, m_options.empty_message, faded(g_theme.text_faint, t_alpha));
		return;
	}

	const float height = row_height(m_fonts);
	const auto  first  = static_cast<u32>(std::max(0.0f, m_scroll.offset() / height));
	const u32   last   = std::min(static_cast<u32>(m_matches.size()), static_cast<u32>((m_scroll.offset() + t_layout.list.h) / height) + 1);

	t_draw_list->push_clip(t_layout.list);

	for (u32 match = first; match < last; match += 1) {
		draw_row(t_draw_list, row_rect(t_layout, match), m_matches[match], match == m_highlighted, t_alpha);
	}

	t_draw_list->pop_clip();

	m_scroll.draw_edge_fade(t_draw_list, t_layout.list, list_scroll(t_layout), faded(g_theme.popup, t_alpha));
	m_scroll.draw(t_draw_list, list_scroll(t_layout), t_mouse, t_alpha);
}

auto ListPopup::draw(DrawList* t_draw_list, Vec2 t_mouse) -> void
{
	if (m_open_amount <= 0.01f) return;

	const auto   alpha   = to_alpha(m_open_amount);
	const Layout current = layout();
	controls::draw_popup_shadow(t_draw_list, current.popup, K_POPUP_RADIUS, m_open_amount);
	t_draw_list->add_bordered_rect(current.popup, rounded(K_POPUP_RADIUS), faded(g_theme.popup, alpha), faded(g_theme.border, alpha), 1.0f);

	if (is_searchable()) {
		draw_search(t_draw_list, current, t_mouse, alpha);
	}

	draw_rows(t_draw_list, current, t_mouse, alpha);

	if (is_searchable()) {
		draw_key_hints(t_draw_list, current, alpha);
	}
}

auto ListPopup::draw_key_hints(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) const -> void
{
	const Font& font   = m_fonts->secondary;
	const Rect& footer = t_layout.footer;
	const float radius = K_POPUP_RADIUS - 1.0f;
	const float cap    = controls::keycap_height(font);
	const float start  = footer.x + K_FOOTER_PADDING_X;
	const float right  = footer.right() - K_FOOTER_PADDING_X;
	const Color label  = faded(g_theme.text_faint, t_alpha);
	float       x      = start;
	float       y      = footer.y + K_FOOTER_PADDING_Y;

	t_draw_list->add_rect(Rect{footer.x, footer.y - 1.0f, footer.w, 1.0f}, faded(g_theme.separator, t_alpha));
	t_draw_list->add_rounded_rect(footer, rounded(0.0f, 0.0f, radius, radius), faded(g_theme.field, t_alpha));

	for (u32 i = 0; i < hint_count(); i += 1) {
		const float width = hint_width(i);

		if (x > start && x + width > right) {
			x = start;
			y += cap + K_HINT_LINE_GAP;
		}

		const float baseline = font.centered_baseline(Rect{x, y, width, cap});
		float       text_x   = x;

		if (i == 0) {
			for (const bool up : {true, false}) {
				const Rect key{snapped_to_pixel(text_x), snapped_to_pixel(y), cap, cap};
				const Rect chevron{key.center().x - K_HINT_ARROW_SIZE.x * 0.5f, key.center().y - K_HINT_ARROW_SIZE.y * 0.5f - 0.5f, K_HINT_ARROW_SIZE.x,
				                   K_HINT_ARROW_SIZE.y};

				controls::draw_keycap_frame(t_draw_list, key, g_theme.field, t_alpha);
				controls::draw_chevron(t_draw_list, chevron, up, faded(controls::keycap_label_color(), t_alpha));
				text_x += cap + K_HINT_ARROW_GAP;
			}

			text_x += K_HINT_KEY_GAP - K_HINT_ARROW_GAP;
		} else if (i == K_HOVER_HINT) {
			controls::draw_mouse_keycap(t_draw_list, Rect{snapped_to_pixel(text_x), snapped_to_pixel(y), cap, cap}, g_theme.field, t_alpha);
			text_x += cap + K_HINT_KEY_GAP;
		} else if (!K_HINT_KEYS[i].empty()) {
			const float key_width = controls::keycap_width(font, K_HINT_KEYS[i]);

			controls::draw_keycap(t_draw_list, font, Rect{snapped_to_pixel(text_x), snapped_to_pixel(y), key_width, cap}, K_HINT_KEYS[i], g_theme.field,
			                      t_alpha);
			text_x += key_width + K_HINT_KEY_GAP;
		}

		draw_text(t_draw_list, font, Vec2{snapped_to_pixel(text_x), baseline}, K_HINT_LABELS[i], label);
		x += width + K_HINT_GAP;
	}
}
