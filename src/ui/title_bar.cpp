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
constexpr float glyph_thickness = 1.5f;
constexpr float icon_size = 16.0f;
constexpr float status_icon_gap = 8.0f;
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
		case UpdateStage::Available:
		case UpdateStage::ManualUpgradeRequired:
		case UpdateStage::Downloading:
		case UpdateStage::Verifying:
		case UpdateStage::Installing:
		case UpdateStage::ReadyToRelaunch:
		case UpdateStage::Error:
		case UpdateStage::Cancelled:
			return true;
		default:
			return false;
	}
}

constexpr float status_width_rate = 16.0f;
constexpr float spin_turns_per_second = 0.9f;
constexpr float status_reveal_rate = 14.0f;
constexpr float status_shift = 4.0f;
constexpr float status_padding_right = 10.0f;
constexpr float status_emphasis_rate = 16.0f;
constexpr float status_emphasis_strength = 0.75f;

struct StatusLook {
	std::string_view label;
	std::string_view sizing_label;
	Asset icon = Asset::IconUpdate;
	Color icon_color{};
	Color text_color{};
	bool spinning = false;
	bool check = false;
};

StatusLook status_look(const Updater &t_updater, UpdateStage t_stage, bool t_release_notes, char (&t_percent)[8])
{
	const Theme &colors = theme();

	if (t_release_notes) {
		return StatusLook{"What's new", "What's new", Asset::IconUpdate, colors.text_dim, colors.text};
	}

	switch (t_stage) {
		case UpdateStage::Idle:
		case UpdateStage::Checking:
			return StatusLook{"Checking for updates", "Checking for updates", Asset::IconUpdate,
							  colors.text_dim,		  colors.text_dim,		  true};

		case UpdateStage::UpToDate:
			return StatusLook{"Up to date", "Up to date", Asset::IconCheck, colors.text_dim, colors.text_dim,
							  false,		true};

		case UpdateStage::CheckFailed:
			return StatusLook{"Couldn't check", "Couldn't check", Asset::IconUpdate, colors.error, colors.error};

		case UpdateStage::Available:
		case UpdateStage::ManualUpgradeRequired:
			return StatusLook{"Update available", "Update available", Asset::IconDownload, colors.success, colors.text};

		case UpdateStage::Downloading: {
			const u64 total = t_updater.total_bytes();
			const u64 percent = total > 0 ? t_updater.bytes_downloaded() * 100 / total : 0;
			const int written =
				std::snprintf(t_percent, sizeof(t_percent), "%u%%", static_cast<unsigned>(std::min<u64>(percent, 100)));

			return StatusLook{std::string_view{t_percent, static_cast<usize>(std::max(written, 0))}, "100%",
							  Asset::IconDownload, colors.text_dim, colors.text};
		}

		case UpdateStage::Verifying:
			return StatusLook{"Verifying", "Verifying", Asset::IconUpdate, colors.text_dim, colors.text_dim, true};

		case UpdateStage::Installing:
			return StatusLook{"Installing", "Installing", Asset::IconUpdate, colors.text_dim, colors.text_dim, true};

		case UpdateStage::ReadyToRelaunch:
			return StatusLook{"Restarting", "Restarting", Asset::IconUpdate, colors.text_dim, colors.text_dim, true};

		case UpdateStage::Error:
			return StatusLook{"Update failed", "Update failed", Asset::IconUpdate, colors.error, colors.error};

		case UpdateStage::Cancelled:
			return StatusLook{"Update cancelled", "Update cancelled", Asset::IconUpdate, colors.text_dim,
							  colors.text_dim};
	}

	return StatusLook{};
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
										m_window.title_bar_button_rect(TitleBarButton::Update).contains(m_mouse));
	m_status_emphasis =
		animation::ease_toward(m_status_emphasis, emphasized ? 1.0f : 0.0f, status_emphasis_rate, t_delta_seconds);

	if (!visible && m_update_reveal <= 0.0f) return;

	char percent[8];
	const StatusLook look =
		status_look(m_updater, m_update_overlay.shown_stage(), m_update_overlay.is_showing_release_notes(), percent);
	const float target = identity_gap * 0.5f + icon_size + status_icon_gap +
						 std::ceil(text_width(m_fonts.secondary, look.sizing_label)) + status_padding_right;

	m_status_width = m_status_width <= 0.0f ? target
											: animation::ease_toward(m_status_width, target, status_width_rate,
																	 t_delta_seconds, animation::settled_pixels);
	m_window.set_update_button_width(m_status_width);

	if (look.spinning) {
		constexpr float full_turn = std::numbers::pi_v<float> * 2.0f;

		m_status_spin = std::fmod(m_status_spin + t_delta_seconds * spin_turns_per_second * full_turn, full_turn);
		animation::request_frame();
	}
}

bool TitleBar::on_pointer_down(Vec2 t_point)
{
	return m_window.title_bar_button_at(t_point) != TitleBarButton::None;
}

