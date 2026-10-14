#include "ui/profiler_overlay.h"

#ifdef PULSAR_PROFILING

#include <ranges>

#include "core/str.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "ui/text.h"

namespace {
constexpr float K_MARGIN           = 12.0f;
constexpr float K_PADDING          = 10.0f;
constexpr float K_INDENT_PER_DEPTH = 12.0f;
constexpr float K_PANEL_WIDTH      = 340.0f;
constexpr float K_CORNER_RADIUS    = 8.0f;
constexpr float K_FRAME_BUDGET_MS  = 16.6f;

constexpr Color K_COLOR_PANEL{14, 14, 16, 225};
constexpr Color K_COLOR_BORDER{60, 60, 68, 255};
constexpr Color K_COLOR_HEADING{236, 236, 240, 255};
constexpr Color K_COLOR_OVER_BUDGET{235, 110, 110, 255};
constexpr Color K_COLOR_NAME{198, 198, 206, 255};
constexpr Color K_COLOR_NUMBERS{150, 190, 240, 255};
}

ProfilerOverlay::ProfilerOverlay(const Fonts* t_fonts)
	: m_fonts(t_fonts)
{
}

auto ProfilerOverlay::on_key_down(os::Key t_key) -> bool
{
	if (t_key == os::Key::F1) {
		m_shown = !m_shown;
		return true;
	}

	if (t_key == os::Key::F2 && m_shown) {
		profiler::reset();
		return true;
	}

	return false;
}

auto ProfilerOverlay::draw(DrawList* t_draw_list) -> void
{
	if (!m_shown) return;

	const Font& font        = m_fonts->secondary;
	const u32   scope_count = profiler::scope_count();
	const Rect  panel{K_MARGIN, K_MARGIN, K_PANEL_WIDTH, K_PADDING * 2.0f + (scope_count + 2) * font.line_height()};

	t_draw_list->add_bordered_rect(panel, rounded(K_CORNER_RADIUS), K_COLOR_PANEL, K_COLOR_BORDER, 1.0f);

	const float x        = panel.x + K_PADDING;
	float       baseline = panel.y + K_PADDING + font.ascent;

	char                   line[64];
	const std::string_view frame_line  = format_into(line, "Frame {:.2f} ms   F1 hide   F2 reset", profiler::frame_ms());
	const Color            frame_color = profiler::frame_ms() > K_FRAME_BUDGET_MS ? K_COLOR_OVER_BUDGET : K_COLOR_HEADING;
	draw_text(t_draw_list, font, Vec2{x, baseline}, frame_line, frame_color);

	baseline += font.line_height();
	draw_text(t_draw_list, font, Vec2{x, baseline}, "scope                     avg    peak  n", K_COLOR_NAME);

	for (const u32 i : std::views::iota(0u, scope_count)) {
		const profiler::ScopeStats& stats = profiler::scope(i);
		baseline += font.line_height();

		const std::string_view numbers = format_into(line, "{:6.2f}  {:6.2f}  {:3}", stats.average_ms, stats.peak_ms, stats.calls);

		const float numbers_x  = x + K_PANEL_WIDTH - K_PADDING * 2.0f - text_width(font, numbers);
		const float name_x     = x + stats.depth * K_INDENT_PER_DEPTH;
		const float name_width = numbers_x - name_x - K_PADDING;

		draw_text_truncated(t_draw_list, font, Vec2{name_x, baseline}, stats.name, name_width, K_COLOR_NAME);
		draw_text(t_draw_list, font, Vec2{numbers_x, baseline}, numbers, K_COLOR_NUMBERS);
	}
}

#endif
