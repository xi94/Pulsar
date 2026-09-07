#pragma once

#include <string_view>

struct SemVer {
	u32 Major = 0;
	u32 Minor = 0;
	u32 Patch = 0;
};

/// Accepts "1", "1.2" and "1.2.3"; missing components stay 0. Fails only if there is no
/// leading number at all.
bool SemVerParse(std::string_view text, SemVer &outVersion);

inline bool operator==(const SemVer &a, const SemVer &b)
{
	return a.Major == b.Major && a.Minor == b.Minor && a.Patch == b.Patch;
}

inline bool operator<(const SemVer &a, const SemVer &b)
{
	if (a.Major != b.Major) return a.Major < b.Major;

	if (a.Minor != b.Minor) return a.Minor < b.Minor;

	return a.Patch < b.Patch;
}

inline bool operator!=(const SemVer &a, const SemVer &b)
{
	return !(a == b);
}

inline bool operator>(const SemVer &a, const SemVer &b)
{
	return b < a;
}

inline bool operator<=(const SemVer &a, const SemVer &b)
{
	return !(b < a);
}

inline bool operator>=(const SemVer &a, const SemVer &b)
{
	return !(a < b);
}
