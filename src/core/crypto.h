#pragma once

#include <span>
#include <string_view>

namespace crypto {

constexpr u32 K_KEY_SIZE   = 32;
constexpr u32 K_SALT_SIZE  = 16;
constexpr u32 K_NONCE_SIZE = 24;
constexpr u32 K_TAG_SIZE   = 16;

auto random_bytes(std::span<u8> t_out) -> void;

[[nodiscard]] auto derive_key(std::string_view t_password, const u8* t_salt, u64 t_ops_limit, usize t_mem_limit, u8* t_out_key) -> bool;

[[nodiscard]] auto default_ops_limit() -> u64;
[[nodiscard]] auto default_mem_limit() -> usize;

[[nodiscard]] auto encrypt(const u8* t_key, const u8* t_nonce, std::span<const u8> t_plaintext, u8* t_out_ciphertext, u8* t_out_tag) -> bool;

[[nodiscard]] auto decrypt(const u8* t_key, const u8* t_nonce, std::span<const u8> t_ciphertext, const u8* t_tag, u8* t_out_plaintext) -> bool;

}

struct MasterKeyParams {
	u8    salt[crypto::K_SALT_SIZE]{};
	u64   ops_limit = 0;
	usize mem_limit = 0;
	u8    wrap_nonce[crypto::K_NONCE_SIZE]{};
	u8    wrapped_data_key[crypto::K_KEY_SIZE + crypto::K_TAG_SIZE]{};
};

class MasterKey {
  public:
	MasterKey() = default;
	~MasterKey();

	MasterKey(const MasterKey&)                    = delete;
	auto operator=(const MasterKey&) -> MasterKey& = delete;

	[[nodiscard]] auto create(std::string_view t_password, MasterKeyParams* t_out_params) -> bool;
	[[nodiscard]] auto unlock(std::string_view t_password, const MasterKeyParams& t_params) -> bool;
	auto lock() -> void;
	auto swap(MasterKey* t_other) -> void;

	[[nodiscard]] auto is_unlocked() const -> bool
	{
		return m_unlocked;
	}

	[[nodiscard]] auto data_key() const -> const u8*
	{
		return m_data_key;
	}

  private:
	u8   m_data_key[crypto::K_KEY_SIZE]{};
	bool m_unlocked = false;
};
