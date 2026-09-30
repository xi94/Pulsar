#pragma once

#include "core/settings.h"
#include "gfx/font.h"
#include "ui/color_picker.h"
#include "ui/commands.h"
#include "ui/draggable.h"
#include "ui/scrollable.h"
#include "ui/text_input.h"
#include "ui/list_popup.h"
#include "ui/tooltip.h"
#include "ui/widget.h"

class Assets;
class Renderer;
class Window;

enum class SettingsTab : u8 {
	appearance,
	behavior,
	privacy,
	security,
	count,
};

constexpr u32 settings_tab_count = static_cast<u32>(SettingsTab::count);

enum class ResettableSetting : u8 {
	theme,
	font,
	font_size,
	secondary_font_size,
	accent,
	corner_roundness,
	background,
	background_light,
	background_grain,
	animations,
	notifications,
	hide_from_capture,
	block_overlay_injection,
	close_to_tray,
	auto_lock,
	count,
};

class SettingsPanel : public Widget {
  public:
	SettingsPanel(Settings &t_settings, Fonts &t_fonts, Renderer &t_renderer, const Window &t_window,
				  const Assets &t_assets, CommandQueue &t_commands);

	void open();
	void close();
	void sync_with_settings();
	void restore_committed_previews(Settings &t_settings) const;

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
	static constexpr u32 group_count = 7;

	enum class SliderKind : u8 {
		corner_roundness,
		pattern_strength,
		light_strength,
		grain_strength,
		animation_speed,
		auto_lock,
		count,
	};

	static constexpr u32 slider_count = static_cast<u32>(SliderKind::count);

	struct Layout {
		Rect panel;
		Rect inner;
		Rect header;
		Rect rail;
		Rect rows_region;
		bool docked;
	};

	struct Rows {
		Rect theme;
		Rect font;
		Rect font_size;
		Rect secondary_font_size;
		Rect accent;
		Rect corner_roundness;
		Rect background;
		Rect background_light;
		Rect background_grain;
		Rect notifications;
		Rect animations;
		Rect hide_from_capture;
		Rect block_overlay_injection;
		Rect close_to_tray;
		Rect auto_lock;
		Rect master_password;
		float content_height;
		Rect cards[group_count];
		Rect group_titles[group_count];
		u32 listed_count;
	};

	struct ThemeChoice {
		ThemeKind theme;
		Color accent;

		bool operator==(const ThemeChoice &) const = default;
	};

	struct RowSpec {
		Rect Rows::*row;
		SettingsTab tab;
		u32 group;
		const char *title;
		const char *description;
		const char *keywords;
	};

	struct GroupSpec {
		SettingsTab tab;
		const char *title;
	};

	struct Toggle {
		bool Settings::*value;
		Rect Rows::*row;
	};

	static constexpr u32 row_spec_count = 16;
	static const RowSpec row_specs[row_spec_count];
	static const GroupSpec group_specs[group_count];

	struct PercentSlider {
		Rect Rows::*row;
		float Settings::*value;
		bool (*shown)(const Settings &t_settings);
	};

	static constexpr u32 percent_slider_count = 3;
	static const PercentSlider percent_sliders[percent_slider_count];

	static constexpr u32 toggle_count = 6;
	static const Toggle toggles[toggle_count];

	Layout layout() const;
	Rows rows(const Layout &t_layout) const;
	ScrollGeometry rows_scroll(const Layout &t_layout, const Rows &t_rows) const;
	bool is_on_screen(const Layout &t_layout, Rect t_row) const;
	bool hits(const Layout &t_layout, Rect t_row, Rect t_control, Vec2 t_point) const;
	ListPopup *open_list();
	const ListPopup *open_list() const;
	bool has_popup_open() const;
	Rect tab_rect(const Layout &t_layout, SettingsTab t_tab) const;
	std::optional<SettingsTab> tab_at(const Layout &t_layout, Vec2 t_point) const;
	void select_tab(SettingsTab t_tab);
	bool is_row_hovered(const Layout &t_layout, Rect t_row) const;

	static const RowSpec &spec_of(Rect Rows::*t_row);
	bool is_searching() const;
	bool matches_search(const RowSpec &t_spec) const;
	bool is_listed(const RowSpec &t_spec) const;
	float row_extent(const RowSpec &t_spec) const;

	float inline_slider_left(Rect t_row) const;
	Rect slider_line(const Rows &t_rows, SliderKind t_slider) const;
	Rect slider_rect(const Rows &t_rows, SliderKind t_slider) const;
	Rect slider_hit_rect(const Rows &t_rows, SliderKind t_slider) const;
	float slider_visibility(SliderKind t_slider) const;
	float slider_fraction(SliderKind t_slider) const;
	std::string_view slider_readout(SliderKind t_slider, char (&t_buffer)[16]) const;
	std::optional<SliderKind> slider_at(const Layout &t_layout, const Rows &t_rows, Vec2 t_point) const;
	void apply_slider(SliderKind t_slider, float t_fraction);

