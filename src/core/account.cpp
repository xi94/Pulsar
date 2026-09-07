#include "core/account.h"

#include <cassert>
#include <cstring>

namespace {
// Zeroes the whole buffer first so an edit that shortens a field leaves no stale tail.
void SetField(char *pDest, u64 capacity, std::string_view value)
{
	assert(value.size() < capacity && "field value too long for its fixed buffer");

	std::memset(pDest, 0, capacity);
	std::memcpy(pDest, value.data(), value.size());
}
} // namespace

void Account::Init(std::string_view username, std::string_view note, std::string_view password)
{
	SetField(m_szUsername, sizeof(m_szUsername), username);
	SetField(m_szNote, sizeof(m_szNote), note);
	SetField(m_szPassword, sizeof(m_szPassword), password);

	m_uVisibleBannerMask = 0;
	std::memset(m_aReserved, 0, sizeof(m_aReserved));
}

u16 Account::GetEffectiveVisibleMask(u32 ownBannerIndex) const
{
	if (m_uVisibleBannerMask != 0) return m_uVisibleBannerMask;

	return static_cast<u16>(1u << ownBannerIndex);
}
