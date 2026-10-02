#pragma once

namespace debug_log {

auto init() -> void;
auto shutdown() -> void;

[[nodiscard]] auto is_enabled() -> bool;
[[nodiscard]] auto file_path() -> const char*;

auto write(const char* t_category, const char* t_format, ...) -> void;

auto mark_ui_thread_alive() -> void;

class Scope {
  public:
	Scope(const char* t_category, const char* t_format, ...);
	~Scope();

	Scope(const Scope&)                    = delete;
	auto operator=(const Scope&) -> Scope& = delete;

	[[nodiscard]] auto elapsed_ms() const -> u64;

  private:
	const char* m_category = nullptr;
	i32         m_slot     = -1;
	u64         m_start_ms = 0;
	char        m_label[160]{};
};

}
