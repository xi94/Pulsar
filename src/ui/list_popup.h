#pragma once

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ui/scrollable.h"
#include "ui/text_input.h"

class Assets;
class DrawList;
struct Fonts;
struct Settings;

struct ListPopupOptions {
	std::string_view search_placeholder;
	std::string_view empty_message;
	float min_width = 0.0f;
	Vec2 preview_size{};
	std::function<void(DrawList &t_draw_list, Rect t_preview, u32 t_item, Color t_backdrop, u8 t_alpha)> draw_preview;
	float hover_preview_seconds = 0.0f;
};

class ListPopup {
  public:
	ListPopup(const Fonts &t_fonts, const Assets &t_assets, const Settings &t_settings, ListPopupOptions t_options);

	void open(std::span<const std::string_view> t_items, std::optional<u32> t_selected);
	void close();

	bool is_open() const
	{
		return m_open;
	}

	std::optional<u32> previewed_item() const;

	void update(float t_delta_seconds, Rect t_anchor, Rect t_bounds);

	void on_pointer_down(Vec2 t_point);
	void on_pointer_move(Vec2 t_point);
	std::optional<u32> on_pointer_up(Vec2 t_point);
	TextInput *on_right_click(Vec2 t_point);
	void on_scroll(float t_wheel_delta);
	std::optional<u32> on_key_down(u32 t_key);
	void on_char(u32 t_character);

	CursorKind cursor(Vec2 t_mouse) const;
	void draw(DrawList &t_draw_list, Vec2 t_mouse);

  private:
	enum class Press : u8 {
		None,
		Outside,
		Search,
		Clear,
		Scrollbar,
		Row,
	};

	struct Placement {
		float top;
		float bottom;
		bool opens_below;
	};

	struct Layout {
		Rect popup;
		Rect search;
		Rect separator;
		Rect list;
		Rect footer;
	};

	bool is_searchable() const
	{
		return !m_options.search_placeholder.empty();
	}

	float popup_width() const;
	u32 hint_count() const;
	float hint_width(u32 t_hint) const;
	u32 hint_lines() const;
	float footer_height() const;
	float chrome_height() const;
	Placement placement() const;
	u32 row_capacity() const;
	u32 shown_row_target() const;

	Layout layout() const;
	ScrollGeometry list_scroll(const Layout &t_layout) const;
	Rect search_field_rect(const Layout &t_layout) const;
	Rect clear_button_rect(const Layout &t_layout) const;
	Rect row_rect(const Layout &t_layout, u32 t_match) const;
	std::optional<u32> match_at(const Layout &t_layout, Vec2 t_point) const;

	void rebuild_matches();
	void refresh_matches();
	void move_highlight(i32 t_rows);
	std::optional<u32> choose_highlighted();

	void draw_search(DrawList &t_draw_list, const Layout &t_layout, Vec2 t_mouse, u8 t_alpha);
	void draw_rows(DrawList &t_draw_list, const Layout &t_layout, Vec2 t_mouse, u8 t_alpha) const;
	void draw_row(DrawList &t_draw_list, Rect t_row, u32 t_item, bool t_highlighted, u8 t_alpha) const;
	void draw_label(DrawList &t_draw_list, Rect t_row, float t_left, float t_max_width, std::string_view t_label,
					u8 t_alpha) const;
	void draw_key_hints(DrawList &t_draw_list, const Layout &t_layout, u8 t_alpha) const;

	const Fonts &m_fonts;
	const Assets &m_assets;
	const Settings &m_settings;
	ListPopupOptions m_options;

	std::span<const std::string_view> m_items;
	std::optional<u32> m_selected;
	std::vector<u32> m_matches;
	std::string m_matched_query;
	u32 m_highlighted = 0;
	std::optional<u32> m_previewed_item;
	float m_hover_seconds = 0.0f;

	TextInput m_search;
	Scrollable m_scroll;
	Press m_press = Press::None;
	std::optional<u32> m_pressed_match;

	bool m_open = false;
	float m_open_amount = 0.0f;
	float m_shown_rows = 1.0f;
	Rect m_anchor{};
	Rect m_bounds{};
};
