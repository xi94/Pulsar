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
constexpr float            K_GLYPH_THICKNESS       = 1.5f;
constexpr float            K_ICON_SIZE             = 16.0f;
constexpr float            K_STATUS_ICON_GAP       = 8.0f;
constexpr float            K_IDENTITY_MARK_SIZE    = 16.0f;
constexpr float            K_IDENTITY_GAP          = 8.0f;
constexpr float            K_SEARCH_PILL_HEIGHT    = 26.0f;
constexpr float            K_SEARCH_PILL_RADIUS    = 7.0f;
constexpr float            K_SEARCH_PILL_PADDING   = 10.0f;
constexpr float            K_SEARCH_PILL_ICON_SIZE = 13.0f;
constexpr float            K_SEARCH_PILL_ICON_GAP  = 8.0f;
constexpr std::string_view K_SEARCH_PILL_TEXT      = "Search all accounts";
constexpr std::string_view K_SEARCH_PILL_SHORTCUT  = "Ctrl+S";

[[nodiscard]] auto is_update_worth_showing(UpdateStage t_stage) -> bool
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

constexpr float K_STATUS_WIDTH_RATE        = 16.0f;
constexpr float K_SPIN_TURNS_PER_SECOND    = 0.9f;
constexpr float K_STATUS_REVEAL_RATE       = 14.0f;
constexpr float K_STATUS_SHIFT             = 4.0f;
constexpr float K_STATUS_PADDING_RIGHT     = 10.0f;
constexpr float K_STATUS_EMPHASIS_RATE     = 16.0f;
constexpr float K_STATUS_EMPHASIS_STRENGTH = 0.75f;

struct StatusLook {
	std::string_view label;
	std::string_view sizing_label;
	Asset            icon = Asset::IconUpdate;
	Color            icon_color{};
	Color            text_color{};
	bool             spinning = false;
	bool             check    = false;
};

[[nodiscard]] auto status_look(const Updater* t_updater, UpdateStage t_stage, bool t_release_notes, char (&t_percent)[8]) -> StatusLook
{
	if (t_release_notes) {
		return StatusLook{"What's new", "What's new", Asset::IconUpdate, g_theme.text_dim, g_theme.text};
	}

	switch (t_stage) {
		case UpdateStage::Idle:
		case UpdateStage::Checking:
			return StatusLook{"Checking for updates", "Checking for updates", Asset::IconUpdate, g_theme.text_dim, g_theme.text_dim, true};

		case UpdateStage::UpToDate:
			return StatusLook{"Up to date", "Up to date", Asset::IconCheck, g_theme.text_dim, g_theme.text_dim, false, true};

		case UpdateStage::CheckFailed:
			return StatusLook{"Couldn't check", "Couldn't check", Asset::IconUpdate, g_theme.error, g_theme.error};

		case UpdateStage::Available:
		case UpdateStage::ManualUpgradeRequired:
			return StatusLook{"Update available", "Update available", Asset::IconDownload, g_theme.success, g_theme.text};

		case UpdateStage::Downloading: {
			const u64 total   = t_updater->total_bytes();
			const u64 percent = total > 0 ? t_updater->bytes_downloaded() * 100 / total : 0;
			const int written = std::snprintf(t_percent, sizeof(t_percent), "%u%%", static_cast<unsigned>(std::min<u64>(percent, 100)));

			return StatusLook{std::string_view{t_percent, static_cast<usize>(std::max(written, 0))}, "100%", Asset::IconDownload, g_theme.text_dim,
			                  g_theme.text};
		}

		case UpdateStage::Verifying:
			return StatusLook{"Verifying", "Verifying", Asset::IconUpdate, g_theme.text_dim, g_theme.text_dim, true};

		case UpdateStage::Installing:
			return StatusLook{"Installing", "Installing", Asset::IconUpdate, g_theme.text_dim, g_theme.text_dim, true};

		case UpdateStage::ReadyToRelaunch:
			return StatusLook{"Restarting", "Restarting", Asset::IconUpdate, g_theme.text_dim, g_theme.text_dim, true};

		case UpdateStage::Error:
			return StatusLook{"Update failed", "Update failed", Asset::IconUpdate, g_theme.error, g_theme.error};

		case UpdateStage::Cancelled:
			return StatusLook{"Update cancelled", "Update cancelled", Asset::IconUpdate, g_theme.text_dim, g_theme.text_dim};
	}

	return StatusLook{};
}
}

