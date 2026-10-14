#include "ui/account_list.h"

#include <algorithm>
#include <bit>
#include <iterator>
#include <ranges>

#include <sodium.h>

#include "core/animation.h"
#include "core/str.h"
#include "core/regions.h"
#include "os/input.h"
#include "ui/login_session.h"
#include "ui/toasts.h"

namespace {
constexpr float K_SHIFT_EASE_RATE = 18.0f;
constexpr float K_LIFT_EASE_RATE  = 16.0f;
constexpr float K_HOVER_EASE_RATE = 20.0f;
constexpr float K_UNDO_SECONDS    = 6.0f;

constexpr EnumArray<EditField, u32> K_FIELD_LENGTHS{{sizeof(Account::note) - 1, sizeof(Account::username) - 1, sizeof(Account::password) - 1}};
constexpr EditField                 K_FIELD_ORDER[]{EditField::USERNAME, EditField::PASSWORD, EditField::NOTE};

[[nodiscard]] auto game_bit(u32 t_game) -> u16
{
	return static_cast<u16>(1u << t_game);
}

[[nodiscard]] auto row_top(u32 t_row, float t_row_height) -> float
{
	return static_cast<float>(t_row) * t_row_height;
}
}

AccountList::AccountList(Library* t_library, LoginSession* t_session, Toasts* t_toasts, CommandType t_undo_command)
	: m_library(t_library)
	, m_session(t_session)
	, m_toasts(t_toasts)
	, m_undo_command(t_undo_command)
{
}

auto AccountList::has_game() const -> bool
{
	return m_game && *m_game < m_library->game_count;
}

auto AccountList::rows() const -> VisibleAccounts
{
	if (!has_game()) return VisibleAccounts{};

	return m_library->visible_accounts(*m_game, m_query);
}

auto AccountList::set_query(std::string_view t_query) -> bool
{
	if (t_query == std::string_view{m_query}) return false;

	copy_to(t_query, m_query);
	return true;
}

auto AccountList::shows_every_account() const -> bool
{
	return has_game() && m_query[0] == '\0';
}

auto AccountList::lifted_top(float t_pointer_in_list, float t_row_height) const -> float
{
	return m_drag.clamp_top(t_pointer_in_list - m_drag.grab_offset, t_row_height);
}

auto AccountList::lift(const VisibleAccounts& t_rows, u32 t_row, float t_pointer_in_list, float t_row_height) -> void
{
	const RowRange section  = drag_range(t_rows, t_row);
	const float    held_top = row_top(t_row, t_row_height) + m_offsets[t_row];

	m_drag.lift(t_row, m_selection.m_rows, section.first, section.last);
	m_drag.grab_offset = t_pointer_in_list - held_top;

	if (m_drag.rows.count() == 1) {
		m_selection.clear();
	}

	for (const u32 row : std::views::iota(section.first, section.last + 1)) {
		if (!m_drag.rows[row]) continue;

		const float own_top   = row_top(row, t_row_height) + m_offsets[row];
		const float block_top = held_top + m_drag.rows_from_held(row) * t_row_height;
		m_offsets[row]        = own_top - block_top;
	}

	m_raised = m_drag.rows;
}

auto AccountList::drop(float t_lifted_top, float t_row_height) -> void
{
	if (!m_drag.lifted) {
		m_drag = RowDrag{};
		return;
	}

	const VisibleAccounts before = rows();
	const RowDrag         drag   = m_drag;

	cancel_drag(t_lifted_top, t_row_height);

	if (drag.from_row >= before.count || !has_game()) {
		reset_motion();
		return;
	}

	u32 moved_rows[K_MAX_VISIBLE_ACCOUNTS];
	u32 moved_count = 0;

	for (const u32 row : std::views::iota(0u, before.count)) {
		if (drag.rows[row]) {
			moved_rows[moved_count] = row;
			moved_count += 1;
		}
	}

	m_library->move_visible_accounts(*m_game, std::span{moved_rows, moved_count}, drag.insert);
	slide_from(before, t_row_height);

	m_raised.reset();
	m_selection.clear();

	for (const u32 row : std::views::iota(drag.insert, drag.insert + moved_count)) {
		m_raised.set(row);
	}

	if (moved_count > 1) {
		m_selection.select_block(drag.insert, moved_count);
	}

	m_selection.set_source_rows(rows());
}

