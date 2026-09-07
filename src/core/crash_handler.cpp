#include "core/crash_handler.h"

#include "core/app_identity.h"
#include "core/app_paths.h"

#include <atomic>
#include <cstdlib>
#include <cwchar>
#include <exception>
#include <string>

#include <Windows.h>
#include <CommCtrl.h>
#include <DbgHelp.h>
#include <combaseapi.h>
#include <shellapi.h>
#include <shlobj.h>

namespace {
constexpr int kOpenFolderButtonId = 1001;
constexpr int kCloseButtonId = 1002;

// Full memory so the dump can be opened in a real debugger and inspected by hand, not just
// read as a curated call stack.
constexpr auto kCrashDumpType = static_cast<MINIDUMP_TYPE>(MiniDumpWithFullMemory | MiniDumpWithHandleData |
														   MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules);

// Much cheaper than kCrashDumpType: a hang dump is taken while the process is still
// expected to carry on, so it captures stacks without the hundreds of megabytes.
constexpr auto kHangDumpType =
	static_cast<MINIDUMP_TYPE>(MiniDumpWithThreadInfo | MiniDumpWithHandleData | MiniDumpWithUnloadedModules);

// Guards against crashing while already handling a crash - the second one falls through to
// the OS rather than recursing or racing the first.
std::atomic<bool> g_bHandlingCrash{false};

std::wstring CrashDumpDirectory()
{
	return AppDataSubdirectory(L"crashes");
}

std::wstring FormatTimestamp()
{
	SYSTEMTIME st;
	GetLocalTime(&st);

	wchar_t buffer[32];
	swprintf_s(buffer, L"%04u%02u%02u_%02u%02u%02u", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

	return buffer;
}

// "<module>+0x1A2B3C" - the one piece of a raw crash that is useful against a disassembly
// without loading the dump first.
std::wstring ModuleRelativeOffset(void *pAddress)
{
	const HMODULE hModule = GetModuleHandleW(nullptr);

	wchar_t modulePath[MAX_PATH]{};
	GetModuleFileNameW(hModule, modulePath, ARRAYSIZE(modulePath));

	const wchar_t *pBaseName = wcsrchr(modulePath, L'\\');
	pBaseName = pBaseName != nullptr ? pBaseName + 1 : modulePath;

	const auto base = reinterpret_cast<uptr>(hModule);
	const auto address = reinterpret_cast<uptr>(pAddress);

	wchar_t buffer[MAX_PATH + 32];
	if (address >= base) {
		swprintf_s(buffer, L"%s+0x%llX", pBaseName, static_cast<unsigned long long>(address - base));
	} else {
		swprintf_s(buffer, L"%s (address outside module: 0x%p)", pBaseName, pAddress);
	}

	return buffer;
}

// A null pExceptionPointers is meaningful: MiniDumpWriteDump reads it as "capture the
// current state of every thread", which is the right fallback for the non-SEH paths and for
// a hang dump.
std::wstring WriteMiniDump(EXCEPTION_POINTERS *pExceptionPointers, const std::wstring &dumpDirectory,
						   const wchar_t *pTag, MINIDUMP_TYPE dumpType)
{
	if (dumpDirectory.empty()) return L"";

	const std::wstring path = dumpDirectory + L"\\" + kAppNameW + L"_" + pTag + L"_" + FormatTimestamp() + L".dmp";
	const HANDLE hFile =
		CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hFile == INVALID_HANDLE_VALUE) return L"";

	MINIDUMP_EXCEPTION_INFORMATION exceptionInfo{};
	exceptionInfo.ThreadId = GetCurrentThreadId();
	exceptionInfo.ExceptionPointers = pExceptionPointers;
	exceptionInfo.ClientPointers = FALSE;

	const BOOL written = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, dumpType,
										   pExceptionPointers != nullptr ? &exceptionInfo : nullptr, nullptr, nullptr);
	CloseHandle(hFile);

	return written != FALSE ? path : L"";
}

// Returning S_FALSE keeps the dialog open, so Open Crash Folder does not double as a way out
// of it.
HRESULT CALLBACK CrashDialogCallback(HWND hWnd, UINT msg, WPARAM wParam, LPARAM /*lParam*/, LONG_PTR lpRefData)
{
	if (msg != TDN_BUTTON_CLICKED || wParam != kOpenFolderButtonId) return S_OK;

	const auto *pDumpDirectory = reinterpret_cast<const wchar_t *>(lpRefData);
	if (pDumpDirectory != nullptr && pDumpDirectory[0] != L'\0') {
		ShellExecuteW(hWnd, L"open", pDumpDirectory, nullptr, nullptr, SW_SHOWNORMAL);
	}

	return S_FALSE;
}

std::wstring BuildCrashDialogText(const std::wstring &reason, const std::wstring &offset, const std::wstring &dumpPath)
{
	std::wstring text = L"An unexpected error occurred and the app needs to close. A crash report has been saved "
						L"locally.\n\nError: " +
						reason + L"\nLocation: " + offset;

	text += dumpPath.empty() ? std::wstring(L"\n\nThe crash report itself could not be saved.")
							 : (L"\n\nSaved to:\n" + dumpPath);

	return text;
}

