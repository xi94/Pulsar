#pragma once

#include <string_view>

/// One stored login. Fields are null-terminated inside their fixed buffers - unlike
/// std::string_view elsewhere in this project - and public, because CStorage memcpys the whole
/// record as one flat wire format.
class Account {
  public:
	/// Asserts rather than truncates: a field that does not fit is a caller bug.
	void Init(std::string_view username, std::string_view note, std::string_view password);

	std::string_view GetUsername() const
	{
		return std::string_view{m_szUsername};
	}

	std::string_view GetNote() const
	{
		return std::string_view{m_szNote};
	}

	/// The explicit mask if one was ever set, otherwise a mask with only this account's
	/// banner bit. Every reader goes through here so the zero-means-default convention lives
	/// in one place.
	u16 GetEffectiveVisibleMask(u32 ownBannerIndex) const;

	char m_szUsername[64];
	char m_szNote[32];
	char m_szPassword[128];

	/// Which banners this account shows under, one bit per banner index. Zero means "never
	/// chosen" - see GetEffectiveVisibleMask.
	u16 m_uVisibleBannerMask;

	u8 m_aReserved[30]; // pads 226 -> 256, exactly four cache lines
};

static_assert(sizeof(Account) == 256);
