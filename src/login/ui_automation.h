#pragma once

#include <atomic>

#include <UIAutomation.h>
#include <Windows.h>
#include <wrl/client.h>

class UiElement {
  public:
	UiElement() = default;
	explicit UiElement(Microsoft::WRL::ComPtr<IUIAutomationElement> t_element);

	bool is_valid() const
	{
		return m_element != nullptr;
	}

	const Microsoft::WRL::ComPtr<IUIAutomationElement> &com() const
	{
		return m_element;
	}

	bool set_value(const wchar_t *t_text) const;
	bool invoke() const;
	bool focus() const;
	bool has_keyboard_focus() const;

  private:
	Microsoft::WRL::ComPtr<IUIAutomationElement> m_element;
};

class UiAutomation {
  public:
	explicit UiAutomation(const std::atomic<bool> *t_cancel);
	~UiAutomation();

	UiAutomation(const UiAutomation &) = delete;
	UiAutomation &operator=(const UiAutomation &) = delete;

	static void keep_process_mta_alive();

	bool init();
	void shutdown();

	static HWND find_top_level_window(u32 t_process_id);

	UiElement element_from_window(HWND t_window) const;
	UiElement find_descendant(const UiElement &t_root, const wchar_t *t_name) const;
	UiElement find_descendant(const UiElement &t_root, const wchar_t *t_name, CONTROLTYPEID t_control_type) const;

	void type_text(const wchar_t *t_text) const;
	void press_key(WORD t_virtual_key) const;

  private:
	bool is_cancelled() const;
	bool can_search(const UiElement &t_root) const;

	UiElement find_first(const UiElement &t_root, Microsoft::WRL::ComPtr<IUIAutomationCondition> t_condition, const char *t_label) const;

	const std::atomic<bool> *m_cancel;
	Microsoft::WRL::ComPtr<IUIAutomation> m_automation;
	bool m_com_initialized = false;
};
