#include "core/profiler.h"

#ifdef PULSAR_PROFILING

#include <algorithm>
#include <chrono>

namespace {
constexpr u32   K_MAX_SCOPES        = 64;
constexpr float K_AVERAGE_RETENTION = 0.9f;

struct Scope {
	const char*          name        = nullptr;
	u32                  depth       = 0;
	i64                  start_ticks = 0;
	i64                  frame_ticks = 0;
	u32                  frame_calls = 0;
	profiler::ScopeStats stats{};
};

struct Profiler {
	Scope scopes[K_MAX_SCOPES]{};
	u32   scope_count = 0;
	u32   open_scopes[K_MAX_SCOPES]{};
	u32   open_count        = 0;
	i64   frame_start_ticks = 0;
	float frame_ms          = 0.0f;
	bool  in_frame          = false;
};

Profiler g_profiler;

[[nodiscard]] auto current_ticks() -> i64
{
	return std::chrono::steady_clock::now().time_since_epoch().count();
}

[[nodiscard]] auto ticks_to_ms(i64 t_ticks) -> float
{
	return std::chrono::duration<float, std::milli>(std::chrono::steady_clock::duration{t_ticks}).count();
}

[[nodiscard]] auto find_or_add_scope(const char* t_name) -> Scope*
{
	for (u32 i = 0; i < g_profiler.scope_count; i += 1) {
		if (g_profiler.scopes[i].name == t_name) return &g_profiler.scopes[i];
	}

	if (g_profiler.scope_count >= K_MAX_SCOPES) return nullptr;

	Scope* scope = &g_profiler.scopes[g_profiler.scope_count];
	g_profiler.scope_count += 1;

	scope->name       = t_name;
	scope->stats.name = t_name;

	return scope;
}
}

auto profiler::begin_frame() -> void
{
	g_profiler.frame_start_ticks = current_ticks();
	g_profiler.in_frame          = true;
	g_profiler.open_count        = 0;

	for (u32 i = 0; i < g_profiler.scope_count; i += 1) {
		g_profiler.scopes[i].frame_ticks = 0;
		g_profiler.scopes[i].frame_calls = 0;
	}
}

auto profiler::end_frame() -> void
{
	if (!g_profiler.in_frame) return;

	g_profiler.frame_ms = ticks_to_ms(current_ticks() - g_profiler.frame_start_ticks);
	g_profiler.in_frame = false;

	for (u32 i = 0; i < g_profiler.scope_count; i += 1) {
		Scope*      scope   = &g_profiler.scopes[i];
		const float last_ms = ticks_to_ms(scope->frame_ticks);

		scope->stats.calls      = scope->frame_calls;
		scope->stats.depth      = scope->depth;
		scope->stats.last_ms    = last_ms;
		scope->stats.average_ms = scope->stats.average_ms * K_AVERAGE_RETENTION + last_ms * (1.0f - K_AVERAGE_RETENTION);
		scope->stats.peak_ms    = std::max(scope->stats.peak_ms, last_ms);
	}
}

auto profiler::begin_scope(const char* t_name) -> void
{
	Scope* scope = find_or_add_scope(t_name);
	if (scope == nullptr || g_profiler.open_count >= K_MAX_SCOPES) return;

	scope->depth       = g_profiler.open_count;
	scope->start_ticks = current_ticks();
	scope->frame_calls += 1;

	g_profiler.open_scopes[g_profiler.open_count] = static_cast<u32>(scope - g_profiler.scopes);
	g_profiler.open_count += 1;
}

auto profiler::end_scope() -> void
{
	if (g_profiler.open_count == 0) return;

	g_profiler.open_count -= 1;

	Scope* scope = &g_profiler.scopes[g_profiler.open_scopes[g_profiler.open_count]];
	scope->frame_ticks += current_ticks() - scope->start_ticks;
}

auto profiler::reset() -> void
{
	for (u32 i = 0; i < g_profiler.scope_count; i += 1) {
		g_profiler.scopes[i].stats.average_ms = 0.0f;
		g_profiler.scopes[i].stats.peak_ms    = 0.0f;
	}
}

auto profiler::scope_count() -> u32
{
	return g_profiler.scope_count;
}

auto profiler::scope(u32 t_index) -> const profiler::ScopeStats&
{
	return g_profiler.scopes[t_index].stats;
}

auto profiler::frame_ms() -> float
{
	return g_profiler.frame_ms;
}

#endif
