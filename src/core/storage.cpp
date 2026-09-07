#include "core/storage.h"

#include "core/app_identity.h"
#include "core/debug_log.h"
#include "core/str.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <Windows.h>

#include <nlohmann/json.hpp>
#include <sodium.h>

#include "core/account.h"
#include "core/atomic_file.h"
#include "core/crypto.h"
#include <string_view>

using nlohmann::json;

namespace {
constexpr int kFormatVersion = 1;
constexpr const char *kAccountsFileName = "accounts.vault";
constexpr const char *kSettingsFileName = "settings.json";

// Cleared by a failed load, never set again. See CStorage::IsSettingsWritable.
bool g_bSettingsWritable = true;
bool g_bAccountsWritable = true;

// A load result that must seal the file: the file is there, but nothing readable came out of
// either it or its .bak, so whatever is in memory is defaults rather than the user's data.
// NoFile is not that - a first run has nothing to protect and must be free to write.
bool SealsFile(EStorageLoadResult result)
{
	return result == EStorageLoadResult::Failed;
}

bool PathExists(const char *pPath)
{
	return GetFileAttributesA(pPath) != INVALID_FILE_ATTRIBUTES;
}

// Moves one data file out of the newest legacy folder that still has it. Keyed on the file rather
// than on the folder, because the folder is not a reliable signal: the diagnostic log creates the
// new folder during startup, well before storage is first touched, so a folder-level check would
// find it already there and conclude there was nothing to migrate.
//
// A file already present under the new name always wins - this only ever fills a gap, so running
// an old build again after migrating cannot clobber newer data on the way back.
void MigrateLegacyFile(const char *pLocalAppData, const char *pNewDir, const char *pFileName)
{
	char newPath[MAX_PATH];
	std::snprintf(newPath, sizeof(newPath), "%s\\%s", pNewDir, pFileName);

	if (PathExists(newPath)) return;

	for (const char *pLegacyName : kLegacyDataFolderNames) {
		char legacyPath[MAX_PATH];
		std::snprintf(legacyPath, sizeof(legacyPath), "%s\\%s\\%s", pLocalAppData, pLegacyName, pFileName);

		if (PathExists(legacyPath) && CopyFileA(legacyPath, newPath, TRUE)) {
			DebugLog::Write("storage", "migrated %s from the %s folder", pFileName, pLegacyName);
			return;
		}
	}
}

bool GetStorageDirectory(char *pBuffer, usize bufferSize)
{
	char localAppData[MAX_PATH];
	const DWORD length = GetEnvironmentVariableA("LOCALAPPDATA", localAppData, sizeof(localAppData));
	if (length == 0 || length >= sizeof(localAppData)) return false;

	char dir[MAX_PATH];
	std::snprintf(dir, sizeof(dir), "%s\\%s", localAppData, kAppDataFolderName);
	CreateDirectoryA(dir, nullptr);

	// Copied rather than moved, so a rollback to a build that still looks under the old name
	// finds its data intact. The duplicate is the price of that, and it is two small files.
	MigrateLegacyFile(localAppData, dir, kAccountsFileName);
	MigrateLegacyFile(localAppData, dir, kSettingsFileName);

	const int written = std::snprintf(pBuffer, bufferSize, "%s", dir);

	return written > 0 && static_cast<usize>(written) < bufferSize;
}

bool GetStorageFilePath(const char *pFileName, char *pBuffer, usize bufferSize)
{
	char dir[MAX_PATH];
	if (!GetStorageDirectory(dir, sizeof(dir))) return false;

	const int written = std::snprintf(pBuffer, bufferSize, "%s\\%s", dir, pFileName);

	return written > 0 && static_cast<usize>(written) < bufferSize;
}

bool FileExists(const char *pPath)
{
	FILE *pFile = nullptr;
	const bool exists = fopen_s(&pFile, pPath, "rb") == 0 && pFile != nullptr;

	if (pFile != nullptr) {
		std::fclose(pFile);
	}

	return exists;
}

std::string BytesToHex(const u8 *pData, usize length)
{
	std::string out(length * 2 + 1, '\0');
	sodium_bin2hex(out.data(), out.size(), pData, length);
	out.resize(length * 2);

	return out;
}

// False - leaving pOut untouched - unless hex decodes to exactly outLength bytes. A key
// that does not exist yet in an older file lands here and keeps whatever pOut already had.
bool HexToBytes(const std::string &hex, u8 *pOut, usize outLength)
{
	if (hex.size() != outLength * 2) return false;

	usize decodedLength = 0;

	return sodium_hex2bin(pOut, outLength, hex.c_str(), hex.size(), nullptr, &decodedLength, nullptr) == 0 &&
		   decodedLength == outLength;
}

std::string BytesToBase64(const u8 *pData, usize length)
{
	const usize encodedLength = sodium_base64_ENCODED_LEN(length, sodium_base64_VARIANT_ORIGINAL);

	std::string out(encodedLength, '\0');
	sodium_bin2base64(out.data(), out.size(), pData, length, sodium_base64_VARIANT_ORIGINAL);
	out.resize(std::strlen(out.c_str()));

	return out;
}

bool Base64ToBytes(const std::string &base64, std::vector<u8> &outBytes)
{
	outBytes.resize(base64.size()); // base64 never decodes to more bytes than it encodes to

	usize decodedLength = 0;
	if (sodium_base642bin(outBytes.data(), outBytes.size(), base64.c_str(), base64.size(), nullptr, &decodedLength,
						  nullptr, sodium_base64_VARIANT_ORIGINAL) != 0) {
		return false;
	}

	outBytes.resize(decodedLength);

	return true;
}

bool ParseJsonFile(const char *pPath, json &outJson)
{
	u8 *pData = nullptr;
	usize length = 0;
	if (!CAtomicFile::ReadFile(pPath, &pData, &length)) return false;

	bool ok = false;
	try {
		outJson = json::parse(pData, pData + length);
		ok = outJson.is_object();
	} catch (const json::exception &) {
		ok = false;
	}

	std::free(pData);

	return ok;
}

// A corrupted file is an expected, recoverable condition here rather than a programming
// error, so the .bak sibling gets a turn before this gives up.
bool ReadJsonWithFallback(const char *pPath, json &outJson)
{
	if (ParseJsonFile(pPath, outJson)) return true;

	char backupPath[MAX_PATH];

	return CAtomicFile::BackupPathFor(pPath, backupPath, sizeof(backupPath)) && ParseJsonFile(backupPath, outJson);
}

// Account::Init asserts rather than truncates, and asserts compile out of a Release build,
// so an over-long decrypted string would overflow. Successful decryption already rules out
// tampering, but not a future bug in this file's serialization.
std::string_view TruncatedView(const std::string &value, u64 maxLength)
{
	return std::string_view{value.data(), std::min<u64>(value.size(), maxLength)};
}

json MasterPasswordToJson(const Settings &settings)
{
	json out;
	out["enabled"] = settings.m_bMasterPasswordEnabled;
	out["salt_hex"] = BytesToHex(settings.m_aMasterPasswordSalt, sizeof(settings.m_aMasterPasswordSalt));
	out["ops_limit"] = settings.m_masterPasswordOpsLimit;
	out["mem_limit"] = static_cast<u64>(settings.m_masterPasswordMemLimit);
	out["wrap_nonce_hex"] =
		BytesToHex(settings.m_aMasterPasswordWrapNonce, sizeof(settings.m_aMasterPasswordWrapNonce));
	out["wrapped_dek_hex"] =
		BytesToHex(settings.m_aMasterPasswordWrappedDek, sizeof(settings.m_aMasterPasswordWrappedDek));

	return out;
}

void MasterPasswordFromJson(const json &j, Settings &settings)
{
	settings.m_bMasterPasswordEnabled = false;

	if (!j.contains("master_password") || !j["master_password"].is_object()) return;

	const json &mp = j["master_password"];
	settings.m_bMasterPasswordEnabled = mp.value("enabled", false);
	settings.m_masterPasswordOpsLimit = mp.value("ops_limit", static_cast<u64>(0));
	settings.m_masterPasswordMemLimit = static_cast<usize>(mp.value("mem_limit", static_cast<u64>(0)));

	HexToBytes(mp.value("salt_hex", std::string()), settings.m_aMasterPasswordSalt,
			   sizeof(settings.m_aMasterPasswordSalt));
	HexToBytes(mp.value("wrap_nonce_hex", std::string()), settings.m_aMasterPasswordWrapNonce,
			   sizeof(settings.m_aMasterPasswordWrapNonce));
	HexToBytes(mp.value("wrapped_dek_hex", std::string()), settings.m_aMasterPasswordWrappedDek,
			   sizeof(settings.m_aMasterPasswordWrappedDek));
}

void ReadAppearanceFromJson(const json &j, Settings &settings)
{
	settings.m_bAnimationsEnabled = j.value("animations_enabled", settings.m_bAnimationsEnabled);
	settings.m_flAnimationSpeed = j.value("animation_speed", settings.m_flAnimationSpeed);
	settings.m_flFontPixelSize = j.value("font_pixel_size", settings.m_flFontPixelSize);
	settings.m_flSecondaryFontPixelSize = j.value("secondary_font_pixel_size", settings.m_flSecondaryFontPixelSize);
	settings.m_flCornerRoundness = j.value("corner_roundness", settings.m_flCornerRoundness);

	// The roundness slider replaced a plain on/off toggle, which is no longer written.
	// Still honored on read so an install that had corners off does not get them back.
	if (!j.value("rounded_corners_enabled", true)) {
		settings.m_flCornerRoundness = 0.0f;
	}

	if (j.contains("accent_color") && j["accent_color"].is_array() && j["accent_color"].size() == 4) {
		const json &a = j["accent_color"];
		settings.m_clrAccent = Color{a[0].get<u8>(), a[1].get<u8>(), a[2].get<u8>(), a[3].get<u8>()};
	}

	const std::string fontName = j.value("font_name", std::string(settings.m_szFontName));
	CopyTo(TruncatedView(fontName, sizeof(settings.m_szFontName) - 1), settings.m_szFontName,
		   sizeof(settings.m_szFontName));
}

json BannerToJson(const Banner &banner)
{
	json accounts = json::array();
	for (u32 i = 0; i < banner.AccountCount; i += 1) {
		const Account &account = banner.Accounts[i];

		json entry;
		entry["username"] = std::string(account.m_szUsername);
		entry["note"] = std::string(account.m_szNote);
		entry["password"] = std::string(account.m_szPassword);
		entry["visible_mask"] = account.m_uVisibleBannerMask;
		accounts.push_back(std::move(entry));
	}

	json out;
	out["title"] = std::string(banner.Title.data(), banner.Title.size());
	out["accounts"] = std::move(accounts);

	return out;
}

void ReadAccountsIntoBanner(const json &source, Banner &banner)
{
	banner.AccountCount = 0;

	if (!source.contains("accounts") || !source["accounts"].is_array()) return;

	for (const json &entry : source["accounts"]) {
		if (banner.AccountCount >= kCarouselMaxAccountsPerBanner) break;

		const std::string username = entry.value("username", std::string());
		const std::string note = entry.value("note", std::string());
		const std::string password = entry.value("password", std::string());

		Account &account = banner.Accounts[banner.AccountCount];
		account.Init(TruncatedView(username, sizeof(account.m_szUsername) - 1),
					 TruncatedView(note, sizeof(account.m_szNote) - 1),
					 TruncatedView(password, sizeof(account.m_szPassword) - 1));
		account.m_uVisibleBannerMask = entry.value("visible_mask", static_cast<u16>(0));
		banner.AccountCount += 1;
	}
}

Banner *FindBannerByTitle(Banner *pBanners, u32 bannerCount, const std::string &title)
{
	for (u32 i = 0; i < bannerCount; i += 1) {
		if (pBanners[i].Title == title.c_str()) return &pBanners[i];
	}

	return nullptr;
}

bool DecryptVault(const json &envelope, const CMasterKey &masterKey, std::vector<u8> &outPlaintext)
{
	u8 nonce[CCrypto::kNonceSize];
	u8 tag[CCrypto::kTagSize];
	std::vector<u8> ciphertext;

	if (!HexToBytes(envelope.value("nonce_hex", std::string()), nonce, sizeof(nonce)) ||
		!HexToBytes(envelope.value("tag_hex", std::string()), tag, sizeof(tag)) ||
		!Base64ToBytes(envelope.value("ciphertext_b64", std::string()), ciphertext)) {
		return false;
	}

	outPlaintext.resize(ciphertext.size());

	return CCrypto::Decrypt(masterKey.m_aDek, nonce, ciphertext.data(), static_cast<u32>(ciphertext.size()), tag,
							outPlaintext.data());
}
} // namespace

