#pragma once

/// A runtime-gated diagnostic log: on by default in a Debug build, switched on in any build
/// by setting PULSAR_DEBUG_LOG, and a single predictable branch away from free when off.
///
/// Not compiled out even in release. The bug class this exists for - a login worker wedging
/// inside a cross-process call it does not control the other end of - is timing dependent and
/// does not reliably reproduce in a Debug build, so turning the same instrumentation on in a
/// shipped build without rebuilding is the entire point.
///
/// Three pieces, and the third is what actually finds a hang:
///
///   1. Write, a timestamped and categorised line, flushed per line. A hung process gets
///      force-killed from Task Manager, which flushes nothing - so anything left in a stdio
///      buffer is exactly the part that would have said where it hung.
///
///   2. CScope, an RAII breadcrumb around a call that could block. Logs nothing on entry and
///      only reports on exit if the call was slow, so a 100ms poll loop does not drown out a
///      single nine-second stall.
///
///   3. The watchdog thread, which reports any CScope still open past its escalating
///      deadline, notices the render thread missing frames, and writes one diagnostic
///      minidump the first time anything crosses the hang threshold.
namespace DebugLog {

/// Opens the log file and starts the watchdog. A second call does nothing.
void Init();

/// Stops the watchdog and closes the file. Safe with or without a prior Init.
void Shutdown();

bool IsEnabled();

/// The current run's log path in UTF-8, or empty when logging is off.
const char *GetFilePath();

/// pCategory is a short tag ("uia", "riot", "login", "app") padded into its own column so the
/// log stays greppable by subsystem.
void Write(const char *pCategory, const char *pFormat, ...);

/// Called once per frame from the render thread - the watchdog's only way to tell a frozen UI
/// apart from a busy worker. Never call it from a worker.
void MarkUiThreadAlive();

/// Construct one directly above the call that can block, not around a whole function, so the
/// label names the thing that actually blocks:
///
///     const DebugLog::CScope scope("uia", "FindFirst(Name=%ls)", pName);
///
/// Copying would break the slot bookkeeping the watchdog reads.
class CScope {
  public:
	CScope(const char *pCategory, const char *pFormat, ...);
	~CScope();

	CScope(const CScope &) = delete;
	CScope &operator=(const CScope &) = delete;

	u64 ElapsedMs() const;

  private:
	const char *m_pCategory = nullptr;
	i32 m_slot = -1; // -1 when logging is off, or when every slot was taken
	u64 m_startMs = 0;
	char m_szLabel[160]{};
};

} // namespace DebugLog
