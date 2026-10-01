#pragma once

#include <span>
#include <string_view>

namespace crypto {

constexpr u32 key_size = 32;
constexpr u32 salt_size = 16;
constexpr u32 nonce_size = 24;
constexpr u32 tag_size = 16;

void random_bytes(std::span<u8> t_out);

bool derive_key(std::string_view t_password, const u8 *t_salt, u64 t_ops_limit, usize t_mem_limit, u8 *t_out_key);

u64 default_ops_limit();
usize default_mem_limit();

bool encrypt(const u8 *t_key, const u8 *t_nonce, std::span<const u8> t_plaintext, u8 *t_out_ciphertext, u8 *t_out_tag);

bool decrypt(const u8 *t_key, const u8 *t_nonce, std::span<const u8> t_ciphertext, const u8 *t_tag, u8 *t_out_plaintext);

}

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

	bool create(std::string_view t_password, MasterKeyParams *t_out_params);
	bool unlock(std::string_view t_password, const MasterKeyParams &t_params);
	void lock();
	void swap(MasterKey *t_other);

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
