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

bool decrypt(const u8 *t_key, const u8 *t_nonce, std::span<const u8> t_ciphertext, const u8 *t_tag,
			 u8 *t_out_plaintext);

}
