#include "core/crypto.h"

#include <sodium.h>

void CCrypto::RandomBytes(u8 *pOut, u32 length)
{
	randombytes_buf(pOut, length);
}

bool CCrypto::Argon2idDeriveKey(std::string_view password, const u8 salt[kSaltSize], u64 opsLimit, usize memLimit,
								u8 outKey[kKeySize])
{
	return crypto_pwhash(outKey, kKeySize, password.data(), static_cast<unsigned long long>(password.size()), salt,
						 opsLimit, memLimit, crypto_pwhash_ALG_ARGON2ID13) == 0;
}

u64 CCrypto::DefaultOpsLimit()
{
	return crypto_pwhash_OPSLIMIT_MODERATE;
}

usize CCrypto::DefaultMemLimit()
{
	return crypto_pwhash_MEMLIMIT_MODERATE;
}

bool CCrypto::Encrypt(const u8 key[kKeySize], const u8 nonce[kNonceSize], const u8 *pPlaintext, u32 length,
					  u8 *pCiphertext, u8 outTag[kTagSize])
{
	unsigned long long tagLength = 0;

	return crypto_aead_xchacha20poly1305_ietf_encrypt_detached(pCiphertext, outTag, &tagLength, pPlaintext, length,
															   nullptr, 0, nullptr, nonce, key) == 0;
}

bool CCrypto::Decrypt(const u8 key[kKeySize], const u8 nonce[kNonceSize], const u8 *pCiphertext, u32 length,
					  const u8 tag[kTagSize], u8 *pOutPlaintext)
{
	return crypto_aead_xchacha20poly1305_ietf_decrypt_detached(pOutPlaintext, nullptr, pCiphertext, length, tag,
															   nullptr, 0, nonce, key) == 0;
}
