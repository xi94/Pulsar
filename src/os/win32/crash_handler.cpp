#include "os/crash_handler.h"

#include <atomic>
#include <cstdlib>
#include <cwchar>
#include <exception>

#include <Windows.h>
#include <CommCtrl.h>
#include <DbgHelp.h>
#include <shellapi.h>

#include "core/app_identity.h"
#include "core/file.h"
#include "os/win32/win32.h"

namespace {
constexpr int K_OPEN_FOLDER_BUTTON_ID = 1001;
constexpr int K_CLOSE_BUTTON_ID       = 1002;

// Never MiniDumpWithFullMemory: it would write the vault key and every decrypted password to disk.
constexpr auto K_DUMP_TYPE = static_cast<MINIDUMP_TYPE>(MiniDumpWithThreadInfo | MiniDumpWithHandleData | MiniDumpWithUnloadedModules);

std::atomic<bool> g_handling_crash{false};

[[nodiscard]] auto crash_dump_directory() -> std::wstring
{
	return os::win32::to_wide(app_data_subdirectory("crashes"));
}

[[nodiscard]] auto timestamp() -> std::wstring
{
	SYSTEMTIME time;
	GetLocalTime(&time);

	wchar_t buffer[32];
	swprintf_s(buffer, L"%04u%02u%02u_%02u%02u%02u", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);

	return buffer;
}

[[nodiscard]] auto module_relative_address(void* t_address) -> std::wstring
{
	const HMODULE module = GetModuleHandleW(nullptr);

	wchar_t module_path[MAX_PATH]{};
	GetModuleFileNameW(module, module_path, MAX_PATH);

	const wchar_t* last_separator = wcsrchr(module_path, L'\\');
	const wchar_t* module_name    = last_separator != nullptr ? last_separator + 1 : module_path;

	const auto base    = reinterpret_cast<uptr>(module);
	const auto address = reinterpret_cast<uptr>(t_address);

	wchar_t buffer[MAX_PATH + 32];
	if (address >= base) {
		swprintf_s(buffer, L"%s+0x%llX", module_name, static_cast<unsigned long long>(address - base));
	} else {
		swprintf_s(buffer, L"%s (address outside module: 0x%p)", module_name, t_address);
	}

	return buffer;
}

[[nodiscard]] auto write_minidump(EXCEPTION_POINTERS* t_exception, const std::wstring& t_directory, const wchar_t* t_tag, MINIDUMP_TYPE t_type) -> std::wstring
{
	if (t_directory.empty()) return {};

	const std::wstring path = t_directory + L"\\" + os::win32::K_APP_NAME_WIDE + L"_" + t_tag + L"_" + timestamp() + L".dmp";
	const HANDLE       file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE) return {};

	MINIDUMP_EXCEPTION_INFORMATION exception_info{
		.ThreadId          = GetCurrentThreadId(),
		.ExceptionPointers = t_exception,
		.ClientPointers    = FALSE,
	};

	const BOOL written =
		MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, t_type, t_exception != nullptr ? &exception_info : nullptr, nullptr, nullptr);
	CloseHandle(file);

	return written ? path : std::wstring{};
}

auto CALLBACK crash_dialog_callback(HWND t_window, UINT t_notification, WPARAM t_button, LPARAM, LONG_PTR t_dump_directory) -> HRESULT
{
	if (t_notification != TDN_BUTTON_CLICKED || t_button != K_OPEN_FOLDER_BUTTON_ID) return S_OK;

	const auto* dump_directory = reinterpret_cast<const wchar_t*>(t_dump_directory);
	if (dump_directory != nullptr && dump_directory[0] != L'\0') {
		ShellExecuteW(t_window, L"open", dump_directory, nullptr, nullptr, SW_SHOWNORMAL);
	}

	return S_FALSE;
}

