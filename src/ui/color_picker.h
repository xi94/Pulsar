#pragma once

#include <optional>
#include <string_view>

#include "core/types.h"
#include "ui/draggable.h"
#include "ui/text_input.h"

class Assets;
class DrawList;
struct Fonts;

struct ColorPickerHint {
	std::string_view text;
	Rect anchor;
};

class ColorPicker {
  public:
	ColorPicker(const Fonts &t_fonts, const Assets &t_assets);

	void open(Color t_initial, Rect t_anchor, Rect t_bounds);
	void close();

	bool is_open() const
	{
		return m_open;
	}

	bool is_dragging() const
	{
		return m_saturation_value_drag.is_pressed() || m_hue_drag.is_pressed() || m_alpha_drag.is_pressed();
	}

	bool contains(Vec2 t_point) const;

	Color color() const
	{
		return m_color;
	}

	bool take_changed();

	void update(float t_delta_seconds);

	bool on_pointer_down(Vec2 t_point);
	void on_pointer_move(Vec2 t_point);
	bool on_pointer_up(Vec2 t_point);
	TextInput *on_right_click(Vec2 t_point);
	bool on_key_down(u32 t_key);
	bool on_char(u32 t_character);

	std::optional<ColorPickerHint> hint(Vec2 t_mouse) const;
	CursorKind cursor(Vec2 t_mouse) const;
	void draw(DrawList &t_draw_list, Vec2 t_mouse);

  private:
	static constexpr u32 field_count = 4;
	static constexpr u32 hex_field = 0;
	static constexpr u32 channel_count = 3;

	enum class Press : u8 {
		None,
		Revert,
		Copy,
		Paste,
		Field,
	};

	struct Layout {
		Rect popup;
		Rect square;
		Rect hue;
		Rect alpha;
		Rect swatch;
		Rect fields[field_count];
		Rect copy;
		Rect paste;
	};

	Layout layout() const;
	Rect field_text_rect(const Layout &t_layout, u32 t_field) const;
	i32 field_at(const Layout &t_layout, Vec2 t_point) const;
	i32 focused_field() const;

	void set_color(Color t_color);
	void apply_hsv();
	void sync_fields(i32 t_skipped_field);
	void read_field(u32 t_field);
	void focus_field(i32 t_field);
	void copy_hex();
	void paste_color();
	void end_drags();

	const Fonts &m_fonts;
	const Assets &m_assets;

	bool m_open = false;
	Rect m_anchor{};
	Rect m_bounds{};

	Color m_initial{};
	Color m_color{};
	bool m_changed = false;

	float m_hue = 0.0f;
	float m_saturation = 0.0f;
	float m_value = 0.0f;

	Draggable m_saturation_value_drag;
	Draggable m_hue_drag;
	Draggable m_alpha_drag;

	TextInput m_fields[field_count];
	char m_synced_text[field_count][text_input_capacity]{};

	Press m_press = Press::None;
	float m_copied_seconds = 0.0f;
	float m_paste_failed_seconds = 0.0f;
};
