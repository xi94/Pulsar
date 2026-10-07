#pragma once

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

// shortcut is Ctrl on Windows and Command on macOS; word_step is the key that moves the caret a word at a time (Ctrl or Option);
// control is the physical Ctrl key on both.
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
