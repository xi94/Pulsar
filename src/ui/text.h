#pragma once

#include <optional>
#include <span>
#include <string_view>

#include "core/types.h"

class DrawList;
struct Font;

[[nodiscard]] auto text_width(const Font& t_font, std::string_view t_text) -> float;
[[nodiscard]] auto text_index_at(const Font& t_font, std::string_view t_text, float t_x) -> u32;

auto draw_text(DrawList* t_draw_list, const Font& t_font, Vec2 t_baseline, std::string_view t_text, Color t_color) -> void;
auto draw_text_centered(DrawList* t_draw_list, const Font& t_font, Rect t_box, std::string_view t_text, Color t_color) -> void;
auto draw_text_truncated(DrawList* t_draw_list, const Font& t_font, Vec2 t_baseline, std::string_view t_text, float t_max_width, Color t_color) -> void;

struct TruncatedText {
	Rect             bounds;
	std::string_view text;
};

auto begin_truncation_probe(DrawList* t_draw_list, Vec2 t_point) -> void;
[[nodiscard]] auto hovered_truncated_text(const DrawList* t_draw_list) -> std::optional<TruncatedText>;

[[nodiscard]] auto wrap_text(const Font& t_font, std::string_view t_text, float t_max_width, std::span<std::string_view> t_out_lines) -> u32;
auto draw_wrapped_text(DrawList*        t_draw_list,
                       const Font&      t_font,
                       Vec2             t_first_baseline,
                       float            t_max_width,
                       std::string_view t_text,
                       Color            t_color,
                       u32              t_max_lines) -> float;