bool CStorage::GetDataDirectory(char *pBuffer, usize bufferSize)
{
	return GetStorageDirectory(pBuffer, bufferSize);
}

bool CStorage::IsSettingsWritable()
{
	return g_bSettingsWritable;
}

bool CStorage::IsAccountsWritable()
{
	return g_bAccountsWritable;
}

bool CStorage::SaveSettings(const Settings &settings, i32 carouselZoomStop, i32 carouselSelectedBanner)
{
	if (!g_bSettingsWritable) return false;

	json j;
	j["format_version"] = kFormatVersion;
	j["window_width"] = settings.m_nWindowWidth;
	j["window_height"] = settings.m_nWindowHeight;
	j["animations_enabled"] = settings.m_bAnimationsEnabled;
	j["animation_speed"] = settings.m_flAnimationSpeed;
	j["corner_roundness"] = settings.m_flCornerRoundness;
	j["font_pixel_size"] = settings.m_flFontPixelSize;
	j["secondary_font_pixel_size"] = settings.m_flSecondaryFontPixelSize;
	j["accent_color"] = {settings.m_clrAccent.R, settings.m_clrAccent.G, settings.m_clrAccent.B,
						 settings.m_clrAccent.A};
	j["font_name"] = std::string(settings.m_szFontName);
	j["exclude_account_list_from_capture"] = settings.m_bExcludeAccountListFromCapture;
	j["close_to_tray"] = settings.m_bCloseToTray;
	j["block_overlay_injection"] = settings.m_bBlockOverlayInjection;
	j["show_notifications"] = settings.m_bShowNotifications;
	j["last_run_version"] = settings.m_szLastRunVersion;
	j["carousel_zoom_stop"] = carouselZoomStop;
	j["carousel_selected_banner"] = carouselSelectedBanner;
	j["master_password"] = MasterPasswordToJson(settings);

	char path[MAX_PATH];
	if (!GetStorageFilePath(kSettingsFileName, path, sizeof(path))) return false;

	const std::string text = j.dump(2);

	return CAtomicFile::WriteAtomic(path, text.data(), text.size());
}

