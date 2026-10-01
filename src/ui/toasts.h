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
	std::string_view message;
	std::optional<Asset> icon;
	bool spin_icon = false;
	std::optional<Command> on_click;
	float seconds = 0.0f;
	bool always_show = false;
};

class Toasts : public Widget {
  public:
	Toasts(const Settings &t_settings, const Fonts &t_fonts, const Assets &t_assets, const Window &t_window,
		   CommandQueue &t_commands);

	void notify(const Notification &t_notification);
	void notify_countdown(std::string_view t_message, float t_seconds);
	bool is_offering(CommandType t_type) const;
	void dismiss();

	void update(float t_delta_seconds) override;
	void draw(DrawList &t_draw_list) override;

	bool on_pointer_down(Vec2 t_point) override;
	bool on_pointer_up(Vec2 t_point) override;

	CursorKind cursor() const override;

  private:
	static constexpr u32 max_lines = 2;

	void show(const Notification &t_notification, float t_seconds, bool t_countdown);

	float icon_column_width() const;
	Rect card_rect() const;
	Rect animated_card_rect() const;
	bool is_clickable_at(Vec2 t_point) const;
	void draw_time_left_bar(DrawList &t_draw_list, Rect t_card, u8 t_alpha) const;

	const Settings &m_settings;
	const Fonts &m_fonts;
	const Assets &m_assets;
	const Window &m_window;
	CommandQueue &m_commands;

	char m_message[96]{};
	std::optional<Asset> m_icon;
	bool m_spin_icon = false;
	std::optional<Command> m_on_click;

	bool m_showing = false;
	bool m_countdown = false;
	float m_remaining_seconds = 0.0f;
	float m_total_seconds = 0.0f;
	float m_presence = 0.0f;
	float m_elapsed_seconds = 0.0f;
};
