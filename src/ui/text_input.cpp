#include "ui/text_input.h"

#include <algorithm>
#include <cstring>

#include <Windows.h>

#include "gfx/font.h"
#include "ui/draw_list.h"
#include "ui/text.h"

namespace {
constexpr float kCaretBlinkPeriodSeconds = 1.0f;
constexpr float kCaretWidth = 1.5f;
constexpr float kTextPadding = 8.0f;

bool IsPrintableAscii(u32 character)
{
	return character >= 0x20 && character <= 0x7E;
}

// CF_TEXT rather than the wide variant, since this buffer is already plain ASCII.
void CopyToClipboard(std::string_view text)
{
	if (text.empty() || !OpenClipboard(nullptr)) return;

	EmptyClipboard();

	const HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, static_cast<SIZE_T>(text.size()) + 1);
	if (memory != nullptr) {
		auto *pDestination = static_cast<char *>(GlobalLock(memory));
		if (pDestination != nullptr) {
			std::memcpy(pDestination, text.data(), text.size());
			pDestination[text.size()] = '\0';
			GlobalUnlock(memory);
			SetClipboardData(CF_TEXT, memory);
		}
	}

	CloseClipboard();
}

bool IsKeyDown(int virtualKey)
{
	return (GetKeyState(virtualKey) & 0x8000) != 0;
}
} // namespace

void CTextInput::Init(std::string_view initialValue)
{
	m_bFocused = false;
	m_flCaretBlinkSeconds = 0.0f;

	SetValue(initialValue);
}

void CTextInput::SetValue(std::string_view value)
{
	const auto length = static_cast<u32>(std::min<u64>(value.size(), kTextInputCapacity));
	std::memcpy(m_szBuffer, value.data(), length);

	m_nLength = length;
	m_nCursor = length;
	m_nSelectionAnchor = -1;
}

CTextInput::Range CTextInput::SelectionRange() const
{
	const auto anchor = static_cast<u32>(m_nSelectionAnchor);

	return Range{std::min(anchor, m_nCursor), std::max(anchor, m_nCursor)};
}

void CTextInput::EraseRange(u32 start, u32 count)
{
	if (count == 0 || start >= m_nLength) return;

	count = std::min(count, m_nLength - start);
	std::memmove(m_szBuffer + start, m_szBuffer + start + count, m_nLength - start - count);

	m_nLength -= count;
	m_nCursor = start;
	m_nSelectionAnchor = -1;
}

void CTextInput::DeleteSelection()
{
	if (!HasSelection()) return;

	const Range range = SelectionRange();
	EraseRange(range.Start, range.End - range.Start);
}

void CTextInput::InsertText(std::string_view text)
{
	DeleteSelection();

	char accepted[kTextInputCapacity];
	u32 acceptedCount = 0;

	for (u64 i = 0; i < text.size() && m_nLength + acceptedCount < kTextInputCapacity; i += 1) {
		const auto c = static_cast<unsigned char>(text.data()[i]);
		if (IsPrintableAscii(c)) {
			accepted[acceptedCount] = static_cast<char>(c);
			acceptedCount += 1;
		}
	}

	if (acceptedCount == 0) return;

	std::memmove(m_szBuffer + m_nCursor + acceptedCount, m_szBuffer + m_nCursor, m_nLength - m_nCursor);
	std::memcpy(m_szBuffer + m_nCursor, accepted, acceptedCount);

	m_nLength += acceptedCount;
	m_nCursor += acceptedCount;
	m_nSelectionAnchor = -1;
}

void CTextInput::MoveCursorTo(u32 target, bool extendSelection)
{
	if (extendSelection) {
		if (m_nSelectionAnchor < 0) {
			m_nSelectionAnchor = static_cast<i32>(m_nCursor);
		}
	} else {
		m_nSelectionAnchor = -1;
	}

	m_nCursor = target;
}

void CTextInput::OnChar(u32 character)
{
	if (!m_bFocused || !IsPrintableAscii(character)) return;

	if (!HasSelection() && m_nLength >= kTextInputCapacity) return;

	const char typed = static_cast<char>(character);
	InsertText(std::string_view{&typed, 1});

	// Typing keeps the caret solid rather than leaving it mid-blink.
	m_flCaretBlinkSeconds = 0.0f;
}

void CTextInput::PasteFromClipboard()
{
	if (!OpenClipboard(nullptr)) return;

	const HANDLE handle = GetClipboardData(CF_TEXT);
	if (handle != nullptr) {
		const auto *pData = static_cast<const char *>(GlobalLock(handle));
		if (pData != nullptr) {
			InsertText(std::string_view{pData});
			GlobalUnlock(handle);
		}
	}

	CloseClipboard();
}