auto AccountList::cancel_drag(float t_lifted_top, float t_row_height) -> void
{
	if (m_drag.lifted) {
		for (const u32 row : std::views::iota(0u, K_MAX_VISIBLE_ACCOUNTS)) {
			if (!m_drag.rows[row]) continue;

			const float block_top = t_lifted_top + m_drag.rows_from_held(row) * t_row_height;
			m_offsets[row] += block_top - row_top(row, t_row_height);
		}
	}

	m_drag = RowDrag{};
}

auto AccountList::update_motion(u32 t_row_count, float t_lifted_top, float t_row_height, float t_delta_seconds) -> void
{
	if (m_drag.lifted && m_drag.from_row >= t_row_count) {
		reset_motion();
	}

	if (m_drag.lifted) {
		m_drag.aim(t_lifted_top, t_row_height);
		animation::request_frame();
	}

	bool raised_settled = true;

	for (const u32 row : std::views::iota(0u, t_row_count)) {
		const bool  makes_room = m_drag.lifted && !m_drag.rows[row];
		const float target     = makes_room ? m_drag.rows_moved_aside(row) * t_row_height : 0.0f;

		m_offsets[row] = animation::ease_toward(m_offsets[row], target, K_SHIFT_EASE_RATE, t_delta_seconds, animation::K_SETTLED_PIXELS);
		raised_settled = raised_settled && (!m_raised[row] || m_offsets[row] == 0.0f);
	}

	const float lift_target = m_drag.lifted ? 1.0f : 0.0f;
	m_lift                  = animation::ease_toward(m_lift, lift_target, K_LIFT_EASE_RATE, t_delta_seconds);

	if (!m_drag.lifted && m_lift == 0.0f && raised_settled) {
		m_raised.reset();
	}
}

auto AccountList::ease_hover(std::optional<u32> t_hovered_row, u32 t_row_count, float t_delta_seconds) -> void
{
	for (const u32 row : std::views::iota(0u, t_row_count)) {
		const float target = row == t_hovered_row ? 1.0f : 0.0f;
		m_hover[row]       = animation::ease_toward(m_hover[row], target, K_HOVER_EASE_RATE, t_delta_seconds);
	}
}

auto AccountList::slide_from(const VisibleAccounts& t_before, float t_row_height) -> void
{
	const VisibleAccounts after = rows();
	const auto            then  = t_before.view();
	float                 offsets[K_MAX_VISIBLE_ACCOUNTS]{};

	for (const u32 row : std::views::iota(0u, after.count)) {
		const auto was = std::ranges::find(then, after.refs[row]);
		if (was == then.end()) continue;

		const auto old_row = static_cast<u32>(was - then.begin());
		offsets[row]       = row_top(old_row, t_row_height) - row_top(row, t_row_height) + m_offsets[old_row];
	}

	std::ranges::copy(offsets, m_offsets);
}

auto AccountList::reset_motion() -> void
{
	m_drag = RowDrag{};
	std::ranges::fill(m_offsets, 0.0f);
	std::ranges::fill(m_hover, 0.0f);
	m_raised.reset();
	m_lift = 0.0f;
}

auto AccountList::select_with_modifiers(u32 t_row) -> bool
{
	const os::Modifiers held = os::modifiers();
	if (!held.shortcut && !held.shift) return false;

	if (held.shift) {
		m_selection.extend_to(t_row);
	} else {
		m_selection.toggle(t_row);
	}

	return true;
}

auto AccountList::select_accounts(std::span<const AccountRef> t_accounts) -> void
{
	const VisibleAccounts shown = rows();
	m_selection.clear();

	for (const u32 row : std::views::iota(0u, shown.count)) {
		if (std::ranges::contains(t_accounts, shown.refs[row])) {
			m_selection.toggle(row);
		}
	}

	m_selection.set_source_rows(shown);
}

auto AccountList::toggle_favorite(AccountRef t_account, float t_row_height) -> void
{
	Account* account = m_library->find_account(t_account);
	if (account == nullptr) return;

	const VisibleAccounts before = rows();
	account->favorite            = !account->favorite;
	slide_from(before, t_row_height);

	if (const std::optional<u32> row = rows().row_of(t_account)) {
		m_raised.reset();
		m_raised.set(*row);
	}
}

