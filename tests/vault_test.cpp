#include <algorithm>
#include <array>
#include <string_view>

#include "core/crypto.h"
#include "test.h"

namespace {
constexpr std::string_view K_SECRET = "hunter2 and a much longer secret";

struct Sealed {
	std::array<u8, crypto::K_KEY_SIZE>   key{};
	std::array<u8, crypto::K_NONCE_SIZE> nonce{};
	std::array<u8, K_SECRET.size()>      ciphertext{};
	std::array<u8, crypto::K_TAG_SIZE>   tag{};
};

[[nodiscard]] auto sealed_secret() -> Sealed
{
	Sealed sealed{};
	crypto::random_bytes(sealed.key);
	crypto::random_bytes(sealed.nonce);

	const auto* plaintext = reinterpret_cast<const u8*>(K_SECRET.data());
	if (!crypto::encrypt(sealed.key.data(), sealed.nonce.data(), {plaintext, K_SECRET.size()}, sealed.ciphertext.data(), sealed.tag.data())) return {};

	return sealed;
}

[[nodiscard]] auto opens(const Sealed& t_sealed) -> bool
{
	std::array<u8, K_SECRET.size()> plaintext{};
	if (!crypto::decrypt(t_sealed.key.data(), t_sealed.nonce.data(), t_sealed.ciphertext, t_sealed.tag.data(), plaintext.data())) return false;

	return std::ranges::equal(plaintext, K_SECRET, [](u8 t_byte, char t_character) { return t_byte == static_cast<u8>(t_character); });
}
}

TEST_CASE("encrypted data decrypts to the original")
{
	CHECK(opens(sealed_secret()));
}

TEST_CASE("a changed ciphertext, tag, nonce or key doesn't decrypt")
{
	Sealed ciphertext = sealed_secret();
	ciphertext.ciphertext[3] ^= 1;
	CHECK(!opens(ciphertext));

	Sealed tag = sealed_secret();
	tag.tag[0] ^= 1;
	CHECK(!opens(tag));

	Sealed nonce = sealed_secret();
	nonce.nonce[5] ^= 1;
	CHECK(!opens(nonce));

	Sealed key = sealed_secret();
	key.key[7] ^= 1;
	CHECK(!opens(key));
}

TEST_CASE("the master password unlocks the vault key it created")
{
	MasterKey       created;
	MasterKeyParams params{};
	REQUIRE(created.create("correct horse battery staple", &params));

	MasterKey unlocked;
	REQUIRE(unlocked.unlock("correct horse battery staple", params));
	CHECK(std::equal(created.data_key(), created.data_key() + crypto::K_KEY_SIZE, unlocked.data_key()));
}

TEST_CASE("a wrong master password doesn't unlock the vault")
{
	MasterKey       created;
	MasterKeyParams params{};
	REQUIRE(created.create("correct horse battery staple", &params));

	MasterKey unlocked;
	CHECK(!unlocked.unlock("correct horse battery stapler", params));
	CHECK(!unlocked.is_unlocked());
}
