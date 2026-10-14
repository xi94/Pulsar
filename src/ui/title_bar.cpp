#include "ui/title_bar.h"

#include <cmath>
#include <numbers>

#include "core/animation.h"
#include "core/app_identity.h"
#include "core/str.h"
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
constexpr std::string_view K_SEARCH_PILL_SHORTCUT  = PULSAR_SHORTCUT_KEY "+S";

[[nodiscard]] auto is_update_worth_showing(UpdateStage t_stage) -> bool
{
	switch (t_stage) {
		using enum UpdateStage;

		case AVAILABLE:
		case MANUAL_UPGRADE_REQUIRED:
		case DOWNLOADING:
		case VERIFYING:
		case INSTALLING:
		case READY_TO_RELAUNCH:
		case UPDATE_FAILED:
		case CANCELLED: {
			return true;
		}

		default: {
			return false;
		}
	}
}

constexpr float K_STATUS_WIDTH_RATE        = 16.0f;
constexpr float K_SPIN_TURNS_PER_SECOND    = 0.9f;
constexpr float K_STATUS_REVEAL_RATE       = 14.0f;
constexpr float K_STATUS_SHIFT             = 4.0f;
constexpr float K_STATUS_PADDING_OUTER     = 10.0f;
constexpr float K_STATUS_EMPHASIS_RATE     = 16.0f;
constexpr float K_STATUS_EMPHASIS_STRENGTH = 0.75f;

struct StatusLook {
	std::string_view label;
	std::string_view sizing_label;
	Asset            icon = Asset::ICON_UPDATE;
	Color            icon_color{};
	Color            text_color{};
	bool             spinning = false;
	bool             check    = false;
};

[[nodiscard]] auto status_look(const Updater* t_updater, UpdateStage t_stage, bool t_release_notes, char (&t_percent)[8]) -> StatusLook
{
	if (t_release_notes) {
		return StatusLook{"What's new", "What's new", Asset::ICON_UPDATE, g_theme.text_dim, g_theme.text};
	}

	switch (t_stage) {
		using enum UpdateStage;

		case IDLE:
		case CHECKING: {
			return StatusLook{"Checking for updates", "Checking for updates", Asset::ICON_UPDATE, g_theme.text_dim, g_theme.text_dim, true};
		}

		case UP_TO_DATE: {
			return StatusLook{"Up to date", "Up to date", Asset::ICON_CHECK, g_theme.text_dim, g_theme.text_dim, false, true};
		}

		case CHECK_FAILED: {
			return StatusLook{"Couldn't check", "Couldn't check", Asset::ICON_UPDATE, g_theme.error, g_theme.error};
		}

		case AVAILABLE:
		case MANUAL_UPGRADE_REQUIRED: {
			return StatusLook{"Update available", "Update available", Asset::ICON_DOWNLOAD, g_theme.success, g_theme.text};
		}

		case DOWNLOADING: {
			const u64              total   = t_updater->total_bytes();
			const u64              percent = total > 0 ? t_updater->bytes_downloaded() * 100 / total : 0;
			const std::string_view label   = format_into(t_percent, "{}%", std::min<u64>(percent, 100));

			return StatusLook{label, "100%", Asset::ICON_DOWNLOAD, g_theme.text_dim, g_theme.text};
		}

		case VERIFYING: {
			return StatusLook{"Verifying", "Verifying", Asset::ICON_UPDATE, g_theme.text_dim, g_theme.text_dim, true};
		}

		case INSTALLING: {
			return StatusLook{"Installing", "Installing", Asset::ICON_UPDATE, g_theme.text_dim, g_theme.text_dim, true};
		}

		case READY_TO_RELAUNCH: {
			return StatusLook{"Restarting", "Restarting", Asset::ICON_UPDATE, g_theme.text_dim, g_theme.text_dim, true};
		}

		case UPDATE_FAILED: {
			return StatusLook{"Update failed", "Update failed", Asset::ICON_UPDATE, g_theme.error, g_theme.error};
		}

		case CANCELLED: {
			return StatusLook{"Update cancelled", "Update cancelled", Asset::ICON_UPDATE, g_theme.text_dim, g_theme.text_dim};
		}
	}

	return StatusLook{};
}
}

