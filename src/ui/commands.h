#pragma once

#include <optional>

#include "core/library.h"
#include "core/types.h"
#include "ui/text_input.h"

enum class CommandType : u8 {
	ToggleAppMenu,
	ToggleUpdateOverlay,
	OpenUpdateOverlay,
	OpenSettings,
	OpenDataFolder,
	OpenSetup,
	CheckForUpdates,
	OpenGame,
	SaveChanges,
	RequestNewMasterPassword,
	VaultUnlocked,
	VaultCreated,
	ShowAccountMenu,
	ShowTextMenu,
	CopyUsername,
	CopyPassword,
	EditText,
	UndoDelete,
	ToggleFavorite,
	LockVault,
	OpenAccountSearch,
	EditAccount,
	LoginAccount,
	CopyAccountUsername,
	CopyAccountPassword,
};

struct ArtSource {
	Rect rect;
	float radius;
	bool is_icon;
	float border = 0.0f;
	Color border_color{};
	float glow = 0.0f;
	Color glow_color{};
};

struct Command {
	CommandType type;
	i32 index = -1;
	Vec2 position{};
	TextInput *text_input = nullptr;
	TextEdit text_edit = TextEdit::Copy;
	AccountRef account{};
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
