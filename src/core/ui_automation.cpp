#include "core/ui_automation.h"

#include <algorithm>
#include <chrono>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include <TlHelp32.h>

#include "core/debug_log.h"

namespace {
using Microsoft::WRL::ComPtr;

constexpr const char *kLogCategory = "uia";

// Short enough that a control appearing "as soon as it can" does not feel like a stall, long
// enough not to burn a core spinning on UI Automation calls, which are not cheap.
constexpr u32 kPollIntervalMs = 100;

// The bounds here are real, from a captured log of the hang this exists for: an ordinary
// lookup against a settled Riot Client returns in well under 250ms, while the one that wedged
// was still inside the provider 39 seconds later. Five seconds is comfortably past the
// slowest legitimate first contact and still leaves room for the caller's next poll.
constexpr auto kUiaCallTimeout = std::chrono::milliseconds(5000);

// Each abandoned lookup permanently strands one OS thread inside the provider, so this is
// really "how many threads one login attempt may leak". A provider that has failed to answer
// twice is not about to start.
constexpr u32 kMaxAbandonedCalls = 2;

// Heap-allocated and co-owned by the caller and the throwaway thread: an abandoned thread that
// finally returns minutes later must write into memory it still owns, not into a dead frame.
struct BoundedLookupResult {
	ComPtr<IUIAutomationElement> Element;
};

// A VARIANT holding a copy of pText, released on scope exit.
struct CAutoVariantString {
	VARIANT Value;

	explicit CAutoVariantString(const wchar_t *pText)
	{
		VariantInit(&Value);
		Value.vt = VT_BSTR;
		Value.bstrVal = SysAllocString(pText);
	}

	~CAutoVariantString()
	{
		VariantClear(&Value);
	}

	CAutoVariantString(const CAutoVariantString &) = delete;
	CAutoVariantString &operator=(const CAutoVariantString &) = delete;
};

VARIANT VariantFromControlType(CONTROLTYPEID controlType)
{
	VARIANT value;
	VariantInit(&value);
	value.vt = VT_I4;
	value.lVal = controlType;

	return value;
}

// A fresh snapshot every call rather than a cache, since a process tree can change - a crashed
// and relaunched subprocess - for as long as a caller keeps polling.
std::vector<DWORD> CollectDescendantProcessIds(DWORD rootProcessId)
{
	std::vector<DWORD> result{rootProcessId};

	const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == INVALID_HANDLE_VALUE) return result;

	std::vector<std::pair<DWORD, DWORD>> parentChildPairs;
	PROCESSENTRY32W entry{.dwSize = sizeof(entry)};

	if (Process32FirstW(snapshot, &entry)) {
		do {
			parentChildPairs.emplace_back(entry.th32ParentProcessID, entry.th32ProcessID);
		} while (Process32NextW(snapshot, &entry));
	}

	CloseHandle(snapshot);

	// Repeated passes over the fixed snapshot until one adds nothing - a breadth-first walk
	// outward from the root, which always terminates because the snapshot is finite.
	bool addedAny = true;
	while (addedAny) {
		addedAny = false;

		for (const auto &[parentProcessId, processId] : parentChildPairs) {
			const bool parentInSet = std::find(result.begin(), result.end(), parentProcessId) != result.end();
			const bool alreadyInSet = std::find(result.begin(), result.end(), processId) != result.end();

			if (parentInSet && !alreadyInSet) {
				result.push_back(processId);
				addedAny = true;
			}
		}
	}

	return result;
}

struct FindWindowState {
	const std::vector<DWORD> *pCandidateProcessIds;
	HWND Result;
};

