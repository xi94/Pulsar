#pragma once

#include <optional>
#include <string_view>

#include "core/library.h"
#include "ui/commands.h"
#include "ui/list_popup.h"
#include "ui/scrollable.h"
#include "ui/text_input.h"
#include "ui/tooltip.h"
#include "ui/widget.h"

class Assets;
struct Fonts;
class LoginSession;
class Toasts;
namespace os {
class Window;
}
struct Settings;

enum class EditField : u8 {
	NOTE,
	USERNAME,
	PASSWORD,
	COUNT,
};

class AccountModal : public Widget {
  public:
	AccountModal(Library*          t_library,
	             Settings*         t_settings,
	             const Fonts*      t_fonts,
	             const Assets*     t_assets,
	             const os::Window* t_window,
	             Toasts*           t_toasts,
	             LoginSession*     t_session,
	             CommandQueue*     t_commands);

	auto open(i32 t_game) -> void;

	auto set_art_source(ArtSource t_source) -> void
	{
		m_art_source = t_source;
	}

	[[nodiscard]] auto detached_game() const -> i32;
	auto close() -> void;
	auto quick_login(u32 t_game, AccountRef t_account) -> void;
	auto edit_account(AccountRef t_account) -> void;
	auto undo_delete() -> void;
	auto toggle_favorite(i32 t_row) -> void;
	auto forget_secrets() -> void;

	[[nodiscard]] auto account_at_row(i32 t_row) const -> const Account*;

	auto update(float t_delta_seconds) -> void override;
	auto draw(DrawList* t_draw_list) -> void override;

	auto on_pointer_down(Vec2 t_point) -> bool override;
	auto on_pointer_move(Vec2 t_point) -> bool override;
	auto on_pointer_up(Vec2 t_point) -> bool override;
	auto on_right_click(Vec2 t_point) -> bool override;
	auto on_scroll(Vec2 t_point, float t_wheel_delta) -> bool override;
	auto on_key_down(os::Key t_key) -> bool override;
	auto on_char(u32 t_character) -> bool override;

	[[nodiscard]] auto is_blocking() const -> bool override
	{
		return m_open || m_open_amount > 0.01f;
	}

	[[nodiscard]] auto cursor() const -> CursorKind override;

  private:
	static constexpr u32 K_FIELD_COUNT = static_cast<u32>(EditField::COUNT);

	enum class Mode : u8 {
		ACCOUNT_LIST,
		LOGIN_PROGRESS,
		EDIT_ACCOUNT,
	};

	struct Layout {
		Rect panel;
		Rect inner;
		Rect art_column;
		Rect main_column;
		Rect footer;
	};

	struct AccountRows {
		VisibleAccounts accounts;
		float           row_height;
		Rect            region;
		ScrollGeometry  scroll;
	};

	struct EmptyState {
		Rect  icon;
		float title_baseline;
		float hint_baseline;
		Rect  button;
	};

	static constexpr u32 K_FORM_ROW_COUNT = K_FIELD_COUNT + 2;
	static constexpr u32 K_REGION_ROW     = K_FIELD_COUNT;
	static constexpr u32 K_SHOW_IN_ROW    = K_FIELD_COUNT + 1;

	struct FormLayout {
		Rect  region;
		Rect  labels[K_FORM_ROW_COUNT];
		Rect  inputs[K_FORM_ROW_COUNT];
		Rect  tiles;
		u32   tile_columns;
		float content_height;
	};

	struct DeletedAccount {
		Account    account;
		AccountRef position;
	};

	struct RowRange {
		u32 first;
		u32 last;
	};

	struct RowDrag {
		std::optional<u32> pressed_row;
		Vec2               press_point{};
		bool               lifted      = false;
		u32                from_row    = 0;
		u32                target_row  = 0;
		float              grab_offset = 0.0f;
	};

	[[nodiscard]] auto has_game() const -> bool;
	[[nodiscard]] auto back_badge_rect(const Layout& t_layout) const -> Rect;
	auto request_tooltip() -> void;
	auto request_row_tooltip(const Layout& t_layout) -> void;
	[[nodiscard]] auto empty_state(Rect t_region) const -> EmptyState;
	[[nodiscard]] auto floating_panel_size() const -> Vec2;
	[[nodiscard]] auto expanded_form_panel_height(float t_panel_width) const -> float;
	[[nodiscard]] auto is_docked() const -> bool;
	[[nodiscard]] auto panel_rect() const -> Rect;
	[[nodiscard]] auto layout() const -> Layout;

