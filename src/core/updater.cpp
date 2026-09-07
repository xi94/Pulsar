#include "core/updater.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <vector>

#include <Windows.h>
#include <winhttp.h>

#include <sodium.h>

#include "core/app_identity.h"
#include "core/semver.h"
#include "core/thread_util.h"
#include "core/update_signing_key.h"

namespace {
constexpr wchar_t kUserAgent[] = L"Pulsar-Updater/1.0";

enum class EHttpResult : u8 {
	Ok,
	Cancelled,
	Failed,
};

struct HttpProgress {
	std::atomic<bool> *pCancelRequested = nullptr;
	std::atomic<u64> *pBytesDownloaded = nullptr;
	std::atomic<u64> *pTotalBytes = nullptr;
	std::atomic<double> *pBytesPerSecond = nullptr;
};

struct CInternetHandle {
	HINTERNET Handle = nullptr;

	~CInternetHandle()
	{
		if (Handle != nullptr) {
			WinHttpCloseHandle(Handle);
		}
	}

	operator HINTERNET() const
	{
		return Handle;
	}
};

struct CrackedUrl {
	std::wstring Host;
	std::wstring PathAndQuery;
	INTERNET_PORT Port = 0;
	bool Https = false;
};

bool CrackUrl(const wchar_t *pUrl, CrackedUrl &out)
{
	wchar_t host[256]{};
	wchar_t path[2048]{};
	wchar_t extra[2048]{};

	URL_COMPONENTS components{};
	components.dwStructSize = sizeof(components);
	components.lpszHostName = host;
	components.dwHostNameLength = static_cast<DWORD>(std::size(host));
	components.lpszUrlPath = path;
	components.dwUrlPathLength = static_cast<DWORD>(std::size(path));
	components.lpszExtraInfo = extra;
	components.dwExtraInfoLength = static_cast<DWORD>(std::size(extra));

	if (!WinHttpCrackUrl(pUrl, 0, 0, &components)) return false;

	out.Host = host;
	out.PathAndQuery = std::wstring(path) + extra;
	out.Port = components.nPort;
	out.Https = components.nScheme == INTERNET_SCHEME_HTTPS;

	return true;
}

// Only ever used to fold an ASCII hostname into an error message.
std::string WideToUtf8(const std::wstring &wide)
{
	if (wide.empty()) return {};

	const int length =
		WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
	if (length <= 0) return {};

	std::string result(static_cast<usize>(length), '\0');
	WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), result.data(), length, nullptr,
						nullptr);

	return result;
}

std::wstring Utf8ToWide(const char *pUtf8)
{
	const int length = MultiByteToWideChar(CP_UTF8, 0, pUtf8, -1, nullptr, 0);
	if (length <= 0) return L"";

	std::wstring result(static_cast<usize>(length - 1), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, pUtf8, -1, result.data(), length);

	return result;
}

// Explicit rather than WinHTTP's platform default: a connection that is accepted and then never
// sends anything would otherwise block every call below. Resolve and connect get a shorter
// budget than send and receive, which need room for a multi-megabyte download.
void ApplyRequestTimeouts(HINTERNET session)
{
	WinHttpSetTimeouts(session, 10000, 10000, 15000, 15000);
}

CInternetHandle OpenRequest(const CrackedUrl &cracked, HINTERNET connect)
{
	const DWORD requestFlags = cracked.Https ? WINHTTP_FLAG_SECURE : 0;
	CInternetHandle request{WinHttpOpenRequest(connect, L"GET", cracked.PathAndQuery.c_str(), nullptr,
											   WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, requestFlags)};

	if (request.Handle != nullptr) {
		// Explicit, because WinHTTP's default follows HTTPS to HTTPS but not HTTPS to HTTP -
		// and the github.com to CDN hop every fetch here goes through needs this.
		DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
		WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));
	}

	return request;
}

bool ReadStatusCode(HINTERNET request, DWORD &outStatusCode)
{
	DWORD size = sizeof(outStatusCode);

	return WinHttpQueryHeaders(request, WINHTTP_QUERY_FLAG_NUMBER | WINHTTP_QUERY_STATUS_CODE,
							   WINHTTP_HEADER_NAME_BY_INDEX, &outStatusCode, &size, WINHTTP_NO_HEADER_INDEX) != 0;
}