static EStorageLoadResult LoadSettingsFromDisk(Settings &settings, i32 &outCarouselZoomStop,
											   i32 &outCarouselSelectedBanner)
{
	char path[MAX_PATH];
	if (!GetStorageFilePath(kSettingsFileName, path, sizeof(path))) return EStorageLoadResult::Failed;

	const bool fileExists = FileExists(path);

	json j;
	if (!ReadJsonWithFallback(path, j)) return fileExists ? EStorageLoadResult::Failed : EStorageLoadResult::NoFile;

	settings.m_nWindowWidth = j.value("window_width", settings.m_nWindowWidth);
	settings.m_nWindowHeight = j.value("window_height", settings.m_nWindowHeight);
	settings.m_bExcludeAccountListFromCapture =
		j.value("exclude_account_list_from_capture", settings.m_bExcludeAccountListFromCapture);

	// This setting was "minimize_to_tray" back when it hooked minimize instead of close;
	// read as the fallback so an existing file keeps the user's choice.
	settings.m_bCloseToTray = j.value("close_to_tray", j.value("minimize_to_tray", settings.m_bCloseToTray));
	settings.m_bBlockOverlayInjection = j.value("block_overlay_injection", settings.m_bBlockOverlayInjection);
	settings.m_bShowNotifications = j.value("show_notifications", settings.m_bShowNotifications);
	CopyTo(TruncatedView(j.value("last_run_version", std::string()), sizeof(settings.m_szLastRunVersion) - 1),
		   settings.m_szLastRunVersion, sizeof(settings.m_szLastRunVersion));

	outCarouselZoomStop = j.value("carousel_zoom_stop", outCarouselZoomStop);
	outCarouselSelectedBanner = j.value("carousel_selected_banner", outCarouselSelectedBanner);

	ReadAppearanceFromJson(j, settings);
	MasterPasswordFromJson(j, settings);

	return EStorageLoadResult::Ok;
}

