#pragma once

#ifdef PULSAR_PROFILING

namespace profiler {

struct ScopeStats {
	const char *name;
	u32 calls;
	u32 depth;
	float last_ms;
	float average_ms;
	float peak_ms;
};

void begin_frame();
void end_frame();

void begin_scope(const char *t_name);
void end_scope();

void reset();

u32 scope_count();
const ScopeStats &scope(u32 t_index);
float frame_ms();

}

class ProfileScope {
  public:
	explicit ProfileScope(const char *t_name)
	{
		profiler::begin_scope(t_name);
	}

	~ProfileScope()
	{
		profiler::end_scope();
	}

	ProfileScope(const ProfileScope &) = delete;
	ProfileScope &operator=(const ProfileScope &) = delete;
};

#define PULSAR_PROFILE_CONCAT_IMPL(a, b) a##b
#define PULSAR_PROFILE_CONCAT(a, b) PULSAR_PROFILE_CONCAT_IMPL(a, b)
#define PULSAR_PROFILE_SCOPE(name) const ProfileScope PULSAR_PROFILE_CONCAT(profile_scope_, __LINE__)(name)
#define PULSAR_PROFILE_FRAME_BEGIN() profiler::begin_frame()
#define PULSAR_PROFILE_FRAME_END() profiler::end_frame()

#else

#define PULSAR_PROFILE_SCOPE(name) ((void)0)
#define PULSAR_PROFILE_FRAME_BEGIN() ((void)0)
#define PULSAR_PROFILE_FRAME_END() ((void)0)

#endif
