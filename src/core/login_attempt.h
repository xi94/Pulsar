#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

#include "core/riot_client.h"
#include <string_view>

// One login attempt, driven on a background worker.
//
// No mutex: a single-producer worker and single-consumer render thread need nothing beyond an
// atomic. The worker's terminal Stage store is the one synchronization point, and szMessage is
// written before it and only read after it, so the release/acquire pairing on Stage is what
// makes the message safe to read.
//
// Cancel is a real interrupt, not a UI-only "stop showing this": the worker's CRiotClient calls
// check the flag on every poll iteration, so a click stops whichever wait is in flight within
// about one poll interval rather than letting the worker keep killing processes and typing
// credentials in the background.

enum class ELoginStage : u8 {
	Idle,
	WaitingForProcess,
	Connecting,
	Authenticating,
	Launching,
	Success,
	Error,
	Cancelled, // distinct from Error so a deliberate cancel does not flash red
};

/// Heap-allocated and co-owned by CLoginAttempt and its worker, not plain members
/// the worker points into.
///
/// That co-ownership is what lets CLoginAttempt give up on a worker it can neither join nor
/// interrupt. An abandoned worker keeps writing into the block it already co-owns and frees it
/// when it eventually unwinds, while the next attempt gets a brand new one - so a zombie can
/// never overwrite a live attempt's state or share its CRiotClient.
struct LoginAttemptState {
	static constexpr u32 kMaxMessageLength = 160;

	std::atomic<ELoginStage> Stage{ELoginStage::Idle};
	std::atomic<bool> bCancelRequested{false};

	/// The worker's very last act, after its terminal Stage store - a terminal stage still
	/// leaves it unwinding its CUiAutomation, which is what Update joins on.
	std::atomic<bool> bWorkerFinished{false};

	/// Sized to match Account's fields. Copied in Start, read only by the worker.
	char szUsername[64]{};
	char szPassword[128]{};
	char szGameTitle[32]{};

	char szMessage[kMaxMessageLength]{};

	CRiotClient RiotClient;
};

class CLoginAttempt {
  public:
	/// Requests cancellation, then joins with a bound. std::thread's destructor terminates
	/// the process against a still-joinable thread, which was a real observed crash when the app
	/// closed before a worker had noticed a cancel.
	~CLoginAttempt();

	void Init();

	/// Starts a worker that kills every known client process, launches fresh, waits for the
	/// window, submits the credentials, and watches for the client's inline error.
	///
	/// A no-op while a previous attempt on this object is still active. Blocking here instead
	/// would mean joining a worker that may be stuck deep inside one slow UI Automation call
	/// with no cancellation point - a real observed freeze of the whole app. Callers gate on
	/// IsActive rather than relying on this to queue.
	///
	/// Copies the credentials into this attempt's buffers rather than holding the caller's
	/// views across the thread boundary: the account could be deleted mid-login.
	void Start(std::string_view username, std::string_view password, std::string_view gameTitle);

	/// Requests that the in-flight attempt stop the next time it checks. Does not join - Update
	/// still does that - so IsActive keeps reporting true until the worker actually stops.
	///
	/// Also starts Update's grace period: a healthy worker acknowledges within a poll interval,
	/// so one that does not is wedged somewhere it cannot be interrupted from, and the user has
	/// already said they are done waiting.
	void Cancel();

	/// True from Start until Update disposes of the worker, by joining a finished one or
	/// abandoning one that blew its cancel grace period. Not the same as "still doing real work"
	/// the instant Cancel is called.
	bool IsActive() const
	{
		return m_bActive;
	}

	/// The one place the single-producer/single-consumer contract is enforced - never read the
	/// state block's Stage directly.
	ELoginStage GetStage() const;

	/// Valid once GetStage reports a terminal stage. Empty on Success and Cancelled unless
	/// something notable happened; always non-empty on Error.
	std::string_view GetTerminalMessage() const;

	static bool IsTerminalStage(ELoginStage stage);

	/// Call once per frame whether or not anything is showing this attempt's progress. Joins
	/// only a worker that has already signalled it finished, so the join is always effectively
	/// instantaneous and this never blocks the caller.
	void Update();

  private:
	/// Gives up on a worker that was cancelled but never acknowledged it. Detaches the OS
	/// thread, hands it sole ownership of the block it is still writing into, and swaps in a
	/// fresh block already carrying the terminal result - so the next Start is free to run.
	void AbandonWorker();

	std::shared_ptr<LoginAttemptState> m_pState;
	std::thread m_worker;
	bool m_bActive = false;

	/// Only meaningful once a cancel has been requested.
	std::chrono::steady_clock::time_point m_cancelDeadline{};
};
