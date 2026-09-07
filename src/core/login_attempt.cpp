#include "core/login_attempt.h"

#include "core/str.h"

#include <chrono>
#include <memory>
#include <string>
#include <utility>

#include <Windows.h>

#include "core/debug_log.h"
#include "core/thread_util.h"
#include "core/ui_automation.h"

namespace {
constexpr const char *kLogCategory = "login";

constexpr u32 kFormTimeoutMs = 10000;
constexpr u32 kResultTimeoutMs = 6000;
constexpr u32 kForegroundTimeoutMs = 5000;
constexpr u32 kFocusTimeoutMs = 5000;

// Generous but not load-bearing: the login already went through by the time this runs, so a
// timeout here only means the game was left unlaunched.
constexpr u32 kPlayButtonTimeoutMs = 8000;

// A healthy worker notices a cancel within one 100ms poll interval.
constexpr auto kCancelGracePeriod = std::chrono::milliseconds(5000);

// Riot's wording is never shown verbatim: it is not stable across client versions or locales.
// Wrong credentials is the one case worth calling out, since it is the only actionable one.
constexpr const char *kInvalidCredentialsMessage = "Invalid username or password.";
constexpr const char *kServerErrorMessage =
	"Something went wrong - Riot's servers might be overloaded. Try again in a moment.";
constexpr const char *kGameInProgressMessage = "A game is already running - close it before switching accounts.";
constexpr const char *kNoRiotClientMessage = "Couldn't find the Riot Client - is it installed?";
constexpr const char *kLaunchFailedMessage = "Couldn't launch the Riot Client.";
constexpr const char *kWindowTimeoutMessage = "The Riot Client didn't respond in time.";
constexpr const char *kFormTimeoutMessage = "Couldn't find the Riot Client's login form.";
constexpr const char *kUnresponsiveClientMessage = "The Riot Client stopped responding - try again.";

// Distinct from kServerErrorMessage: automation failing to start is a problem with this
// machine's accessibility stack, not with Riot.
constexpr const char *kAutomationFailedMessage = "Couldn't start Windows UI Automation - try again.";

enum class ESubmitResult : u8 {
	FormNotFound,
	ErrorShown,	  // the client's inline error tooltip appeared
	NoErrorShown, // nothing appeared in time - the "assume success" inference
};

const char *StageName(ELoginStage stage)
{
	switch (stage) {
		case ELoginStage::Idle:
			return "IDLE";
		case ELoginStage::WaitingForProcess:
			return "WAITING_FOR_PROCESS";
		case ELoginStage::Connecting:
			return "CONNECTING";
		case ELoginStage::Authenticating:
			return "AUTHENTICATING";
		case ELoginStage::Launching:
			return "LAUNCHING";
		case ELoginStage::Success:
			return "SUCCESS";
		case ELoginStage::Error:
			return "ERROR";
		case ELoginStage::Cancelled:
			return "CANCELLED";
	}

	return "?";
}

// Every stage change goes through here rather than storing directly, so the log shows the same
// sequence the UI sees. Release ordering on every stage, not just the terminal ones.
void StoreStage(LoginAttemptState &state, ELoginStage stage)
{
	DebugLog::Write(kLogCategory, "stage -> %s%s", StageName(stage),
					state.szMessage[0] != '\0' ? " (with a message)" : "");
	state.Stage.store(stage, std::memory_order_release);
}

void StoreError(LoginAttemptState &state, const char *pMessage)
{
	CopyTo(pMessage, state.szMessage, LoginAttemptState::kMaxMessageLength);
	StoreStage(state, ELoginStage::Error);
}

bool IsCancelled(const LoginAttemptState &state)
{
	return state.bCancelRequested.load(std::memory_order_relaxed);
}

// True once the attempt has been stopped, so a caller can `if (StoreIfCancelled(state)) return;`
// at each of the flow's checkpoints.
bool StoreIfCancelled(LoginAttemptState &state)
{
	if (!IsCancelled(state)) return false;

	StoreStage(state, ELoginStage::Cancelled);

	return true;
}

// A CRiotClient poll returning false means either "the user cancelled" or "this step genuinely
// failed", and the worker cannot tell which without re-checking the flag itself.
void StoreCancelledOrError(LoginAttemptState &state, const char *pFailureMessage)
{
	if (!StoreIfCancelled(state)) {
		StoreError(state, pFailureMessage);
	}
}

// A plain substring check is enough: among every message the client has been observed to
// surface, this is the only one containing the word at all.
bool LooksLikeInvalidCredentials(const std::wstring &wide)
{
	return wide.find(L"credentials") != std::wstring::npos;
}

// pIgnorePreviousMessage is only set for the retry. Without it the retry can read the first
// attempt's still-on-screen tooltip as its own result before the client replaces it.
ESubmitResult SubmitAndWaitForResult(LoginAttemptState &state, CUiAutomation &uiAutomation,
									 std::wstring &outErrorMessage,
									 const std::wstring *pIgnorePreviousMessage = nullptr)
{
	outErrorMessage.clear();

	if (!state.RiotClient.SubmitLogin(uiAutomation, state.szUsername, state.szPassword, kFormTimeoutMs,
									  &state.bCancelRequested)) {
		return ESubmitResult::FormNotFound;
	}

	if (state.RiotClient.WaitForLoginError(uiAutomation, outErrorMessage, kResultTimeoutMs, &state.bCancelRequested,
										   pIgnorePreviousMessage)) {
		return ESubmitResult::ErrorShown;
	}

	return ESubmitResult::NoErrorShown;
}

// Kills every known client process and launches a fresh one, then waits for its window. False
// means this stored its own terminal stage and the caller should return.
bool StartFreshClient(LoginAttemptState &state)
{
	// Checked before the kill rather than after, since there is nothing to undo if it bails.
	if (CRiotClient::IsGameInProgress()) {
		StoreError(state, kGameInProgressMessage);
		return false;
	}

	if (StoreIfCancelled(state)) return false;

	// Always kill and relaunch, never reuse: an already-running client can be sitting logged in
	// on its library page, which has no login form at all.
	CRiotClient::KillAllClientProcesses();

	if (!state.RiotClient.ResolveExecutablePath()) {
		StoreError(state, kNoRiotClientMessage);
		return false;
	}

	if (!state.RiotClient.Launch(CRiotClient::LaunchProductForBannerTitle(state.szGameTitle))) {
		StoreError(state, kLaunchFailedMessage);
		return false;
	}

	// Unbounded, unlike every other step here. A cold Electron start depends on the machine, its
	// disk, and whether the client patches itself first, so any timeout is a guess at someone
	// else's hardware. The user's Cancel is the bound instead, which means a false return here
	// only ever means cancelled.
	if (!state.RiotClient.WaitForWindow(CRiotClient::kWaitForeverMs, &state.bCancelRequested)) {
		StoreCancelledOrError(state, kWindowTimeoutMessage);
		return false;
	}

	return !StoreIfCancelled(state);
}

// Brings the client forward and gives it keyboard focus. Both are best effort by design; only
// cancellation stops the flow here.
bool FocusClientForInput(LoginAttemptState &state)
{
	StoreStage(state, ELoginStage::Connecting);

	state.RiotClient.BringToForeground(kForegroundTimeoutMs, &state.bCancelRequested);
	state.RiotClient.SetKeyboardFocus(kFocusTimeoutMs, &state.bCancelRequested);

	return !StoreIfCancelled(state);
}

// A generic "trouble signing you in" error - never wrong credentials, which resubmitting cannot
// fix - clears up on one immediate retry against the same still-open client. The one place a
// retry without a fresh relaunch is correct.
bool ShouldRetryAfterError(ESubmitResult result, const std::wstring &errorMessage)
{
	return result == ESubmitResult::ErrorShown && !LooksLikeInvalidCredentials(errorMessage);
}

// Submits, retries once if the failure looks transient, and stores the terminal stage on any
// outcome that ends the attempt. False means the caller should return.
bool Authenticate(LoginAttemptState &state, CUiAutomation &uiAutomation)
{
	StoreStage(state, ELoginStage::Authenticating);

	std::wstring errorMessage;
	ESubmitResult result = SubmitAndWaitForResult(state, uiAutomation, errorMessage);

	if (result == ESubmitResult::FormNotFound) {
		StoreCancelledOrError(state, uiAutomation.HasWedged() ? kUnresponsiveClientMessage : kFormTimeoutMessage);
		return false;
	}

	if (ShouldRetryAfterError(result, errorMessage)) {
		if (!FocusClientForInput(state)) return false;

		StoreStage(state, ELoginStage::Authenticating);

		const std::wstring previousErrorMessage = errorMessage;
		result = SubmitAndWaitForResult(state, uiAutomation, errorMessage, &previousErrorMessage);

		if (result == ESubmitResult::FormNotFound) {
			StoreCancelledOrError(state, uiAutomation.HasWedged() ? kUnresponsiveClientMessage : kFormTimeoutMessage);
			return false;
		}
	}

	if (result == ESubmitResult::ErrorShown) {
		StoreError(state, LooksLikeInvalidCredentials(errorMessage) ? kInvalidCredentialsMessage : kServerErrorMessage);
		return false;
	}

	// Before the "no error appeared, so it worked" inference below: a client whose provider
	// stopped answering produces the same absence as a login that succeeded.
	if (uiAutomation.HasWedged()) {
		DebugLog::Write(kLogCategory, "UI Automation gave up on the client - refusing to infer success from silence");
		StoreError(state, kUnresponsiveClientMessage);
		return false;
	}

	return !StoreIfCancelled(state);
}

void RunLoginAttempt(LoginAttemptState &state)
{
	state.szMessage[0] = '\0';
	StoreStage(state, ELoginStage::WaitingForProcess);

	if (!StartFreshClient(state) || !FocusClientForInput(state)) return;

	// Fresh per attempt, because this runs on a brand new OS thread every time and apartment
	// membership belongs to the thread.
	CUiAutomation uiAutomation;
	if (!uiAutomation.Init()) {
		DebugLog::Write(kLogCategory, "CUiAutomation::Init failed - see the uia lines just above for the HRESULT");
		StoreError(state, kAutomationFailedMessage);
		return;
	}

	if (!Authenticate(state, uiAutomation)) return;

	// The login has already succeeded by this point, so a Play button that never turns up just
	// leaves the game unlaunched rather than failing the attempt.
	StoreStage(state, ELoginStage::Launching);
	state.RiotClient.WaitForPlayButtonAndClick(uiAutomation, kPlayButtonTimeoutMs, &state.bCancelRequested);

	if (StoreIfCancelled(state)) return;

	StoreStage(state, ELoginStage::Success);
}

// Co-owns the state block it drives so abandoning this thread mid-call cannot leave it writing
// into freed memory.
void WorkerMain(std::shared_ptr<LoginAttemptState> pState)
{
	DebugLog::Write(kLogCategory, "worker started");
	RunLoginAttempt(*pState);

	// Set here, not at each of RunLoginAttempt's returns, so an early-out added later cannot
	// forget it.
	pState->bWorkerFinished.store(true, std::memory_order_release);

	// The gap between this line and the one above is the CUiAutomation unwind. A log ending at
	// "worker finished" with no "unwinding" after it is that teardown hanging.
	DebugLog::Write(kLogCategory, "worker finished (stage %s), unwinding",
					StageName(pState->Stage.load(std::memory_order_acquire)));
}
} // namespace

