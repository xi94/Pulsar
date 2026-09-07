#include "core/debug_log.h"

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <share.h>
#include <string>

#include <Windows.h>
#include <shlobj.h>

#include "core/crash_handler.h"
#include "core/app_identity.h"
#include "core/app_paths.h"

namespace {
// Every poll loop in this project ticks at 100ms, so anything under this is the normal case
// and reporting it would bury the abnormal one.
constexpr u64 kSlowCallMs = 250;

// The watchdog's first callout for a still-open scope, and the ceiling its doubling interval
// grows to. Escalating rather than fixed, so a permanently abandoned call settles into an
// occasional reminder instead of a line every scan forever.
constexpr u64 kFirstReportMs = 2000;
constexpr u64 kMaxReportIntervalMs = 60000;

// Comfortably past every real timeout in the login flow - the longest is CRiotClient's 10s
// form wait - so the dump only fires for something that genuinely is not coming back.
constexpr u64 kHangDumpMs = 20000;

// Two seconds is many missed frames, far past any legitimate hitch.
constexpr u64 kUiStallMs = 2000;

constexpr u64 kWatchdogScanIntervalMs = 500;

// Never grown: an abandoned thread's breadcrumb is never released, so this is really "how
// many permanently wedged calls can be tracked before tracking stops being useful".
constexpr i32 kMaxBreadcrumbs = 64;

struct Breadcrumb {
	bool bInUse = false;
	DWORD ThreadId = 0;
	u64 StartMs = 0;
	u64 NextReportMs = 0;	  // an absolute tick, not a duration
	u64 ReportIntervalMs = 0; // doubles up to kMaxReportIntervalMs
	const char *pCategory = nullptr;
	char szLabel[160]{};
};

// One report the watchdog decided to emit, copied out of the table so the logging happens
// with g_breadcrumbLock already released.
struct PendingReport {
	const char *pCategory;
	DWORD ThreadId;
	u64 AgeMs;
	char szLabel[160];
};

std::atomic<bool> g_bEnabled{false};
std::atomic<bool> g_bInitialized{false};

u64 g_startTicks = 0;

// Guards the sinks below so two threads' lines never interleave. Never held across anything
// that can block.
SRWLOCK g_writeLock = SRWLOCK_INIT;
FILE *g_pFile = nullptr;
std::string g_filePath;

// Strictly inner to g_writeLock: the watchdog copies out what it wants to report, releases
// this, and only then writes.
SRWLOCK g_breadcrumbLock = SRWLOCK_INIT;
Breadcrumb g_breadcrumbs[kMaxBreadcrumbs];

std::atomic<u64> g_lastUiAliveMs{0};
std::atomic<bool> g_bUiStallReported{false};
std::atomic<bool> g_bHangDumpWritten{false};

HANDLE g_hWatchdogStop = nullptr;
HANDLE g_hWatchdogThread = nullptr;

u64 NowMs()
{
	return GetTickCount64();
}

// Under %LOCALAPPDATA% rather than next to the .exe, which may sit somewhere a normal user
// cannot write at all.
std::wstring LogDirectory()
{
	return AppDataSubdirectory(L"logs");
}

std::string WideToUtf8(const std::wstring &wide)
{
	if (wide.empty()) return std::string{};

	const int length =
		WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
	if (length <= 0) return std::string{};

	std::string utf8(static_cast<usize>(length), '\0');
	WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), utf8.data(), length, nullptr, nullptr);

	return utf8;
}

// The file is the sink that matters, since it survives the force-kill that ends a hang.
// OutputDebugStringA is for watching live; stdout only goes anywhere in a Debug build.
void WriteLine(const char *pCategory, const char *pMessage)
{
	SYSTEMTIME localTime;
	GetLocalTime(&localTime);

	const u64 elapsedMs = NowMs() - g_startTicks;

	char line[1400];
	_snprintf_s(line, _TRUNCATE, "%02u:%02u:%02u.%03u  +%4llu.%03llus  t%-5lu  %-8s  %s\n", localTime.wHour,
				localTime.wMinute, localTime.wSecond, localTime.wMilliseconds,
				static_cast<unsigned long long>(elapsedMs / 1000), static_cast<unsigned long long>(elapsedMs % 1000),
				GetCurrentThreadId(), pCategory != nullptr ? pCategory : "-", pMessage);

	AcquireSRWLockExclusive(&g_writeLock);

	if (g_pFile != nullptr) {
		std::fputs(line, g_pFile);
		std::fflush(g_pFile);
	}

	OutputDebugStringA(line);
	std::fputs(line, stdout);
	std::fflush(stdout);

	ReleaseSRWLockExclusive(&g_writeLock);
}

// This file's reporting, which has no reason to re-check the enabled flag Write already
// tested.
void WriteFormatted(const char *pCategory, const char *pFormat, ...)
{
	char message[1024];

	va_list args;
	va_start(args, pFormat);
	_vsnprintf_s(message, sizeof(message), _TRUNCATE, pFormat, args);
	va_end(args);

	WriteLine(pCategory, message);
}

