#pragma once

#include <optional>
#include <span>
#include <string_view>

#include "core/enum_array.h"
#include "core/library.h"
#include "ui/account_list.h"
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

class AccountModal : public Widget {
  public:
	AccountModal(Library* t_library, Settings* t_settings, const Fonts* t_fonts, const Assets* t_assets, const os::Window* t_window, Toasts* t_toasts, LoginSession* t_session, CommandQueue* t_commands);

	auto open(u32 t_game) -> void;
	auto close() -> void;
	auto quick_login(u32 t_game, AccountRef t_account) -> void;
	auto edit_account(AccountRef t_account) -> void;
	auto edit_row(std::optional<u32> t_row) -> void;
	auto delete_row(std::optional<u32> t_row) -> void;
	auto delete_picked() -> void;
	auto undo_delete() -> void;
	auto forget_secrets() -> void;

	[[nodiscard]] auto detached_game() const -> std::optional<u32>;
	[[nodiscard]] auto account_at_row(std::optional<u32> t_row) const -> const Account*;

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

	std::optional<ArtSource> m_art_source;

  private:
	enum class Mode : u8 {
		ACCOUNT_LIST,
		LOGIN_PROGRESS,
		EDIT_ACCOUNT,
	};

	struct Layout {
		Rect panel;
		Rect inner;
		Rect art_column;
		Rect sheet;
		Rect main_column;
		Rect footer;
	};

	struct ListLayout {
		VisibleAccounts accounts;
		float           row_height;
		Rect            area;
		ScrollGeometry  scroll;
	};

	struct FormLayout {
		Rect                       area;
		EnumArray<EditField, Rect> field_labels;
		EnumArray<EditField, Rect> field_inputs;
		Rect                       reveal;
		Rect                       region_label;
		Rect                       region_input;
		Rect                       show_in_label;
		Rect                       show_in_input;
		Rect                       tiles;
		u32                        tile_columns;
		float                      content_height;
	};

	[[nodiscard]] auto floating_panel_size() const -> Vec2;
	[[nodiscard]] auto expanded_form_panel_height(float t_panel_width) const -> float;
	[[nodiscard]] auto is_docked() const -> bool;
	[[nodiscard]] auto panel_rect() const -> Rect;
	[[nodiscard]] auto layout() const -> Layout;
	[[nodiscard]] auto back_badge_rect(const Layout& t_layout) const -> Rect;

	[[nodiscard]] auto list_layout(const Layout& t_layout) const -> ListLayout;
	[[nodiscard]] auto list_to_screen(const ListLayout& t_list, float t_list_y) const -> float;
	[[nodiscard]] auto pointer_in_list(const ListLayout& t_list, float t_y) const -> float;
	[[nodiscard]] auto row_rect_at(const Layout& t_layout, const ListLayout& t_list, float t_top_in_list) const -> Rect;
	[[nodiscard]] auto row_rect(const Layout& t_layout, const ListLayout& t_list, u32 t_row) const -> Rect;
	[[nodiscard]] auto row_at(const Layout& t_layout, const ListLayout& t_list, Vec2 t_point) const -> std::optional<u32>;
	[[nodiscard]] auto row_account(std::optional<u32> t_row) const -> std::optional<AccountRef>;
	[[nodiscard]] auto login_button_rect(Rect t_row) const -> Rect;
	[[nodiscard]] auto favorite_button_rect(Rect t_row) const -> Rect;
	[[nodiscard]] auto is_row_button_hit(Rect t_row, Vec2 t_point) const -> bool;
	[[nodiscard]] auto list_header(Rect t_main) const -> Rect;
	[[nodiscard]] auto add_button_rect(Rect t_main) const -> Rect;
	[[nodiscard]] auto search_rect(Rect t_main) const -> Rect;
	[[nodiscard]] auto is_search_visible() const -> bool;
	[[nodiscard]] auto empty_zone(const Layout& t_layout, Rect t_list_area) const -> Rect;
	[[nodiscard]] auto lifted_top(const ListLayout& t_list) const -> float;
	[[nodiscard]] auto raised_top(const ListLayout& t_list, u32 t_row) const -> float;
	[[nodiscard]] auto can_select() const -> bool;

	[[nodiscard]] auto primary_button_rect(Rect t_footer) const -> Rect;
	[[nodiscard]] auto cancel_button_rect(Rect t_primary) const -> Rect;
	[[nodiscard]] auto asks_for_permission() const -> bool;
	[[nodiscard]] auto save_label() const -> std::string_view;
	[[nodiscard]] auto edit_header_height() const -> float;
	[[nodiscard]] auto form_area(Rect t_main) const -> Rect;
	[[nodiscard]] auto form_layout(Rect t_main) const -> FormLayout;
	[[nodiscard]] auto form_scroll(const FormLayout& t_form) const -> ScrollGeometry;
	[[nodiscard]] auto field_text_rect(const FormLayout& t_form, EditField t_field) const -> Rect;
	[[nodiscard]] auto show_in_columns(float t_width) const -> u32;
	[[nodiscard]] auto show_in_tile_height() const -> float;
	[[nodiscard]] auto show_in_tile(const FormLayout& t_form, u32 t_game) const -> Rect;
	[[nodiscard]] auto show_in_tile_at(const FormLayout& t_form, Vec2 t_point) const -> std::optional<u32>;
	[[nodiscard]] auto is_in_form(const FormLayout& t_form, Rect t_part, Vec2 t_point) const -> bool;
	[[nodiscard]] auto field_at(const FormLayout& t_form, Vec2 t_point) const -> std::optional<EditField>;

