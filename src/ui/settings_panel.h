#pragma once

#include <future>
#include <iterator>
#include <span>
#include <string_view>

#include "core/settings.h"
#include "gfx/font.h"
#include "ui/color_picker.h"
#include "ui/commands.h"
#include "ui/draggable.h"
#include "ui/list_popup.h"
#include "ui/scrollable.h"
#include "ui/text_input.h"
#include "ui/tooltip.h"
#include "ui/widget.h"

class Assets;
class Renderer;
namespace os {
class Window;
}

enum class SettingsTab : u8 {
	APPEARANCE,
	BEHAVIOR,
	PRIVACY,
	SECURITY,
	COUNT,
};

constexpr u32 K_SETTINGS_TAB_COUNT = static_cast<u32>(SettingsTab::COUNT);

enum class ResettableSetting : u8 {
	THEME,
	FONT,
	FONT_SIZE,
	SECONDARY_FONT_SIZE,
	ACCENT,
	CORNER_ROUNDNESS,
	BACKGROUND,
	PATTERN_INTENSITY,
	LIGHT_INTENSITY,
	GRAIN_INTENSITY,
	CARET_STYLE,
	TRAIL_STRENGTH,
	ANIMATION_SPEED,
	CLOSE_TO_TRAY,
	RENDERER,
	AUTO_LOCK,
	RIOT_CLIENT,
	COUNT,
};

class SettingsPanel : public Widget {
  public:
	SettingsPanel(Settings* t_settings, Fonts* t_fonts, Renderer* t_renderer, const os::Window* t_window, const Assets* t_assets, CommandQueue* t_commands);