void WriteHangDump()
{
	bool expected = false;
	if (!g_bHangDumpWritten.compare_exchange_strong(expected, true)) {
		return; // one per run is the point
	}

	WriteLine("watchdog", "past the hang threshold - writing a diagnostic minidump of every thread");

	const std::string dumpPath = WideToUtf8(WriteDiagnosticDump(L"hang"));
	if (dumpPath.empty()) {
		WriteLine("watchdog", "diagnostic minidump FAILED to write");
	} else {
		WriteFormatted("watchdog", "diagnostic minidump written: %s", dumpPath.c_str());
	}
}

// Collects everything due for a callout and reschedules each one, all under the lock, so the
// caller can log without holding it.
i32 CollectDueReports(u64 now, PendingReport *pOutReports, bool &outAnyPastHangThreshold)
{
	i32 reportCount = 0;
	outAnyPastHangThreshold = false;

	AcquireSRWLockExclusive(&g_breadcrumbLock);

	for (Breadcrumb &crumb : g_breadcrumbs) {
		if (!crumb.bInUse || now < crumb.NextReportMs) continue;

		const u64 ageMs = now - crumb.StartMs;
		outAnyPastHangThreshold = outAnyPastHangThreshold || ageMs >= kHangDumpMs;

		PendingReport &report = pOutReports[reportCount];
		report.pCategory = crumb.pCategory;
		report.ThreadId = crumb.ThreadId;
		report.AgeMs = ageMs;
		std::memcpy(report.szLabel, crumb.szLabel, sizeof(report.szLabel));
		reportCount += 1;

		const u64 nextInterval = crumb.ReportIntervalMs * 2;
		crumb.ReportIntervalMs = nextInterval < kMaxReportIntervalMs ? nextInterval : kMaxReportIntervalMs;
		crumb.NextReportMs = now + crumb.ReportIntervalMs;
	}

	ReleaseSRWLockExclusive(&g_breadcrumbLock);

	return reportCount;
}

void ScanBreadcrumbs()
{
	PendingReport reports[kMaxBreadcrumbs];
	bool bAnyPastHangThreshold = false;
	const i32 reportCount = CollectDueReports(NowMs(), reports, bAnyPastHangThreshold);

	for (i32 i = 0; i < reportCount; i += 1) {
		const PendingReport &report = reports[i];
		WriteFormatted("watchdog", "STILL RUNNING after %llums on thread t%lu  [%s] %s",
					   static_cast<unsigned long long>(report.AgeMs), report.ThreadId,
					   report.pCategory != nullptr ? report.pCategory : "-", report.szLabel);
	}

	if (bAnyPastHangThreshold) {
		WriteHangDump();
	}
}

void ScanUiThread()
{
	const u64 lastAlive = g_lastUiAliveMs.load(std::memory_order_relaxed);
	if (lastAlive == 0) {
		return; // the render thread has not reached its main loop yet
	}

	const u64 sinceMs = NowMs() - lastAlive;

	if (sinceMs >= kUiStallMs) {
		// Latched, or a frozen UI would say so on every scan for as long as it stays frozen.
		bool expected = false;
		if (g_bUiStallReported.compare_exchange_strong(expected, true)) {
			WriteFormatted("watchdog",
						   "UI THREAD STALLED - no frame for %llums (the whole app is frozen, not just a worker)",
						   static_cast<unsigned long long>(sinceMs));
		}

		return;
	}

	bool expected = true;
	if (g_bUiStallReported.compare_exchange_strong(expected, false)) {
		WriteLine("watchdog", "UI thread recovered - frames are being drawn again");
	}
}

DWORD WINAPI WatchdogMain(LPVOID)
{
	while (WaitForSingleObject(g_hWatchdogStop, static_cast<DWORD>(kWatchdogScanIntervalMs)) != WAIT_OBJECT_0) {
		ScanBreadcrumbs();
		ScanUiThread();
	}

	return 0;
}

void OpenLogFile()
{
	const std::wstring dir = LogDirectory();
	if (dir.empty()) return;

	const std::wstring path = dir + L"\\" + kAppNameW + L"-debug.log";

	// Keep exactly one previous run, so the interesting run's log is not destroyed by the
	// relaunch that goes on to report it.
	MoveFileExW(path.c_str(), (dir + L"\\" + kAppNameW + L"-debug.prev.log").c_str(), MOVEFILE_REPLACE_EXISTING);

	// _SH_DENYWR rather than a plain open: the most useful thing to do with this file is read
	// it while the run that is hanging is still hung, and an exclusive lock prevents that.
	FILE *pFile = _wfsopen(path.c_str(), L"wb", _SH_DENYWR);
	if (pFile != nullptr) {
		g_pFile = pFile;
		g_filePath = WideToUtf8(path);
	}
}

