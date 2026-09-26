#include "core/str.h"

#include <Windows.h>

namespace {
char lowered(char t_character)
{
	return t_character >= 'A' && t_character <= 'Z' ? static_cast<char>(t_character - 'A' + 'a') : t_character;
}
}

usize find_ignoring_case(std::string_view t_text, std::string_view t_query)
{
	if (t_query.size() > t_text.size()) return std::string_view::npos;

	for (usize start = 0; start + t_query.size() <= t_text.size(); start += 1) {
		const bool matches = std::equal(t_query.begin(), t_query.end(), t_text.begin() + start,
										[](char t_a, char t_b) { return lowered(t_a) == lowered(t_b); });
		if (matches) return start;
	}

	return std::string_view::npos;
}

std::string to_utf8(std::wstring_view t_wide)
{
	const auto wide_length = static_cast<int>(t_wide.size());
	const int length = WideCharToMultiByte(CP_UTF8, 0, t_wide.data(), wide_length, nullptr, 0, nullptr, nullptr);
	if (length <= 0) return {};

	std::string utf8(static_cast<usize>(length), '\0');
	WideCharToMultiByte(CP_UTF8, 0, t_wide.data(), wide_length, utf8.data(), length, nullptr, nullptr);

	return utf8;
}

std::wstring to_wide(std::string_view t_utf8)
{
	const auto utf8_length = static_cast<int>(t_utf8.size());
	const int length = MultiByteToWideChar(CP_UTF8, 0, t_utf8.data(), utf8_length, nullptr, 0);
	if (length <= 0) return {};

	std::wstring wide(static_cast<usize>(length), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, t_utf8.data(), utf8_length, wide.data(), length);

	return wide;
}
