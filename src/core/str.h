#pragma once

#include <algorithm>
#include <cstring>
#include <string>
#include <string_view>

struct Utf8Codepoint {
	u32 value;
	u32 length;
};

[[nodiscard]] inline auto is_utf8_continuation(char t_byte) -> bool
{
	return (static_cast<u8>(t_byte) & 0xC0) == 0x80;
}

inline auto copy_to(std::string_view t_text, char* t_destination, usize t_capacity) -> void
{
	usize length = std::min(t_text.size(), t_capacity - 1);

	while (length > 0 && length < t_text.size() && is_utf8_continuation(t_text[length])) {
		length -= 1;
	}

	std::memcpy(t_destination, t_text.data(), length);
	t_destination[length] = '\0';
}

template <usize Capacity>
auto copy_to(std::string_view t_text, char (&t_destination)[Capacity]) -> void
{
	copy_to(t_text, t_destination, Capacity);
}

[[nodiscard]] auto decode_utf8(std::string_view t_text, usize t_index) -> Utf8Codepoint;
[[nodiscard]] auto encode_utf8(u32 t_codepoint, char* t_out) -> u32;
[[nodiscard]] auto next_codepoint(std::string_view t_text, usize t_index) -> usize;
[[nodiscard]] auto previous_codepoint(std::string_view t_text, usize t_index) -> usize;

[[nodiscard]] auto find_ignoring_case(std::string_view t_text, std::string_view t_query) -> usize;
[[nodiscard]] auto trimmed(std::string_view t_text) -> std::string_view;

[[nodiscard]] auto to_utf8(std::wstring_view t_wide) -> std::string;
[[nodiscard]] auto to_wide(std::string_view t_utf8) -> std::wstring;
