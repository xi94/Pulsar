#include "os/path_picker.h"

#include <atomic>
#include <thread>
#include <utility>

#include <Windows.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include "os/win32/win32.h"

using Microsoft::WRL::ComPtr;

namespace {
constexpr int K_THREAD_START_POLLS = 20;

[[nodiscard]] auto is_folder(const std::wstring& t_path) -> bool
{
	const DWORD attributes = GetFileAttributesW(t_path.c_str());

	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

[[nodiscard]] auto nearest_existing_folder(std::wstring t_path) -> std::wstring
{
	while (!t_path.empty() && !is_folder(t_path)) {
		const usize slash = t_path.find_last_of(L"\\/");
		if (slash == std::wstring::npos) return {};

		t_path.resize(slash);
	}

	return t_path;
}

auto CALLBACK close_thread_window(HWND t_window, LPARAM) -> BOOL
{
	PostMessageW(t_window, WM_CLOSE, 0, 0);

	return TRUE;
}

[[nodiscard]] auto show_dialog(HWND t_owner, const os::PathRequest& t_request) -> std::optional<std::string>
{
	using os::win32::to_wide;

	ComPtr<IFileOpenDialog> dialog;
	if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) return std::nullopt;

	const DWORD kind_options = t_request.kind == os::PathKind::FOLDER ? FOS_PICKFOLDERS | FOS_PATHMUSTEXIST : FOS_FILEMUSTEXIST;
	DWORD       options      = 0;
	dialog->GetOptions(&options);
	dialog->SetOptions(options | kind_options | FOS_FORCEFILESYSTEM);
	dialog->SetTitle(to_wide(t_request.title).c_str());

	if (t_request.ok_label != nullptr) {
		dialog->SetOkButtonLabel(to_wide(t_request.ok_label).c_str());
	}

	if (t_request.file_type_pattern != nullptr) {
		const std::wstring      name    = to_wide(t_request.file_type_name != nullptr ? t_request.file_type_name : "");
		const std::wstring      pattern = to_wide(t_request.file_type_pattern);
		const COMDLG_FILTERSPEC file_type{name.c_str(), pattern.c_str()};
		dialog->SetFileTypes(1, &file_type);
	}

	const std::wstring start = nearest_existing_folder(to_wide(t_request.start_path));
	ComPtr<IShellItem> start_item;
	if (!start.empty() && SUCCEEDED(SHCreateItemFromParsingName(start.c_str(), nullptr, IID_PPV_ARGS(&start_item)))) {
		dialog->SetFolder(start_item.Get());
	}

	ComPtr<IShellItem>         picked;
	PWSTR                      path = nullptr;
	std::optional<std::string> result;

	if (SUCCEEDED(dialog->Show(t_owner)) && SUCCEEDED(dialog->GetResult(&picked)) && SUCCEEDED(picked->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
		result = os::win32::to_utf8(path);
	}

	CoTaskMemFree(path);

	return result;
}
}

namespace os {

struct PathPicker::Native {
	std::thread                thread;
	std::atomic<bool>          finished{false};
	std::atomic<DWORD>         thread_id{0};
	std::optional<std::string> result;
};

PathPicker::PathPicker()
	: m_native(std::make_unique<Native>())
{
}

PathPicker::~PathPicker()
{
	if (!m_native->thread.joinable()) return;

	// The dialog blocks its thread until it closes, so it is closed rather than waited on.
	if (!m_native->finished.load(std::memory_order_acquire)) {
		for (int attempt = 0; attempt < K_THREAD_START_POLLS && m_native->thread_id.load(std::memory_order_acquire) == 0; attempt += 1) {
			Sleep(10);
		}

		if (const DWORD thread_id = m_native->thread_id.load(std::memory_order_acquire); thread_id != 0) {
			EnumThreadWindows(thread_id, close_thread_window, 0);
		}
	}

	m_native->thread.join();
}

auto PathPicker::open(const Window* t_owner, PathRequest t_request) -> void
{
	if (is_open()) return;

	if (m_native->thread.joinable()) {
		m_native->thread.join();
	}

	m_native->finished.store(false, std::memory_order_release);
	m_native->thread_id.store(0, std::memory_order_release);
	m_native->result.reset();

	const HWND owner = t_owner != nullptr ? win32::window_handle(t_owner) : nullptr;
	Native*    state = m_native.get();

	state->thread = std::thread([state, owner, request = std::move(t_request)]() {
		state->thread_id.store(GetCurrentThreadId(), std::memory_order_release);

		const win32::ComScope com;
		state->result = show_dialog(owner, request);

		state->finished.store(true, std::memory_order_release);
	});
}

auto PathPicker::is_open() const -> bool
{
	return m_native->thread.joinable() && !m_native->finished.load(std::memory_order_acquire);
}

auto PathPicker::take_result() -> std::optional<std::string>
{
	if (!m_native->finished.load(std::memory_order_acquire)) return std::nullopt;

	if (m_native->thread.joinable()) {
		m_native->thread.join();
	}

	m_native->finished.store(false, std::memory_order_release);

	return std::exchange(m_native->result, std::nullopt);
}

}