BOOL CALLBACK FindTopLevelWindowProc(HWND hWnd, LPARAM lParam)
{
	auto *pState = reinterpret_cast<FindWindowState *>(lParam);

	DWORD windowProcessId = 0;
	GetWindowThreadProcessId(hWnd, &windowProcessId);

	const auto &candidates = *pState->pCandidateProcessIds;
	if (std::find(candidates.begin(), candidates.end(), windowProcessId) == candidates.end()) return TRUE;

	// An owner rules out a dialog or tooltip; invisible rules out the hidden helper windows
	// some frameworks create before their real UI is ready.
	if (GetWindow(hWnd, GW_OWNER) != nullptr || !IsWindowVisible(hWnd)) return TRUE;

	// Chromium-derived helper processes create tiny windows for internal purposes.
	RECT rect;
	if (GetWindowRect(hWnd, &rect) && (rect.right - rect.left) < 50) return TRUE;

	pState->Result = hWnd;

	return FALSE;
}

// Runs one cross-process lookup on a throwaway thread, setting bOutAbandoned if the thread had
// to be let go still running. The thread joins the MTA itself, and fn is copied into it, so an
// abandoned thread still holds its own references and can never touch a released interface.
template <typename TFunc>
ComPtr<IUIAutomationElement> RunBoundedLookup(const char *pLabel, TFunc fn, bool &bOutAbandoned)
{
	auto pResult = std::make_shared<BoundedLookupResult>();

	std::thread worker([pResult, fn]() {
		const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		fn(pResult->Element);

		if (comResult == S_OK || comResult == S_FALSE) {
			CoUninitialize();
		}
	});

	const DebugLog::CScope scope(kLogCategory, "%s", pLabel);

	if (WaitForSingleObject(worker.native_handle(), static_cast<DWORD>(kUiaCallTimeout.count())) == WAIT_OBJECT_0) {
		worker.join();
		bOutAbandoned = false;
		return std::move(pResult->Element);
	}

	worker.detach();
	bOutAbandoned = true;
	DebugLog::Write(kLogCategory, "ABANDONED %s after %llums - the provider never answered", pLabel,
					static_cast<unsigned long long>(kUiaCallTimeout.count()));

	return nullptr;
}

// Retries fn every kPollIntervalMs until it returns a valid element or timeoutMs elapses.
template <typename TFunc>
CUiElement PollForElement(u32 timeoutMs, TFunc fn)
{
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

	for (;;) {
		CUiElement found = fn();
		if (found.IsValid()) return found;

		if (std::chrono::steady_clock::now() >= deadline) return CUiElement{};

		std::this_thread::sleep_for(std::chrono::milliseconds(kPollIntervalMs));
	}
}
} // namespace

CUiElement::CUiElement(ComPtr<IUIAutomationElement> pElement)
	: m_pElement(std::move(pElement))
{
}

bool CUiElement::SetValue(const wchar_t *pText) const
{
	if (!IsValid()) return false;

	ComPtr<IUIAutomationValuePattern> pValuePattern;
	if (FAILED(m_pElement->GetCurrentPatternAs(UIA_ValuePatternId, IID_PPV_ARGS(&pValuePattern))) ||
		pValuePattern == nullptr) {
		DebugLog::Write(kLogCategory, "SetValue: element has no ValuePattern - caller falls back to keystrokes");
		return false;
	}

	// Never logs pText: this is the call the account password goes through.
	const DebugLog::CScope scope(kLogCategory, "ValuePattern::SetValue");

	BSTR bstrText = SysAllocString(pText);
	const HRESULT hr = pValuePattern->SetValue(bstrText);
	SysFreeString(bstrText);

	if (FAILED(hr)) {
		// E_ACCESSDENIED is the classic sign of the target running elevated while this process
		// does not: UIPI blocks the write, and the keystroke fallback for the same reason.
		DebugLog::Write(kLogCategory, "ValuePattern::SetValue FAILED hr=0x%08lX", static_cast<unsigned long>(hr));
	}

	return SUCCEEDED(hr);
}

