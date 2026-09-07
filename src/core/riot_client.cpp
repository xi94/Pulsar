#include "core/riot_client.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <thread>
#include <vector>

#include <TlHelp32.h>

#include <nlohmann/json.hpp>

#include "core/debug_log.h"
#include "core/thread_util.h"

namespace {
constexpr const char *kLogCategory = "riot";

constexpr const char *kInstallsJsonPath = "C:\\ProgramData\\Riot Games\\RiotClientInstalls.json";
constexpr const char *kExecutablePathKey = "rc_default";

// Every name below was confirmed via Inspect against a real install.
constexpr const wchar_t *kClientWindowTitle = L"Riot Client";
constexpr const wchar_t *kUsernameFieldName = L"USERNAME";
constexpr const wchar_t *kPasswordFieldName = L"PASSWORD";
constexpr const wchar_t *kPlayButtonName = L"Play";

// The generic heading shown for every login failure. It does not say which one happened - the
// reason is a separate element, matched independently.
constexpr const wchar_t *kLoginErrorToolTipName = L"Login error";

// The two failure reasons seen so far. Riot may have others; anything else falls back to the
// generic heading above.
constexpr const wchar_t *kInvalidCredentialsReasonText =
	L"Your login credentials don't match an account in our system.";
constexpr const wchar_t *kTroubleSigningInReasonText =
	L"Sorry, we're having trouble signing you in right now. Please try again later.";

constexpr const wchar_t *const kClientProcessNames[]{
	L"Riot Client.exe",	 L"RiotClientServices.exe", L"RiotClientUx.exe", L"RiotClientUxRender.exe",
	L"LeagueClient.exe", L"LeagueClientUx.exe",		L"LoR.exe",
};

// The in-match processes a kill must never touch. "League of Legends.exe" is the real game,
// distinct from LeagueClient.exe's lobby launcher, which is safe to kill.
constexpr const wchar_t *const kGameProcessNames[]{
	L"VALORANT-Win64-Shipping.exe",
	L"League of Legends.exe",
};

// Generous: a client tree normally unwinds well inside this.
constexpr auto kProcessExitTimeout = std::chrono::milliseconds(3000);

constexpr u32 kResponsiveProbeTimeoutMs = 750;
constexpr auto kActivateTimeout = std::chrono::milliseconds(3000);
constexpr auto kPollInterval = std::chrono::milliseconds(100);

// Ordinal rather than culture-aware: process image names are ASCII.
bool ProcessNameEquals(const wchar_t *pExeFileName, const wchar_t *pTarget)
{
	return CompareStringOrdinal(pExeFileName, -1, pTarget, -1, TRUE) == CSTR_EQUAL;
}

bool MatchesAnyName(const wchar_t *pExeFileName, const wchar_t *const *pNames, usize nameCount)
{
	for (usize i = 0; i < nameCount; i += 1) {
		if (ProcessNameEquals(pExeFileName, pNames[i])) return true;
	}

	return false;
}

bool AnyProcessNameMatches(const wchar_t *const *pNames, usize nameCount)
{
	const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == INVALID_HANDLE_VALUE) return false;

	bool found = false;
	PROCESSENTRY32W entry{.dwSize = sizeof(entry)};

	if (Process32FirstW(snapshot, &entry)) {
		do {
			found = MatchesAnyName(entry.szExeFile, pNames, nameCount);
		} while (!found && Process32NextW(snapshot, &entry));
	}

	CloseHandle(snapshot);

	return found;
}

// A window whose owning process is not the one Launch started means this latched onto a dying
// client's about-to-vanish handle.
void LogWindowIdentity(const char *pWhat, HWND hWnd)
{
	if (!DebugLog::IsEnabled()) return;

	if (hWnd == nullptr) {
		DebugLog::Write(kLogCategory, "%s: no client window found", pWhat);
		return;
	}

	DWORD processId = 0;
	const DWORD threadId = GetWindowThreadProcessId(hWnd, &processId);

	wchar_t title[128]{};
	GetWindowTextW(hWnd, title, ARRAYSIZE(title));

	DebugLog::Write(kLogCategory, "%s: hwnd=0x%p owned by pid %lu / thread t%lu, title \"%ls\"", pWhat, hWnd, processId,
					threadId, title);
}

