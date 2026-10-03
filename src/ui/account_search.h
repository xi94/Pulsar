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
namespace os {
class Window;
}

class AccountSearch : public Widget {
  public:
	AccountSearch(const Library* t_library, const Fonts* t_fonts, const Assets* t_assets, const os::Window* t_window, CommandQueue* t_commands);

	auto open() -> void;
	auto close() -> void;

	[[nodiscard]] auto is_open() const -> bool
	{
		return m_open;
	}

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
	enum class ActionKind : u8 {
		Edit,
		CopyUsername,
		CopyPassword,
		Login,
	};

	struct Action {
		ActionKind kind;
		u32        game;
	};

	struct Layout {
		Rect panel;
		Rect header;
		Rect list;
		Rect footer;
	};

	static constexpr u32 K_MAX_ACTIONS = 3 + K_MAX_GAMES;

	[[nodiscard]] auto is_valid(AccountRef t_account) const -> bool;
	auto rebuild_results() -> void;
	auto rebuild_actions() -> void;
	auto show_account(AccountRef t_account) -> void;
	auto show_results() -> void;
	auto activate(u32 t_row) -> void;
	auto move_highlight(i32 t_rows) -> void;

	[[nodiscard]] auto row_count() const -> u32;
	[[nodiscard]] auto has_group_gap() const -> bool;
	[[nodiscard]] auto shown_rows() const -> u32;
	[[nodiscard]] auto row_height() const -> float;
	[[nodiscard]] auto layout() const -> Layout;
	[[nodiscard]] auto query_text_rect(const Layout& t_layout) const -> Rect;
	[[nodiscard]] auto back_button_rect(const Layout& t_layout) const -> Rect;
	[[nodiscard]] auto row_rect(const Layout& t_layout, u32 t_row) const -> Rect;
	[[nodiscard]] auto row_at(const Layout& t_layout, Vec2 t_point) const -> std::optional<u32>;

	auto draw_header(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) -> void;
	auto draw_result(DrawList* t_draw_list, Rect t_row, AccountRef t_account, bool t_highlighted, u8 t_alpha) const -> void;
	auto draw_action(DrawList* t_draw_list, Rect t_row, const Action& t_action, bool t_highlighted, u8 t_alpha) const -> void;
	auto draw_footer(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) const -> void;

	const Library*    m_library;
	const Fonts*      m_fonts;
	const Assets*     m_assets;
	const os::Window* m_window;
	CommandQueue*     m_commands;

	bool                      m_open        = false;
	float                     m_open_amount = 0.0f;
	TextInput                 m_query;
	std::vector<AccountRef>   m_results;
	float                     m_name_column   = 0.0f;
	float                     m_region_column = 0.0f;
	std::optional<AccountRef> m_account;
	Action                    m_actions[K_MAX_ACTIONS]{};
	u32                       m_action_count = 0;
	u32                       m_highlighted  = 0;
	u32                       m_first_row    = 0;
	std::optional<u32>        m_pressed_row;
	bool                      m_pressed_outside = false;
	bool                      m_pressed_back    = false;
};
