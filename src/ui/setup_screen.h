#pragma once

#include <optional>
#include <string_view>

#include "os/installation.h"
#include "os/path_picker.h"
#include "ui/text_input.h"
#include "ui/widget.h"

class Assets;
struct Fonts;
class Texture;
namespace os {
class Window;
}
struct Settings;

enum class SetupMode : u8 {
	FirstRun,
	Manage,
	Uninstall,
};

enum class SetupOutcome : u8 {
	Closed,
	Portable,
	Quit,
};

class SetupScreen : public Widget {
  public:
	SetupScreen(SetupMode       t_mode,
	            const Settings* t_settings,
	            const Fonts*    t_fonts,
	            const Fonts*    t_heading_fonts,
	            const Fonts*    t_title_fonts,
	            const Assets*   t_assets,
	            os::Window*     t_window);

	auto set_app_icon(const Texture* t_icon) -> void
	{
		m_app_icon = t_icon;
	}

	[[nodiscard]] auto outcome() const -> std::optional<SetupOutcome>
	{
		return m_outcome;
	}

	auto update(float t_delta_seconds) -> void override;
	auto draw(DrawList* t_draw_list) -> void override;

	auto on_pointer_down(Vec2 t_point) -> bool override;
	auto on_pointer_move(Vec2 t_point) -> bool override;
	auto on_pointer_up(Vec2 t_point) -> bool override;
	auto on_key_down(os::Key t_key) -> bool override;
	auto on_char(u32 t_character) -> bool override;

	[[nodiscard]] auto cursor() const -> CursorKind override;

  private:
	enum class Page : u8 {
		Choose,
		Location,
		Working,
		Done,
		Failed,
		Manage,
		ConfirmUninstall,
	};

	enum class Hit : u8 {
		None,
		Install,
		Portable,
		Field,
		Browse,
		Option0,
		Option1,
		Option2,
		DeleteData,
		OpenFolder,
		Secondary,
		Primary,
		Count,
	};

	static constexpr u32 K_OPTION_COUNT = 3;
	static constexpr u32 K_HIT_COUNT    = static_cast<u32>(Hit::Count);

	struct Reveal {
		float alpha;
		float rise;
	};

	struct PageDraw {
		Vec2  offset;
		float alpha;
		float seconds;
	};

	auto go_to(Page t_page, bool t_forward) -> void;
	auto start_install() -> void;
	auto start_uninstall() -> void;
	auto open_installed_app() -> void;
	auto finish(SetupOutcome t_outcome) -> void;
	auto activate(Hit t_hit) -> void;
	auto go_back() -> void;
	[[nodiscard]] auto has_footer(Page t_page) const -> bool;
	[[nodiscard]] auto is_option_page(Page t_page) const -> bool;
	[[nodiscard]] auto is_busy() const -> bool;

	[[nodiscard]] auto content_rect() const -> Rect;
	[[nodiscard]] auto footer_rect() const -> Rect;
	[[nodiscard]] auto footer_button(bool t_right, std::string_view t_label) const -> Rect;
	[[nodiscard]] auto primary_label(Page t_page) const -> std::string_view;
	[[nodiscard]] auto secondary_label(Page t_page) const -> std::string_view;
	[[nodiscard]] auto header_icon_rect(Page t_page) const -> Rect;
	[[nodiscard]] auto choice_rect(u32 t_choice) const -> Rect;
	[[nodiscard]] auto field_rect() const -> Rect;
	[[nodiscard]] auto field_text_rect() const -> Rect;
	[[nodiscard]] auto browse_rect() const -> Rect;
	[[nodiscard]] auto option_rect(Page t_page, u32 t_option) const -> Rect;
	[[nodiscard]] auto delete_data_rect() const -> Rect;
	[[nodiscard]] auto installed_box_rect() const -> Rect;
	[[nodiscard]] auto open_folder_rect() const -> Rect;
	[[nodiscard]] auto location_heading_top() const -> float;
	[[nodiscard]] auto hit_at(Vec2 t_point) const -> Hit;

	[[nodiscard]] auto reveal(float t_seconds, u32 t_order) const -> Reveal;
	auto draw_page(DrawList* t_draw_list, Page t_page, const PageDraw& t_draw) -> void;
	auto draw_header(DrawList* t_draw_list, Page t_page, const PageDraw& t_draw) const -> void;
	auto draw_choice(DrawList* t_draw_list, u32 t_choice, const PageDraw& t_draw) const -> void;
	auto draw_location(DrawList* t_draw_list, const PageDraw& t_draw) -> void;
	auto draw_check_row(DrawList* t_draw_list, Rect t_row, std::string_view t_label, float t_checked, float t_hover, float t_alpha, Color t_fill) const -> void;
	auto draw_options(DrawList* t_draw_list, Page t_page, const PageDraw& t_draw, u32 t_first_order) const -> void;
	auto draw_confirm(DrawList* t_draw_list, const PageDraw& t_draw) const -> void;
	auto draw_installed_location(DrawList* t_draw_list, const PageDraw& t_draw) const -> void;
	auto draw_status(DrawList* t_draw_list, Page t_page, const PageDraw& t_draw) const -> void;
	auto draw_footer(DrawList* t_draw_list, Page t_page, const PageDraw& t_draw) const -> void;
	auto draw_app_icon(DrawList* t_draw_list, Rect t_rect, u8 t_alpha) const -> void;

	SetupMode       m_mode;
	const Settings* m_settings;
	const Fonts*    m_fonts;
	const Fonts*    m_heading_fonts;
	const Fonts*    m_title_fonts;
	const Assets*   m_assets;
	os::Window*     m_window;
	const Texture*  m_app_icon = nullptr;

	std::optional<os::installation::Installed> m_installed;
	os::installation::Options                  m_options;
	os::installation::Job                      m_job;
	os::PathPicker                             m_picker;
	TextInput                                  m_location;
	std::string_view                           m_field_error;

	Page                m_page = Page::Choose;
	std::optional<Page> m_previous_page;
	Page                m_return_page      = Page::Choose;
	float               m_transition       = 1.0f;
	float               m_direction        = 1.0f;
	float               m_page_seconds     = 0.0f;
	float               m_progress         = 0.0f;
	float               m_finished_seconds = 0.0f;
	bool                m_saving           = false;
	bool                m_delete_data      = false;
	float               m_delete_check     = 0.0f;

	float                       m_hover[K_HIT_COUNT]{};
	float                       m_check[K_OPTION_COUNT]{};
	Hit                         m_pressed = Hit::None;
	std::optional<SetupOutcome> m_outcome;
};
