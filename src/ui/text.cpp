#include "ui/text.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/str.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"

namespace {
constexpr std::string_view K_ELLIPSIS = "...";

struct TruncationProbe {
	Vec2  point{-1.0f, -1.0f};
	bool  found = false;
	Rect  bounds{};
	u32   cover_count = 0;
	char  text[512]{};
	usize length = 0;
};

TruncationProbe g_truncation_probe;

[[nodiscard]] auto is_drawn(u32 t_codepoint) -> bool
{
	return t_codepoint >= 0x20 && t_codepoint != 0x7F && !is_blocked_script(t_codepoint);
}

[[nodiscard]] auto is_space(char t_character) -> bool
{
	return t_character == ' ';
}

[[nodiscard]] auto skip_spaces(std::string_view t_text, usize t_index) -> usize
{
	while (t_index < t_text.size() && is_space(t_text[t_index])) {
		t_index += 1;
	}

	return t_index;
}

[[nodiscard]] auto skip_word(std::string_view t_text, usize t_index) -> usize
{
	while (t_index < t_text.size() && !is_space(t_text[t_index])) {
		t_index += 1;
	}

	return t_index;
}

[[nodiscard]] auto wrap_paragraph(const Font& t_font, std::string_view t_paragraph, float t_max_width, std::span<std::string_view> t_out_lines) -> u32
{
	if (t_out_lines.empty()) return 0;

	if (t_paragraph.empty()) {
		t_out_lines[0] = t_paragraph;
		return 1;
	}

	u32   line_count = 0;
	usize line_start = 0;

	while (line_start < t_paragraph.size() && line_count < t_out_lines.size()) {
		usize line_end = line_start;
		usize scan     = line_start;

		for (;;) {
			const usize word_start = skip_spaces(t_paragraph, scan);
			const usize word_end   = skip_word(t_paragraph, word_start);
			if (word_start == word_end) break;

			const bool fits = text_width(t_font, t_paragraph.substr(line_start, word_end - line_start)) <= t_max_width;
			if (!fits && line_end > line_start) break;

			line_end = word_end;
			scan     = word_end;
		}

		if (line_end == line_start) {
			const usize word_end = skip_word(t_paragraph, line_start);
			line_end             = next_codepoint(t_paragraph, line_start);

			while (line_end < word_end) {
				const usize next = next_codepoint(t_paragraph, line_end);
				if (text_width(t_font, t_paragraph.substr(line_start, next - line_start)) > t_max_width) break;

				line_end = next;
			}
		}

		t_out_lines[line_count] = t_paragraph.substr(line_start, line_end - line_start);
		line_count += 1;
		line_start = skip_spaces(t_paragraph, line_end);
	}

	return line_count;
}
}

[[nodiscard]] auto text_width(const Font& t_font, std::string_view t_text) -> float
{
	if (t_font.glyphs == nullptr) return 0.0f;

	float advance = 0.0f;

	for (usize index = 0; index < t_text.size();) {
		const Utf8Codepoint codepoint = decode_utf8(t_text, index);
		index += codepoint.length;

		if (is_drawn(codepoint.value)) {
			advance += t_font.glyph(codepoint.value)->advance;
		}
	}

	return advance / t_font.bake_scale;
}

[[nodiscard]] auto text_index_at(const Font& t_font, std::string_view t_text, float t_x) -> u32
{
	if (t_font.glyphs == nullptr) return 0;

	const float baked_x = t_x * t_font.bake_scale;
	float       pen_x   = 0.0f;

	for (usize index = 0; index < t_text.size();) {
		const Utf8Codepoint codepoint = decode_utf8(t_text, index);

		if (is_drawn(codepoint.value)) {
			const float glyph_start = pen_x;
			pen_x += t_font.glyph(codepoint.value)->advance;

			if (baked_x < (glyph_start + pen_x) * 0.5f) return static_cast<u32>(index);
		}

		index += codepoint.length;
	}

	return static_cast<u32>(t_text.size());
}

auto draw_text(DrawList* t_draw_list, const Font& t_font, Vec2 t_baseline, std::string_view t_text, Color t_color) -> void
{
	if (t_font.glyphs == nullptr) return;

	// The atlas is baked at physical resolution, so glyphs only stay crisp on whole physical pixels.
	const float bake_scale = t_font.bake_scale;
	const float pen_y      = std::round(t_baseline.y * bake_scale);
	float       pen_x      = std::round(t_baseline.x * bake_scale);

	for (usize index = 0; index < t_text.size();) {
		const Utf8Codepoint codepoint = decode_utf8(t_text, index);
		index += codepoint.length;

		if (!is_drawn(codepoint.value)) continue;

		const Glyph* glyph = t_font.glyph(codepoint.value);

		if (glyph->x1 > glyph->x0) {
			const float x = std::round(pen_x) + glyph->x0;
			const float y = pen_y + glyph->y0;
			const Rect  quad{x / bake_scale, y / bake_scale, (glyph->x1 - glyph->x0) / bake_scale, (glyph->y1 - glyph->y0) / bake_scale};

			t_draw_list->add_image(quad, t_font.atlas.get(), t_color, K_SQUARE_CORNERS, UvRect{glyph->u0, glyph->v0, glyph->u1, glyph->v1});
		}

		pen_x += glyph->advance;
	}
}

