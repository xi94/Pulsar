#pragma once

#include <optional>
#include <string_view>

#include "core/library.h"
#include "core/login_attempt.h"
#include "ui/commands.h"
#include "ui/game_select_popup.h"
#include "ui/list_popup.h"
#include "ui/scrollable.h"
#include "ui/text_input.h"
#include "ui/tooltip.h"
#include "ui/widget.h"

class Assets;
class Fonts;
class Toasts;
class Window;
struct Settings;

enum class EditField : u8 {
	note,
	username,
	password,
	count,
};

class AccountModal : public Widget {
  public:
	AccountModal(Library &t_library, const Settings &t_settings, const Fonts &t_fonts, const Assets &t_assets,
				 const Window &t_window, Toasts &t_toasts, CommandQueue &t_commands);

	void open(i32 t_game);

	void set_art_source(ArtSource t_source)
	{
		m_art_source = t_source;
	}

	bool has_art_source() const
	{
		return m_art_source.has_value();
	}

	i32 detached_game() const;
	void close();
	void quick_login(u32 t_game, AccountRef t_account);
	void undo_delete();
	void toggle_favorite(i32 t_row);
	void forget_secrets();

	const Account *account_at_row(i32 t_row) const;

	void update(float t_delta_seconds) override;
	void draw(DrawList &t_draw_list) override;

	bool on_pointer_down(Vec2 t_point) override;
	bool on_pointer_move(Vec2 t_point) override;
	bool on_pointer_up(Vec2 t_point) override;
	bool on_right_click(Vec2 t_point) override;
	bool on_scroll(Vec2 t_point, float t_wheel_delta) override;
	bool on_key_down(u32 t_key) override;
	bool on_char(u32 t_character) override;

	bool is_blocking() const override
	{
		return m_open || m_open_amount > 0.01f;
	}

	CursorKind cursor() const override;

  private:
	static constexpr u32 field_count = static_cast<u32>(EditField::count);

	enum class Mode : u8 {
		account_list,
		login_progress,
		edit_account,
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
		float row_height;
		Rect region;
		ScrollGeometry scroll;
	};

	struct EmptyState {
		Rect icon;
		float title_baseline;
		float hint_baseline;
		Rect button;
	};

	static constexpr u32 form_row_count = field_count + 2;
	static constexpr u32 region_row = field_count;
	static constexpr u32 show_in_row = field_count + 1;

	struct FormLayout {
		Rect region;
		Rect card;
		Rect rows[form_row_count];
		float content_height;
	};

	struct PendingLogin {
		u32 game;
		AccountRef account;
	};

	struct DeletedAccount {
		Account account;
		AccountRef position;
	};

	struct RowRange {
		u32 first;
		u32 last;
	};

	struct RowDrag {
		std::optional<u32> pressed_row;
		Vec2 press_point{};
		bool lifted = false;
		u32 from_row = 0;
		u32 target_row = 0;
		float grab_offset = 0.0f;
	};

	bool has_game() const;
	Rect back_badge_rect(const Layout &t_layout) const;
	void request_tooltip();
	void request_row_tooltip(const Layout &t_layout);
	EmptyState empty_state(Rect t_region) const;
	Vec2 floating_panel_size() const;
	bool is_docked() const;
	Rect panel_rect() const;
	Layout layout() const;

	VisibleAccounts displayed_accounts() const;
	AccountRows account_rows(const Layout &t_layout) const;
	i32 selected_row(const VisibleAccounts &t_accounts) const;

	float content_to_screen(const AccountRows &t_rows, float t_content_y) const;
	Rect row_rect_at(const Layout &t_layout, const AccountRows &t_rows, float t_content_top) const;
	Rect row_rect(const Layout &t_layout, const AccountRows &t_rows, u32 t_row) const;
	i32 row_at(const Layout &t_layout, const AccountRows &t_rows, Vec2 t_point) const;
	Rect remove_button_rect(Rect t_row) const;
	Rect edit_button_rect(Rect t_row) const;
	Rect favorite_button_rect(Rect t_row) const;
	bool is_row_button_hit(Rect t_row, Vec2 t_point) const;
	Rect add_button_rect(Rect t_main) const;
	Rect search_rect(Rect t_main) const;
	Rect search_text_rect(Rect t_search) const;
	Rect search_clear_rect(Rect t_search) const;
	Rect primary_button_rect(Rect t_footer) const;
	Rect cancel_button_rect(Rect t_primary) const;
	Rect form_region(Rect t_main) const;
	ScrollGeometry form_scroll(Rect t_main) const;
	FormLayout form_layout(Rect t_main) const;
	Rect field_input_rect(Rect t_main, u32 t_field) const;
	Rect field_text_rect(Rect t_main, u32 t_field) const;
	Rect reveal_button_rect(Rect t_main) const;
	Rect show_in_rect(Rect t_main) const;
	bool is_show_in_hit(Rect t_main, Vec2 t_point) const;
	Rect region_rect(Rect t_main) const;
	bool is_region_hit(Rect t_main, Vec2 t_point) const;
	void open_region_list();
	void choose_region(u32 t_index);
	std::string_view visibility_summary(char (&t_buffer)[32]) const;