bool CStorage::SaveAccounts(const Banner *pBanners, u32 bannerCount, bool masterPasswordEnabled,
							const CMasterKey &masterKey)
{
	if (!masterPasswordEnabled || !masterKey.m_bEnabled || !g_bAccountsWritable) return false;

	json banners = json::array();
	for (u32 i = 0; i < bannerCount; i += 1) {
		banners.push_back(BannerToJson(pBanners[i]));
	}

	const std::string plaintext = banners.dump();

	u8 nonce[CCrypto::kNonceSize];
	CCrypto::RandomBytes(nonce, sizeof(nonce));

	u8 tag[CCrypto::kTagSize];
	std::vector<u8> ciphertext(plaintext.size());
	if (!CCrypto::Encrypt(masterKey.m_aDek, nonce, reinterpret_cast<const u8 *>(plaintext.data()),
						  static_cast<u32>(plaintext.size()), ciphertext.data(), tag)) {
		return false;
	}

	json envelope;
	envelope["format_version"] = kFormatVersion;
	envelope["nonce_hex"] = BytesToHex(nonce, sizeof(nonce));
	envelope["tag_hex"] = BytesToHex(tag, sizeof(tag));
	envelope["ciphertext_b64"] = BytesToBase64(ciphertext.data(), ciphertext.size());

	char path[MAX_PATH];
	if (!GetStorageFilePath(kAccountsFileName, path, sizeof(path))) return false;

	const std::string text = envelope.dump();

	return CAtomicFile::WriteAtomic(path, text.data(), text.size());
}

