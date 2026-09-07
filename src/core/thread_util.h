#pragma once

#include <chrono>
#include <thread>
#include <utility>

#include <Windows.h>

/// Destroying a joinable std::thread calls std::terminate, so a worker wedged forever in a
/// call this process does not own the other end of - a UI Automation COM call into an
/// unresponsive client, a WinHTTP request with no response - still has to be dealt with.
///
/// Bounded join first, then detach: an OS thread leaked once is far better than hanging
/// shutdown, which is what a bare join() did here. That hang gave the crash handler nothing
/// to run on, so force-killing the process was the only way out - exactly what reads to a
/// user as "it froze and nothing ever came up".
inline void JoinWithTimeoutOrDetach(std::thread &thread, std::chrono::milliseconds timeout)
{
	if (!thread.joinable()) return;

	if (WaitForSingleObject(thread.native_handle(), static_cast<DWORD>(timeout.count())) == WAIT_OBJECT_0) {
		thread.join();
	} else {
		thread.detach();
	}
}

/// Runs fn on a throwaway thread, waiting up to timeout. A false return means fn is still
/// running on a detached thread, so fn must own everything it touches - capture by value,
/// never a reference into the caller's frame.
///
/// This exists for core/riot_client.cpp's window-activation calls. AttachThreadInput,
/// SetForegroundWindow and friends all run the *target's* window procedure synchronously and
/// none of them takes a timeout, so a target that does not return blocks the caller forever
/// with no cancellation point. Bounding the call from outside is the only thing that helps;
/// a WM_NULL probe proves the thread dequeues messages, not that this message returns.
template <typename TFunc>
inline bool RunBoundedOrAbandon(TFunc fn, std::chrono::milliseconds timeout)
{
	std::thread worker(std::move(fn));

	if (WaitForSingleObject(worker.native_handle(), static_cast<DWORD>(timeout.count())) == WAIT_OBJECT_0) {
		worker.join();
		return true;
	}

	worker.detach();

	return false;
}