	TextInput &field(EditField t_field)
	{
		return m_fields[static_cast<u32>(t_field)];
	}

	const TextInput &field(EditField t_field) const
	{
		return m_fields[static_cast<u32>(t_field)];
	}

	i32 focused_field() const;
	void focus_field(i32 t_field);
	i32 field_at(Rect t_main, Vec2 t_point) const;
	bool is_reveal_hit(Rect t_main, Vec2 t_point) const;
	void reveal_field(i32 t_field);

	void start_adding();
	void start_editing(AccountRef t_account);
	bool can_save() const;
	bool has_changes() const;
	void save_edit();
	void delete_account(AccountRef t_account);
	void forget_deleted();
	void arm_or_delete(AccountRef t_account);
	float delete_countdown(AccountRef t_account) const;
	void toggle_favorite(AccountRef t_account);
	void follow_insert(AccountRef t_inserted);
	void follow_removal(AccountRef t_removed);

	void request_login(u32 t_game, AccountRef t_account);
	void start_login(PendingLogin t_login);
	void cancel_login();
	void record_login_result();

	void refresh_search();
	void clear_search();
	void reveal_selected();
	void select_step(i32 t_step);

	bool is_search_visible() const;
	RowRange drag_range(const VisibleAccounts &t_accounts, u32 t_row) const;
	float lifted_top(const AccountRows &t_rows) const;
	void lift_row(const AccountRows &t_rows, u32 t_row, Vec2 t_point);
	void drop_row();
	void cancel_row_drag();
	void update_row_drag(float t_delta_seconds);
	void animate_reorder(const VisibleAccounts &t_before);
	void reset_row_motion();

	bool handle_list_key(u32 t_key);
	void handle_list_press(const Layout &t_layout, Vec2 t_point);
	void handle_list_click(const Layout &t_layout, Vec2 t_point);
	void handle_edit_click(const Layout &t_layout, Vec2 t_point);

	void notify(std::string_view t_message);

	CursorKind list_cursor(const Layout &t_layout) const;
	CursorKind edit_cursor(const Layout &t_layout) const;

	void draw_chrome(DrawList &t_draw_list, const Layout &t_layout, bool t_with_art, u8 t_alpha) const;
	void draw_back_badge(DrawList &t_draw_list, const Layout &t_layout, u8 t_alpha) const;
	void draw_morphing_art(DrawList &t_draw_list, const Layout &t_layout, float t_scale) const;
	void draw_section_title(DrawList &t_draw_list, Rect t_main, std::string_view t_title, u8 t_alpha) const;
	void draw_search(DrawList &t_draw_list, Rect t_main, u8 t_alpha);
	void draw_empty_state(DrawList &t_draw_list, Rect t_region, u8 t_alpha) const;
	void draw_no_matches(DrawList &t_draw_list, Rect t_region, u8 t_alpha) const;
	void draw_account_list(DrawList &t_draw_list, const Layout &t_layout, u8 t_alpha);
	void draw_account_row(DrawList &t_draw_list, Rect t_main, Rect t_row, const Account &t_account, bool t_selected,
						  bool t_raised, float t_delete_countdown, u8 t_alpha) const;
	void draw_row_details(DrawList &t_draw_list, Rect t_row, float t_baseline, float t_max_width,
						  const Account &t_account, u8 t_alpha) const;
	void draw_login_progress(DrawList &t_draw_list, Rect t_main, u8 t_alpha) const;
	void draw_edit_form(DrawList &t_draw_list, Rect t_main, u8 t_alpha);
	void draw_footer(DrawList &t_draw_list, Rect t_footer, u8 t_alpha) const;
	void draw_edit_footer(DrawList &t_draw_list, Rect t_footer, u8 t_alpha) const;

	Library &m_library;
	const Settings &m_settings;
	const Fonts &m_fonts;
	const Assets &m_assets;
	const Window &m_window;
	Toasts &m_toasts;
	CommandQueue &m_commands;

	bool m_open = false;
	float m_open_amount = 0.0f;
	std::optional<ArtSource> m_art_source;
	float m_morph_progress = 0.0f;
	bool m_press_swallowed = false;
	i32 m_game = -1;
	std::optional<AccountRef> m_selected;
	Mode m_mode = Mode::account_list;
	Scrollable m_rows_scroll;
	Scrollable m_form_scroll;
	Tooltip m_tooltip;

	TextInput m_search;
	char m_applied_query[text_input_capacity]{};

	RowDrag m_drag;
	float m_row_offsets[max_visible_accounts]{};
	std::optional<u32> m_raised_row;
	float m_lift_amount = 0.0f;

	std::optional<DeletedAccount> m_deleted;
	std::optional<AccountRef> m_armed_delete;
	float m_armed_seconds = 0.0f;

	LoginAttempt m_login;
	std::optional<PendingLogin> m_queued_login;
	std::optional<AccountRef> m_login_account;
	float m_login_seconds = 0.0f;

	TextInput m_fields[field_count];
	std::optional<AccountRef> m_edited;
	bool m_show_required = false;
	GameSelectPopup m_visible_games;
	char m_region[8]{};
	ListPopup m_region_list;
};
