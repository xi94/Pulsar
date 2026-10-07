#pragma once

#include <future>
#include <memory>
#include <string_view>

#include "core/crypto.h"
#include "ui/commands.h"
#include "ui/text_input.h"
#include "ui/widget.h"

class Assets;
struct Fonts;
class Texture;
namespace os {
class Window;
}
struct Settings;

class UnlockScreen : public Widget {
  public:
	UnlockScreen(Settings*         t_settings,
	             MasterKey*        t_master_key,
	             const Fonts*      t_fonts,
	             const Assets*     t_assets,
	             const os::Window* t_window,
	             CommandQueue*     t_commands);

	auto show_unlock(bool t_animated) -> void;
	auto show_setup(bool t_animated) -> void;
	auto hide() -> void;

	[[nodiscard]] auto covers_window() const -> bool
	{
		return m_backdrop >= 1.0f;
	}

	auto set_app_icon(const Texture* t_icon) -> void
	{
		m_app_icon = t_icon;
	}

	auto update(float t_delta_seconds) -> void override;
	auto draw(DrawList* t_draw_list) -> void override;

	auto on_pointer_down(Vec2 t_point) -> bool override;
	auto on_pointer_move(Vec2 t_point) -> bool override;
	auto on_pointer_up(Vec2 t_point) -> bool override;
	auto on_right_click(Vec2 t_point) -> bool override;
	auto on_key_down(os::Key t_key) -> bool override;
	auto on_char(u32 t_character) -> bool override;

	[[nodiscard]] auto is_blocking() const -> bool override
	{
		return m_active;
	}

	[[nodiscard]] auto cursor() const -> CursorKind override;

  private:
	static constexpr u32 K_PASSWORD     = 0;
	static constexpr u32 K_CONFIRMATION = 1;

	struct DerivedKey {
		MasterKey       key;
		MasterKeyParams params;
		bool            succeeded = false;
	};

	struct Layout {
		Rect  icon;
		float title_baseline;
		float subtitle_baseline;
		Rect  fields[2];
		float notice_top;
	};

	[[nodiscard]] auto field_count() const -> u32
	{
		return m_setup ? 2 : 1;
	}

	[[nodiscard]] auto is_deriving() const -> bool
	{
		return m_derivation.valid();
	}

	[[nodiscard]] auto subtitle_lines() const -> u32
	{
		return m_setup ? 2 : 1;
	}

	[[nodiscard]] auto layout() const -> Layout;
	[[nodiscard]] auto shake_offset() const -> float;
	[[nodiscard]] auto field_text_rect(const Layout& t_layout, u32 t_field) const -> Rect;
	[[nodiscard]] auto reveal_rect(const Layout& t_layout, u32 t_field) const -> Rect;
	[[nodiscard]] auto submit_rect(const Layout& t_layout) const -> Rect;
	[[nodiscard]] auto field_at(const Layout& t_layout, Vec2 t_point) const -> i32;
	[[nodiscard]] auto is_reveal_hit(const Layout& t_layout, Vec2 t_point) const -> bool;
	[[nodiscard]] auto error_message() const -> std::string_view;

	auto reset_fields() -> void;
	auto focus_field(u32 t_field) -> void;
	auto submit() -> void;
	auto attempt_unlock() -> void;
	auto attempt_setup() -> void;
	auto clear_errors() -> void;
	auto start_derivation(std::string_view t_password, bool t_create) -> void;
	auto finish_derivation(std::unique_ptr<DerivedKey> t_derived) -> void;

	auto show(bool t_animated) -> void;
	auto animate_presence(float t_delta_seconds) -> void;

	auto draw_field(DrawList* t_draw_list, const Layout& t_layout, u32 t_field, u8 t_alpha) -> void;
	auto draw_submit_button(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) const -> void;
	auto draw_notice(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) const -> void;

	Settings*         m_settings;
	MasterKey*        m_master_key;
	const Fonts*      m_fonts;
	const Assets*     m_assets;
	const os::Window* m_window;
	CommandQueue*     m_commands;
	const Texture*    m_app_icon = nullptr;

	bool  m_active           = false;
	bool  m_setup            = false;
	bool  m_wrong_password   = false;
	bool  m_passwords_differ = false;
	bool  m_setup_failed     = false;
	float m_shake_seconds    = 0.0f;
	float m_backdrop         = 0.0f;
	float m_content          = 0.0f;
	float m_spin             = 0.0f;

	std::future<std::unique_ptr<DerivedKey>> m_derivation;

	TextInput m_fields[2];
};