bool CUiElement::Invoke() const
{
	if (!IsValid()) return false;

	ComPtr<IUIAutomationInvokePattern> pInvokePattern;
	if (FAILED(m_pElement->GetCurrentPatternAs(UIA_InvokePatternId, IID_PPV_ARGS(&pInvokePattern))) ||
		pInvokePattern == nullptr) {
		return false;
	}

	const DebugLog::CScope scope(kLogCategory, "InvokePattern::Invoke");
	const HRESULT hr = pInvokePattern->Invoke();

	if (FAILED(hr)) {
		DebugLog::Write(kLogCategory, "InvokePattern::Invoke FAILED hr=0x%08lX", static_cast<unsigned long>(hr));
	}

	return SUCCEEDED(hr);
}

bool CUiElement::SetFocus() const
{
	if (!IsValid()) return false;

	const DebugLog::CScope scope(kLogCategory, "IUIAutomationElement::SetFocus");
	const HRESULT hr = m_pElement->SetFocus();

	if (FAILED(hr)) {
		DebugLog::Write(kLogCategory, "SetFocus FAILED hr=0x%08lX", static_cast<unsigned long>(hr));
	}

	return SUCCEEDED(hr);
}

bool CUiElement::HasKeyboardFocus() const
{
	if (!IsValid()) return false;

	BOOL hasFocus = FALSE;

	return SUCCEEDED(m_pElement->get_CurrentHasKeyboardFocus(&hasFocus)) && hasFocus != FALSE;
}

bool CUiElement::GetName(std::wstring &outName) const
{
	if (!IsValid()) return false;

	BSTR bstrName = nullptr;
	if (FAILED(m_pElement->get_CurrentName(&bstrName)) || bstrName == nullptr) return false;

	outName.assign(bstrName, SysStringLen(bstrName));
	SysFreeString(bstrName);

	return true;
}

bool CUiElement::GetAutomationId(std::wstring &outAutomationId) const
{
	if (!IsValid()) return false;

	BSTR bstrId = nullptr;
	if (FAILED(m_pElement->get_CurrentAutomationId(&bstrId)) || bstrId == nullptr) return false;

	outAutomationId.assign(bstrId, SysStringLen(bstrId));
	SysFreeString(bstrId);

	return true;
}

CUiAutomation::~CUiAutomation()
{
	Shutdown();
}

void CUiAutomation::KeepProcessMtaAlive()
{
	// The MTA exists only while at least one thread is in it, and every CUiAutomation lives on a
	// short-lived worker, so the last Shutdown tears the whole apartment down and the next
	// attempt rebuilds it. CoIncrementMTAUsage holds it open without joining any thread, which is
	// why the render thread can call it. The cookie is never released.
	static CO_MTA_USAGE_COOKIE s_cookie = nullptr;
	if (s_cookie != nullptr) return;

	const HRESULT hr = CoIncrementMTAUsage(&s_cookie);
	DebugLog::Write(kLogCategory, "CoIncrementMTAUsage hr=0x%08lX (process-wide MTA %s)",
					static_cast<unsigned long>(hr),
					SUCCEEDED(hr) ? "held open" : "NOT held - apartment will be torn down between attempts");
}

bool CUiAutomation::Init()
{
	if (m_pAutomation != nullptr) return true;

	if (m_bComInitialized) {
		// A previous Init took the COM reference but failed to create the interface. Balancing
		// it is Shutdown's job; taking a second one here would leak it.
		DebugLog::Write(kLogCategory, "Init retried after a failed one - reusing the existing COM reference");
	} else {
		// RPC_E_CHANGED_MODE means this thread already joined a single-threaded apartment
		// itself. Nothing to balance, and not a hard failure: UI Automation generally still
		// works against whatever apartment is active.
		const DebugLog::CScope comScope(kLogCategory, "CoInitializeEx(COINIT_MULTITHREADED)");
		const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		m_bComInitialized = comResult == S_OK || comResult == S_FALSE;

		DebugLog::Write(kLogCategory, "CoInitializeEx hr=0x%08lX (%s)", static_cast<unsigned long>(comResult),
						comResult == S_OK	   ? "joined the MTA"
						: comResult == S_FALSE ? "already in a compatible apartment"
											   : "NOT initialised by us");
	}

	// The one call in this flow that has actually been suspected of hanging when login attempts
	// come in quick succession - scoped so the watchdog names it if it ever does.
	const DebugLog::CScope createScope(kLogCategory, "CoCreateInstance(CLSID_CUIAutomation)");
	const HRESULT hr =
		CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_pAutomation));

	DebugLog::Write(kLogCategory, "CoCreateInstance(CLSID_CUIAutomation) hr=0x%08lX after %llums",
					static_cast<unsigned long>(hr), static_cast<unsigned long long>(createScope.ElapsedMs()));

	return SUCCEEDED(hr);
}