// Waits for every handle against one shared deadline, batched because WaitForMultipleObjects
// caps out below the process count a Riot Client tree mid-teardown can reach. Consumes them
// either way.
void WaitForProcessesToExit(std::vector<HANDLE> &processes, std::chrono::milliseconds timeout)
{
	const auto deadline = std::chrono::steady_clock::now() + timeout;

	bool bTimedOut = false;
	for (usize offset = 0; offset < processes.size(); offset += MAXIMUM_WAIT_OBJECTS) {
		const auto count = static_cast<DWORD>(std::min<usize>(MAXIMUM_WAIT_OBJECTS, processes.size() - offset));
		const auto now = std::chrono::steady_clock::now();
		const DWORD remainingMs =
			now >= deadline
				? 0
				: static_cast<DWORD>(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count());

		bTimedOut =
			WaitForMultipleObjects(count, processes.data() + offset, TRUE, remainingMs) == WAIT_TIMEOUT || bTimedOut;
	}

	if (bTimedOut) {
		DebugLog::Write(kLogCategory, "TIMED OUT waiting for killed processes to exit - launching anyway");
	}

	for (const HANDLE process : processes) {
		CloseHandle(process);
	}

	processes.clear();
}

// SYNCHRONIZE alongside PROCESS_TERMINATE purely so the handle can be waited on afterwards.
std::vector<HANDLE> TerminateMatchingProcesses(const wchar_t *const *pNames, usize nameCount)
{
	std::vector<HANDLE> terminated;

	const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == INVALID_HANDLE_VALUE) return terminated;

	PROCESSENTRY32W entry{.dwSize = sizeof(entry)};
	if (Process32FirstW(snapshot, &entry)) {
		do {
			if (!MatchesAnyName(entry.szExeFile, pNames, nameCount)) continue;

			const HANDLE process = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, entry.th32ProcessID);
			if (process == nullptr) continue;

			if (TerminateProcess(process, 0)) {
				DebugLog::Write(kLogCategory, "terminated %ls (pid %lu)", entry.szExeFile, entry.th32ProcessID);
				terminated.push_back(process);
			} else {
				DebugLog::Write(kLogCategory, "TerminateProcess FAILED for %ls (pid %lu), err=%lu", entry.szExeFile,
								entry.th32ProcessID, GetLastError());
				CloseHandle(process);
			}
		} while (Process32NextW(snapshot, &entry));
	}

	CloseHandle(snapshot);

	return terminated;
}

void KillProcessesByName(const wchar_t *const *pNames, usize nameCount)
{
	std::vector<HANDLE> terminated = TerminateMatchingProcesses(pNames, nameCount);

	const DebugLog::CScope scope(kLogCategory, "wait for %zu killed client process(es) to exit", terminated.size());
	WaitForProcessesToExit(terminated, kProcessExitTimeout);

	DebugLog::Write(kLogCategory, "killed client processes gone after %llums",
					static_cast<unsigned long long>(scope.ElapsedMs()));
}

std::wstring Utf8ToWide(std::string_view text)
{
	if (text.empty()) return std::wstring{};

	const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
	if (length <= 0) return std::wstring{};

	std::wstring wide(static_cast<usize>(length), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), length);

	return wide;
}

bool IsCancelled(const std::atomic<bool> *pCancelRequested)
{
	return pCancelRequested != nullptr && pCancelRequested->load(std::memory_order_relaxed);
}

