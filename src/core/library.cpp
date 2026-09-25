#include "core/library.h"

#include <cassert>

#include "core/str.h"

void Account::assign(std::string_view t_username, std::string_view t_note, std::string_view t_password)
{
	*this = Account{.visible_game_mask = visible_game_mask};

	copy_to(t_username, username);
	copy_to(t_note, note);
	copy_to(t_password, password);
}

u16 Account::visible_games(u32 t_owning_game) const
{
	return visible_game_mask != 0 ? visible_game_mask : static_cast<u16>(1u << t_owning_game);
}

void Library::add_game(std::string_view t_title, const Texture *t_banner, const Texture *t_icon, Color t_accent)
{
	assert(m_game_count < max_games);

	m_games[m_game_count] = Game{.title = t_title, .accent = t_accent, .banner = t_banner, .icon = t_icon};
	m_game_count += 1;
}

std::optional<AccountRef> Library::add_account(u32 t_game, const Account &t_account)
{
	Game &game = m_games[t_game];
	if (game.account_count >= max_accounts_per_game) return std::nullopt;

	game.accounts[game.account_count] = t_account;
	game.account_count += 1;

	return AccountRef{t_game, game.account_count - 1};
}

void Library::remove_account(AccountRef t_ref)
{
	Game &game = m_games[t_ref.game];
	assert(t_ref.index < game.account_count);

	for (u32 i = t_ref.index; i + 1 < game.account_count; i += 1) {
		game.accounts[i] = game.accounts[i + 1];
	}

	game.account_count -= 1;
}

VisibleAccounts Library::visible_accounts(u32 t_game) const
{
	VisibleAccounts visible;
	if (t_game >= m_game_count) return visible;

	for (u32 i = 0; i < m_games[t_game].account_count; i += 1) {
		visible.refs[visible.count] = AccountRef{t_game, i};
		visible.count += 1;
	}

	const u16 target_bit = static_cast<u16>(1u << t_game);

	for (u32 game = 0; game < m_game_count; game += 1) {
		if (game == t_game) continue;

		for (u32 i = 0; i < m_games[game].account_count; i += 1) {
			if ((m_games[game].accounts[i].visible_games(game) & target_bit) == 0) continue;

			visible.refs[visible.count] = AccountRef{game, i};
			visible.count += 1;
		}
	}

	return visible;
}

std::optional<AccountRef> Library::visible_account(u32 t_game, u32 t_row) const
{
	const VisibleAccounts visible = visible_accounts(t_game);
	if (t_row >= visible.count) return std::nullopt;

	return visible.refs[t_row];
}
