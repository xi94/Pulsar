#include "ui/context_menu.h"

#include <algorithm>
#include <cmath>
#include <ranges>

#include "core/animation.h"
#include "gfx/assets.h"
#include "gfx/draw_list.h"
#include "gfx/font.h"
#include "ui/controls.h"
#include "ui/text.h"
#include "ui/theme.h"

namespace {
constexpr float K_MIN_MENU_WIDTH         = 180.0f;
constexpr float K_MENU_PADDING           = 4.0f;
constexpr float K_MENU_RADIUS            = 10.0f;
constexpr float K_ITEM_RADIUS            = 6.0f;
constexpr float K_WINDOW_MARGIN          = 8.0f;
constexpr float K_LABEL_INSET            = 10.0f;
constexpr float K_SHORTCUT_GAP           = 24.0f;
constexpr float K_ICON_SIZE              = 16.0f;
constexpr float K_ICON_GAP               = 10.0f;
constexpr float K_CONFIRM_SECONDS        = 3.0f;
constexpr float K_CONFIRM_BAR_GAP        = 3.0f;
constexpr float K_CONFIRM_BAR_HEIGHT     = 1.5f;
constexpr u8    K_DANGER_HIGHLIGHT_ALPHA = 34;
constexpr float K_SEPARATOR_SPACE        = 9.0f;

constexpr std::string_view K_CONFIRM_LABEL = "Confirm";

[[nodiscard]] auto item_height(const Fonts* t_fonts) -> float
{
	return std::max(28.0f, t_fonts->body.line_height() + 8.0f);
}
}

ContextMenu::ContextMenu(const Fonts* t_fonts, const Assets* t_assets, CommandQueue* t_commands)
	: m_fonts(t_fonts)
	, m_assets(t_assets)
	, m_commands(t_commands)
{
}

auto ContextMenu::open(Vec2 t_position, std::span<const ContextMenuItem> t_items, Vec2 t_window_size) -> void
{
	m_item_count = static_cast<u32>(std::min<usize>(t_items.size(), K_MAX_ITEMS));
	std::copy_n(t_items.begin(), m_item_count, m_items);
	m_has_icons = std::ranges::any_of(std::span{m_items, m_item_count}, [](const ContextMenuItem& t_item) { return t_item.icon != Asset::COUNT; });
	m_armed.reset();

	float content = 0.0f;
	for (const ContextMenuItem& item : std::span{m_items, m_item_count}) {
		const float shortcut = item.shortcut.empty() ? 0.0f : K_SHORTCUT_GAP + controls::shortcut_width(m_fonts->secondary, item.shortcut);
		const float label    = std::max(text_width(m_fonts->body, item.label), item.destructive ? text_width(m_fonts->body, K_CONFIRM_LABEL) : 0.0f);
		content              = std::max(content, label + shortcut);
	}

	const float icon_space = m_has_icons ? K_ICON_SIZE + K_ICON_GAP : 0.0f;
	m_width                = std::ceil(std::max(K_MIN_MENU_WIDTH, content + icon_space + (K_LABEL_INSET + K_MENU_PADDING) * 2.0f));

	const float height = menu_rect().h;
	m_position.x       = std::clamp(t_position.x, K_WINDOW_MARGIN, std::max(K_WINDOW_MARGIN, t_window_size.x - K_WINDOW_MARGIN - m_width));
	m_position.y       = std::clamp(t_position.y, K_WINDOW_MARGIN, std::max(K_WINDOW_MARGIN, t_window_size.y - K_WINDOW_MARGIN - height));
	m_open             = true;
}

auto ContextMenu::close() -> void
{
	m_open = false;
	m_armed.reset();
}

auto ContextMenu::update(float t_delta_seconds) -> void
{
	if (!m_armed) return;

	m_armed_seconds -= t_delta_seconds;

	if (m_armed_seconds <= 0.0f) {
		m_armed.reset();
	} else {
		animation::request_frame();
	}
}

auto ContextMenu::menu_rect() const -> Rect
{
	const float bottom = m_item_count == 0 ? K_MENU_PADDING : item_rect(m_item_count - 1).bottom() - m_position.y;

	return Rect{m_position.x, m_position.y, m_width, bottom + K_MENU_PADDING};
}

auto ContextMenu::item_rect(u32 t_index) const -> Rect
{
	const float height = item_height(m_fonts);
	float       top    = m_position.y + K_MENU_PADDING + height * t_index;

	for (u32 i = 1; i <= t_index; i += 1) {
		top += m_items[i].separated ? K_SEPARATOR_SPACE : 0.0f;
	}

	return Rect{m_position.x + K_MENU_PADDING, top, m_width - K_MENU_PADDING * 2.0f, height};
}

