#pragma once

#include <atomic>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <Windows.h>
#include <UIAutomation.h>
#include <wrl/client.h>

class UiElement {
  public:
	UiElement() = default;
	explicit UiElement(Microsoft::WRL::ComPtr<IUIAutomationElement> t_element);

	[[nodiscard]] auto is_valid() const -> bool
	{
		return m_element != nullptr;
	}

	[[nodiscard]] auto set_value(std::wstring_view t_text) const -> bool;
	auto invoke() const -> bool;
	auto focus() const -> bool;
	[[nodiscard]] auto has_keyboard_focus() const -> bool;
	[[nodiscard]] auto is_enabled() const -> bool;
	[[nodiscard]] auto bounds() const -> std::optional<RECT>;

	Microsoft::WRL::ComPtr<IUIAutomationElement> m_element;

  private:
};

struct UiButton {
	UiElement element;
	RECT      bounds{};
	bool      named     = false;
	bool      invokable = false;
	bool      enabled   = false;
};

class UiAutomation {
  public:
	explicit UiAutomation(const std::atomic<bool>* t_cancel);
	~UiAutomation();

	UiAutomation(const UiAutomation&)                    = delete;
	auto operator=(const UiAutomation&) -> UiAutomation& = delete;

	static auto keep_process_mta_alive() -> void;

	[[nodiscard]] auto init() -> bool;
	auto shutdown() -> void;

	[[nodiscard]] static auto find_top_level_window(u32 t_process_id) -> HWND;

	[[nodiscard]] auto element_from_window(HWND t_window) const -> UiElement;
	[[nodiscard]] auto find_descendant(const UiElement& t_root, const wchar_t* t_name) const -> UiElement;
	[[nodiscard]] auto find_descendant(const UiElement& t_root, const wchar_t* t_name, CONTROLTYPEID t_control_type) const -> UiElement;
	[[nodiscard]] auto find_edit(const UiElement& t_root, bool t_password) const -> UiElement;
	[[nodiscard]] auto find_of_type(const UiElement& t_root, CONTROLTYPEID t_control_type) const -> UiElement;
	[[nodiscard]] auto find_buttons(const UiElement& t_root) const -> std::vector<UiButton>;
	[[nodiscard]] auto text_inside(const UiElement& t_root, const wchar_t* t_ignored_text) const -> std::wstring;

	auto type_text(const wchar_t* t_text) const -> void;
	auto press_key(WORD t_virtual_key) const -> void;

  private:
	[[nodiscard]] auto is_cancelled() const -> bool;
	[[nodiscard]] auto can_search(const UiElement& t_root) const -> bool;

	[[nodiscard]] auto name_condition(const wchar_t* t_name) const -> Microsoft::WRL::ComPtr<IUIAutomationCondition>;
	[[nodiscard]] auto find_first(const UiElement& t_root, const Microsoft::WRL::ComPtr<IUIAutomationCondition>& t_condition, const char* t_label) const -> UiElement;

	const std::atomic<bool>*              m_cancel;
	Microsoft::WRL::ComPtr<IUIAutomation> m_automation;
	bool                                  m_com_initialized = false;
};
