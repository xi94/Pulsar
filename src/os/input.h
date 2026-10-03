#pragma once

#include "core/types.h"

namespace os {

enum class Key : u8 {
	None,
	Backspace,
	Tab,
	Enter,
	Escape,
	Delete,
	Left,
	Right,
	Up,
	Down,
	Home,
	End,
	PageUp,
	PageDown,
	Comma,
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

// shortcut is Ctrl on Windows and Command on macOS; word_step is the key that moves the caret a word at a time (Ctrl or Option).
struct Modifiers {
	bool shift     = false;
	bool shortcut  = false;
	bool word_step = false;
};

enum class InputEventType : u8 {
	MouseDown,
	MouseUp,
	MouseMove,
	MouseWheel,
	RightClick,
	KeyDown,
	Character,
};

struct InputEvent {
	InputEventType type;
	Vec2           position;
	float          wheel_delta;
	Key            key;
	u32            codepoint;
};

[[nodiscard]] auto modifiers() -> Modifiers;
[[nodiscard]] auto is_caps_lock_on() -> bool;
[[nodiscard]] auto is_primary_button_down() -> bool;
[[nodiscard]] auto double_click_ms() -> u32;

}