void StartWatchdog()
{
	g_hWatchdogStop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
	if (g_hWatchdogStop != nullptr) {
		g_hWatchdogThread = CreateThread(nullptr, 0, WatchdogMain, nullptr, 0, nullptr);
	}

	if (g_hWatchdogThread == nullptr) {
		WriteLine("app", "watchdog thread could not be started - stuck-call reporting is off for this run");
	}
}

void StopWatchdog()
{
	if (g_hWatchdogStop != nullptr) {
		SetEvent(g_hWatchdogStop);
	}

	if (g_hWatchdogThread != nullptr) {
		// Bounded: the watchdog can be mid-minidump, and hanging shutdown on the one thread
		// whose job is diagnosing hangs would be a poor joke.
		WaitForSingleObject(g_hWatchdogThread, 5000);
		CloseHandle(g_hWatchdogThread);
		g_hWatchdogThread = nullptr;
	}

	if (g_hWatchdogStop != nullptr) {
		CloseHandle(g_hWatchdogStop);
		g_hWatchdogStop = nullptr;
	}
}
} // namespace

namespace DebugLog {

void Init()
{
	bool expected = false;
	if (!g_bInitialized.compare_exchange_strong(expected, true)) return;

	g_startTicks = NowMs();

	const bool bForced = GetEnvironmentVariableW(kDebugLogEnvironmentVariable, nullptr, 0) != 0;
	if (!kIsDebugBuild && !bForced) return;

	OpenLogFile();
	g_bEnabled.store(true, std::memory_order_release);

	WriteFormatted("app", "%s %s%s - diagnostic log started", kAppName, kAppVersion,
				   kIsDebugBuild ? " [debug]" : " [release]");
	StartWatchdog();
}

void Shutdown()
{
	if (!g_bEnabled.load(std::memory_order_acquire)) return;

	WriteLine("app", "diagnostic log stopped");
	StopWatchdog();

	g_bEnabled.store(false, std::memory_order_release);

	AcquireSRWLockExclusive(&g_writeLock);

	if (g_pFile != nullptr) {
		std::fclose(g_pFile);
		g_pFile = nullptr;
	}

	ReleaseSRWLockExclusive(&g_writeLock);
}

bool IsEnabled()
{
	return g_bEnabled.load(std::memory_order_acquire);
}

const char *GetFilePath()
{
	return g_filePath.c_str();
}

void Write(const char *pCategory, const char *pFormat, ...)
{
	if (!g_bEnabled.load(std::memory_order_acquire)) return;

	char message[1024];

	va_list args;
	va_start(args, pFormat);
	_vsnprintf_s(message, sizeof(message), _TRUNCATE, pFormat, args);
	va_end(args);

	WriteLine(pCategory, message);
}

void MarkUiThreadAlive()
{
	// Unconditional: this is a single counter read, and gating it would let the first scan
	// after a mid-run enable see a stale zero and cry stall.
	g_lastUiAliveMs.store(NowMs(), std::memory_order_relaxed);
}

CScope::CScope(const char *pCategory, const char *pFormat, ...)
	: m_pCategory(pCategory)
{
	if (!g_bEnabled.load(std::memory_order_acquire)) return;

	va_list args;
	va_start(args, pFormat);
	_vsnprintf_s(m_szLabel, sizeof(m_szLabel), _TRUNCATE, pFormat, args);
	va_end(args);

	m_startMs = NowMs();

	AcquireSRWLockExclusive(&g_breadcrumbLock);

	for (i32 i = 0; i < kMaxBreadcrumbs; i += 1) {
		if (g_breadcrumbs[i].bInUse) continue;

		g_breadcrumbs[i].bInUse = true;
		g_breadcrumbs[i].ThreadId = GetCurrentThreadId();
		g_breadcrumbs[i].StartMs = m_startMs;
		g_breadcrumbs[i].ReportIntervalMs = kFirstReportMs;
		g_breadcrumbs[i].NextReportMs = m_startMs + kFirstReportMs;
		g_breadcrumbs[i].pCategory = pCategory;
		std::memcpy(g_breadcrumbs[i].szLabel, m_szLabel, sizeof(m_szLabel));
		m_slot = i;
		break;
	}

	ReleaseSRWLockExclusive(&g_breadcrumbLock);
}

CScope::~CScope()
{
	if (m_slot >= 0) {
		AcquireSRWLockExclusive(&g_breadcrumbLock);
		g_breadcrumbs[m_slot].bInUse = false;
		ReleaseSRWLockExclusive(&g_breadcrumbLock);
	}

	if (m_startMs == 0 || !g_bEnabled.load(std::memory_order_acquire)) return;

	const u64 elapsedMs = NowMs() - m_startMs;
	if (elapsedMs < kSlowCallMs) return;

	WriteFormatted(m_pCategory, "slow: %s took %llums", m_szLabel, static_cast<unsigned long long>(elapsedMs));
}

u64 CScope::ElapsedMs() const
{
	return m_startMs == 0 ? 0 : NowMs() - m_startMs;
}

} // namespace DebugLog
