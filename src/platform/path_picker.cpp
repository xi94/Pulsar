#include "platform/path_picker.h"

#include <utility>

#include <shobjidl.h>
#include <wrl/client.h>

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

[[nodiscard]] auto show_dialog(HWND t_owner, const PathRequest& t_request) -> std::optional<std::wstring>
{
	ComPtr<IFileOpenDialog> dialog;
	if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) return std::nullopt;

	const DWORD kind_options = t_request.kind == PathKind::Folder ? FOS_PICKFOLDERS | FOS_PATHMUSTEXIST : FOS_FILEMUSTEXIST;
	DWORD       options      = 0;
	dialog->GetOptions(&options);
	dialog->SetOptions(options | kind_options | FOS_FORCEFILESYSTEM);
	dialog->SetTitle(t_request.title);

	if (t_request.ok_label != nullptr) {
		dialog->SetOkButtonLabel(t_request.ok_label);
	}

	if (t_request.file_type_pattern != nullptr) {
		const COMDLG_FILTERSPEC file_type{t_request.file_type_name, t_request.file_type_pattern};
		dialog->SetFileTypes(1, &file_type);
	}

	const std::wstring start = nearest_existing_folder(t_request.start_path);
	ComPtr<IShellItem> start_item;
	if (!start.empty() && SUCCEEDED(SHCreateItemFromParsingName(start.c_str(), nullptr, IID_PPV_ARGS(&start_item)))) {
		dialog->SetFolder(start_item.Get());
	}

	ComPtr<IShellItem>          picked;
	PWSTR                       path = nullptr;
	std::optional<std::wstring> result;

	if (SUCCEEDED(dialog->Show(t_owner)) && SUCCEEDED(dialog->GetResult(&picked)) && SUCCEEDED(picked->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
		result = path;
	}

	CoTaskMemFree(path);

	return result;
}
}

ComScope::ComScope()
	: m_initialized(SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)))
{
}

ComScope::~ComScope()
{
	if (m_initialized) {
		CoUninitialize();
	}
}

PathPicker::~PathPicker()
{
	if (!m_thread.joinable()) return;

	// The dialog blocks its thread until it closes, so it is closed rather than waited on.
	if (!m_finished.load(std::memory_order_acquire)) {
		for (int attempt = 0; attempt < K_THREAD_START_POLLS && m_thread_id.load(std::memory_order_acquire) == 0; attempt += 1) {
			Sleep(10);
		}

		if (const DWORD thread_id = m_thread_id.load(std::memory_order_acquire); thread_id != 0) {
			EnumThreadWindows(thread_id, close_thread_window, 0);
		}
	}

	m_thread.join();
}

auto PathPicker::open(HWND t_owner, PathRequest t_request) -> void
{
	if (is_open()) return;

	if (m_thread.joinable()) {
		m_thread.join();
	}

	m_finished.store(false, std::memory_order_release);
	m_thread_id.store(0, std::memory_order_release);
	m_result.reset();

	m_thread = std::thread([this, t_owner, request = std::move(t_request)]() {
		m_thread_id.store(GetCurrentThreadId(), std::memory_order_release);

		const ComScope com;
		m_result = show_dialog(t_owner, request);

		m_finished.store(true, std::memory_order_release);
	});
}

auto PathPicker::take_result() -> std::optional<std::wstring>
{
	if (!m_finished.load(std::memory_order_acquire)) return std::nullopt;

	if (m_thread.joinable()) {
		m_thread.join();
	}

	m_finished.store(false, std::memory_order_release);

	return std::exchange(m_result, std::nullopt);
}
