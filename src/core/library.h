#pragma once

#include <optional>
#include <span>
#include <string_view>

#include "core/types.h"

class Texture;

constexpr u32 max_games = 16;
constexpr u32 max_accounts_per_game = 32;
constexpr u32 max_visible_accounts = max_games * max_accounts_per_game;

struct Account {
	char username[64]{};
	char note[32]{};
	char password[128]{};
	char region[8]{};
	u16 visible_game_mask = 0;
	bool favorite = false;
	i64 last_used = 0;
	u32 order = 0;

	void assign(std::string_view t_username, std::string_view t_note, std::string_view t_password);
	u16 visible_games(u32 t_owning_game) const;
};

struct Game {
	std::string_view title;
	Color accent{};
	const Texture *banner = nullptr;
	const Texture *icon = nullptr;
	Account accounts[max_accounts_per_game]{};
	u32 account_count = 0;
};

struct AccountRef {
	u32 game;
	u32 index;

	bool operator==(const AccountRef &) const = default;
};

struct VisibleAccounts {
	AccountRef refs[max_visible_accounts];
	u32 count = 0;

	std::span<const AccountRef> view() const
	{
		return {refs, count};
	}
};

class Library {
  public:
	void add_game(std::string_view t_title, const Texture *t_banner, const Texture *t_icon, Color t_accent);

	std::optional<AccountRef> add_account(u32 t_game, const Account &t_account);
	std::optional<AccountRef> insert_account(AccountRef t_where, const Account &t_account);
	void remove_account(AccountRef t_ref);
	void move_visible_account(u32 t_game, u32 t_from_row, u32 t_to_row);
	void number_unordered_accounts();
	void wipe_accounts();

	VisibleAccounts visible_accounts(u32 t_game) const;
	std::optional<AccountRef> visible_account(u32 t_game, u32 t_row) const;

	Account &account(AccountRef t_ref)
	{
		return m_games[t_ref.game].accounts[t_ref.index];
	}

	const Account &account(AccountRef t_ref) const
	{
		return m_games[t_ref.game].accounts[t_ref.index];
	}

	Game &game(u32 t_index)
	{
		return m_games[t_index];
	}

	const Game &game(u32 t_index) const
	{
		return m_games[t_index];
	}

	u32 game_count() const
	{
		return m_game_count;
	}

	std::span<Game> games()
	{
		return {m_games, m_game_count};
	}

	std::span<const Game> games() const
	{
		return {m_games, m_game_count};
	}

  private:
	Game m_games[max_games];
	u32 m_game_count = 0;
	u32 m_next_order = 1;
};