auto AccountList::delete_accounts(std::span<const AccountRef> t_accounts) -> std::span<const AccountRef>
{
	if (t_accounts.empty()) return {};

	forget_deleted();

	const usize                 count   = std::min(t_accounts.size(), std::size(m_changed));
	const std::span<AccountRef> removed = std::span{m_changed}.first(count);

	std::ranges::copy(t_accounts.first(count), removed.begin());
	sort_for_removal(removed);

	// Reserved up front so the list never moves, which would leave copies of the passwords behind.
	m_deleted.reserve(count);

	for (const AccountRef account : removed) {
		DeletedAccount& deleted = m_deleted.emplace_back();
		deleted.account         = *m_library->account(account);
		deleted.position        = account;

		m_library->remove_account(account);
		m_session->follow_removal(account);
	}

	char                   several[64];
	const std::string_view message = count == 1 ? std::string_view{"Account deleted. Click to undo."} : format_into(several, "{} accounts deleted. Click to undo.", count);
	m_toasts->notify(Notification{.message = message, .on_click = Command{.type = m_undo_command}, .seconds = K_UNDO_SECONDS, .always_show = true});

	return removed;
}

auto AccountList::take_picked() -> std::span<const AccountRef>
{
	const VisibleAccounts shown        = rows();
	usize                 picked_count = 0;

	for (const u32 row : std::views::iota(0u, shown.count)) {
		if (m_selection.contains(row)) {
			m_picked[picked_count] = shown.refs[row];
			picked_count += 1;
		}
	}

	m_selection.clear();

	return std::span{m_picked}.first(picked_count);
}

auto AccountList::undo_delete() -> std::span<const AccountRef>
{
	if (m_deleted.empty()) return {};

	const usize total    = m_deleted.size();
	usize       restored = 0;

	for (const DeletedAccount& deleted : m_deleted | std::views::reverse) {
		const std::optional<AccountRef> back = m_library->insert_account(deleted.position, deleted.account);
		if (!back) continue;

		m_session->follow_insert(*back);
		m_changed[restored] = *back;
		restored += 1;
	}

	forget_deleted();

	if (restored < total) {
		const std::string_view message = total == 1 ? "There's no room left to restore that account." : "There's no room left to restore them all.";
		m_toasts->notify(Notification{.message = message});
	}

	return std::span{m_changed}.first(restored);
}

auto AccountList::forget_deleted_without_undo() -> void
{
	if (!m_deleted.empty() && !m_toasts->is_offering(m_undo_command)) {
		forget_deleted();
	}
}

auto AccountList::forget_deleted() -> void
{
	for (DeletedAccount& deleted : m_deleted) {
		sodium_memzero(&deleted.account, sizeof(Account));
	}

	m_deleted.clear();
}

auto AccountList::forget_secrets() -> void
{
	forget_deleted();
	reset_motion();
	m_selection.clear();
	sodium_memzero(m_query, sizeof(m_query));
}

auto AccountList::drag_range(const VisibleAccounts& t_rows, u32 t_row) const -> RowRange
{
	const auto shown          = t_rows.view();
	const auto first_unstared = std::ranges::find_if(shown, [this](AccountRef t_account) { return !m_library->account(t_account)->favorite; });
	const auto favorites      = static_cast<u32>(first_unstared - shown.begin());

	if (t_row < favorites) return RowRange{0, favorites - 1};

	return RowRange{favorites, t_rows.count - 1};
}

AccountForm::AccountForm()
{
	for (const EditField field : each<EditField>()) {
		m_fields[field].set_max_length(K_FIELD_LENGTHS[field]);
	}

	m_fields[EditField::NOTE].m_placeholder = "Main, smurf, ranked...";
}

auto AccountForm::start(const Library& t_library, std::optional<AccountRef> t_account, u32 t_game) -> void
{
	m_account       = t_account;
	m_show_required = false;

	if (t_account) {
		const Account& account = *t_library.account(*t_account);

		m_fields[EditField::USERNAME].set_value(account.username);
		m_fields[EditField::PASSWORD].set_value(account.password);
		m_fields[EditField::NOTE].set_value(account.note);
		copy_to(std::string_view{account.region}, m_region);
		m_visible_mask = account.visible_games(t_account->game);
	} else {
		for (TextInput& field : m_fields) {
			field.set_value("");
		}

		m_region[0]    = '\0';
		m_visible_mask = game_bit(t_game);
	}

	m_fields[EditField::PASSWORD].m_masked = true;
	focus(EditField::USERNAME);
}

