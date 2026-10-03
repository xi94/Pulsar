#pragma once

#include <atomic>
#include <string>
#include <thread>

enum class UpdateStage : u8 {
	Idle,
	Checking,
	UpToDate,
	Available,
	ManualUpgradeRequired,
	CheckFailed,
	Downloading,
	Verifying,
	Installing,
	ReadyToRelaunch,
	Error,
	Cancelled,
};

struct UpdateManifest {
	char version[32]{};
	char min_upgrade_version[32]{};
	char url[512]{};
	char sha256_hex[65]{};
	char signature_base64[128]{};
	char notes[1024]{};
};

class Updater {
  public:
	Updater() = default;
	~Updater();

	Updater(const Updater&)                    = delete;
	auto operator=(const Updater&) -> Updater& = delete;

	auto check_for_update() -> void;
	auto start_download() -> void;
	auto update() -> void;

	auto request_cancel() -> void
	{
		m_cancel_requested.store(true, std::memory_order_relaxed);
	}

	[[nodiscard]] auto consume_ready_to_relaunch() -> bool;

	[[nodiscard]] auto stage() const -> UpdateStage
	{
		return m_stage.load(std::memory_order_acquire);
	}

	[[nodiscard]] auto manifest() const -> const UpdateManifest&
	{
		return m_manifest;
	}

	[[nodiscard]] auto error_message() const -> const char*
	{
		return m_error_message;
	}

	[[nodiscard]] auto bytes_downloaded() const -> u64
	{
		return m_bytes_downloaded.load(std::memory_order_relaxed);
	}

	[[nodiscard]] auto total_bytes() const -> u64
	{
		return m_total_bytes.load(std::memory_order_relaxed);
	}

	[[nodiscard]] auto bytes_per_second() const -> double
	{
		return m_bytes_per_second.load(std::memory_order_relaxed);
	}

  private:
	auto check_for_update_on_worker() -> void;
	auto download_and_install_on_worker(UpdateManifest t_manifest) -> void;
	auto finish_worker(UpdateStage t_stage) -> void;
	auto fail_worker(UpdateStage t_stage, const char* t_prefix, const char* t_detail) -> void;
	auto prepare_new_worker() -> void;

	std::atomic<UpdateStage> m_stage{UpdateStage::Idle};
	std::atomic<bool>        m_cancel_requested{false};
	std::atomic<bool>        m_worker_finished{false};
	std::thread              m_worker;
	bool                     m_worker_active     = false;
	bool                     m_ready_to_relaunch = false;

	std::atomic<u64>    m_bytes_downloaded{0};
	std::atomic<u64>    m_total_bytes{0};
	std::atomic<double> m_bytes_per_second{0.0};

	UpdateManifest m_manifest{};
	char           m_error_message[256]{};
};
