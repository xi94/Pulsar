#pragma once

#include "ui/commands.h"
#include "ui/text_input.h"
#include "ui/widget.h"

class Assets;
struct Fonts;
class MasterKey;
namespace os {
class Window;
}
struct Settings;

class UnlockScreen : public Widget {
  public:
	UnlockScreen(Settings*         t_settings,
	             MasterKey*        t_master_key,
	             const Fonts*      t_fonts,
	             const Assets*     t_assets,
	             const os::Window* t_window,
	             CommandQueue*     t_commands);

	auto show_unlock() -> void;
	auto show_setup() -> void;
	auto hide() -> void;

	auto update(float t_delta_seconds) -> void override;
	auto draw(DrawList* t_draw_list) -> void override;

	auto on_pointer_down(Vec2 t_point) -> bool override;
	auto on_pointer_move(Vec2 t_point) -> bool override;
	auto on_pointer_up(Vec2 t_point) -> bool override;
	auto on_right_click(Vec2 t_point) -> bool override;
	auto on_key_down(os::Key t_key) -> bool override;
	auto on_char(u32 t_character) -> bool override;

	[[nodiscard]] auto is_blocking() const -> bool override
	{
		return m_active;
	}

	[[nodiscard]] auto cursor() const -> CursorKind override;

  private:
	static constexpr u32 K_PASSWORD     = 0;
	static constexpr u32 K_CONFIRMATION = 1;

	[[nodiscard]] auto field_count() const -> u32
	{
		return m_setup ? 2 : 1;
	}

	[[nodiscard]] auto card_rect() const -> Rect;
	[[nodiscard]] auto field_rect(u32 t_field) const -> Rect;
	[[nodiscard]] auto field_text_rect(u32 t_field) const -> Rect;
	[[nodiscard]] auto reveal_rect(u32 t_field) const -> Rect;
	[[nodiscard]] auto submit_rect() const -> Rect;
	[[nodiscard]] auto field_at(Vec2 t_point) const -> i32;
	[[nodiscard]] auto is_reveal_hit(Vec2 t_point) const -> bool;

	auto reset_fields() -> void;
	auto focus_field(u32 t_field) -> void;
	auto submit() -> void;
	auto attempt_unlock() -> void;
	auto attempt_setup() -> void;

	auto draw_field(DrawList* t_draw_list, u32 t_field) -> void;
	auto draw_submit_button(DrawList* t_draw_list, std::string_view t_label) const -> void;

	Settings*         m_settings;
	MasterKey*        m_master_key;
	const Fonts*      m_fonts;
	const Assets*     m_assets;
	const os::Window* m_window;
	CommandQueue*     m_commands;

	bool m_active           = false;
	bool m_setup            = false;
	bool m_wrong_password   = false;
	bool m_passwords_differ = false;
	bool m_setup_failed     = false;

	TextInput m_fields[2];
};
