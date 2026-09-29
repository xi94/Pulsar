#pragma once

#include <optional>
#include <span>
#include <string_view>

#include "core/types.h"

class DrawList;
class Font;

float text_width(const Font &t_font, std::string_view t_text);
u32 text_index_at(const Font &t_font, std::string_view t_text, float t_x);

void draw_text(DrawList &t_draw_list, const Font &t_font, Vec2 t_baseline, std::string_view t_text, Color t_color);
void draw_text_centered(DrawList &t_draw_list, const Font &t_font, Rect t_box, std::string_view t_text, Color t_color);
void draw_text_truncated(DrawList &t_draw_list, const Font &t_font, Vec2 t_baseline, std::string_view t_text,
						 float t_max_width, Color t_color);

struct TruncatedText {
	Rect bounds;
	std::string_view text;
};

void begin_truncation_probe(DrawList &t_draw_list, Vec2 t_point);
std::optional<TruncatedText> hovered_truncated_text(const DrawList &t_draw_list);

u32 wrap_text(const Font &t_font, std::string_view t_text, float t_max_width, std::span<std::string_view> t_out_lines);
float draw_wrapped_text(DrawList &t_draw_list, const Font &t_font, Vec2 t_first_baseline, float t_max_width,
						std::string_view t_text, Color t_color, u32 t_max_lines);