	[[nodiscard]] auto displayed_accounts() const -> VisibleAccounts;
	[[nodiscard]] auto account_rows(const Layout& t_layout) const -> AccountRows;
	[[nodiscard]] auto selected_row(const VisibleAccounts& t_accounts) const -> i32;

	[[nodiscard]] auto content_to_screen(const AccountRows& t_rows, float t_content_y) const -> float;
	[[nodiscard]] auto row_rect_at(const Layout& t_layout, const AccountRows& t_rows, float t_content_top) const -> Rect;
	[[nodiscard]] auto row_rect(const Layout& t_layout, const AccountRows& t_rows, u32 t_row) const -> Rect;
	[[nodiscard]] auto row_at(const Layout& t_layout, const AccountRows& t_rows, Vec2 t_point) const -> i32;
	[[nodiscard]] auto remove_button_rect(Rect t_row) const -> Rect;
	[[nodiscard]] auto edit_button_rect(Rect t_row) const -> Rect;
	[[nodiscard]] auto favorite_button_rect(Rect t_row) const -> Rect;
	[[nodiscard]] auto is_row_button_hit(Rect t_row, Vec2 t_point) const -> bool;
	[[nodiscard]] auto add_button_rect(Rect t_main) const -> Rect;
	[[nodiscard]] auto search_rect(Rect t_main) const -> Rect;
	[[nodiscard]] auto primary_button_rect(Rect t_footer) const -> Rect;
	[[nodiscard]] auto cancel_button_rect(Rect t_primary) const -> Rect;
	[[nodiscard]] auto form_region(Rect t_main) const -> Rect;
	[[nodiscard]] auto form_scroll(Rect t_main) const -> ScrollGeometry;
	[[nodiscard]] auto form_layout(Rect t_main) const -> FormLayout;
	[[nodiscard]] auto field_input_rect(Rect t_main, u32 t_field) const -> Rect;
	[[nodiscard]] auto field_text_rect(Rect t_main, u32 t_field) const -> Rect;
	[[nodiscard]] auto reveal_button_rect(Rect t_main) const -> Rect;
	[[nodiscard]] auto show_in_rect(Rect t_main) const -> Rect;
	[[nodiscard]] auto is_show_in_hit(Rect t_main, Vec2 t_point) const -> bool;
	[[nodiscard]] auto region_rect(Rect t_main) const -> Rect;
	[[nodiscard]] auto is_region_hit(Rect t_main, Vec2 t_point) const -> bool;
	auto open_region_list() -> void;
	auto choose_region(u32 t_index) -> void;
	[[nodiscard]] auto edit_header_height() const -> float;
	[[nodiscard]] auto save_label() const -> std::string_view;
	[[nodiscard]] auto asks_for_permission() const -> bool;
	[[nodiscard]] auto show_in_columns(float t_width) const -> u32;
	[[nodiscard]] auto show_in_tile_height() const -> float;
	[[nodiscard]] auto show_in_tile(const FormLayout& t_form, u32 t_game) const -> Rect;
	[[nodiscard]] auto show_in_tile_at(Rect t_main, Vec2 t_point) const -> std::optional<u32>;
	auto toggle_visible_game(u32 t_game) -> void;

	[[nodiscard]] auto field(EditField t_field) -> TextInput*
	{
		return &m_fields[static_cast<u32>(t_field)];
	}

	[[nodiscard]] auto field(EditField t_field) const -> const TextInput*
	{
		return &m_fields[static_cast<u32>(t_field)];
	}

	[[nodiscard]] auto focused_field() const -> i32;
	auto focus_field(i32 t_field) -> void;
	[[nodiscard]] auto field_at(Rect t_main, Vec2 t_point) const -> i32;
	[[nodiscard]] auto is_reveal_hit(Rect t_main, Vec2 t_point) const -> bool;
	auto reveal_field(i32 t_field) -> void;

	auto start_adding() -> void;
	auto start_editing(AccountRef t_account) -> void;
	[[nodiscard]] auto can_save() const -> bool;
	[[nodiscard]] auto has_changes() const -> bool;
	auto save_edit() -> void;
	auto delete_account(AccountRef t_account) -> void;
	auto forget_deleted() -> void;
	auto arm_or_delete(AccountRef t_account) -> void;
	[[nodiscard]] auto delete_countdown(AccountRef t_account) const -> float;
	auto toggle_favorite(AccountRef t_account) -> void;
	auto follow_insert(AccountRef t_inserted) -> void;
	auto follow_removal(AccountRef t_removed) -> void;

	auto request_login(u32 t_game, AccountRef t_account) -> void;
	auto cancel_login() -> void;

	auto refresh_search() -> void;
	auto clear_search() -> void;
	auto reveal_selected() -> void;
	auto select_step(i32 t_step) -> void;

