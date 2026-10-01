#include "core/debug_log.h"

#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <share.h>
#include <span>
#include <string>

#include <Windows.h>

#include "core/app_identity.h"
#include "core/crash_handler.h"
#include "core/file.h"
#include "core/str.h"

namespace {
constexpr u64 slow_call_ms = 250;
constexpr u64 first_stuck_report_ms = 2000;
constexpr u64 max_stuck_report_interval_ms = 60000;
constexpr u64 hang_dump_after_ms = 20000;
constexpr u64 ui_stall_ms = 2000;
constexpr DWORD watchdog_scan_interval_ms = 500;
constexpr DWORD watchdog_shutdown_wait_ms = 5000;
constexpr u32 max_open_scopes = 64;
constexpr usize label_capacity = 160;

struct OpenScope {
	bool in_use = false;
	DWORD thread_id = 0;
	u64 start_ms = 0;
	u64 next_report_ms = 0;
	u64 report_interval_ms = 0;
	const char *category = nullptr;
	char label[label_capacity]{};
};

struct StuckReport {
	const char *category;
	DWORD thread_id;
	u64 age_ms;
	char label[label_capacity];
};

std::atomic<bool> g_enabled{false};
std::atomic<bool> g_initialized{false};
u64 g_start_ms = 0;

SRWLOCK g_write_lock = SRWLOCK_INIT;
FILE *g_file = nullptr;
std::string g_file_path;

SRWLOCK g_scope_lock = SRWLOCK_INIT;
OpenScope g_open_scopes[max_open_scopes];

std::atomic<u64> g_last_ui_alive_ms{0};
std::atomic<bool> g_ui_stall_reported{false};
std::atomic<bool> g_hang_dump_written{false};

HANDLE g_watchdog_stop = nullptr;
HANDLE g_watchdog_thread = nullptr;

u64 now_ms()
{
	return GetTickCount64();
}

void write_line(const char *t_category, const char *t_message)
{
	SYSTEMTIME local_time;
	GetLocalTime(&local_time);

	const u64 elapsed_ms = now_ms() - g_start_ms;

	char line[1400];
	_snprintf_s(line, _TRUNCATE, "%02u:%02u:%02u.%03u  +%4llu.%03llus  t%-5lu  %-8s  %s\n", local_time.wHour, local_time.wMinute, local_time.wSecond,
				local_time.wMilliseconds, elapsed_ms / 1000, elapsed_ms % 1000, GetCurrentThreadId(), t_category != nullptr ? t_category : "-", t_message);

	AcquireSRWLockExclusive(&g_write_lock);

	if (g_file != nullptr) {
		std::fputs(line, g_file);
		std::fflush(g_file);
	}

	OutputDebugStringA(line);
	std::fputs(line, stdout);
	std::fflush(stdout);

	ReleaseSRWLockExclusive(&g_write_lock);
}

void write_formatted(const char *t_category, const char *t_format, va_list t_args)
{
	char message[1024];
	_vsnprintf_s(message, sizeof(message), _TRUNCATE, t_format, t_args);

	write_line(t_category, message);
}

void write_unchecked(const char *t_category, const char *t_format, ...)
{
	va_list args;
	va_start(args, t_format);
	write_formatted(t_category, t_format, args);
	va_end(args);
}

void write_hang_dump_once()
{
	bool already_written = false;
	if (!g_hang_dump_written.compare_exchange_strong(already_written, true)) return;

	write_line("watchdog", "past the hang threshold - writing a diagnostic minidump of every thread");

	const std::string dump_path = to_utf8(write_diagnostic_dump(L"hang"));
	if (dump_path.empty()) {
		write_line("watchdog", "diagnostic minidump FAILED to write");
	} else {
		write_unchecked("watchdog", "diagnostic minidump written: %s", dump_path.c_str());
	}
}

u32 collect_stuck_scopes(u64 t_now, StuckReport *t_out_reports, bool *t_out_past_hang_threshold)
{
	u32 report_count = 0;
	*t_out_past_hang_threshold = false;

	AcquireSRWLockExclusive(&g_scope_lock);

	for (OpenScope &scope : g_open_scopes) {
		if (!scope.in_use || t_now < scope.next_report_ms) continue;

		const u64 age_ms = t_now - scope.start_ms;
		*t_out_past_hang_threshold = *t_out_past_hang_threshold || age_ms >= hang_dump_after_ms;

		StuckReport *report = &t_out_reports[report_count];
		report->category = scope.category;
		report->thread_id = scope.thread_id;
		report->age_ms = age_ms;
		std::memcpy(report->label, scope.label, sizeof(report->label));
		report_count += 1;

		scope.report_interval_ms = std::min(scope.report_interval_ms * 2, max_stuck_report_interval_ms);
		scope.next_report_ms = t_now + scope.report_interval_ms;
	}

	ReleaseSRWLockExclusive(&g_scope_lock);

	return report_count;
}

void report_stuck_scopes()
{
	StuckReport reports[max_open_scopes];
	bool past_hang_threshold = false;
	const u32 report_count = collect_stuck_scopes(now_ms(), reports, &past_hang_threshold);

	for (const StuckReport &report : std::span{reports, report_count}) {
		write_unchecked("watchdog", "STILL RUNNING after %llums on thread t%lu  [%s] %s", report.age_ms, report.thread_id,
						report.category != nullptr ? report.category : "-", report.label);
	}

	if (past_hang_threshold) {
		write_hang_dump_once();
	}
}

void report_ui_thread_stall()
{
	const u64 last_alive_ms = g_last_ui_alive_ms.load(std::memory_order_relaxed);
	if (last_alive_ms == 0) return;

	const u64 since_ms = now_ms() - last_alive_ms;

	if (since_ms >= ui_stall_ms) {
		bool already_reported = false;
		if (g_ui_stall_reported.compare_exchange_strong(already_reported, true)) {
			write_unchecked("watchdog", "UI THREAD STALLED - no frame for %llums (the whole app is frozen, not just a worker)", since_ms);
		}

		return;
	}

	bool was_reported = true;
	if (g_ui_stall_reported.compare_exchange_strong(was_reported, false)) {
		write_line("watchdog", "UI thread recovered - frames are being drawn again");
	}
}

DWORD WINAPI watchdog_main(LPVOID)
{
	while (WaitForSingleObject(g_watchdog_stop, watchdog_scan_interval_ms) != WAIT_OBJECT_0) {
		report_stuck_scopes();
		report_ui_thread_stall();
	}

	return 0;
}

void open_log_file()
{
	const std::wstring directory = app_data_subdirectory(L"logs");
	if (directory.empty()) return;

	const std::wstring path = directory + L"\\" + app_name_wide + L"-debug.log";
	const std::wstring previous_path = directory + L"\\" + app_name_wide + L"-debug.prev.log";
	MoveFileExW(path.c_str(), previous_path.c_str(), MOVEFILE_REPLACE_EXISTING);

	// Shared for reading so the log of a run that is hung right now can still be opened.
	g_file = _wfsopen(path.c_str(), L"wb", _SH_DENYWR);
	if (g_file != nullptr) {
		g_file_path = to_utf8(path);
	}
}

void start_watchdog()
{
	g_watchdog_stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
	if (g_watchdog_stop != nullptr) {
		g_watchdog_thread = CreateThread(nullptr, 0, watchdog_main, nullptr, 0, nullptr);
	}

	if (g_watchdog_thread == nullptr) {
		write_line("app", "watchdog thread could not be started - stuck-call reporting is off for this run");
	}
}

void stop_watchdog()
{
	if (g_watchdog_stop != nullptr) {
		SetEvent(g_watchdog_stop);
	}

	if (g_watchdog_thread != nullptr) {
		WaitForSingleObject(g_watchdog_thread, watchdog_shutdown_wait_ms);
		CloseHandle(g_watchdog_thread);
		g_watchdog_thread = nullptr;
	}

	if (g_watchdog_stop != nullptr) {
		CloseHandle(g_watchdog_stop);
		g_watchdog_stop = nullptr;
	}
}
}