void ReserveForContentLength(HINTERNET request, std::vector<u8> &outBody, const HttpProgress &progress)
{
	DWORD contentLength = 0;
	DWORD contentLengthSize = sizeof(contentLength);

	if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_FLAG_NUMBER | WINHTTP_QUERY_CONTENT_LENGTH,
							 WINHTTP_HEADER_NAME_BY_INDEX, &contentLength, &contentLengthSize,
							 WINHTTP_NO_HEADER_INDEX)) {
		return;
	}

	if (progress.pTotalBytes != nullptr) {
		progress.pTotalBytes->store(contentLength, std::memory_order_relaxed);
	}

	outBody.reserve(contentLength);
}

EHttpResult ReadResponseBody(HINTERNET request, std::vector<u8> &outBody, const HttpProgress &progress,
							 std::string &outError)
{
	const ULONGLONG startTick = GetTickCount64();
	u64 totalRead = 0;

	for (;;) {
		if (progress.pCancelRequested != nullptr && progress.pCancelRequested->load(std::memory_order_relaxed)) {
			return EHttpResult::Cancelled;
		}

		DWORD available = 0;
		if (!WinHttpQueryDataAvailable(request, &available)) {
			outError = "the connection was interrupted while reading";
			return EHttpResult::Failed;
		}

		if (available == 0) return EHttpResult::Ok;

		const usize previousSize = outBody.size();
		outBody.resize(previousSize + available);

		DWORD bytesRead = 0;
		if (!WinHttpReadData(request, outBody.data() + previousSize, available, &bytesRead)) {
			outError = "the connection was interrupted while reading";
			return EHttpResult::Failed;
		}

		outBody.resize(previousSize + bytesRead);
		totalRead += bytesRead;

		if (progress.pBytesDownloaded != nullptr) {
			progress.pBytesDownloaded->store(totalRead, std::memory_order_relaxed);
		}

		const ULONGLONG elapsedMs = GetTickCount64() - startTick;
		if (progress.pBytesPerSecond != nullptr && elapsedMs > 0) {
			const double seconds = static_cast<double>(elapsedMs) / 1000.0;
			progress.pBytesPerSecond->store(static_cast<double>(totalRead) / seconds, std::memory_order_relaxed);
		}
	}
}

// A blocking GET that follows redirects, including across hosts. The manifest fetch passes an
// empty progress block; cancellation is only polled between reads, which is fine for a few
// hundred bytes.
EHttpResult HttpGet(const std::wstring &url, std::vector<u8> &outBody, const HttpProgress &progress,
					std::string &outError)
{
	CrackedUrl cracked;
	if (!CrackUrl(url.c_str(), cracked)) {
		outError = "could not parse the update URL";
		return EHttpResult::Failed;
	}

	CInternetHandle session{WinHttpOpen(kUserAgent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
										WINHTTP_NO_PROXY_BYPASS, 0)};
	if (session.Handle == nullptr) {
		outError = "could not open an HTTP session";
		return EHttpResult::Failed;
	}

	ApplyRequestTimeouts(session);

	CInternetHandle connect{WinHttpConnect(session, cracked.Host.c_str(), cracked.Port, 0)};
	if (connect.Handle == nullptr) {
		outError = "could not connect to " + WideToUtf8(cracked.Host);
		return EHttpResult::Failed;
	}

	CInternetHandle request = OpenRequest(cracked, connect);
	if (request.Handle == nullptr) {
		outError = "could not open an HTTP request";
		return EHttpResult::Failed;
	}

	if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
		outError = "the request failed to send";
		return EHttpResult::Failed;
	}

	if (!WinHttpReceiveResponse(request, nullptr)) {
		outError = "no response was received";
		return EHttpResult::Failed;
	}

	DWORD statusCode = 0;
	ReadStatusCode(request, statusCode);
	if (statusCode < 200 || statusCode >= 300) {
		outError = "server returned HTTP " + std::to_string(statusCode);
		return EHttpResult::Failed;
	}

	ReserveForContentLength(request, outBody, progress);

	return ReadResponseBody(request, outBody, progress, outError);
}

