#pragma once

#include "core/crypto.h"
#include <string_view>

/// The unlocked master-password session.
///
/// The typed password derives a key-encryption key, which does nothing but unwrap a
/// separate random data-encryption key; m_aDek is what actually encrypts account passwords.
/// Changing the master password therefore re-wraps a few dozen bytes instead of
/// re-encrypting every account, and leaves room for a second unlock path later.
///
/// There is no separate password verifier: the AEAD tag check when unwrapping already
/// rejects a wrong password, and a second check could only ever disagree with the first.
class CMasterKey {
  public:
	~CMasterKey();

	void Init();

	/// Generates a fresh salt and DEK and wraps the DEK under the new password. On success
	/// the caller is responsible for persisting the non-secret fields below into Settings.
	bool Set(std::string_view password);

	/// Re-derives the KEK from previously persisted parameters and unwraps the DEK. A false
	/// return cannot distinguish a wrong password from corrupted data, by design.
	bool Unlock(std::string_view password, const u8 salt[CCrypto::kSaltSize], u64 opsLimit, usize memLimit,
				const u8 wrapNonce[CCrypto::kNonceSize], const u8 wrappedDek[CCrypto::kKeySize + CCrypto::kTagSize]);

	/// sodium_memzero, not memset: the DEK is a real secret and a plain memset here is a
	/// dead store the compiler is free to drop.
	void Clear();

	bool m_bEnabled = false;
	u8 m_aDek[CCrypto::kKeySize]{};

	/// Not secret - nobody recovers the DEK from these without the password. Persisted by
	/// the caller after Set and handed back to Unlock in a later session.
	u8 m_aSalt[CCrypto::kSaltSize]{};
	u64 m_opsLimit = 0;
	usize m_memLimit = 0;
	u8 m_aWrapNonce[CCrypto::kNonceSize]{};
	u8 m_aWrappedDek[CCrypto::kKeySize + CCrypto::kTagSize]{};
};
