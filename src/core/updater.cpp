#include "core/updater.h"

#include <array>
#include <chrono>
#include <compare>
#include <cstdio>
#include <cstring>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <Windows.h>
#include <winhttp.h>

#include <nlohmann/json.hpp>
#include <sodium.h>

#include "core/app_identity.h"
#include "core/str.h"
#include "core/thread_util.h"
#include "platform/process.h"

namespace {
constexpr wchar_t K_USER_AGENT[] = L"Pulsar-Updater/1.0";

constexpr std::array<u8, 32> K_RELEASE_SIGNING_PUBLIC_KEY{
	0xD0, 0x82, 0xF5, 0xE2, 0x12, 0xC9, 0x37, 0x3F, 0x4E, 0xEE, 0x56, 0x8D, 0xA3, 0x32, 0x8A, 0x64,
	0x3E, 0x73, 0x86, 0xBA, 0x0F, 0xE6, 0x13, 0xC3, 0x7F, 0x70, 0x23, 0x76, 0x63, 0x73, 0xF5, 0xE3,
};

constexpr int   K_RESOLVE_TIMEOUT_MS    = 10000;
constexpr int   K_CONNECT_TIMEOUT_MS    = 10000;
constexpr int   K_SEND_TIMEOUT_MS       = 15000;
constexpr int   K_RECEIVE_TIMEOUT_MS    = 15000;
constexpr auto  K_SHUTDOWN_JOIN_TIMEOUT = std::chrono::milliseconds(3000);
constexpr usize K_MAX_DOWNLOAD_BYTES    = 64ull * 1024 * 1024;

enum class HttpResult : u8 {
	Ok,
	Cancelled,
	Failed,
};

struct DownloadProgress {
	std::atomic<bool>*   cancel_requested = nullptr;
	std::atomic<u64>*    bytes_downloaded = nullptr;
	std::atomic<u64>*    total_bytes      = nullptr;
	std::atomic<double>* bytes_per_second = nullptr;
};

class InternetHandle {
  public:
	explicit InternetHandle(HINTERNET t_handle)
		: m_handle(t_handle)
	{
	}

	~InternetHandle()
	{
		if (m_handle != nullptr) {
			WinHttpCloseHandle(m_handle);
		}
	}

	InternetHandle(const InternetHandle&)                    = delete;
	auto operator=(const InternetHandle&) -> InternetHandle& = delete;

	operator HINTERNET() const
	{
		return m_handle;
	}

  private:
	HINTERNET m_handle;
};

struct SemVer {
	u32 major = 0;
	u32 minor = 0;
	u32 patch = 0;

	auto operator<=>(const SemVer&) const = default;
};

auto parse_version_component(std::string_view t_text, usize* t_index, u32* t_out_value) -> bool
{
	const usize start = *t_index;
	*t_out_value      = 0;

	while (*t_index < t_text.size() && t_text[*t_index] >= '0' && t_text[*t_index] <= '9') {
		*t_out_value = *t_out_value * 10 + static_cast<u32>(t_text[*t_index] - '0');
		*t_index += 1;
	}

	if (*t_index < t_text.size() && t_text[*t_index] == '.') {
		*t_index += 1;
	}

	return *t_index > start;
}

[[nodiscard]] auto parse_version(std::string_view t_text) -> std::optional<SemVer>
{
	SemVer version{};
	usize  index = 0;
	if (!parse_version_component(t_text, &index, &version.major)) return std::nullopt;

	parse_version_component(t_text, &index, &version.minor);
	parse_version_component(t_text, &index, &version.patch);

	return version;
}

template <usize Capacity>
[[nodiscard]] auto copy_string_field(const nlohmann::json& t_json, const char* t_key, char (&t_destination)[Capacity]) -> bool
{
	const auto field = t_json.find(t_key);
	if (field == t_json.end() || !field->is_string()) return false;

	copy_to(field->get_ref<const std::string&>(), t_destination);

	return true;
}

[[nodiscard]] auto parse_manifest(const std::vector<u8>& t_json, UpdateManifest* t_out_manifest) -> bool
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

[[nodiscard]] auto open_request(HINTERNET t_connection, const std::wstring& t_path, bool t_https) -> HINTERNET
{
	const HINTERNET request =
		WinHttpOpenRequest(t_connection, L"GET", t_path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, t_https ? WINHTTP_FLAG_SECURE : 0);

	if (request != nullptr) {
		// The github.com to CDN redirect crosses hosts, which WinHTTP's default policy refuses.
		DWORD redirect_policy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
		WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &redirect_policy, sizeof(redirect_policy));
	}

