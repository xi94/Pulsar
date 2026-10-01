#pragma once

#include <string_view>

#include "core/updater.h"
#include "ui/controls.h"
#include "ui/scrollable.h"
#include "ui/widget.h"

class Assets;
class Fonts;
class Window;
struct Settings;

class UpdateOverlay : public Widget {
  public:
	UpdateOverlay(Updater &t_updater, const Settings &t_settings, const Fonts &t_fonts, const Assets &t_assets,
				  const Window &t_window);

	void open();
	void begin_check();
	void close();
	void show_release_notes(std::string_view t_version, std::string_view t_notes);

	bool is_open() const
	{
		return m_open;
	}

	bool is_shown() const
	{
		return m_open_amount > 0.01f;
	}

	bool wants_status() const
	{
		return m_check_requested || m_status_linger > 0.0f;
	}

	UpdateStage shown_stage() const
	{
		return m_shown_stage;
	}

	bool is_showing_release_notes() const
	{
		return m_showing_release;
	}

	void update(float t_delta_seconds) override;
	void draw(DrawList &t_draw_list) override;

	bool on_pointer_down(Vec2 t_point) override;
	bool on_pointer_move(Vec2 t_point) override;
	bool on_pointer_up(Vec2 t_point) override;
	bool on_scroll(Vec2 t_point, float t_wheel_delta) override;
	bool on_key_down(u32 t_key) override;

	bool is_blocking() const override
	{
		return is_shown();
	}

	CursorKind cursor() const override;

  private:
	enum class Action : u8 {
		none,
		check,
		download,
		cancel,
		close,
		releases,
	};

	struct Button {
		std::string_view label;
		Action action = Action::none;
		controls::ButtonStyle style = controls::ButtonStyle::neutral;
	};

	struct Content {
		char title[64]{};
		char detail[192]{};
		bool spinning = false;
		bool detail_is_error = false;
		bool progress = false;
		bool notes = false;
		Button primary;
		Button secondary;
	};

	struct Layout {
		Rect popover;
		float title_top;
		float detail_top;
		Rect progress;
		float caption_top;
		Rect notes;
		Rect primary;
		Rect secondary;
	};

	Content describe() const;
	u32 content_key() const;
	float content_height(const Content &t_content) const;
	float notes_box_height() const;
	float notes_content_height() const;
	std::string_view shown_notes() const;
	Rect anchor_rect() const;
	Layout layout(const Content &t_content) const;
	ScrollGeometry notes_scroll(const Layout &t_layout) const;
	void run(Action t_action);
	void advance_shown_stage(float t_delta_seconds);

	void draw_title(DrawList &t_draw_list, const Content &t_content, Rect t_line, u8 t_alpha) const;
	void draw_notes(DrawList &t_draw_list, Rect t_box, const ScrollGeometry &t_scroll, u8 t_alpha) const;
	void draw_progress(DrawList &t_draw_list, const Layout &t_layout, u8 t_alpha) const;

	Updater &m_updater;
	const Settings &m_settings;
	const Fonts &m_fonts;
	const Assets &m_assets;
	const Window &m_window;

	bool m_open = false;
	float m_open_amount = 0.0f;
	float m_height = 0.0f;
	float m_content_fade = 1.0f;
	float m_progress = 0.0f;
	float m_spin = 0.0f;
	u32 m_content_key = 0;
	UpdateStage m_shown_stage = UpdateStage::idle;
	float m_shown_seconds = 0.0f;
	bool m_check_requested = false;
	float m_status_linger = 0.0f;
	bool m_pressed_outside = false;
	Scrollable m_notes_scroll;

	bool m_showing_release = false;
	char m_release_version[32]{};
	char m_release_notes[1024]{};
};