	Vec2 pattern_tile_size() const;
	Rect pattern_popup_rect(const Rows &t_rows) const;
	Rect pattern_tile(Rect t_popup, u32 t_index) const;
	std::optional<u32> pattern_at(Rect t_popup, Vec2 t_point) const;

	Rect close_choice_rect(Rect t_row) const;
	Rect close_segment(Rect t_choice, u32 t_index) const;
	Rect search_rect(const Layout &t_layout) const;
	Rect search_text_rect(Rect t_search) const;
	Rect search_clear_rect(Rect t_search) const;
	void refresh_search();
	void clear_search();
	void focus_search();

	Rect reset_row(const Rows &t_rows, ResettableSetting t_setting) const;
	Rect reset_control(const Rows &t_rows, ResettableSetting t_setting) const;
	Rect reset_button(const Rows &t_rows, ResettableSetting t_setting) const;
	bool is_default(ResettableSetting t_setting) const;
	void reset(ResettableSetting t_setting);

	bool load_fonts(std::string_view t_file);
	void open_font_list();
	void open_theme_list();
	void choose_font(u32 t_index);
	void refresh_font_label();
	void show_theme(ThemeChoice t_choice);
	void select_theme(ThemeKind t_theme);
	void choose_theme(ThemeKind t_theme);
	void cycle_theme(i32 t_step);
	void update_theme_preview();
	void choose_background(u32 t_index);
	void pull_picked_color();
	void step_font_size(Rect t_stepper, float &t_value, float t_min, float t_max, Vec2 t_point);
	void handle_click(Vec2 t_point);

	void update_hover_hints(float t_delta_seconds);

	void draw_chrome(DrawList &t_draw_list, const Layout &t_layout, u8 t_alpha) const;
	void draw_rail(DrawList &t_draw_list, const Layout &t_layout, u8 t_alpha) const;
	void draw_label(DrawList &t_draw_list, const Rows &t_rows, Rect Rows::*t_row, Rect t_control, u8 t_alpha) const;
	void draw_dropdown_row(DrawList &t_draw_list, const Layout &t_layout, const Rows &t_rows, Rect Rows::*t_row,
						   std::string_view t_value, bool t_open, u8 t_alpha) const;
	void draw_search(DrawList &t_draw_list, const Layout &t_layout, u8 t_alpha);
	void draw_cards(DrawList &t_draw_list, const Layout &t_layout, const Rows &t_rows, u8 t_alpha) const;
	void draw_captions(DrawList &t_draw_list, const Layout &t_layout, const Rows &t_rows, u8 t_alpha) const;
	void draw_no_results(DrawList &t_draw_list, const Layout &t_layout, u8 t_alpha) const;
	void draw_appearance(DrawList &t_draw_list, const Layout &t_layout, const Rows &t_rows, u8 t_alpha) const;
	void draw_pattern_row(DrawList &t_draw_list, const Layout &t_layout, const Rows &t_rows, u8 t_alpha) const;
	void draw_pattern_popup(DrawList &t_draw_list, const Rows &t_rows, u8 t_alpha) const;
	void draw_close_choice(DrawList &t_draw_list, const Layout &t_layout, const Rows &t_rows, u8 t_alpha) const;
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
	SettingsTab m_tab = SettingsTab::appearance;
	float m_tab_indicator = 0.0f;
	float m_open_amount = 0.0f;

	InstalledFonts m_installed_fonts;
	std::vector<std::string_view> m_font_names;
	std::string m_font_label;
	ListPopup m_font_list;
	ListPopup m_theme_list;
	std::optional<ThemeChoice> m_theme_before_preview;
	float m_theme_wheel = 0.0f;
	ColorPicker m_color_picker;
	Scrollable m_rows_scroll;
	Tooltip m_tooltip;
	Draggable m_slider_drags[slider_count];

	TextInput m_search;
	char m_applied_query[text_input_capacity]{};

	float m_toggles_shown[toggle_count]{1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
	float m_slider_hover[slider_count]{};
	float m_pattern_ring[background_count]{};
	bool m_pattern_open = false;
	float m_pattern_open_amount = 0.0f;
	float m_close_choice_shown = 0.0f;

	float m_font_size_shown = 0.0f;
	float m_secondary_font_size_shown = 0.0f;
	float m_animation_speed_shown = 0.0f;
	float m_corner_roundness_shown = 0.0f;
	float m_percent_shown[percent_slider_count]{};
	float m_percent_reveal[percent_slider_count]{};
	float m_animation_speed_reveal = 0.0f;
	float m_auto_lock_shown = 0.0f;
	float m_accent_shown[3]{};

	float m_reset_visible[resettable_count]{};
	float m_reset_spin[resettable_count]{};
};
