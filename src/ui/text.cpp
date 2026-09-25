#include "ui/text.h"

#include <algorithm>
#include <cmath>

#include "gfx/draw_list.h"
#include "gfx/font.h"

namespace {
constexpr std::string_view ellipsis = "...";

bool has_glyph(char t_character)
{
	const auto code = static_cast<unsigned char>(t_character);

	return code >= Font::first_char && code < Font::first_char + Font::char_count;
}

stbtt_aligned_quad advance_glyph(const Font &t_font, char t_character, float &t_pen_x, float &t_pen_y)
{
	const auto atlas_size = static_cast<int>(t_font.atlas_size());
	const int glyph = static_cast<unsigned char>(t_character) - static_cast<int>(Font::first_char);

	stbtt_aligned_quad quad;
	stbtt_GetPackedQuad(t_font.packed_chars(), atlas_size, atlas_size, glyph, &t_pen_x, &t_pen_y, &quad, 1);

	return quad;
}

bool is_space(char t_character)
{
	return t_character == ' ';
}

usize skip_spaces(std::string_view t_text, usize t_index)
{
	while (t_index < t_text.size() && is_space(t_text[t_index])) {
		t_index += 1;
	}

	return t_index;
}

usize skip_word(std::string_view t_text, usize t_index)
{
	while (t_index < t_text.size() && !is_space(t_text[t_index])) {
		t_index += 1;
	}

	return t_index;
}

u32 wrap_paragraph(const Font &t_font, std::string_view t_paragraph, float t_max_width,
				   std::span<std::string_view> t_out_lines)
{
	if (t_out_lines.empty()) return 0;

	if (t_paragraph.empty()) {
		t_out_lines[0] = t_paragraph;
		return 1;
	}

	u32 line_count = 0;
	usize line_start = 0;

	while (line_start < t_paragraph.size() && line_count < t_out_lines.size()) {
		usize line_end = line_start;
		usize scan = line_start;

		for (;;) {
			const usize word_start = skip_spaces(t_paragraph, scan);
			const usize word_end = skip_word(t_paragraph, word_start);
			if (word_start == word_end) break;

			const bool fits = text_width(t_font, t_paragraph.substr(line_start, word_end - line_start)) <= t_max_width;
			if (!fits && line_end > line_start) break;

			line_end = word_end;
			scan = word_end;
		}

		if (line_end == line_start) {
			line_end = std::max(skip_word(t_paragraph, line_start), line_start + 1);
		}

		t_out_lines[line_count] = t_paragraph.substr(line_start, line_end - line_start);
		line_count += 1;
		line_start = skip_spaces(t_paragraph, line_end);
	}

	return line_count;
}
}

float text_width(const Font &t_font, std::string_view t_text)
{
	float pen_x = 0.0f;
	float pen_y = 0.0f;

	for (const char character : t_text) {
		if (has_glyph(character)) {
			advance_glyph(t_font, character, pen_x, pen_y);
		}
	}

	return pen_x / t_font.bake_scale();
}

u32 text_index_at(const Font &t_font, std::string_view t_text, float t_x)
{
	const float baked_x = t_x * t_font.bake_scale();
	float pen_x = 0.0f;
	float pen_y = 0.0f;

	for (u32 i = 0; i < t_text.size(); i += 1) {
		if (!has_glyph(t_text[i])) continue;

		const float glyph_start = pen_x;
		advance_glyph(t_font, t_text[i], pen_x, pen_y);

		if (baked_x < (glyph_start + pen_x) * 0.5f) return i;
	}

	return static_cast<u32>(t_text.size());
}

void draw_text(DrawList &t_draw_list, const Font &t_font, Vec2 t_baseline, std::string_view t_text, Color t_color)
{
	if (t_font.atlas() == nullptr) return;

	// Whole pixels: a sub-pixel baseline makes the bilinear-sampled glyph edges shimmer as animations settle.
	const float bake_scale = t_font.bake_scale();
	float pen_x = std::round(t_baseline.x) * bake_scale;
	float pen_y = std::round(t_baseline.y) * bake_scale;

	for (const char character : t_text) {
		if (!has_glyph(character)) continue;

		const stbtt_aligned_quad quad = advance_glyph(t_font, character, pen_x, pen_y);
		const Rect glyph{quad.x0 / bake_scale, quad.y0 / bake_scale, (quad.x1 - quad.x0) / bake_scale,
						 (quad.y1 - quad.y0) / bake_scale};

		t_draw_list.add_image(glyph, t_font.atlas(), t_color, square_corners,
							  UvRect{quad.s0, quad.t0, quad.s1, quad.t1});
	}
}

void draw_text_centered(DrawList &t_draw_list, const Font &t_font, Rect t_box, std::string_view t_text, Color t_color)
{
	const float x = t_box.x + (t_box.w - text_width(t_font, t_text)) * 0.5f;

	draw_text(t_draw_list, t_font, Vec2{x, t_font.centered_baseline(t_box)}, t_text, t_color);
}

void draw_text_truncated(DrawList &t_draw_list, const Font &t_font, Vec2 t_baseline, std::string_view t_text,
						 float t_max_width, Color t_color)
{
	if (t_max_width <= 0.0f) return;

	if (text_width(t_font, t_text) <= t_max_width) {
		draw_text(t_draw_list, t_font, t_baseline, t_text, t_color);
		return;
	}

	const float ellipsis_width = text_width(t_font, ellipsis);
	if (ellipsis_width > t_max_width) return;

	usize kept = 0;
	while (kept < t_text.size() && text_width(t_font, t_text.substr(0, kept + 1)) + ellipsis_width <= t_max_width) {
		kept += 1;
	}

	const std::string_view head = t_text.substr(0, kept);
	draw_text(t_draw_list, t_font, t_baseline, head, t_color);
	draw_text(t_draw_list, t_font, Vec2{t_baseline.x + text_width(t_font, head), t_baseline.y}, ellipsis, t_color);
}

u32 wrap_text(const Font &t_font, std::string_view t_text, float t_max_width, std::span<std::string_view> t_out_lines)
{
	u32 line_count = 0;
	usize paragraph_start = 0;

	while (paragraph_start <= t_text.size() && line_count < t_out_lines.size()) {
		const usize newline = t_text.find('\n', paragraph_start);
		const usize paragraph_end = newline == std::string_view::npos ? t_text.size() : newline;

		line_count += wrap_paragraph(t_font, t_text.substr(paragraph_start, paragraph_end - paragraph_start),
									 t_max_width, t_out_lines.subspan(line_count));
		paragraph_start = paragraph_end + 1;
	}

	return line_count;
}

float draw_wrapped_text(DrawList &t_draw_list, const Font &t_font, Vec2 t_first_baseline, float t_max_width,
						std::string_view t_text, Color t_color, u32 t_max_lines)
{
	constexpr u32 max_drawn_lines = 8;

	std::string_view lines[max_drawn_lines];
	const u32 line_count =
		wrap_text(t_font, t_text, t_max_width, std::span{lines, std::min(t_max_lines, max_drawn_lines)});

	Vec2 baseline = t_first_baseline;
	for (const std::string_view line : std::span{lines, line_count}) {
		draw_text(t_draw_list, t_font, baseline, line, t_color);
		baseline.y += t_font.line_height();
	}

	return baseline.y;
}
