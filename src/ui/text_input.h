#pragma once

#include <optional>
#include <string_view>

#include "core/types.h"

class DrawList;
struct Font;

constexpr u32 K_TEXT_INPUT_CAPACITY       = 512;
constexpr u32 K_DEFAULT_TEXT_INPUT_LENGTH = 128;

enum class TextEdit : u8 {
	Cut,
	Copy,
	Paste,
	SelectAll,
};

struct TextRange {
	u32 start;
	u32 end;
};

class TextInput {
  public:
	[[nodiscard]] auto value() const -> std::string_view
	{
		return std::string_view{m_text, m_length};
	}

	auto set_value(std::string_view t_value) -> void;
	auto set_max_length(u32 t_max_length) -> void;

	auto set_placeholder(std::string_view t_placeholder) -> void
	{
		m_placeholder = t_placeholder;
	}

	[[nodiscard]] auto is_focused() const -> bool
	{
		return m_focused;
	}

	auto set_focused(bool t_focused) -> void
	{
		m_focused = t_focused;
	}

	[[nodiscard]] auto is_masked() const -> bool
	{
		return m_masked;
	}

	auto set_masked(bool t_masked) -> void
	{
		m_masked = t_masked;
	}

	[[nodiscard]] auto is_selecting() const -> bool
	{
		return m_selecting;
	}

	[[nodiscard]] auto can_apply(TextEdit t_edit) const -> bool;
	auto apply(TextEdit t_edit) -> void;

	auto on_char(u32 t_character) -> void;
	auto on_key_down(u32 t_key) -> void;

	auto on_pointer_down(const Font& t_font, Rect t_field, float t_x) -> void;
	auto on_pointer_move(const Font& t_font, Rect t_field, float t_x) -> void;
	auto on_pointer_up() -> void;
	auto on_right_click(const Font& t_font, Rect t_field, float t_x) -> void;

	auto update(float t_delta_seconds) -> void;
	auto
	draw(DrawList* t_draw_list, const Font& t_font, Rect t_field, Color t_text_color, Color t_caret_color, std::optional<Rect> t_placeholder_box = std::nullopt)
		-> void;

  private:
	[[nodiscard]] auto has_selection() const -> bool
	{
		return m_anchor != m_cursor;
	}

	[[nodiscard]] auto selection() const -> TextRange;
	[[nodiscard]] auto shown_text(char (&t_mask)[K_TEXT_INPUT_CAPACITY]) const -> std::string_view;
	[[nodiscard]] auto index_at(const Font& t_font, Rect t_field, float t_x) const -> u32;

	auto move_cursor(u32 t_index, bool t_extend_selection) -> void;
	auto select(TextRange t_range) -> void;
	auto erase(TextRange t_range) -> void;
	auto insert(std::string_view t_text) -> void;
	auto restart_caret_blink() -> void;

	char             m_text[K_TEXT_INPUT_CAPACITY]{};
	u32              m_length     = 0;
	u32              m_max_length = K_DEFAULT_TEXT_INPUT_LENGTH;
	std::string_view m_placeholder;

	u32 m_cursor = 0;
	u32 m_anchor = 0;

	bool m_focused        = false;
	bool m_masked         = false;
	bool m_selecting      = false;
	u16  m_high_surrogate = 0;

	u32   m_click_count   = 0;
	u64   m_last_click_ms = 0;
	float m_last_click_x  = 0.0f;

	float m_scroll_x            = 0.0f;
	float m_caret_blink_seconds = 0.0f;
};
