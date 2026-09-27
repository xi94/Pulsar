#pragma once

#include <optional>

#include "core/types.h"
#include "ui/text_input.h"

enum class CommandType : u8 {
	toggle_app_menu,
	toggle_update_overlay,
	open_update_overlay,
	open_settings,
	open_data_folder,
	check_for_updates,
	open_game,
	save_changes,
	request_new_master_password,
	vault_unlocked,
	vault_created,
	show_account_menu,
	show_text_menu,
	copy_username,
	copy_password,
	edit_text,
	undo_delete,
	toggle_favorite,
	lock_vault,
};

struct Command {
	CommandType type;
	i32 index = -1;
	Vec2 position{};
	TextInput *text_input = nullptr;
	TextEdit text_edit = TextEdit::copy;
};

class CommandQueue {
  public:
	void push(const Command &t_command)
	{
		if (m_count == capacity) return;

		m_commands[(m_first + m_count) % capacity] = t_command;
		m_count += 1;
	}

	std::optional<Command> pop()
	{
		if (m_count == 0) return std::nullopt;

		const Command command = m_commands[m_first];
		m_first = (m_first + 1) % capacity;
		m_count -= 1;

		return command;
	}

  private:
	static constexpr u32 capacity = 32;

	Command m_commands[capacity]{};
	u32 m_first = 0;
	u32 m_count = 0;
};