auto ContextMenu::item_at(Vec2 t_point) const -> std::optional<u32>
{
	for (const u32 i : std::views::iota(0u, m_item_count)) {
		if (m_items[i].enabled && item_rect(i).contains(t_point)) return i;
	}

	return std::nullopt;
}

auto ContextMenu::on_pointer_up(Vec2 t_point) -> bool
{
	if (!m_open) return false;

	const std::optional<u32> chosen = item_at(t_point);

	if (chosen && m_items[*chosen].destructive && m_armed != chosen) {
		m_armed         = chosen;
		m_armed_seconds = K_CONFIRM_SECONDS;
		return true;
	}

	if (chosen) {
		m_commands->push(m_items[*chosen].command);
	}

	close();

	return true;
}

auto ContextMenu::on_right_click(Vec2) -> bool
{
	close();

	return false;
}

auto ContextMenu::on_key_down(os::Key t_key) -> bool
{
	if (!m_open) return false;

	if (t_key == os::Key::ESCAPE) {
		close();
	}

	return true;
}

auto ContextMenu::cursor() const -> CursorKind
{
	return m_open && item_at(m_mouse) ? CursorKind::HAND : CursorKind::ARROW;
}

auto ContextMenu::draw(DrawList* t_draw_list) -> void
{
	if (!m_open) return;

	const Font&          font = m_fonts->body;
	const RoundnessScope corners{controls::K_POPUP_ROUNDNESS};
	const float          label_x = K_LABEL_INSET + (m_has_icons ? K_ICON_SIZE + K_ICON_GAP : 0.0f);

	controls::draw_popup_shadow(t_draw_list, menu_rect(), K_MENU_RADIUS, 1.0f);
	controls::draw_glass(t_draw_list, menu_rect(), rounded(K_MENU_RADIUS), controls::GlassSurface::MENU, 255);

	for (const u32 i : std::views::iota(0u, m_item_count)) {
		const Rect             row  = item_rect(i);
		const ContextMenuItem& item = m_items[i];

		const bool  hovered_row = item.enabled && row.contains(m_mouse);
		const bool  armed       = m_armed == i;
		const Color backdrop    = hovered_row ? hovered(g_theme.popup) : g_theme.popup;
		const Color danger      = armed ? controls::confirm_red() : g_theme.error;
		const float baseline    = font.centered_baseline(row);

		Color ink = item.enabled ? g_theme.text : g_theme.text_faint;
		if (item.destructive && item.enabled) {
			ink = danger;
		}

		if (item.separated && i > 0) {
			const float y = snapped_to_pixel(row.y - K_SEPARATOR_SPACE * 0.5f);
			const Rect  line{row.x + K_LABEL_INSET, y, row.w - K_LABEL_INSET * 2.0f, 1.0f};
			t_draw_list->add_rect(line, g_theme.separator);
		}

		if (hovered_row || armed) {
			const Color highlight = item.destructive ? with_alpha(danger, K_DANGER_HIGHLIGHT_ALPHA) : controls::glass_highlight();
			t_draw_list->add_rounded_rect(row, rounded(K_ITEM_RADIUS), highlight);
		}

		if (item.icon != Asset::COUNT) {
			const Rect icon{row.x + K_LABEL_INSET, snapped_to_pixel(row.center().y - K_ICON_SIZE * 0.5f), K_ICON_SIZE, K_ICON_SIZE};

			Color icon_color = item.enabled ? g_theme.text_dim : g_theme.text_faint;
			if (item.destructive) {
				icon_color = ink;
			}

			t_draw_list->add_image(icon, m_assets->texture(item.icon), icon_color);
		}

		const std::string_view label = armed ? K_CONFIRM_LABEL : item.label;
		const Vec2             label_baseline{row.x + label_x, baseline};
		draw_text(t_draw_list, font, label_baseline, label, ink);

		if (armed) {
			const float left = std::clamp(m_armed_seconds / K_CONFIRM_SECONDS, 0.0f, 1.0f);
			const Rect  bar{row.x + label_x, baseline + K_CONFIRM_BAR_GAP, text_width(font, label) * left, K_CONFIRM_BAR_HEIGHT};
			t_draw_list->add_rounded_rect(bar, rounded(K_CONFIRM_BAR_HEIGHT * 0.5f), danger);
		}

		if (!item.shortcut.empty()) {
			const Vec2 keys_right_center{row.right() - K_LABEL_INSET, row.center().y};
			const u8   keys_alpha = item.enabled ? 255 : 128;
			controls::draw_shortcut(t_draw_list, m_fonts->secondary, keys_right_center, item.shortcut, backdrop, keys_alpha);
		}
	}
}
