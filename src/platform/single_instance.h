#pragma once

#include <Windows.h>

/// A named-mutex guard so only one instance runs at a time. The mutex, not a window lookup, is what
/// decides: an instance still starting up has no window yet, and two launches racing would both
/// conclude they were first.
class CSingleInstanceGuard {
  public:
	CSingleInstanceGuard();
	~CSingleInstanceGuard();

	CSingleInstanceGuard(const CSingleInstanceGuard &) = delete;
	CSingleInstanceGuard &operator=(const CSingleInstanceGuard &) = delete;

	bool IsFirstInstance() const
	{
		return m_bFirstInstance;
	}

	/// Hands the claim back early. The updater's relaunch needs this: the replacement build would
	/// otherwise see this still-running process's mutex and exit as a duplicate instead of
	/// starting.
	void Release();

  private:
	HANDLE m_hMutex = nullptr;
	bool m_bFirstInstance = false;
};
