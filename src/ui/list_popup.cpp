#include "ui/list_popup.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include <Windows.h>

#include "core/animation.h"
#include "core/settings.h"
#include "core/str.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "ui/controls.h"
#include "ui/text.h"
#include "ui/theme.h"

namespace {
constexpr float popup_radius = 10.0f;
constexpr float anchor_gap = 6.0f;
constexpr float bounds_margin = 10.0f;
constexpr float list_padding = 6.0f;
constexpr float search_field_margin = 6.0f;
constexpr float search_field_radius = 7.0f;
constexpr float search_icon_inset = 9.0f;
constexpr float search_icon_size = 14.0f;
constexpr float search_icon_gap = 7.0f;
constexpr float clear_size = 20.0f;
constexpr float clear_margin = 4.0f;
constexpr float row_inset = 10.0f;
constexpr float row_radius = 6.0f;
constexpr float row_gap = 10.0f;
constexpr float check_size = 14.0f;
constexpr float scrollbar_gap = 4.0f;
constexpr float footer_padding_x = 10.0f;
constexpr float footer_padding_y = 5.0f;
constexpr float hint_gap = 14.0f;
constexpr float hint_key_gap = 5.0f;
constexpr float hint_arrow_gap = 3.0f;
constexpr float hint_line_gap = 4.0f;
constexpr Vec2 hint_arrow_size{7.0f, 4.0f};
constexpr std::string_view hint_keys[]{"", "Enter", "Esc", ""};
constexpr std::string_view hint_labels[]{"navigate", "select", "close", "preview"};
constexpr u32 hover_hint = 3;
constexpr float match_contrast = 0.3f;
constexpr u32 max_visible_rows = 8;

constexpr float open_ease_rate = 22.0f;
constexpr float resize_ease_rate = 20.0f;
constexpr float slide_distance = 6.0f;

float search_height(const Fonts &t_fonts)
{
	return std::max(40.0f, t_fonts.body().line_height() + 16.0f);
}

float row_height(const Fonts &t_fonts)
{
	return std::max(30.0f, t_fonts.body().line_height() + 10.0f);
}
}

ListPopup::ListPopup(const Fonts &t_fonts, const Assets &t_assets, const Settings &t_settings,
					 ListPopupOptions t_options)
	: m_fonts(t_fonts)
	, m_assets(t_assets)
	, m_settings(t_settings)
	, m_options(std::move(t_options))
{
	m_search.set_placeholder(m_options.search_placeholder);
}

void ListPopup::open(std::span<const std::string_view> t_items, std::optional<u32> t_selected)
{
	m_items = t_items;
	m_selected = t_selected;
	m_search.set_value("");
	m_search.set_focused(is_searchable());
	rebuild_matches();

	const u32 last = m_matches.empty() ? 0 : static_cast<u32>(m_matches.size()) - 1;
	m_highlighted = std::min(t_selected.value_or(0), last);
	m_previewed_item = t_selected;
	m_hover_seconds = 0.0f;
	m_shown_rows = static_cast<float>(shown_row_target());
	m_press = Press::none;
	m_open = true;

	const Layout current = layout();
	const float highlighted_center = (m_highlighted + 0.5f) * row_height(m_fonts);
	m_scroll.jump_to(highlighted_center - current.list.h * 0.5f, list_scroll(current));
}

void ListPopup::close()
{
	m_open = false;
	m_press = Press::none;
	m_search.set_focused(false);
	m_search.on_pointer_up();
	m_scroll.on_pointer_up();
}

std::optional<u32> ListPopup::previewed_item() const
{
	return m_open ? m_previewed_item : std::nullopt;
}

float ListPopup::popup_width() const
{
	const float width = std::max(m_options.min_width, m_anchor.w);
	if (!is_searchable()) return width;

	float hints = footer_padding_x * 2.0f + 2.0f;
	for (u32 i = 0; i < hint_count(); i += 1) {
		hints += hint_width(i) + (i > 0 ? hint_gap : 0.0f);
	}

	return std::max(width, std::min(std::ceil(hints) + 1.0f, m_bounds.w - bounds_margin * 2.0f));
}

