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

class Assets;
class DrawList;
struct Fonts;
class LoginSession;
struct Settings;
class Toasts;

class LibraryView {
  public:
	LibraryView(Library* t_library, const Settings* t_settings, const Fonts* t_fonts, const Assets* t_assets, Toasts* t_toasts, LoginSession* t_session, CommandQueue* t_commands);

	auto show_game(std::optional<u32> t_game) -> void;
	auto update(float t_delta_seconds) -> void;
	auto draw(DrawList* t_draw_list, u8 t_alpha) -> void;

	auto on_pointer_down(Vec2 t_point) -> void;
	auto on_pointer_move(Vec2 t_point) -> bool;
	auto on_pointer_up(Vec2 t_point) -> bool;
	auto on_right_click(Vec2 t_point) -> void;
	auto on_scroll(float t_wheel_delta) -> void;
	auto on_key_down(os::Key t_key) -> bool;
	auto on_char(u32 t_character) -> bool;
	auto cancel_press() -> void;
	auto unfocus() -> void;

	auto edit(AccountRef t_account) -> void;
	auto delete_account(AccountRef t_account) -> void;
	auto delete_picked() -> void;
	auto undo_delete() -> void;
	auto forget_secrets() -> void;

	[[nodiscard]] auto cursor() const -> CursorKind;

	Rect m_bounds{};
	Vec2 m_mouse{-1.0f, -1.0f};

  private:
	enum class Target : u8 {
		NONE,
		ROW,
		STAR,
		LOGIN,
		CANCEL_LOGIN,
		PERMISSION,
		ADD,
		FILTER,
		FILTER_CLEAR,
		FIELD,
		REVEAL,
		REGION,
		SHOW_IN,
		CANCEL_FORM,
		SAVE,
		FORM,
	};

	struct Hit {
		Target    target = Target::NONE;
		u32       index  = 0;
		EditField field  = EditField::USERNAME;

		auto operator==(const Hit&) const -> bool = default;
	};

	struct Columns {
		float star;
		float account;
		float account_width;
		float region;
		float region_width;
		float played;
		float played_width;
		float actions;
		bool  show_region;
		bool  show_played;
	};

	struct Layout {
		Rect    header;
		float   title_baseline;
		Rect    filter;
		Rect    add;
		Rect    labels;
		Rect    rows_area;
		float   row_height;
		Columns columns;
	};

	struct FormLayout {
		Rect                       title;
		EnumArray<EditField, Rect> labels;
		EnumArray<EditField, Rect> fields;
		Rect                       region_label;
		Rect                       show_in_label;
		Rect                       reveal;
		Rect                       region;
		Rect                       games[K_MAX_GAMES];
		bool                       has_games;
		Rect                       cancel;
		Rect                       save;
		float                      height;
	};

	[[nodiscard]] static auto is_row_target(Target t_target) -> bool;
	[[nodiscard]] auto layout() const -> Layout;
	[[nodiscard]] auto columns(float t_row_width) const -> Columns;
	[[nodiscard]] auto first_row_y(const Layout& t_layout) const -> float;
	[[nodiscard]] auto pointer_in_list(const Layout& t_layout, float t_y) const -> float;
	[[nodiscard]] auto lifted_top(const Layout& t_layout) const -> float;
	[[nodiscard]] auto form_shown() const -> bool;
	[[nodiscard]] auto form_row(const VisibleAccounts& t_accounts) const -> std::optional<u32>;
	[[nodiscard]] auto form_growth(const Layout& t_layout) const -> float;
	[[nodiscard]] auto form_card(const Layout& t_layout, const VisibleAccounts& t_accounts) const -> Rect;
	[[nodiscard]] auto form_layout(Rect t_card) const -> FormLayout;
	[[nodiscard]] auto row_rect(const Layout& t_layout, const VisibleAccounts& t_accounts, u32 t_row) const -> Rect;
	[[nodiscard]] auto dragged_rect(const Layout& t_layout, u32 t_row) const -> Rect;
	[[nodiscard]] auto star_rect(const Layout& t_layout, Rect t_row) const -> Rect;
	[[nodiscard]] auto login_rect(Rect t_row, std::string_view t_label) const -> Rect;
	[[nodiscard]] auto field_text_rect(const FormLayout& t_form, EditField t_field) const -> Rect;
	[[nodiscard]] auto content_height(const Layout& t_layout, const VisibleAccounts& t_accounts) const -> float;
	[[nodiscard]] auto scroll_geometry(const Layout& t_layout, const VisibleAccounts& t_accounts) const -> ScrollGeometry;
	[[nodiscard]] auto empty_zone(const Layout& t_layout) const -> Rect;
	[[nodiscard]] auto marquee_area(const Layout& t_layout) const -> Rect;
	[[nodiscard]] auto shows_login(AccountRef t_account) const -> bool;
	[[nodiscard]] auto login_button_label(AccountRef t_account) const -> std::string_view;
	[[nodiscard]] auto hit_at(Vec2 t_point) const -> Hit;
	[[nodiscard]] auto row_hit(const Layout& t_layout, Rect t_row, AccountRef t_account, u32 t_index, Vec2 t_point) const -> Hit;
	[[nodiscard]] auto form_hit(Rect t_card, Vec2 t_point) const -> Hit;
	[[nodiscard]] auto row_entrance(u32 t_row) const -> float;
	[[nodiscard]] auto can_select() const -> bool;