// The standard WM_NULL hung-app probe. AttachThreadInput hangs outright against a thread that is
// not pumping, and the client's window matches by title seconds before its UI thread starts.
bool IsWindowResponsive(HWND hWnd, u32 timeoutMs)
{
	const DebugLog::CScope scope(kLogCategory, "WM_NULL responsiveness probe (hwnd=0x%p)", hWnd);

	DWORD_PTR probeResult = 0;

	return SendMessageTimeoutW(hWnd, WM_NULL, 0, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK, timeoutMs, &probeResult) != 0;
}

// Windows silently ignores SetForegroundWindow and SetFocus aimed at a window whose thread is
// not part of the caller's input queue, so activation has to attach first.
//
// Only ever constructed on ActivateWindowBounded's throwaway thread: both AttachThreadInput
// calls are unbounded trips into the target's thread, and the probe above does not guard the
// detach - once attached the two threads share one input queue.
class CScopedThreadInputAttach {
  public:
	explicit CScopedThreadInputAttach(HWND hWnd)
	{
		if (!IsWindowResponsive(hWnd, kResponsiveProbeTimeoutMs)) {
			DebugLog::Write(kLogCategory, "activation: target window is not pumping - skipping AttachThreadInput");
			return;
		}

		m_targetThreadId = GetWindowThreadProcessId(hWnd, nullptr);
		m_currentThreadId = GetCurrentThreadId();

		if (m_targetThreadId != 0 && m_targetThreadId != m_currentThreadId) {
			const DebugLog::CScope scope(kLogCategory, "AttachThreadInput(TRUE) to t%lu", m_targetThreadId);
			m_bAttached = AttachThreadInput(m_currentThreadId, m_targetThreadId, TRUE) != 0;
		}

		DebugLog::Write(kLogCategory, "activation: input queue %s target thread t%lu",
						m_bAttached ? "attached to" : "NOT attached to", m_targetThreadId);
	}

	~CScopedThreadInputAttach()
	{
		if (m_bAttached) {
			const DebugLog::CScope scope(kLogCategory, "AttachThreadInput(FALSE) from t%lu", m_targetThreadId);
			AttachThreadInput(m_currentThreadId, m_targetThreadId, FALSE);
		}
	}

	CScopedThreadInputAttach(const CScopedThreadInputAttach &) = delete;
	CScopedThreadInputAttach &operator=(const CScopedThreadInputAttach &) = delete;

  private:
	DWORD m_targetThreadId = 0;
	DWORD m_currentThreadId = 0;
	bool m_bAttached = false;
};

// Every call here is an unbounded trip through the target's window procedure, so this only runs
// via ActivateWindowBounded. Each gets a breadcrumb naming the call that never came back.
void ActivateWindowNow(HWND hWnd, bool bAlsoFocus)
{
	const CScopedThreadInputAttach attach(hWnd);

	if (IsIconic(hWnd)) {
		const DebugLog::CScope scope(kLogCategory, "ShowWindow(SW_RESTORE)");
		ShowWindow(hWnd, SW_RESTORE);
	}

	{
		const DebugLog::CScope scope(kLogCategory, "SetForegroundWindow");
		SetForegroundWindow(hWnd);
	}

	{
		const DebugLog::CScope scope(kLogCategory, "BringWindowToTop");
		BringWindowToTop(hWnd);
	}

	if (bAlsoFocus) {
		// Only meaningful while the attach above is still in effect.
		const DebugLog::CScope scope(kLogCategory, "SetFocus");
		SetFocus(hWnd);
	}
}

// False means the pass is still stuck inside the client's window procedure on a thread that has
// been abandoned. The client simply does not come forward, which both callers tolerate.
bool ActivateWindowBounded(HWND hWnd, bool bAlsoFocus)
{
	DebugLog::Write(kLogCategory, "activation pass starting (hwnd=0x%p, focus=%s)", hWnd, bAlsoFocus ? "yes" : "no");

	const bool bCompleted =
		RunBoundedOrAbandon([hWnd, bAlsoFocus]() { ActivateWindowNow(hWnd, bAlsoFocus); }, kActivateTimeout);

	DebugLog::Write(kLogCategory, "activation pass %s",
					bCompleted ? "completed" : "ABANDONED - a thread is stuck in the client's window procedure");

	return bCompleted;
}