u32 ListPopup::hint_count() const
{
	return m_options.hover_preview_seconds > 0.0f ? 4 : 3;
}

float ListPopup::hint_width(u32 t_hint) const
{
	const Font &font = m_fonts.secondary();
	float keys = 0.0f;

	if (t_hint == 0) {
		keys = controls::keycap_height(font) * 2.0f + hint_arrow_gap + hint_key_gap;
	} else if (t_hint == hover_hint) {
		keys = controls::keycap_height(font) + hint_key_gap;
	} else if (!hint_keys[t_hint].empty()) {
		keys = controls::keycap_width(font, hint_keys[t_hint]) + hint_key_gap;
	}

	return keys + text_width(font, hint_labels[t_hint]);
}

u32 ListPopup::hint_lines() const
{
	const float room = popup_width() - 2.0f - footer_padding_x * 2.0f;
	u32 lines = 1;
	float x = 0.0f;

	for (u32 i = 0; i < hint_count(); i += 1) {
		const float width = hint_width(i);

		if (x > 0.0f && x + width > room) {
			lines += 1;
			x = 0.0f;
		}

		x += width + hint_gap;
	}

	return lines;
}

float ListPopup::footer_height() const
{
	const auto lines = static_cast<float>(hint_lines());

	return lines * controls::keycap_height(m_fonts.secondary()) + (lines - 1.0f) * hint_line_gap +
		   footer_padding_y * 2.0f;
}

float ListPopup::chrome_height() const
{
	const float search = is_searchable() ? search_height(m_fonts) + 1.0f : 0.0f;
	const float footer = is_searchable() ? footer_height() + 2.0f : 0.0f;

	return search + footer + list_padding * 2.0f;
}

ListPopup::Placement ListPopup::placement() const
{
	const Placement below{m_anchor.bottom() + anchor_gap, m_bounds.bottom() - bounds_margin, true};
	const Placement above{m_bounds.y + bounds_margin, m_anchor.y - anchor_gap, false};

	const float room_below = below.bottom - below.top;
	const float room_above = above.bottom - above.top;
	const float tallest = chrome_height() + max_visible_rows * row_height(m_fonts);

	return room_below >= tallest || room_below >= room_above ? below : above;
}

u32 ListPopup::row_capacity() const
{
	const Placement space = placement();
	const float rows = (space.bottom - space.top - chrome_height()) / row_height(m_fonts);

	return std::clamp(static_cast<u32>(std::max(rows, 0.0f)), 1u, max_visible_rows);
}

u32 ListPopup::shown_row_target() const
{
	return std::clamp(static_cast<u32>(m_matches.size()), 1u, row_capacity());
}

ListPopup::Layout ListPopup::layout() const
{
	const Placement space = placement();
	const float width = popup_width();
	const float height = chrome_height() + m_shown_rows * row_height(m_fonts);
	const float slide = (1.0f - m_open_amount) * slide_distance;
	const float y = space.opens_below ? space.top - slide : space.bottom - height + slide;

	const float min_x = m_bounds.x + bounds_margin;
	const float max_x = std::max(min_x, m_bounds.right() - bounds_margin - width);

	Layout result{};
	result.popup = Rect{std::clamp(m_anchor.right() - width, min_x, max_x),
						snapped_to_pixel(std::max(y, m_bounds.y + bounds_margin)), width, height};

	Rect remaining = result.popup;
	if (is_searchable()) {
		result.search = remaining.split_top(search_height(m_fonts));
		result.separator = remaining.split_top(1.0f).inset(1.0f, 0.0f);
		result.footer = remaining.split_bottom(footer_height() + 1.0f).inset(1.0f, 0.0f);
		result.footer.h -= 1.0f;
		remaining.split_bottom(1.0f);
	}

	result.list = remaining.inset(list_padding);

	return result;
}

ScrollGeometry ListPopup::list_scroll(const Layout &t_layout) const
{
	const Rect &list = t_layout.list;
	const Rect track{list.right() - scrollbar_width, list.y, scrollbar_width, list.h};

	return ScrollGeometry{track, static_cast<float>(m_matches.size()) * row_height(m_fonts), list.h};
}

