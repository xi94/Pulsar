#include <string_view>

#include "os/input.h"
#include "test.h"
#include "ui/text_input.h"

// These tests press keys without modifiers; os::modifiers() reads the real keyboard, so don't hold Shift or Ctrl while they run.

namespace {
auto type(TextInput* t_input, std::string_view t_text) -> void
{
	for (const char character : t_text) {
		t_input->on_char(static_cast<u8>(character));
	}
}
}

TEST_CASE("typing inserts at the cursor")
{
	TextInput input;
	input.set_focused(true);
	input.set_value("ac");
	input.on_key_down(os::Key::LEFT);
	type(&input, "b");

	CHECK(input.value() == "abc");
}

TEST_CASE("backspace and forward delete remove one whole character")
{
	TextInput input;
	input.set_focused(true);
	input.set_value("aé€");

	input.on_key_down(os::Key::BACKSPACE);
	CHECK(input.value() == "aé");

	input.on_key_down(os::Key::HOME);
	input.on_key_down(os::Key::FORWARD_DELETE);
	CHECK(input.value() == "é");
}

TEST_CASE("Home and End jump to either end")
{
	TextInput input;
	input.set_focused(true);
	input.set_value("middle");

	input.on_key_down(os::Key::HOME);
	type(&input, "<");
	input.on_key_down(os::Key::END);
	type(&input, ">");

	CHECK(input.value() == "<middle>");
}

TEST_CASE("typing stops at the length limit")
{
	TextInput input;
	input.set_focused(true);
	input.set_max_length(3);
	type(&input, "abcdef");

	CHECK(input.value() == "abc");
}

TEST_CASE("blocked scripts and control characters can't be typed")
{
	TextInput input;
	input.set_focused(true);
	input.on_char(0x05D0);
	input.on_char(0x0627);
	input.on_char(0x0915);
	input.on_char('\t');
	input.on_char(0x7F);
	input.on_char(0x00E9);

	CHECK(input.value() == "é");
}