auto show_crash_dialog(const std::wstring& t_reason, const std::wstring& t_location, const std::wstring& t_dump_path, const std::wstring& t_dump_directory)
	-> void
{
	std::wstring content = L"An unexpected error occurred and the app needs to close. A crash report has been saved "
	                       L"locally.\n\nError: " +
	                       t_reason + L"\nLocation: " + t_location;
	content += t_dump_path.empty() ? L"\n\nThe crash report itself could not be saved." : L"\n\nSaved to:\n" + t_dump_path;

	const std::wstring instruction = std::wstring{os::win32::K_APP_NAME_WIDE} + L" has stopped working";

	const TASKDIALOG_BUTTON buttons[]{
		{K_OPEN_FOLDER_BUTTON_ID, L"Open crash folder"},
		{K_CLOSE_BUTTON_ID, L"Close"},
	};

	TASKDIALOGCONFIG config{};
	config.cbSize             = sizeof(config);
	config.dwFlags            = TDF_SIZE_TO_CONTENT;
	config.pszWindowTitle     = os::win32::K_APP_NAME_WIDE;
	config.pszMainIcon        = TD_ERROR_ICON;
	config.pszMainInstruction = instruction.c_str();
	config.pszContent         = content.c_str();
	config.cButtons           = ARRAYSIZE(buttons);
	config.pButtons           = buttons;
	config.nDefaultButton     = K_CLOSE_BUTTON_ID;
	config.pfCallback         = crash_dialog_callback;
	config.lpCallbackData     = reinterpret_cast<LONG_PTR>(t_dump_directory.c_str());

	TaskDialogIndirect(&config, nullptr, nullptr, nullptr);
}

[[nodiscard]] auto exception_name(DWORD t_code) -> const wchar_t*
{
	switch (t_code) {
		case EXCEPTION_ACCESS_VIOLATION:
			return L"Access violation";
		case EXCEPTION_STACK_OVERFLOW:
			return L"Stack overflow";
		case EXCEPTION_ILLEGAL_INSTRUCTION:
			return L"Illegal instruction";
		case EXCEPTION_INT_DIVIDE_BY_ZERO:
			return L"Integer divide by zero";
		case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
			return L"Array bounds exceeded";
		case EXCEPTION_DATATYPE_MISALIGNMENT:
			return L"Datatype misalignment";
		case EXCEPTION_PRIV_INSTRUCTION:
			return L"Privileged instruction";
		case EXCEPTION_IN_PAGE_ERROR:
			return L"In-page I/O error";
		case EXCEPTION_BREAKPOINT:
			return L"Breakpoint (unhandled)";
		default:
			return L"Unknown exception";
	}
}

auto report_crash(EXCEPTION_POINTERS* t_exception, const wchar_t* t_reason) -> void
{
	const std::wstring location       = module_relative_address(t_exception->ExceptionRecord->ExceptionAddress);
	const std::wstring dump_directory = crash_dump_directory();
	const std::wstring dump_path      = write_minidump(t_exception, dump_directory, L"crash", K_DUMP_TYPE);

	show_crash_dialog(t_reason, location, dump_path, dump_directory);
}

auto WINAPI unhandled_exception_filter(EXCEPTION_POINTERS* t_exception) -> LONG
{
	bool already_handling = false;
	if (!g_handling_crash.compare_exchange_strong(already_handling, true)) return EXCEPTION_CONTINUE_SEARCH;

	const DWORD code = t_exception->ExceptionRecord->ExceptionCode;

	// Restores the guard page so the handler below has stack to run on.
	if (code == EXCEPTION_STACK_OVERFLOW) {
		_resetstkoflw();
	}

	report_crash(t_exception, exception_name(code));

	return EXCEPTION_EXECUTE_HANDLER;
}

[[noreturn]] void handle_fatal_condition(const wchar_t* t_reason)
{
	bool already_handling = false;
	if (!g_handling_crash.compare_exchange_strong(already_handling, true)) {
		std::abort();
	}

	CONTEXT context{};
	RtlCaptureContext(&context);

	EXCEPTION_RECORD record{.ExceptionCode = STATUS_FATAL_APP_EXIT};
#if defined(_M_X64)
	record.ExceptionAddress = reinterpret_cast<void*>(context.Rip);
#elif defined(_M_IX86)
	record.ExceptionAddress = reinterpret_cast<void*>(context.Eip);
#endif

	EXCEPTION_POINTERS pointers{&record, &context};
	report_crash(&pointers, t_reason);

	std::abort();
}

[[noreturn]] void on_terminate()
{
	handle_fatal_condition(L"Unhandled exception");
}

[[noreturn]] auto __cdecl on_pure_call() -> void
{
	handle_fatal_condition(L"Pure virtual function call");
}

auto __cdecl on_invalid_parameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uptr) -> void
{
	handle_fatal_condition(L"CRT invalid parameter");
}
}

namespace os {

auto install_crash_handler() -> void
{
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
	_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);

	SetUnhandledExceptionFilter(unhandled_exception_filter);
	std::set_terminate(on_terminate);
	_set_purecall_handler(on_pure_call);
	_set_invalid_parameter_handler(on_invalid_parameter);
}

auto write_diagnostic_dump(const char* t_tag) -> std::string
{
	return win32::to_utf8(write_minidump(nullptr, crash_dump_directory(), win32::to_wide(t_tag).c_str(), K_DUMP_TYPE));
}

}
