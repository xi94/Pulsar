#pragma once

#include "core/banner.h"
#include "core/master_key.h"
#include "core/settings.h"

// Local persistence, split across two independent files under %LOCALAPPDATA%\Pulsar:
// settings.json (preferences plus the master password's KEK parameters) and
// accounts.vault (every banner's account list).
//
// Both are versioned JSON, and every field is read individually against an explicit
// default. That is the point of the format: a missing, extra, or reordered key is a no-op
// for every other key. The fixed-layout struct this replaced treated any size change as
// "file absent", which once silently reset m_bMasterPasswordEnabled while accounts.vault
// still held real ciphertext, and cost a user their saved accounts.
//
// settings.json is never encrypted, because nothing in it is secret. Its master-password
// fields are exactly what CMasterKey::Unlock needs before it can derive anything at all -
// the salt cannot be encrypted under the key that needs the salt to derive - and Argon2id
// plus the AEAD wrap is what protects them. accounts.vault holds real user data and is
// encrypted whole under the DEK before it ever reaches disk.
//
// Accounts are never read at all while locked: the blob stays opaque ciphertext, banners
// simply show no accounts, and that is the complete literal meaning of "locked" here.
//
// Both files go through CAtomicFile, so a crash mid-write cannot leave a half-written file
// and a corrupted primary recovers from its .bak sibling.

enum class EStorageLoadResult : u8 {
	NoFile, // normal first run
	Failed, // neither the primary nor its .bak parsed or decrypted; treat as NoFile
	Ok,
	Locked, // accounts.vault was never read - pBanners is left exactly as it was
};

class CStorage {
  public:
	/// %LOCALAPPDATA%\Pulsar, created if absent. Public for callers that want the directory
	/// itself: the "open my data folder" action, and crash_handler's dump folder.
	static bool GetDataDirectory(char *pBuffer, usize bufferSize);

	/// Fails while locked - there is nothing trustworthy in memory to encrypt, since
	/// accounts are never loaded in that state either.
	static bool SaveAccounts(const Banner *pBanners, u32 bannerCount, bool masterPasswordEnabled,
							 const CMasterKey &masterKey);

	static bool SaveSettings(const Settings &settings, i32 carouselZoomStop, i32 carouselSelectedBanner);

	/// False once a load of that file returned Failed. A file that exists but could not be read
	/// leaves memory holding defaults, and saving those would overwrite the real contents with
	/// them - so the first failed load seals the file for the rest of the session and every
	/// later save of it is refused. Nothing re-opens it but a launch that loads it cleanly,
	/// which is what makes the on-disk copy survive long enough to be worth recovering.
	static bool IsSettingsWritable();
	static bool IsAccountsWritable();

	/// Replaces the account list of each banner whose title matches one in the file;
	/// banners the file does not mention are left alone. A wrong key fails the AEAD tag
	/// check outright rather than producing garbage.
	static EStorageLoadResult LoadAccounts(Banner *pBanners, u32 bannerCount, bool masterPasswordEnabled,
										   const CMasterKey &masterKey);

	static EStorageLoadResult LoadSettings(Settings &settings, i32 &outCarouselZoomStop,
										   i32 &outCarouselSelectedBanner);
};