	[[nodiscard]] auto is_search_visible() const -> bool;
	[[nodiscard]] auto drag_range(const VisibleAccounts& t_accounts, u32 t_row) const -> RowRange;
	[[nodiscard]] auto lifted_top(const AccountRows& t_rows) const -> float;
	auto lift_row(const AccountRows& t_rows, u32 t_row, Vec2 t_point) -> void;
	auto drop_row() -> void;
	auto cancel_row_drag() -> void;
	auto update_row_drag(float t_delta_seconds) -> void;
	auto animate_reorder(const VisibleAccounts& t_before) -> void;
	auto reset_row_motion() -> void;

	auto handle_list_key(os::Key t_key) -> bool;
	auto handle_list_press(const Layout& t_layout, Vec2 t_point) -> void;
	auto handle_list_click(const Layout& t_layout, Vec2 t_point) -> void;
	auto handle_edit_click(const Layout& t_layout, Vec2 t_point) -> void;

	auto notify(std::string_view t_message) -> void;

	[[nodiscard]] auto list_cursor(const Layout& t_layout) const -> CursorKind;
	[[nodiscard]] auto edit_cursor(const Layout& t_layout) const -> CursorKind;

	auto draw_chrome(DrawList* t_draw_list, const Layout& t_layout, bool t_with_art, u8 t_alpha) const -> void;
	auto draw_back_badge(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) const -> void;
	auto draw_morphing_art(DrawList* t_draw_list, const Layout& t_layout, float t_scale) const -> void;
	auto draw_section_title(DrawList* t_draw_list, Rect t_main, std::string_view t_title, u8 t_alpha) const -> void;
	auto draw_search(DrawList* t_draw_list, Rect t_main, u8 t_alpha) -> void;
	auto draw_empty_state(DrawList* t_draw_list, Rect t_region, u8 t_alpha) const -> void;
	auto draw_no_matches(DrawList* t_draw_list, Rect t_region, u8 t_alpha) const -> void;
	auto draw_account_list(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) -> void;
	auto draw_account_row(DrawList*      t_draw_list,
	                      Rect           t_main,
	                      Rect           t_row,
	                      const Account* t_account,
	                      bool           t_selected,
	                      bool           t_raised,
	                      float          t_delete_countdown,
	                      u8             t_alpha) const -> void;
	auto draw_login_progress(DrawList* t_draw_list, Rect t_main, u8 t_alpha) const -> void;
	auto draw_edit_header(DrawList* t_draw_list, Rect t_main, u8 t_alpha) const -> void;
	auto draw_show_in(DrawList* t_draw_list, const FormLayout& t_form, u8 t_alpha) const -> void;
	auto draw_edit_form(DrawList* t_draw_list, Rect t_main, u8 t_alpha) -> void;
	auto draw_footer(DrawList* t_draw_list, Rect t_footer, u8 t_alpha) const -> void;
	auto draw_edit_footer(DrawList* t_draw_list, Rect t_footer, u8 t_alpha) const -> void;

	Library*          m_library;
	Settings*         m_settings;
	const Fonts*      m_fonts;
	const Assets*     m_assets;
	const os::Window* m_window;
	Toasts*           m_toasts;
	LoginSession*     m_session;
	CommandQueue*     m_commands;

	bool                      m_open        = false;
	float                     m_open_amount = 0.0f;
	std::optional<ArtSource>  m_art_source;
	float                     m_morph_progress  = 0.0f;
	bool                      m_press_swallowed = false;
	i32                       m_game            = -1;
	std::optional<AccountRef> m_selected;
	Mode                      m_mode = Mode::ACCOUNT_LIST;
	Scrollable                m_rows_scroll;
	Scrollable                m_form_scroll;
	Tooltip                   m_tooltip;

	TextInput m_search;
	char      m_applied_query[K_TEXT_INPUT_CAPACITY]{};

	RowDrag            m_drag;
	float              m_row_offsets[K_MAX_VISIBLE_ACCOUNTS]{};
	std::optional<u32> m_raised_row;
	float              m_lift_amount = 0.0f;

	std::optional<DeletedAccount> m_deleted;
	std::optional<AccountRef>     m_armed_delete;
	float                         m_armed_seconds = 0.0f;

	TextInput                 m_fields[K_FIELD_COUNT];
	std::optional<AccountRef> m_edited;
	bool                      m_show_required  = false;
	u16                       m_visible_mask   = 0;
	bool                      m_show_in_open   = false;
	float                     m_show_in_amount = 0.0f;
	char                      m_region[8]{};
	ListPopup                 m_region_list;
};