TitleBar::TitleBar(os::Window* t_window, const Updater* t_updater, const UpdateOverlay* t_update_overlay, const Fonts* t_fonts, const Assets* t_assets, CommandQueue* t_commands)
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
	const bool visible = is_update_worth_showing(m_updater->stage()) || m_update_overlay->m_open || m_update_overlay->is_shown() || m_update_overlay->wants_status();
	m_update_visible   = visible;
	m_update_reveal    = animation::ease_toward(m_update_reveal, visible ? 1.0f : 0.0f, K_STATUS_REVEAL_RATE, t_delta_seconds);

	const bool emphasized = visible && (m_update_overlay->m_open || layout().button_rect(TitleBarButton::UPDATE).contains(m_mouse));
	m_status_emphasis     = animation::ease_toward(m_status_emphasis, emphasized ? 1.0f : 0.0f, K_STATUS_EMPHASIS_RATE, t_delta_seconds);

	if (!visible && m_update_reveal <= 0.0f) return;

	char             percent[8];
	const StatusLook look   = status_look(m_updater, m_update_overlay->m_shown_stage, m_update_overlay->m_showing_release_notes, percent);
	const float      target = K_IDENTITY_GAP * 0.5f + K_ICON_SIZE + K_STATUS_ICON_GAP + std::ceil(text_width(m_fonts->secondary, look.sizing_label)) + K_STATUS_PADDING_OUTER;

	m_status_width = m_status_width <= 0.0f ? target : animation::ease_toward(m_status_width, target, K_STATUS_WIDTH_RATE, t_delta_seconds, animation::K_SETTLED_PIXELS);

	if (look.spinning) {
		constexpr float FULL_TURN = std::numbers::pi_v<float> * 2.0f;

		m_status_spin = std::fmod(m_status_spin + t_delta_seconds * K_SPIN_TURNS_PER_SECOND * FULL_TURN, FULL_TURN);
		animation::request_frame();
	}
}

auto TitleBar::layout() const -> TitleBarLayout
{
	return TitleBarLayout{
		.width                 = static_cast<float>(m_window->m_width),
		.native_controls_width = m_window->m_native_controls_width,
		.update_visible        = m_update_visible,
		.update_width          = m_status_width > 0.0f ? m_status_width : K_UPDATE_BUTTON_WIDTH,
		.search_visible        = m_search_visible,
		.identity_width        = identity_width(),
	};
}

auto TitleBar::identity_width() const -> float
{
	return K_IDENTITY_GAP * 0.5f + K_IDENTITY_MARK_SIZE + K_IDENTITY_GAP + std::ceil(text_width(m_fonts->secondary, K_APP_NAME));
}

auto TitleBar::on_pointer_down(Vec2 t_point) -> bool
{
	return layout().button_at(t_point) != TitleBarButton::NONE;
}

auto TitleBar::on_pointer_up(Vec2 t_point) -> bool
{
	switch (layout().button_at(t_point)) {
		using enum TitleBarButton;

		case NONE: {
			return false;
		}

		case MENU: {
			m_commands->push(Command{.type = CommandType::TOGGLE_APP_MENU});
			break;
		}

		case SEARCH: {
			m_commands->push(Command{.type = CommandType::OPEN_ACCOUNT_SEARCH});
			break;
		}

		case UPDATE: {
			m_commands->push(Command{.type = CommandType::TOGGLE_UPDATE_OVERLAY});
			break;
		}

		case MINIMIZE: {
			m_window->minimize();
			break;
		}

		case MAXIMIZE: {
			m_window->toggle_maximized();
			break;
		}

		case CLOSE: {
			m_window->close();
			break;
		}
	}

	return true;
}

auto TitleBar::cursor() const -> CursorKind
{
	return layout().button_at(m_mouse) == TitleBarButton::NONE ? CursorKind::ARROW : CursorKind::HAND;
}

auto TitleBar::draw_hover(DrawList* t_draw_list, TitleBarButton t_button, TitleBarButton t_hovered) const -> void
{
	if (t_button != t_hovered) return;

	const Color hover = t_button == TitleBarButton::CLOSE ? K_TITLE_BAR_CLOSE_HOVER : with_alpha(g_theme.text, K_TITLE_BAR_HOVER_ALPHA);
	t_draw_list->add_rect(layout().button_rect(t_button), hover);
}

