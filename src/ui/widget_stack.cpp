#include "ui/widget_stack.h"

#include "core/profiler.h"

#include <algorithm>

namespace {
// Visits entries in visual top-to-bottom order, stopping as soon as the visitor returns true -
// the "first consumer wins" shape every dispatch method needs. Update wants the same order but
// never stops early, so its visitor always returns false.
template <typename TVisitor>
bool IterateTopDown(std::vector<CWidgetStack::Entry> &entries, TVisitor &&visitor)
{
	for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
		if (it->m_bAlwaysTopmost && visitor(*it->m_pWidget)) return true;
	}

	for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
		if (!it->m_bAlwaysTopmost && visitor(*it->m_pWidget)) return true;
	}

	return false;
}

ECursorKind FirstCursorOpinion(const std::vector<CWidgetStack::Entry> &entries, bool wantTopmost)
{
	for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
		if (it->m_bAlwaysTopmost != wantTopmost || !it->m_pWidget->m_bVisible) continue;

		const ECursorKind cursor = it->m_pWidget->GetDesiredCursor();
		if (cursor != ECursorKind::Arrow) return cursor;
	}

	return ECursorKind::Arrow;
}
} // namespace

CWidget *CWidgetStack::Push(std::unique_ptr<CWidget> widget, bool alwaysTopmost)
{
	CWidget *pRaw = widget.get();
	m_aWidgets.push_back(Entry{std::move(widget), alwaysTopmost});

	return pRaw;
}

void CWidgetStack::Remove(CWidget *pWidget)
{
	std::erase_if(m_aWidgets, [pWidget](const Entry &entry) { return entry.m_pWidget.get() == pWidget; });
}

void CWidgetStack::SetRealMousePosition(float x, float y)
{
	m_flRealMouseX = x;
	m_flRealMouseY = y;
}

void CWidgetStack::Update(float deltaSeconds)
{
	bool somethingAboveIsBlocking = false;

	IterateTopDown(m_aWidgets, [&](CWidget &widget) {
		widget.SetMouseGated(somethingAboveIsBlocking, m_flRealMouseX, m_flRealMouseY);
		widget.Update(deltaSeconds);
		somethingAboveIsBlocking |= widget.IsBlocking();

		return false;
	});
}

void CWidgetStack::Draw(CDrawList &drawList)
{
	for (const Entry &entry : m_aWidgets) {
		if (!entry.m_bAlwaysTopmost && entry.m_pWidget->m_bVisible) {
			entry.m_pWidget->Draw(drawList);
		}
	}

	for (const Entry &entry : m_aWidgets) {
		if (entry.m_bAlwaysTopmost && entry.m_pWidget->m_bVisible) {
			entry.m_pWidget->Draw(drawList);
		}
	}
}

bool CWidgetStack::DispatchPointerDown(float x, float y)
{
	return IterateTopDown(m_aWidgets, [&](CWidget &widget) { return widget.m_bVisible && widget.OnPointerDown(x, y); });
}

bool CWidgetStack::DispatchPointerMove(float x, float y)
{
	return IterateTopDown(m_aWidgets, [&](CWidget &widget) { return widget.m_bVisible && widget.OnPointerMove(x, y); });
}

bool CWidgetStack::DispatchPointerUp(float x, float y)
{
	return IterateTopDown(m_aWidgets, [&](CWidget &widget) { return widget.m_bVisible && widget.OnPointerUp(x, y); });
}

bool CWidgetStack::DispatchRightPointerUp(float x, float y)
{
	return IterateTopDown(m_aWidgets,
						  [&](CWidget &widget) { return widget.m_bVisible && widget.OnRightPointerUp(x, y); });
}

bool CWidgetStack::DispatchScroll(float x, float y, float wheelDelta)
{
	return IterateTopDown(m_aWidgets,
						  [&](CWidget &widget) { return widget.m_bVisible && widget.OnScroll(x, y, wheelDelta); });
}

bool CWidgetStack::DispatchKeyDown(u32 keyCode)
{
	return IterateTopDown(m_aWidgets, [&](CWidget &widget) { return widget.m_bVisible && widget.OnKeyDown(keyCode); });
}

bool CWidgetStack::DispatchChar(u32 character)
{
	return IterateTopDown(m_aWidgets, [&](CWidget &widget) { return widget.m_bVisible && widget.OnChar(character); });
}

ECursorKind CWidgetStack::GetDesiredCursor() const
{
	const ECursorKind topmostCursor = FirstCursorOpinion(m_aWidgets, true);

	return topmostCursor != ECursorKind::Arrow ? topmostCursor : FirstCursorOpinion(m_aWidgets, false);
}
