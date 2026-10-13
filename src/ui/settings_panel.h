#pragma once

#include <future>
#include <iterator>
#include <optional>
#include <span>
#include <string_view>

#include "core/enum_array.h"
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
	enum class SettingsTab : u8 {
		APPEARANCE,
		BEHAVIOR,
		PRIVACY,
		SECURITY,
		COUNT,
	};

	enum class SettingsGroup : u8 {
		LOOK,
		BACKGROUND,
		TEXT,
		CURSOR,
		GLASS,
		MOTION,
		GENERAL,
		RIOT_CLIENT,
		PROTECTION,
		VAULT,
		COUNT,
	};

	struct GroupSpec {
		SettingsTab      tab;
		std::string_view title;
	};

	struct Layout {
		Rect panel;
		Rect inner;
		Rect header;
		Rect rail;
		Rect rows_region;
		bool docked;
	};

	struct Rows {
		Rect                           theme;
		Rect                           font;
		Rect                           font_size;
		Rect                           secondary_font_size;
		Rect                           caret_style;
		Rect                           caret_trail;
		Rect                           glass;
		Rect                           glass_tint;
		Rect                           accent;
		Rect                           corner_roundness;
		Rect                           background;
		Rect                           background_light;
		Rect                           background_grain;
		Rect                           snow;
		Rect                           notifications;
		Rect                           animations;
		Rect                           hide_from_capture;
		Rect                           block_overlay_injection;
		Rect                           start_with_system;
		Rect                           close_to_tray;
		Rect                           renderer;
		Rect                           riot_client;
		Rect                           auto_lock;
		Rect                           master_password;
		float                          content_height;
		float                          row_width;
		EnumArray<SettingsGroup, Rect> cards;
		EnumArray<SettingsGroup, Rect> group_titles;
		u32                            listed_count;
	};

	struct RowSpec {
		Rect Rows::*  row;
		SettingsGroup group;
		const char*   title;
		const char*   description;
		const char*   keywords;
	};

	struct ThemeChoice {
		ThemeKind theme;
		Color     accent;

		auto operator==(const ThemeChoice&) const -> bool = default;
	};

	enum class SliderKind : u8 {
		CORNER_ROUNDNESS,
		PATTERN_STRENGTH,
		LIGHT_STRENGTH,
		GRAIN_STRENGTH,
		TRAIL_STRENGTH,
		GLASS_BLUR,
		GLASS_TINT,
		ANIMATION_SPEED,
		AUTO_LOCK,
		COUNT,
	};

	// A slider sits in a row of its own, or inline in a row with another control. Inline sliders in Appearance all end where the Pattern
	// select starts, so they line up whatever their row's control is. The animation speed slider sits against its toggle.
	enum class SliderPlace : u8 {
		OWN_ROW,
		INLINE,
		BESIDE_TOGGLE,
	};

	// A slider and the setting it moves. Auto-lock moves between fixed stops rather than over a range, so it has no value here.
	struct SliderSpec {
		Rect Rows::*     row;
		SliderPlace      place;
		std::string_view caption;
		float Settings::* value;
		float             min;
		float             max;
		bool (*shown)(const Settings* t_settings);
	};

	enum class ChoiceKind : u8 {
		CARET_STYLE,
		STARTUP,
		CLOSE_TO_TRAY,
		RENDERER,
		COUNT,
	};

	// A row of segments for picking one of a few options, and how its segments map onto the settings.
	struct ChoiceSpec {
		Rect Rows::*                      row;
		std::span<const std::string_view> labels;
		u32 (*chosen)(const Settings* t_settings);
		void (*choose)(Settings* t_settings, u32 t_segment);
	};

	struct Toggle {
		bool Settings::* value;
		Rect Rows::* row;
	};

	static constexpr Toggle K_TOGGLES[]{
		{&Settings::show_notifications, &Rows::notifications},
		{&Settings::animations_enabled, &Rows::animations},
		{&Settings::caret_trail, &Rows::caret_trail},
		{&Settings::glass, &Rows::glass},
		{&Settings::hide_from_capture, &Rows::hide_from_capture},
		{&Settings::block_overlay_injection, &Rows::block_overlay_injection},
		{&Settings::background_light, &Rows::background_light},
		{&Settings::background_grain, &Rows::background_grain},
		{&Settings::snow, &Rows::snow},
	};

	static constexpr u32 K_TOGGLE_COUNT = static_cast<u32>(std::size(K_TOGGLES));

	enum class ResettableSetting : u8 {
		THEME,
		FONT,
		FONT_SIZE,
		SECONDARY_FONT_SIZE,
		ACCENT,
		CORNER_ROUNDNESS,
		BACKGROUND,
		PATTERN_STRENGTH,
		LIGHT,
		LIGHT_STRENGTH,
		GRAIN,
		GRAIN_STRENGTH,
		SNOW,
		CARET_STYLE,
		CARET_TRAIL,
		TRAIL_STRENGTH,
		GLASS,
		GLASS_BLUR,
		GLASS_TINT,
		ANIMATIONS,
		ANIMATION_SPEED,
		NOTIFICATIONS,
		STARTUP,
		CLOSE_TO_TRAY,
		RENDERER,
		RIOT_CLIENT,
		HIDE_FROM_CAPTURE,
		BLOCK_OVERLAY_INJECTION,
		AUTO_LOCK,
		COUNT,
	};

	// The control a reset button sits beside.
	enum class ResetAnchor : u8 {
		DROPDOWN,
		PATTERN_SELECT,
		STEPPER,
		SWATCH,
		SLIDER,
		CHOICE,
		TOGGLE,
		BROWSE_BUTTON,
	};

	// Everything about a setting's reset button: the row and control it sits beside, whether the setting is at its default, and how to put
	// it back.
	struct ResetSpec {
		Rect Rows::* row;
		ResetAnchor  anchor;
		bool (*is_default)(const Settings* t_settings);
		void (*restore)(SettingsPanel* t_panel);
	};

	static const EnumArray<SettingsTab, std::string_view> K_TAB_NAMES;
	static const EnumArray<SettingsGroup, GroupSpec>      K_GROUPS;
	static const RowSpec                                  K_ROW_SPECS[];
	static const EnumArray<SliderKind, SliderSpec>        K_SLIDERS;
	static const EnumArray<ChoiceKind, ChoiceSpec>        K_CHOICES;

	static const EnumArray<ResettableSetting, ResetSpec> K_RESETS;

	template <auto MEMBER>
	static auto restore_default(SettingsPanel* t_panel) -> void;

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

	[[nodiscard]] static auto slider_on(Rect Rows::* t_row) -> std::optional<SliderKind>;
	[[nodiscard]] auto slider_offered(SliderKind t_slider) const -> bool;
	[[nodiscard]] auto inline_slider_left(Rect t_row) const -> float;
	[[nodiscard]] auto slider_rect(const Rows& t_rows, SliderKind t_slider) const -> Rect;
	[[nodiscard]] auto slider_hit_rect(const Rows& t_rows, SliderKind t_slider) const -> Rect;
	[[nodiscard]] auto slider_value(SliderKind t_slider) const -> float;
	[[nodiscard]] auto slider_fraction(SliderKind t_slider) const -> float;
	[[nodiscard]] auto slider_readout(SliderKind t_slider, char (&t_buffer)[16]) const -> std::string_view;
	[[nodiscard]] auto slider_at(const Layout& t_layout, const Rows& t_rows, Vec2 t_point) const -> std::optional<SliderKind>;
	auto apply_slider(SliderKind t_slider, float t_fraction) -> void;

	[[nodiscard]] static auto choice_on(Rect Rows::* t_row) -> std::optional<ChoiceKind>;
	[[nodiscard]] auto choice_rect(const Rows& t_rows, ChoiceKind t_choice) const -> Rect;

	[[nodiscard]] auto pattern_tile_size() const -> Vec2;
	[[nodiscard]] auto pattern_popup_rect(const Rows& t_rows) const -> Rect;
	[[nodiscard]] auto pattern_tile(Rect t_popup, BackgroundStyle t_style) const -> Rect;
	[[nodiscard]] auto pattern_at(Rect t_popup, Vec2 t_point) const -> std::optional<BackgroundStyle>;

	[[nodiscard]] auto segment_choice_rect(Rect t_row, std::span<const std::string_view> t_labels) const -> Rect;
	[[nodiscard]] auto choice_segment(Rect t_choice, std::span<const std::string_view> t_labels, u32 t_index) const -> Rect;
	[[nodiscard]] auto search_rect(const Layout& t_layout) const -> Rect;
	auto refresh_search() -> void;
	auto clear_search() -> void;
	auto focus_search() -> void;

	[[nodiscard]] auto reset_control(const Rows& t_rows, ResettableSetting t_setting) const -> Rect;
	[[nodiscard]] auto reset_button(const Rows& t_rows, ResettableSetting t_setting) const -> Rect;
	[[nodiscard]] auto reset_slider(ResettableSetting t_setting) const -> std::optional<SliderKind>;
	[[nodiscard]] auto shows_reset_on_hover(ResettableSetting t_setting) const -> bool;
	[[nodiscard]] auto can_reset(ResettableSetting t_setting) const -> bool;
	auto settle_resets() -> void;
	auto reset_to_default(ResettableSetting t_setting) -> void;

	auto load_fonts(std::string_view t_file) -> bool;
	auto start_font_scan() -> void;
	auto take_font_scan(bool t_wait) -> void;
	auto open_font_list() -> void;
	auto open_theme_list() -> void;
	auto choose_font(u32 t_index) -> void;
	auto restore_font() -> void;
	auto restore_font_size(float Settings::* t_size) -> void;
	auto refresh_font_label() -> void;
	auto show_theme(ThemeChoice t_choice) -> void;
	auto select_theme(ThemeKind t_theme) -> void;
	auto choose_theme(ThemeKind t_theme) -> void;
	auto update_theme_preview() -> void;
	auto choose_background(BackgroundStyle t_style) -> void;
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
	auto draw_choice(DrawList* t_draw_list, const Layout& t_layout, const Rows& t_rows, ChoiceKind t_choice, u8 t_alpha) const -> void;
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

	TextInput m_search;
	char      m_applied_query[K_TEXT_INPUT_CAPACITY]{};

	EnumArray<SliderKind, Draggable>  m_slider_drags{};
	EnumArray<SliderKind, float>      m_slider_shown{};
	EnumArray<SliderKind, float>      m_slider_reveal{};
	EnumArray<SliderKind, float>      m_slider_hover{};
	EnumArray<ChoiceKind, float>      m_choice_shown{};
	EnumArray<BackgroundStyle, float> m_pattern_ring{};
	float                             m_toggles_shown[K_TOGGLE_COUNT]{};
	bool                              m_pattern_open        = false;
	float                             m_pattern_open_amount = 0.0f;

	float m_font_size_shown           = 0.0f;
	float m_secondary_font_size_shown = 0.0f;
	float m_accent_shown[3]{};

	EnumArray<ResettableSetting, float> m_reset_visible{};
	EnumArray<ResettableSetting, float> m_reset_spin{};
};