auto TitleBar::draw_identity(DrawList* t_draw_list, float t_amount) const -> void
{
	const TitleBarLayout title_bar = layout();
	const Rect           menu      = title_bar.button_rect(TitleBarButton::MENU);
	const Font&          font      = m_fonts->secondary;
	const float          start     = title_bar.menu_on_right() ? title_bar.native_controls_width : menu.right();
	const float          left      = start + K_IDENTITY_GAP * 0.5f;
	const float          shift     = -K_STATUS_SHIFT * (1.0f - t_amount);
	const Rect           mark{left, (K_TITLE_BAR_HEIGHT - K_IDENTITY_MARK_SIZE) * 0.5f + shift, K_IDENTITY_MARK_SIZE, K_IDENTITY_MARK_SIZE};
	const Vec2           name_baseline{mark.right() + K_IDENTITY_GAP, font.centered_baseline(menu) + shift};
	const auto           alpha = to_alpha(t_amount);

	t_draw_list->add_image(mark, m_assets->texture(Asset::ICON_APP), faded(g_theme.text_dim, alpha));
	draw_text(t_draw_list, font, name_baseline, K_APP_NAME, faded(g_theme.text_dim, alpha));
}

auto TitleBar::draw_update_status(DrawList* t_draw_list, float t_amount) const -> void
{
	char                 percent[8];
	const StatusLook     look      = status_look(m_updater, m_update_overlay->m_shown_stage, m_update_overlay->m_showing_release_notes, percent);
	const TitleBarLayout title_bar = layout();
	const Rect           area      = title_bar.button_rect(TitleBarButton::UPDATE);
	const auto           alpha     = to_alpha(t_amount);
	const float          shift     = K_STATUS_SHIFT * (1.0f - t_amount);
	t_draw_list->push_clip(area);

	const Font& font    = m_fonts->secondary;
	const float padding = title_bar.menu_on_right() ? K_STATUS_PADDING_OUTER : K_IDENTITY_GAP * 0.5f;
	const Rect  icon{area.x + padding, (K_TITLE_BAR_HEIGHT - K_ICON_SIZE) * 0.5f + shift, K_ICON_SIZE, K_ICON_SIZE};
	const Color icon_color = faded(look.icon_color, alpha);

	if (look.spinning) {
		t_draw_list->add_rotated_image(icon, m_status_spin, m_assets->texture(look.icon), icon_color);
	} else if (look.check) {
		controls::draw_check(t_draw_list, m_assets, icon, icon_color);
	} else {
		t_draw_list->add_image(icon, m_assets->texture(look.icon), icon_color);
	}

	const Vec2  label_baseline{icon.right() + K_STATUS_ICON_GAP, font.centered_baseline(area) + shift};
	const Color label_color = faded(mix(look.text_color, g_theme.text, K_STATUS_EMPHASIS_STRENGTH * m_status_emphasis), alpha);

	draw_text(t_draw_list, font, label_baseline, look.label, label_color);
	t_draw_list->pop_clip();
}

auto TitleBar::draw_search_pill(DrawList* t_draw_list, TitleBarButton t_hovered) const -> void
{
	if (!m_search_visible) return;

	const Rect area = layout().button_rect(TitleBarButton::SEARCH);
	if (area.w <= 0.0f) return;

	const bool  is_hovered = t_hovered == TitleBarButton::SEARCH;
	const Rect  pill       = area.inset(0.0f, (area.h - K_SEARCH_PILL_HEIGHT) * 0.5f);
	const Color fill       = is_hovered ? hovered(g_theme.field) : g_theme.field;
	const Color border     = is_hovered ? g_theme.border : g_theme.separator;
	const Color label      = is_hovered ? g_theme.text_dim : g_theme.text_faint;
	const Font& font       = m_fonts->secondary;
	const Font& key_font   = m_fonts->caption;

	t_draw_list->add_bordered_rect(pill, rounded(K_SEARCH_PILL_RADIUS), fill, border, 1.0f);

	const Rect  icon{pill.x + K_SEARCH_PILL_PADDING, pill.center().y - K_SEARCH_PILL_ICON_SIZE * 0.5f, K_SEARCH_PILL_ICON_SIZE, K_SEARCH_PILL_ICON_SIZE};
	const float keys_right  = pill.right() - std::floor((pill.h - controls::keycap_height(key_font)) * 0.5f);
	const float keys_left   = keys_right - controls::shortcut_width(key_font, K_SEARCH_PILL_SHORTCUT);
	const float text_left   = icon.right() + K_SEARCH_PILL_ICON_GAP;
	const float room        = std::max(0.0f, keys_left - K_SEARCH_PILL_ICON_GAP - text_left);
	const float label_width = std::min(text_width(font, K_SEARCH_PILL_TEXT), room);
	const Vec2  label_baseline{snapped_to_pixel(text_left + (room - label_width) * 0.5f), font.centered_baseline(pill)};
	const Vec2  keys_right_center{keys_right, pill.center().y};

	controls::draw_magnifier(t_draw_list, icon, g_theme.text_faint);
	draw_text_truncated(t_draw_list, font, label_baseline, K_SEARCH_PILL_TEXT, room, label);
	controls::draw_shortcut(t_draw_list, key_font, keys_right_center, K_SEARCH_PILL_SHORTCUT, fill, 255);
}