// SetFocus can return success before the focus change takes effect, and a keystroke synthesized
// in that gap goes nowhere. Proceeds anyway on timeout - this is insurance, not a requirement.
void WaitForKeyboardFocus(const CUiElement &element, u32 timeoutMs)
{
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

	while (!element.HasKeyboardFocus()) {
		if (std::chrono::steady_clock::now() >= deadline) return;

		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
}

void SetFieldValue(const CUiAutomation &uiAutomation, const CUiElement &field, const std::wstring &value)
{
	if (field.SetValue(value.c_str())) return;

	field.SetFocus();
	WaitForKeyboardFocus(field, 500);
	uiAutomation.SendKeystrokes(value.c_str());
}

// The tooltip only says that some error is showing. The specific reason is a separate element,
// searched for across the whole window rather than under the tooltip.
std::wstring ResolveLoginErrorMessage(const CUiAutomation &uiAutomation, const CUiElement &window)
{
	if (uiAutomation.FindFirstDescendantByName(window, kInvalidCredentialsReasonText).IsValid()) {
		return kInvalidCredentialsReasonText;
	}

	if (uiAutomation.FindFirstDescendantByName(window, kTroubleSigningInReasonText).IsValid()) {
		return kTroubleSigningInReasonText;
	}

	return kLoginErrorToolTipName;
}
} // namespace

CRiotClient::~CRiotClient()
{
	// Not a kill: the point is to leave the user logged in, not close the client on them.
	if (m_hProcess != nullptr) {
		CloseHandle(m_hProcess);
	}
}

bool CRiotClient::IsGameInProgress()
{
	const bool bInProgress = AnyProcessNameMatches(kGameProcessNames, ARRAYSIZE(kGameProcessNames));
	DebugLog::Write(kLogCategory, "IsGameInProgress -> %s", bInProgress ? "yes" : "no");

	return bInProgress;
}

void CRiotClient::KillAllClientProcesses()
{
	DebugLog::Write(kLogCategory, "killing every known Riot Client process");
	KillProcessesByName(kClientProcessNames, ARRAYSIZE(kClientProcessNames));
}

std::string_view CRiotClient::LaunchProductForBannerTitle(std::string_view bannerTitle)
{
	if (bannerTitle == "League of Legends" || bannerTitle == "Teamfight Tactics") return "league_of_legends";

	if (bannerTitle == "Valorant") return "valorant";

	if (bannerTitle == "Legends of Runeterra") return "bacon";

	if (bannerTitle == "2XKO") return "lion";

	return "";
}

bool CRiotClient::ResolveExecutablePath()
{
	m_executablePath.clear();

	std::ifstream file(kInstallsJsonPath);
	if (!file.is_open()) return false;

	nlohmann::json parsed;
	try {
		file >> parsed;
	} catch (const nlohmann::json::exception &) {
		return false;
	}

	const auto it = parsed.find(kExecutablePathKey);
	if (it == parsed.end() || !it->is_string()) return false;

	const std::string path = it->get<std::string>();
	if (path.empty()) return false;

	m_executablePath = Utf8ToWide(std::string_view{path.data(), path.size()});

	// The JSON stores forward slashes; CreateProcessW accepts either, but normalizing keeps this
	// consistent with every other path here.
	for (wchar_t &c : m_executablePath) {
		if (c == L'/') {
			c = L'\\';
		}
	}

	DebugLog::Write(kLogCategory, "resolved Riot Client executable: %ls", m_executablePath.c_str());

	return !m_executablePath.empty();
}

