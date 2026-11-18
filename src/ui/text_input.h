#pragma once

#include <optional>
#include <string_view>

#include "core/caret.h"
#include "core/text_editor.h"
#include "core/types.h"
#include "os/input.h"

class DrawList;
struct Font;

class TextInput : public TextEditor {
  public:
	auto set_value(std::string_view t_value) -> void;
	auto set_focused(bool t_focused) -> void;
	auto apply(TextEdit t_edit) -> void;

	auto on_char(u32 t_codepoint) -> void;
	auto on_key_down(os::Key t_key, os::Modifiers t_held = os::modifiers()) -> void;

	auto on_pointer_down(const Font& t_font, Rect t_field, float t_x) -> void;
	auto on_pointer_move(const Font& t_font, Rect t_field, float t_x) -> void;
	auto on_pointer_up() -> void;
	auto on_right_click(const Font& t_font, Rect t_field, float t_x) -> void;

	auto update(float t_delta_seconds) -> void;
	auto draw(DrawList* t_draw_list, const Font& t_font, Rect t_field, Color t_text_color, Color t_caret_color, std::optional<Rect> t_placeholder_box = std::nullopt) -> void;

	bool             m_focused = false;
	std::string_view m_placeholder;

  private:
	[[nodiscard]] auto index_at(const Font& t_font, Rect t_field, float t_x) const -> u32;

	auto restart_caret_blink() -> void;
	[[nodiscard]] auto caret_size(const Font& t_font, std::string_view t_shown, float t_height) const -> Vec2;
	auto draw_caret(DrawList* t_draw_list, Vec2 t_origin, Vec2 t_size, Color t_color) const -> void;

	float       m_scroll_x            = 0.0f;
	float       m_caret_blink_seconds = 0.0f;
	float       m_trail_seconds       = 0.0f;
	CaretMotion m_caret;
};
