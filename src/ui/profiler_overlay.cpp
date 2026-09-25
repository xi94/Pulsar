#include "ui/profiler_overlay.h"

#ifdef PULSAR_PROFILING

#include <cstdio>

#include <Windows.h>

#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "ui/text.h"

namespace {
constexpr float margin = 12.0f;
constexpr float padding = 10.0f;
constexpr float indent_per_depth = 12.0f;
constexpr float panel_width = 340.0f;
constexpr float corner_radius = 8.0f;
constexpr float frame_budget_ms = 16.6f;

constexpr Color color_panel{14, 14, 16, 225};
constexpr Color color_border{60, 60, 68, 255};
constexpr Color color_heading{236, 236, 240, 255};
constexpr Color color_over_budget{235, 110, 110, 255};
constexpr Color color_name{198, 198, 206, 255};
constexpr Color color_numbers{150, 190, 240, 255};
}

ProfilerOverlay::ProfilerOverlay(const Fonts &t_fonts)
	: m_fonts(t_fonts)
{
}

bool ProfilerOverlay::on_key_down(u32 t_key)
{
	if (t_key == VK_F1) {
		m_shown = !m_shown;
		return true;
	}

	if (t_key == VK_F2 && m_shown) {
		profiler::reset();
		return true;
	}

	return false;
}

void ProfilerOverlay::draw(DrawList &t_draw_list)
{
	if (!m_shown) return;

	const Font &font = m_fonts.secondary();
	const u32 scope_count = profiler::scope_count();
	const Rect panel{margin, margin, panel_width, padding * 2.0f + (scope_count + 2) * font.line_height()};

	t_draw_list.add_bordered_rect(panel, rounded(corner_radius), color_panel, color_border, 1.0f);

	const float x = panel.x + padding;
	float baseline = panel.y + padding + font.ascent();

	char line[64];
	std::snprintf(line, sizeof(line), "Frame %.2f ms   F1 hide   F2 reset", profiler::frame_ms());
	draw_text(t_draw_list, font, Vec2{x, baseline}, line,
			  profiler::frame_ms() > frame_budget_ms ? color_over_budget : color_heading);

	baseline += font.line_height();
	draw_text(t_draw_list, font, Vec2{x, baseline}, "scope                     avg    peak  n", color_name);

	for (u32 i = 0; i < scope_count; i += 1) {
		const profiler::ScopeStats &stats = profiler::scope(i);
		baseline += font.line_height();

		std::snprintf(line, sizeof(line), "%6.2f  %6.2f  %3u", stats.average_ms, stats.peak_ms, stats.calls);

		const float numbers_x = x + panel_width - padding * 2.0f - text_width(font, line);
		const float name_x = x + stats.depth * indent_per_depth;

		draw_text_truncated(t_draw_list, font, Vec2{name_x, baseline}, stats.name, numbers_x - name_x - padding,
							color_name);
		draw_text(t_draw_list, font, Vec2{numbers_x, baseline}, line, color_numbers);
	}
}

#endif