CLoginAttempt::~CLoginAttempt()
{
	if (m_pState != nullptr) {
		m_pState->bCancelRequested.store(true, std::memory_order_relaxed);
	}

	if (m_worker.joinable()) {
		DebugLog::Write(kLogCategory, "shutting down with a worker still running - cancelling and joining");
	}

	// Bounded rather than a bare join: a worker stuck inside a UI Automation call would otherwise
	// hang shutdown. Detaching is safe because the worker co-owns its state block.
	const DebugLog::CScope scope(kLogCategory, "shutdown join of the login worker");
	JoinWithTimeoutOrDetach(m_worker, std::chrono::milliseconds(3000));
}

void CLoginAttempt::Init()
{
	// Does not touch m_worker: dropping this object's reference is the whole reset, and the block
	// stays alive under the worker's own reference.
	m_pState.reset();
	m_bActive = false;
}

bool CLoginAttempt::IsTerminalStage(ELoginStage stage)
{
	return stage == ELoginStage::Success || stage == ELoginStage::Error || stage == ELoginStage::Cancelled;
}

void CLoginAttempt::Start(std::string_view username, std::string_view password, std::string_view gameTitle)
{
	if (m_bActive && !IsTerminalStage(GetStage())) {
		DebugLog::Write(kLogCategory, "Start refused - the previous attempt is still active (stage %s)",
						StageName(GetStage()));
		return;
	}

	// The instantaneous case. The bound only covers a worker that stored its terminal stage but
	// is still unwinding.
	JoinWithTimeoutOrDetach(m_worker, std::chrono::milliseconds(50));
	m_bActive = false;

	auto pState = std::make_shared<LoginAttemptState>();
	CopyTo(username, pState->szUsername, sizeof(pState->szUsername));
	CopyTo(password, pState->szPassword, sizeof(pState->szPassword));
	CopyTo(gameTitle, pState->szGameTitle, sizeof(pState->szGameTitle));
	pState->Stage.store(ELoginStage::WaitingForProcess, std::memory_order_relaxed);

	m_pState = pState;
	m_worker = std::thread(WorkerMain, std::move(pState));
	m_bActive = true;

	DebugLog::Write(kLogCategory, "attempt started for game \"%s\"", m_pState->szGameTitle);
}

