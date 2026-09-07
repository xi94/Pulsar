#pragma once

#include <atomic>
#include <chrono>
#include <string>
#include <string_view>

#include <Windows.h>

#include "core/ui_automation.h"

/// Drives the Riot Client from the outside: kill, launch, then a UI Automation login that fills
/// the form and submits it, exactly as a person clicking through the client would.
///
/// Always kill-then-launch. An already-running client can be sitting logged in on its library
/// page, which has no login form at all.
///
/// Window lookups match on the title. The client is a multi-process Electron app, so the process
/// CreateProcessW returns is not the one that owns the window.
///
/// Not thread-safe and does not own a CUiAutomation: that interface is thread-affine, so each
/// worker constructs one locally and passes it in.
class CRiotClient {
  public:
	/// Passed as a timeout to WaitForWindow to poll without a deadline. A cold client start is
	/// slow by an amount no fixed number predicts, so the user's Cancel is the real bound.
	static constexpr u32 kWaitForeverMs = 0xFFFFFFFFu;

	~CRiotClient();

	/// Whether VALORANT's or League's game process is running, as opposed to the client shells.
	/// Checked before any kill so a relaunch never interrupts a live match.
	static bool IsGameInProgress();

	/// Terminates every client shell process and waits for them to actually exit. Best effort.
	///
	/// The wait matters: TerminateProcess only requests termination, so returning early races
	/// the dying client's single-instance lock and the new instance silently hands off to it.
	static void KillAllClientProcesses();

	/// Maps a carousel banner title to the client's --launch-product value. An unrecognized
	/// title returns empty, which Launch treats as "just open the client".
	static std::string_view LaunchProductForBannerTitle(std::string_view bannerTitle);

	/// Reads the "rc_default" key from RiotClientInstalls.json. False on a machine with no Riot
	/// Client installed.
	bool ResolveExecutablePath();

	const std::wstring &GetExecutablePath() const
	{
		return m_executablePath;
	}

	/// Starts the client, waiting only on CreateProcessW. A non-empty launchProduct opens it
	/// straight onto that product's page.
	bool Launch(std::string_view launchProduct);

	bool IsRunning() const;

	/// Polls until the client window exists and its thread is pumping messages. The client shows
	/// a transient splash window with the same title first, so nothing here caches the result.
	bool WaitForWindow(u32 timeoutMs, const std::atomic<bool> *pCancelRequested = nullptr) const;

	/// Brings the client forward, with the AttachThreadInput trick a background process needs.
	///
	/// Best effort. Each Win32 call is an unbounded trip through the client's window
	/// procedure, so the pass runs on a throwaway thread that is abandoned after a few seconds.
	/// False means the client did not come forward, never that the caller is stuck.
	bool BringToForeground(u32 timeoutMs = kDefaultActionTimeoutMs,
						   const std::atomic<bool> *pCancelRequested = nullptr) const;

	/// BringToForeground plus SetFocus, so synthesized keystrokes have somewhere to land.
	bool SetKeyboardFocus(u32 timeoutMs = kDefaultActionTimeoutMs,
						  const std::atomic<bool> *pCancelRequested = nullptr) const;

	/// Fills in and submits the login form; uiAutomation must be initialized on this thread.
	/// True means the form was submitted, not that the login succeeded.
	bool SubmitLogin(const CUiAutomation &uiAutomation, std::string_view username, std::string_view password,
					 u32 timeoutMs, const std::atomic<bool> *pCancelRequested = nullptr) const;

	/// Polls for the client's inline login-error tooltip. False means no error appeared in time
	/// or the wait was cancelled - it is not proof of success.
	///
	/// pIgnoreMessage covers a resubmit race: the previous attempt's tooltip can still be on
	/// screen when polling restarts, so a tooltip matching it is only accepted once its text
	/// changes or it has been seen gone. Pass nullptr on a first attempt.
	bool WaitForLoginError(const CUiAutomation &uiAutomation, std::wstring &outMessage, u32 timeoutMs,
						   const std::atomic<bool> *pCancelRequested = nullptr,
						   const std::wstring *pIgnoreMessage = nullptr) const;

	/// Finds and presses the Play button on the logged-in library page, which is real
	/// confirmation of a successful login since it never exists on the login page. Pressing it
	/// is required: --launch-product now only navigates to the page instead of starting the game.
	bool WaitForPlayButtonAndClick(const CUiAutomation &uiAutomation, u32 timeoutMs,
								   const std::atomic<bool> *pCancelRequested = nullptr) const;

  private:
	/// Short, since callers have already seen a window once, but long enough to ride out the
	/// splash-to-real-window transition.
	static constexpr u32 kDefaultActionTimeoutMs = 5000;

	/// Title match first, falling back to a process-tree search if the title ever changes.
	HWND FindClientWindow() const;

	/// FindClientWindow polled until the window exists and its thread is pumping messages.
	HWND WaitForResponsiveClientWindow(u32 timeoutMs, const std::atomic<bool> *pCancelRequested) const;

	/// The current window as an automation element, briefly cached and re-resolved whenever the
	/// HWND changes.
	CUiElement CurrentWindowElement(const CUiAutomation &uiAutomation) const;

	/// A cache, not state, so every method that touches it stays const.
	mutable HWND m_cachedWindowHandle = nullptr;
	mutable CUiElement m_cachedWindowElement;
	mutable std::chrono::steady_clock::time_point m_cachedWindowExpiry{};

	std::wstring m_executablePath;
	HANDLE m_hProcess = nullptr;
	u32 m_processId = 0;
};
