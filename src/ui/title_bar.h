#pragma once

#include "os/window.h"
#include "ui/commands.h"
#include "ui/widget.h"
#include "ui/window_layout.h"

class Assets;
struct Fonts;
class UpdateOverlay;
class Updater;

class TitleBar : public Widget {
  public:
	TitleBar(os::Window*          t_window,
	         const Updater*       t_updater,
	         const UpdateOverlay* t_update_overlay,
	         const Fonts*         t_fonts,
	         const Assets*        t_assets,
	         CommandQueue*        t_commands);

	auto update(float t_delta_seconds) -> void override;
	auto draw(DrawList* t_draw_list) -> void override;

	auto on_pointer_down(Vec2 t_point) -> bool override;
	auto on_pointer_up(Vec2 t_point) -> bool override;

	[[nodiscard]] auto cursor() const -> CursorKind override;

	[[nodiscard]] auto layout() const -> TitleBarLayout;

	auto set_search_visible(bool t_visible) -> void
	{
		m_search_visible = t_visible;
	}

  private:
	auto draw_hover(DrawList* t_draw_list, TitleBarButton t_button, TitleBarButton t_hovered) const -> void;
	auto draw_search_pill(DrawList* t_draw_list, TitleBarButton t_hovered) const -> void;
	auto draw_identity(DrawList* t_draw_list, float t_amount) const -> void;
	auto draw_update_status(DrawList* t_draw_list, float t_amount) const -> void;
	auto draw_maximize_glyph(DrawList* t_draw_list, Color t_color) const -> void;

	os::Window*          m_window;
	const Updater*       m_updater;
	const UpdateOverlay* m_update_overlay;
	const Fonts*         m_fonts;
	const Assets*        m_assets;
	CommandQueue*        m_commands;

	bool  m_update_visible  = false;
	bool  m_search_visible  = false;
	float m_status_width    = 0.0f;
	float m_update_reveal   = 0.0f;
	float m_status_emphasis = 0.0f;
	float m_status_spin     = 0.0f;
};
