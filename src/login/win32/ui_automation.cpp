#include "login/win32/ui_automation.h"

#include <algorithm>
#include <chrono>
#include <concepts>
#include <memory>
#include <ranges>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <Windows.h>
#include <TlHelp32.h>

#include "core/debug_log.h"
#include "core/thread_util.h"

using Microsoft::WRL::ComPtr;

namespace {
constexpr const char* K_LOG_CATEGORY           = "uia";
constexpr LONG        K_MIN_REAL_WINDOW_WIDTH  = 50;
constexpr u32         K_MAX_TEXT_WALK_DEPTH    = 4;
constexpr u32         K_MAX_TEXT_WALK_ELEMENTS = 64;
constexpr PROPERTYID  K_BUTTON_PROPERTIES[]{UIA_NamePropertyId, UIA_IsEnabledPropertyId, UIA_BoundingRectanglePropertyId, UIA_IsInvokePatternAvailablePropertyId};

class VariantString {
  public:
	explicit VariantString(const wchar_t* t_text)
	{
		VariantInit(&m_value);
		m_value.vt      = VT_BSTR;
		m_value.bstrVal = SysAllocString(t_text);
	}

	~VariantString()
	{
		VariantClear(&m_value);
	}

	VariantString(const VariantString&)                    = delete;
	auto operator=(const VariantString&) -> VariantString& = delete;

	VARIANT m_value;

  private:
};

[[nodiscard]] auto variant_from_control_type(CONTROLTYPEID t_control_type) -> VARIANT
{
	VARIANT value;
	VariantInit(&value);
	value.vt   = VT_I4;
	value.lVal = t_control_type;

	return value;
}

[[nodiscard]] auto process_tree(DWORD t_root_process_id) -> std::vector<DWORD>
{
	std::vector<DWORD> tree{t_root_process_id};

	const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == INVALID_HANDLE_VALUE) return tree;

	std::vector<std::pair<DWORD, DWORD>> parent_child_pairs;
	PROCESSENTRY32W                      entry{.dwSize = sizeof(entry)};

	if (Process32FirstW(snapshot, &entry)) {
		do {
			parent_child_pairs.emplace_back(entry.th32ParentProcessID, entry.th32ProcessID);
		} while (Process32NextW(snapshot, &entry));
	}

	CloseHandle(snapshot);

	const auto in_tree = [&tree](DWORD t_process_id) { return std::ranges::find(tree, t_process_id) != tree.end(); };

	for (bool grew = true; grew;) {
		grew = false;

		for (const auto& [parent, child] : parent_child_pairs) {
			if (in_tree(parent) && !in_tree(child)) {
				tree.push_back(child);
				grew = true;
			}
		}
	}

	return tree;
}

struct WindowSearch {
	const std::vector<DWORD>* process_ids;
	HWND                      found;
};

auto CALLBACK find_top_level_window_proc(HWND t_window, LPARAM t_search) -> BOOL
{
	auto* search = reinterpret_cast<WindowSearch*>(t_search);

	DWORD process_id = 0;
	GetWindowThreadProcessId(t_window, &process_id);

	if (std::ranges::find(*search->process_ids, process_id) == search->process_ids->end()) return TRUE;
	if (GetWindow(t_window, GW_OWNER) != nullptr || !IsWindowVisible(t_window)) return TRUE;

	RECT rect;
	if (GetWindowRect(t_window, &rect) && rect.right - rect.left < K_MIN_REAL_WINDOW_WIDTH) return TRUE;

	search->found = t_window;

	return FALSE;
}

[[nodiscard]] auto run_lookup(const char* t_label, const std::invocable<ComPtr<IUIAutomationElement>*> auto& t_lookup, const std::atomic<bool>* t_cancel) -> ComPtr<IUIAutomationElement>
{
	auto                   result = std::make_shared<ComPtr<IUIAutomationElement>>();
	const debug_log::Scope scope(K_LOG_CATEGORY, "%s", t_label);

	// A busy client can block a provider call for as long as it likes, so only a cancel may walk away from one.
	const bool finished = run_unless_cancelled(
		[result, t_lookup]() {
			const HRESULT com_result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
			t_lookup(result.get());

			if (com_result == S_OK || com_result == S_FALSE) {
				CoUninitialize();
			}
		},
		t_cancel);

	if (!finished) {
		debug_log::write(K_LOG_CATEGORY, "ABANDONED %s - cancelled while the provider was still answering", t_label);
		return nullptr;
	}

	return std::move(*result);
}

