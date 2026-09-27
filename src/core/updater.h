#pragma once

#include <atomic>
#include <string>
#include <thread>

enum class UpdateStage : u8 {
	idle,
	checking,
	up_to_date,
	available,
	manual_upgrade_required,
	check_failed,
	downloading,
	verifying,
	installing,
	ready_to_relaunch,
	error,
	cancelled,
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

	Updater(const Updater &) = delete;
	Updater &operator=(const Updater &) = delete;

	static bool handed_off_to_repaired_copy();

	void check_for_update();
	void start_download();
	void update();

	void request_cancel()
	{
		m_cancel_requested.store(true, std::memory_order_relaxed);
	}

	bool consume_ready_to_relaunch();

	UpdateStage stage() const
	{
		return m_stage.load(std::memory_order_acquire);
	}

	const UpdateManifest &manifest() const
	{
		return m_manifest;
	}

	const char *error_message() const
	{
		return m_error_message;
	}

	u64 bytes_downloaded() const
	{
		return m_bytes_downloaded.load(std::memory_order_relaxed);
	}

	u64 total_bytes() const
	{
		return m_total_bytes.load(std::memory_order_relaxed);
	}

	double bytes_per_second() const
	{
		return m_bytes_per_second.load(std::memory_order_relaxed);
	}

  private:
	void check_for_update_on_worker();
	void download_and_install_on_worker(UpdateManifest t_manifest);
	void finish_worker(UpdateStage t_stage);
	void fail_worker(UpdateStage t_stage, const char *t_prefix, const char *t_detail);
	void prepare_new_worker();

	std::atomic<UpdateStage> m_stage{UpdateStage::idle};
	std::atomic<bool> m_cancel_requested{false};
	std::atomic<bool> m_worker_finished{false};
	std::thread m_worker;
	bool m_worker_active = false;
	bool m_ready_to_relaunch = false;

	std::atomic<u64> m_bytes_downloaded{0};
	std::atomic<u64> m_total_bytes{0};
	std::atomic<double> m_bytes_per_second{0.0};

	UpdateManifest m_manifest{};
	char m_error_message[256]{};
};