bool TitleBar::on_pointer_up(Vec2 t_point)
{
	switch (m_window.title_bar_button_at(t_point)) {
		case TitleBarButton::None:
			return false;

		case TitleBarButton::Menu:
			m_commands.push(Command{.type = CommandType::ToggleAppMenu});
			break;

		case TitleBarButton::Search:
			m_commands.push(Command{.type = CommandType::OpenAccountSearch});
			break;

		case TitleBarButton::Update:
			m_commands.push(Command{.type = CommandType::ToggleUpdateOverlay});
			break;

		case TitleBarButton::Minimize:
			ShowWindow(m_window.handle(), SW_MINIMIZE);
			break;

		case TitleBarButton::Maximize:
			ShowWindow(m_window.handle(), m_window.is_maximized() ? SW_RESTORE : SW_MAXIMIZE);
			break;

		case TitleBarButton::Close:
			PostMessageW(m_window.handle(), WM_CLOSE, 0, 0);
			break;
	}

	return true;
}

CursorKind TitleBar::cursor() const
{
	return m_window.title_bar_button_at(m_mouse) == TitleBarButton::None ? CursorKind::Arrow : CursorKind::Hand;
}

void TitleBar::draw_hover(DrawList &t_draw_list, TitleBarButton t_button, TitleBarButton t_hovered) const
{
	if (t_button != t_hovered) return;

	t_draw_list.add_rect(m_window.title_bar_button_rect(t_button),
						 t_button == TitleBarButton::Close ? title_bar_close_hover
														   : with_alpha(theme().text, title_bar_hover_alpha));
}

void TitleBar::draw_identity(DrawList &t_draw_list, float t_amount) const
{
	const Rect menu = m_window.title_bar_button_rect(TitleBarButton::Menu);
	const float shift = -status_shift * (1.0f - t_amount);
	const Rect mark{menu.right() + identity_gap * 0.5f, (title_bar_height - identity_mark_size) * 0.5f + shift,
					identity_mark_size, identity_mark_size};
	const Font &font = m_fonts.secondary;
	const auto alpha = to_alpha(t_amount);

	t_draw_list.add_image(mark, m_assets.get(Asset::IconApp), faded(theme().text_dim, alpha));
	draw_text(t_draw_list, font, Vec2{mark.right() + identity_gap, font.centered_baseline(menu) + shift}, app_name,
			  faded(theme().text_dim, alpha));
}

void TitleBar::draw_update_status(DrawList &t_draw_list, float t_amount) const
{
	char percent[8];
	const StatusLook look =
		status_look(m_updater, m_update_overlay.shown_stage(), m_update_overlay.is_showing_release_notes(), percent);
	const Rect area = m_window.title_bar_button_rect(TitleBarButton::Update);
	const auto alpha = to_alpha(t_amount);
	const float shift = status_shift * (1.0f - t_amount);
	t_draw_list.push_clip(area);

	const Font &font = m_fonts.secondary;
	const Rect icon{area.x + identity_gap * 0.5f, (title_bar_height - icon_size) * 0.5f + shift, icon_size, icon_size};
	const Color icon_color = faded(look.icon_color, alpha);

	if (look.spinning) {
		t_draw_list.add_rotated_image(icon, m_status_spin, m_assets.get(look.icon), icon_color);
	} else if (look.check) {
		controls::draw_check(t_draw_list, m_assets, icon, icon_color);
	} else {
		t_draw_list.add_image(icon, m_assets.get(look.icon), icon_color);
	}

	draw_text(t_draw_list, font, Vec2{icon.right() + status_icon_gap, font.centered_baseline(area) + shift}, look.label,
			  faded(mix(look.text_color, theme().text, status_emphasis_strength * m_status_emphasis), alpha));

	t_draw_list.pop_clip();
}

void TitleBar::draw_search_pill(DrawList &t_draw_list, TitleBarButton t_hovered) const
{
	if (!m_window.is_search_button_visible()) return;

	const Rect area = m_window.title_bar_button_rect(TitleBarButton::Search);
	if (area.w <= 0.0f) return;

	const Theme &colors = theme();
	const bool is_hovered = t_hovered == TitleBarButton::Search;
	const Rect pill = area.inset(0.0f, (area.h - search_pill_height) * 0.5f);
	const Color fill = is_hovered ? hovered(colors.field) : colors.field;
	const Font &font = m_fonts.secondary;
	const Font &key_font = m_fonts.caption;

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
	const Vec2 center = m_window.title_bar_button_rect(TitleBarButton::Maximize).center();

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

	draw_hover(t_draw_list, TitleBarButton::Menu, hovered);
	t_draw_list.add_image(icon_rect(TitleBarButton::Menu), m_assets.get(Asset::IconMenu), theme().text);

	if (m_update_reveal < 0.999f) {
		draw_identity(t_draw_list, 1.0f - m_update_reveal);
	}

	if (m_update_reveal > 0.001f) {
		draw_update_status(t_draw_list, m_update_reveal);
	}

	draw_search_pill(t_draw_list, hovered);

	draw_hover(t_draw_list, TitleBarButton::Minimize, hovered);
	t_draw_list.add_image(icon_rect(TitleBarButton::Minimize), m_assets.get(Asset::IconMinimize),
						  glyph_color(TitleBarButton::Minimize));

	draw_hover(t_draw_list, TitleBarButton::Maximize, hovered);
	draw_maximize_glyph(t_draw_list, glyph_color(TitleBarButton::Maximize));

	draw_hover(t_draw_list, TitleBarButton::Close, hovered);
	t_draw_list.add_image(icon_rect(TitleBarButton::Close), m_assets.get(Asset::IconClose),
						  hovered == TitleBarButton::Close ? title_bar_close_glyph_hover : theme().text);
}
