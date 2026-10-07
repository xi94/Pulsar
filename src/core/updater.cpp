#include "core/updater.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include <sodium.h>

#include "core/app_identity.h"
#include "core/str.h"
#include "core/thread_util.h"
#include "core/version.h"
#include "os/http.h"
#include "os/self_update.h"

namespace {
constexpr std::array<u8, 32> K_RELEASE_SIGNING_PUBLIC_KEY{
	0xD0, 0x82, 0xF5, 0xE2, 0x12, 0xC9, 0x37, 0x3F, 0x4E, 0xEE, 0x56, 0x8D, 0xA3, 0x32, 0x8A, 0x64,
	0x3E, 0x73, 0x86, 0xBA, 0x0F, 0xE6, 0x13, 0xC3, 0x7F, 0x70, 0x23, 0x76, 0x63, 0x73, 0xF5, 0xE3,
};

constexpr auto  K_SHUTDOWN_JOIN_TIMEOUT = std::chrono::milliseconds(3000);
constexpr usize K_MAX_DOWNLOAD_BYTES    = 64ull * 1024 * 1024;

template <usize Capacity>
[[nodiscard]] auto copy_string_field(const nlohmann::json& t_json, const char* t_key, char (&t_destination)[Capacity]) -> bool
{
	const auto field = t_json.find(t_key);
	if (field == t_json.end() || !field->is_string()) return false;

	copy_to(field->get_ref<const std::string&>(), t_destination);

	return true;
}

[[nodiscard]] auto equals_ignoring_case(std::string_view t_a, std::string_view t_b) -> bool
{
	const auto lowered = [](char t_character) { return t_character >= 'A' && t_character <= 'Z' ? static_cast<char>(t_character - 'A' + 'a') : t_character; };

	return std::ranges::equal(t_a, t_b, [&lowered](char t_left, char t_right) { return lowered(t_left) == lowered(t_right); });
}
}

auto parse_update_manifest(const std::vector<u8>& t_json, UpdateManifest* t_out_manifest) -> bool
{
	const nlohmann::json parsed = nlohmann::json::parse(t_json.begin(), t_json.end(), nullptr, false);
	if (!parsed.is_object()) return false;

	const bool has_required_fields = copy_string_field(parsed, "version", t_out_manifest->version) && copy_string_field(parsed, "url", t_out_manifest->url) &&
	                                 copy_string_field(parsed, "sha256", t_out_manifest->sha256_hex) &&
	                                 copy_string_field(parsed, "signature", t_out_manifest->signature_base64);
	if (!has_required_fields) return false;

	if (!copy_string_field(parsed, "min_upgrade_version", t_out_manifest->min_upgrade_version)) {
		copy_to("0.0.0", t_out_manifest->min_upgrade_version);
	}

	if (!copy_string_field(parsed, "notes", t_out_manifest->notes)) {
		t_out_manifest->notes[0] = '\0';
	}

	return true;
}

auto verify_update(const std::vector<u8>& t_build, const UpdateManifest& t_manifest, std::span<const u8, 32> t_public_key, std::string* t_out_error) -> bool
{
	u8 digest[crypto_hash_sha256_BYTES];
	crypto_hash_sha256(digest, t_build.data(), t_build.size());

	char digest_hex[crypto_hash_sha256_BYTES * 2 + 1];
	sodium_bin2hex(digest_hex, sizeof(digest_hex), digest, sizeof(digest));

	if (!equals_ignoring_case(digest_hex, t_manifest.sha256_hex)) {
		*t_out_error = "the download doesn't match the manifest's SHA-256 - it may be corrupted or truncated";
		return false;
	}

	u8         signature[crypto_sign_BYTES];
	usize      signature_length = 0;
	const bool decoded = sodium_base642bin(signature, sizeof(signature), t_manifest.signature_base64, std::strlen(t_manifest.signature_base64), nullptr,
	                                       &signature_length, nullptr, sodium_base64_VARIANT_ORIGINAL) == 0 &&
	                     signature_length == crypto_sign_BYTES;

	if (!decoded) {
		*t_out_error = "the manifest's signature is malformed";
		return false;
	}

	if (crypto_sign_verify_detached(signature, digest, sizeof(digest), t_public_key.data()) != 0) {
		*t_out_error = "signature verification failed - refusing to install an unsigned or tampered update";
		return false;
	}

	return true;
}

Updater::~Updater()
{
	request_cancel();
	join_or_abandon(&m_worker, &m_worker_finished, K_SHUTDOWN_JOIN_TIMEOUT);
}

auto Updater::check_for_update() -> void
{
	const UpdateStage current         = stage();
	const bool        nothing_pending = current == UpdateStage::IDLE || current == UpdateStage::UP_TO_DATE || current == UpdateStage::CHECK_FAILED;
	if (m_worker_active || !nothing_pending) return;

	prepare_new_worker();
	m_stage.store(UpdateStage::CHECKING, std::memory_order_relaxed);
	m_worker = std::thread([this]() { check_for_update_on_worker(); });
}

