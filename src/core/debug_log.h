#pragma once

namespace debug_log {

void init();
void shutdown();

bool is_enabled();
const char *file_path();

void write(const char *t_category, const char *t_format, ...);

void mark_ui_thread_alive();

class Scope {
  public:
	Scope(const char *t_category, const char *t_format, ...);
	~Scope();

	Scope(const Scope &) = delete;
	Scope &operator=(const Scope &) = delete;

	u64 elapsed_ms() const;

  private:
	const char *m_category = nullptr;
	i32 m_slot = -1;
	u64 m_start_ms = 0;
	char m_label[160]{};
};

}
