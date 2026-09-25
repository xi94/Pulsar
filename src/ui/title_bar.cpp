#include "ui/title_bar.h"

#include "core/updater.h"
#include "gfx/assets.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "ui/controls.h"
#include "ui/text.h"

namespace {
constexpr Color color_glyph{214, 214, 218, 255};
constexpr Color color_glyph_dim{150, 150, 156, 255};
constexpr Color color_hover{255, 255, 255, 18};
constexpr Color color_close_hover{232, 17, 35, 255};
constexpr Color color_update_good_background{32, 58, 44, 255};
constexpr Color color_update_good{110, 220, 150, 255};
constexpr Color color_update_bad_background{58, 34, 34, 255};
constexpr Color color_update_bad{230, 120, 110, 255};

constexpr float glyph_thickness = 1.5f;
constexpr float icon_size = 16.0f;
constexpr float pill_inset_y = 7.0f;
constexpr float pill_inset_x = 4.0f;
constexpr float pill_padding = 10.0f;
constexpr float pill_icon_gap = 8.0f;

bool is_update_worth_showing(UpdateStage t_stage)
{
	switch (t_stage) {
		case UpdateStage::available:
		case UpdateStage::manual_upgrade_required:
		case UpdateStage::downloading:
		case UpdateStage::verifying:
		case UpdateStage::installing:
		case UpdateStage::ready_to_relaunch:
		case UpdateStage::error:
		case UpdateStage::cancelled:
			return true;
		default:
			return false;
	}
}

bool is_update_failure(UpdateStage t_stage)
{
	return t_stage == UpdateStage::error || t_stage == UpdateStage::cancelled;
}

bool is_update_waiting(UpdateStage t_stage)
{
	return t_stage == UpdateStage::available || t_stage == UpdateStage::manual_upgrade_required;
}

std::string_view update_label(UpdateStage t_stage)
{
	if (is_update_waiting(t_stage)) return "Update Available";
	if (is_update_failure(t_stage)) return "Update Failed";

	return "Updating...";
}
}

TitleBar::TitleBar(Window &t_window, const Updater &t_updater, const Fonts &t_fonts, const Assets &t_assets,
				   CommandQueue &t_commands)
	: m_window(t_window)
	, m_updater(t_updater)
	, m_fonts(t_fonts)
	, m_assets(t_assets)
	, m_commands(t_commands)
{
}

void TitleBar::update(float)
{
	m_window.set_update_button_visible(is_update_worth_showing(m_updater.stage()));
}

bool TitleBar::on_pointer_down(Vec2 t_point)
{
	return m_window.title_bar_button_at(t_point) != TitleBarButton::none;
}

bool TitleBar::on_pointer_up(Vec2 t_point)
{
	switch (m_window.title_bar_button_at(t_point)) {
		case TitleBarButton::none:
			return false;

		case TitleBarButton::menu:
			m_commands.push(Command{.type = CommandType::toggle_app_menu});
			break;

		case TitleBarButton::update:
			m_commands.push(Command{.type = CommandType::toggle_update_overlay});
			break;

		case TitleBarButton::minimize:
			ShowWindow(m_window.handle(), SW_MINIMIZE);
			break;

		case TitleBarButton::maximize:
			ShowWindow(m_window.handle(), m_window.is_maximized() ? SW_RESTORE : SW_MAXIMIZE);
			break;

		case TitleBarButton::close:
			PostMessageW(m_window.handle(), WM_CLOSE, 0, 0);
			break;
	}

	return true;
}

CursorKind TitleBar::cursor() const
{
	return m_window.title_bar_button_at(m_mouse) == TitleBarButton::none ? CursorKind::arrow : CursorKind::hand;
}

void TitleBar::draw_hover(DrawList &t_draw_list, TitleBarButton t_button, TitleBarButton t_hovered) const
{
	if (t_button != t_hovered) return;

	t_draw_list.add_rect(m_window.title_bar_button_rect(t_button),
						 t_button == TitleBarButton::close ? color_close_hover : color_hover);
}

void TitleBar::draw_update_pill(DrawList &t_draw_list) const
{
	const UpdateStage stage = m_updater.stage();
	if (!is_update_worth_showing(stage)) return;

	const bool failed = is_update_failure(stage);
	const bool waiting = is_update_waiting(stage);
	const Color background =
		failed ? color_update_bad_background : (waiting ? color_update_good_background : color_hover);
	const Color foreground = failed ? color_update_bad : (waiting ? color_update_good : color_glyph_dim);

	const Rect pill = m_window.title_bar_button_rect(TitleBarButton::update).inset(pill_inset_x, pill_inset_y);
	t_draw_list.add_rounded_rect(pill, rounded(pill.h * 0.5f), background);

	const Rect icon{pill.x + pill_padding, pill.y + (pill.h - icon_size) * 0.5f, icon_size, icon_size};
	controls::draw_icon(t_draw_list, icon, m_assets.get(Asset::icon_update), foreground);

	const Font &font = m_fonts.secondary();
	draw_text(t_draw_list, font, Vec2{icon.right() + pill_icon_gap, font.centered_baseline(pill)}, update_label(stage),
			  foreground);
}

void TitleBar::draw_maximize_glyph(DrawList &t_draw_list, Color t_color) const
{
	const Vec2 center = m_window.title_bar_button_rect(TitleBarButton::maximize).center();

	if (!m_window.is_maximized()) {
		constexpr float size = 10.0f;
		t_draw_list.add_rect_outline(Rect{center.x - size * 0.5f, center.y - size * 0.5f, size, size}, glyph_thickness,
									 t_color);
		return;
	}

	constexpr float size = 8.0f;
	constexpr float offset = 3.0f;
	t_draw_list.add_rect_outline(Rect{center.x - size * 0.5f + offset, center.y - size * 0.5f - offset, size, size},
								 glyph_thickness, t_color);
	t_draw_list.add_rect_outline(Rect{center.x - size * 0.5f - offset, center.y - size * 0.5f + offset, size, size},
								 glyph_thickness, t_color);
}

void TitleBar::draw(DrawList &t_draw_list)
{
	t_draw_list.add_rect(Rect{0.0f, 0.0f, static_cast<float>(m_window.width()), title_bar_height}, title_bar_color);

	const TitleBarButton hovered = m_window.title_bar_button_at(m_mouse);
	const auto icon_rect = [this](TitleBarButton t_button) {
		return m_window.title_bar_button_rect(t_button).centered(icon_size, icon_size);
	};
	const auto glyph_color = [hovered](TitleBarButton t_button) {
		return hovered == t_button ? color_glyph : color_glyph_dim;
	};

	draw_hover(t_draw_list, TitleBarButton::menu, hovered);
	controls::draw_icon(t_draw_list, icon_rect(TitleBarButton::menu), m_assets.get(Asset::icon_menu), color_glyph);

	draw_update_pill(t_draw_list);

	draw_hover(t_draw_list, TitleBarButton::minimize, hovered);
	controls::draw_icon(t_draw_list, icon_rect(TitleBarButton::minimize), m_assets.get(Asset::icon_minimize),
						glyph_color(TitleBarButton::minimize));

	draw_hover(t_draw_list, TitleBarButton::maximize, hovered);
	draw_maximize_glyph(t_draw_list, glyph_color(TitleBarButton::maximize));

	draw_hover(t_draw_list, TitleBarButton::close, hovered);
	controls::draw_icon(t_draw_list, icon_rect(TitleBarButton::close), m_assets.get(Asset::icon_close), color_glyph);
}
