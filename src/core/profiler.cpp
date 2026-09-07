#include "core/profiler.h"

#ifdef PULSAR_PROFILING

#include <Windows.h>

namespace {
/// How much of the previous average survives each frame. High enough that a row does not flicker
/// between two numbers, low enough that a real regression shows up within a second.
constexpr float kAverageRetention = 0.9f;

struct Scope {
	const char *pName = nullptr;
	u32 Depth = 0;
	i64 StartTicks = 0;

	// This frame's totals, folded into the report at EndFrame.
	i64 FrameTicks = 0;
	u32 FrameCalls = 0;

	CProfiler::ScopeStats Stats{};
};

struct ProfilerState {
	Scope Scopes[CProfiler::kMaxScopes]{};
	u32 ScopeCount = 0;

	// Indices into Scopes, so a scope entered twice in one frame nests correctly.
	u32 OpenScopes[CProfiler::kMaxScopes]{};
	u32 OpenCount = 0;

	i64 FrameStartTicks = 0;
	float FrameMs = 0.0f;
	bool bInFrame = false;
};

ProfilerState g_profiler;

i64 CurrentTicks()
{
	LARGE_INTEGER counter;
	QueryPerformanceCounter(&counter);

	return counter.QuadPart;
}

// Cached because QueryPerformanceFrequency is fixed for the life of the system, and this is
// called for every scope that closes.
double TicksToMilliseconds()
{
	static const double scale = [] {
		LARGE_INTEGER frequency;
		QueryPerformanceFrequency(&frequency);

		return 1000.0 / static_cast<double>(frequency.QuadPart);
	}();

	return scale;
}

/// Scopes are named by string literal, so pointer identity is enough and the linear scan is over
/// a handful of entries. A new name claims the next slot; past kMaxScopes it is dropped, which
/// costs a row in the report and nothing else.
Scope *FindOrAddScope(const char *pName)
{
	for (u32 i = 0; i < g_profiler.ScopeCount; i += 1) {
		if (g_profiler.Scopes[i].pName == pName) return &g_profiler.Scopes[i];
	}

	if (g_profiler.ScopeCount >= CProfiler::kMaxScopes) return nullptr;

	Scope &scope = g_profiler.Scopes[g_profiler.ScopeCount];
	g_profiler.ScopeCount += 1;

	scope.pName = pName;
	scope.Stats.pName = pName;

	return &scope;
}
} // namespace

void CProfiler::BeginFrame()
{
	g_profiler.FrameStartTicks = CurrentTicks();
	g_profiler.bInFrame = true;
	g_profiler.OpenCount = 0;

	for (u32 i = 0; i < g_profiler.ScopeCount; i += 1) {
		g_profiler.Scopes[i].FrameTicks = 0;
		g_profiler.Scopes[i].FrameCalls = 0;
	}
}

void CProfiler::EndFrame()
{
	if (!g_profiler.bInFrame) return;

	g_profiler.FrameMs =
		static_cast<float>(static_cast<double>(CurrentTicks() - g_profiler.FrameStartTicks) * TicksToMilliseconds());
	g_profiler.bInFrame = false;

	for (u32 i = 0; i < g_profiler.ScopeCount; i += 1) {
		Scope &scope = g_profiler.Scopes[i];
		const auto lastMs = static_cast<float>(static_cast<double>(scope.FrameTicks) * TicksToMilliseconds());

		scope.Stats.Calls = scope.FrameCalls;
		scope.Stats.Depth = scope.Depth;
		scope.Stats.LastMs = lastMs;
		scope.Stats.AverageMs = scope.Stats.AverageMs * kAverageRetention + lastMs * (1.0f - kAverageRetention);

		if (lastMs > scope.Stats.PeakMs) {
			scope.Stats.PeakMs = lastMs;
		}
	}
}

void CProfiler::BeginScope(const char *pName)
{
	Scope *pScope = FindOrAddScope(pName);
	if (pScope == nullptr || g_profiler.OpenCount >= kMaxScopes) return;

	pScope->Depth = g_profiler.OpenCount;
	pScope->StartTicks = CurrentTicks();
	pScope->FrameCalls += 1;

	g_profiler.OpenScopes[g_profiler.OpenCount] = static_cast<u32>(pScope - g_profiler.Scopes);
	g_profiler.OpenCount += 1;
}

void CProfiler::EndScope()
{
	if (g_profiler.OpenCount == 0) return;

	g_profiler.OpenCount -= 1;

	Scope &scope = g_profiler.Scopes[g_profiler.OpenScopes[g_profiler.OpenCount]];
	scope.FrameTicks += CurrentTicks() - scope.StartTicks;
}

void CProfiler::Reset()
{
	for (u32 i = 0; i < g_profiler.ScopeCount; i += 1) {
		g_profiler.Scopes[i].Stats.AverageMs = 0.0f;
		g_profiler.Scopes[i].Stats.PeakMs = 0.0f;
	}
}

u32 CProfiler::GetScopeCount()
{
	return g_profiler.ScopeCount;
}

const CProfiler::ScopeStats &CProfiler::GetScope(u32 index)
{
	return g_profiler.Scopes[index].Stats;
}

float CProfiler::GetFrameMs()
{
	return g_profiler.FrameMs;
}

#endif // PULSAR_PROFILING
