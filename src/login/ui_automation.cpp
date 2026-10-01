#include "login/ui_automation.h"

#include <algorithm>
#include <chrono>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include <TlHelp32.h>

#include "core/debug_log.h"
#include "core/thread_util.h"

using Microsoft::WRL::ComPtr;

namespace {
constexpr const char *log_category = "uia";
constexpr LONG min_real_window_width = 50;

class VariantString {
  public:
	explicit VariantString(const wchar_t *t_text)
	{
		VariantInit(&m_value);
		m_value.vt = VT_BSTR;
		m_value.bstrVal = SysAllocString(t_text);
	}

	~VariantString()
	{
		VariantClear(&m_value);
	}

	VariantString(const VariantString &) = delete;
	VariantString &operator=(const VariantString &) = delete;

	const VARIANT &get() const
	{
		return m_value;
	}

  private:
	VARIANT m_value;
};

VARIANT variant_from_control_type(CONTROLTYPEID t_control_type)
{
	VARIANT value;
	VariantInit(&value);
	value.vt = VT_I4;
	value.lVal = t_control_type;

	return value;
}

std::vector<DWORD> process_tree(DWORD t_root_process_id)
{
	std::vector<DWORD> tree{t_root_process_id};

	const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == INVALID_HANDLE_VALUE) return tree;

	std::vector<std::pair<DWORD, DWORD>> parent_child_pairs;
	PROCESSENTRY32W entry{.dwSize = sizeof(entry)};

	if (Process32FirstW(snapshot, &entry)) {
		do {
			parent_child_pairs.emplace_back(entry.th32ParentProcessID, entry.th32ProcessID);
		} while (Process32NextW(snapshot, &entry));
	}

	CloseHandle(snapshot);

	const auto in_tree = [&tree](DWORD t_process_id) { return std::ranges::find(tree, t_process_id) != tree.end(); };

	for (bool grew = true; grew;) {
		grew = false;

		for (const auto &[parent, child] : parent_child_pairs) {
			if (in_tree(parent) && !in_tree(child)) {
				tree.push_back(child);
				grew = true;
			}
		}
	}

	return tree;
}

struct WindowSearch {
	const std::vector<DWORD> *process_ids;
	HWND found;
};

BOOL CALLBACK find_top_level_window_proc(HWND t_window, LPARAM t_search)
{
	auto *search = reinterpret_cast<WindowSearch *>(t_search);

	DWORD process_id = 0;
	GetWindowThreadProcessId(t_window, &process_id);

	if (std::ranges::find(*search->process_ids, process_id) == search->process_ids->end()) return TRUE;
	if (GetWindow(t_window, GW_OWNER) != nullptr || !IsWindowVisible(t_window)) return TRUE;

	RECT rect;
	if (GetWindowRect(t_window, &rect) && rect.right - rect.left < min_real_window_width) return TRUE;

	search->found = t_window;

	return FALSE;
}

template <typename Lookup>
ComPtr<IUIAutomationElement> run_lookup(const char *t_label, Lookup t_lookup, const std::atomic<bool> *t_cancel)
{
	auto result = std::make_shared<ComPtr<IUIAutomationElement>>();
	const debug_log::Scope scope(log_category, "%s", t_label);

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
		debug_log::write(log_category, "ABANDONED %s - cancelled while the provider was still answering", t_label);
		return nullptr;
	}

	return std::move(*result);
}

template <typename Pattern>
ComPtr<Pattern> pattern_of(const ComPtr<IUIAutomationElement> &t_element, PATTERNID t_pattern_id)
{
	ComPtr<Pattern> pattern;
	if (FAILED(t_element->GetCurrentPatternAs(t_pattern_id, IID_PPV_ARGS(&pattern)))) return nullptr;

	return pattern;
}
}

UiElement::UiElement(ComPtr<IUIAutomationElement> t_element)
	: m_element(std::move(t_element))
{
}

