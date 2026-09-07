#pragma once

#include <cstring>
#include <string_view>

/// Copies `text` into a fixed char buffer, writing at most `capacity - 1` bytes and always
/// null-terminating. std::string_view::copy does neither, and every fixed-size field in this
/// project is a C string.
inline void CopyTo(std::string_view text, char *pDest, usize capacity)
{
	const usize copied = text.size() < capacity - 1 ? text.size() : capacity - 1;

	std::memcpy(pDest, text.data(), copied);
	pDest[copied] = '\0';
}