auto Updater::start_download() -> void
{
	const UpdateStage current       = stage();
	const bool        have_manifest = current == UpdateStage::AVAILABLE || current == UpdateStage::UPDATE_FAILED || current == UpdateStage::CANCELLED;
	if (m_worker_active || !have_manifest) return;

	prepare_new_worker();
	m_worker = std::thread([this, manifest = m_manifest]() { download_and_install_on_worker(manifest); });
}

auto Updater::update() -> void
{
	if (!m_worker_active || !m_worker_finished.load(std::memory_order_acquire)) return;

	if (m_worker.joinable()) {
		m_worker.join();
	}

	m_worker_active     = false;
	m_ready_to_relaunch = stage() == UpdateStage::READY_TO_RELAUNCH;
}

auto Updater::consume_ready_to_relaunch() -> bool
{
	return std::exchange(m_ready_to_relaunch, false);
}

auto Updater::prepare_new_worker() -> void
{
	if (m_worker.joinable()) {
		m_worker.join();
	}

	m_cancel_requested.store(false, std::memory_order_relaxed);
	m_worker_finished.store(false, std::memory_order_relaxed);
	m_worker_active = true;
}

auto Updater::finish_worker(UpdateStage t_stage) -> void
{
	m_stage.store(t_stage, std::memory_order_release);
	m_worker_finished.store(true, std::memory_order_release);
}

auto Updater::fail_worker(UpdateStage t_stage, const char* t_prefix, const char* t_detail) -> void
{
	std::snprintf(m_error_message, sizeof(m_error_message), "%s%s", t_prefix, t_detail);
	finish_worker(t_stage);
}

auto Updater::check_for_update_on_worker() -> void
{
	constexpr const char* PREFIX = "Couldn't check for updates: ";

	std::vector<u8> body;
	std::string     error;

	if (os::http_get(K_UPDATE_MANIFEST_URL, K_MAX_DOWNLOAD_BYTES, &body, os::HttpProgress{}, &error) != os::HttpResult::OK) {
		fail_worker(UpdateStage::CHECK_FAILED, PREFIX, error.c_str());
		return;
	}

	UpdateManifest manifest{};
	if (!parse_update_manifest(body, &manifest)) {
		fail_worker(UpdateStage::CHECK_FAILED, PREFIX, "malformed manifest");
		return;
	}

	const std::optional<Version> latest = parse_version(manifest.version);
	if (!latest) {
		fail_worker(UpdateStage::CHECK_FAILED, PREFIX, "manifest has an unparseable version");
		return;
	}

	const Version current                 = parse_version(K_APP_VERSION).value_or(Version{});
	const Version minimum_for_auto_update = parse_version(manifest.min_upgrade_version).value_or(Version{});

	m_manifest = manifest;

	if (*latest <= current) {
		finish_worker(UpdateStage::UP_TO_DATE);
	} else if (current < minimum_for_auto_update || !os::can_self_update()) {
		finish_worker(UpdateStage::MANUAL_UPGRADE_REQUIRED);
	} else {
		finish_worker(UpdateStage::AVAILABLE);
	}
}

auto Updater::download_and_install_on_worker(UpdateManifest t_manifest) -> void
{
	m_bytes_downloaded.store(0, std::memory_order_relaxed);
	m_total_bytes.store(0, std::memory_order_relaxed);
	m_bytes_per_second.store(0.0, std::memory_order_relaxed);
	m_stage.store(UpdateStage::DOWNLOADING, std::memory_order_release);

	const os::HttpProgress progress{&m_cancel_requested, &m_bytes_downloaded, &m_total_bytes, &m_bytes_per_second};

	std::vector<u8>      body;
	std::string          error;
	const os::HttpResult result = os::http_get(t_manifest.url, K_MAX_DOWNLOAD_BYTES, &body, progress, &error);

	if (result == os::HttpResult::CANCELLED) {
		finish_worker(UpdateStage::CANCELLED);
		return;
	}

	if (result != os::HttpResult::OK) {
		fail_worker(UpdateStage::UPDATE_FAILED, "Download failed: ", error.c_str());
		return;
	}

	m_stage.store(UpdateStage::VERIFYING, std::memory_order_release);
	if (!verify_update(body, t_manifest, K_RELEASE_SIGNING_PUBLIC_KEY, &error)) {
		fail_worker(UpdateStage::UPDATE_FAILED, "", error.c_str());
		return;
	}

	if (m_cancel_requested.load(std::memory_order_relaxed)) {
		finish_worker(UpdateStage::CANCELLED);
		return;
	}

	m_stage.store(UpdateStage::INSTALLING, std::memory_order_release);
	if (!os::install_update(body, K_APP_VERSION, &error)) {
		fail_worker(UpdateStage::UPDATE_FAILED, "", error.c_str());
		return;
	}

	finish_worker(UpdateStage::READY_TO_RELAUNCH);
}
