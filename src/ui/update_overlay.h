#pragma once

#include <string_view>

#include "core/updater.h"
#include "ui/scrollable.h"
#include "ui/widget.h"

class Fonts;
class Window;
struct Settings;

class UpdateOverlay : public Widget {
  public:
	UpdateOverlay(Updater &t_updater, const Settings &t_settings, const Fonts &t_fonts, const Window &t_window);

	void open();
	void close();
	void show_release_notes(std::string_view t_version, std::string_view t_notes);

	bool is_open() const
	{
		return m_open;
	}

	void update(float t_delta_seconds) override;
	void draw(DrawList &t_draw_list) override;

	bool on_pointer_down(Vec2 t_point) override;
	bool on_pointer_move(Vec2 t_point) override;
	bool on_pointer_up(Vec2 t_point) override;
	bool on_scroll(Vec2 t_point, float t_wheel_delta) override;

	bool is_blocking() const override
	{
		return m_open;
	}

	CursorKind cursor() const override;

  private:
	struct Notes {
		Rect box;
		ScrollGeometry scroll;
	};

	Rect card_rect() const;
	Rect close_button_rect() const;
	Rect primary_button_rect() const;
	Notes notes() const;
	float notes_chrome_height() const;
	float notes_content_height() const;
	std::string_view shown_notes() const;
	bool has_notes() const;
	float text_column_x() const;
	float text_column_width() const;

	void draw_close_button(DrawList &t_draw_list) const;
	void draw_title(DrawList &t_draw_list, float &t_baseline, std::string_view t_title) const;
	void draw_detail(DrawList &t_draw_list, float t_baseline, std::string_view t_text) const;
	void draw_primary_button(DrawList &t_draw_list, std::string_view t_label, bool t_accented) const;
	void draw_notes(DrawList &t_draw_list) const;
	void draw_progress(DrawList &t_draw_list, float t_baseline, UpdateStage t_stage) const;

	Updater &m_updater;
	const Settings &m_settings;
	const Fonts &m_fonts;
	const Window &m_window;

	bool m_open = false;
	Scrollable m_notes_scroll;

	bool m_showing_release = false;
	char m_release_version[32]{};
	char m_release_notes[1024]{};
};
