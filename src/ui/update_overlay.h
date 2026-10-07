#pragma once

#include <string_view>

#include "core/updater.h"
#include "ui/controls.h"
#include "ui/scrollable.h"
#include "ui/widget.h"

class Assets;
struct Fonts;
namespace os {
class Window;
}
struct Settings;

class UpdateOverlay : public Widget {
  public:
	UpdateOverlay(Updater* t_updater, const Settings* t_settings, const Fonts* t_fonts, const Assets* t_assets, const os::Window* t_window);

	auto open() -> void;
	auto begin_check() -> void;
	auto close() -> void;
	auto show_release_notes(std::string_view t_version, std::string_view t_notes) -> void;

	[[nodiscard]] auto is_open() const -> bool
	{
		return m_open;
	}

	[[nodiscard]] auto is_shown() const -> bool
	{
		return m_open_amount > 0.01f;
	}

	[[nodiscard]] auto wants_status() const -> bool
	{
		return m_check_requested || m_status_linger > 0.0f;
	}

	[[nodiscard]] auto shown_stage() const -> UpdateStage
	{
		return m_shown_stage;
	}

	[[nodiscard]] auto is_showing_release_notes() const -> bool
	{
		return m_showing_release;
	}

	auto update(float t_delta_seconds) -> void override;
	auto draw(DrawList* t_draw_list) -> void override;

	auto on_pointer_down(Vec2 t_point) -> bool override;
	auto on_pointer_move(Vec2 t_point) -> bool override;
	auto on_pointer_up(Vec2 t_point) -> bool override;
	auto on_scroll(Vec2 t_point, float t_wheel_delta) -> bool override;
	auto on_key_down(os::Key t_key) -> bool override;

	[[nodiscard]] auto is_blocking() const -> bool override
	{
		return is_shown();
	}

	[[nodiscard]] auto cursor() const -> CursorKind override;

  private:
	enum class Action : u8 {
		NONE,
		CHECK,
		DOWNLOAD,
		CANCEL,
		CLOSE,
		RELEASES,
	};

	struct Button {
		std::string_view      label;
		Action                action = Action::NONE;
		controls::ButtonStyle style  = controls::ButtonStyle::NEUTRAL;
	};

	struct Content {
		char   title[64]{};
		char   detail[192]{};
		bool   spinning        = false;
		bool   detail_is_error = false;
		bool   progress        = false;
		bool   notes           = false;
		Button primary;
		Button secondary;
	};

	struct Layout {
		Rect  popover;
		float title_top;
		float detail_top;
		Rect  progress;
		float caption_top;
		Rect  notes;
		Rect  primary;
		Rect  secondary;
	};

	[[nodiscard]] auto describe() const -> Content;
	[[nodiscard]] auto content_key() const -> u32;
	[[nodiscard]] auto content_height(const Content& t_content) const -> float;
	[[nodiscard]] auto notes_box_height() const -> float;
	[[nodiscard]] auto notes_content_height() const -> float;
	[[nodiscard]] auto shown_notes() const -> std::string_view;
	[[nodiscard]] auto anchored_x() const -> float;
	[[nodiscard]] auto layout(const Content& t_content) const -> Layout;
	[[nodiscard]] auto notes_scroll(const Layout& t_layout) const -> ScrollGeometry;
	auto run(Action t_action) -> void;
	auto advance_shown_stage(float t_delta_seconds) -> void;

	auto draw_title(DrawList* t_draw_list, const Content& t_content, Rect t_line, u8 t_alpha) const -> void;
	auto draw_notes(DrawList* t_draw_list, Rect t_box, const ScrollGeometry& t_scroll, u8 t_alpha) const -> void;
	auto draw_progress(DrawList* t_draw_list, const Layout& t_layout, u8 t_alpha) const -> void;

	Updater*          m_updater;
	const Settings*   m_settings;
	const Fonts*      m_fonts;
	const Assets*     m_assets;
	const os::Window* m_window;

	bool        m_open            = false;
	float       m_open_amount     = 0.0f;
	float       m_height          = 0.0f;
	float       m_content_fade    = 1.0f;
	float       m_progress        = 0.0f;
	float       m_spin            = 0.0f;
	u32         m_content_key     = 0;
	UpdateStage m_shown_stage     = UpdateStage::IDLE;
	float       m_shown_seconds   = 0.0f;
	bool        m_check_requested = false;
	float       m_status_linger   = 0.0f;
	bool        m_pressed_outside = false;
	Scrollable  m_notes_scroll;

	bool m_showing_release = false;
	char m_release_version[32]{};
	char m_release_notes[1024]{};
};
