#include "os/input.h"

#include "os/win32/win32.h"

namespace {
[[nodiscard]] auto is_held(int t_virtual_key) -> bool
{
	return (GetKeyState(t_virtual_key) & 0x8000) != 0;
}
}

namespace os {

auto modifiers() -> Modifiers
{
	const bool control = is_held(VK_CONTROL);

	return Modifiers{.shift = is_held(VK_SHIFT), .shortcut = control, .word_step = control, .control = control};
}

auto is_caps_lock_on() -> bool
{
	return (GetKeyState(VK_CAPITAL) & 1) != 0;
}

auto is_primary_button_down() -> bool
{
	return is_held(VK_LBUTTON);
}

auto double_click_ms() -> u32
{
	return GetDoubleClickTime();
}

}

namespace os::win32 {

auto key_from_virtual_key(WPARAM t_virtual_key) -> Key
{
	if (t_virtual_key >= 'A' && t_virtual_key <= 'Z') {
		return static_cast<Key>(static_cast<u32>(Key::A) + static_cast<u32>(t_virtual_key - 'A'));
	}

	switch (t_virtual_key) {
		case VK_BACK: {
			return Key::BACKSPACE;
		}

		case VK_TAB: {
			return Key::TAB;
		}

		case VK_RETURN: {
			return Key::ENTER;
		}

		case VK_ESCAPE: {
			return Key::ESCAPE;
		}

		case VK_DELETE: {
			return Key::FORWARD_DELETE;
		}

		case VK_LEFT: {
			return Key::LEFT;
		}

		case VK_RIGHT: {
			return Key::RIGHT;
		}

		case VK_UP: {
			return Key::UP;
		}

		case VK_DOWN: {
			return Key::DOWN;
		}

		case VK_HOME: {
			return Key::HOME;
		}

		case VK_END: {
			return Key::END;
		}

		case VK_PRIOR: {
			return Key::PAGE_UP;
		}

		case VK_NEXT: {
			return Key::PAGE_DOWN;
		}

		case VK_OEM_COMMA: {
			return Key::COMMA;
		}

		case VK_F1: {
			return Key::F1;
		}

		case VK_F2: {
			return Key::F2;
		}

		default: {
			return Key::NONE;
		}
	}
}

}
