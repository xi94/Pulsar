#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/library.h"
#include "ui/commands.h"
#include "ui/text_input.h"
#include "ui/widget.h"

class Assets;
struct Fonts;
class Window;

class AccountSearch : public Widget {
  public:
	AccountSearch(const Library *t_library, const Fonts *t_fonts, const Assets *t_assets, const Window *t_window, CommandQueue *t_commands);

	void open();
	void close();

	bool is_open() const
	{
		return m_open;
	}

	void update(float t_delta_seconds) override;
	void draw(DrawList *t_draw_list) override;

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
	enum class ActionKind : u8 {
		Edit,
		CopyUsername,
		CopyPassword,
		Login,
	};

	struct Action {
		ActionKind kind;
		u32 game;
	};

	struct Layout {
		Rect panel;
		Rect header;
		Rect list;
		Rect footer;
	};

	static constexpr u32 max_actions = 3 + max_games;

	bool is_valid(AccountRef t_account) const;
	void rebuild_results();
	void rebuild_actions();
	void show_account(AccountRef t_account);
	void show_results();
	void activate(u32 t_row);
	void move_highlight(i32 t_rows);

	u32 row_count() const;
	bool has_group_gap() const;
	u32 shown_rows() const;
	float row_height() const;
	Layout layout() const;
	Rect query_text_rect(const Layout &t_layout) const;
	Rect back_button_rect(const Layout &t_layout) const;
	Rect row_rect(const Layout &t_layout, u32 t_row) const;
	std::optional<u32> row_at(const Layout &t_layout, Vec2 t_point) const;

	void draw_header(DrawList *t_draw_list, const Layout &t_layout, u8 t_alpha);
	void draw_result(DrawList *t_draw_list, Rect t_row, AccountRef t_account, bool t_highlighted, u8 t_alpha) const;
	void draw_action(DrawList *t_draw_list, Rect t_row, const Action &t_action, bool t_highlighted, u8 t_alpha) const;
	void draw_footer(DrawList *t_draw_list, const Layout &t_layout, u8 t_alpha) const;

	const Library *m_library;
	const Fonts *m_fonts;
	const Assets *m_assets;
	const Window *m_window;
	CommandQueue *m_commands;

	bool m_open = false;
	float m_open_amount = 0.0f;
	TextInput m_query;
	std::vector<AccountRef> m_results;
	float m_name_column = 0.0f;
	float m_region_column = 0.0f;
	std::optional<AccountRef> m_account;
	Action m_actions[max_actions]{};
	u32 m_action_count = 0;
	u32 m_highlighted = 0;
	u32 m_first_row = 0;
	std::optional<u32> m_pressed_row;
	bool m_pressed_outside = false;
	bool m_pressed_back = false;
};
