#pragma once

#include "core/settings.h"
#include "ui/color_picker.h"
#include "ui/commands.h"
#include "ui/draggable.h"
#include "ui/scrollable.h"
#include "ui/text_input.h"
#include "ui/tooltip.h"
#include "ui/widget.h"

class Assets;
class Fonts;
class Renderer;
class Window;

enum class ResettableSetting : u8 {
	font,
	font_size,
	secondary_font_size,
	accent,
	corner_roundness,
	animations,
	animation_speed,
	notifications,
	hide_from_capture,
	block_overlay_injection,
	close_to_tray,
	count,
};

class SettingsPanel : public Widget {
  public:
	SettingsPanel(Settings &t_settings, Fonts &t_fonts, Renderer &t_renderer, const Window &t_window,
				  const Assets &t_assets, CommandQueue &t_commands);

	void open();
	void close();
	void sync_with_settings();

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
		return m_open_amount > 0.01f;
	}

	CursorKind cursor() const override;

  private:
	static constexpr u32 resettable_count = static_cast<u32>(ResettableSetting::count);

	struct Layout {
		Rect panel;
		Rect inner;
		Rect header;
		Rect footer;
		Rect rows_region;
	};

	struct Rows {
		Rect appearance_heading;
		Rect font;
		Rect font_size;
		Rect secondary_font_size;
		Rect accent;
		Rect corner_roundness;
		Rect notifications;
		Rect motion_heading;
		Rect animations;
		Rect animation_speed;
		Rect privacy_heading;
		Rect hide_from_capture;
		Rect block_overlay_injection;
		Rect close_to_tray;
		Rect security_heading;
		Rect master_password;
		float content_height;
	};

	struct Toggle {
		bool Settings::*value;
		Rect Rows::*row;
		const char *title;
		const char *description;
	};

	static constexpr u32 toggle_count = 5;
	static const Toggle toggles[toggle_count];

	Layout layout() const;
	Rows rows(const Layout &t_layout) const;
	ScrollGeometry rows_scroll(const Layout &t_layout, const Rows &t_rows) const;
	bool is_on_screen(const Layout &t_layout, Rect t_row) const;
	bool hits(const Layout &t_layout, Rect t_row, Rect t_control, Vec2 t_point) const;
	bool is_font_field_hit(const Layout &t_layout, const Rows &t_rows, Vec2 t_point) const;

	Rect reset_row(const Rows &t_rows, ResettableSetting t_setting) const;
	Rect reset_control(const Rows &t_rows, ResettableSetting t_setting) const;
	Rect reset_button(const Rows &t_rows, ResettableSetting t_setting) const;
	bool is_default(ResettableSetting t_setting) const;
	void reset(ResettableSetting t_setting);

	void apply_fonts();
	void apply_animation_speed(Rect t_track, float t_x);
	void apply_corner_roundness(Rect t_track, float t_x);
	void step_font_size(Rect t_stepper, float &t_value, float t_min, float t_max, Vec2 t_point);
	void handle_click(Vec2 t_point);

	void update_reset_buttons(float t_delta_seconds);

	void draw_chrome(DrawList &t_draw_list, const Layout &t_layout, u8 t_alpha) const;
	void draw_row_highlight(DrawList &t_draw_list, const Layout &t_layout, const Rows &t_rows, u8 t_alpha) const;
	void draw_headings(DrawList &t_draw_list, const Layout &t_layout, const Rows &t_rows, u8 t_alpha) const;
	void draw_appearance(DrawList &t_draw_list, const Layout &t_layout, const Rows &t_rows, u8 t_alpha);
	void draw_toggles(DrawList &t_draw_list, const Layout &t_layout, const Rows &t_rows, u8 t_alpha) const;
	void draw_sliders(DrawList &t_draw_list, const Layout &t_layout, const Rows &t_rows, u8 t_alpha) const;
	void draw_master_password(DrawList &t_draw_list, const Layout &t_layout, const Rows &t_rows, u8 t_alpha) const;
	void draw_reset_buttons(DrawList &t_draw_list, const Layout &t_layout, const Rows &t_rows, u8 t_alpha) const;

	Settings &m_settings;
	Fonts &m_fonts;
	Renderer &m_renderer;
	const Window &m_window;
	const Assets &m_assets;
	CommandQueue &m_commands;

	bool m_open = false;
	float m_open_amount = 0.0f;

	TextInput m_font_name;
	ColorPicker m_color_picker;
	Scrollable m_rows_scroll;
	Tooltip m_tooltip;
	Draggable m_animation_speed_drag;
	Draggable m_corner_roundness_drag;

	float m_toggles_shown[toggle_count]{1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

	float m_font_size_shown = 0.0f;
	float m_secondary_font_size_shown = 0.0f;
	float m_animation_speed_shown = 0.0f;
	float m_corner_roundness_shown = 0.0f;
	float m_accent_shown[3]{};

	float m_reset_visible[resettable_count]{};
	float m_reset_spin[resettable_count]{};
};
