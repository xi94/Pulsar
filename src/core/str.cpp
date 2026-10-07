#include "core/str.h"

#include <algorithm>
#include <cstdio>

#include "os/system.h"

namespace {
constexpr u32 K_REPLACEMENT_CHARACTER = 0xFFFD;

struct CodepointRange {
	u32 first;
	u32 last;
};

constexpr CodepointRange K_BLOCKED_SCRIPTS[]{
	{0x0590, 0x06FF}, {0x0750, 0x077F}, {0x0870, 0x08FF}, {0x0900, 0x0D7F},   {0x1CD0, 0x1CFF},
	{0xA8E0, 0xA8FF}, {0xFB1D, 0xFDFF}, {0xFE70, 0xFEFF}, {0x11B00, 0x11B5F}, {0x1EE00, 0x1EEFF},
};

[[nodiscard]] auto lowered(char t_character) -> char
{
	return t_character >= 'A' && t_character <= 'Z' ? static_cast<char>(t_character - 'A' + 'a') : t_character;
}

[[nodiscard]] auto is_ascii(std::string_view t_text) -> bool
{
	return std::ranges::all_of(t_text, [](char t_byte) { return static_cast<u8>(t_byte) < 0x80; });
}

struct TimeUnit {
	i64         seconds;
	const char* name;
};

constexpr i64 K_SECONDS_PER_DAY = 86400;

constexpr TimeUnit K_TIME_UNITS[]{
	{365 * K_SECONDS_PER_DAY, "year"},
	{30 * K_SECONDS_PER_DAY, "month"},
	{7 * K_SECONDS_PER_DAY, "week"},
	{K_SECONDS_PER_DAY, "day"},
	{3600, "hour"},
	{60, "minute"},
};
}

[[nodiscard]] auto decode_utf8(std::string_view t_text, usize t_index) -> Utf8Codepoint
{
	constexpr Utf8Codepoint INVALID{K_REPLACEMENT_CHARACTER, 1};
	constexpr u32           SMALLEST_FOR_LENGTH[]{0, 0, 0x80, 0x800, 0x10000};

	const auto lead = static_cast<u8>(t_text[t_index]);
	if (lead < 0x80) [[likely]] {
		return Utf8Codepoint{lead, 1};
	}

	u32 length = 0;
	u32 value  = 0;

	if ((lead & 0xE0) == 0xC0) {
		length = 2;
		value  = lead & 0x1F;
	} else if ((lead & 0xF0) == 0xE0) {
		length = 3;
		value  = lead & 0x0F;
	} else if ((lead & 0xF8) == 0xF0) {
		length = 4;
		value  = lead & 0x07;
	} else {
		return INVALID;
	}

	if (t_text.size() - t_index < length) [[unlikely]] {
		return INVALID;
	}

	for (u32 i = 1; i < length; i += 1) {
		const char next = t_text[t_index + i];
		if (!is_utf8_continuation(next)) return INVALID;

		value = (value << 6) | (static_cast<u8>(next) & 0x3F);
	}

	if (value < SMALLEST_FOR_LENGTH[length] || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF)) return INVALID;

	return Utf8Codepoint{value, length};
}

[[nodiscard]] auto encode_utf8(u32 t_codepoint, char* t_out) -> u32
{
	if (t_codepoint > 0x10FFFF || (t_codepoint >= 0xD800 && t_codepoint <= 0xDFFF)) {
		t_codepoint = K_REPLACEMENT_CHARACTER;
	}

	if (t_codepoint < 0x80) {
		t_out[0] = static_cast<char>(t_codepoint);
		return 1;
	}

	if (t_codepoint < 0x800) {
		t_out[0] = static_cast<char>(0xC0 | (t_codepoint >> 6));
		t_out[1] = static_cast<char>(0x80 | (t_codepoint & 0x3F));
		return 2;
	}

	if (t_codepoint < 0x10000) {
		t_out[0] = static_cast<char>(0xE0 | (t_codepoint >> 12));
		t_out[1] = static_cast<char>(0x80 | ((t_codepoint >> 6) & 0x3F));
		t_out[2] = static_cast<char>(0x80 | (t_codepoint & 0x3F));
		return 3;
	}

	t_out[0] = static_cast<char>(0xF0 | (t_codepoint >> 18));
	t_out[1] = static_cast<char>(0x80 | ((t_codepoint >> 12) & 0x3F));
	t_out[2] = static_cast<char>(0x80 | ((t_codepoint >> 6) & 0x3F));
	t_out[3] = static_cast<char>(0x80 | (t_codepoint & 0x3F));
	return 4;
}

[[nodiscard]] auto next_codepoint(std::string_view t_text, usize t_index) -> usize
{
	if (t_index >= t_text.size()) return t_text.size();

	return std::min(t_text.size(), t_index + decode_utf8(t_text, t_index).length);
}

[[nodiscard]] auto previous_codepoint(std::string_view t_text, usize t_index) -> usize
{
	if (t_index == 0) return 0;

	usize index = t_index - 1;
	while (index > 0 && t_index - index < 4 && is_utf8_continuation(t_text[index])) {
		index -= 1;
	}

	return index;
}

[[nodiscard]] auto is_blocked_script(u32 t_codepoint) -> bool
{
	if (t_codepoint < K_BLOCKED_SCRIPTS[0].first) [[likely]] {
		return false;
	}

	return std::ranges::any_of(K_BLOCKED_SCRIPTS,
	                           [t_codepoint](CodepointRange t_range) { return t_codepoint >= t_range.first && t_codepoint <= t_range.last; });
}

[[nodiscard]] auto find_ignoring_case(std::string_view t_text, std::string_view t_query) -> usize
{
	if (!is_ascii(t_text) || !is_ascii(t_query)) return os::find_ignoring_case(t_text, t_query);
	if (t_query.size() > t_text.size()) return std::string_view::npos;

	for (usize start = 0; start + t_query.size() <= t_text.size(); start += 1) {
		const bool matches =
			std::equal(t_query.begin(), t_query.end(), t_text.begin() + start, [](char t_a, char t_b) { return lowered(t_a) == lowered(t_b); });
		if (matches) return start;
	}

	return std::string_view::npos;
}

[[nodiscard]] auto trimmed(std::string_view t_text) -> std::string_view
{
	constexpr std::string_view BLANK = " \t\r\n\v\f";

	const usize first = t_text.find_first_not_of(BLANK);
	if (first == std::string_view::npos) return {};

	return t_text.substr(first, t_text.find_last_not_of(BLANK) - first + 1);
}

[[nodiscard]] auto relative_time(i64 t_then, i64 t_now, char (&t_buffer)[32]) -> std::string_view
{
	const i64 elapsed = std::max<i64>(0, t_now - t_then);

	for (const TimeUnit& unit : K_TIME_UNITS) {
		const i64 count = elapsed / unit.seconds;
		if (count == 0) continue;
		if (unit.seconds == K_SECONDS_PER_DAY && count == 1) return "yesterday";

		const int written = std::snprintf(t_buffer, sizeof(t_buffer), "%lld %s%s ago", static_cast<long long>(count), unit.name, count == 1 ? "" : "s");

		return std::string_view{t_buffer, static_cast<usize>(std::max(written, 0))};
	}

	return "just now";
}