bool UiElement::set_value(const wchar_t *t_text) const
{
	if (!is_valid()) return false;

	const auto value_pattern = pattern_of<IUIAutomationValuePattern>(m_element, UIA_ValuePatternId);
	if (value_pattern == nullptr) {
		debug_log::write(log_category, "SetValue: element has no ValuePattern - caller falls back to keystrokes");
		return false;
	}

	// Never log t_text: account passwords go through here.
	const debug_log::Scope scope(log_category, "ValuePattern::SetValue");

	BSTR text = SysAllocString(t_text);
	const HRESULT result = value_pattern->SetValue(text);
	SysFreeString(text);

	if (FAILED(result)) {
		debug_log::write(log_category, "ValuePattern::SetValue FAILED hr=0x%08lX", static_cast<unsigned long>(result));
	}

	return SUCCEEDED(result);
}

bool UiElement::invoke() const
{
	if (!is_valid()) return false;

	const auto invoke_pattern = pattern_of<IUIAutomationInvokePattern>(m_element, UIA_InvokePatternId);
	if (invoke_pattern == nullptr) return false;

	const debug_log::Scope scope(log_category, "InvokePattern::Invoke");
	const HRESULT result = invoke_pattern->Invoke();

	if (FAILED(result)) {
		debug_log::write(log_category, "InvokePattern::Invoke FAILED hr=0x%08lX", static_cast<unsigned long>(result));
	}

	return SUCCEEDED(result);
}

bool UiElement::focus() const
{
	if (!is_valid()) return false;

	const debug_log::Scope scope(log_category, "IUIAutomationElement::SetFocus");
	const HRESULT result = m_element->SetFocus();

	if (FAILED(result)) {
		debug_log::write(log_category, "SetFocus FAILED hr=0x%08lX", static_cast<unsigned long>(result));
	}

	return SUCCEEDED(result);
}

bool UiElement::has_keyboard_focus() const
{
	BOOL focused = FALSE;

	return is_valid() && SUCCEEDED(m_element->get_CurrentHasKeyboardFocus(&focused)) && focused;
}

UiAutomation::UiAutomation(const std::atomic<bool> *t_cancel)
	: m_cancel(t_cancel)
{
}

UiAutomation::~UiAutomation()
{
	shutdown();
}

void UiAutomation::keep_process_mta_alive()
{
	static CO_MTA_USAGE_COOKIE cookie = nullptr;
	if (cookie != nullptr) return;

	const HRESULT result = CoIncrementMTAUsage(&cookie);
	debug_log::write(log_category, "CoIncrementMTAUsage hr=0x%08lX (process-wide MTA %s)", static_cast<unsigned long>(result),
					 SUCCEEDED(result) ? "held open" : "NOT held - apartment will be torn down between attempts");
}

bool UiAutomation::init()
{
	if (m_automation != nullptr) return true;

	if (!m_com_initialized) {
		const debug_log::Scope scope(log_category, "CoInitializeEx(COINIT_MULTITHREADED)");
		const HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		m_com_initialized = result == S_OK || result == S_FALSE;

		debug_log::write(log_category, "CoInitializeEx hr=0x%08lX (%s)", static_cast<unsigned long>(result),
						 m_com_initialized ? "in the MTA" : "NOT initialised by us");
	}

	const debug_log::Scope scope(log_category, "CoCreateInstance(CLSID_CUIAutomation)");
	const HRESULT result = CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_automation));

	debug_log::write(log_category, "CoCreateInstance(CLSID_CUIAutomation) hr=0x%08lX after %llums", static_cast<unsigned long>(result), scope.elapsed_ms());

	return SUCCEEDED(result);
}

void UiAutomation::shutdown()
{
	if (m_automation != nullptr) {
		const debug_log::Scope scope(log_category, "release IUIAutomation");
		m_automation.Reset();
	}

	if (m_com_initialized) {
		const debug_log::Scope scope(log_category, "CoUninitialize");
		CoUninitialize();
		m_com_initialized = false;
	}
}

HWND UiAutomation::find_top_level_window(u32 t_process_id)
{
	const std::vector<DWORD> process_ids = process_tree(t_process_id);

	WindowSearch search{.process_ids = &process_ids, .found = nullptr};
	EnumWindows(find_top_level_window_proc, reinterpret_cast<LPARAM>(&search));

	return search.found;
}

