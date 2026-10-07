#pragma once

#include <optional>

#include "core/library.h"
#include "core/types.h"
#include "ui/text_input.h"

enum class CommandType : u8 {
	TOGGLE_APP_MENU,
	TOGGLE_UPDATE_OVERLAY,
	OPEN_UPDATE_OVERLAY,
	OPEN_SETTINGS,
	OPEN_DATA_FOLDER,
	OPEN_PERMISSION_SETTINGS,
	OPEN_SETUP,
	CHECK_FOR_UPDATES,
	OPEN_GAME,
	SAVE_CHANGES,
	REQUEST_NEW_MASTER_PASSWORD,
	VAULT_UNLOCKED,
	VAULT_CREATED,
	SHOW_ACCOUNT_MENU,
	SHOW_TEXT_MENU,
	COPY_USERNAME,
	COPY_PASSWORD,
	EDIT_TEXT,
	UNDO_DELETE,
	TOGGLE_FAVORITE,
	LOCK_VAULT,
	OPEN_ACCOUNT_SEARCH,
	EDIT_ACCOUNT,
	LOGIN_ACCOUNT,
	UNDO_LIBRARY_DELETE,
	TOGGLE_ACCOUNT_FAVORITE,
	EDIT_ACCOUNT_IN_PLACE,
	COPY_ACCOUNT_USERNAME,
	COPY_ACCOUNT_PASSWORD,
	LOCATE_RIOT_CLIENT,
};

struct ArtSource {
	Rect  rect;
	float radius;
	bool  is_icon;
	float border = 0.0f;
	Color border_color{};
	float glow = 0.0f;
	Color glow_color{};
};

struct Command {
	CommandType type;
	i32         index = -1;
	Vec2        position{};
	TextInput*  text_input = nullptr;
	TextEdit    text_edit  = TextEdit::COPY;
	AccountRef  account{};
};

class CommandQueue {
  public:
	auto push(const Command& t_command) -> void
	{
		if (m_count == K_CAPACITY) return;

		m_commands[(m_first + m_count) % K_CAPACITY] = t_command;
		m_count += 1;
	}

	[[nodiscard]] auto pop() -> std::optional<Command>
	{
		if (m_count == 0) return std::nullopt;

		const Command command = m_commands[m_first];
		m_first               = (m_first + 1) % K_CAPACITY;
		m_count -= 1;

		return command;
	}

  private:
	static constexpr u32 K_CAPACITY = 32;

	Command m_commands[K_CAPACITY]{};
	u32     m_first = 0;
	u32     m_count = 0;
};
