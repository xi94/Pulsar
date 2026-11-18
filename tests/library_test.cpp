#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "core/library.h"
#include "test.h"
#include "ui/row_selection.h"

namespace {
// One game whose accounts are named a, b, c... in the order they were added.
[[nodiscard]] auto library_with(u32 t_count) -> std::unique_ptr<Library>
{
	auto library        = std::make_unique<Library>();
	library->game_count = 1;

	for (const u32 i : std::views::iota(0u, t_count)) {
		const char name[2]{static_cast<char>('a' + i), '\0'};

		Account account{};
		account.assign(name, "", "password");
		std::ignore = library->add_account(0, account);
	}

	return library;
}

[[nodiscard]] auto order_of(const Library& t_library) -> std::string
{
	std::string order;

	for (const AccountRef ref : t_library.visible_accounts(0).view()) {
		order += t_library.account(ref)->username;
	}

	return order;
}

auto move_rows(Library* t_library, const std::vector<u32>& t_rows, u32 t_insert) -> void
{
	t_library->move_visible_accounts(0, t_rows, t_insert);
}
}

TEST_CASE("moving one account puts it on the row it was dropped on")
{
	const std::unique_ptr<Library> library = library_with(5);

	move_rows(library.get(), {0}, 3);
	CHECK(order_of(*library) == "bcdae");

	move_rows(library.get(), {3}, 0);
	CHECK(order_of(*library) == "abcde");
}

TEST_CASE("moving several accounts keeps their order and puts them together")
{
	const std::unique_ptr<Library> library = library_with(5);

	move_rows(library.get(), {1, 3}, 0);
	CHECK(order_of(*library) == "bdace");

	move_rows(library.get(), {0, 4}, 2);
	CHECK(order_of(*library) == "dabec");
}

TEST_CASE("a move past the end puts the accounts last, and a move of nothing changes nothing")
{
	const std::unique_ptr<Library> library = library_with(4);

	move_rows(library.get(), {0, 1}, 99);
	CHECK(order_of(*library) == "cdab");

	move_rows(library.get(), {}, 0);
	move_rows(library.get(), {7}, 0);
	CHECK(order_of(*library) == "cdab");
}

TEST_CASE("favourites stay above the rest when the rest are reordered")
{
	const std::unique_ptr<Library> library = library_with(5);

	library->account(AccountRef{0, 3})->favorite = true;
	CHECK(order_of(*library) == "dabce");

	move_rows(library.get(), {3, 4}, 1);
	CHECK(order_of(*library) == "dceab");
}

TEST_CASE("where a row lands after a move matches the move itself")
{
	constexpr u32 COUNT = 6;

	for (const u32 mask : std::views::iota(1u, (1u << COUNT))) {
		RowSet           moved;
		std::vector<u32> rows;

		for (const u32 row : std::views::iota(0u, COUNT)) {
			if ((mask & (1u << row)) != 0) {
				moved.set(row);
				rows.push_back(row);
			}
		}

		for (u32 insert = 0; insert + rows.size() <= COUNT; insert += 1) {
			const std::unique_ptr<Library> library = library_with(COUNT);
			const std::string              before  = order_of(*library);

			move_rows(library.get(), rows, insert);
			const std::string after = order_of(*library);

			for (const u32 row : std::views::iota(0u, COUNT)) {
				REQUIRE(after[row_after_move(row, moved, insert)] == before[row]);
			}
		}
	}
}

TEST_CASE("deleting several accounts and putting them back in reverse restores the list")
{
	const std::unique_ptr<Library> library = library_with(6);

	AccountRef doomed[]{AccountRef{0, 1}, AccountRef{0, 4}, AccountRef{0, 2}};
	sort_for_removal(doomed);

	std::vector<std::pair<AccountRef, Account>> removed;
	for (const AccountRef ref : doomed) {
		removed.emplace_back(ref, *library->account(ref));
		library->remove_account(ref);
	}

	CHECK(order_of(*library) == "adf");

	for (auto it = removed.rbegin(); it != removed.rend(); ++it) {
		CHECK(library->insert_account(it->first, it->second).has_value());
	}

	for (const u32 i : std::views::iota(0u, 6u)) {
		CHECK(library->games[0].accounts[i].username[0] == static_cast<char>('a' + i));
	}
}

TEST_CASE("saving a new account adds it with every detail")
{
	const auto library = library_with(1);

	const AccountDetails            details{.username = "haku", .password = "secret", .note = "Main", .region = "EUW", .visible_games = 1};
	const std::optional<AccountRef> saved = library->save_account(std::nullopt, 0, details);

	REQUIRE(saved.has_value());
	const Account& account = *library->account(*saved);
	CHECK(std::string_view{account.username} == "haku");
	CHECK(std::string_view{account.password} == "secret");
	CHECK(std::string_view{account.note} == "Main");
	CHECK(std::string_view{account.region} == "EUW");
	CHECK(order_of(*library) == "ahaku");
}

TEST_CASE("saving over an account changes it in place")
{
	const auto       library = library_with(2);
	const AccountRef first   = library->visible_accounts(0).refs[0];

	const AccountDetails            details{.username = "renamed", .password = "new", .note = "", .region = "", .visible_games = 1};
	const std::optional<AccountRef> saved = library->save_account(first, 0, details);

	REQUIRE(saved.has_value());
	CHECK(*saved == first);
	CHECK(order_of(*library) == "renamedb");
	CHECK(std::string_view{library->account(first)->password} == "new");
}

TEST_CASE("an account never used has no last used label, and a note drops the prefix")
{
	char    buffer[48];
	Account account{};

	CHECK(last_used_label(account, 1000, buffer).empty());

	account.last_used = 1000;
	CHECK(last_used_label(account, 1000 + 2 * 60 * 60, buffer) == "Last used 2 hours ago");

	account.assign("haku", "Main", "secret");
	CHECK(last_used_label(account, 1000 + 2 * 60 * 60, buffer) == "2 hours ago");
}
