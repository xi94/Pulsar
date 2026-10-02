#pragma once

#include <optional>
#include <string_view>

#include "gfx/assets.h"
#include "ui/commands.h"
#include "ui/widget.h"

struct Fonts;
class Window;
struct Settings;

struct Notification {
	std::string_view       message;
	std::optional<Asset>   icon;
	bool                   spin_icon = false;
	std::optional<Command> on_click;
	float                  seconds     = 0.0f;
	bool                   always_show = false;
};

class Toasts : public Widget {
  public:
	Toasts(const Settings* t_settings, const Fonts* t_fonts, const Assets* t_assets, const Window* t_window, CommandQueue* t_commands);

	auto notify(const Notification& t_notification) -> void;
	auto notify_countdown(std::string_view t_message, float t_seconds) -> void;
	[[nodiscard]] auto is_offering(CommandType t_type) const -> bool;
	auto dismiss() -> void;

	auto update(float t_delta_seconds) -> void override;
	auto draw(DrawList* t_draw_list) -> void override;

	auto on_pointer_down(Vec2 t_point) -> bool override;
	auto on_pointer_up(Vec2 t_point) -> bool override;

	[[nodiscard]] auto cursor() const -> CursorKind override;

  private:
	static constexpr u32 K_MAX_LINES = 2;

	auto show(const Notification& t_notification, float t_seconds, bool t_countdown) -> void;

	[[nodiscard]] auto icon_column_width() const -> float;
	[[nodiscard]] auto card_rect() const -> Rect;
	[[nodiscard]] auto animated_card_rect() const -> Rect;
	[[nodiscard]] auto is_clickable_at(Vec2 t_point) const -> bool;
	auto draw_time_left_bar(DrawList* t_draw_list, Rect t_card, u8 t_alpha) const -> void;

	const Settings* m_settings;
	const Fonts*    m_fonts;
	const Assets*   m_assets;
	const Window*   m_window;
	CommandQueue*   m_commands;

	char                   m_message[96]{};
	std::optional<Asset>   m_icon;
	bool                   m_spin_icon = false;
	std::optional<Command> m_on_click;

	bool  m_showing           = false;
	bool  m_countdown         = false;
	float m_remaining_seconds = 0.0f;
	float m_total_seconds     = 0.0f;
	float m_presence          = 0.0f;
	float m_elapsed_seconds   = 0.0f;
};
