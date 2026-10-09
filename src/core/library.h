#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "core/types.h"

class Texture;

constexpr u32 K_MAX_GAMES             = 16;
constexpr u32 K_MAX_ACCOUNTS_PER_GAME = 32;
constexpr u32 K_MAX_VISIBLE_ACCOUNTS  = K_MAX_GAMES * K_MAX_ACCOUNTS_PER_GAME;

struct Account {
	char username[64]{};
	char note[32]{};
	char password[128]{};
	char region[8]{};
	u16  visible_game_mask = 0;
	bool favorite          = false;
	i64  last_used         = 0;
	u32  order             = 0;

	auto assign(std::string_view t_username, std::string_view t_note, std::string_view t_password) -> void;
	[[nodiscard]] auto visible_games(u32 t_owning_game) const -> u16;
};

struct Game {
	std::string_view title;
	std::string_view short_title;
	std::string_view launch_product;
	Color            accent{};
	const Texture*   banner = nullptr;
	const Texture*   icon   = nullptr;
	Account          accounts[K_MAX_ACCOUNTS_PER_GAME]{};
	u32              account_count = 0;
};

struct AccountRef {
	u32 game;
	u32 index;

	auto operator==(const AccountRef&) const -> bool = default;
};

struct VisibleAccounts {
	AccountRef refs[K_MAX_VISIBLE_ACCOUNTS];
	u32        count = 0;

	[[nodiscard]] auto view() const -> std::span<const AccountRef>
	{
		return {refs, count};
	}
};

// Orders accounts to be removed from the last place in each game to the first. Removing one then never moves those still to go, and
// inserting them back in the opposite order puts each where it was.
auto sort_for_removal(std::span<AccountRef> t_refs) -> void;
auto shift_after_insert(std::optional<AccountRef>* t_ref, AccountRef t_inserted) -> void;
auto shift_after_removal(std::optional<AccountRef>* t_ref, AccountRef t_removed) -> void;

struct Library {
	Game        games[K_MAX_GAMES];
	u32         game_count = 0;
	u32         next_order = 1;
	std::string unlisted_games;

	[[nodiscard]] auto add_account(u32 t_game, const Account& t_account) -> std::optional<AccountRef>;
	[[nodiscard]] auto insert_account(AccountRef t_where, const Account& t_account) -> std::optional<AccountRef>;
	auto remove_account(AccountRef t_ref) -> void;
	// Takes the rows out, keeping their order, and puts them back together starting at t_insert among the rows that are left.
	auto move_visible_accounts(u32 t_game, std::span<const u32> t_rows, u32 t_insert) -> void;
	auto number_unordered_accounts() -> void;
	auto wipe_accounts() -> void;

	[[nodiscard]] auto visible_accounts(u32 t_game) const -> VisibleAccounts;
	[[nodiscard]] auto visible_account(u32 t_game, u32 t_row) const -> std::optional<AccountRef>;

	[[nodiscard]] auto account(AccountRef t_ref) -> Account*
	{
		return &games[t_ref.game].accounts[t_ref.index];
	}

	[[nodiscard]] auto account(AccountRef t_ref) const -> const Account*
	{
		return &games[t_ref.game].accounts[t_ref.index];
	}
};
