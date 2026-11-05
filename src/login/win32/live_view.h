#pragma once

#include <chrono>
#include <memory>
#include <string>

struct LiveViewState;

// A small window in the corner of the screen that mirrors the hidden Riot Client while Pulsar signs in. It never takes focus, and it closes itself after close_after, so nothing ever waits on it.
class LiveView {
  public:
	LiveView() = default;
	~LiveView();

	LiveView(const LiveView&)                    = delete;
	auto operator=(const LiveView&) -> LiveView& = delete;

	auto open() -> void;
	auto set_status(std::wstring t_status) -> void;
	auto count_focus_given_back() -> void;
	auto close_after(std::chrono::milliseconds t_delay) -> void;

  private:
	std::shared_ptr<LiveViewState> m_state;
};