auto draw_text_centered(DrawList* t_draw_list, const Font& t_font, Rect t_box, std::string_view t_text, Color t_color) -> void
{
	const float x = t_box.x + (t_box.w - text_width(t_font, t_text)) * 0.5f;

	draw_text(t_draw_list, t_font, Vec2{x, t_font.centered_baseline(t_box)}, t_text, t_color);
}

auto draw_text_truncated(DrawList* t_draw_list, const Font& t_font, Vec2 t_baseline, std::string_view t_text, float t_max_width, Color t_color) -> void
{
	if (t_max_width <= 0.0f) return;

	if (text_width(t_font, t_text) <= t_max_width) {
		draw_text(t_draw_list, t_font, t_baseline, t_text, t_color);
		return;
	}

	const float ellipsis_width = text_width(t_font, K_ELLIPSIS);
	if (ellipsis_width > t_max_width) return;

	usize kept       = 0;
	float kept_width = 0.0f;

	while (kept < t_text.size()) {
		const usize next  = next_codepoint(t_text, kept);
		const float width = text_width(t_font, t_text.substr(kept, next - kept));
		if (kept_width + width + ellipsis_width > t_max_width) break;

		kept = next;
		kept_width += width;
	}

	const std::string_view head       = t_text.substr(0, kept);
	const float            head_width = text_width(t_font, head);
	draw_text(t_draw_list, t_font, t_baseline, head, t_color);
	draw_text(t_draw_list, t_font, Vec2{t_baseline.x + head_width, t_baseline.y}, K_ELLIPSIS, t_color);

	TruncationProbe* probe = &g_truncation_probe;
	const Rect       shown{t_baseline.x, t_baseline.y - t_font.ascent, head_width + ellipsis_width, t_font.line_height()};
	const Rect       visible = t_draw_list->visible_rect(shown);
	if (!visible.contains(probe->point)) return;

	probe->found       = true;
	probe->bounds      = visible;
	probe->cover_count = t_draw_list->probe_cover_count();
	probe->length      = std::min(t_text.size(), sizeof(probe->text));
	std::memcpy(probe->text, t_text.data(), probe->length);
}

auto begin_truncation_probe(DrawList* t_draw_list, Vec2 t_point) -> void
{
	t_draw_list->set_probe(t_point);
	g_truncation_probe.point = t_point;
	g_truncation_probe.found = false;
}

[[nodiscard]] auto hovered_truncated_text(const DrawList* t_draw_list) -> std::optional<TruncatedText>
{
	const TruncationProbe* probe = &g_truncation_probe;
	if (!probe->found || probe->cover_count != t_draw_list->probe_cover_count()) return std::nullopt;

	return TruncatedText{probe->bounds, std::string_view{probe->text, probe->length}};
}

[[nodiscard]] auto wrap_text(const Font& t_font, std::string_view t_text, float t_max_width, std::span<std::string_view> t_out_lines) -> u32
{
	u32   line_count      = 0;
	usize paragraph_start = 0;

	while (paragraph_start <= t_text.size() && line_count < t_out_lines.size()) {
		const usize newline       = t_text.find('\n', paragraph_start);
		const usize paragraph_end = newline == std::string_view::npos ? t_text.size() : newline;

		line_count += wrap_paragraph(t_font, t_text.substr(paragraph_start, paragraph_end - paragraph_start), t_max_width, t_out_lines.subspan(line_count));
		paragraph_start = paragraph_end + 1;
	}

	return line_count;
}

auto draw_wrapped_text(DrawList*        t_draw_list,
                       const Font&      t_font,
                       Vec2             t_first_baseline,
                       float            t_max_width,
                       std::string_view t_text,
                       Color            t_color,
                       u32              t_max_lines) -> float
{
	constexpr u32 MAX_DRAWN_LINES = 8;

	std::string_view lines[MAX_DRAWN_LINES];
	const u32        line_count = wrap_text(t_font, t_text, t_max_width, std::span{lines, std::min(t_max_lines, MAX_DRAWN_LINES)});

	Vec2 baseline = t_first_baseline;
	for (const std::string_view line : std::span{lines, line_count}) {
		draw_text(t_draw_list, t_font, baseline, line, t_color);
		baseline.y += t_font.line_height();
	}

	return baseline.y;
}