void CUiAutomation::Shutdown()
{
	if (m_pAutomation == nullptr && !m_bComInitialized) return;

	// Scoped separately because they fail differently: releasing the last reference to a
	// cross-process proxy is itself a call into the target, and CoUninitialize can block
	// draining the apartment's RPC work.
	{
		const DebugLog::CScope scope(kLogCategory, "release IUIAutomation");
		m_pAutomation.Reset();
	}

	if (m_bComInitialized) {
		const DebugLog::CScope scope(kLogCategory, "CoUninitialize");
		CoUninitialize();
		m_bComInitialized = false;
	}
}

CUiElement CUiAutomation::FinishBoundedLookup(ComPtr<IUIAutomationElement> pFound, bool bAbandoned) const
{
	if (!bAbandoned) return pFound != nullptr ? CUiElement{std::move(pFound)} : CUiElement{};

	m_abandonedCallCount += 1;

	if (m_abandonedCallCount >= kMaxAbandonedCalls && !m_bWedged) {
		m_bWedged = true;
		DebugLog::Write(kLogCategory,
						"GIVING UP on this target - %u lookup(s) abandoned; every later one now fails immediately",
						m_abandonedCallCount);
	}

	return CUiElement{};
}

HWND CUiAutomation::FindTopLevelWindow(u32 processId)
{
	const std::vector<DWORD> candidateProcessIds = CollectDescendantProcessIds(static_cast<DWORD>(processId));

	FindWindowState state{.pCandidateProcessIds = &candidateProcessIds, .Result = nullptr};
	EnumWindows(FindTopLevelWindowProc, reinterpret_cast<LPARAM>(&state));

	return state.Result;
}

HWND CUiAutomation::FindWindowByName(const wchar_t *pTitle)
{
	return FindWindowW(nullptr, pTitle);
}

CUiElement CUiAutomation::WaitForWindowByProcessId(u32 processId, u32 timeoutMs) const
{
	return PollForElement(timeoutMs, [this, processId] {
		const HWND hWnd = FindTopLevelWindow(processId);
		return hWnd != nullptr ? ElementFromWindow(hWnd) : CUiElement{};
	});
}

CUiElement CUiAutomation::WaitForWindowByName(const wchar_t *pTitle, u32 timeoutMs) const
{
	return PollForElement(timeoutMs, [this, pTitle] {
		const HWND hWnd = FindWindowByName(pTitle);
		return hWnd != nullptr ? ElementFromWindow(hWnd) : CUiElement{};
	});
}

CUiElement CUiAutomation::ElementFromWindow(HWND hWnd) const
{
	if (m_pAutomation == nullptr || hWnd == nullptr || m_bWedged) return CUiElement{};

	// This is the call that has to wake the target's accessibility provider up - a Chromium
	// client builds its whole tree on first contact - and the one confirmed to have sat inside
	// the provider for 39 seconds with no way out.
	char label[64];
	_snprintf_s(label, _TRUNCATE, "ElementFromHandle(hwnd=0x%p)", hWnd);

	bool bAbandoned = false;
	auto pFound = RunBoundedLookup(
		label,
		[pAutomation = m_pAutomation, hWnd](ComPtr<IUIAutomationElement> &outElement) {
			pAutomation->ElementFromHandle(hWnd, &outElement);
		},
		bAbandoned);

	return FinishBoundedLookup(std::move(pFound), bAbandoned);
}

