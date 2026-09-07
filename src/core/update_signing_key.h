#pragma once

#include <array>

/// The key every downloaded update is verified against before it may replace this running exe.
/// The matching secret key is the CI-side signing key and lives nowhere in this repo; losing it
/// means generating a new pair and shipping one release with this constant updated.
///
/// Regenerate with a throwaway program: sodium_init, crypto_sign_keypair, print both.
namespace rift::update {
constexpr std::array<u8, 32> kEd25519PublicKey{
	0xD0, 0x82, 0xF5, 0xE2, 0x12, 0xC9, 0x37, 0x3F, 0x4E, 0xEE, 0x56, 0x8D, 0xA3, 0x32, 0x8A, 0x64,
	0x3E, 0x73, 0x86, 0xBA, 0x0F, 0xE6, 0x13, 0xC3, 0x7F, 0x70, 0x23, 0x76, 0x63, 0x73, 0xF5, 0xE3,
};
} // namespace rift::update
