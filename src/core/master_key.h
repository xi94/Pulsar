#pragma once

#include <string_view>

#include "core/crypto.h"

struct MasterKeyParams {
	u8 salt[crypto::salt_size]{};
	u64 ops_limit = 0;
	usize mem_limit = 0;
	u8 wrap_nonce[crypto::nonce_size]{};
	u8 wrapped_data_key[crypto::key_size + crypto::tag_size]{};
};

class MasterKey {
  public:
	MasterKey() = default;
	~MasterKey();

	MasterKey(const MasterKey &) = delete;
	MasterKey &operator=(const MasterKey &) = delete;

	bool create(std::string_view t_password, MasterKeyParams &t_out_params);
	bool unlock(std::string_view t_password, const MasterKeyParams &t_params);
	void lock();
	void swap(MasterKey &t_other);

	bool is_unlocked() const
	{
		return m_unlocked;
	}

	const u8 *data_key() const
	{
		return m_data_key;
	}

  private:
	u8 m_data_key[crypto::key_size]{};
	bool m_unlocked = false;
};
