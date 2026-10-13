#pragma once

#include <optional>
#include <string_view>

#include "core/enum_array.h"
#include "core/types.h"
#include "os/input.h"
#include "ui/draggable.h"
#include "ui/text_input.h"

class Assets;
class DrawList;
struct Fonts;

struct ColorPickerHint {
	std::string_view text;
	Rect             anchor;
};

class ColorPicker {
  public:
	ColorPicker(const Fonts* t_fonts, const Assets* t_assets);

	auto open(Color t_initial, Rect t_anchor, Rect t_bounds) -> void;
	auto close() -> void;

	[[nodiscard]] auto is_open() const -> bool
	{
		return m_open;
	}

	[[nodiscard]] auto is_dragging() const -> bool
	{
		return m_saturation_value_drag.is_pressed() || m_hue_drag.is_pressed() || m_alpha_drag.is_pressed();
	}

	[[nodiscard]] auto contains(Vec2 t_point) const -> bool;

	[[nodiscard]] auto color() const -> Color
	{
		return m_color;
	}

	[[nodiscard]] auto take_changed() -> bool;

	auto update(float t_delta_seconds) -> void;

	auto on_pointer_down(Vec2 t_point) -> bool;
	auto on_pointer_move(Vec2 t_point) -> void;
	auto on_pointer_up(Vec2 t_point) -> bool;
	auto on_right_click(Vec2 t_point) -> TextInput*;
	auto on_key_down(os::Key t_key) -> bool;
	auto on_char(u32 t_character) -> bool;

	[[nodiscard]] auto hint(Vec2 t_mouse) const -> std::optional<ColorPickerHint>;
	[[nodiscard]] auto cursor(Vec2 t_mouse) const -> CursorKind;
	auto draw(DrawList* t_draw_list, Vec2 t_mouse) -> void;

  private:
	// The hex code, then red, green and blue.
	enum class ColorField : u8 {
		HEX,
		RED,
		GREEN,
		BLUE,
		COUNT,
	};

	static constexpr ColorField K_CHANNEL_FIELDS[]{ColorField::RED, ColorField::GREEN, ColorField::BLUE};

	static constexpr EnumArray<ColorField, std::string_view> K_FIELD_LABELS{{"#", "R", "G", "B"}};

	// The byte of the colour each channel field edits.
	static constexpr EnumArray<ColorField, u8 Color::*> K_CHANNELS{{nullptr, &Color::r, &Color::g, &Color::b}};

	enum class Press : u8 {
		NONE,
		REVERT,
		COPY,
		PASTE,
		FIELD,
	};

	struct Layout {
		Rect                        popup;
		Rect                        square;
		Rect                        hue;
		Rect                        alpha;
		Rect                        swatch;
		EnumArray<ColorField, Rect> fields;
		Rect                        copy;
		Rect                        paste;
	};

	[[nodiscard]] auto layout() const -> Layout;
	[[nodiscard]] auto field_text_rect(const Layout& t_layout, ColorField t_field) const -> Rect;
	[[nodiscard]] auto field_at(const Layout& t_layout, Vec2 t_point) const -> std::optional<ColorField>;
	[[nodiscard]] auto focused_field() const -> std::optional<ColorField>;

	auto set_color(Color t_color) -> void;
	auto apply_hsv() -> void;
	auto sync_fields(std::optional<ColorField> t_skipped_field = std::nullopt) -> void;
	auto read_field(ColorField t_field) -> void;
	auto focus_field(std::optional<ColorField> t_field) -> void;
	auto copy_hex() -> void;
	auto paste_color() -> void;
	auto end_drags() -> void;

	const Fonts*  m_fonts;
	const Assets* m_assets;

	bool m_open = false;
	Rect m_anchor{};
	Rect m_bounds{};

	Color m_initial{};
	Color m_color{};
	bool  m_changed = false;

	float m_hue        = 0.0f;
	float m_saturation = 0.0f;
	float m_value      = 0.0f;

	Draggable m_saturation_value_drag;
	Draggable m_hue_drag;
	Draggable m_alpha_drag;

	EnumArray<ColorField, TextInput>                   m_fields{};
	EnumArray<ColorField, char[K_TEXT_INPUT_CAPACITY]> m_synced_text{};

	Press m_press                = Press::NONE;
	float m_copied_seconds       = 0.0f;
	float m_paste_failed_seconds = 0.0f;
};
