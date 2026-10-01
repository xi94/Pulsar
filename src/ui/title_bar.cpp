#include "ui/title_bar.h"

#include <cmath>
#include <cstdio>
#include <numbers>

#include "core/animation.h"
#include "core/app_identity.h"
#include "core/updater.h"
#include "gfx/assets.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "ui/controls.h"
#include "ui/text.h"
#include "ui/theme.h"
#include "ui/update_overlay.h"

namespace {
constexpr Color color_close_hover{232, 17, 35, 255};
constexpr Color color_close_glyph_hover{255, 255, 255, 255};
constexpr u8 hover_alpha = 18;

constexpr float glyph_thickness = 1.5f;
constexpr float icon_size = 16.0f;
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

constexpr float pill_ease_rate = 16.0f;
constexpr float pill_spin_turns_per_second = 0.9f;
constexpr float status_reveal_rate = 14.0f;
constexpr float status_shift = 4.0f;
constexpr float status_padding_right = 10.0f;
constexpr float status_emphasis_rate = 16.0f;
constexpr float status_emphasis_strength = 0.75f;

struct PillLook {
	std::string_view label;
	std::string_view sizing_label;
	Asset icon = Asset::icon_update;
	Color icon_color{};
	Color text_color{};
	bool spinning = false;
	bool check = false;
};

PillLook pill_look(const Updater &t_updater, UpdateStage t_stage, bool t_release_notes, char (&t_percent)[8])
{
	const Theme &colors = theme();

	if (t_release_notes) {
		return PillLook{"What's new", "What's new", Asset::icon_update, colors.text_dim, colors.text};
	}

	switch (t_stage) {
		case UpdateStage::idle:
		case UpdateStage::checking:
			return PillLook{"Checking for updates", "Checking for updates", Asset::icon_update,
							colors.text_dim,		colors.text_dim,		true};

		case UpdateStage::up_to_date:
			return PillLook{"Up to date", "Up to date", Asset::icon_check, colors.text_dim, colors.text_dim,
							false,		  true};

		case UpdateStage::check_failed:
			return PillLook{"Couldn't check", "Couldn't check", Asset::icon_update, colors.error, colors.error};

		case UpdateStage::available:
		case UpdateStage::manual_upgrade_required:
			return PillLook{"Update available", "Update available", Asset::icon_download, colors.success, colors.text};

		case UpdateStage::downloading: {
			const u64 total = t_updater.total_bytes();
			const u64 percent = total > 0 ? t_updater.bytes_downloaded() * 100 / total : 0;
			const int written =
				std::snprintf(t_percent, sizeof(t_percent), "%u%%", static_cast<unsigned>(std::min<u64>(percent, 100)));

			return PillLook{std::string_view{t_percent, static_cast<usize>(std::max(written, 0))}, "100%",
							Asset::icon_download, colors.text_dim, colors.text};
		}

		case UpdateStage::verifying:
			return PillLook{"Verifying", "Verifying", Asset::icon_update, colors.text_dim, colors.text_dim, true};

		case UpdateStage::installing:
			return PillLook{"Installing", "Installing", Asset::icon_update, colors.text_dim, colors.text_dim, true};

		case UpdateStage::ready_to_relaunch:
			return PillLook{"Restarting", "Restarting", Asset::icon_update, colors.text_dim, colors.text_dim, true};

		case UpdateStage::error:
			return PillLook{"Update failed", "Update failed", Asset::icon_update, colors.error, colors.error};

		case UpdateStage::cancelled:
			return PillLook{"Update cancelled", "Update cancelled", Asset::icon_update, colors.text_dim,
							colors.text_dim};
	}

	return PillLook{};
}
}

TitleBar::TitleBar(Window &t_window, const Updater &t_updater, const UpdateOverlay &t_update_overlay,
				   const Fonts &t_fonts, const Assets &t_assets, CommandQueue &t_commands)
	: m_window(t_window)
	, m_updater(t_updater)
	, m_update_overlay(t_update_overlay)
	, m_fonts(t_fonts)
	, m_assets(t_assets)
	, m_commands(t_commands)
{
}