bool CRiotClient::Launch(std::string_view launchProduct)
{
	if (m_executablePath.empty()) return false;

	// CreateProcessW may rewrite lpCommandLine in place, so this has to be a mutable local.
	std::wstring commandLine = L"\"" + m_executablePath + L"\"";
	if (!launchProduct.empty()) {
		commandLine += L" --launch-product=" + Utf8ToWide(launchProduct) + L" --launch-patchline=live";
	}

	STARTUPINFOW startupInfo{.cb = sizeof(startupInfo)};
	PROCESS_INFORMATION processInfo{};

	if (!CreateProcessW(m_executablePath.c_str(), commandLine.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr,
						&startupInfo, &processInfo)) {
		DebugLog::Write(kLogCategory, "CreateProcessW FAILED err=%lu for %ls", GetLastError(),
						m_executablePath.c_str());
		return false;
	}

	DebugLog::Write(kLogCategory, "launched Riot Client pid %lu (%ls)", processInfo.dwProcessId, commandLine.c_str());
	CloseHandle(processInfo.hThread);

	if (m_hProcess != nullptr) {
		CloseHandle(m_hProcess); // releases this instance's handle, does not kill the process
	}

	m_hProcess = processInfo.hProcess;
	m_processId = processInfo.dwProcessId;

	return true;
}

bool CRiotClient::IsRunning() const
{
	if (m_hProcess == nullptr) return false;

	DWORD exitCode = 0;

	return GetExitCodeProcess(m_hProcess, &exitCode) != 0 && exitCode == STILL_ACTIVE;
}

HWND CRiotClient::FindClientWindow() const
{
	HWND hWnd = CUiAutomation::FindWindowByName(kClientWindowTitle);

	if (hWnd == nullptr && m_processId != 0) {
		hWnd = CUiAutomation::FindTopLevelWindow(m_processId);
	}

	return hWnd;
}

CUiElement CRiotClient::CurrentWindowElement(const CUiAutomation &uiAutomation) const
{
	const HWND hWnd = FindClientWindow();
	if (hWnd == nullptr) return CUiElement{};

	// Cached because every poll loop below calls this each iteration and ElementFromWindow is the
	// expensive cross-process half: a ten-second form wait was making a hundred of these, each
	// another chance for the provider to wedge.
	//
	// A changed handle invalidates it, since the splash window and the real login window are
	// different handles. So does the short expiry, since one handle can outlive the accessibility
	// tree underneath it.
	constexpr auto kWindowElementCacheLifetime = std::chrono::milliseconds(2000);

	const auto now = std::chrono::steady_clock::now();
	if (hWnd == m_cachedWindowHandle && m_cachedWindowElement.IsValid() && now < m_cachedWindowExpiry) {
		return m_cachedWindowElement;
	}

	CUiElement element = uiAutomation.ElementFromWindow(hWnd);
	m_cachedWindowHandle = element.IsValid() ? hWnd : nullptr;
	m_cachedWindowElement = element;
	m_cachedWindowExpiry = now + kWindowElementCacheLifetime;

	return element;
}