TitleBar::TitleBar(Window*              t_window,
                   const Updater*       t_updater,
                   const UpdateOverlay* t_update_overlay,
                   const Fonts*         t_fonts,
                   const Assets*        t_assets,
                   CommandQueue*        t_commands)
	: m_window(t_window)
	, m_updater(t_updater)
	, m_update_overlay(t_update_overlay)
	, m_fonts(t_fonts)
	, m_assets(t_assets)
	, m_commands(t_commands)
{
}

auto TitleBar::update(float t_delta_seconds) -> void
{
	const bool visible =
		is_update_worth_showing(m_updater->stage()) || m_update_overlay->is_open() || m_update_overlay->is_shown() || m_update_overlay->wants_status();
	m_window->set_update_button_visible(visible);
	m_update_reveal = animation::ease_toward(m_update_reveal, visible ? 1.0f : 0.0f, K_STATUS_REVEAL_RATE, t_delta_seconds);

	const bool emphasized = visible && (m_update_overlay->is_open() || m_window->title_bar_button_rect(TitleBarButton::Update).contains(m_mouse));
	m_status_emphasis     = animation::ease_toward(m_status_emphasis, emphasized ? 1.0f : 0.0f, K_STATUS_EMPHASIS_RATE, t_delta_seconds);

	if (!visible && m_update_reveal <= 0.0f) return;

	char             percent[8];
	const StatusLook look = status_look(m_updater, m_update_overlay->shown_stage(), m_update_overlay->is_showing_release_notes(), percent);
	const float      target =
		K_IDENTITY_GAP * 0.5f + K_ICON_SIZE + K_STATUS_ICON_GAP + std::ceil(text_width(m_fonts->secondary, look.sizing_label)) + K_STATUS_PADDING_RIGHT;

	m_status_width =
		m_status_width <= 0.0f ? target : animation::ease_toward(m_status_width, target, K_STATUS_WIDTH_RATE, t_delta_seconds, animation::K_SETTLED_PIXELS);
	m_window->set_update_button_width(m_status_width);

	if (look.spinning) {
		constexpr float FULL_TURN = std::numbers::pi_v<float> * 2.0f;

		m_status_spin = std::fmod(m_status_spin + t_delta_seconds * K_SPIN_TURNS_PER_SECOND * FULL_TURN, FULL_TURN);
		animation::request_frame();
	}
}

auto TitleBar::on_pointer_down(Vec2 t_point) -> bool
{
	return m_window->title_bar_button_at(t_point) != TitleBarButton::None;
}

auto TitleBar::on_pointer_up(Vec2 t_point) -> bool
{
	switch (m_window->title_bar_button_at(t_point)) {
		case TitleBarButton::None:
			return false;

		case TitleBarButton::Menu:
			m_commands->push(Command{.type = CommandType::ToggleAppMenu});
			break;

		case TitleBarButton::Search:
			m_commands->push(Command{.type = CommandType::OpenAccountSearch});
			break;

		case TitleBarButton::Update:
			m_commands->push(Command{.type = CommandType::ToggleUpdateOverlay});
			break;

		case TitleBarButton::Minimize:
			ShowWindow(m_window->handle(), SW_MINIMIZE);
			break;

		case TitleBarButton::Maximize:
			ShowWindow(m_window->handle(), m_window->is_maximized() ? SW_RESTORE : SW_MAXIMIZE);
			break;

		case TitleBarButton::Close:
			PostMessageW(m_window->handle(), WM_CLOSE, 0, 0);
			break;
	}

	return true;
}

auto TitleBar::cursor() const -> CursorKind
{
	return m_window->title_bar_button_at(m_mouse) == TitleBarButton::None ? CursorKind::Arrow : CursorKind::Hand;
}