void BytesToHexLower(const unsigned char *pBytes, usize length, char *pOut)
{
	static constexpr char kHexDigits[] = "0123456789abcdef";

	for (usize i = 0; i < length; i += 1) {
		pOut[i * 2] = kHexDigits[pBytes[i] >> 4];
		pOut[i * 2 + 1] = kHexDigits[pBytes[i] & 0x0F];
	}

	pOut[length * 2] = '\0';
}

// Integrity then authenticity - see updater.h on why neither substitutes for the other.
bool VerifyDownload(const std::vector<u8> &body, const UpdateManifest &manifest, std::string &outError)
{
	unsigned char digest[crypto_hash_sha256_BYTES];
	crypto_hash_sha256(digest, body.data(), body.size());

	char hex[crypto_hash_sha256_BYTES * 2 + 1];
	BytesToHexLower(digest, sizeof(digest), hex);

	if (_stricmp(hex, manifest.szSha256Hex) != 0) {
		outError = "the download doesn't match the manifest's SHA-256 - it may be corrupted or truncated";
		return false;
	}

	unsigned char signature[crypto_sign_BYTES];
	usize signatureLength = 0;
	if (sodium_base642bin(signature, sizeof(signature), manifest.szSignatureBase64,
						  std::strlen(manifest.szSignatureBase64), nullptr, &signatureLength, nullptr,
						  sodium_base64_VARIANT_ORIGINAL) != 0 ||
		signatureLength != crypto_sign_BYTES) {
		outError = "the manifest's signature is malformed";
		return false;
	}

	if (crypto_sign_verify_detached(signature, digest, sizeof(digest), rift::update::kEd25519PublicKey.data()) != 0) {
		outError = "signature verification failed - refusing to install an unsigned or tampered update";
		return false;
	}

	return true;
}

bool WriteFileBytes(const std::wstring &path, const std::vector<u8> &bytes)
{
	const HANDLE hFile =
		CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hFile == INVALID_HANDLE_VALUE) return false;

	DWORD written = 0;
	const BOOL wroteOk = WriteFile(hFile, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr);
	CloseHandle(hFile);

	return wroteOk != FALSE && written == bytes.size();
}

// The atomic path first: a same-volume MoveFileExW is one NTFS transaction, so the exe path
// never stops existing, and Windows permits renaming a running image's backing file.
//
// When that is refused (a file lock, some antivirus configurations), the fallback renames self
// to "<exe>.old" and moves the new build in. A crash between those two leaves "<exe>.old"
// holding a known-good build, which RunStartupRecoveryAndMaybeExit recovers from.
bool ApplyDownloadedExe(const std::vector<u8> &newExeBytes, std::string &outError)
{
	wchar_t selfPath[MAX_PATH];
	const DWORD selfPathLength = GetModuleFileNameW(nullptr, selfPath, MAX_PATH);
	if (selfPathLength == 0 || selfPathLength == MAX_PATH) {
		outError = "could not resolve this program's path";
		return false;
	}

	const std::wstring updatePath = std::wstring(selfPath) + L".update";
	const std::wstring oldPath = std::wstring(selfPath) + L".old";

	if (!WriteFileBytes(updatePath, newExeBytes)) {
		DeleteFileW(updatePath.c_str());
		outError = "could not write the downloaded update to disk (disk full?)";
		return false;
	}

	if (MoveFileExW(updatePath.c_str(), selfPath, MOVEFILE_REPLACE_EXISTING)) {
		DeleteFileW(oldPath.c_str()); // best effort, for a stale .old from a previous fallback
		return true;
	}

	if (!MoveFileExW(selfPath, oldPath.c_str(), MOVEFILE_REPLACE_EXISTING)) {
		DeleteFileW(updatePath.c_str());
		outError = "could not replace the running executable (it may be locked by another program)";
		return false;
	}

	if (!MoveFileExW(updatePath.c_str(), selfPath, MOVEFILE_REPLACE_EXISTING)) {
		MoveFileExW(oldPath.c_str(), selfPath, MOVEFILE_REPLACE_EXISTING); // best-effort restore
		outError = "could not move the new build into place";
		return false;
	}

	return true;
}

