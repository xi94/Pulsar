#include "core/master_key.h"

#include <cstring>

#include <sodium.h>

MasterKey::~MasterKey()
{
	sodium_memzero(m_data_key, sizeof(m_data_key));
}

bool MasterKey::create(std::string_view t_password, MasterKeyParams &t_out_params)
{
	MasterKeyParams params{
		.ops_limit = crypto::default_ops_limit(),
		.mem_limit = crypto::default_mem_limit(),
	};

	crypto::random_bytes(params.salt);
	crypto::random_bytes(params.wrap_nonce);

	u8 data_key[crypto::key_size];
	crypto::random_bytes(data_key);

	u8 key_encryption_key[crypto::key_size];
	if (!crypto::derive_key(t_password, params.salt, params.ops_limit, params.mem_limit, key_encryption_key)) {
		sodium_memzero(data_key, sizeof(data_key));
		return false;
	}

	const bool wrapped = crypto::encrypt(key_encryption_key, params.wrap_nonce, data_key, params.wrapped_data_key,
										 params.wrapped_data_key + crypto::key_size);
	sodium_memzero(key_encryption_key, sizeof(key_encryption_key));

	if (wrapped) {
		std::memcpy(m_data_key, data_key, sizeof(m_data_key));
		m_unlocked = true;
		t_out_params = params;
	}

	sodium_memzero(data_key, sizeof(data_key));

	return wrapped;
}

bool MasterKey::unlock(std::string_view t_password, const MasterKeyParams &t_params)
{
	u8 key_encryption_key[crypto::key_size];
	if (!crypto::derive_key(t_password, t_params.salt, t_params.ops_limit, t_params.mem_limit, key_encryption_key)) {
		return false;
	}

	u8 data_key[crypto::key_size];
	const std::span<const u8> wrapped{t_params.wrapped_data_key, crypto::key_size};
	const bool unwrapped = crypto::decrypt(key_encryption_key, t_params.wrap_nonce, wrapped,
										   t_params.wrapped_data_key + crypto::key_size, data_key);
	sodium_memzero(key_encryption_key, sizeof(key_encryption_key));

	if (unwrapped) {
		std::memcpy(m_data_key, data_key, sizeof(m_data_key));
		m_unlocked = true;
	}

	sodium_memzero(data_key, sizeof(data_key));

	return unwrapped;
}
