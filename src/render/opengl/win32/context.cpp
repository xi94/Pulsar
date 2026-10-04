#include "render/opengl/context.h"

#include <Windows.h>

#include "os/win32/win32.h"
#include "os/window.h"

namespace {
constexpr int K_WGL_CONTEXT_MAJOR_VERSION_ARB    = 0x2091;
constexpr int K_WGL_CONTEXT_MINOR_VERSION_ARB    = 0x2092;
constexpr int K_WGL_CONTEXT_PROFILE_MASK_ARB     = 0x9126;
constexpr int K_WGL_CONTEXT_CORE_PROFILE_BIT_ARB = 0x0001;
constexpr int K_REQUIRED_MAJOR_VERSION           = 3;
constexpr int K_REQUIRED_MINOR_VERSION           = 3;

using CreateContextAttribs = HGLRC(WINAPI*)(HDC, HGLRC, const int*);
using SwapInterval         = BOOL(WINAPI*)(int);

[[nodiscard]] auto is_valid_proc(PROC t_proc) -> bool
{
	const auto value = reinterpret_cast<uptr>(t_proc);

	// wglGetProcAddress signals failure with 1, 2, 3 or -1 on some drivers instead of null.
	return value > 3 && value != static_cast<uptr>(-1);
}
}

struct GlContext::Native {
	HWND    window         = nullptr;
	HDC     device_context = nullptr;
	HGLRC   context        = nullptr;
	HMODULE library        = nullptr;
};

GlContext::GlContext()
	: m_native(std::make_unique<Native>())
{
}

GlContext::~GlContext()
{
	Native* native = m_native.get();

	if (native->context != nullptr) {
		wglMakeCurrent(nullptr, nullptr);
		wglDeleteContext(native->context);
	}

	if (native->device_context != nullptr) {
		ReleaseDC(native->window, native->device_context);
	}

	if (native->library != nullptr) {
		FreeLibrary(native->library);
	}
}

auto GlContext::create(const os::Window* t_window) -> bool
{
	Native* native         = m_native.get();
	native->window         = os::win32::window_handle(t_window);
	native->device_context = GetDC(native->window);
	native->library        = LoadLibraryW(L"opengl32.dll");
	if (native->device_context == nullptr || native->library == nullptr) return false;

	const PIXELFORMATDESCRIPTOR format{
		.nSize      = sizeof(PIXELFORMATDESCRIPTOR),
		.nVersion   = 1,
		.dwFlags    = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
		.iPixelType = PFD_TYPE_RGBA,
		.cColorBits = 32,
		.cAlphaBits = 8,
		.iLayerType = PFD_MAIN_PLANE,
	};

	// A window keeps its first pixel format for life, so a second attempt on the same window reuses it.
	if (GetPixelFormat(native->device_context) == 0) {
		const int index = ChoosePixelFormat(native->device_context, &format);
		if (index == 0 || !SetPixelFormat(native->device_context, index, &format)) return false;
	}

	const HGLRC legacy = wglCreateContext(native->device_context);
	if (legacy == nullptr) return false;

	if (wglMakeCurrent(native->device_context, legacy)) {
		const PROC create_proc = wglGetProcAddress("wglCreateContextAttribsARB");
		const int  attributes[]{
			K_WGL_CONTEXT_MAJOR_VERSION_ARB,
			K_REQUIRED_MAJOR_VERSION,
			K_WGL_CONTEXT_MINOR_VERSION_ARB,
			K_REQUIRED_MINOR_VERSION,
			K_WGL_CONTEXT_PROFILE_MASK_ARB,
			K_WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
			0,
		};

		if (is_valid_proc(create_proc)) {
			native->context = reinterpret_cast<CreateContextAttribs>(create_proc)(native->device_context, nullptr, attributes);
		}
	}

	wglMakeCurrent(nullptr, nullptr);
	wglDeleteContext(legacy);

	if (native->context == nullptr || !wglMakeCurrent(native->device_context, native->context)) return false;

	if (const PROC swap_proc = wglGetProcAddress("wglSwapIntervalEXT"); is_valid_proc(swap_proc)) {
		reinterpret_cast<SwapInterval>(swap_proc)(1);
	}

	return true;
}

auto GlContext::resize() -> void {}

auto GlContext::present() -> void
{
	SwapBuffers(m_native->device_context);
}

auto GlContext::proc_address(const char* t_name) const -> void*
{
	const PROC proc = wglGetProcAddress(t_name);
	if (is_valid_proc(proc)) return reinterpret_cast<void*>(proc);

	return reinterpret_cast<void*>(GetProcAddress(m_native->library, t_name));
}