	auto start_form(std::optional<AccountRef> t_account) -> void;
	auto save_form() -> void;
	auto open_region_list() -> void;
	auto reveal_field(EditField t_field) -> void;

	auto request_login(u32 t_game, AccountRef t_account) -> void;
	auto toggle_favorite(AccountRef t_account) -> void;
	auto delete_accounts(std::span<const AccountRef> t_accounts) -> void;
	auto refresh_search() -> void;
	auto clear_search() -> void;
	auto reveal_account(AccountRef t_account) -> void;
	auto lift_row(const ListLayout& t_list, u32 t_row, Vec2 t_point) -> void;
	auto drop_row() -> void;
	auto cancel_row_drag() -> void;
	auto scroll_near_edges(const ListLayout& t_list, float t_delta_seconds) -> void;
	auto update_list(float t_delta_seconds) -> void;
	auto update_marquee(const ListLayout& t_list, float t_delta_seconds) -> void;
	auto request_tooltip() -> void;
	auto request_row_tooltip(const Layout& t_layout) -> void;

	auto press_in_list(const Layout& t_layout, Vec2 t_point) -> void;
	auto click_in_list(const Layout& t_layout, Vec2 t_point) -> void;
	auto click_in_form(const Layout& t_layout, Vec2 t_point) -> void;
	auto click_in_login_progress(const Layout& t_layout, Vec2 t_point) -> void;
	auto key_in_list(os::Key t_key) -> void;
	auto key_in_form(os::Key t_key) -> void;
	auto escape() -> void;

	[[nodiscard]] auto list_cursor(const Layout& t_layout) const -> CursorKind;
	[[nodiscard]] auto form_cursor(const Layout& t_layout) const -> CursorKind;

	auto draw_chrome(DrawList* t_draw_list, const Layout& t_layout, bool t_with_art, u8 t_alpha) const -> void;
	auto draw_back_badge(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) const -> void;
	auto draw_morphing_art(DrawList* t_draw_list, const Layout& t_layout, float t_scale) const -> void;
	auto draw_list_header(DrawList* t_draw_list, Rect t_main, float t_rows_top, u8 t_alpha) -> void;
	auto draw_empty_state(DrawList* t_draw_list, const Layout& t_layout, Rect t_list_area, u8 t_alpha) const -> void;
	auto draw_no_matches(DrawList* t_draw_list, Rect t_list_area, u8 t_alpha) const -> void;
	auto draw_account_list(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) -> void;
	auto draw_account_rows(DrawList* t_draw_list, const Layout& t_layout, const ListLayout& t_list, u8 t_alpha) -> void;
	auto draw_account_row(DrawList* t_draw_list, Rect t_row, const Account& t_account, bool t_raised, bool t_selected, float t_hover, u8 t_alpha) const -> void;
	auto draw_login_progress(DrawList* t_draw_list, Rect t_main, u8 t_alpha) const -> void;
	auto draw_form_header(DrawList* t_draw_list, Rect t_main, u8 t_alpha) const -> void;
	auto draw_form(DrawList* t_draw_list, Rect t_main, u8 t_alpha) -> void;
	auto draw_form_fields(DrawList* t_draw_list, const FormLayout& t_form, bool t_live, u8 t_alpha) -> void;
	auto draw_form_region(DrawList* t_draw_list, const FormLayout& t_form, bool t_live, u8 t_alpha) const -> void;
	auto draw_show_in(DrawList* t_draw_list, const FormLayout& t_form, u8 t_alpha) const -> void;
	auto draw_show_in_tiles(DrawList* t_draw_list, const FormLayout& t_form, bool t_live, u8 t_alpha) const -> void;
	auto draw_footer(DrawList* t_draw_list, Rect t_footer, u8 t_alpha) const -> void;
	auto draw_form_footer(DrawList* t_draw_list, Rect t_footer, u8 t_alpha) const -> void;

	Library*          m_library;
	Settings*         m_settings;
	const Fonts*      m_fonts;
	const Assets*     m_assets;
	const os::Window* m_window;
	Toasts*           m_toasts;
	LoginSession*     m_session;
	CommandQueue*     m_commands;

	AccountList m_list;
	AccountForm m_form;
	TextInput   m_search;
	Scrollable  m_rows_scroll;
	Scrollable  m_form_scroll;
	Tooltip     m_tooltip;
	ListPopup   m_region_list;

	Mode  m_mode            = Mode::ACCOUNT_LIST;
	bool  m_open            = false;
	float m_open_amount     = 0.0f;
	float m_morph_progress  = 0.0f;
	bool  m_press_swallowed = false;
	float m_zone_hover      = 0.0f;
	bool  m_show_in_open    = false;
	float m_show_in_amount  = 0.0f;
};