UiElement UiAutomation::element_from_window(HWND t_window) const
{
	if (m_automation == nullptr || t_window == nullptr || is_cancelled()) return {};

	char label[64];
	_snprintf_s(label, _TRUNCATE, "ElementFromHandle(hwnd=0x%p)", t_window);

	return UiElement{run_lookup(
		label,
		[automation = m_automation, t_window](ComPtr<IUIAutomationElement> *t_out) {
			automation->ElementFromHandle(t_window, t_out->ReleaseAndGetAddressOf());
		},
		m_cancel)};
}

UiElement UiAutomation::find_descendant(const UiElement &t_root, const wchar_t *t_name) const
{
	if (!can_search(t_root)) return {};

	const VariantString name(t_name);

	ComPtr<IUIAutomationCondition> condition;
	if (FAILED(m_automation->CreatePropertyCondition(UIA_NamePropertyId, name.get(), &condition))) return {};

	char label[192];
	_snprintf_s(label, _TRUNCATE, "FindFirst(Name=%ls)", t_name);

	return find_first(t_root, std::move(condition), label);
}

UiElement UiAutomation::find_descendant(const UiElement &t_root, const wchar_t *t_name, CONTROLTYPEID t_control_type) const
{
	if (!can_search(t_root)) return {};

	const VariantString name(t_name);

	ComPtr<IUIAutomationCondition> name_condition;
	ComPtr<IUIAutomationCondition> type_condition;
	ComPtr<IUIAutomationCondition> both;

	if (FAILED(m_automation->CreatePropertyCondition(UIA_NamePropertyId, name.get(), &name_condition)) ||
		FAILED(m_automation->CreatePropertyCondition(UIA_ControlTypePropertyId, variant_from_control_type(t_control_type), &type_condition)) ||
		FAILED(m_automation->CreateAndCondition(name_condition.Get(), type_condition.Get(), &both))) {
		return {};
	}

	char label[192];
	_snprintf_s(label, _TRUNCATE, "FindFirst(Name=%ls, ControlType=%d)", t_name, static_cast<int>(t_control_type));

	return find_first(t_root, std::move(both), label);
}

void UiAutomation::type_text(const wchar_t *t_text) const
{
	u32 sent = 0;
	u32 rejected = 0;

	for (const wchar_t *character = t_text; *character != L'\0'; character += 1) {
		INPUT inputs[2]{};
		inputs[0].type = INPUT_KEYBOARD;
		inputs[0].ki.wScan = static_cast<WORD>(*character);
		inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;
		inputs[1] = inputs[0];
		inputs[1].ki.dwFlags |= KEYEVENTF_KEYUP;

		if (SendInput(2, inputs, sizeof(INPUT)) == 2) {
			sent += 1;
		} else {
			rejected += 1;
		}
	}

	debug_log::write(log_category, "type_text: %u character(s) sent, %u rejected%s", sent, rejected,
					 rejected != 0 ? " - SendInput was blocked (elevation/UIPI?)" : "");
}

void UiAutomation::press_key(WORD t_virtual_key) const
{
	INPUT inputs[2]{};
	inputs[0].type = INPUT_KEYBOARD;
	inputs[0].ki.wVk = t_virtual_key;
	inputs[1] = inputs[0];
	inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;

	const UINT inserted = SendInput(2, inputs, sizeof(INPUT));

	debug_log::write(log_category, "press_key(vk=0x%02X): %u of 2 event(s) inserted%s", t_virtual_key, inserted,
					 inserted != 2 ? " - SendInput was blocked (elevation/UIPI?)" : "");
}

bool UiAutomation::is_cancelled() const
{
	return m_cancel->load(std::memory_order_relaxed);
}

bool UiAutomation::can_search(const UiElement &t_root) const
{
	return m_automation != nullptr && t_root.is_valid() && !is_cancelled();
}

UiElement UiAutomation::find_first(const UiElement &t_root, ComPtr<IUIAutomationCondition> t_condition, const char *t_label) const
{
	return UiElement{run_lookup(
		t_label,
		[root = t_root.com(), t_condition](ComPtr<IUIAutomationElement> *t_out) {
			root->FindFirst(TreeScope_Descendants, t_condition.Get(), t_out->ReleaseAndGetAddressOf());
		},
		m_cancel)};
}
