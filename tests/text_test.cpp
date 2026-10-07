#include <string_view>

#include "core/str.h"
#include "test.h"

TEST_CASE("UTF-8 round-trips one to four byte characters")
{
	for (const u32 codepoint : {0x41u, 0xE9u, 0x20ACu, 0x1F600u}) {
		char       encoded[4]{};
		const u32  length  = encode_utf8(codepoint, encoded);
		const auto decoded = decode_utf8(std::string_view{encoded, length}, 0);

		CHECK(decoded.value == codepoint);
		CHECK(decoded.length == length);
	}
}

TEST_CASE("the cursor steps over whole characters")
{
	const std::string_view text = "aé€😀";

	CHECK(next_codepoint(text, 0) == 1);
	CHECK(next_codepoint(text, 1) == 3);
	CHECK(next_codepoint(text, 3) == 6);
	CHECK(next_codepoint(text, 6) == 10);
	CHECK(previous_codepoint(text, 10) == 6);
	CHECK(previous_codepoint(text, 6) == 3);
	CHECK(previous_codepoint(text, 3) == 1);
}

TEST_CASE("copying into a small buffer never cuts a character in half")
{
	char buffer[3]{};
	copy_to("aé", buffer);

	CHECK(std::string_view{buffer} == "a");
}

TEST_CASE("blocked scripts are blocked while other scripts aren't")
{
	CHECK(is_blocked_script(0x05D0));
	CHECK(is_blocked_script(0x0627));
	CHECK(is_blocked_script(0x0915));

	CHECK(!is_blocked_script('A'));
	CHECK(!is_blocked_script(0x00E9));
	CHECK(!is_blocked_script(0x0E01));
	CHECK(!is_blocked_script(0x3042));
	CHECK(!is_blocked_script(0xAC00));
}

TEST_CASE("search ignores case and reports byte offsets")
{
	CHECK(find_ignoring_case("League of Legends", "LEGENDS") == 10);
	CHECK(find_ignoring_case("League of Legends", "") == 0);
	CHECK(find_ignoring_case("League", "Valorant") == std::string_view::npos);
}

// Text beyond ASCII goes through the OS (CompareStringEx on Windows, NSString on macOS); both must agree.
TEST_CASE("search ignores case beyond ASCII the same way on every OS")
{
	CHECK(find_ignoring_case("Ünïcode ÉCOLE", "école") == 10);
	CHECK(find_ignoring_case("Привет МИР", "мир") == 13);
	CHECK(find_ignoring_case("Ünïcode", "école") == std::string_view::npos);
}

TEST_CASE("trimmed drops surrounding whitespace only")
{
	CHECK(trimmed("  name \t\n") == "name");
	CHECK(trimmed("two words") == "two words");
	CHECK(trimmed(" \t ").empty());
}

TEST_CASE("relative times use the largest whole unit")
{
	char buffer[32];

	CHECK(relative_time(1000, 1000, buffer) == "just now");
	CHECK(relative_time(1000, 1059, buffer) == "just now");
	CHECK(relative_time(1000, 500, buffer) == "just now");
	CHECK(relative_time(0, 60, buffer) == "1 minute ago");
	CHECK(relative_time(0, 2 * 3600 + 5, buffer) == "2 hours ago");
	CHECK(relative_time(0, 86400 + 3600, buffer) == "yesterday");
	CHECK(relative_time(0, 3 * 86400, buffer) == "3 days ago");
	CHECK(relative_time(0, 365 * 86400, buffer) == "1 year ago");
}
