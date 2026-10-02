#pragma once

#include "core/profiler.h"

#ifdef PULSAR_PROFILING

#include "ui/widget.h"

struct Fonts;

class ProfilerOverlay : public Widget {
  public:
	explicit ProfilerOverlay(const Fonts* t_fonts);

	auto draw(DrawList* t_draw_list) -> void override;
	auto on_key_down(u32 t_key) -> bool override;

  private:
	const Fonts* m_fonts;
	bool         m_shown = false;
};

#endif
