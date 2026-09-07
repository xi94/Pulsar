#pragma once

#include <string>

#include <UIAutomation.h>
#include <Windows.h>
#include <wrl/client.h>

// A thin wrapper over Win32 UI Automation, the accessibility API a screen reader uses, for
// inspecting and driving another process's UI. Nothing here knows about the Riot Client.
//
// Threading: apartment membership belongs to the OS thread, not to this object. Construct a
// fresh instance on whichever thread will use it and never share one, even sequentially.
//
// Every cross-process lookup runs on a throwaway thread and is abandoned if it does not return
// in time. These are uncancellable COM calls into another process's provider, and a
// Chromium-based one rebuilding its tree has been seen sitting inside a single
// ElementFromHandle for over thirty seconds. An abandoned lookup reports "not found"; HasWedged
// is what tells that apart from a genuine miss.

/// One found element. Copyable; a default-constructed instance is the "not found" state every
/// lookup returns instead of a null or an error code.
class CUiElement {
  public:
	CUiElement() = default;
	explicit CUiElement(Microsoft::WRL::ComPtr<IUIAutomationElement> pElement);

	bool IsValid() const
	{
		return m_pElement != nullptr;
	}

	/// Escape hatch for anything this wrapper does not cover.
	const Microsoft::WRL::ComPtr<IUIAutomationElement> &Get() const
	{
		return m_pElement;
	}

	/// A true text write via IUIAutomationValuePattern. Not every control supports it; SetFocus
	/// plus CUiAutomation::SendKeystrokes is the fallback when this returns false.
	bool SetValue(const wchar_t *pText) const;

	bool Invoke() const;
	bool SetFocus() const;

	/// Whether this element holds OS-level keyboard focus right now. Some providers post the
	/// focus change rather than applying it, so poll this before synthesizing a keystroke.
	bool HasKeyboardFocus() const;

	bool GetName(std::wstring &outName) const;
	bool GetAutomationId(std::wstring &outAutomationId) const;

  private:
	Microsoft::WRL::ComPtr<IUIAutomationElement> m_pElement;
};

class CUiAutomation {
  public:
	~CUiAutomation();

	/// Call once early in startup, before any instance exists. Holds the process's MTA open so
	/// the per-instance CoInitializeEx/CoUninitialize pairs stop tearing down the whole
	/// apartment - and UI Automation's state inside it - every time. Joins no apartment itself.
	static void KeepProcessMtaAlive();

	/// Must succeed before any other method. See this file's threading note.
	bool Init();
	void Shutdown();

	/// True once too many of this instance's lookups have been abandoned, meaning the target has
	/// stopped answering and later lookups now fail fast.
	///
	/// Any caller that reads "found nothing" as an answer must check this - a wedged provider
	/// and a genuine absence look identical otherwise.
	bool HasWedged() const
	{
		return m_bWedged;
	}

	/// Searches processId and every descendant process: the launched Riot Client process is a
	/// lightweight parent and only one grandchild owns the real window. Only top-level, visible,
	/// unowned windows above a minimum size count.
	static HWND FindTopLevelWindow(u32 processId);

	/// An exact top-level title match, desktop-wide. Preferred over FindTopLevelWindow, which
	/// remains for a caller that only has a process id to go on.
	static HWND FindWindowByName(const wchar_t *pTitle);

	/// Collapses the two common startup waits - no window yet, and window without an automation
	/// tree yet - into one call.
	CUiElement WaitForWindowByProcessId(u32 processId, u32 timeoutMs) const;
	CUiElement WaitForWindowByName(const wchar_t *pTitle, u32 timeoutMs) const;

	CUiElement ElementFromWindow(HWND hWnd) const;

	/// One-shot lookups, for when the element should already exist. Descendants rather than
	/// children: Riot Client-style UI nests controls several levels deep.
	CUiElement FindFirstDescendantByAutomationId(const CUiElement &root, const wchar_t *pAutomationId) const;
	CUiElement FindFirstDescendantByName(const CUiElement &root, const wchar_t *pName) const;
	CUiElement FindFirstDescendantByControlType(const CUiElement &root, CONTROLTYPEID controlType) const;

	/// Name and ControlType together. The login form's fields are CEF-rendered inputs with a
	/// Name but no AutomationId, and pairing the two is what stops this matching an unrelated
	/// element that happens to share the same visible label.
	CUiElement FindFirstDescendantByNameAndControlType(const CUiElement &root, const wchar_t *pName,
													   CONTROLTYPEID controlType) const;

	/// The same lookups, polled: a freshly launched process's controls often do not exist for
	/// the first several hundred milliseconds, and one lookup cannot tell "not yet" from "never".
	CUiElement WaitForDescendantByAutomationId(const CUiElement &root, const wchar_t *pAutomationId,
											   u32 timeoutMs) const;
	CUiElement WaitForDescendantByName(const CUiElement &root, const wchar_t *pName, u32 timeoutMs) const;
	CUiElement WaitForDescendantByControlType(const CUiElement &root, CONTROLTYPEID controlType, u32 timeoutMs) const;
	CUiElement WaitForDescendantByNameAndControlType(const CUiElement &root, const wchar_t *pName,
													 CONTROLTYPEID controlType, u32 timeoutMs) const;

	/// Synthesized input via SendInput, so it lands as a genuine keypress rather than a posted
	/// WM_KEYDOWN some UI frameworks ignore. Types into whatever holds keyboard focus.
	void SendKeystrokes(const wchar_t *pText) const;
	void SendKey(WORD virtualKeyCode) const;

  private:
	bool CanSearch(const CUiElement &root) const;

	/// Shared body of the four one-shot lookups: everything past building the condition.
	CUiElement FindFirstWithCondition(const CUiElement &root, Microsoft::WRL::ComPtr<IUIAutomationCondition> pCondition,
									  const char *pLabel) const;

	/// Shared tail of every bounded lookup, keeping the abandoned-call bookkeeping in one place.
	CUiElement FinishBoundedLookup(Microsoft::WRL::ComPtr<IUIAutomationElement> pFound, bool bAbandoned) const;

	Microsoft::WRL::ComPtr<IUIAutomation> m_pAutomation;
	bool m_bComInitialized = false;

	/// Mutable because the lookups are const: they only count how badly the other end is
	/// behaving. Unsynchronized, since an instance belongs to exactly one thread.
	mutable u32 m_abandonedCallCount = 0;
	mutable bool m_bWedged = false;
};
