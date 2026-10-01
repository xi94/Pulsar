#pragma once

#include "platform/window.h"
#include "ui/commands.h"
#include "ui/widget.h"

class Assets;
class Fonts;
class UpdateOverlay;
class Updater;

class TitleBar : public Widget {
  public:
	TitleBar(Window &t_window, const Updater &t_updater, const UpdateOverlay &t_update_overlay, const Fonts &t_fonts,
			 const Assets &t_assets, CommandQueue &t_commands);

	void update(float t_delta_seconds) override;
	void draw(DrawList &t_draw_list) override;

	bool on_pointer_down(Vec2 t_point) override;
	bool on_pointer_up(Vec2 t_point) override;

	CursorKind cursor() const override;

  private:
	void draw_hover(DrawList &t_draw_list, TitleBarButton t_button, TitleBarButton t_hovered) const;
	void draw_search_pill(DrawList &t_draw_list, TitleBarButton t_hovered) const;
	void draw_identity(DrawList &t_draw_list, float t_amount) const;
	void draw_update_status(DrawList &t_draw_list, float t_amount) const;
	void draw_maximize_glyph(DrawList &t_draw_list, Color t_color) const;

	Window &m_window;
	const Updater &m_updater;
	const UpdateOverlay &m_update_overlay;
	const Fonts &m_fonts;
	const Assets &m_assets;
	CommandQueue &m_commands;

	float m_pill_width = 0.0f;
	float m_update_reveal = 0.0f;
	float m_status_emphasis = 0.0f;
	float m_pill_spin = 0.0f;
};