auto AccountForm::clear() -> void
{
	for (TextInput& field : m_fields) {
		field.set_value("");
		field.set_focused(false);
	}

	sodium_memzero(m_region, sizeof(m_region));
	m_account.reset();
	m_show_required = false;
}

auto AccountForm::focused_field() const -> std::optional<EditField>
{
	for (const EditField field : each<EditField>()) {
		if (m_fields[field].m_focused) return field;
	}

	return std::nullopt;
}

auto AccountForm::focus(std::optional<EditField> t_field) -> void
{
	for (const EditField field : each<EditField>()) {
		m_fields[field].set_focused(field == t_field);
	}
}

auto AccountForm::next_field(bool t_backwards) const -> EditField
{
	constexpr usize COUNT   = std::size(K_FIELD_ORDER);
	const auto      current = std::ranges::find(K_FIELD_ORDER, focused_field());
	if (current == std::end(K_FIELD_ORDER)) return K_FIELD_ORDER[0];

	const usize position = static_cast<usize>(current - std::begin(K_FIELD_ORDER));
	const usize step     = t_backwards ? COUNT - 1 : 1;

	return K_FIELD_ORDER[(position + step) % COUNT];
}

auto AccountForm::is_selecting_text() const -> bool
{
	return std::ranges::any_of(m_fields, &TextInput::m_selecting);
}

auto AccountForm::end_text_selection() -> bool
{
	const bool was_selecting = is_selecting_text();

	for (TextInput& field : m_fields) {
		field.on_pointer_up();
	}

	return was_selecting;
}

auto AccountForm::update(float t_delta_seconds) -> void
{
	for (TextInput& field : m_fields) {
		field.update(t_delta_seconds);
	}
}

auto AccountForm::is_only_game(u32 t_game) const -> bool
{
	return (m_visible_mask & game_bit(t_game)) != 0 && std::popcount(m_visible_mask) == 1;
}

auto AccountForm::toggle_game(u32 t_game) -> void
{
	if (!is_only_game(t_game)) {
		m_visible_mask ^= game_bit(t_game);
	}
}

auto AccountForm::choose_region(u32 t_index) -> void
{
	if (t_index < K_REGION_COUNT) {
		copy_to(K_REGION_OPTIONS[t_index].code, m_region);
	}
}

auto AccountForm::toggle_password_shown() -> void
{
	TextInput* password = &m_fields[EditField::PASSWORD];
	password->m_masked  = !password->m_masked;
}

auto AccountForm::has_changes(const Library& t_library) const -> bool
{
	const std::string_view note     = m_fields[EditField::NOTE].value();
	const std::string_view username = m_fields[EditField::USERNAME].value();
	const std::string_view password = m_fields[EditField::PASSWORD].value();
	const std::string_view region   = m_region;

	if (!m_account) return !note.empty() || !region.empty() || !username.empty() || !password.empty();

	const Account& account = *t_library.account(*m_account);

	return note != account.note || region != account.region || username != account.username || password != account.password || m_visible_mask != account.visible_games(m_account->game);
}

auto AccountForm::save(Library* t_library, u32 t_game) -> std::expected<AccountRef, SaveProblem>
{
	const std::string_view username = m_fields[EditField::USERNAME].value();
	const std::string_view password = m_fields[EditField::PASSWORD].value();
	const std::string_view note     = m_fields[EditField::NOTE].value();

	if (username.empty() || password.empty()) {
		const EditField first_missing = username.empty() ? EditField::USERNAME : EditField::PASSWORD;
		m_show_required               = true;
		focus(first_missing);
		return std::unexpected(SaveProblem::MISSING_FIELDS);
	}

	if (m_account) {
		Account* account = t_library->account(*m_account);
		account->assign(username, note, password);
		copy_to(std::string_view{m_region}, account->region);
		account->visible_game_mask = m_visible_mask;

		return *m_account;
	}

	Account account{.visible_game_mask = m_visible_mask};
	account.assign(username, note, password);
	copy_to(std::string_view{m_region}, account.region);

	const std::optional<AccountRef> added = t_library->add_account(t_game, account);
	sodium_memzero(&account, sizeof(account));

	if (!added) return std::unexpected(SaveProblem::NO_ROOM);

	return *added;
}
