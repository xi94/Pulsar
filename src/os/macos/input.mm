#include "os/input.h"

#include "os/macos/macos.h"

namespace {
struct KeyPosition {
	u16     code;
	os::Key key;
};

constexpr KeyPosition K_LETTER_POSITIONS[]{
	{kVK_ANSI_A, os::Key::A}, {kVK_ANSI_B, os::Key::B},         {kVK_ANSI_C, os::Key::C}, {kVK_ANSI_D, os::Key::D}, {kVK_ANSI_E, os::Key::E},
	{kVK_ANSI_F, os::Key::F}, {kVK_ANSI_G, os::Key::G},         {kVK_ANSI_H, os::Key::H}, {kVK_ANSI_I, os::Key::I}, {kVK_ANSI_J, os::Key::J},
	{kVK_ANSI_K, os::Key::K}, {kVK_ANSI_L, os::Key::L},         {kVK_ANSI_M, os::Key::M}, {kVK_ANSI_N, os::Key::N}, {kVK_ANSI_O, os::Key::O},
	{kVK_ANSI_P, os::Key::P}, {kVK_ANSI_Q, os::Key::Q},         {kVK_ANSI_R, os::Key::R}, {kVK_ANSI_S, os::Key::S}, {kVK_ANSI_T, os::Key::T},
	{kVK_ANSI_U, os::Key::U}, {kVK_ANSI_V, os::Key::V},         {kVK_ANSI_W, os::Key::W}, {kVK_ANSI_X, os::Key::X}, {kVK_ANSI_Y, os::Key::Y},
	{kVK_ANSI_Z, os::Key::Z}, {kVK_ANSI_Comma, os::Key::Comma},
};

[[nodiscard]] auto is_held(NSEventModifierFlags t_flag) -> bool
{
	return (NSEvent.modifierFlags & t_flag) != 0;
}

[[nodiscard]] auto key_from_character(NSEvent* t_event) -> os::Key
{
	NSString* characters = t_event.charactersIgnoringModifiers.lowercaseString;
	if (characters.length != 1) return os::Key::None;

	const unichar character = [characters characterAtIndex:0];
	if (character >= 'a' && character <= 'z') return static_cast<os::Key>(static_cast<u32>(os::Key::A) + (character - 'a'));
	if (character == ',') return os::Key::Comma;

	return os::Key::None;
}

[[nodiscard]] auto key_from_position(u16 t_code) -> os::Key
{
	for (const KeyPosition& position : K_LETTER_POSITIONS) {
		if (position.code == t_code) return position.key;
	}

	return os::Key::None;
}
}

namespace os {

auto modifiers() -> Modifiers
{
	return Modifiers{
		.shift     = is_held(NSEventModifierFlagShift),
		.shortcut  = is_held(NSEventModifierFlagCommand),
		.word_step = is_held(NSEventModifierFlagOption),
		.control   = is_held(NSEventModifierFlagControl),
	};
}

auto is_caps_lock_on() -> bool
{
	return is_held(NSEventModifierFlagCapsLock);
}

auto is_primary_button_down() -> bool
{
	return (NSEvent.pressedMouseButtons & 1) != 0;
}

auto double_click_ms() -> u32
{
	return static_cast<u32>(NSEvent.doubleClickInterval * 1000.0);
}

}

namespace os::macos {

auto key_from_event(NSEvent* t_event) -> Key
{
	const bool command = (t_event.modifierFlags & NSEventModifierFlagCommand) != 0;

	switch (t_event.keyCode) {
		case kVK_Delete:
			return Key::Backspace;
		case kVK_Tab:
			return Key::Tab;
		case kVK_Return:
		case kVK_ANSI_KeypadEnter:
			return Key::Enter;
		case kVK_Escape:
			return Key::Escape;
		case kVK_ForwardDelete:
			return Key::Delete;
		case kVK_LeftArrow:
			return command ? Key::Home : Key::Left;
		case kVK_RightArrow:
			return command ? Key::End : Key::Right;
		case kVK_UpArrow:
			return Key::Up;
		case kVK_DownArrow:
			return Key::Down;
		case kVK_Home:
			return Key::Home;
		case kVK_End:
			return Key::End;
		case kVK_PageUp:
			return Key::PageUp;
		case kVK_PageDown:
			return Key::PageDown;
		case kVK_F1:
			return Key::F1;
		case kVK_F2:
			return Key::F2;
		default:
			break;
	}

	const Key typed = key_from_character(t_event);

	return typed != Key::None ? typed : key_from_position(t_event.keyCode);
}

}
