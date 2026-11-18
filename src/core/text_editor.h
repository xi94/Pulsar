#pragma once

#include <string_view>

#include "core/types.h"
#include "os/input.h"

constexpr u32 K_TEXT_INPUT_CAPACITY       = 512;
constexpr u32 K_DEFAULT_TEXT_INPUT_LENGTH = 128;

enum class TextEdit : u8 {
	CUT,
	COPY,
	PASTE,
	SELECT_ALL,
};

struct TextRange {
	u32 start;
	u32 end;
};

class TextEditor {
  public:
	[[nodiscard]] auto value() const -> std::string_view
	{
		return std::string_view{m_text, m_length};
	}

	[[nodiscard]] auto cursor() const -> u32
	{
		return m_cursor;
	}

	[[nodiscard]] auto has_selection() const -> bool
	{
		return m_anchor != m_cursor;
	}

	auto set_value(std::string_view t_value) -> void;
	auto set_max_length(u32 t_max_length) -> void;

	[[nodiscard]] auto selection() const -> TextRange;
	[[nodiscard]] auto shown_text(char (&t_mask)[K_TEXT_INPUT_CAPACITY]) const -> std::string_view;

	[[nodiscard]] auto can_apply(TextEdit t_edit) const -> bool;
	auto edit(TextEdit t_edit) -> bool;
	auto type(u32 t_codepoint) -> bool;
	auto press_key(os::Key t_key, os::Modifiers t_held) -> bool;

	auto press_at(u32 t_index, float t_x) -> void;
	auto drag_to(u32 t_index) -> bool;
	auto release() -> void;
	auto right_click_at(u32 t_index) -> void;

	bool m_masked    = false;
	bool m_selecting = false;

  private:
	auto move_cursor(u32 t_index, bool t_extend_selection) -> void;
	auto select(TextRange t_range) -> void;
	auto erase(TextRange t_range) -> void;
	auto insert(std::string_view t_text) -> void;

	char m_text[K_TEXT_INPUT_CAPACITY]{};
	u32  m_length     = 0;
	u32  m_max_length = K_DEFAULT_TEXT_INPUT_LENGTH;

	u32 m_cursor = 0;
	u32 m_anchor = 0;

	u32   m_click_count   = 0;
	u64   m_last_click_ms = 0;
	float m_last_click_x  = 0.0f;
};
