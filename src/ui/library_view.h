#pragma once

#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "core/library.h"
#include "ui/commands.h"
#include "ui/list_popup.h"
#include "ui/row_selection.h"
#include "ui/scrollable.h"
#include "ui/text_input.h"

class Assets;
class DrawList;
struct Fonts;
class LoginSession;
struct Settings;
class Toasts;

// The account side of the Library view: the picked game's accounts in columns, logged into with one click and edited in place.
class LibraryView {
  public:
	LibraryView(Library*        t_library,
	            const Settings* t_settings,
	            const Fonts*    t_fonts,
	            const Assets*   t_assets,
	            Toasts*         t_toasts,
	            LoginSession*   t_session,
	            CommandQueue*   t_commands);

	auto show_game(i32 t_game) -> void;
	auto restart() -> void;

	auto set_bounds(Rect t_bounds) -> void
	{
		m_bounds = t_bounds;
	}

	[[nodiscard]] auto bounds() const -> Rect
	{
		return m_bounds;
	}

	auto set_mouse(Vec2 t_mouse) -> void
	{
		m_mouse = t_mouse;
	}

	auto update(float t_delta_seconds) -> void;
	auto draw(DrawList* t_draw_list, u8 t_alpha) -> void;

	auto on_pointer_down(Vec2 t_point) -> void;
	auto on_pointer_move(Vec2 t_point) -> bool;
	auto on_pointer_up(Vec2 t_point) -> bool;
	auto on_right_click(Vec2 t_point) -> void;
	auto on_scroll(float t_wheel_delta) -> void;
	auto on_key_down(os::Key t_key) -> bool;
	auto on_char(u32 t_character) -> bool;
	auto drop_press() -> void;
	auto blur() -> void;

	auto toggle_favorite(AccountRef t_account) -> void;
	auto edit(AccountRef t_account) -> void;
	auto delete_account(AccountRef t_account) -> void;
	auto delete_picked() -> void;
	auto undo_delete() -> void;
	auto forget_secrets() -> void;

	[[nodiscard]] auto cursor() const -> CursorKind;