// Deletes a leftover "<exe>.old", retried briefly because the previous process can still hold
// it open for a moment after spawning this one.
void DeleteStaleBackup(const std::wstring &oldPath)
{
	for (int attempt = 0; attempt < 20; attempt += 1) {
		if (DeleteFileW(oldPath.c_str()) || GetLastError() == ERROR_FILE_NOT_FOUND) return;

		Sleep(50);
	}
}
} // namespace

CUpdater::~CUpdater()
{
	RequestCancel();

	// Bounded rather than a bare join: a WinHTTP call with no response and a cancellation flag
	// only checked between reads is the same "stuck inside one blocking call" risk the login
	// worker has.
	JoinWithTimeoutOrDetach(m_worker, std::chrono::milliseconds(3000));
}

void CUpdater::Init()
{
	m_stage.store(EUpdateStage::Idle, std::memory_order_relaxed);
	m_bCancelRequested.store(false, std::memory_order_relaxed);
	m_bWorkerFinished.store(false, std::memory_order_relaxed);
	m_bActive = false;
	m_bReadyToRelaunchLatched = false;
}

void CUpdater::CheckForUpdateAsync(const char *currentVersion)
{
	if (m_bActive) return;

	if (m_worker.joinable()) {
		m_worker.join();
	}

	m_bCancelRequested.store(false, std::memory_order_relaxed);
	m_bWorkerFinished.store(false, std::memory_order_relaxed);
	m_stage.store(EUpdateStage::Checking, std::memory_order_relaxed);
	m_bActive = true;

	std::string versionCopy(currentVersion);
	m_worker = std::thread(
		[this, versionCopy = std::move(versionCopy)]() mutable { WorkerCheckForUpdate(std::move(versionCopy)); });
}

void CUpdater::StartDownloadAsync()
{
	if (m_bActive || m_stage.load(std::memory_order_acquire) != EUpdateStage::Available) return;

	if (m_worker.joinable()) {
		m_worker.join();
	}

	m_bCancelRequested.store(false, std::memory_order_relaxed);
	m_bWorkerFinished.store(false, std::memory_order_relaxed);
	m_bActive = true;

	const UpdateManifest manifestCopy = m_manifest;
	m_worker = std::thread([this, manifestCopy]() { WorkerDownloadAndInstall(manifestCopy); });
}

void CUpdater::Update()
{
	if (!m_bActive || !m_bWorkerFinished.load(std::memory_order_acquire)) return;

	if (m_worker.joinable()) {
		m_worker.join();
	}

	m_bActive = false;

	if (GetStage() == EUpdateStage::ReadyToRelaunch) {
		m_bReadyToRelaunchLatched = true;
	}
}

bool CUpdater::ConsumeReadyToRelaunch()
{
	const bool ready = m_bReadyToRelaunchLatched;
	m_bReadyToRelaunchLatched = false;

	return ready;
}

void CUpdater::FinishWorker(EUpdateStage stage)
{
	m_stage.store(stage, std::memory_order_release);
	m_bWorkerFinished.store(true, std::memory_order_release);
}

void CUpdater::FailWorker(EUpdateStage stage, const char *pPrefix, const char *pDetail)
{
	std::snprintf(m_szErrorMessage, sizeof(m_szErrorMessage), "%s%s", pPrefix, pDetail);
	FinishWorker(stage);
}