auto TitleBar::draw_hover(DrawList* t_draw_list, TitleBarButton t_button, TitleBarButton t_hovered) const -> void
{
	if (t_button != t_hovered) return;

	t_draw_list->add_rect(m_window->title_bar_button_rect(t_button),
	                      t_button == TitleBarButton::Close ? K_TITLE_BAR_CLOSE_HOVER : with_alpha(g_theme.text, K_TITLE_BAR_HOVER_ALPHA));
}

auto TitleBar::draw_identity(DrawList* t_draw_list, float t_amount) const -> void
{
	const Rect  menu  = m_window->title_bar_button_rect(TitleBarButton::Menu);
	const float shift = -K_STATUS_SHIFT * (1.0f - t_amount);
	const Rect  mark{menu.right() + K_IDENTITY_GAP * 0.5f, (K_TITLE_BAR_HEIGHT - K_IDENTITY_MARK_SIZE) * 0.5f + shift, K_IDENTITY_MARK_SIZE,
	                 K_IDENTITY_MARK_SIZE};
	const Font& font  = m_fonts->secondary;
	const auto  alpha = to_alpha(t_amount);

	t_draw_list->add_image(mark, m_assets->get(Asset::IconApp), faded(g_theme.text_dim, alpha));
	draw_text(t_draw_list, font, Vec2{mark.right() + K_IDENTITY_GAP, font.centered_baseline(menu) + shift}, K_APP_NAME, faded(g_theme.text_dim, alpha));
}

auto TitleBar::draw_update_status(DrawList* t_draw_list, float t_amount) const -> void
{
	char             percent[8];
	const StatusLook look  = status_look(m_updater, m_update_overlay->shown_stage(), m_update_overlay->is_showing_release_notes(), percent);
	const Rect       area  = m_window->title_bar_button_rect(TitleBarButton::Update);
	const auto       alpha = to_alpha(t_amount);
	const float      shift = K_STATUS_SHIFT * (1.0f - t_amount);
	t_draw_list->push_clip(area);

	const Font& font = m_fonts->secondary;
	const Rect  icon{area.x + K_IDENTITY_GAP * 0.5f, (K_TITLE_BAR_HEIGHT - K_ICON_SIZE) * 0.5f + shift, K_ICON_SIZE, K_ICON_SIZE};
	const Color icon_color = faded(look.icon_color, alpha);

	if (look.spinning) {
		t_draw_list->add_rotated_image(icon, m_status_spin, m_assets->get(look.icon), icon_color);
	} else if (look.check) {
		controls::draw_check(t_draw_list, m_assets, icon, icon_color);
	} else {
		t_draw_list->add_image(icon, m_assets->get(look.icon), icon_color);
	}

	draw_text(t_draw_list, font, Vec2{icon.right() + K_STATUS_ICON_GAP, font.centered_baseline(area) + shift}, look.label,
	          faded(mix(look.text_color, g_theme.text, K_STATUS_EMPHASIS_STRENGTH * m_status_emphasis), alpha));

	t_draw_list->pop_clip();
}

auto TitleBar::draw_search_pill(DrawList* t_draw_list, TitleBarButton t_hovered) const -> void
{
	if (!m_window->is_search_button_visible()) return;

	const Rect area = m_window->title_bar_button_rect(TitleBarButton::Search);
	if (area.w <= 0.0f) return;

	const bool  is_hovered = t_hovered == TitleBarButton::Search;
	const Rect  pill       = area.inset(0.0f, (area.h - K_SEARCH_PILL_HEIGHT) * 0.5f);
	const Color fill       = is_hovered ? hovered(g_theme.field) : g_theme.field;
	const Font& font       = m_fonts->secondary;
	const Font& key_font   = m_fonts->caption;

	t_draw_list->add_bordered_rect(pill, rounded(K_SEARCH_PILL_RADIUS), fill, is_hovered ? g_theme.border : g_theme.separator, 1.0f);

	const Rect  icon{pill.x + K_SEARCH_PILL_PADDING, pill.center().y - K_SEARCH_PILL_ICON_SIZE * 0.5f, K_SEARCH_PILL_ICON_SIZE, K_SEARCH_PILL_ICON_SIZE};
	const float keys_right  = pill.right() - std::floor((pill.h - controls::keycap_height(key_font)) * 0.5f);
	const float keys_left   = keys_right - controls::shortcut_width(key_font, K_SEARCH_PILL_SHORTCUT);
	const float text_left   = icon.right() + K_SEARCH_PILL_ICON_GAP;
	const float room        = std::max(0.0f, keys_left - K_SEARCH_PILL_ICON_GAP - text_left);
	const float label_width = std::min(text_width(font, K_SEARCH_PILL_TEXT), room);

	controls::draw_magnifier(t_draw_list, icon, g_theme.text_faint);
	draw_text_truncated(t_draw_list, font, Vec2{snapped_to_pixel(text_left + (room - label_width) * 0.5f), font.centered_baseline(pill)}, K_SEARCH_PILL_TEXT,
	                    room, is_hovered ? g_theme.text_dim : g_theme.text_faint);
	controls::draw_shortcut(t_draw_list, key_font, Vec2{keys_right, pill.center().y}, K_SEARCH_PILL_SHORTCUT, fill, 255);
}

