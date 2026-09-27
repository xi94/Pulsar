#include "core/profiler.h"

#ifdef PULSAR_PROFILING

#include <algorithm>

#include <Windows.h>

namespace {
constexpr u32 max_scopes = 64;
constexpr float average_retention = 0.9f;

struct Scope {
	const char *name = nullptr;
	u32 depth = 0;
	i64 start_ticks = 0;
	i64 frame_ticks = 0;
	u32 frame_calls = 0;
	profiler::ScopeStats stats{};
};

struct Profiler {
	Scope scopes[max_scopes]{};
	u32 scope_count = 0;
	u32 open_scopes[max_scopes]{};
	u32 open_count = 0;
	i64 frame_start_ticks = 0;
	float frame_ms = 0.0f;
	bool in_frame = false;
};

Profiler g_profiler;

i64 current_ticks()
{
	LARGE_INTEGER counter;
	QueryPerformanceCounter(&counter);

	return counter.QuadPart;
}

float ticks_to_ms(i64 t_ticks)
{
	static const double ms_per_tick = [] {
		LARGE_INTEGER frequency;
		QueryPerformanceFrequency(&frequency);

		return 1000.0 / static_cast<double>(frequency.QuadPart);
	}();

	return static_cast<float>(static_cast<double>(t_ticks) * ms_per_tick);
}

Scope *find_or_add_scope(const char *t_name)
{
	for (u32 i = 0; i < g_profiler.scope_count; i += 1) {
		if (g_profiler.scopes[i].name == t_name) return &g_profiler.scopes[i];
	}

	if (g_profiler.scope_count >= max_scopes) return nullptr;

	Scope &scope = g_profiler.scopes[g_profiler.scope_count];
	g_profiler.scope_count += 1;

	scope.name = t_name;
	scope.stats.name = t_name;

	return &scope;
}
}

void profiler::begin_frame()
{
	g_profiler.frame_start_ticks = current_ticks();
	g_profiler.in_frame = true;
	g_profiler.open_count = 0;

	for (u32 i = 0; i < g_profiler.scope_count; i += 1) {
		g_profiler.scopes[i].frame_ticks = 0;
		g_profiler.scopes[i].frame_calls = 0;
	}
}

void profiler::end_frame()
{
	if (!g_profiler.in_frame) return;

	g_profiler.frame_ms = ticks_to_ms(current_ticks() - g_profiler.frame_start_ticks);
	g_profiler.in_frame = false;

	for (u32 i = 0; i < g_profiler.scope_count; i += 1) {
		Scope &scope = g_profiler.scopes[i];
		const float last_ms = ticks_to_ms(scope.frame_ticks);

		scope.stats.calls = scope.frame_calls;
		scope.stats.depth = scope.depth;
		scope.stats.last_ms = last_ms;
		scope.stats.average_ms = scope.stats.average_ms * average_retention + last_ms * (1.0f - average_retention);
		scope.stats.peak_ms = std::max(scope.stats.peak_ms, last_ms);
	}
}

void profiler::begin_scope(const char *t_name)
{
	Scope *scope = find_or_add_scope(t_name);
	if (scope == nullptr || g_profiler.open_count >= max_scopes) return;

	scope->depth = g_profiler.open_count;
	scope->start_ticks = current_ticks();
	scope->frame_calls += 1;

	g_profiler.open_scopes[g_profiler.open_count] = static_cast<u32>(scope - g_profiler.scopes);
	g_profiler.open_count += 1;
}

void profiler::end_scope()
{
	if (g_profiler.open_count == 0) return;

	g_profiler.open_count -= 1;

	Scope &scope = g_profiler.scopes[g_profiler.open_scopes[g_profiler.open_count]];
	scope.frame_ticks += current_ticks() - scope.start_ticks;
}

void profiler::reset()
{
	for (u32 i = 0; i < g_profiler.scope_count; i += 1) {
		g_profiler.scopes[i].stats.average_ms = 0.0f;
		g_profiler.scopes[i].stats.peak_ms = 0.0f;
	}
}

u32 profiler::scope_count()
{
	return g_profiler.scope_count;
}

const profiler::ScopeStats &profiler::scope(u32 t_index)
{
	return g_profiler.scopes[t_index].stats;
}

float profiler::frame_ms()
{
	return g_profiler.frame_ms;
}

#endif
