#pragma once

#include "core/profiler.h"

#ifdef PULSAR_PROFILING

#include "gfx/font_manager.h"
#include "ui/widget.h"

/// The profiler's report, drawn in the top-left corner and toggled with F1. Debug builds only -
/// the whole file is compiled out otherwise, and CMake does not even list it in a Release build.
///
/// Never blocking and never consuming a pointer event: it has to be possible to read this while
/// using the thing it is measuring.
class CProfilerOverlay : public CWidget {
  public:
	explicit CProfilerOverlay(CFontManager *pFonts);

	void Update(float deltaSeconds) override;
	void Draw(CDrawList &drawList) override;

	/// F1 toggles, F2 clears the accumulated averages and peaks.
	bool OnKeyDown(u32 keyCode) override;

  private:
	void DrawRow(CDrawList &drawList, const CProfiler::ScopeStats &stats, float x, float baselineY) const;

	CFontManager *m_pFonts = nullptr;
	bool m_bShown = false;
};

#endif // PULSAR_PROFILING
