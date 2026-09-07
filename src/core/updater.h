#pragma once

#include <atomic>
#include <string>
#include <thread>

#include "core/update_manifest.h"

// The in-app updater: checks a manifest published with this project's GitHub releases,
// downloads and verifies a newer build, and swaps it into place to relaunch into. All off the
// render thread, in the same worker-plus-atomics shape as CLoginAttempt.
//
// The manifest comes from GitHub's stable "latest release" redirect rather than the REST API,
// so this needs no auth token and hits no rate limit.
//
// Trust: HTTPS proves the bytes came from GitHub unmodified. It does not prove that nobody
// with repo write access swapped the asset, and it does nothing against a machine with a
// corporate MITM root CA. So two independent checks run before the running exe is ever
// touched - the SHA-256 of the download against the manifest, then an Ed25519 verify of that
// digest against the embedded public key. The signature covers the digest, so a tampered
// manifest fails too. Either failing aborts; nothing is executed or moved into place until
// both pass.
//
// Threading: the stage and progress counters are plain atomics the worker writes and the
// render thread reads. The manifest and error message are written before the worker's final
// stage store and only read after the corresponding stage is observed.

enum class EUpdateStage : u8 {
	Idle,
	Checking,
	UpToDate,
	Available,

	/// A newer version exists, but this build is too old to auto-update to it. The overlay
	/// points at a manual download instead of offering one.
	ManualUpgradeRequired,

	/// Distinct from Error so a background check failing, before the user has opted into
	/// anything, does not surface the same way a failed in-flight download does.
	CheckFailed,

	Downloading,
	Verifying,
	Installing,
	ReadyToRelaunch,
	Error,
	Cancelled,
};

class CUpdater {
  public:
	~CUpdater();

	void Init();

	/// A no-op while a worker is already active; callers poll IsActive rather than relying on
	/// this to queue.
	void CheckForUpdateAsync(const char *currentVersion);

	/// Only meaningful once the stage reports Available, and reads the manifest the last check
	/// populated.
	void StartDownloadAsync();

	/// Sets the flag the download loop polls between chunks. A no-op past Downloading: verify
	/// and install are sub-second local operations with no meaningful cancellation point, which
	/// is why the overlay stops offering Cancel there.
	void RequestCancel()
	{
		m_bCancelRequested.store(true, std::memory_order_relaxed);
	}

	bool IsActive() const
	{
		return m_bActive;
	}

	EUpdateStage GetStage() const
	{
		return m_stage.load(std::memory_order_acquire);
	}

	const UpdateManifest &GetManifest() const
	{
		return m_manifest;
	}

	u64 GetBytesDownloaded() const
	{
		return m_bytesDownloaded.load(std::memory_order_relaxed);
	}

	u64 GetTotalBytes() const
	{
		return m_totalBytes.load(std::memory_order_relaxed);
	}

	double GetBytesPerSecond() const
	{
		return m_bytesPerSecond.load(std::memory_order_relaxed);
	}

	/// Valid once the stage reports Error or CheckFailed.
	const char *GetErrorMessage() const
	{
		return m_szErrorMessage;
	}

	/// Call once per frame regardless of activity: joins a finished worker and latches the
	/// relaunch flag the frame it first sees ReadyToRelaunch.
	void Update();

	/// True at most once, the frame the verified update was swapped into this exe's path.
	/// The owner should save whatever matters, spawn a fresh process at that path, and exit.
	bool ConsumeReadyToRelaunch();

	/// Recovery for an update interrupted between the two-step fallback's rename calls. Call
	/// exactly once, as the very first thing main does, before any window or asset init.
	///
	/// True means this process should return from main immediately: it either just repaired and
	/// relaunched a working copy of itself, or there was nothing here for it to do. False - the
	/// overwhelmingly common case - means proceed with a completely normal startup.
	static bool RunStartupRecoveryAndMaybeExit();

  private:
	void WorkerCheckForUpdate(std::string currentVersion);
	void WorkerDownloadAndInstall(UpdateManifest manifest);

	/// A worker's last two stores, always in this order.
	void FinishWorker(EUpdateStage stage);
	void FailWorker(EUpdateStage stage, const char *pPrefix, const char *pDetail);

	std::atomic<EUpdateStage> m_stage{EUpdateStage::Idle};
	std::atomic<bool> m_bCancelRequested{false};
	std::thread m_worker;
	bool m_bActive = false;

	/// Separate from m_stage, which can hold a terminal-looking value left over
	/// from the previous worker - Available from a finished check - while the download worker
	/// that value just started is still running. Joining on the stage alone would park the
	/// render thread on that brand new worker.
	std::atomic<bool> m_bWorkerFinished{false};

	std::atomic<u64> m_bytesDownloaded{0};
	std::atomic<u64> m_totalBytes{0};
	std::atomic<double> m_bytesPerSecond{0.0};

	UpdateManifest m_manifest{};
	char m_szErrorMessage[256]{};

	bool m_bReadyToRelaunchLatched = false; // set in Update, so render-thread-only
};
