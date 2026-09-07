#pragma once

// A frame profiler for finding what is actually slow, compiled in only when PULSAR_PROFILING is
// defined - which CMake does for every configuration except Release, so a shipped build carries
// none of this and every macro below expands to nothing.
//
// Scopes are named by string literal and keyed on the pointer, not the text, so recording one is
// a comparison against a handful of pointers rather than a hash of a string. Nesting is tracked,
// so a scope's children can be read as the indented rows beneath it.
//
// Single-threaded by design: this measures the frame, and the frame is one thread. Calling it
// from a worker is not a data race that is guarded against, it is a mistake.

#ifdef PULSAR_PROFILING

class CProfiler {
  public:
	/// One row of the report. LastMs is the most recent frame, AverageMs is smoothed so a row
	/// stays readable, and PeakMs is the worst frame since the last reset - which is usually the
	/// number worth chasing.
	struct ScopeStats {
		const char *pName;
		u32 Calls;
		u32 Depth;
		float LastMs;
		float AverageMs;
		float PeakMs;
	};

	static constexpr u32 kMaxScopes = 64;

	/// Rolls the previous frame's totals into the report and starts fresh.
	static void BeginFrame();
	static void EndFrame();

	/// Paired by CProfileScope; call these directly only where a scope cannot bracket the work.
	static void BeginScope(const char *pName);
	static void EndScope();

	/// Clears every accumulated average and peak, for measuring one specific thing without the
	/// startup frames dragging the numbers around.
	static void Reset();

	static u32 GetScopeCount();
	static const ScopeStats &GetScope(u32 index);

	/// Wall time for the whole of the last frame, which is what every scope should be read
	/// against - a scope at 2ms means nothing until you know whether the frame was 3ms or 30.
	static float GetFrameMs();
};

/// RAII bracket. The macro below is the only intended way to make one.
class CProfileScope {
  public:
	explicit CProfileScope(const char *pName)
	{
		CProfiler::BeginScope(pName);
	}

	~CProfileScope()
	{
		CProfiler::EndScope();
	}

	CProfileScope(const CProfileScope &) = delete;
	CProfileScope &operator=(const CProfileScope &) = delete;
};

#define PULSAR_PROFILE_CONCAT_IMPL(a, b) a##b
#define PULSAR_PROFILE_CONCAT(a, b) PULSAR_PROFILE_CONCAT_IMPL(a, b)

/// Measures from here to the end of the enclosing scope. The name must be a string literal.
#define PULSAR_PROFILE_SCOPE(name) const CProfileScope PULSAR_PROFILE_CONCAT(profileScope_, __LINE__)(name)

#define PULSAR_PROFILE_FRAME_BEGIN() CProfiler::BeginFrame()
#define PULSAR_PROFILE_FRAME_END() CProfiler::EndFrame()

#else

#define PULSAR_PROFILE_SCOPE(name) ((void)0)
#define PULSAR_PROFILE_FRAME_BEGIN() ((void)0)
#define PULSAR_PROFILE_FRAME_END() ((void)0)

#endif // PULSAR_PROFILING