// Walking another process's whole UI tree node by node over COM is the most expensive call
// this class makes, so it goes through RunBoundedLookup like everything else here.
CUiElement CUiAutomation::FindFirstWithCondition(const CUiElement &root, ComPtr<IUIAutomationCondition> pCondition,
												 const char *pLabel) const
{
	bool bAbandoned = false;
	auto pFound = RunBoundedLookup(
		pLabel,
		[pRoot = root.Get(), pCondition](ComPtr<IUIAutomationElement> &outElement) {
			pRoot->FindFirst(TreeScope_Descendants, pCondition.Get(), &outElement);
		},
		bAbandoned);

	return FinishBoundedLookup(std::move(pFound), bAbandoned);
}

bool CUiAutomation::CanSearch(const CUiElement &root) const
{
	return m_pAutomation != nullptr && root.IsValid() && !m_bWedged;
}

CUiElement CUiAutomation::FindFirstDescendantByAutomationId(const CUiElement &root, const wchar_t *pAutomationId) const
{
	if (!CanSearch(root)) return CUiElement{};

	const CAutoVariantString value(pAutomationId);

	ComPtr<IUIAutomationCondition> pCondition;
	if (FAILED(m_pAutomation->CreatePropertyCondition(UIA_AutomationIdPropertyId, value.Value, &pCondition)) ||
		pCondition == nullptr) {
		return CUiElement{};
	}

	char label[192];
	_snprintf_s(label, _TRUNCATE, "FindFirst(AutomationId=%ls)", pAutomationId);

	return FindFirstWithCondition(root, std::move(pCondition), label);
}

CUiElement CUiAutomation::FindFirstDescendantByName(const CUiElement &root, const wchar_t *pName) const
{
	if (!CanSearch(root)) return CUiElement{};

	const CAutoVariantString value(pName);

	ComPtr<IUIAutomationCondition> pCondition;
	if (FAILED(m_pAutomation->CreatePropertyCondition(UIA_NamePropertyId, value.Value, &pCondition)) ||
		pCondition == nullptr) {
		return CUiElement{};
	}

	char label[192];
	_snprintf_s(label, _TRUNCATE, "FindFirst(Name=%ls)", pName);

	return FindFirstWithCondition(root, std::move(pCondition), label);
}

CUiElement CUiAutomation::FindFirstDescendantByControlType(const CUiElement &root, CONTROLTYPEID controlType) const
{
	if (!CanSearch(root)) return CUiElement{};

	ComPtr<IUIAutomationCondition> pCondition;
	if (FAILED(m_pAutomation->CreatePropertyCondition(UIA_ControlTypePropertyId, VariantFromControlType(controlType),
													  &pCondition)) ||
		pCondition == nullptr) {
		return CUiElement{};
	}

	char label[64];
	_snprintf_s(label, _TRUNCATE, "FindFirst(ControlType=%d)", static_cast<int>(controlType));

	return FindFirstWithCondition(root, std::move(pCondition), label);
}

