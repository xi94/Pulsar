#pragma once

#include "core/enum_array.h"
#include "core/types.h"

namespace os {

enum class Key : u8 {
	NONE,
	BACKSPACE,
	TAB,
	ENTER,
	ESCAPE,
	FORWARD_DELETE,
	LEFT,
	RIGHT,
	UP,
	DOWN,
	HOME,
	END,
	PAGE_UP,
	PAGE_DOWN,
	COMMA,
	F1,
	F2,
	A,
	B,
	C,
	D,
	E,
	F,
	G,
	H,
	I,
	J,
	K,
	L,
	M,
	N,
	O,
	P,
	Q,
	R,
	S,
	T,
	U,
	V,
	W,
	X,
	Y,
	Z,
};

// The letter keys run from A to Z in alphabetical order, so a lowercase letter and its key convert directly.
static_assert(enum_index(Key::Z) - enum_index(Key::A) == 'z' - 'a');

[[nodiscard]] constexpr auto is_letter_key(Key t_key) -> bool
{
	return t_key >= Key::A && t_key <= Key::Z;
}

[[nodiscard]] constexpr auto letter_key(char t_letter) -> Key
{
	return enum_at<Key>(enum_index(Key::A) + static_cast<u32>(t_letter - 'a'));
}

[[nodiscard]] constexpr auto key_letter(Key t_key) -> char
{
	return static_cast<char>('a' + (enum_index(t_key) - enum_index(Key::A)));
}

// shortcut is Ctrl on Windows and Command on macOS; word_step is the key that moves the caret a word at a time (Ctrl or Option); control is the physical Ctrl key on both.
struct Modifiers {
	bool shift     = false;
	bool shortcut  = false;
	bool word_step = false;
	bool control   = false;
};

enum class InputEventType : u8 {
	MOUSE_DOWN,
	MOUSE_UP,
	MOUSE_MOVE,
	MOUSE_WHEEL,
	RIGHT_CLICK,
	KEY_DOWN,
	CHARACTER,
	MENU_ITEM,
};

struct InputEvent {
	InputEventType type;
	Vec2           position;
	float          wheel_delta;
	Key            key;
	u32            codepoint;
	u32            menu_item;
};

[[nodiscard]] auto modifiers() -> Modifiers;
[[nodiscard]] auto is_caps_lock_on() -> bool;
[[nodiscard]] auto is_primary_button_down() -> bool;
[[nodiscard]] auto double_click_ms() -> u32;

}