auto TitleBar::draw_maximize_glyph(DrawList* t_draw_list, Color t_color) const -> void
{
	const Vec2 center = layout().button_rect(TitleBarButton::MAXIMIZE).center();

	if (!m_window->is_maximized()) {
		constexpr float SQUARE_SIZE = 10.0f;

		const Rect square{center.x - SQUARE_SIZE * 0.5f, center.y - SQUARE_SIZE * 0.5f, SQUARE_SIZE, SQUARE_SIZE};
		t_draw_list->add_rect_outline(square, K_GLYPH_THICKNESS, t_color);
		return;
	}

	constexpr float SQUARE_SIZE   = 8.0f;
	constexpr float SQUARE_OFFSET = 3.0f;

	const Rect middle{center.x - SQUARE_SIZE * 0.5f, center.y - SQUARE_SIZE * 0.5f, SQUARE_SIZE, SQUARE_SIZE};
	const Rect back  = middle.moved(Vec2{SQUARE_OFFSET, -SQUARE_OFFSET});
	const Rect front = middle.moved(Vec2{-SQUARE_OFFSET, SQUARE_OFFSET});

	t_draw_list->add_rect_outline(back, K_GLYPH_THICKNESS, t_color);
	t_draw_list->add_rect_outline(front, K_GLYPH_THICKNESS, t_color);
}

auto TitleBar::draw(DrawList* t_draw_list) -> void
{
	const Rect bar{0.0f, 0.0f, static_cast<float>(m_window->m_width), K_TITLE_BAR_HEIGHT};
	t_draw_list->add_rect(bar, g_theme.chrome);

	const TitleBarButton hovered     = layout().button_at(m_mouse);
	const auto           icon_rect   = [this](TitleBarButton t_button) { return layout().button_rect(t_button).centered(K_ICON_SIZE, K_ICON_SIZE); };
	const auto           glyph_color = [hovered](TitleBarButton t_button) { return hovered == t_button ? g_theme.text : g_theme.text_dim; };

	draw_hover(t_draw_list, TitleBarButton::MENU, hovered);
	t_draw_list->add_image(icon_rect(TitleBarButton::MENU), m_assets->texture(Asset::ICON_MENU), g_theme.text);

	// On Windows the update status takes the name's place beside the menu. On macOS they sit at opposite ends, so the name stays.
	const float identity = layout().menu_on_right() ? 1.0f : 1.0f - m_update_reveal;
	if (identity > 0.001f) {
		draw_identity(t_draw_list, identity);
	}

	if (m_update_reveal > 0.001f) {
		draw_update_status(t_draw_list, m_update_reveal);
	}

	draw_search_pill(t_draw_list, hovered);

	if (m_window->m_native_controls_width > 0.0f) return;

	draw_hover(t_draw_list, TitleBarButton::MINIMIZE, hovered);
	t_draw_list->add_image(icon_rect(TitleBarButton::MINIMIZE), m_assets->texture(Asset::ICON_MINIMIZE), glyph_color(TitleBarButton::MINIMIZE));

	draw_hover(t_draw_list, TitleBarButton::MAXIMIZE, hovered);
	draw_maximize_glyph(t_draw_list, glyph_color(TitleBarButton::MAXIMIZE));

	const Color close_glyph = hovered == TitleBarButton::CLOSE ? K_TITLE_BAR_CLOSE_GLYPH_HOVER : g_theme.text;

	draw_hover(t_draw_list, TitleBarButton::CLOSE, hovered);
	t_draw_list->add_image(icon_rect(TitleBarButton::CLOSE), m_assets->texture(Asset::ICON_CLOSE), close_glyph);
}