// A TaskDialog rather than MessageBoxW, for the Open Crash Folder button its fixed button
// sets cannot provide. No IDCANCEL and no TDF_ALLOW_DIALOG_CANCELLATION, so Close is the only
// way out. hwndParent is null on purpose: this app's window may be in an arbitrary state
// by the time this runs.
void ShowCrashDialog(const std::wstring &reason, const std::wstring &offset, const std::wstring &dumpPath,
					 const std::wstring &dumpDirectory)
{
	const std::wstring content = BuildCrashDialogText(reason, offset, dumpPath);

	const TASKDIALOG_BUTTON buttons[]{
		{kOpenFolderButtonId, L"Open Crash Folder"},
		{kCloseButtonId, L"Close"},
	};

	const std::wstring instruction = std::wstring(kAppNameW) + L" has stopped working";

	TASKDIALOGCONFIG config{};
	config.cbSize = sizeof(config);
	config.hwndParent = nullptr;
	config.dwFlags = TDF_SIZE_TO_CONTENT;
	config.pszWindowTitle = kAppNameW;
	config.pszMainIcon = TD_ERROR_ICON;
	config.pszMainInstruction = instruction.c_str();
	config.pszContent = content.c_str();
	config.pButtons = buttons;
	config.cButtons = ARRAYSIZE(buttons);
	config.nDefaultButton = kCloseButtonId;
	config.pfCallback = CrashDialogCallback;
	config.lpCallbackData = reinterpret_cast<LONG_PTR>(dumpDirectory.c_str());

	int selectedButton = 0;
	TaskDialogIndirect(&config, &selectedButton, nullptr, nullptr);
}

const wchar_t *ExceptionCodeName(DWORD code)
{
	switch (code) {
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

LONG WINAPI UnhandledExceptionFilterProc(EXCEPTION_POINTERS *pExceptionPointers)
{
	bool expected = false;
	if (!g_bHandlingCrash.compare_exchange_strong(expected, true)) return EXCEPTION_CONTINUE_SEARCH;

	const DWORD code = pExceptionPointers->ExceptionRecord->ExceptionCode;

	// A stack overflow leaves almost no stack for the rest of this handler; reclaiming the
	// guard page buys back enough room for std::wstring and DbgHelp below.
	if (code == EXCEPTION_STACK_OVERFLOW) {
		_resetstkoflw();
	}

	const std::wstring reason = ExceptionCodeName(code);
	const std::wstring offset = ModuleRelativeOffset(pExceptionPointers->ExceptionRecord->ExceptionAddress);
	const std::wstring dumpDirectory = CrashDumpDirectory();
	const std::wstring dumpPath = WriteMiniDump(pExceptionPointers, dumpDirectory, L"crash", kCrashDumpType);

	ShowCrashDialog(reason, offset, dumpPath, dumpDirectory);

	return EXCEPTION_EXECUTE_HANDLER; // terminate now, without WER's dialog on top of ours
}

// The non-SEH paths hand over no EXCEPTION_POINTERS, so this synthesizes one from the current
// register state. Its instruction pointer is not where the underlying problem started, but it
// is where the process actually stopped being able to continue.
[[noreturn]] void HandleFatalCondition(const wchar_t *pReason)
{
	bool expected = false;
	if (!g_bHandlingCrash.compare_exchange_strong(expected, true)) {
		std::abort();
	}

	CONTEXT context{};
	RtlCaptureContext(&context);

	EXCEPTION_RECORD exceptionRecord{};
	exceptionRecord.ExceptionCode = STATUS_FATAL_APP_EXIT;
#if defined(_M_X64)
	exceptionRecord.ExceptionAddress = reinterpret_cast<void *>(context.Rip);
#elif defined(_M_IX86)
	exceptionRecord.ExceptionAddress = reinterpret_cast<void *>(context.Eip);
#endif

	EXCEPTION_POINTERS exceptionPointers{&exceptionRecord, &context};

	const std::wstring offset = ModuleRelativeOffset(exceptionRecord.ExceptionAddress);
	const std::wstring dumpDirectory = CrashDumpDirectory();
	const std::wstring dumpPath = WriteMiniDump(&exceptionPointers, dumpDirectory, L"crash", kCrashDumpType);

	ShowCrashDialog(pReason, offset, dumpPath, dumpDirectory);

	std::abort();
}

// std::terminate has several real causes and this path knows none of them for certain, so it
// reports only what it does know.
[[noreturn]] void TerminateHandler()
{
	HandleFatalCondition(L"Unhandled exception");
}

[[noreturn]] void __cdecl PureCallHandler()
{
	HandleFatalCondition(L"Pure virtual function call");
}

void __cdecl InvalidParameterHandler(const wchar_t *, const wchar_t *, const wchar_t *, unsigned int, uptr)
{
	HandleFatalCondition(L"CRT invalid parameter");
}
} // namespace

void InstallCrashHandler()
{
	// ShowCrashDialog replaces both of Windows' own crash UIs, so a user sees exactly one
	// dialog and it is the one with useful information in it.
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
	_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);

	SetUnhandledExceptionFilter(UnhandledExceptionFilterProc);
	std::set_terminate(TerminateHandler);
	_set_purecall_handler(PureCallHandler);
	_set_invalid_parameter_handler(InvalidParameterHandler);
}

std::wstring WriteDiagnosticDump(const wchar_t *pTag)
{
	// Outside g_bHandlingCrash: a diagnostic dump is not a crash, and taking one
	// must never make a later real crash fall through unhandled.
	const wchar_t *pResolvedTag = pTag != nullptr && pTag[0] != L'\0' ? pTag : L"diagnostic";

	return WriteMiniDump(nullptr, CrashDumpDirectory(), pResolvedTag, kHangDumpType);
}
