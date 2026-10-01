#include "core/library.h"

#include <algorithm>
#include <cassert>
#include <span>
#include <vector>

#include <sodium.h>

#include "core/str.h"

void Account::assign(std::string_view t_username, std::string_view t_note, std::string_view t_password)
{
	const Account kept{
		.visible_game_mask = visible_game_mask,
		.favorite = favorite,
		.last_used = last_used,
		.order = order,
	};

	sodium_memzero(password, sizeof(password));
	*this = kept;

	copy_to(t_username, username);
	copy_to(t_note, note);
	copy_to(t_password, password);
}

u16 Account::visible_games(u32 t_owning_game) const
{
	return visible_game_mask != 0 ? visible_game_mask : static_cast<u16>(1u << t_owning_game);
}

std::optional<AccountRef> Library::add_account(u32 t_game, const Account &t_account)
{
	Game &game = games[t_game];
	if (game.account_count >= max_accounts_per_game) return std::nullopt;

	Account &added = game.accounts[game.account_count];
	added = t_account;
	added.order = next_order;

	next_order += 1;
	game.account_count += 1;

	return AccountRef{t_game, game.account_count - 1};
}

std::optional<AccountRef> Library::insert_account(AccountRef t_where, const Account &t_account)
{
	Game &game = games[t_where.game];
	if (game.account_count >= max_accounts_per_game) return std::nullopt;

	const u32 index = std::min(t_where.index, game.account_count);
	for (u32 i = game.account_count; i > index; i -= 1) {
		game.accounts[i] = game.accounts[i - 1];
	}

	game.accounts[index] = t_account;
	game.account_count += 1;

	return AccountRef{t_where.game, index};
}

void Library::remove_account(AccountRef t_ref)
{
	Game &game = games[t_ref.game];
	assert(t_ref.index < game.account_count);

	for (u32 i = t_ref.index; i + 1 < game.account_count; i += 1) {
		game.accounts[i] = game.accounts[i + 1];
	}

	game.account_count -= 1;
	sodium_memzero(&game.accounts[game.account_count], sizeof(Account));
}

void Library::move_visible_account(u32 t_game, u32 t_from_row, u32 t_to_row)
{
	const VisibleAccounts visible = visible_accounts(t_game);
	if (t_from_row >= visible.count || t_to_row >= visible.count || t_from_row == t_to_row) return;

	std::vector<AccountRef> sequence(visible.refs, visible.refs + visible.count);
	const AccountRef moved = sequence[t_from_row];
	sequence.erase(sequence.begin() + t_from_row);
	sequence.insert(sequence.begin() + t_to_row, moved);

	std::vector<u32> orders;
	orders.reserve(sequence.size());
	for (const AccountRef ref : sequence) {
		orders.push_back(account(ref).order);
	}

	std::ranges::sort(orders);

	for (usize i = 0; i < sequence.size(); i += 1) {
		account(sequence[i]).order = orders[i];
	}
}

void Library::number_unordered_accounts()
{
	u32 highest = 0;
	for (const Game &game : std::span{games, game_count}) {
		for (u32 i = 0; i < game.account_count; i += 1) {
			highest = std::max(highest, game.accounts[i].order);
		}
	}

	next_order = highest + 1;

	for (Game &game : std::span{games, game_count}) {
		for (u32 i = 0; i < game.account_count; i += 1) {
			if (game.accounts[i].order == 0) {
				game.accounts[i].order = next_order;
				next_order += 1;
			}
		}
	}
}

void Library::wipe_accounts()
{
	for (Game &game : std::span{games, game_count}) {
		sodium_memzero(game.accounts, sizeof(game.accounts));
		game.account_count = 0;
	}

	sodium_memzero(unlisted_games.data(), unlisted_games.size());
	unlisted_games.clear();
}

VisibleAccounts Library::visible_accounts(u32 t_game) const
{
	VisibleAccounts visible;
	if (t_game >= game_count) return visible;

	const u16 target_bit = static_cast<u16>(1u << t_game);

	for (u32 game = 0; game < game_count; game += 1) {
		for (u32 i = 0; i < games[game].account_count; i += 1) {
			if ((games[game].accounts[i].visible_games(game) & target_bit) == 0) continue;

			visible.refs[visible.count] = AccountRef{game, i};
			visible.count += 1;
		}
	}

	std::sort(visible.refs, visible.refs + visible.count, [this](AccountRef t_a, AccountRef t_b) {
		const Account &a = account(t_a);
		const Account &b = account(t_b);

		if (a.favorite != b.favorite) return a.favorite;
		if (a.order != b.order) return a.order < b.order;
		if (t_a.game != t_b.game) return t_a.game < t_b.game;

		return t_a.index < t_b.index;
	});

	return visible;
}

std::optional<AccountRef> Library::visible_account(u32 t_game, u32 t_row) const
{
	const VisibleAccounts visible = visible_accounts(t_game);
	if (t_row >= visible.count) return std::nullopt;

	return visible.refs[t_row];
}
