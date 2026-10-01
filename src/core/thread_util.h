#pragma once

#include <atomic>
#include <chrono>
#include <thread>
#include <utility>

#include <Windows.h>

inline bool wait_for_thread(std::thread *t_thread, std::chrono::milliseconds t_timeout)
{
	return WaitForSingleObject(t_thread->native_handle(), static_cast<DWORD>(t_timeout.count())) == WAIT_OBJECT_0;
}

inline void join_or_abandon(std::thread *t_thread, std::chrono::milliseconds t_timeout)
{
	if (!t_thread->joinable()) return;

	if (wait_for_thread(t_thread, t_timeout)) {
		t_thread->join();
	} else {
		t_thread->detach();
	}
}

template <typename Work>
bool run_unless_cancelled(Work t_work, const std::atomic<bool> *t_cancel)
{
	std::thread worker(std::move(t_work));

	while (!wait_for_thread(&worker, std::chrono::milliseconds{50})) {
		if (t_cancel->load(std::memory_order_relaxed)) {
			worker.detach();
			return false;
		}
	}

	worker.join();

	return true;
}