Rect ListPopup::search_field_rect(const Layout &t_layout) const
{
	const float left = t_layout.search.x + search_field_margin + search_icon_inset + search_icon_size + search_icon_gap;
	const float right = clear_button_rect(t_layout).x - search_icon_gap;

	return Rect{left, t_layout.search.y, std::max(0.0f, right - left), t_layout.search.h};
}

Rect ListPopup::clear_button_rect(const Layout &t_layout) const
{
	const Rect &search = t_layout.search;

	return Rect{search.right() - search_field_margin - clear_margin - clear_size, search.center().y - clear_size * 0.5f,
				clear_size, clear_size};
}

Rect ListPopup::row_rect(const Layout &t_layout, u32 t_match) const
{
	const float height = row_height(m_fonts);
	const bool scrolls = Scrollable::is_needed(list_scroll(t_layout));
	const float width = t_layout.list.w - (scrolls ? scrollbar_width + scrollbar_gap : 0.0f);

	return Rect{t_layout.list.x, t_layout.list.y + t_match * height - m_scroll.offset(), width, height};
}

std::optional<u32> ListPopup::match_at(const Layout &t_layout, Vec2 t_point) const
{
	if (!t_layout.list.contains(t_point)) return std::nullopt;

	const float row = (t_point.y - t_layout.list.y + m_scroll.offset()) / row_height(m_fonts);
	const auto match = static_cast<u32>(std::max(0.0f, row));
	if (match >= m_matches.size() || !row_rect(t_layout, match).contains(t_point)) return std::nullopt;

	return match;
}

void ListPopup::rebuild_matches()
{
	const std::string_view query = m_search.value();
	m_matched_query = query;
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

void ListPopup::refresh_matches()
{
	if (m_search.value() == m_matched_query) return;

	rebuild_matches();
	m_highlighted = 0;
	m_scroll.jump_to(0.0f, list_scroll(layout()));
}

void ListPopup::move_highlight(i32 t_rows)
{
	if (m_matches.empty()) return;

	const i32 last = static_cast<i32>(m_matches.size()) - 1;
	m_highlighted = static_cast<u32>(std::clamp(static_cast<i32>(m_highlighted) + t_rows, 0, last));
	m_previewed_item = m_matches[m_highlighted];

	const Layout current = layout();
	const Rect row = row_rect(current, m_highlighted);
	m_scroll.reveal(row.y, row.bottom(), current.list.y, current.list.bottom(), list_scroll(current));
}

std::optional<u32> ListPopup::choose_highlighted()
{
	if (m_highlighted >= m_matches.size()) return std::nullopt;

	const u32 chosen = m_matches[m_highlighted];
	close();

	return chosen;
}

void ListPopup::update(float t_delta_seconds, Rect t_anchor, Rect t_bounds)
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

	m_open_amount = animation::ease_toward(m_open_amount, m_open ? 1.0f : 0.0f, open_ease_rate, t_delta_seconds);
	m_shown_rows = animation::ease_toward(m_shown_rows, static_cast<float>(shown_row_target()), resize_ease_rate,
										  t_delta_seconds, animation::settled_pixels / row_height(m_fonts));
	m_scroll.update(t_delta_seconds);
	m_search.update(t_delta_seconds);
}

void ListPopup::on_pointer_down(Vec2 t_point)
{
	if (!m_open) return;

	const Layout current = layout();
	m_pressed_match.reset();

	if (!current.popup.contains(t_point)) {
		m_press = Press::outside;
	} else if (m_scroll.on_pointer_down(t_point, list_scroll(current))) {
		m_press = Press::scrollbar;
	} else if (!m_search.value().empty() && clear_button_rect(current).contains(t_point)) {
		m_press = Press::clear;
	} else if (current.search.contains(t_point)) {
		m_press = Press::search;
		m_search.on_pointer_down(m_fonts.body(), search_field_rect(current), t_point.x);
	} else {
		m_press = Press::row;
		m_pressed_match = match_at(current, t_point);
	}
}