	return request;
}

auto query_number_header(HINTERNET t_request, DWORD t_header, DWORD* t_out_value) -> bool
{
	DWORD size = sizeof(DWORD);

	return WinHttpQueryHeaders(t_request, WINHTTP_QUERY_FLAG_NUMBER | t_header, WINHTTP_HEADER_NAME_BY_INDEX, t_out_value, &size, WINHTTP_NO_HEADER_INDEX) != 0;
}

[[nodiscard]] auto read_body(HINTERNET t_request, std::vector<u8>* t_out_body, const DownloadProgress& t_progress, std::string* t_out_error) -> HttpResult
{
	DWORD content_length = 0;
	if (query_number_header(t_request, WINHTTP_QUERY_CONTENT_LENGTH, &content_length)) {
		if (content_length > K_MAX_DOWNLOAD_BYTES) {
			*t_out_error = "the server offered a file far larger than any Pulsar build";
			return HttpResult::Failed;
		}

		t_out_body->reserve(content_length);

		if (t_progress.total_bytes != nullptr) {
			t_progress.total_bytes->store(content_length, std::memory_order_relaxed);
		}
	}

	const auto started = std::chrono::steady_clock::now();

	for (;;) {
		if (t_progress.cancel_requested != nullptr && t_progress.cancel_requested->load(std::memory_order_relaxed)) {
			return HttpResult::Cancelled;
		}

		DWORD available = 0;
		if (!WinHttpQueryDataAvailable(t_request, &available)) {
			*t_out_error = "the connection was interrupted while reading";
			return HttpResult::Failed;
		}

		if (available == 0) return HttpResult::Ok;

		if (t_out_body->size() + available > K_MAX_DOWNLOAD_BYTES) {
			*t_out_error = "the download grew far larger than any Pulsar build";
			return HttpResult::Failed;
		}

		const usize previous_size = t_out_body->size();
		t_out_body->resize(previous_size + available);

		DWORD read = 0;
		if (!WinHttpReadData(t_request, t_out_body->data() + previous_size, available, &read)) {
			*t_out_error = "the connection was interrupted while reading";
			return HttpResult::Failed;
		}

		t_out_body->resize(previous_size + read);

		if (t_progress.bytes_downloaded != nullptr) {
			t_progress.bytes_downloaded->store(t_out_body->size(), std::memory_order_relaxed);
		}

		const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - started;
		if (t_progress.bytes_per_second != nullptr && elapsed.count() > 0.0) {
			t_progress.bytes_per_second->store(static_cast<double>(t_out_body->size()) / elapsed.count(), std::memory_order_relaxed);
		}
	}
}

[[nodiscard]] auto http_get(const std::wstring& t_url, std::vector<u8>* t_out_body, const DownloadProgress& t_progress, std::string* t_out_error) -> HttpResult
{
	wchar_t host[256]{};
	wchar_t path[2048]{};
	wchar_t query[2048]{};

	URL_COMPONENTS url{
		.dwStructSize      = sizeof(URL_COMPONENTS),
		.lpszHostName      = host,
		.dwHostNameLength  = ARRAYSIZE(host),
		.lpszUrlPath       = path,
		.dwUrlPathLength   = ARRAYSIZE(path),
		.lpszExtraInfo     = query,
		.dwExtraInfoLength = ARRAYSIZE(query),
	};

	if (!WinHttpCrackUrl(t_url.c_str(), 0, 0, &url)) {
		*t_out_error = "could not parse the update URL";
		return HttpResult::Failed;
	}

	const InternetHandle session{WinHttpOpen(K_USER_AGENT, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)};
	if (session == nullptr) {
		*t_out_error = "could not open an HTTP session";
		return HttpResult::Failed;
	}

	WinHttpSetTimeouts(session, K_RESOLVE_TIMEOUT_MS, K_CONNECT_TIMEOUT_MS, K_SEND_TIMEOUT_MS, K_RECEIVE_TIMEOUT_MS);

	const InternetHandle connection{WinHttpConnect(session, host, url.nPort, 0)};
	if (connection == nullptr) {
		*t_out_error = "could not connect to " + to_utf8(host);
		return HttpResult::Failed;
	}

	const InternetHandle request{open_request(connection, std::wstring{path} + query, url.nScheme == INTERNET_SCHEME_HTTPS)};
	if (request == nullptr) {
		*t_out_error = "could not open an HTTP request";
		return HttpResult::Failed;
	}

	if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
		*t_out_error = "the request failed to send";
		return HttpResult::Failed;
	}

	if (!WinHttpReceiveResponse(request, nullptr)) {
		*t_out_error = "no response was received";
		return HttpResult::Failed;
	}

	DWORD status = 0;
	query_number_header(request, WINHTTP_QUERY_STATUS_CODE, &status);
	if (status < 200 || status >= 300) {
		*t_out_error = "server returned HTTP " + std::to_string(status);
		return HttpResult::Failed;
	}

	return read_body(request, t_out_body, t_progress, t_out_error);
}