	auto click_target(Hit t_hit) -> void;
	auto request_login(AccountRef t_account) -> void;
	auto toggle_favorite(AccountRef t_account) -> void;
	auto open_form(std::optional<AccountRef> t_account) -> void;
	auto close_form(bool t_animated) -> void;
	auto save_form() -> void;
	auto delete_accounts(std::span<const AccountRef> t_accounts) -> void;
	auto reveal_form(const Layout& t_layout, const VisibleAccounts& t_accounts) -> void;
	auto focus_filter(bool t_focused) -> void;
	auto open_region_list() -> void;
	auto refresh_filter() -> void;
	auto reveal_row(AccountRef t_account) -> void;
	auto lift_row(u32 t_row, Vec2 t_point) -> void;
	auto drop_row() -> void;
	auto cancel_drag() -> void;
	auto update_drag(float t_delta_seconds) -> void;
	auto update_marquee(float t_delta_seconds) -> void;
	auto scroll_near_edges(const Layout& t_layout, const VisibleAccounts& t_accounts, float t_delta_seconds) -> void;

	auto draw_header(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) -> void;
	auto draw_labels(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) const -> void;
	auto draw_row(DrawList* t_draw_list, const Layout& t_layout, Rect t_row, AccountRef t_account, u32 t_index, bool t_raised, u8 t_alpha) const -> void;
	auto draw_login_state(DrawList* t_draw_list, Rect t_row, AccountRef t_account, u32 t_index, u8 t_alpha) const -> void;
	auto draw_form(DrawList* t_draw_list, const Layout& t_layout, const VisibleAccounts& t_accounts, Rect t_card, u8 t_alpha) -> void;
	auto draw_form_fields(DrawList* t_draw_list, const FormLayout& t_form, bool t_live, u8 t_alpha) -> void;
	auto draw_form_region(DrawList* t_draw_list, const FormLayout& t_form, bool t_live, u8 t_alpha) const -> void;
	auto draw_form_games(DrawList* t_draw_list, const FormLayout& t_form, bool t_live, u8 t_alpha) const -> void;
	auto draw_empty_state(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) const -> void;
	auto draw_no_matches(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) const -> void;

	Library*        m_library;
	const Settings* m_settings;
	const Fonts*    m_fonts;
	const Assets*   m_assets;
	Toasts*         m_toasts;
	LoginSession*   m_session;
	CommandQueue*   m_commands;

	AccountList m_list;
	AccountForm m_form;
	TextInput   m_filter;
	Scrollable  m_scroll;
	ListPopup   m_region_list;

	float m_appear = 0.0f;
	Hit   m_pressed;
	Hit   m_hovered;
	float m_add_hover  = 0.0f;
	float m_zone_hover = 0.0f;

	std::optional<AccountRef> m_flash_account;
	float                     m_flash = 0.0f;

	bool  m_form_open         = false;
	float m_form_amount       = 0.0f;
	float m_form_height       = 0.0f;
	bool  m_keep_form_in_view = false;
};