void ListPopup::on_pointer_move(Vec2 t_point)
{
	if (!m_open) return;

	const Layout current = layout();
	m_scroll.on_pointer_move(t_point.y, list_scroll(current));

	if (m_search.is_selecting()) {
		m_search.on_pointer_move(m_fonts.body(), search_field_rect(current), t_point.x);
	}

	if (m_press == Press::none || m_press == Press::row) {
		const std::optional<u32> hovered = match_at(current, t_point);
		if (hovered && *hovered != m_highlighted) {
			m_highlighted = *hovered;
			m_hover_seconds = 0.0f;
		}
	}
}

std::optional<u32> ListPopup::on_pointer_up(Vec2 t_point)
{
	const Press press = std::exchange(m_press, Press::none);
	m_scroll.on_pointer_up();
	m_search.on_pointer_up();

	if (!m_open) return std::nullopt;

	const Layout current = layout();

	switch (press) {
		case Press::outside:
			if (!current.popup.contains(t_point)) {
				close();
			}
			break;

		case Press::clear:
			if (clear_button_rect(current).contains(t_point)) {
				m_search.set_value("");
				refresh_matches();
			}
			break;

		case Press::row:
			if (m_pressed_match && match_at(current, t_point) == m_pressed_match) {
				m_highlighted = *m_pressed_match;
				return choose_highlighted();
			}
			break;

		case Press::none:
		case Press::search:
		case Press::scrollbar:
			break;
	}

	return std::nullopt;
}

TextInput *ListPopup::on_right_click(Vec2 t_point)
{
	if (!m_open || !is_searchable()) return nullptr;

	const Layout current = layout();
	if (!current.search.contains(t_point)) return nullptr;

	m_search.on_right_click(m_fonts.body(), search_field_rect(current), t_point.x);

	return &m_search;
}

void ListPopup::on_scroll(float t_wheel_delta)
{
	if (!m_open) return;

	m_scroll.on_scroll(t_wheel_delta, list_scroll(layout()));
}

std::optional<u32> ListPopup::on_key_down(u32 t_key)
{
	if (!m_open) return std::nullopt;

	const auto page = static_cast<i32>(row_capacity()) - 1;
	refresh_matches();

	switch (t_key) {
		case VK_ESCAPE:
			close();
			break;

		case VK_RETURN:
			return choose_highlighted();

		case VK_UP:
			move_highlight(-1);
			break;

		case VK_DOWN:
			move_highlight(1);
			break;

		case VK_PRIOR:
			move_highlight(-page);
			break;

		case VK_NEXT:
			move_highlight(page);
			break;

		default:
			m_search.on_key_down(t_key);
			refresh_matches();
			break;
	}

	return std::nullopt;
}

void ListPopup::on_char(u32 t_character)
{
	if (!m_open) return;

	m_search.on_char(t_character);
	refresh_matches();
}

CursorKind ListPopup::cursor(Vec2 t_mouse) const
{
	if (!m_open) return CursorKind::arrow;
	if (m_scroll.is_dragging()) return CursorKind::drag;
	if (m_search.is_selecting()) return CursorKind::ibeam;

	const Layout current = layout();
	if (!current.popup.contains(t_mouse)) return CursorKind::arrow;
	if (!m_search.value().empty() && clear_button_rect(current).contains(t_mouse)) return CursorKind::hand;
	if (current.search.contains(t_mouse)) return CursorKind::ibeam;
	if (m_scroll.is_over_track(t_mouse, list_scroll(current))) return CursorKind::hand;

	return match_at(current, t_mouse) ? CursorKind::hand : CursorKind::arrow;
}

