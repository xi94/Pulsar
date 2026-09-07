#pragma once

#include <string_view>

/// libsodium primitives behind the master password: Argon2id to turn the typed password
/// into a key, XChaCha20-Poly1305 to encrypt with it.
///
/// Argon2id over PBKDF2 because it is memory-hard, so guessing a stolen vault's password
/// costs real money per attempt even on custom hardware. XChaCha20-Poly1305 over AES-GCM
/// because its 192-bit nonce is safe to draw at random for every single save; GCM's 96-bit
/// nonce is not, and the deterministic scheme that avoids that reuses a nonce on every
/// re-save instead.
class CCrypto {
  public:
	static constexpr u32 kKeySize = 32;
	static constexpr u32 kSaltSize = 16;
	static constexpr u32 kNonceSize = 24;
	static constexpr u32 kTagSize = 16;

	/// libsodium aborts the process itself if the OS entropy source is unavailable, so
	/// there is no failure to report here.
	static void RandomBytes(u8 *pOut, u32 length);

	/// opsLimit/memLimit are parameters rather than constants so a vault keeps deriving with
	/// whatever it was created with - retuning the defaults must never lock anyone out.
	static bool Argon2idDeriveKey(std::string_view password, const u8 salt[kSaltSize], u64 opsLimit, usize memLimit,
								  u8 outKey[kKeySize]);

	/// libsodium's "moderate" preset, used only when setting a brand-new password.
	static u64 DefaultOpsLimit();
	static usize DefaultMemLimit();

	/// Ciphertext is the same length as plaintext; the tag is a separate output.
	static bool Encrypt(const u8 key[kKeySize], const u8 nonce[kNonceSize], const u8 *pPlaintext, u32 length,
						u8 *pCiphertext, u8 outTag[kTagSize]);

	/// False on a wrong key or a tampered ciphertext or tag, leaving pOutPlaintext
	/// unspecified.
	static bool Decrypt(const u8 key[kKeySize], const u8 nonce[kNonceSize], const u8 *pCiphertext, u32 length,
						const u8 tag[kTagSize], u8 *pOutPlaintext);
};
