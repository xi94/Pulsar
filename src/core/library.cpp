#include "core/library.h"

#include <algorithm>
#include <cassert>
#include <span>
#include <vector>

#include <sodium.h>

#include "core/str.h"

auto Account::assign(std::string_view t_username, std::string_view t_note, std::string_view t_password) -> void
{
	const Account kept{
		.visible_game_mask = visible_game_mask,
		.favorite          = favorite,
		.last_used         = last_used,
		.order             = order,
	};

	sodium_memzero(password, sizeof(password));
	*this = kept;

	copy_to(t_username, username);
	copy_to(t_note, note);
	copy_to(t_password, password);
}

auto Account::visible_games(u32 t_owning_game) const -> u16
{
	return visible_game_mask != 0 ? visible_game_mask : static_cast<u16>(1u << t_owning_game);
}

auto Library::add_account(u32 t_game, const Account& t_account) -> std::optional<AccountRef>
{
	Game* game = &games[t_game];
	if (game->account_count >= K_MAX_ACCOUNTS_PER_GAME) return std::nullopt;

	Account* added = &game->accounts[game->account_count];
	*added         = t_account;
	added->order   = next_order;

	next_order += 1;
	game->account_count += 1;

	return AccountRef{t_game, game->account_count - 1};
}

auto Library::insert_account(AccountRef t_where, const Account& t_account) -> std::optional<AccountRef>
{
	Game* game = &games[t_where.game];
	if (game->account_count >= K_MAX_ACCOUNTS_PER_GAME) return std::nullopt;

	const u32 index = std::min(t_where.index, game->account_count);
	for (u32 i = game->account_count; i > index; i -= 1) {
		game->accounts[i] = game->accounts[i - 1];
	}

	game->accounts[index] = t_account;
	game->account_count += 1;

	return AccountRef{t_where.game, index};
}

auto Library::remove_account(AccountRef t_ref) -> void
{
	Game* game = &games[t_ref.game];
	assert(t_ref.index < game->account_count);

	for (u32 i = t_ref.index; i + 1 < game->account_count; i += 1) {
		game->accounts[i] = game->accounts[i + 1];
	}

	game->account_count -= 1;
	sodium_memzero(&game->accounts[game->account_count], sizeof(Account));
}

auto Library::move_visible_account(u32 t_game, u32 t_from_row, u32 t_to_row) -> void
{
	const VisibleAccounts visible = visible_accounts(t_game);
	if (t_from_row >= visible.count || t_to_row >= visible.count || t_from_row == t_to_row) return;

	std::vector<AccountRef> sequence(visible.refs, visible.refs + visible.count);
	const AccountRef        moved = sequence[t_from_row];
	sequence.erase(sequence.begin() + t_from_row);
	sequence.insert(sequence.begin() + t_to_row, moved);

	std::vector<u32> orders;
	orders.reserve(sequence.size());
	for (const AccountRef ref : sequence) {
		orders.push_back(account(ref)->order);
	}

	std::ranges::sort(orders);

	for (usize i = 0; i < sequence.size(); i += 1) {
		account(sequence[i])->order = orders[i];
	}
}

auto Library::number_unordered_accounts() -> void
{
	u32 highest = 0;
	for (const Game& game : std::span{games, game_count}) {
		for (u32 i = 0; i < game.account_count; i += 1) {
			highest = std::max(highest, game.accounts[i].order);
		}
	}

	next_order = highest + 1;

	for (Game& game : std::span{games, game_count}) {
		for (u32 i = 0; i < game.account_count; i += 1) {
			if (game.accounts[i].order == 0) {
				game.accounts[i].order = next_order;
				next_order += 1;
			}
		}
	}
}

auto Library::wipe_accounts() -> void
{
	for (Game& game : std::span{games, game_count}) {
		sodium_memzero(game.accounts, sizeof(game.accounts));
		game.account_count = 0;
	}

	sodium_memzero(unlisted_games.data(), unlisted_games.size());
	unlisted_games.clear();
}

auto Library::visible_accounts(u32 t_game) const -> VisibleAccounts
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
		const Account* a = account(t_a);
		const Account* b = account(t_b);

		if (a->favorite != b->favorite) return a->favorite;
		if (a->order != b->order) return a->order < b->order;
		if (t_a.game != t_b.game) return t_a.game < t_b.game;

		return t_a.index < t_b.index;
	});

	return visible;
}

auto Library::visible_account(u32 t_game, u32 t_row) const -> std::optional<AccountRef>
{
	const VisibleAccounts visible = visible_accounts(t_game);
	if (t_row >= visible.count) return std::nullopt;

	return visible.refs[t_row];
}

auto shift_after_insert(std::optional<AccountRef>* t_ref, AccountRef t_inserted) -> void
{
	if (!t_ref->has_value()) return;

	AccountRef* ref = &t_ref->value();
	if (ref->game == t_inserted.game && ref->index >= t_inserted.index) {
		ref->index += 1;
	}
}

auto shift_after_removal(std::optional<AccountRef>* t_ref, AccountRef t_removed) -> void
{
	if (!t_ref->has_value() || t_ref->value().game != t_removed.game) return;

	AccountRef* ref = &t_ref->value();
	if (ref->index == t_removed.index) {
		t_ref->reset();
	} else if (ref->index > t_removed.index) {
		ref->index -= 1;
	}
}
