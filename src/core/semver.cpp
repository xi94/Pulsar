#include "core/semver.h"

namespace {
// Consumes digits at *pIndex plus a following '.', if any. False means there were no
// digits there at all.
bool ParseComponent(std::string_view text, u64 *pIndex, u32 &outValue)
{
	outValue = 0;

	const u64 start = *pIndex;
	while (*pIndex < text.size() && text.data()[*pIndex] >= '0' && text.data()[*pIndex] <= '9') {
		outValue = outValue * 10 + static_cast<u32>(text.data()[*pIndex] - '0');
		*pIndex += 1;
	}

	if (*pIndex == start) return false;

	if (*pIndex < text.size() && text.data()[*pIndex] == '.') {
		*pIndex += 1;
	}

	return true;
}
} // namespace

bool SemVerParse(std::string_view text, SemVer &outVersion)
{
	outVersion = SemVer{};

	u64 index = 0;
	if (!ParseComponent(text, &index, outVersion.Major)) return false;

	ParseComponent(text, &index, outVersion.Minor);
	ParseComponent(text, &index, outVersion.Patch);

	return true;
}
