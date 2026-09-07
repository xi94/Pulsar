#include "core/master_key.h"

#include <cstring>

#include <sodium.h>

CMasterKey::~CMasterKey()
{
	Clear();
}

void CMasterKey::Init()
{
	m_bEnabled = false;
	m_opsLimit = 0;
	m_memLimit = 0;

	sodium_memzero(m_aDek, sizeof(m_aDek));
	std::memset(m_aSalt, 0, sizeof(m_aSalt));
	std::memset(m_aWrapNonce, 0, sizeof(m_aWrapNonce));
	std::memset(m_aWrappedDek, 0, sizeof(m_aWrappedDek));
}

bool CMasterKey::Set(std::string_view password)
{
	u8 salt[CCrypto::kSaltSize];
	CCrypto::RandomBytes(salt, sizeof(salt));

	u8 dek[CCrypto::kKeySize];
	CCrypto::RandomBytes(dek, sizeof(dek));

	const u64 opsLimit = CCrypto::DefaultOpsLimit();
	const usize memLimit = CCrypto::DefaultMemLimit();

	u8 kek[CCrypto::kKeySize];
	if (!CCrypto::Argon2idDeriveKey(password, salt, opsLimit, memLimit, kek)) return false;

	u8 wrapNonce[CCrypto::kNonceSize];
	CCrypto::RandomBytes(wrapNonce, sizeof(wrapNonce));

	u8 wrappedDek[CCrypto::kKeySize + CCrypto::kTagSize];
	const bool wrapped = CCrypto::Encrypt(kek, wrapNonce, dek, sizeof(dek), wrappedDek, wrappedDek + CCrypto::kKeySize);
	sodium_memzero(kek, sizeof(kek));

	if (!wrapped) return false;

	std::memcpy(m_aDek, dek, sizeof(m_aDek));
	sodium_memzero(dek, sizeof(dek));

	std::memcpy(m_aSalt, salt, sizeof(m_aSalt));
	std::memcpy(m_aWrapNonce, wrapNonce, sizeof(m_aWrapNonce));
	std::memcpy(m_aWrappedDek, wrappedDek, sizeof(m_aWrappedDek));
	m_opsLimit = opsLimit;
	m_memLimit = memLimit;
	m_bEnabled = true;

	return true;
}

bool CMasterKey::Unlock(std::string_view password, const u8 salt[CCrypto::kSaltSize], u64 opsLimit, usize memLimit,
						const u8 wrapNonce[CCrypto::kNonceSize],
						const u8 wrappedDek[CCrypto::kKeySize + CCrypto::kTagSize])
{
	u8 kek[CCrypto::kKeySize];
	if (!CCrypto::Argon2idDeriveKey(password, salt, opsLimit, memLimit, kek)) return false;

	u8 dek[CCrypto::kKeySize];
	const bool unwrapped =
		CCrypto::Decrypt(kek, wrapNonce, wrappedDek, CCrypto::kKeySize, wrappedDek + CCrypto::kKeySize, dek);
	sodium_memzero(kek, sizeof(kek));

	if (!unwrapped) return false;

	std::memcpy(m_aDek, dek, sizeof(m_aDek));
	sodium_memzero(dek, sizeof(dek));

	std::memcpy(m_aSalt, salt, sizeof(m_aSalt));
	std::memcpy(m_aWrapNonce, wrapNonce, sizeof(m_aWrapNonce));
	std::memcpy(m_aWrappedDek, wrappedDek, sizeof(m_aWrappedDek));
	m_opsLimit = opsLimit;
	m_memLimit = memLimit;
	m_bEnabled = true;

	return true;
}

void CMasterKey::Clear()
{
	m_bEnabled = false;
	sodium_memzero(m_aDek, sizeof(m_aDek));
}
