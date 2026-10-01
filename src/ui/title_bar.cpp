#include "ui/title_bar.h"

#include "core/app_identity.h"
#include "core/updater.h"
#include "gfx/assets.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "ui/controls.h"
#include "ui/text.h"
#include "ui/theme.h"

namespace {
constexpr Color color_close_hover{232, 17, 35, 255};
constexpr Color color_close_glyph_hover{255, 255, 255, 255};
constexpr u8 hover_alpha = 18;
constexpr float pill_tint = 0.2f;

constexpr float glyph_thickness = 1.5f;
constexpr float icon_size = 16.0f;
constexpr float pill_inset_y = 7.0f;
constexpr float pill_inset_x = 4.0f;
constexpr float pill_padding = 10.0f;
constexpr float pill_icon_gap = 8.0f;
constexpr float identity_mark_size = 16.0f;
constexpr float identity_gap = 8.0f;
constexpr float search_pill_height = 26.0f;
constexpr float search_pill_radius = 7.0f;
constexpr float search_pill_padding = 10.0f;
constexpr float search_pill_icon_size = 13.0f;
constexpr float search_pill_icon_gap = 8.0f;
constexpr std::string_view search_pill_text = "Search all accounts";
constexpr std::string_view search_pill_shortcut = "Ctrl+F";

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

Color update_color(UpdateStage t_stage)
{
	if (is_update_failure(t_stage)) return theme().error;
	if (is_update_waiting(t_stage)) return theme().success;

	return theme().text_dim;
}

std::string_view update_label(UpdateStage t_stage)
{
	if (is_update_waiting(t_stage)) return "Update available";
	if (is_update_failure(t_stage)) return "Update failed";

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

		case TitleBarButton::search:
			m_commands.push(Command{.type = CommandType::open_account_search});
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
						 t_button == TitleBarButton::close ? color_close_hover : with_alpha(theme().text, hover_alpha));
}

void TitleBar::draw_update_pill(DrawList &t_draw_list) const
{
	const UpdateStage stage = m_updater.stage();
	if (!is_update_worth_showing(stage)) return;

	const bool in_progress = !is_update_failure(stage) && !is_update_waiting(stage);
	const Color foreground = update_color(stage);
	const Color background =
		in_progress ? with_alpha(theme().text, hover_alpha) : mix(theme().chrome, foreground, pill_tint);

	const Rect pill = m_window.title_bar_button_rect(TitleBarButton::update).inset(pill_inset_x, pill_inset_y);
	t_draw_list.add_rounded_rect(pill, rounded(pill.h * 0.5f), background);

	const Rect icon{pill.x + pill_padding, pill.y + (pill.h - icon_size) * 0.5f, icon_size, icon_size};
	controls::draw_icon(t_draw_list, icon, m_assets.get(Asset::icon_update), foreground);

	const Font &font = m_fonts.secondary();
	draw_text(t_draw_list, font, Vec2{icon.right() + pill_icon_gap, font.centered_baseline(pill)}, update_label(stage),
			  foreground);
}

void TitleBar::draw_search_pill(DrawList &t_draw_list, TitleBarButton t_hovered) const
{
	if (!m_window.is_search_button_visible()) return;

	const Rect area = m_window.title_bar_button_rect(TitleBarButton::search);
	if (area.w <= 0.0f) return;

	const Theme &colors = theme();
	const bool is_hovered = t_hovered == TitleBarButton::search;
	const Rect pill = area.inset(0.0f, (area.h - search_pill_height) * 0.5f);
	const Color fill = is_hovered ? hovered(colors.field) : colors.field;
	const Font &font = m_fonts.secondary();
	const Font &key_font = m_fonts.caption();

	t_draw_list.add_bordered_rect(pill, rounded(search_pill_radius), fill,
								  is_hovered ? colors.border : colors.separator, 1.0f);

	const Rect icon{pill.x + search_pill_padding, pill.center().y - search_pill_icon_size * 0.5f, search_pill_icon_size,
					search_pill_icon_size};
	const float keys_right = pill.right() - std::floor((pill.h - controls::keycap_height(key_font)) * 0.5f);
	const float keys_left = keys_right - controls::shortcut_width(key_font, search_pill_shortcut);
	const float text_left = icon.right() + search_pill_icon_gap;
	const float room = std::max(0.0f, keys_left - search_pill_icon_gap - text_left);
	const float label_width = std::min(text_width(font, search_pill_text), room);

	controls::draw_magnifier(t_draw_list, icon, colors.text_faint);
	draw_text_truncated(t_draw_list, font,
						Vec2{snapped_to_pixel(text_left + (room - label_width) * 0.5f), font.centered_baseline(pill)},
						search_pill_text, room, is_hovered ? colors.text_dim : colors.text_faint);
	controls::draw_shortcut(t_draw_list, key_font, Vec2{keys_right, pill.center().y}, search_pill_shortcut, fill, 255);
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
	t_draw_list.add_rect(Rect{0.0f, 0.0f, static_cast<float>(m_window.width()), title_bar_height}, theme().chrome);

	const TitleBarButton hovered = m_window.title_bar_button_at(m_mouse);
	const auto icon_rect = [this](TitleBarButton t_button) {
		return m_window.title_bar_button_rect(t_button).centered(icon_size, icon_size);
	};
	const auto glyph_color = [hovered](TitleBarButton t_button) {
		return hovered == t_button ? theme().text : theme().text_dim;
	};

	draw_hover(t_draw_list, TitleBarButton::menu, hovered);
	controls::draw_icon(t_draw_list, icon_rect(TitleBarButton::menu), m_assets.get(Asset::icon_menu), theme().text);

	const Rect menu = m_window.title_bar_button_rect(TitleBarButton::menu);
	const Rect mark{menu.right() + identity_gap * 0.5f, (title_bar_height - identity_mark_size) * 0.5f,
					identity_mark_size, identity_mark_size};
	const Font &font = m_fonts.secondary();

	controls::draw_icon(t_draw_list, mark, m_assets.get(Asset::icon_app), theme().text_dim);
	draw_text(t_draw_list, font, Vec2{mark.right() + identity_gap, font.centered_baseline(menu)}, app_name,
			  theme().text_dim);

	draw_search_pill(t_draw_list, hovered);
	draw_update_pill(t_draw_list);

	draw_hover(t_draw_list, TitleBarButton::minimize, hovered);
	controls::draw_icon(t_draw_list, icon_rect(TitleBarButton::minimize), m_assets.get(Asset::icon_minimize),
						glyph_color(TitleBarButton::minimize));

	draw_hover(t_draw_list, TitleBarButton::maximize, hovered);
	draw_maximize_glyph(t_draw_list, glyph_color(TitleBarButton::maximize));

	draw_hover(t_draw_list, TitleBarButton::close, hovered);
	controls::draw_icon(t_draw_list, icon_rect(TitleBarButton::close), m_assets.get(Asset::icon_close),
						hovered == TitleBarButton::close ? color_close_glyph_hover : theme().text);
}