void TitleBar::update(float t_delta_seconds)
{
	const bool visible = is_update_worth_showing(m_updater.stage()) || m_update_overlay.is_open() ||
						 m_update_overlay.is_shown() || m_update_overlay.wants_status();
	m_window.set_update_button_visible(visible);
	m_update_reveal =
		animation::ease_toward(m_update_reveal, visible ? 1.0f : 0.0f, status_reveal_rate, t_delta_seconds);

	const bool emphasized = visible && (m_update_overlay.is_open() ||
										m_window.title_bar_button_rect(TitleBarButton::update).contains(m_mouse));
	m_status_emphasis =
		animation::ease_toward(m_status_emphasis, emphasized ? 1.0f : 0.0f, status_emphasis_rate, t_delta_seconds);

	if (!visible && m_update_reveal <= 0.0f) return;

	char percent[8];
	const PillLook look =
		pill_look(m_updater, m_update_overlay.shown_stage(), m_update_overlay.is_showing_release_notes(), percent);
	const float target = identity_gap * 0.5f + icon_size + pill_icon_gap +
						 std::ceil(text_width(m_fonts.secondary(), look.sizing_label)) + status_padding_right;

	m_pill_width = m_pill_width <= 0.0f ? target
										: animation::ease_toward(m_pill_width, target, pill_ease_rate, t_delta_seconds,
																 animation::settled_pixels);
	m_window.set_update_button_width(m_pill_width);

	if (look.spinning) {
		constexpr float full_turn = std::numbers::pi_v<float> * 2.0f;

		m_pill_spin = std::fmod(m_pill_spin + t_delta_seconds * pill_spin_turns_per_second * full_turn, full_turn);
		animation::request_frame();
	}
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

void TitleBar::draw_identity(DrawList &t_draw_list, float t_amount) const
{
	const Rect menu = m_window.title_bar_button_rect(TitleBarButton::menu);
	const float shift = -status_shift * (1.0f - t_amount);
	const Rect mark{menu.right() + identity_gap * 0.5f, (title_bar_height - identity_mark_size) * 0.5f + shift,
					identity_mark_size, identity_mark_size};
	const Font &font = m_fonts.secondary();
	const auto alpha = static_cast<u8>(255.0f * t_amount);

	controls::draw_icon(t_draw_list, mark, m_assets.get(Asset::icon_app), faded(theme().text_dim, alpha));
	draw_text(t_draw_list, font, Vec2{mark.right() + identity_gap, font.centered_baseline(menu) + shift}, app_name,
			  faded(theme().text_dim, alpha));
}

void TitleBar::draw_update_status(DrawList &t_draw_list, float t_amount) const
{
	char percent[8];
	const PillLook look =
		pill_look(m_updater, m_update_overlay.shown_stage(), m_update_overlay.is_showing_release_notes(), percent);
	const Rect area = m_window.title_bar_button_rect(TitleBarButton::update);
	const auto alpha = static_cast<u8>(255.0f * t_amount);
	const float shift = status_shift * (1.0f - t_amount);
	t_draw_list.push_clip(area);

	const Font &font = m_fonts.secondary();
	const Rect icon{area.x + identity_gap * 0.5f, (title_bar_height - icon_size) * 0.5f + shift, icon_size, icon_size};
	const Color icon_color = faded(look.icon_color, alpha);

	if (look.spinning) {
		t_draw_list.add_rotated_image(icon, m_pill_spin, m_assets.get(look.icon), icon_color);
	} else if (look.check) {
		controls::draw_check(t_draw_list, m_assets, icon, icon_color);
	} else {
		controls::draw_icon(t_draw_list, icon, m_assets.get(look.icon), icon_color);
	}

	draw_text(t_draw_list, font, Vec2{icon.right() + pill_icon_gap, font.centered_baseline(area) + shift}, look.label,
			  faded(mix(look.text_color, theme().text, status_emphasis_strength * m_status_emphasis), alpha));

	t_draw_list.pop_clip();
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

	if (m_update_reveal < 0.999f) {
		draw_identity(t_draw_list, 1.0f - m_update_reveal);
	}

	if (m_update_reveal > 0.001f) {
		draw_update_status(t_draw_list, m_update_reveal);
	}

	draw_search_pill(t_draw_list, hovered);

	draw_hover(t_draw_list, TitleBarButton::minimize, hovered);
	controls::draw_icon(t_draw_list, icon_rect(TitleBarButton::minimize), m_assets.get(Asset::icon_minimize),
						glyph_color(TitleBarButton::minimize));

	draw_hover(t_draw_list, TitleBarButton::maximize, hovered);
	draw_maximize_glyph(t_draw_list, glyph_color(TitleBarButton::maximize));

	draw_hover(t_draw_list, TitleBarButton::close, hovered);
	controls::draw_icon(t_draw_list, icon_rect(TitleBarButton::close), m_assets.get(Asset::icon_close),
						hovered == TitleBarButton::close ? color_close_glyph_hover : theme().text);
}