template <typename Pattern>
[[nodiscard]] auto pattern_of(const ComPtr<IUIAutomationElement>& t_element, PATTERNID t_pattern_id) -> ComPtr<Pattern>
{
	ComPtr<Pattern> pattern;
	if (FAILED(t_element->GetCurrentPatternAs(t_pattern_id, IID_PPV_ARGS(&pattern)))) return nullptr;

	return pattern;
}

[[nodiscard]] auto cached_button(const ComPtr<IUIAutomationElement>& t_element) -> UiButton
{
	UiButton button{.element = UiElement{t_element}};

	BSTR name = nullptr;
	if (SUCCEEDED(t_element->get_CachedName(&name)) && name != nullptr) {
		button.named = SysStringLen(name) > 0;
		SysFreeString(name);
	}

	BOOL enabled   = FALSE;
	button.enabled = SUCCEEDED(t_element->get_CachedIsEnabled(&enabled)) && enabled;

	if (RECT bounds{}; SUCCEEDED(t_element->get_CachedBoundingRectangle(&bounds))) {
		button.bounds = bounds;
	}

	VARIANT invokable;
	VariantInit(&invokable);

	if (SUCCEEDED(t_element->GetCachedPropertyValue(UIA_IsInvokePatternAvailablePropertyId, &invokable))) {
		button.invokable = invokable.vt == VT_BOOL && invokable.boolVal == VARIANT_TRUE;
	}

	VariantClear(&invokable);

	return button;
}
}

UiElement::UiElement(ComPtr<IUIAutomationElement> t_element)
	: m_element(std::move(t_element))
{
}

auto UiElement::set_value(std::wstring_view t_text) const -> bool
{
	if (!is_valid()) return false;

	const auto value_pattern = pattern_of<IUIAutomationValuePattern>(m_element, UIA_ValuePatternId);
	if (value_pattern == nullptr) {
		debug_log::write(K_LOG_CATEGORY, "SetValue: element has no ValuePattern - caller falls back to keystrokes");
		return false;
	}

	// Never log t_text: account passwords go through here, and the copy is wiped before the string cache can hand it out again.
	const debug_log::Scope scope(K_LOG_CATEGORY, "ValuePattern::SetValue");

	BSTR          text   = SysAllocStringLen(t_text.data(), static_cast<UINT>(t_text.size()));
	const HRESULT result = value_pattern->SetValue(text);
	SecureZeroMemory(text, SysStringByteLen(text));
	SysFreeString(text);

	if (FAILED(result)) {
		debug_log::write(K_LOG_CATEGORY, "ValuePattern::SetValue FAILED hr=0x%08lX", static_cast<unsigned long>(result));
	}

	return SUCCEEDED(result);
}

auto UiElement::invoke() const -> bool
{
	if (!is_valid()) return false;

	const auto invoke_pattern = pattern_of<IUIAutomationInvokePattern>(m_element, UIA_InvokePatternId);
	if (invoke_pattern == nullptr) return false;

	const debug_log::Scope scope(K_LOG_CATEGORY, "InvokePattern::Invoke");
	const HRESULT          result = invoke_pattern->Invoke();

	if (FAILED(result)) {
		debug_log::write(K_LOG_CATEGORY, "InvokePattern::Invoke FAILED hr=0x%08lX", static_cast<unsigned long>(result));
	}

	return SUCCEEDED(result);
}

auto UiElement::focus() const -> bool
{
	if (!is_valid()) return false;

	const debug_log::Scope scope(K_LOG_CATEGORY, "IUIAutomationElement::SetFocus");
	const HRESULT          result = m_element->SetFocus();

	if (FAILED(result)) {
		debug_log::write(K_LOG_CATEGORY, "SetFocus FAILED hr=0x%08lX", static_cast<unsigned long>(result));
	}

	return SUCCEEDED(result);
}

auto UiElement::has_keyboard_focus() const -> bool
{
	BOOL focused = FALSE;

	return is_valid() && SUCCEEDED(m_element->get_CurrentHasKeyboardFocus(&focused)) && focused;
}

auto UiElement::is_enabled() const -> bool
{
	BOOL enabled = FALSE;

	return is_valid() && SUCCEEDED(m_element->get_CurrentIsEnabled(&enabled)) && enabled;
}