auto TitleBar::draw_maximize_glyph(DrawList* t_draw_list, Color t_color) const -> void
{
	const Vec2 center = m_window->title_bar_button_rect(TitleBarButton::Maximize).center();

	if (!m_window->is_maximized()) {
		constexpr float SQUARE_SIZE = 10.0f;
		t_draw_list->add_rect_outline(Rect{center.x - SQUARE_SIZE * 0.5f, center.y - SQUARE_SIZE * 0.5f, SQUARE_SIZE, SQUARE_SIZE}, K_GLYPH_THICKNESS, t_color);
		return;
	}

	constexpr float SQUARE_SIZE   = 8.0f;
	constexpr float SQUARE_OFFSET = 3.0f;
	t_draw_list->add_rect_outline(Rect{center.x - SQUARE_SIZE * 0.5f + SQUARE_OFFSET, center.y - SQUARE_SIZE * 0.5f - SQUARE_OFFSET, SQUARE_SIZE, SQUARE_SIZE},
	                              K_GLYPH_THICKNESS, t_color);
	t_draw_list->add_rect_outline(Rect{center.x - SQUARE_SIZE * 0.5f - SQUARE_OFFSET, center.y - SQUARE_SIZE * 0.5f + SQUARE_OFFSET, SQUARE_SIZE, SQUARE_SIZE},
	                              K_GLYPH_THICKNESS, t_color);
}

auto TitleBar::draw(DrawList* t_draw_list) -> void
{
	t_draw_list->add_rect(Rect{0.0f, 0.0f, static_cast<float>(m_window->width()), K_TITLE_BAR_HEIGHT}, g_theme.chrome);

	const TitleBarButton hovered     = m_window->title_bar_button_at(m_mouse);
	const auto           icon_rect   = [this](TitleBarButton t_button) { return m_window->title_bar_button_rect(t_button).centered(K_ICON_SIZE, K_ICON_SIZE); };
	const auto           glyph_color = [hovered](TitleBarButton t_button) { return hovered == t_button ? g_theme.text : g_theme.text_dim; };

	draw_hover(t_draw_list, TitleBarButton::Menu, hovered);
	t_draw_list->add_image(icon_rect(TitleBarButton::Menu), m_assets->get(Asset::IconMenu), g_theme.text);

	if (m_update_reveal < 0.999f) {
		draw_identity(t_draw_list, 1.0f - m_update_reveal);
	}

	if (m_update_reveal > 0.001f) {
		draw_update_status(t_draw_list, m_update_reveal);
	}

	draw_search_pill(t_draw_list, hovered);

	draw_hover(t_draw_list, TitleBarButton::Minimize, hovered);
	t_draw_list->add_image(icon_rect(TitleBarButton::Minimize), m_assets->get(Asset::IconMinimize), glyph_color(TitleBarButton::Minimize));

	draw_hover(t_draw_list, TitleBarButton::Maximize, hovered);
	draw_maximize_glyph(t_draw_list, glyph_color(TitleBarButton::Maximize));

	draw_hover(t_draw_list, TitleBarButton::Close, hovered);
	t_draw_list->add_image(icon_rect(TitleBarButton::Close), m_assets->get(Asset::IconClose),
	                       hovered == TitleBarButton::Close ? K_TITLE_BAR_CLOSE_GLYPH_HOVER : g_theme.text);
}
