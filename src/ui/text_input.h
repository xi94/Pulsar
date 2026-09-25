#pragma once

#include <string_view>

#include "core/types.h"

class DrawList;
class Font;

constexpr u32 text_input_capacity = 128;

enum class TextEdit : u8 {
	cut,
	copy,
	paste,
	select_all,
};

struct TextRange {
	u32 start;
	u32 end;
};

class TextInput {
  public:
	std::string_view value() const
	{
		return std::string_view{m_text, m_length};
	}

	void set_value(std::string_view t_value);
	void set_max_length(u32 t_max_length);

	bool is_focused() const
	{
		return m_focused;
	}

	void set_focused(bool t_focused)
	{
		m_focused = t_focused;
	}

	bool is_masked() const
	{
		return m_masked;
	}

	void set_masked(bool t_masked)
	{
		m_masked = t_masked;
	}

	bool is_selecting() const
	{
		return m_selecting;
	}

	bool can_apply(TextEdit t_edit) const;
	void apply(TextEdit t_edit);

	void on_char(u32 t_character);
	void on_key_down(u32 t_key);

	void on_pointer_down(const Font &t_font, Rect t_field, float t_x);
	void on_pointer_move(const Font &t_font, Rect t_field, float t_x);
	void on_pointer_up();
	void on_right_click(const Font &t_font, Rect t_field, float t_x);

	void update(float t_delta_seconds);
	void draw(DrawList &t_draw_list, const Font &t_font, Rect t_field, Color t_text_color, Color t_caret_color);

  private:
	bool has_selection() const
	{
		return m_anchor != m_cursor;
	}

	TextRange selection() const;
	std::string_view shown_text(char (&t_mask)[text_input_capacity]) const;
	u32 index_at(const Font &t_font, Rect t_field, float t_x) const;

	void move_cursor(u32 t_index, bool t_extend_selection);
	void select(TextRange t_range);
	void erase(TextRange t_range);
	void insert(std::string_view t_text);
	void restart_caret_blink();

	char m_text[text_input_capacity]{};
	u32 m_length = 0;
	u32 m_max_length = text_input_capacity;

	u32 m_cursor = 0;
	u32 m_anchor = 0;

	bool m_focused = false;
	bool m_masked = false;
	bool m_selecting = false;

	u32 m_click_count = 0;
	u64 m_last_click_ms = 0;
	float m_last_click_x = 0.0f;

	float m_scroll_x = 0.0f;
	float m_caret_blink_seconds = 0.0f;
};