  private:
	static constexpr u32 K_FIELD_COUNT = 3;
	static constexpr u32 K_USERNAME    = 0;
	static constexpr u32 K_PASSWORD    = 1;
	static constexpr u32 K_NOTE        = 2;

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
		Target target = Target::NONE;
		u32    index  = 0;

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
		Rect    rows;
		float   row_height;
		Columns columns;
	};

	struct FormLayout {
		Rect  title;
		Rect  labels[K_FIELD_COUNT + 2];
		Rect  fields[K_FIELD_COUNT];
		Rect  reveal;
		Rect  region;
		Rect  games[K_MAX_GAMES];
		bool  has_games;
		Rect  cancel;
		Rect  save;
		float height;
	};

	struct DeletedAccount {
		Account    account;
		AccountRef position;
	};

	struct RowRange {
		u32 first;
		u32 last;
	};

	[[nodiscard]] static auto is_row_target(Target t_target) -> bool;
	[[nodiscard]] auto has_game() const -> bool;
	[[nodiscard]] auto shown_accounts() const -> VisibleAccounts;
	[[nodiscard]] auto row_of(const VisibleAccounts& t_accounts, AccountRef t_account) const -> std::optional<u32>;
	[[nodiscard]] auto layout() const -> Layout;
	[[nodiscard]] auto columns(float t_row_width) const -> Columns;
	[[nodiscard]] auto pitch(const Layout& t_layout) const -> float;
	[[nodiscard]] auto form_shown() const -> bool;
	[[nodiscard]] auto form_row(const VisibleAccounts& t_accounts) const -> std::optional<u32>;
	[[nodiscard]] auto form_extra(const Layout& t_layout) const -> float;
	[[nodiscard]] auto form_card(const Layout& t_layout, const VisibleAccounts& t_accounts) const -> Rect;
	[[nodiscard]] auto form_layout(Rect t_card) const -> FormLayout;
	[[nodiscard]] auto row_rect(const Layout& t_layout, const VisibleAccounts& t_accounts, u32 t_row) const -> Rect;
	[[nodiscard]] auto dragged_rect(const Layout& t_layout, u32 t_row) const -> Rect;
	[[nodiscard]] auto star_rect(const Layout& t_layout, Rect t_row) const -> Rect;
	[[nodiscard]] auto login_rect(const Layout& t_layout, Rect t_row, std::string_view t_label) const -> Rect;
	[[nodiscard]] auto field_text_rect(const FormLayout& t_form, u32 t_field) const -> Rect;
	[[nodiscard]] auto content_height(const Layout& t_layout, const VisibleAccounts& t_accounts) const -> float;
	[[nodiscard]] auto scroll_geometry(const Layout& t_layout, const VisibleAccounts& t_accounts) const -> ScrollGeometry;
	[[nodiscard]] auto empty_zone(const Layout& t_layout) const -> Rect;
	[[nodiscard]] auto shows_login(AccountRef t_account) const -> bool;
	[[nodiscard]] auto login_action(AccountRef t_account) const -> std::string_view;
	[[nodiscard]] auto hit_at(Vec2 t_point) const -> Hit;
	[[nodiscard]] auto row_hit(const Layout& t_layout, Rect t_row, AccountRef t_account, u32 t_index, Vec2 t_point) const -> Hit;
	[[nodiscard]] auto form_hit(Rect t_card, Vec2 t_point) const -> Hit;
	[[nodiscard]] auto row_entrance(u32 t_row) const -> float;
	[[nodiscard]] auto drag_range(const VisibleAccounts& t_accounts, u32 t_row) const -> RowRange;
	[[nodiscard]] auto lifted_top(const Layout& t_layout) const -> float;
	[[nodiscard]] auto focused_field() const -> std::optional<u32>;
	[[nodiscard]] auto can_select() const -> bool;
	[[nodiscard]] auto marquee_area(const Layout& t_layout) const -> Rect;

	auto activate(Hit t_hit) -> void;
	auto request_login(AccountRef t_account) -> void;
	auto open_form(std::optional<AccountRef> t_account) -> void;
	auto close_form(bool t_animated) -> void;
	auto clear_form() -> void;
	auto save_form() -> void;
	auto delete_accounts(std::span<const AccountRef> t_accounts) -> void;
	auto reveal_form(const Layout& t_layout, const VisibleAccounts& t_accounts) -> void;
	auto forget_deleted() -> void;
	auto focus_field(std::optional<u32> t_field) -> void;
	auto focus_filter(bool t_focused) -> void;
	auto toggle_shown_game(u32 t_game) -> void;
	auto open_region_list() -> void;
	auto refresh_filter() -> void;
	auto reveal_row(AccountRef t_account) -> void;
	auto lift_row(u32 t_row, Vec2 t_point) -> void;
	auto drop_row() -> void;
	auto cancel_drag() -> void;
	auto update_drag(float t_delta_seconds) -> void;
	auto update_marquee(float t_delta_seconds) -> void;
	auto scroll_near_edges(const Layout& t_layout, const VisibleAccounts& t_accounts, float t_delta_seconds) -> void;
	auto select_with_modifiers(u32 t_row) -> bool;
	auto animate_reorder(const VisibleAccounts& t_before) -> void;
	auto reset_row_motion() -> void;

	auto draw_header(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) -> void;
	auto draw_labels(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) const -> void;
	auto draw_row(DrawList* t_draw_list, const Layout& t_layout, Rect t_row, AccountRef t_account, u32 t_index, bool t_raised, u8 t_alpha) const -> void;
	auto draw_login_state(DrawList* t_draw_list, const Layout& t_layout, Rect t_row, AccountRef t_account, u32 t_index, u8 t_alpha) const -> void;
	auto draw_form(DrawList* t_draw_list, const Layout& t_layout, const VisibleAccounts& t_accounts, Rect t_card, u8 t_alpha) -> void;
	auto draw_empty_state(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) const -> void;
	auto draw_no_matches(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) const -> void;

	Library*        m_library;
	const Settings* m_settings;
	const Fonts*    m_fonts;
	const Assets*   m_assets;
	Toasts*         m_toasts;
	LoginSession*   m_session;
	CommandQueue*   m_commands;

	Rect       m_bounds{};
	Vec2       m_mouse{-1.0f, -1.0f};
	i32        m_game   = -1;
	float      m_appear = 0.0f;
	Scrollable m_scroll;
	Hit        m_pressed;
	Hit        m_hovered;
	float      m_add_hover  = 0.0f;
	float      m_zone_hover = 0.0f;
	float      m_row_hover[K_MAX_VISIBLE_ACCOUNTS]{};

	TextInput m_filter;
	char      m_applied_filter[K_TEXT_INPUT_CAPACITY]{};

	RowDrag      m_drag;
	RowSelection m_selection;
	float        m_row_offsets[K_MAX_VISIBLE_ACCOUNTS]{};
	RowSet       m_raised;
	float        m_lift_amount = 0.0f;

	std::optional<AccountRef> m_flash_account;
	float                     m_flash = 0.0f;

	bool                      m_form_open = false;
	std::optional<AccountRef> m_form_account;
	float                     m_form_amount = 0.0f;
	float                     m_form_height = 0.0f;
	TextInput                 m_fields[K_FIELD_COUNT];
	char                      m_region[8]{};
	u16                       m_visible_mask  = 0;
	bool                      m_show_required = false;
	bool                      m_reveal_form   = false;

	std::vector<DeletedAccount> m_deleted;
	ListPopup                   m_region_list;
};