void CUpdater::WorkerCheckForUpdate(std::string currentVersion)
{
	std::vector<u8> body;
	std::string error;

	if (HttpGet(kUpdateManifestUrl, body, HttpProgress{}, error) != EHttpResult::Ok) {
		FailWorker(EUpdateStage::CheckFailed, "Couldn't check for updates: ", error.c_str());
		return;
	}

	UpdateManifest manifest{};
	if (!ParseUpdateManifest(reinterpret_cast<const char *>(body.data()), body.size(), &manifest)) {
		FailWorker(EUpdateStage::CheckFailed, "Couldn't check for updates: ", "malformed manifest");
		return;
	}

	SemVer latest{};
	if (!SemVerParse(manifest.szVersion, latest)) {
		FailWorker(EUpdateStage::CheckFailed, "Couldn't check for updates: ", "manifest has an unparseable version");
		return;
	}

	SemVer current{};
	SemVer minUpgrade{};
	SemVerParse(currentVersion.c_str(), current);
	SemVerParse(manifest.szMinUpgradeVersion, minUpgrade);

	m_manifest = manifest;

	EUpdateStage finalStage = EUpdateStage::UpToDate;
	if (latest > current) {
		finalStage = current < minUpgrade ? EUpdateStage::ManualUpgradeRequired : EUpdateStage::Available;
	}

	FinishWorker(finalStage);
}

void CUpdater::WorkerDownloadAndInstall(UpdateManifest manifest)
{
	m_bytesDownloaded.store(0, std::memory_order_relaxed);
	m_totalBytes.store(0, std::memory_order_relaxed);
	m_bytesPerSecond.store(0.0, std::memory_order_relaxed);
	m_stage.store(EUpdateStage::Downloading, std::memory_order_release);

	const HttpProgress progress{&m_bCancelRequested, &m_bytesDownloaded, &m_totalBytes, &m_bytesPerSecond};

	std::vector<u8> body;
	std::string error;
	const EHttpResult result = HttpGet(Utf8ToWide(manifest.szUrl), body, progress, error);

	if (result == EHttpResult::Cancelled) {
		FinishWorker(EUpdateStage::Cancelled);
		return;
	}

	if (result != EHttpResult::Ok) {
		FailWorker(EUpdateStage::Error, "Download failed: ", error.c_str());
		return;
	}

	m_stage.store(EUpdateStage::Verifying, std::memory_order_release);
	if (!VerifyDownload(body, manifest, error)) {
		FailWorker(EUpdateStage::Error, "", error.c_str());
		return;
	}

	if (m_bCancelRequested.load(std::memory_order_relaxed)) {
		FinishWorker(EUpdateStage::Cancelled);
		return;
	}

	m_stage.store(EUpdateStage::Installing, std::memory_order_release);
	if (!ApplyDownloadedExe(body, error)) {
		FailWorker(EUpdateStage::Error, "", error.c_str());
		return;
	}

	FinishWorker(EUpdateStage::ReadyToRelaunch);
}

bool CUpdater::RunStartupRecoveryAndMaybeExit()
{
	wchar_t selfPathBuffer[MAX_PATH];
	const DWORD length = GetModuleFileNameW(nullptr, selfPathBuffer, MAX_PATH);
	if (length == 0 || length == MAX_PATH) return false;

	const std::wstring selfPath(selfPathBuffer);

	constexpr std::wstring_view kOldSuffix = L".old";
	const bool isOldCopy = selfPath.size() > kOldSuffix.size() &&
						   selfPath.compare(selfPath.size() - kOldSuffix.size(), kOldSuffix.size(), kOldSuffix) == 0;

	if (!isOldCopy) {
		DeleteStaleBackup(selfPath + L".old");
		return false;
	}

	// This process is the ".old" leftover, launched directly by a stale shortcut or a curious
	// user. If the canonical exe is missing, the two-step fallback crashed between its renames -
	// exactly the window this exists to close. These bytes are known good, so recovery is just
	// copying them back under the canonical name and handing off.
	const std::wstring canonicalPath = selfPath.substr(0, selfPath.size() - kOldSuffix.size());
	if (GetFileAttributesW(canonicalPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
		CopyFileW(selfPath.c_str(), canonicalPath.c_str(), FALSE);
	}

	STARTUPINFOW startupInfo{sizeof(startupInfo)};
	PROCESS_INFORMATION processInfo{};

	if (CreateProcessW(canonicalPath.c_str(), nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startupInfo,
					   &processInfo)) {
		CloseHandle(processInfo.hProcess);
		CloseHandle(processInfo.hThread);
	}

	return true;
}
