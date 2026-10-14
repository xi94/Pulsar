#pragma once

#include <expected>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "core/enum_array.h"
#include "core/library.h"
#include "ui/commands.h"
#include "ui/row_selection.h"
#include "ui/text_input.h"

class LoginSession;
class Toasts;

class AccountList {
  public:
	AccountList(Library* t_library, LoginSession* t_session, Toasts* t_toasts, CommandType t_undo_command);

	[[nodiscard]] auto has_game() const -> bool;
	[[nodiscard]] auto rows() const -> VisibleAccounts;
	auto set_query(std::string_view t_query) -> bool;
	[[nodiscard]] auto shows_every_account() const -> bool;

	[[nodiscard]] auto lifted_top(float t_pointer_in_list, float t_row_height) const -> float;
	auto lift(const VisibleAccounts& t_rows, u32 t_row, float t_pointer_in_list, float t_row_height) -> void;
	auto drop(float t_lifted_top, float t_row_height) -> void;
	auto cancel_drag(float t_lifted_top, float t_row_height) -> void;
	auto update_motion(u32 t_row_count, float t_lifted_top, float t_row_height, float t_delta_seconds) -> void;
	auto ease_hover(std::optional<u32> t_hovered_row, u32 t_row_count, float t_delta_seconds) -> void;
	auto slide_from(const VisibleAccounts& t_before, float t_row_height) -> void;
	auto reset_motion() -> void;

	[[nodiscard]] auto select_with_modifiers(u32 t_row) -> bool;
	auto select_accounts(std::span<const AccountRef> t_accounts) -> void;

	auto toggle_favorite(AccountRef t_account, float t_row_height) -> void;
	// The returned spans point into the list and last until its next call.
	auto delete_accounts(std::span<const AccountRef> t_accounts) -> std::span<const AccountRef>;
	auto take_picked() -> std::span<const AccountRef>;
	auto undo_delete() -> std::span<const AccountRef>;
	auto forget_deleted_without_undo() -> void;
	auto forget_deleted() -> void;
	auto forget_secrets() -> void;

	std::optional<u32> m_game;
	char               m_query[K_TEXT_INPUT_CAPACITY]{};
	RowDrag            m_drag;
	RowSelection       m_selection;
	RowSet             m_raised;
	float              m_offsets[K_MAX_VISIBLE_ACCOUNTS]{};
	float              m_hover[K_MAX_VISIBLE_ACCOUNTS]{};
	float              m_lift = 0.0f;

  private:
	struct DeletedAccount {
		Account    account;
		AccountRef position;
	};

	struct RowRange {
		u32 first;
		u32 last;
	};

	[[nodiscard]] auto drag_range(const VisibleAccounts& t_rows, u32 t_row) const -> RowRange;

	Library*      m_library;
	LoginSession* m_session;
	Toasts*       m_toasts;
	CommandType   m_undo_command;

	std::vector<DeletedAccount> m_deleted;
	AccountRef                  m_changed[K_MAX_VISIBLE_ACCOUNTS]{};
	AccountRef                  m_picked[K_MAX_VISIBLE_ACCOUNTS]{};
};

enum class SaveProblem : u8 {
	MISSING_FIELDS,
	NO_ROOM,
};

class AccountForm {
  public:
	AccountForm();

	auto start(const Library& t_library, std::optional<AccountRef> t_account, u32 t_game) -> void;
	auto clear() -> void;

	[[nodiscard]] auto focused_field() const -> std::optional<EditField>;
	auto focus(std::optional<EditField> t_field) -> void;
	[[nodiscard]] auto next_field(bool t_backwards) const -> EditField;
	[[nodiscard]] auto is_selecting_text() const -> bool;
	auto end_text_selection() -> bool;
	auto update(float t_delta_seconds) -> void;

	[[nodiscard]] auto is_only_game(u32 t_game) const -> bool;
	auto toggle_game(u32 t_game) -> void;
	auto choose_region(u32 t_index) -> void;
	auto toggle_password_shown() -> void;

	[[nodiscard]] auto has_changes(const Library& t_library) const -> bool;
	[[nodiscard]] auto save(Library* t_library, u32 t_game) -> std::expected<AccountRef, SaveProblem>;

	EnumArray<EditField, TextInput> m_fields{};
	char                            m_region[sizeof(Account::region)]{};
	u16                             m_visible_mask  = 0;
	bool                            m_show_required = false;
	std::optional<AccountRef>       m_account;
};
