#pragma once

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "os/input.h"
#include "ui/scrollable.h"
#include "ui/text_input.h"

class Assets;
class DrawList;
struct Fonts;
struct Settings;

using PreviewDrawer = std::function<void(DrawList* t_draw_list, Rect t_preview, u32 t_item, Color t_backdrop, u8 t_alpha)>;

struct ListPopupOptions {
	std::string_view search_placeholder;
	std::string_view empty_message;
	float            min_width = 0.0f;
	Vec2             preview_size{};
	PreviewDrawer    draw_preview;
	float            hover_preview_seconds = 0.0f;
};

class ListPopup {
  public:
	ListPopup(const Fonts* t_fonts, const Assets* t_assets, const Settings* t_settings, ListPopupOptions t_options);

	auto open(std::span<const std::string_view> t_items, std::optional<u32> t_selected) -> void;
	auto close() -> void;

	[[nodiscard]] auto is_open() const -> bool
	{
		return m_open;
	}

	[[nodiscard]] auto previewed_item() const -> std::optional<u32>;

	auto update(float t_delta_seconds, Rect t_anchor, Rect t_bounds) -> void;

	auto on_pointer_down(Vec2 t_point) -> void;
	auto on_pointer_move(Vec2 t_point) -> void;
	[[nodiscard]] auto on_pointer_up(Vec2 t_point) -> std::optional<u32>;
	[[nodiscard]] auto on_right_click(Vec2 t_point) -> TextInput*;
	auto on_scroll(float t_wheel_delta) -> void;
	[[nodiscard]] auto on_key_down(os::Key t_key) -> std::optional<u32>;
	auto on_char(u32 t_character) -> void;

	[[nodiscard]] auto cursor(Vec2 t_mouse) const -> CursorKind;
	auto draw(DrawList* t_draw_list, Vec2 t_mouse) -> void;

  private:
	enum class Press : u8 {
		NONE,
		OUTSIDE,
		SEARCH,
		CLEAR,
		SCROLLBAR,
		ROW,
	};

	struct Placement {
		float top;
		float bottom;
		bool  opens_below;
	};

	struct Layout {
		Rect popup;
		Rect search;
		Rect separator;
		Rect list;
		Rect footer;
	};

	[[nodiscard]] auto is_searchable() const -> bool
	{
		return !m_options.search_placeholder.empty();
	}

	[[nodiscard]] auto popup_width() const -> float;
	[[nodiscard]] auto hint_count() const -> u32;
	[[nodiscard]] auto hint_width(u32 t_hint) const -> float;
	[[nodiscard]] auto hint_lines() const -> u32;
	[[nodiscard]] auto footer_height() const -> float;
	[[nodiscard]] auto chrome_height() const -> float;
	[[nodiscard]] auto placement() const -> Placement;
	[[nodiscard]] auto row_capacity() const -> u32;
	[[nodiscard]] auto shown_row_target() const -> u32;

	[[nodiscard]] auto layout() const -> Layout;
	[[nodiscard]] auto list_scroll(const Layout& t_layout) const -> ScrollGeometry;
	[[nodiscard]] auto search_field_rect(const Layout& t_layout) const -> Rect;
	[[nodiscard]] auto clear_button_rect(const Layout& t_layout) const -> Rect;
	[[nodiscard]] auto row_rect(const Layout& t_layout, u32 t_match) const -> Rect;
	[[nodiscard]] auto match_at(const Layout& t_layout, Vec2 t_point) const -> std::optional<u32>;

	auto rebuild_matches() -> void;
	auto refresh_matches() -> void;
	auto move_highlight(i32 t_rows) -> void;
	[[nodiscard]] auto choose_highlighted() -> std::optional<u32>;

	auto draw_search(DrawList* t_draw_list, const Layout& t_layout, Vec2 t_mouse, u8 t_alpha) -> void;
	auto draw_rows(DrawList* t_draw_list, const Layout& t_layout, Vec2 t_mouse, u8 t_alpha) const -> void;
	auto draw_row(DrawList* t_draw_list, Rect t_row, u32 t_item, bool t_highlighted, u8 t_alpha) const -> void;
	auto draw_label(DrawList* t_draw_list, Rect t_row, float t_left, float t_max_width, std::string_view t_label, u8 t_alpha) const -> void;
	auto draw_key_hints(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) const -> void;

	const Fonts*     m_fonts;
	const Assets*    m_assets;
	const Settings*  m_settings;
	ListPopupOptions m_options;

	std::span<const std::string_view> m_items;
	std::optional<u32>                m_selected;
	std::vector<u32>                  m_matches;
	std::string                       m_matched_query;
	u32                               m_highlighted = 0;
	std::optional<u32>                m_previewed_item;
	float                             m_hover_seconds = 0.0f;

	TextInput          m_search;
	Scrollable         m_scroll;
	Press              m_press = Press::NONE;
	std::optional<u32> m_pressed_match;

	bool  m_open        = false;
	float m_open_amount = 0.0f;
	float m_shown_rows  = 1.0f;
	Rect  m_anchor{};
	Rect  m_bounds{};
};