HWND CRiotClient::WaitForResponsiveClientWindow(u32 timeoutMs, const std::atomic<bool> *pCancelRequested) const
{
	const bool bWaitForever = timeoutMs == kWaitForeverMs;
	const auto started = std::chrono::steady_clock::now();
	const auto deadline = started + std::chrono::milliseconds(bWaitForever ? 0 : timeoutMs);

	// No CScope around the loop: it legitimately runs for as long as a cold client takes to
	// start, so a breadcrumb here would report "stuck" on every slow-but-normal launch. The calls
	// inside carry their own.
	DebugLog::Write(kLogCategory, "waiting for a responsive client window (%s)",
					bWaitForever ? "no deadline - until cancelled" : "bounded");

	u32 polls = 0;
	u32 nextProgressPoll = 20; // ~2s in, then every ~5s

	for (;;) {
		// Both halves every iteration: "exists but not pumping yet" is normal for the first
		// seconds of a cold start, and callers must not act on it.
		const HWND hWnd = FindClientWindow();
		if (hWnd != nullptr && IsWindowResponsive(hWnd, kResponsiveProbeTimeoutMs)) {
			LogWindowIdentity("responsive client window", hWnd);
			return hWnd;
		}

		polls += 1;
		if (polls >= nextProgressPoll) {
			nextProgressPoll = polls + 50;
			const auto waitedMs =
				std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started)
					.count();

			DebugLog::Write(kLogCategory, "still waiting for a responsive client window after %lldms (%s)",
							static_cast<long long>(waitedMs),
							hWnd == nullptr ? "no window yet" : "window exists but is not pumping messages");
		}

		if (IsCancelled(pCancelRequested) || (!bWaitForever && std::chrono::steady_clock::now() >= deadline)) {
			DebugLog::Write(kLogCategory, "gave up waiting for a responsive client window (%s)",
							IsCancelled(pCancelRequested) ? "cancelled" : "timed out");
			return nullptr;
		}

		std::this_thread::sleep_for(kPollInterval);
	}
}

bool CRiotClient::WaitForWindow(u32 timeoutMs, const std::atomic<bool> *pCancelRequested) const
{
	return WaitForResponsiveClientWindow(timeoutMs, pCancelRequested) != nullptr;
}

bool CRiotClient::BringToForeground(u32 timeoutMs, const std::atomic<bool> *pCancelRequested) const
{
	const HWND hWnd = WaitForResponsiveClientWindow(timeoutMs, pCancelRequested);
	if (hWnd == nullptr) {
		DebugLog::Write(kLogCategory, "BringToForeground: no responsive window to activate");
		return false;
	}

	return ActivateWindowBounded(hWnd, false);
}

bool CRiotClient::SetKeyboardFocus(u32 timeoutMs, const std::atomic<bool> *pCancelRequested) const
{
	const HWND hWnd = WaitForResponsiveClientWindow(timeoutMs, pCancelRequested);
	if (hWnd == nullptr) {
		DebugLog::Write(kLogCategory, "SetKeyboardFocus: no responsive window to focus");
		return false;
	}

	return ActivateWindowBounded(hWnd, true);
}

bool CRiotClient::SubmitLogin(const CUiAutomation &uiAutomation, std::string_view username, std::string_view password,
							  u32 timeoutMs, const std::atomic<bool> *pCancelRequested) const
{
	// Re-resolves the window and both fields together every iteration: an earlier-found window
	// can turn out to have been the splash screen, which never grows these fields. Polling the
	// pairing as a unit stays correct however many window transitions happen.
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

	CUiElement usernameField;
	CUiElement passwordField;
	u32 polls = 0;

	DebugLog::Write(kLogCategory, "looking for the login form (up to %ums)", timeoutMs);

	for (;;) {
		const CUiElement window = CurrentWindowElement(uiAutomation);
		if (window.IsValid()) {
			usernameField =
				uiAutomation.FindFirstDescendantByNameAndControlType(window, kUsernameFieldName, UIA_EditControlTypeId);
			passwordField =
				uiAutomation.FindFirstDescendantByNameAndControlType(window, kPasswordFieldName, UIA_EditControlTypeId);

			if (usernameField.IsValid() && passwordField.IsValid()) {
				DebugLog::Write(kLogCategory, "login form found after %u poll(s)", polls);
				break;
			}
		}

		polls += 1;

		if (IsCancelled(pCancelRequested) || uiAutomation.HasWedged() || std::chrono::steady_clock::now() >= deadline) {
			// No window at all is a launch problem; a window without the fields usually means an
			// already-logged-in client.
			DebugLog::Write(kLogCategory,
							"login form NOT found after %u poll(s) (%s; window %s, username %s, password %s)", polls,
							IsCancelled(pCancelRequested) ? "cancelled" : "timed out",
							CurrentWindowElement(uiAutomation).IsValid() ? "yes" : "no",
							usernameField.IsValid() ? "yes" : "no", passwordField.IsValid() ? "yes" : "no");
			return false;
		}

		std::this_thread::sleep_for(kPollInterval);
	}

	SetFieldValue(uiAutomation, usernameField, Utf8ToWide(username));
	SetFieldValue(uiAutomation, passwordField, Utf8ToWide(password));

	// Enter rather than a button, since no button's name has been confirmed. The focus wait is
	// what makes it reliable: a key sent straight after SetFocus can land nowhere.
	passwordField.SetFocus();
	WaitForKeyboardFocus(passwordField, 500);

	DebugLog::Write(kLogCategory, "submitting the login form (password field %s real keyboard focus)",
					passwordField.HasKeyboardFocus() ? "has" : "does NOT have");
	uiAutomation.SendKey(VK_RETURN);

	return true;
}

