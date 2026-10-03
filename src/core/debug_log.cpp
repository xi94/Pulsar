#include "core/debug_log.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <span>
#include <string>
#include <system_error>
#include <thread>

#include "core/app_identity.h"
#include "core/file.h"
#include "core/thread_util.h"
#include "os/crash_handler.h"
#include "os/files.h"
#include "os/system.h"

namespace {
constexpr u64   K_SLOW_CALL_MS                 = 250;
constexpr u64   K_FIRST_STUCK_REPORT_MS        = 2000;
constexpr u64   K_MAX_STUCK_REPORT_INTERVAL_MS = 60000;
constexpr u64   K_HANG_DUMP_AFTER_MS           = 20000;
constexpr u64   K_UI_STALL_MS                  = 2000;
constexpr auto  K_WATCHDOG_SCAN_INTERVAL       = std::chrono::milliseconds(500);
constexpr auto  K_WATCHDOG_SHUTDOWN_WAIT       = std::chrono::milliseconds(5000);
constexpr u32   K_MAX_OPEN_SCOPES              = 64;
constexpr usize K_LABEL_CAPACITY               = 160;

struct OpenScope {
	bool        in_use             = false;
	u64         thread_id          = 0;
	u64         start_ms           = 0;
	u64         next_report_ms     = 0;
	u64         report_interval_ms = 0;
	const char* category           = nullptr;
	char        label[K_LABEL_CAPACITY]{};
};

struct StuckReport {
	const char* category;
	u64         thread_id;
	u64         age_ms;
	char        label[K_LABEL_CAPACITY];
};

std::atomic<bool> g_enabled{false};
std::atomic<bool> g_initialized{false};
u64               g_start_ms = 0;

std::mutex  g_write_lock;
FILE*       g_file = nullptr;
std::string g_file_path;

std::mutex g_scope_lock;
OpenScope  g_open_scopes[K_MAX_OPEN_SCOPES];

std::atomic<u64>  g_last_ui_alive_ms{0};
std::atomic<bool> g_ui_stall_reported{false};
std::atomic<bool> g_hang_dump_written{false};

std::thread             g_watchdog;
std::atomic<bool>       g_watchdog_finished{false};
std::mutex              g_watchdog_lock;
std::condition_variable g_watchdog_wake;
bool                    g_watchdog_stopping = false;

[[nodiscard]] auto now_ms() -> u64
{
	return static_cast<u64>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

auto write_line(const char* t_category, const char* t_message) -> void
{
	const os::LocalTime time       = os::local_time();
	const u64           elapsed_ms = now_ms() - g_start_ms;

	char line[1400];
	std::snprintf(line, sizeof(line), "%02u:%02u:%02u.%03u  +%4llu.%03llus  t%-5llu  %-8s  %s\n", time.hour, time.minute, time.second, time.millisecond,
	              static_cast<unsigned long long>(elapsed_ms / 1000), static_cast<unsigned long long>(elapsed_ms % 1000),
	              static_cast<unsigned long long>(os::current_thread_id()), t_category != nullptr ? t_category : "-", t_message);

	const std::lock_guard lock(g_write_lock);

	if (g_file != nullptr) {
		std::fputs(line, g_file);
		std::fflush(g_file);
	}

	os::write_to_debugger(line);
	std::fputs(line, stdout);
	std::fflush(stdout);
}

auto write_formatted(const char* t_category, const char* t_format, va_list t_args) -> void
{
	char message[1024];
	std::vsnprintf(message, sizeof(message), t_format, t_args);

	write_line(t_category, message);
}

auto write_unchecked(const char* t_category, const char* t_format, ...) -> void
{
	va_list args;
	va_start(args, t_format);
	write_formatted(t_category, t_format, args);
	va_end(args);
}

auto write_hang_dump_once() -> void
{
	bool already_written = false;
	if (!g_hang_dump_written.compare_exchange_strong(already_written, true)) return;

	write_line("watchdog", "past the hang threshold - writing a diagnostic minidump of every thread");

	const std::string dump_path = os::write_diagnostic_dump("hang");
	if (dump_path.empty()) {
		write_line("watchdog", "diagnostic minidump FAILED to write");
	} else {
		write_unchecked("watchdog", "diagnostic minidump written: %s", dump_path.c_str());
	}
}

[[nodiscard]] auto collect_stuck_scopes(u64 t_now, StuckReport* t_out_reports, bool* t_out_past_hang_threshold) -> u32
{
	u32 report_count           = 0;
	*t_out_past_hang_threshold = false;

	const std::lock_guard lock(g_scope_lock);

	for (OpenScope& scope : g_open_scopes) {
		if (!scope.in_use || t_now < scope.next_report_ms) continue;

		const u64 age_ms           = t_now - scope.start_ms;
		*t_out_past_hang_threshold = *t_out_past_hang_threshold || age_ms >= K_HANG_DUMP_AFTER_MS;

		StuckReport* report = &t_out_reports[report_count];
		report->category    = scope.category;
		report->thread_id   = scope.thread_id;
		report->age_ms      = age_ms;
		std::memcpy(report->label, scope.label, sizeof(report->label));
		report_count += 1;

		scope.report_interval_ms = std::min(scope.report_interval_ms * 2, K_MAX_STUCK_REPORT_INTERVAL_MS);
		scope.next_report_ms     = t_now + scope.report_interval_ms;
	}

	return report_count;
}

auto report_stuck_scopes() -> void
{
	StuckReport reports[K_MAX_OPEN_SCOPES];
	bool        past_hang_threshold = false;
	const u32   report_count        = collect_stuck_scopes(now_ms(), reports, &past_hang_threshold);

	for (const StuckReport& report : std::span{reports, report_count}) {
		write_unchecked("watchdog", "STILL RUNNING after %llums on thread t%llu  [%s] %s", static_cast<unsigned long long>(report.age_ms),
		                static_cast<unsigned long long>(report.thread_id), report.category != nullptr ? report.category : "-", report.label);
	}

	if (past_hang_threshold) {
		write_hang_dump_once();
	}
}

auto report_ui_thread_stall() -> void
{
	const u64 last_alive_ms = g_last_ui_alive_ms.load(std::memory_order_relaxed);
	if (last_alive_ms == 0) return;

	const u64 since_ms = now_ms() - last_alive_ms;

	if (since_ms >= K_UI_STALL_MS) {
		bool already_reported = false;
		if (g_ui_stall_reported.compare_exchange_strong(already_reported, true)) {
			write_unchecked("watchdog", "UI THREAD STALLED - no frame for %llums (the whole app is frozen, not just a worker)",
			                static_cast<unsigned long long>(since_ms));
		}

		return;
	}

	bool was_reported = true;
	if (g_ui_stall_reported.compare_exchange_strong(was_reported, false)) {
		write_line("watchdog", "UI thread recovered - frames are being drawn again");
	}
}

auto watchdog_main() -> void
{
	std::unique_lock lock(g_watchdog_lock);

	while (!g_watchdog_wake.wait_for(lock, K_WATCHDOG_SCAN_INTERVAL, [] { return g_watchdog_stopping; })) {
		lock.unlock();
		report_stuck_scopes();
		report_ui_thread_stall();
		lock.lock();
	}

	g_watchdog_finished.store(true, std::memory_order_release);
}

auto open_log_file() -> void
{
	const std::string directory = app_data_subdirectory("logs");
	if (directory.empty()) return;

	const std::string path          = joined_path(directory, std::string{K_APP_NAME} + "-debug.log");
	const std::string previous_path = joined_path(directory, std::string{K_APP_NAME} + "-debug.prev.log");
	os::replace_file(path, previous_path);

	g_file = os::open_log_file(path);
	if (g_file != nullptr) {
		g_file_path = path;
	}
}

auto start_watchdog() -> void
{
	try {
		g_watchdog = std::thread(watchdog_main);
	} catch (const std::system_error&) {
		write_line("app", "watchdog thread could not be started - stuck-call reporting is off for this run");
	}
}

auto stop_watchdog() -> void
{
	{
		const std::lock_guard lock(g_watchdog_lock);
		g_watchdog_stopping = true;
	}

	g_watchdog_wake.notify_all();
	join_or_abandon(&g_watchdog, &g_watchdog_finished, K_WATCHDOG_SHUTDOWN_WAIT);
}
}

auto debug_log::init() -> void
{
	bool already_initialized = false;
	if (!g_initialized.compare_exchange_strong(already_initialized, true)) return;

	g_start_ms = now_ms();

	const bool forced_on = os::environment_variable(K_DEBUG_LOG_ENVIRONMENT_VARIABLE).has_value();
	if (!K_IS_DEBUG_BUILD && !forced_on) return;

	open_log_file();
	g_enabled.store(true, std::memory_order_release);

	write_unchecked("app", "%s %s%s - diagnostic log started", K_APP_NAME, K_APP_VERSION, K_IS_DEBUG_BUILD ? " [debug]" : " [release]");
	start_watchdog();
}

auto debug_log::shutdown() -> void
{
	if (!is_enabled()) return;

	write_line("app", "diagnostic log stopped");
	stop_watchdog();

	g_enabled.store(false, std::memory_order_release);

	const std::lock_guard lock(g_write_lock);

	if (g_file != nullptr) {
		std::fclose(g_file);
		g_file = nullptr;
	}
}

auto debug_log::is_enabled() -> bool
{
	return g_enabled.load(std::memory_order_acquire);
}

auto debug_log::file_path() -> const char*
{
	return g_file_path.c_str();
}

auto debug_log::write(const char* t_category, const char* t_format, ...) -> void
{
	if (!is_enabled()) return;

	va_list args;
	va_start(args, t_format);
	write_formatted(t_category, t_format, args);
	va_end(args);
}

auto debug_log::mark_ui_thread_alive() -> void
{
	g_last_ui_alive_ms.store(now_ms(), std::memory_order_relaxed);
}

debug_log::Scope::Scope(const char* t_category, const char* t_format, ...)
	: m_category(t_category)
{
	if (!is_enabled()) return;

	va_list args;
	va_start(args, t_format);
	std::vsnprintf(m_label, sizeof(m_label), t_format, args);
	va_end(args);

	m_start_ms = now_ms();

	const std::lock_guard lock(g_scope_lock);

	for (u32 i = 0; i < K_MAX_OPEN_SCOPES; i += 1) {
		OpenScope* scope = &g_open_scopes[i];
		if (scope->in_use) continue;

		scope->in_use             = true;
		scope->thread_id          = os::current_thread_id();
		scope->start_ms           = m_start_ms;
		scope->report_interval_ms = K_FIRST_STUCK_REPORT_MS;
		scope->next_report_ms     = m_start_ms + K_FIRST_STUCK_REPORT_MS;
		scope->category           = t_category;
		std::memcpy(scope->label, m_label, sizeof(m_label));
		m_slot = static_cast<i32>(i);
		break;
	}
}

debug_log::Scope::~Scope()
{
	if (m_slot >= 0) {
		const std::lock_guard lock(g_scope_lock);
		g_open_scopes[m_slot].in_use = false;
	}

	if (m_start_ms == 0 || !is_enabled()) return;

	const u64 elapsed = now_ms() - m_start_ms;
	if (elapsed >= K_SLOW_CALL_MS) {
		write_unchecked(m_category, "slow: %s took %llums", m_label, static_cast<unsigned long long>(elapsed));
	}
}

auto debug_log::Scope::elapsed_ms() const -> u64
{
	return m_start_ms == 0 ? 0 : now_ms() - m_start_ms;
}
