#pragma once

#include <memory>

#include "core/types.h"

namespace os {
class Window;
}

class GlContext {
  public:
	GlContext();
	~GlContext();

	GlContext(const GlContext&)                    = delete;
	auto operator=(const GlContext&) -> GlContext& = delete;

	[[nodiscard]] auto create(const os::Window* t_window) -> bool;
	auto resize() -> void;
	auto present() -> void;
	[[nodiscard]] auto proc_address(const char* t_name) const -> void*;

  private:
	struct Native;

	std::unique_ptr<Native> m_native;
};