auto UiElement::bounds() const -> std::optional<RECT>
{
	RECT bounds{};
	if (!is_valid() || FAILED(m_element->get_CurrentBoundingRectangle(&bounds))) return std::nullopt;

	return bounds;
}

UiAutomation::UiAutomation(const std::atomic<bool>* t_cancel)
	: m_cancel(t_cancel)
{
}

UiAutomation::~UiAutomation()
{
	shutdown();
}

auto UiAutomation::keep_process_mta_alive() -> void
{
	static CO_MTA_USAGE_COOKIE cookie = nullptr;
	if (cookie != nullptr) return;

	const HRESULT result = CoIncrementMTAUsage(&cookie);
	debug_log::write(K_LOG_CATEGORY, "CoIncrementMTAUsage hr=0x%08lX (process-wide MTA %s)", static_cast<unsigned long>(result), SUCCEEDED(result) ? "held open" : "NOT held - apartment will be torn down between attempts");
}

auto UiAutomation::init() -> bool
{
	if (m_automation != nullptr) return true;

	if (!m_com_initialized) {
		const debug_log::Scope scope(K_LOG_CATEGORY, "CoInitializeEx(COINIT_MULTITHREADED)");
		const HRESULT          result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		m_com_initialized             = result == S_OK || result == S_FALSE;

		debug_log::write(K_LOG_CATEGORY, "CoInitializeEx hr=0x%08lX (%s)", static_cast<unsigned long>(result), m_com_initialized ? "in the MTA" : "NOT initialised by us");
	}

	const debug_log::Scope scope(K_LOG_CATEGORY, "CoCreateInstance(CLSID_CUIAutomation)");
	const HRESULT          result = CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_automation));

	debug_log::write(K_LOG_CATEGORY, "CoCreateInstance(CLSID_CUIAutomation) hr=0x%08lX after %llums", static_cast<unsigned long>(result), scope.elapsed_ms());

	return SUCCEEDED(result);
}

auto UiAutomation::shutdown() -> void
{
	if (m_automation != nullptr) {
		const debug_log::Scope scope(K_LOG_CATEGORY, "release IUIAutomation");
		m_automation.Reset();
	}

	if (m_com_initialized) {
		const debug_log::Scope scope(K_LOG_CATEGORY, "CoUninitialize");
		CoUninitialize();
		m_com_initialized = false;
	}
}

auto UiAutomation::find_top_level_window(u32 t_process_id) -> HWND
{
	const std::vector<DWORD> process_ids = process_tree(t_process_id);

	WindowSearch search{.process_ids = &process_ids, .found = nullptr};
	EnumWindows(find_top_level_window_proc, reinterpret_cast<LPARAM>(&search));

	return search.found;
}

auto UiAutomation::element_from_window(HWND t_window) const -> UiElement
{
	if (m_automation == nullptr || t_window == nullptr || is_cancelled()) return {};

	char label[64];
	_snprintf_s(label, _TRUNCATE, "ElementFromHandle(hwnd=0x%p)", t_window);

	return UiElement{run_lookup(label, [automation = m_automation, t_window](ComPtr<IUIAutomationElement>* t_out) { automation->ElementFromHandle(t_window, t_out->ReleaseAndGetAddressOf()); }, m_cancel)};
}

auto UiAutomation::find_descendant(const UiElement& t_root, const wchar_t* t_name) const -> UiElement
{
	if (!can_search(t_root)) return {};

	const ComPtr<IUIAutomationCondition> condition = name_condition(t_name);
	if (condition == nullptr) return {};

	char label[192];
	_snprintf_s(label, _TRUNCATE, "FindFirst(Name=%ls)", t_name);

	return find_first(t_root, condition, label);
}

auto UiAutomation::find_descendant(const UiElement& t_root, const wchar_t* t_name, CONTROLTYPEID t_control_type) const -> UiElement
{
	if (!can_search(t_root)) return {};

	const ComPtr<IUIAutomationCondition> name = name_condition(t_name);

	ComPtr<IUIAutomationCondition> type_condition;
	ComPtr<IUIAutomationCondition> both;

	if (name == nullptr || FAILED(m_automation->CreatePropertyCondition(UIA_ControlTypePropertyId, variant_from_control_type(t_control_type), &type_condition)) || FAILED(m_automation->CreateAndCondition(name.Get(), type_condition.Get(), &both))) {
		return {};
	}

	char label[192];
	_snprintf_s(label, _TRUNCATE, "FindFirst(Name=%ls, ControlType=%d)", t_name, static_cast<int>(t_control_type));

	return find_first(t_root, both, label);
}