CUiElement CUiAutomation::FindFirstDescendantByNameAndControlType(const CUiElement &root, const wchar_t *pName,
																  CONTROLTYPEID controlType) const
{
	if (!CanSearch(root)) return CUiElement{};

	const CAutoVariantString nameValue(pName);

	ComPtr<IUIAutomationCondition> pNameCondition;
	if (FAILED(m_pAutomation->CreatePropertyCondition(UIA_NamePropertyId, nameValue.Value, &pNameCondition)) ||
		pNameCondition == nullptr) {
		return CUiElement{};
	}

	ComPtr<IUIAutomationCondition> pTypeCondition;
	if (FAILED(m_pAutomation->CreatePropertyCondition(UIA_ControlTypePropertyId, VariantFromControlType(controlType),
													  &pTypeCondition)) ||
		pTypeCondition == nullptr) {
		return CUiElement{};
	}

	ComPtr<IUIAutomationCondition> pAndCondition;
	if (FAILED(m_pAutomation->CreateAndCondition(pNameCondition.Get(), pTypeCondition.Get(), &pAndCondition)) ||
		pAndCondition == nullptr) {
		return CUiElement{};
	}

	char label[192];
	_snprintf_s(label, _TRUNCATE, "FindFirst(Name=%ls, ControlType=%d)", pName, static_cast<int>(controlType));

	return FindFirstWithCondition(root, std::move(pAndCondition), label);
}

CUiElement CUiAutomation::WaitForDescendantByAutomationId(const CUiElement &root, const wchar_t *pAutomationId,
														  u32 timeoutMs) const
{
	return PollForElement(timeoutMs, [&] { return FindFirstDescendantByAutomationId(root, pAutomationId); });
}

CUiElement CUiAutomation::WaitForDescendantByName(const CUiElement &root, const wchar_t *pName, u32 timeoutMs) const
{
	return PollForElement(timeoutMs, [&] { return FindFirstDescendantByName(root, pName); });
}

CUiElement CUiAutomation::WaitForDescendantByControlType(const CUiElement &root, CONTROLTYPEID controlType,
														 u32 timeoutMs) const
{
	return PollForElement(timeoutMs, [&] { return FindFirstDescendantByControlType(root, controlType); });
}

CUiElement CUiAutomation::WaitForDescendantByNameAndControlType(const CUiElement &root, const wchar_t *pName,
																CONTROLTYPEID controlType, u32 timeoutMs) const
{
	return PollForElement(timeoutMs, [&] { return FindFirstDescendantByNameAndControlType(root, pName, controlType); });
}

void CUiAutomation::SendKeystrokes(const wchar_t *pText) const
{
	// Counted rather than logged per character, and the text itself never logged: this is the
	// path an account password takes when a field has no ValuePattern. A non-zero rejected
	// count means SendInput was refused outright - UIPI, or the secure desktop - which
	// otherwise surfaces as an unexplained timeout several steps later.
	u32 sent = 0;
	u32 rejected = 0;

	for (const wchar_t *pChar = pText; *pChar != L'\0'; pChar += 1) {
		INPUT input[2]{};
		input[0].type = INPUT_KEYBOARD;
		input[0].ki.wScan = static_cast<WORD>(*pChar);
		input[0].ki.dwFlags = KEYEVENTF_UNICODE;
		input[1] = input[0];
		input[1].ki.dwFlags |= KEYEVENTF_KEYUP;

		if (SendInput(2, input, sizeof(INPUT)) == 2) {
			sent += 1;
		} else {
			rejected += 1;
		}
	}

	DebugLog::Write(kLogCategory, "SendKeystrokes: %u character(s) sent, %u rejected%s", sent, rejected,
					rejected != 0 ? " - SendInput was blocked (elevation/UIPI?)" : "");
}

void CUiAutomation::SendKey(WORD virtualKeyCode) const
{
	INPUT input[2]{};
	input[0].type = INPUT_KEYBOARD;
	input[0].ki.wVk = virtualKeyCode;
	input[1] = input[0];
	input[1].ki.dwFlags = KEYEVENTF_KEYUP;

	const UINT inserted = SendInput(2, input, sizeof(INPUT));

	// Worth its own line because this is the VK_RETURN that submits the form: if it does not
	// land, nothing does, and the attempt times out waiting for a result it never asked for.
	DebugLog::Write(kLogCategory, "SendKey(vk=0x%02X): %u of 2 event(s) inserted%s", virtualKeyCode, inserted,
					inserted != 2 ? " - SendInput was blocked (elevation/UIPI?)" : "");
}
