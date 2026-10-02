#include "core/crypto.h"

#include <algorithm>
#include <cstring>
#include <utility>

#include <sodium.h>

auto crypto::random_bytes(std::span<u8> t_out) -> void
{
	randombytes_buf(t_out.data(), t_out.size());
}

auto crypto::derive_key(std::string_view t_password, const u8* t_salt, u64 t_ops_limit, usize t_mem_limit, u8* t_out_key) -> bool
{
	return crypto_pwhash(t_out_key, K_KEY_SIZE, t_password.data(), t_password.size(), t_salt, t_ops_limit, t_mem_limit, crypto_pwhash_ALG_ARGON2ID13) == 0;
}

auto crypto::default_ops_limit() -> u64
{
	return crypto_pwhash_OPSLIMIT_MODERATE;
}

auto crypto::default_mem_limit() -> usize
{
	return crypto_pwhash_MEMLIMIT_MODERATE;
}

auto crypto::encrypt(const u8* t_key, const u8* t_nonce, std::span<const u8> t_plaintext, u8* t_out_ciphertext, u8* t_out_tag) -> bool
{
	return crypto_aead_xchacha20poly1305_ietf_encrypt_detached(t_out_ciphertext, t_out_tag, nullptr, t_plaintext.data(), t_plaintext.size(), nullptr, 0,
	                                                           nullptr, t_nonce, t_key) == 0;
}

auto crypto::decrypt(const u8* t_key, const u8* t_nonce, std::span<const u8> t_ciphertext, const u8* t_tag, u8* t_out_plaintext) -> bool
{
	return crypto_aead_xchacha20poly1305_ietf_decrypt_detached(t_out_plaintext, nullptr, t_ciphertext.data(), t_ciphertext.size(), t_tag, nullptr, 0, t_nonce,
	                                                           t_key) == 0;
}

MasterKey::~MasterKey()
{
	lock();
}

auto MasterKey::lock() -> void
{
	sodium_memzero(m_data_key, sizeof(m_data_key));
	m_unlocked = false;
}

auto MasterKey::swap(MasterKey* t_other) -> void
{
	std::swap_ranges(std::begin(m_data_key), std::end(m_data_key), std::begin(t_other->m_data_key));
	std::swap(m_unlocked, t_other->m_unlocked);
}

auto MasterKey::create(std::string_view t_password, MasterKeyParams* t_out_params) -> bool
{
	MasterKeyParams params{
		.ops_limit = crypto::default_ops_limit(),
		.mem_limit = crypto::default_mem_limit(),
	};

	crypto::random_bytes(params.salt);
	crypto::random_bytes(params.wrap_nonce);

	u8 data_key[crypto::K_KEY_SIZE];
	crypto::random_bytes(data_key);

	u8 key_encryption_key[crypto::K_KEY_SIZE];
	if (!crypto::derive_key(t_password, params.salt, params.ops_limit, params.mem_limit, key_encryption_key)) {
		sodium_memzero(data_key, sizeof(data_key));
		return false;
	}

	const bool wrapped =
		crypto::encrypt(key_encryption_key, params.wrap_nonce, data_key, params.wrapped_data_key, params.wrapped_data_key + crypto::K_KEY_SIZE);
	sodium_memzero(key_encryption_key, sizeof(key_encryption_key));

	if (wrapped) {
		std::memcpy(m_data_key, data_key, sizeof(m_data_key));
		m_unlocked    = true;
		*t_out_params = params;
	}

	sodium_memzero(data_key, sizeof(data_key));

	return wrapped;
}

auto MasterKey::unlock(std::string_view t_password, const MasterKeyParams& t_params) -> bool
{
	u8 key_encryption_key[crypto::K_KEY_SIZE];
	if (!crypto::derive_key(t_password, t_params.salt, t_params.ops_limit, t_params.mem_limit, key_encryption_key)) {
		return false;
	}

	u8                        data_key[crypto::K_KEY_SIZE];
	const std::span<const u8> wrapped{t_params.wrapped_data_key, crypto::K_KEY_SIZE};
	const bool unwrapped = crypto::decrypt(key_encryption_key, t_params.wrap_nonce, wrapped, t_params.wrapped_data_key + crypto::K_KEY_SIZE, data_key);
	sodium_memzero(key_encryption_key, sizeof(key_encryption_key));

	if (unwrapped) {
		std::memcpy(m_data_key, data_key, sizeof(m_data_key));
		m_unlocked = true;
	}

	sodium_memzero(data_key, sizeof(data_key));

	return unwrapped;
}
