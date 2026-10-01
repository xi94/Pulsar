#pragma once

#include "ui/commands.h"
#include "ui/text_input.h"
#include "ui/widget.h"

class Assets;
struct Fonts;
class MasterKey;
class Window;
struct Settings;

class UnlockScreen : public Widget {
  public:
	UnlockScreen(Settings &t_settings, MasterKey &t_master_key, const Fonts &t_fonts, const Assets &t_assets,
				 const Window &t_window, CommandQueue &t_commands);

	void show_unlock();
	void show_setup();
	void hide();

	void update(float t_delta_seconds) override;
	void draw(DrawList &t_draw_list) override;

	bool on_pointer_down(Vec2 t_point) override;
	bool on_pointer_move(Vec2 t_point) override;
	bool on_pointer_up(Vec2 t_point) override;
	bool on_right_click(Vec2 t_point) override;
	bool on_key_down(u32 t_key) override;
	bool on_char(u32 t_character) override;

	bool is_blocking() const override
	{
		return m_active;
	}

	CursorKind cursor() const override;

  private:
	static constexpr u32 password = 0;
	static constexpr u32 confirmation = 1;

	u32 field_count() const
	{
		return m_setup ? 2 : 1;
	}

	Rect card_rect() const;
	Rect field_rect(u32 t_field) const;
	Rect field_text_rect(u32 t_field) const;
	Rect reveal_rect(u32 t_field) const;
	Rect submit_rect() const;
	i32 field_at(Vec2 t_point) const;
	bool is_reveal_hit(Vec2 t_point) const;

	void reset_fields();
	void focus_field(u32 t_field);
	void submit();
	void attempt_unlock();
	void attempt_setup();

	void draw_field(DrawList &t_draw_list, u32 t_field);
	void draw_submit_button(DrawList &t_draw_list, std::string_view t_label) const;

	Settings &m_settings;
	MasterKey &m_master_key;
	const Fonts &m_fonts;
	const Assets &m_assets;
	const Window &m_window;
	CommandQueue &m_commands;

	bool m_active = false;
	bool m_setup = false;
	bool m_wrong_password = false;
	bool m_passwords_differ = false;
	bool m_setup_failed = false;

	TextInput m_fields[2];
};
