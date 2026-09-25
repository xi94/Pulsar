#pragma once

#include "core/profiler.h"

#ifdef PULSAR_PROFILING

#include "ui/widget.h"

class Fonts;

class ProfilerOverlay : public Widget {
  public:
	explicit ProfilerOverlay(const Fonts &t_fonts);

	void draw(DrawList &t_draw_list) override;
	bool on_key_down(u32 t_key) override;

  private:
	const Fonts &m_fonts;
	bool m_shown = false;
};

#endif