auto UiAutomation::find_edit(const UiElement& t_root, bool t_password) const -> UiElement
{
	if (!can_search(t_root)) return {};

	VARIANT password;
	VariantInit(&password);
	password.vt      = VT_BOOL;
	password.boolVal = t_password ? VARIANT_TRUE : VARIANT_FALSE;

	ComPtr<IUIAutomationCondition> type_condition;
	ComPtr<IUIAutomationCondition> password_condition;
	ComPtr<IUIAutomationCondition> both;

	if (FAILED(m_automation->CreatePropertyCondition(UIA_ControlTypePropertyId, variant_from_control_type(UIA_EditControlTypeId), &type_condition)) || FAILED(m_automation->CreatePropertyCondition(UIA_IsPasswordPropertyId, password, &password_condition)) || FAILED(m_automation->CreateAndCondition(type_condition.Get(), password_condition.Get(), &both))) {
		return {};
	}

	return find_first(t_root, both, t_password ? "FindFirst(Edit, IsPassword)" : "FindFirst(Edit, not IsPassword)");
}

auto UiAutomation::find_of_type(const UiElement& t_root, CONTROLTYPEID t_control_type) const -> UiElement
{
	if (!can_search(t_root)) return {};

	ComPtr<IUIAutomationCondition> condition;
	if (FAILED(m_automation->CreatePropertyCondition(UIA_ControlTypePropertyId, variant_from_control_type(t_control_type), &condition))) return {};

	char label[64];
	_snprintf_s(label, _TRUNCATE, "FindFirst(ControlType=%d)", static_cast<int>(t_control_type));

	return find_first(t_root, condition, label);
}

auto UiAutomation::find_buttons(const UiElement& t_root) const -> std::vector<UiButton>
{
	if (!can_search(t_root)) return {};

	ComPtr<IUIAutomationCondition>    condition;
	ComPtr<IUIAutomationCacheRequest> cache;
	if (FAILED(m_automation->CreatePropertyCondition(UIA_ControlTypePropertyId, variant_from_control_type(UIA_ButtonControlTypeId), &condition)) || FAILED(m_automation->CreateCacheRequest(&cache))) return {};

	for (const PROPERTYID property : K_BUTTON_PROPERTIES) {
		cache->AddProperty(property);
	}

	auto                   buttons = std::make_shared<std::vector<UiButton>>();
	const debug_log::Scope scope(K_LOG_CATEGORY, "FindAllBuildCache(ControlType=Button)");

	const auto collect_buttons = [root = t_root.m_element, condition, cache, buttons]() {
		const HRESULT com_result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

		ComPtr<IUIAutomationElementArray> found;
		int                               count = 0;

		if (SUCCEEDED(root->FindAllBuildCache(TreeScope_Descendants, condition.Get(), cache.Get(), &found)) && found != nullptr && SUCCEEDED(found->get_Length(&count))) {
			for (const int index : std::views::iota(0, count)) {
				ComPtr<IUIAutomationElement> element;
				if (SUCCEEDED(found->GetElement(index, &element)) && element != nullptr) {
					buttons->push_back(cached_button(element));
				}
			}
		}

		if (com_result == S_OK || com_result == S_FALSE) {
			CoUninitialize();
		}
	};

	if (!run_unless_cancelled(collect_buttons, m_cancel)) {
		debug_log::write(K_LOG_CATEGORY, "ABANDONED FindAllBuildCache(ControlType=Button) - cancelled while the provider was still answering");
		return {};
	}

	return std::move(*buttons);
}

