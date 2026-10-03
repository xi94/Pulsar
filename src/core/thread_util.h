#pragma once

#include <atomic>
#include <chrono>
#include <concepts>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

// t_finished is set by the worker as its last step; a worker that has not set it before the timeout is detached and left to finish alone.
inline auto join_or_abandon(std::thread* t_thread, const std::atomic<bool>* t_finished, std::chrono::milliseconds t_timeout) -> void
{
	if (!t_thread->joinable()) return;

	const auto deadline = std::chrono::steady_clock::now() + t_timeout;
	while (!t_finished->load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline) {
		std::this_thread::sleep_for(std::chrono::milliseconds{1});
	}

	if (t_finished->load(std::memory_order_acquire)) {
		t_thread->join();
	} else {
		t_thread->detach();
	}
}

[[nodiscard]] auto run_unless_cancelled(std::invocable auto t_work, const std::atomic<bool>* t_cancel) -> bool
{
	struct Completion {
		std::mutex              mutex;
		std::condition_variable signal;
		bool                    done = false;
	};

	const auto  completion = std::make_shared<Completion>();
	std::thread worker([completion, work = std::move(t_work)]() mutable {
		work();

		{
			const std::lock_guard lock(completion->mutex);
			completion->done = true;
		}

		completion->signal.notify_all();
	});

	std::unique_lock lock(completion->mutex);
	while (!completion->signal.wait_for(lock, std::chrono::milliseconds{50}, [&completion] { return completion->done; })) {
		if (t_cancel->load(std::memory_order_relaxed)) {
			lock.unlock();
			worker.detach();
			return false;
		}
	}

	lock.unlock();
	worker.join();

	return true;
}