	auto open() -> void;
	auto close() -> void;
	auto sync_with_settings() -> void;
	auto restore_committed_previews(Settings* t_settings) const -> void;

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
		return m_open_amount > 0.01f;
	}

	[[nodiscard]] auto cursor() const -> CursorKind override;

  private:
	static constexpr u32 K_GROUP_COUNT = 9;

	enum class SliderKind : u8 {
		CORNER_ROUNDNESS,
		PATTERN_STRENGTH,
		LIGHT_STRENGTH,
		GRAIN_STRENGTH,
		TRAIL_STRENGTH,
		ANIMATION_SPEED,
		AUTO_LOCK,
		COUNT,
	};

	static constexpr u32 K_SLIDER_COUNT = static_cast<u32>(SliderKind::COUNT);

	struct Layout {
		Rect panel;
		Rect inner;
		Rect header;
		Rect rail;
		Rect rows_region;
		bool docked;
	};

	struct Rows {
		Rect  theme;
		Rect  font;
		Rect  font_size;
		Rect  secondary_font_size;
		Rect  caret_style;
		Rect  caret_trail;
		Rect  accent;
		Rect  corner_roundness;
		Rect  background;
		Rect  background_light;
		Rect  background_grain;
		Rect  snow;
		Rect  notifications;
		Rect  animations;
		Rect  hide_from_capture;
		Rect  block_overlay_injection;
		Rect  close_to_tray;
		Rect  renderer;
		Rect  riot_client;
		Rect  auto_lock;
		Rect  master_password;
		float content_height;
		float row_width;
		Rect  cards[K_GROUP_COUNT];
		Rect  group_titles[K_GROUP_COUNT];
		u32   listed_count;
	};

	struct ThemeChoice {
		ThemeKind theme;
		Color     accent;

		auto operator==(const ThemeChoice&) const -> bool = default;
	};

	struct RowSpec {
		Rect Rows::* row;
		SettingsTab  tab;
		u32          group;
		const char*  title;
		const char*  description;
		const char*  keywords;
	};

	struct GroupSpec {
		SettingsTab tab;
		const char* title;
	};

	struct Toggle {
		bool Settings::* value;
		Rect Rows::* row;
	};

	static const RowSpec   K_ROW_SPECS[];
	static const GroupSpec K_GROUP_SPECS[K_GROUP_COUNT];

	struct PercentSlider {
		Rect Rows::* row;
		float Settings::* value;
		bool (*shown)(const Settings* t_settings);
	};

	static constexpr u32       K_PERCENT_SLIDER_COUNT = 4;
	static const PercentSlider K_PERCENT_SLIDERS[K_PERCENT_SLIDER_COUNT];

	static constexpr Toggle K_TOGGLES[]{
		{&Settings::show_notifications, &Rows::notifications},
		{&Settings::animations_enabled, &Rows::animations},
		{&Settings::caret_trail, &Rows::caret_trail},
		{&Settings::hide_from_capture, &Rows::hide_from_capture},
		{&Settings::block_overlay_injection, &Rows::block_overlay_injection},
		{&Settings::background_light, &Rows::background_light},
		{&Settings::background_grain, &Rows::background_grain},
		{&Settings::snow, &Rows::snow},
	};

	static constexpr u32 K_TOGGLE_COUNT       = static_cast<u32>(std::size(K_TOGGLES));
	static constexpr u32 K_FIRST_TOGGLE_RESET = static_cast<u32>(ResettableSetting::COUNT);
	static constexpr u32 K_RESET_COUNT        = K_FIRST_TOGGLE_RESET + K_TOGGLE_COUNT;

	[[nodiscard]] auto layout() const -> Layout;
	[[nodiscard]] auto rows(const Layout& t_layout) const -> Rows;
	[[nodiscard]] auto rows_scroll(const Layout& t_layout, const Rows& t_rows) const -> ScrollGeometry;
	[[nodiscard]] auto is_on_screen(const Layout& t_layout, Rect t_row) const -> bool;
	[[nodiscard]] auto hits(const Layout& t_layout, Rect t_row, Rect t_control, Vec2 t_point) const -> bool;
	[[nodiscard]] auto open_list() -> ListPopup*;
	[[nodiscard]] auto open_list() const -> const ListPopup*;
	[[nodiscard]] auto has_popup_open() const -> bool;
	[[nodiscard]] auto tab_rect(const Layout& t_layout, SettingsTab t_tab) const -> Rect;
	[[nodiscard]] auto tab_at(const Layout& t_layout, Vec2 t_point) const -> std::optional<SettingsTab>;
	auto select_tab(SettingsTab t_tab) -> void;
	[[nodiscard]] auto is_row_hovered(const Layout& t_layout, Rect t_row) const -> bool;

	[[nodiscard]] static auto spec_of(Rect Rows::* t_row) -> const RowSpec&;
	[[nodiscard]] auto is_searching() const -> bool;
	[[nodiscard]] auto matches_search(const RowSpec& t_spec) const -> bool;
	[[nodiscard]] auto is_listed(const RowSpec& t_spec) const -> bool;
	[[nodiscard]] auto row_extent(const RowSpec& t_spec) const -> float;

	[[nodiscard]] auto inline_slider_left(Rect t_row) const -> float;
	[[nodiscard]] auto slider_line(const Rows& t_rows, SliderKind t_slider) const -> Rect;
	[[nodiscard]] auto slider_rect(const Rows& t_rows, SliderKind t_slider) const -> Rect;
	[[nodiscard]] auto slider_hit_rect(const Rows& t_rows, SliderKind t_slider) const -> Rect;
	[[nodiscard]] auto slider_visibility(SliderKind t_slider) const -> float;
	[[nodiscard]] auto slider_fraction(SliderKind t_slider) const -> float;
	[[nodiscard]] auto slider_readout(SliderKind t_slider, char (&t_buffer)[16]) const -> std::string_view;
	[[nodiscard]] auto slider_at(const Layout& t_layout, const Rows& t_rows, Vec2 t_point) const -> std::optional<SliderKind>;
	auto apply_slider(SliderKind t_slider, float t_fraction) -> void;

	[[nodiscard]] auto pattern_tile_size() const -> Vec2;
	[[nodiscard]] auto pattern_popup_rect(const Rows& t_rows) const -> Rect;
	[[nodiscard]] auto pattern_tile(Rect t_popup, u32 t_index) const -> Rect;
	[[nodiscard]] auto pattern_at(Rect t_popup, Vec2 t_point) const -> std::optional<u32>;

	[[nodiscard]] auto segment_choice_rect(Rect t_row, std::span<const std::string_view> t_labels) const -> Rect;
	[[nodiscard]] auto choice_segment(Rect t_choice, std::span<const std::string_view> t_labels, u32 t_index) const -> Rect;
	[[nodiscard]] auto search_rect(const Layout& t_layout) const -> Rect;
	auto refresh_search() -> void;
	auto clear_search() -> void;
	auto focus_search() -> void;

	[[nodiscard]] static auto reset_toggle(u32 t_setting) -> const Toggle*;
	[[nodiscard]] auto reset_row(const Rows& t_rows, u32 t_setting) const -> Rect;
	[[nodiscard]] auto reset_control(const Rows& t_rows, u32 t_setting) const -> Rect;
	[[nodiscard]] auto reset_button(const Rows& t_rows, u32 t_setting) const -> Rect;
	[[nodiscard]] static auto reset_slider(u32 t_setting) -> std::optional<SliderKind>;
	[[nodiscard]] auto can_reset(u32 t_setting) const -> bool;
	auto settle_resets() -> void;
	[[nodiscard]] auto is_default(u32 t_setting) const -> bool;
	auto reset_to_default(u32 t_setting) -> void;

	auto load_fonts(std::string_view t_file) -> bool;
	auto start_font_scan() -> void;
	auto take_font_scan(bool t_wait) -> void;
	auto open_font_list() -> void;
	auto open_theme_list() -> void;
	auto choose_font(u32 t_index) -> void;
	auto refresh_font_label() -> void;
	auto show_theme(ThemeChoice t_choice) -> void;
	auto select_theme(ThemeKind t_theme) -> void;
	auto choose_theme(ThemeKind t_theme) -> void;
	auto cycle_theme(i32 t_step) -> void;
	auto update_theme_preview() -> void;
	auto choose_background(u32 t_index) -> void;
	auto pull_picked_color() -> void;
	auto step_font_size(Rect t_stepper, float* t_value, float t_min, float t_max, Vec2 t_point) -> void;
	auto handle_click(Vec2 t_point) -> void;

	auto update_hover_hints(float t_delta_seconds) -> void;

	auto draw_chrome(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) const -> void;
	auto draw_rail(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) const -> void;
	[[nodiscard]] auto renderer_note(char (&t_buffer)[96]) const -> const char*;
	auto draw_label(DrawList* t_draw_list, const Rows& t_rows, Rect Rows::* t_row, Rect t_control, u8 t_alpha) const -> void;
	auto draw_dropdown_row(DrawList*     t_draw_list,
	                       const Layout& t_layout,
	                       const Rows&   t_rows,
	                       Rect Rows::*     t_row,
	                       std::string_view t_value,
	                       bool             t_open,
	                       u8               t_alpha) const -> void;
	auto draw_search(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) -> void;
	auto draw_cards(DrawList* t_draw_list, const Layout& t_layout, const Rows& t_rows, u8 t_alpha) const -> void;
	auto draw_captions(DrawList* t_draw_list, const Layout& t_layout, const Rows& t_rows, u8 t_alpha) const -> void;
	auto draw_no_results(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) const -> void;
	auto draw_appearance(DrawList* t_draw_list, const Layout& t_layout, const Rows& t_rows, u8 t_alpha) const -> void;
	auto draw_pattern_row(DrawList* t_draw_list, const Layout& t_layout, const Rows& t_rows, u8 t_alpha) const -> void;
	auto draw_pattern_popup(DrawList* t_draw_list, const Rows& t_rows, u8 t_alpha) const -> void;
	auto draw_segment_choice(DrawList*     t_draw_list,
	                         const Layout& t_layout,
	                         const Rows&   t_rows,
	                         Rect Rows::*                      t_row,
	                         std::span<const std::string_view> t_labels,
	                         float                             t_selected,
	                         u8                                t_alpha) const -> void;
	auto draw_toggles(DrawList* t_draw_list, const Layout& t_layout, const Rows& t_rows, u8 t_alpha) const -> void;
	auto draw_sliders(DrawList* t_draw_list, const Layout& t_layout, const Rows& t_rows, u8 t_alpha) const -> void;
	auto draw_riot_client(DrawList* t_draw_list, const Layout& t_layout, const Rows& t_rows, u8 t_alpha) const -> void;
	auto draw_master_password(DrawList* t_draw_list, const Layout& t_layout, const Rows& t_rows, u8 t_alpha) const -> void;
	auto draw_reset_buttons(DrawList* t_draw_list, const Layout& t_layout, const Rows& t_rows, u8 t_alpha) const -> void;

	Settings*         m_settings;
	Fonts*            m_fonts;
	Renderer*         m_renderer;
	const os::Window* m_window;
	const Assets*     m_assets;
	CommandQueue*     m_commands;

	bool        m_open          = false;
	SettingsTab m_tab           = SettingsTab::APPEARANCE;
	float       m_tab_indicator = 0.0f;
	float       m_open_amount   = 0.0f;

	InstalledFonts                m_installed_fonts;
	std::future<InstalledFonts>   m_font_scan;
	std::vector<std::string_view> m_font_names;
	std::string                   m_font_label;
	ListPopup                     m_font_list;
	ListPopup                     m_theme_list;
	std::optional<ThemeChoice>    m_theme_before_preview;
	float                         m_theme_wheel = 0.0f;
	ColorPicker                   m_color_picker;
	Scrollable                    m_rows_scroll;
	Tooltip                       m_tooltip;
	Draggable                     m_slider_drags[K_SLIDER_COUNT];

	TextInput m_search;
	char      m_applied_query[K_TEXT_INPUT_CAPACITY]{};

	float m_toggles_shown[K_TOGGLE_COUNT]{};
	float m_slider_hover[K_SLIDER_COUNT]{};
	float m_pattern_ring[K_BACKGROUND_COUNT]{};
	bool  m_pattern_open          = false;
	float m_pattern_open_amount   = 0.0f;
	float m_close_choice_shown    = 0.0f;
	float m_renderer_choice_shown = 0.0f;
	float m_caret_style_shown     = 0.0f;

	std::string_view m_renderer_labels[2];

	float m_font_size_shown           = 0.0f;
	float m_secondary_font_size_shown = 0.0f;
	float m_animation_speed_shown     = 0.0f;
	float m_corner_roundness_shown    = 0.0f;
	float m_percent_shown[K_PERCENT_SLIDER_COUNT]{};
	float m_percent_reveal[K_PERCENT_SLIDER_COUNT]{};
	float m_animation_speed_reveal = 0.0f;
	float m_auto_lock_shown        = 0.0f;
	float m_accent_shown[3]{};

	float m_reset_visible[K_RESET_COUNT]{};
	float m_reset_spin[K_RESET_COUNT]{};
};