void ListPopup::draw_search(DrawList &t_draw_list, const Layout &t_layout, Vec2 t_mouse, u8 t_alpha)
{
	const Theme &colors = theme();
	const Rect &search = t_layout.search;
	const bool has_query = !m_search.value().empty();
	const Rect field = search.inset(search_field_margin);
	const Rect icon{field.x + search_icon_inset, search.center().y - search_icon_size * 0.5f, search_icon_size,
					search_icon_size};

	t_draw_list.add_bordered_rect(field, rounded(search_field_radius), faded(colors.field, t_alpha),
								  faded(colors.separator, t_alpha), 1.0f);
	controls::draw_magnifier(t_draw_list, icon, faded(has_query ? colors.text_dim : colors.text_faint, t_alpha));
	m_search.draw(t_draw_list, m_fonts.body(), search_field_rect(t_layout), faded(colors.text, t_alpha),
				  faded(m_settings.accent, t_alpha), search_field_rect(t_layout));

	if (!has_query) return;

	const Rect clear = clear_button_rect(t_layout);
	const bool hovered = clear.contains(t_mouse);

	if (hovered) {
		t_draw_list.add_rounded_rect(clear, rounded(clear.w * 0.5f), faded(colors.control_hover, t_alpha));
	}

	controls::draw_x(t_draw_list, clear.inset(2.0f), faded(hovered ? colors.text : colors.text_faint, t_alpha));
}

void ListPopup::draw_label(DrawList &t_draw_list, Rect t_row, float t_left, float t_max_width, std::string_view t_label,
						   u8 t_alpha) const
{
	const Font &font = m_fonts.body();
	const Vec2 baseline{t_left, font.centered_baseline(t_row)};
	const Color text = faded(theme().text, t_alpha);
	const usize found = find_ignoring_case(t_label, m_matched_query);

	if (m_matched_query.empty() || found == std::string_view::npos || text_width(font, t_label) > t_max_width) {
		draw_text_truncated(t_draw_list, font, baseline, t_label, t_max_width, text);
		return;
	}

	const std::string_view before = t_label.substr(0, found);
	const std::string_view matched = t_label.substr(found, m_matched_query.size());
	const std::string_view after = t_label.substr(found + matched.size());
	const float matched_x = baseline.x + text_width(font, before);
	const float after_x = matched_x + text_width(font, matched);
	const Color match = faded(mix(m_settings.accent, theme().text, match_contrast), t_alpha);

	draw_text(t_draw_list, font, baseline, before, text);
	draw_text(t_draw_list, font, Vec2{matched_x, baseline.y}, matched, match);
	draw_text(t_draw_list, font, Vec2{after_x, baseline.y}, after, text);
}

void ListPopup::draw_row(DrawList &t_draw_list, Rect t_row, u32 t_item, bool t_highlighted, u8 t_alpha) const
{
	const Color backdrop = t_highlighted ? hovered(theme().popup) : theme().popup;

	if (t_highlighted) {
		t_draw_list.add_rounded_rect(t_row, rounded(row_radius), faded(backdrop, t_alpha));
	}

	float left = t_row.x + row_inset;

	if (m_options.draw_preview) {
		const Vec2 size = m_options.preview_size;
		const Rect preview{left, t_row.center().y - size.y * 0.5f, size.x, size.y};

		m_options.draw_preview(t_draw_list, preview, t_item, backdrop, t_alpha);
		left = preview.right() + row_gap;
	}

	const Rect check{t_row.right() - row_inset - check_size, t_row.center().y - check_size * 0.5f, check_size,
					 check_size};
	if (m_selected == t_item) {
		controls::draw_check(t_draw_list, m_assets, check, faded(m_settings.accent, t_alpha));
	}

	draw_label(t_draw_list, t_row, left, check.x - row_gap - left, m_items[t_item], t_alpha);
}

void ListPopup::draw_rows(DrawList &t_draw_list, const Layout &t_layout, Vec2 t_mouse, u8 t_alpha) const
{
	if (m_matches.empty()) {
		draw_text_centered(t_draw_list, m_fonts.secondary(), t_layout.list, m_options.empty_message,
						   faded(theme().text_faint, t_alpha));
		return;
	}

	const float height = row_height(m_fonts);
	const auto first = static_cast<u32>(std::max(0.0f, m_scroll.offset() / height));
	const u32 last = std::min(static_cast<u32>(m_matches.size()),
							  static_cast<u32>((m_scroll.offset() + t_layout.list.h) / height) + 1);

	t_draw_list.push_clip(t_layout.list);

	for (u32 match = first; match < last; match += 1) {
		draw_row(t_draw_list, row_rect(t_layout, match), m_matches[match], match == m_highlighted, t_alpha);
	}

	t_draw_list.pop_clip();

	m_scroll.draw_edge_fade(t_draw_list, t_layout.list, list_scroll(t_layout), faded(theme().popup, t_alpha));
	m_scroll.draw(t_draw_list, list_scroll(t_layout), t_mouse, t_alpha);
}

