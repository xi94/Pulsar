#pragma once

#include <optional>
#include <string_view>

#include "platform/installation.h"
#include "ui/text_input.h"
#include "ui/widget.h"

class Assets;
struct Fonts;
class Texture;
class Window;
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
	SetupScreen(SetupMode t_mode, const Settings &t_settings, const Fonts &t_fonts, const Fonts &t_heading_fonts,
				const Fonts &t_title_fonts, const Assets &t_assets, Window &t_window);

	void set_app_icon(const Texture *t_icon)
	{
		m_app_icon = t_icon;
	}

	std::optional<SetupOutcome> outcome() const
	{
		return m_outcome;
	}

	void update(float t_delta_seconds) override;
	void draw(DrawList &t_draw_list) override;

	bool on_pointer_down(Vec2 t_point) override;
	bool on_pointer_move(Vec2 t_point) override;
	bool on_pointer_up(Vec2 t_point) override;
	bool on_key_down(u32 t_key) override;
	bool on_char(u32 t_character) override;

	CursorKind cursor() const override;

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

	static constexpr u32 option_count = 3;
	static constexpr u32 hit_count = static_cast<u32>(Hit::Count);

	struct Reveal {
		float alpha;
		float rise;
	};

	struct PageDraw {
		Vec2 offset;
		float alpha;
		float seconds;
	};

	void go_to(Page t_page, bool t_forward);
	void start_install();
	void start_uninstall();
	void open_installed_app();
	void finish(SetupOutcome t_outcome);
	void activate(Hit t_hit);
	void go_back();
	bool has_footer(Page t_page) const;
	bool is_option_page(Page t_page) const;
	bool is_busy() const;

	Rect content_rect() const;
	Rect footer_rect() const;
	Rect footer_button(bool t_right, std::string_view t_label) const;
	std::string_view primary_label(Page t_page) const;
	std::string_view secondary_label(Page t_page) const;
	Rect header_icon_rect(Page t_page) const;
	Rect choice_rect(u32 t_choice) const;
	Rect field_rect() const;
	Rect field_text_rect() const;
	Rect browse_rect() const;
	Rect option_rect(Page t_page, u32 t_option) const;
	Rect delete_data_rect() const;
	Rect installed_box_rect() const;
	Rect open_folder_rect() const;
	float location_heading_top() const;
	Hit hit_at(Vec2 t_point) const;

	Reveal reveal(float t_seconds, u32 t_order) const;
	void draw_page(DrawList &t_draw_list, Page t_page, const PageDraw &t_draw);
	void draw_header(DrawList &t_draw_list, Page t_page, const PageDraw &t_draw) const;
	void draw_choice(DrawList &t_draw_list, u32 t_choice, const PageDraw &t_draw) const;
	void draw_location(DrawList &t_draw_list, const PageDraw &t_draw);
	void draw_check_row(DrawList &t_draw_list, Rect t_row, std::string_view t_label, float t_checked, float t_hover,
						float t_alpha, Color t_fill) const;
	void draw_options(DrawList &t_draw_list, Page t_page, const PageDraw &t_draw, u32 t_first_order) const;
	void draw_confirm(DrawList &t_draw_list, const PageDraw &t_draw) const;
	void draw_installed_location(DrawList &t_draw_list, const PageDraw &t_draw) const;
	void draw_status(DrawList &t_draw_list, Page t_page, const PageDraw &t_draw) const;
	void draw_footer(DrawList &t_draw_list, Page t_page, const PageDraw &t_draw) const;
	void draw_app_icon(DrawList &t_draw_list, Rect t_rect, u8 t_alpha) const;

	SetupMode m_mode;
	const Settings &m_settings;
	const Fonts &m_fonts;
	const Fonts &m_heading_fonts;
	const Fonts &m_title_fonts;
	const Assets &m_assets;
	Window &m_window;
	const Texture *m_app_icon = nullptr;

	std::optional<installation::Installed> m_installed;
	installation::Options m_options;
	installation::Job m_job;
	installation::FolderPicker m_picker;
	TextInput m_location;
	std::string_view m_field_error;

	Page m_page = Page::Choose;
	std::optional<Page> m_previous_page;
	Page m_return_page = Page::Choose;
	float m_transition = 1.0f;
	float m_direction = 1.0f;
	float m_page_seconds = 0.0f;
	float m_progress = 0.0f;
	float m_finished_seconds = 0.0f;
	bool m_saving = false;
	bool m_delete_data = false;
	float m_delete_check = 0.0f;

	float m_hover[hit_count]{};
	float m_check[option_count]{};
	Hit m_pressed = Hit::None;
	std::optional<SetupOutcome> m_outcome;
};