static EStorageLoadResult LoadAccountsFromDisk(Banner *pBanners, u32 bannerCount, bool masterPasswordEnabled,
											   const CMasterKey &masterKey)
{
	if (!masterPasswordEnabled || !masterKey.m_bEnabled) return EStorageLoadResult::Locked;

	char path[MAX_PATH];
	if (!GetStorageFilePath(kAccountsFileName, path, sizeof(path))) return EStorageLoadResult::Failed;

	const bool fileExists = FileExists(path);

	json envelope;
	if (!ReadJsonWithFallback(path, envelope)) {
		return fileExists ? EStorageLoadResult::Failed : EStorageLoadResult::NoFile;
	}

	std::vector<u8> plaintext;
	if (!DecryptVault(envelope, masterKey, plaintext)) return EStorageLoadResult::Failed;

	json banners;
	try {
		banners = json::parse(plaintext.begin(), plaintext.end());
	} catch (const json::exception &) {
		return EStorageLoadResult::Failed;
	}

	if (!banners.is_array()) return EStorageLoadResult::Failed;

	for (const json &entry : banners) {
		Banner *pBanner = FindBannerByTitle(pBanners, bannerCount, entry.value("title", std::string()));
		if (pBanner != nullptr) {
			ReadAccountsIntoBanner(entry, *pBanner);
		}
	}

	return EStorageLoadResult::Ok;
}

EStorageLoadResult CStorage::LoadSettings(Settings &settings, i32 &outCarouselZoomStop, i32 &outCarouselSelectedBanner)
{
	const EStorageLoadResult result = LoadSettingsFromDisk(settings, outCarouselZoomStop, outCarouselSelectedBanner);

	// Sealing settings has to seal the vault with it. accounts.vault is only interpretable through
	// the KEK parameters in settings.json, so losing those means the app sees "no master password",
	// offers to set one up, and a fresh key would encrypt the empty in-memory list straight over
	// the real accounts. The vault stays readable to a later launch precisely by refusing this one.
	if (SealsFile(result)) {
		g_bSettingsWritable = false;
		g_bAccountsWritable = false;
		DebugLog::Write("storage", "%s did not load - neither file will be written this session", kSettingsFileName);
	}

	return result;
}

EStorageLoadResult CStorage::LoadAccounts(Banner *pBanners, u32 bannerCount, bool masterPasswordEnabled,
										  const CMasterKey &masterKey)
{
	const EStorageLoadResult result = LoadAccountsFromDisk(pBanners, bannerCount, masterPasswordEnabled, masterKey);

	if (SealsFile(result)) {
		g_bAccountsWritable = false;
		DebugLog::Write("storage", "%s did not load - it will not be written this session", kAccountsFileName);
	}

	return result;
}
