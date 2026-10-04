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
		case VK_BACK:
			return Key::Backspace;
		case VK_TAB:
			return Key::Tab;
		case VK_RETURN:
			return Key::Enter;
		case VK_ESCAPE:
			return Key::Escape;
		case VK_DELETE:
			return Key::Delete;
		case VK_LEFT:
			return Key::Left;
		case VK_RIGHT:
			return Key::Right;
		case VK_UP:
			return Key::Up;
		case VK_DOWN:
			return Key::Down;
		case VK_HOME:
			return Key::Home;
		case VK_END:
			return Key::End;
		case VK_PRIOR:
			return Key::PageUp;
		case VK_NEXT:
			return Key::PageDown;
		case VK_OEM_COMMA:
			return Key::Comma;
		case VK_F1:
			return Key::F1;
		case VK_F2:
			return Key::F2;
		default:
			return Key::None;
	}
}

}
