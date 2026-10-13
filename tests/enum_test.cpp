#include <string_view>

#include "core/enum_array.h"
#include "core/settings.h"
#include "os/input.h"
#include "test.h"

namespace {
enum class Fruit : u8 {
	APPLE,
	PEAR,
	PLUM,
	COUNT,
};
}

TEST_CASE("each visits every value of an enum in order")
{
	Fruit seen[3]{};
	u32   count = 0;

	for (const Fruit fruit : each<Fruit>()) {
		seen[count] = fruit;
		count += 1;
	}

	CHECK(count == 3);
	CHECK(seen[0] == Fruit::APPLE);
	CHECK(seen[1] == Fruit::PEAR);
	CHECK(seen[2] == Fruit::PLUM);
}

TEST_CASE("stepping through an enum wraps round past either end")
{
	CHECK(stepped(Fruit::APPLE, 1) == Fruit::PEAR);
	CHECK(stepped(Fruit::PLUM, 1) == Fruit::APPLE);
	CHECK(stepped(Fruit::APPLE, -1) == Fruit::PLUM);
	CHECK(stepped(Fruit::PEAR, 4) == Fruit::PLUM);
	CHECK(stepped(Fruit::PEAR, -4) == Fruit::APPLE);
}

TEST_CASE("an enum array keeps one value for each enumerator")
{
	EnumArray<Fruit, u32> weights{};
	weights[Fruit::PEAR] = 7;

	CHECK(weights[Fruit::APPLE] == 0);
	CHECK(weights[Fruit::PEAR] == 7);
	CHECK(weights.end() - weights.begin() == 3);
}

TEST_CASE("letter keys convert to and from their letters")
{
	CHECK(os::letter_key('a') == os::Key::A);
	CHECK(os::letter_key('z') == os::Key::Z);
	CHECK(os::key_letter(os::Key::M) == 'm');
	CHECK(os::is_letter_key(os::Key::Q));
	CHECK(!os::is_letter_key(os::Key::COMMA));
}

TEST_CASE("every theme and pattern has its own saved id")
{
	for (const ThemeKind first : each<ThemeKind>()) {
		for (const ThemeKind second : each<ThemeKind>()) {
			CHECK((first == second) == (K_THEME_LABELS[first].id == K_THEME_LABELS[second].id));
		}
	}

	for (const BackgroundStyle first : each<BackgroundStyle>()) {
		for (const BackgroundStyle second : each<BackgroundStyle>()) {
			CHECK((first == second) == (K_BACKGROUND_LABELS[first].id == K_BACKGROUND_LABELS[second].id));
		}
	}
}
