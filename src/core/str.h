#pragma once

#include <algorithm>
#include <cstring>
#include <string>
#include <string_view>

inline void copy_to(std::string_view t_text, char *t_destination, usize t_capacity)
{
	const usize length = std::min(t_text.size(), t_capacity - 1);

	std::memcpy(t_destination, t_text.data(), length);
	t_destination[length] = '\0';
}

template <usize Capacity>
void copy_to(std::string_view t_text, char (&t_destination)[Capacity])
{
	copy_to(t_text, t_destination, Capacity);
}

usize find_ignoring_case(std::string_view t_text, std::string_view t_query);

std::string to_utf8(std::wstring_view t_wide);
std::wstring to_wide(std::string_view t_utf8);