bool CTextInput::HandleControlKey(u32 keyCode)
{
	switch (keyCode) {
		case 'A':
			m_nSelectionAnchor = 0;
			m_nCursor = m_nLength;
			return true;

		case 'E':
			MoveCursorTo(m_nLength, false);
			return true;

		case 'C':
			if (HasSelection()) {
				const Range range = SelectionRange();
				CopyToClipboard(std::string_view{m_szBuffer + range.Start, range.End - range.Start});
			}

			return true;

		case 'X':
			if (HasSelection()) {
				const Range range = SelectionRange();
				CopyToClipboard(std::string_view{m_szBuffer + range.Start, range.End - range.Start});
				DeleteSelection();
			}

			return true;

		case 'V':
			PasteFromClipboard();
			return true;

		default:
			return false;
	}
}

void CTextInput::OnKey(u32 keyCode)
{
	if (!m_bFocused) return;

	if (IsKeyDown(VK_CONTROL) && HandleControlKey(keyCode)) {
		m_flCaretBlinkSeconds = 0.0f;
		return;
	}

	const bool shiftDown = IsKeyDown(VK_SHIFT);

	switch (keyCode) {
		case VK_BACK:
			if (HasSelection()) {
				DeleteSelection();
			} else if (m_nCursor > 0) {
				EraseRange(m_nCursor - 1, 1);
			} else {
				return;
			}

			break;

		case VK_DELETE:
			if (HasSelection()) {
				DeleteSelection();
			} else if (m_nCursor < m_nLength) {
				EraseRange(m_nCursor, 1);
			} else {
				return;
			}

			break;

		// Without shift, an active selection collapses to its edge rather than moving the
		// cursor a further character.
		case VK_LEFT: {
			const u32 back = m_nCursor > 0 ? m_nCursor - 1 : 0;
			MoveCursorTo(!shiftDown && HasSelection() ? SelectionRange().Start : back, shiftDown);
			break;
		}

		case VK_RIGHT: {
			const u32 forward = m_nCursor < m_nLength ? m_nCursor + 1 : m_nLength;
			MoveCursorTo(!shiftDown && HasSelection() ? SelectionRange().End : forward, shiftDown);
			break;
		}

		case VK_HOME:
			MoveCursorTo(0, shiftDown);
			break;

		case VK_END:
			MoveCursorTo(m_nLength, shiftDown);
			break;

		default:
			return;
	}

	m_flCaretBlinkSeconds = 0.0f;
}

void CTextInput::Update(float deltaSeconds)
{
	m_flCaretBlinkSeconds += deltaSeconds;

	if (m_flCaretBlinkSeconds > kCaretBlinkPeriodSeconds) {
		m_flCaretBlinkSeconds -= kCaretBlinkPeriodSeconds;
	}
}

void CTextInput::Draw(CDrawList &drawList, const CFont &font, float x, float y, float w, float h, Color textColor,
					  Color caretColor, bool masked) const
{
	char maskBuffer[kTextInputCapacity];
	std::string_view value = GetValue();

	if (masked) {
		std::memset(maskBuffer, '*', m_nLength);
		value = std::string_view{maskBuffer, m_nLength};
	}

	const float textX = x + kTextPadding;

	if (m_bFocused && HasSelection()) {
		const Range range = SelectionRange();
		const float startX = textX + TextWidth(font, std::string_view{value.data(), range.Start});
		const float endX = textX + TextWidth(font, std::string_view{value.data(), range.End});

		drawList.AddRectFilled(startX, y + 4.0f, endX - startX, h - 8.0f, ColorScaleAlpha(caretColor, 70));
	}

	// Centres the value's visual middle, not its ascent - the descent is negative, and leaving
	// it out sits every typed value visibly low in its box.
	const float baselineY = y + h * 0.5f + (font.GetAscent() + font.GetDescent()) * 0.5f;
	DrawText(drawList, font, textX, baselineY, value, textColor);

	// Half the blink period on, half off.
	if (!m_bFocused || m_flCaretBlinkSeconds >= kCaretBlinkPeriodSeconds * 0.5f) return;

	const float caretX = textX + TextWidth(font, std::string_view{value.data(), m_nCursor});
	drawList.AddRectFilled(std::min(caretX, x + w - kCaretWidth), y + 4.0f, kCaretWidth, h - 8.0f, caretColor);
}
