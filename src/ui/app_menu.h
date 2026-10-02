#pragma once

#include <string_view>

#include "ui/commands.h"
#include "ui/widget.h"

class Assets;
struct Fonts;

class AppMenu : public Widget {
  public:
	AppMenu(const Fonts* t_fonts, const Assets* t_assets, CommandQueue* t_commands);

	auto open(bool t_unlocked, std::string_view t_update_status) -> void;
	auto close() -> void;

	[[nodiscard]] auto is_open() const -> bool
	{
		return m_open;
	}

	auto update(float t_delta_seconds) -> void override;
	auto draw(DrawList* t_draw_list) -> void override;

	auto on_pointer_up(Vec2 t_point) -> bool override;

	[[nodiscard]] auto is_blocking() const -> bool override
	{
		return m_open_amount > 0.01f;
	}

	[[nodiscard]] auto cursor() const -> CursorKind override;

  private:
	static constexpr u32 K_MAX_ITEMS = 8;

	[[nodiscard]] auto is_enabled(u32 t_item) const -> bool;

	const Fonts*  m_fonts;
	const Assets* m_assets;
	CommandQueue* m_commands;

	bool             m_open     = false;
	bool             m_unlocked = false;
	std::string_view m_update_status;
	float            m_open_amount = 0.0f;
	float            m_item_hover[K_MAX_ITEMS]{};
};