void debug_log::init()
{
	bool already_initialized = false;
	if (!g_initialized.compare_exchange_strong(already_initialized, true)) return;

	g_start_ms = now_ms();

	const bool forced_on = GetEnvironmentVariableW(debug_log_environment_variable, nullptr, 0) != 0;
	if (!is_debug_build && !forced_on) return;

	open_log_file();
	g_enabled.store(true, std::memory_order_release);

	write_unchecked("app", "%s %s%s - diagnostic log started", app_name, app_version, is_debug_build ? " [debug]" : " [release]");
	start_watchdog();
}

void debug_log::shutdown()
{
	if (!is_enabled()) return;

	write_line("app", "diagnostic log stopped");
	stop_watchdog();

	g_enabled.store(false, std::memory_order_release);

	AcquireSRWLockExclusive(&g_write_lock);

	if (g_file != nullptr) {
		std::fclose(g_file);
		g_file = nullptr;
	}

	ReleaseSRWLockExclusive(&g_write_lock);
}

bool debug_log::is_enabled()
{
	return g_enabled.load(std::memory_order_acquire);
}

const char *debug_log::file_path()
{
	return g_file_path.c_str();
}

void debug_log::write(const char *t_category, const char *t_format, ...)
{
	if (!is_enabled()) return;

	va_list args;
	va_start(args, t_format);
	write_formatted(t_category, t_format, args);
	va_end(args);
}

void debug_log::mark_ui_thread_alive()
{
	g_last_ui_alive_ms.store(now_ms(), std::memory_order_relaxed);
}

debug_log::Scope::Scope(const char *t_category, const char *t_format, ...)
	: m_category(t_category)
{
	if (!is_enabled()) return;

	va_list args;
	va_start(args, t_format);
	_vsnprintf_s(m_label, sizeof(m_label), _TRUNCATE, t_format, args);
	va_end(args);

	m_start_ms = now_ms();

	AcquireSRWLockExclusive(&g_scope_lock);

	for (u32 i = 0; i < max_open_scopes; i += 1) {
		OpenScope *scope = &g_open_scopes[i];
		if (scope->in_use) continue;

		scope->in_use = true;
		scope->thread_id = GetCurrentThreadId();
		scope->start_ms = m_start_ms;
		scope->report_interval_ms = first_stuck_report_ms;
		scope->next_report_ms = m_start_ms + first_stuck_report_ms;
		scope->category = t_category;
		std::memcpy(scope->label, m_label, sizeof(m_label));
		m_slot = static_cast<i32>(i);
		break;
	}

	ReleaseSRWLockExclusive(&g_scope_lock);
}

debug_log::Scope::~Scope()
{
	if (m_slot >= 0) {
		AcquireSRWLockExclusive(&g_scope_lock);
		g_open_scopes[m_slot].in_use = false;
		ReleaseSRWLockExclusive(&g_scope_lock);
	}

	if (m_start_ms == 0 || !is_enabled()) return;

	const u64 elapsed = now_ms() - m_start_ms;
	if (elapsed >= slow_call_ms) {
		write_unchecked(m_category, "slow: %s took %llums", m_label, elapsed);
	}
}

u64 debug_log::Scope::elapsed_ms() const
{
	return m_start_ms == 0 ? 0 : now_ms() - m_start_ms;
}