// The client only puts some text, like a login error's reason, in the raw view, so this walks that view rather than searching the control view.
auto UiAutomation::text_inside(const UiElement& t_root, const wchar_t* t_ignored_text) const -> std::wstring
{
	if (!can_search(t_root)) return {};

	ComPtr<IUIAutomationTreeWalker> walker;
	if (FAILED(m_automation->get_RawViewWalker(&walker)) || walker == nullptr) return {};

	auto                   text = std::make_shared<std::wstring>();
	const debug_log::Scope scope(K_LOG_CATEGORY, "raw view walk for Text");

	const auto read_text = [root = t_root.m_element, walker, text, ignored = std::wstring{t_ignored_text}]() {
		const HRESULT com_result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

		const auto append = [&](const ComPtr<IUIAutomationElement>& t_element) {
			BSTR name = nullptr;
			if (FAILED(t_element->get_CurrentName(&name)) || name == nullptr) return;

			const std::wstring_view shown{name, SysStringLen(name)};
			if (!shown.empty() && shown != ignored && text->find(shown) == std::wstring::npos) {
				text->append(text->empty() ? L"" : L" ").append(shown);
			}

			SysFreeString(name);
		};

		u32        visited = 0;
		const auto visit   = [&](const auto& t_visit, const ComPtr<IUIAutomationElement>& t_parent, u32 t_depth) -> void {
			if (t_depth > K_MAX_TEXT_WALK_DEPTH) return;

			ComPtr<IUIAutomationElement> child;
			walker->GetFirstChildElement(t_parent.Get(), &child);

			while (child != nullptr && visited < K_MAX_TEXT_WALK_ELEMENTS) {
				visited += 1;

				CONTROLTYPEID control_type = 0;
				if (SUCCEEDED(child->get_CurrentControlType(&control_type)) && control_type == UIA_TextControlTypeId) {
					append(child);
				}

				t_visit(t_visit, child, t_depth + 1);

				ComPtr<IUIAutomationElement> next;
				walker->GetNextSiblingElement(child.Get(), &next);
				child = std::move(next);
			}
		};

		visit(visit, root, 0);

		if (text->empty()) {
			append(root);
		}

		if (com_result == S_OK || com_result == S_FALSE) {
			CoUninitialize();
		}
	};

	if (!run_unless_cancelled(read_text, m_cancel)) {
		debug_log::write(K_LOG_CATEGORY, "ABANDONED the raw view walk for Text - cancelled while the provider was still answering");
		return {};
	}

	return std::move(*text);
}

auto UiAutomation::type_text(const wchar_t* t_text) const -> void
{
	u32 sent     = 0;
	u32 rejected = 0;

	for (const wchar_t* character = t_text; *character != L'\0'; character += 1) {
		INPUT inputs[2]{};
		inputs[0].type       = INPUT_KEYBOARD;
		inputs[0].ki.wScan   = static_cast<WORD>(*character);
		inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;
		inputs[1]            = inputs[0];
		inputs[1].ki.dwFlags |= KEYEVENTF_KEYUP;

		if (SendInput(2, inputs, sizeof(INPUT)) == 2) {
			sent += 1;
		} else {
			rejected += 1;
		}
	}

	debug_log::write(K_LOG_CATEGORY, "type_text: %u character(s) sent, %u rejected%s", sent, rejected, rejected != 0 ? " - SendInput was blocked (elevation/UIPI?)" : "");
}

auto UiAutomation::press_key(WORD t_virtual_key) const -> void
{
	INPUT inputs[2]{};
	inputs[0].type       = INPUT_KEYBOARD;
	inputs[0].ki.wVk     = t_virtual_key;
	inputs[1]            = inputs[0];
	inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;

	const UINT inserted = SendInput(2, inputs, sizeof(INPUT));

	debug_log::write(K_LOG_CATEGORY, "press_key(vk=0x%02X): %u of 2 event(s) inserted%s", t_virtual_key, inserted, inserted != 2 ? " - SendInput was blocked (elevation/UIPI?)" : "");
}

auto UiAutomation::is_cancelled() const -> bool
{
	return m_cancel->load(std::memory_order_relaxed);
}

auto UiAutomation::can_search(const UiElement& t_root) const -> bool
{
	return m_automation != nullptr && t_root.is_valid() && !is_cancelled();
}

auto UiAutomation::name_condition(const wchar_t* t_name) const -> ComPtr<IUIAutomationCondition>
{
	const VariantString name(t_name);

	ComPtr<IUIAutomationCondition> condition;
	if (FAILED(m_automation->CreatePropertyConditionEx(UIA_NamePropertyId, name.m_value, PropertyConditionFlags_IgnoreCase, &condition))) return nullptr;

	return condition;
}

auto UiAutomation::find_first(const UiElement& t_root, const ComPtr<IUIAutomationCondition>& t_condition, const char* t_label) const -> UiElement
{
	return UiElement{run_lookup(t_label, [root = t_root.m_element, t_condition](ComPtr<IUIAutomationElement>* t_out) { root->FindFirst(TreeScope_Descendants, t_condition.Get(), t_out->ReleaseAndGetAddressOf()); }, m_cancel)};
}
