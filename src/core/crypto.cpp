#include "core/crypto.h"

#include <sodium.h>

void crypto::random_bytes(std::span<u8> t_out)
{
	randombytes_buf(t_out.data(), t_out.size());
}

bool crypto::derive_key(std::string_view t_password, const u8 *t_salt, u64 t_ops_limit, usize t_mem_limit,
						u8 *t_out_key)
{
	return crypto_pwhash(t_out_key, key_size, t_password.data(), t_password.size(), t_salt, t_ops_limit, t_mem_limit,
						 crypto_pwhash_ALG_ARGON2ID13) == 0;
}

u64 crypto::default_ops_limit()
{
	return crypto_pwhash_OPSLIMIT_MODERATE;
}

usize crypto::default_mem_limit()
{
	return crypto_pwhash_MEMLIMIT_MODERATE;
}

bool crypto::encrypt(const u8 *t_key, const u8 *t_nonce, std::span<const u8> t_plaintext, u8 *t_out_ciphertext,
					 u8 *t_out_tag)
{
	return crypto_aead_xchacha20poly1305_ietf_encrypt_detached(t_out_ciphertext, t_out_tag, nullptr, t_plaintext.data(),
															   t_plaintext.size(), nullptr, 0, nullptr, t_nonce,
															   t_key) == 0;
}

bool crypto::decrypt(const u8 *t_key, const u8 *t_nonce, std::span<const u8> t_ciphertext, const u8 *t_tag,
					 u8 *t_out_plaintext)
{
	return crypto_aead_xchacha20poly1305_ietf_decrypt_detached(t_out_plaintext, nullptr, t_ciphertext.data(),
															   t_ciphertext.size(), t_tag, nullptr, 0, t_nonce,
															   t_key) == 0;
}