void CLoginAttempt::Cancel()
{
	if (!m_bActive || m_pState == nullptr || IsTerminalStage(GetStage())) return;

	DebugLog::Write(kLogCategory, "cancel requested at stage %s", StageName(GetStage()));
	m_pState->bCancelRequested.store(true, std::memory_order_relaxed);
	m_cancelDeadline = std::chrono::steady_clock::now() + kCancelGracePeriod;
}

ELoginStage CLoginAttempt::GetStage() const
{
	return m_pState != nullptr ? m_pState->Stage.load(std::memory_order_acquire) : ELoginStage::Idle;
}

std::string_view CLoginAttempt::GetTerminalMessage() const
{
	return m_pState != nullptr ? std::string_view{m_pState->szMessage} : std::string_view{""};
}

void CLoginAttempt::AbandonWorker()
{
	// Read before the store below erases the distinction: a cancel the worker never acknowledged
	// should still land as Cancelled rather than as an error.
	const bool userCancelled = m_pState != nullptr && m_pState->bCancelRequested.load(std::memory_order_relaxed);

	if (m_pState != nullptr) {
		// It may come back to life long after this, and the first thing it checks should say stop.
		m_pState->bCancelRequested.store(true, std::memory_order_relaxed);
	}

	if (m_worker.joinable()) {
		// Whichever DebugLog breadcrumb is still open on that thread names the wedged call.
		DebugLog::Write(kLogCategory, "ABANDONING the login worker - it never acknowledged the cancel (stage %s)",
						StageName(GetStage()));
		m_worker.detach();
	}

	auto pAbandoned = std::make_shared<LoginAttemptState>();
	if (userCancelled) {
		pAbandoned->Stage.store(ELoginStage::Cancelled, std::memory_order_relaxed);
	} else {
		CopyTo(kUnresponsiveClientMessage, pAbandoned->szMessage, LoginAttemptState::kMaxMessageLength);
		pAbandoned->Stage.store(ELoginStage::Error, std::memory_order_relaxed);
	}

	pAbandoned->bWorkerFinished.store(true, std::memory_order_relaxed);

	m_pState = std::move(pAbandoned);
	m_bActive = false;
}

void CLoginAttempt::Update()
{
	if (!m_bActive || m_pState == nullptr) return;

	// bWorkerFinished rather than a terminal stage: the worker stores that before unwinding its
	// CUiAutomation, and joining in that window would park the render thread on the teardown.
	if (m_pState->bWorkerFinished.load(std::memory_order_acquire)) {
		// Bounded and on the render thread: if this ever reports slow, the UI hitched.
		const DebugLog::CScope scope(kLogCategory, "render-thread join of a finished login worker");
		JoinWithTimeoutOrDetach(m_worker, std::chrono::milliseconds(50));
		m_bActive = false;

		DebugLog::Write(kLogCategory, "attempt retired (final stage %s)", StageName(GetStage()));

		return;
	}

	// Only after a requested cancel goes unacknowledged past its grace period - never purely
	// for running long.
	if (m_pState->bCancelRequested.load(std::memory_order_relaxed) &&
		std::chrono::steady_clock::now() >= m_cancelDeadline) {
		AbandonWorker();
	}
}