void ListPopup::draw(DrawList &t_draw_list, Vec2 t_mouse)
{
	if (m_open_amount <= 0.01f) return;

	const auto alpha = static_cast<u8>(255.0f * m_open_amount);
	const Layout current = layout();
	const Theme &colors = theme();

	controls::draw_popup_shadow(t_draw_list, current.popup, popup_radius, m_open_amount);
	t_draw_list.add_bordered_rect(current.popup, rounded(popup_radius), faded(colors.popup, alpha),
								  faded(colors.border, alpha), 1.0f);

	if (is_searchable()) {
		draw_search(t_draw_list, current, t_mouse, alpha);
	}

	draw_rows(t_draw_list, current, t_mouse, alpha);

	if (is_searchable()) {
		draw_key_hints(t_draw_list, current, alpha);
	}
}

void ListPopup::draw_key_hints(DrawList &t_draw_list, const Layout &t_layout, u8 t_alpha) const
{
	const Theme &colors = theme();
	const Font &font = m_fonts.secondary();
	const Rect &footer = t_layout.footer;
	const float radius = popup_radius - 1.0f;
	const float cap = controls::keycap_height(font);
	const float start = footer.x + footer_padding_x;
	const float right = footer.right() - footer_padding_x;
	const Color label = faded(colors.text_faint, t_alpha);
	float x = start;
	float y = footer.y + footer_padding_y;

	t_draw_list.add_rect(Rect{footer.x, footer.y - 1.0f, footer.w, 1.0f}, faded(colors.separator, t_alpha));
	t_draw_list.add_rounded_rect(footer, rounded(0.0f, 0.0f, radius, radius), faded(colors.field, t_alpha));

	for (u32 i = 0; i < hint_count(); i += 1) {
		const float width = hint_width(i);

		if (x > start && x + width > right) {
			x = start;
			y += cap + hint_line_gap;
		}

		const float baseline = font.centered_baseline(Rect{x, y, width, cap});
		float text_x = x;

		if (i == 0) {
			for (const bool up : {true, false}) {
				const Rect key{snapped_to_pixel(text_x), snapped_to_pixel(y), cap, cap};
				const Rect chevron{key.center().x - hint_arrow_size.x * 0.5f,
								   key.center().y - hint_arrow_size.y * 0.5f - 0.5f, hint_arrow_size.x,
								   hint_arrow_size.y};

				controls::draw_keycap_frame(t_draw_list, key, colors.field, t_alpha);
				controls::draw_chevron(t_draw_list, chevron, up, faded(controls::keycap_label_color(), t_alpha));
				text_x += cap + hint_arrow_gap;
			}

			text_x += hint_key_gap - hint_arrow_gap;
		} else if (i == hover_hint) {
			controls::draw_mouse_keycap(t_draw_list, Rect{snapped_to_pixel(text_x), snapped_to_pixel(y), cap, cap},
										colors.field, t_alpha);
			text_x += cap + hint_key_gap;
		} else if (!hint_keys[i].empty()) {
			const float key_width = controls::keycap_width(font, hint_keys[i]);

			controls::draw_keycap(t_draw_list, font,
								  Rect{snapped_to_pixel(text_x), snapped_to_pixel(y), key_width, cap}, hint_keys[i],
								  colors.field, t_alpha);
			text_x += key_width + hint_key_gap;
		}

		draw_text(t_draw_list, font, Vec2{snapped_to_pixel(text_x), baseline}, hint_labels[i], label);
		x += width + hint_gap;
	}
}