[[nodiscard]] auto verify_download(const std::vector<u8>& t_body, const UpdateManifest& t_manifest, std::string* t_out_error) -> bool
{
	u8 digest[crypto_hash_sha256_BYTES];
	crypto_hash_sha256(digest, t_body.data(), t_body.size());

	char digest_hex[crypto_hash_sha256_BYTES * 2 + 1];
	sodium_bin2hex(digest_hex, sizeof(digest_hex), digest, sizeof(digest));

	if (_stricmp(digest_hex, t_manifest.sha256_hex) != 0) {
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

	if (crypto_sign_verify_detached(signature, digest, sizeof(digest), K_RELEASE_SIGNING_PUBLIC_KEY.data()) != 0) {
		*t_out_error = "signature verification failed - refusing to install an unsigned or tampered update";
		return false;
	}

	return true;
}

[[nodiscard]] auto is_newer_than_running(const std::wstring& t_executable) -> bool
{
	const DWORD info_size = GetFileVersionInfoSizeW(t_executable.c_str(), nullptr);
	if (info_size == 0) return false;

	std::vector<u8> info(info_size);
	if (!GetFileVersionInfoW(t_executable.c_str(), 0, info_size, info.data())) return false;

	VS_FIXEDFILEINFO* fixed      = nullptr;
	UINT              fixed_size = 0;
	if (!VerQueryValueW(info.data(), L"\\", reinterpret_cast<void**>(&fixed), &fixed_size) || fixed == nullptr || fixed_size < sizeof(VS_FIXEDFILEINFO)) {
		return false;
	}

	const SemVer downloaded{HIWORD(fixed->dwFileVersionMS), LOWORD(fixed->dwFileVersionMS), HIWORD(fixed->dwFileVersionLS)};

	return downloaded > parse_version(K_APP_VERSION).value_or(SemVer{});
}

[[nodiscard]] auto write_bytes(const std::wstring& t_path, const std::vector<u8>& t_bytes) -> bool
{
	const HANDLE file = CreateFileW(t_path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE) return false;

	DWORD      written = 0;
	const bool ok      = WriteFile(file, t_bytes.data(), static_cast<DWORD>(t_bytes.size()), &written, nullptr) && written == t_bytes.size();
	CloseHandle(file);

	return ok;
}

[[nodiscard]] auto replace_running_executable(const std::vector<u8>& t_new_executable, std::string* t_out_error) -> bool
{
	const std::wstring self_path = executable_path();
	if (self_path.empty()) {
		*t_out_error = "could not resolve this program's path";
		return false;
	}

	const std::wstring update_path = self_path + L".update";
	const std::wstring backup_path = self_path + L".old";

	if (!write_bytes(update_path, t_new_executable)) {
		DeleteFileW(update_path.c_str());
		*t_out_error = "could not write the downloaded update to disk (disk full?)";
		return false;
	}

	if (!is_newer_than_running(update_path)) {
		DeleteFileW(update_path.c_str());
		*t_out_error = "the downloaded build is not newer than this one - refusing to downgrade";
		return false;
	}

	if (MoveFileExW(update_path.c_str(), self_path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
		DeleteFileW(backup_path.c_str());
		return true;
	}

	// A crash between these two renames leaves only the .old copy, which handed_off_to_repaired_copy restores.
	if (!MoveFileExW(self_path.c_str(), backup_path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
		DeleteFileW(update_path.c_str());
		*t_out_error = "could not replace the running executable (it may be locked by another program)";
		return false;
	}

	if (!MoveFileExW(update_path.c_str(), self_path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
		MoveFileExW(backup_path.c_str(), self_path.c_str(), MOVEFILE_REPLACE_EXISTING);
		*t_out_error = "could not move the new build into place";
		return false;
	}

	return true;
}

auto delete_stale_backup(const std::wstring& t_backup_path) -> void
{
	constexpr int ATTEMPTS = 20;

	for (int attempt = 0; attempt < ATTEMPTS; attempt += 1) {
		if (DeleteFileW(t_backup_path.c_str()) || GetLastError() == ERROR_FILE_NOT_FOUND) return;

		Sleep(50);
	}
}
}

Updater::~Updater()
{
	request_cancel();
	join_or_abandon(&m_worker, K_SHUTDOWN_JOIN_TIMEOUT);
}

auto Updater::handed_off_to_repaired_copy() -> bool
{
	const std::wstring self_path = executable_path();
	if (self_path.empty()) return false;

	constexpr std::wstring_view BACKUP_SUFFIX = L".old";
	if (!self_path.ends_with(BACKUP_SUFFIX)) {
		delete_stale_backup(self_path + std::wstring{BACKUP_SUFFIX});
		return false;
	}

	const std::wstring canonical_path = self_path.substr(0, self_path.size() - BACKUP_SUFFIX.size());
	if (GetFileAttributesW(canonical_path.c_str()) == INVALID_FILE_ATTRIBUTES) {
		CopyFileW(self_path.c_str(), canonical_path.c_str(), FALSE);
	}

	launch_process(canonical_path);

	return true;
}

auto Updater::check_for_update() -> void
{
	const UpdateStage current         = stage();
	const bool        nothing_pending = current == UpdateStage::Idle || current == UpdateStage::UpToDate || current == UpdateStage::CheckFailed;
	if (m_worker_active || !nothing_pending) return;

	prepare_new_worker();
	m_stage.store(UpdateStage::Checking, std::memory_order_relaxed);
	m_worker = std::thread([this]() { check_for_update_on_worker(); });
}

auto Updater::start_download() -> void
{
	const UpdateStage current       = stage();
	const bool        have_manifest = current == UpdateStage::Available || current == UpdateStage::Error || current == UpdateStage::Cancelled;
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
	m_ready_to_relaunch = stage() == UpdateStage::ReadyToRelaunch;
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

	if (http_get(K_UPDATE_MANIFEST_URL, &body, DownloadProgress{}, &error) != HttpResult::Ok) {
		fail_worker(UpdateStage::CheckFailed, PREFIX, error.c_str());
		return;
	}

	UpdateManifest manifest{};
	if (!parse_manifest(body, &manifest)) {
		fail_worker(UpdateStage::CheckFailed, PREFIX, "malformed manifest");
		return;
	}

	const std::optional<SemVer> latest = parse_version(manifest.version);
	if (!latest) {
		fail_worker(UpdateStage::CheckFailed, PREFIX, "manifest has an unparseable version");
		return;
	}

	const SemVer current                 = parse_version(K_APP_VERSION).value_or(SemVer{});
	const SemVer minimum_for_auto_update = parse_version(manifest.min_upgrade_version).value_or(SemVer{});

	m_manifest = manifest;

	if (*latest <= current) {
		finish_worker(UpdateStage::UpToDate);
	} else if (current < minimum_for_auto_update) {
		finish_worker(UpdateStage::ManualUpgradeRequired);
	} else {
		finish_worker(UpdateStage::Available);
	}
}

auto Updater::download_and_install_on_worker(UpdateManifest t_manifest) -> void
{
	m_bytes_downloaded.store(0, std::memory_order_relaxed);
	m_total_bytes.store(0, std::memory_order_relaxed);
	m_bytes_per_second.store(0.0, std::memory_order_relaxed);
	m_stage.store(UpdateStage::Downloading, std::memory_order_release);

	const DownloadProgress progress{&m_cancel_requested, &m_bytes_downloaded, &m_total_bytes, &m_bytes_per_second};

	std::vector<u8>  body;
	std::string      error;
	const HttpResult result = http_get(to_wide(t_manifest.url), &body, progress, &error);

	if (result == HttpResult::Cancelled) {
		finish_worker(UpdateStage::Cancelled);
		return;
	}

	if (result != HttpResult::Ok) {
		fail_worker(UpdateStage::Error, "Download failed: ", error.c_str());
		return;
	}

	m_stage.store(UpdateStage::Verifying, std::memory_order_release);
	if (!verify_download(body, t_manifest, &error)) {
		fail_worker(UpdateStage::Error, "", error.c_str());
		return;
	}

	if (m_cancel_requested.load(std::memory_order_relaxed)) {
		finish_worker(UpdateStage::Cancelled);
		return;
	}

	m_stage.store(UpdateStage::Installing, std::memory_order_release);
	if (!replace_running_executable(body, &error)) {
		fail_worker(UpdateStage::Error, "", error.c_str());
		return;
	}

	finish_worker(UpdateStage::ReadyToRelaunch);
}