bool CRiotClient::WaitForLoginError(const CUiAutomation &uiAutomation, std::wstring &outMessage, u32 timeoutMs,
									const std::atomic<bool> *pCancelRequested, const std::wstring *pIgnoreMessage) const
{
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

	// Starts true when there is nothing to ignore, so a first attempt accepts a match at once.
	bool sawTooltipAbsent = pIgnoreMessage == nullptr;

	for (;;) {
		const CUiElement window = CurrentWindowElement(uiAutomation);
		const bool tooltipPresent =
			window.IsValid() && uiAutomation.FindFirstDescendantByName(window, kLoginErrorToolTipName).IsValid();

		if (tooltipPresent) {
			std::wstring message = ResolveLoginErrorMessage(uiAutomation, window);

			if (sawTooltipAbsent || pIgnoreMessage == nullptr || message != *pIgnoreMessage) {
				DebugLog::Write(kLogCategory, "login error shown: \"%ls\"", message.c_str());
				outMessage = std::move(message);
				return true;
			}

			DebugLog::Write(kLogCategory, "ignoring the previous attempt's error tooltip, still on screen");
		} else {
			sawTooltipAbsent = true;
		}

		if (uiAutomation.HasWedged()) {
			// Every lookup fails instantly now, so "no error found" means nothing. The caller
			// checks HasWedged before inferring success.
			DebugLog::Write(kLogCategory,
							"abandoning the login-result wait - UI Automation has given up on the client");
			return false;
		}

		if (IsCancelled(pCancelRequested) || std::chrono::steady_clock::now() >= deadline) {
			// "Nothing happened" and "we stopped looking" are otherwise the same line.
			DebugLog::Write(kLogCategory, "no login error appeared (%s) - treating the attempt as successful",
							IsCancelled(pCancelRequested) ? "cancelled" : "waited the full timeout");
			return false;
		}

		std::this_thread::sleep_for(kPollInterval);
	}
}

bool CRiotClient::WaitForPlayButtonAndClick(const CUiAutomation &uiAutomation, u32 timeoutMs,
											const std::atomic<bool> *pCancelRequested) const
{
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

	for (;;) {
		const CUiElement window = CurrentWindowElement(uiAutomation);
		if (window.IsValid()) {
			const CUiElement playButton =
				uiAutomation.FindFirstDescendantByNameAndControlType(window, kPlayButtonName, UIA_ButtonControlTypeId);

			if (playButton.IsValid()) {
				DebugLog::Write(kLogCategory, "Play button found - invoking it");
				playButton.Invoke();
				return true;
			}
		}

		if (IsCancelled(pCancelRequested) || std::chrono::steady_clock::now() >= deadline) {
			// Not load-bearing: the login already succeeded by the time this runs.
			DebugLog::Write(kLogCategory, "Play button not found (%s) - leaving the game unlaunched",
							IsCancelled(pCancelRequested) ? "cancelled" : "timed out");
			return false;
		}

		std::this_thread::sleep_for(kPollInterval);
	}
}
