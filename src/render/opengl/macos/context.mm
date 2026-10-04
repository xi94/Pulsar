#include "render/opengl/context.h"

#include "os/macos/macos.h"

#include <dlfcn.h>

#include "os/window.h"

// Apple deprecated OpenGL as a whole; this backend exists for Macs and VMs without Metal.
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

namespace {
constexpr const char* K_OPENGL_FRAMEWORK_PATH = "/System/Library/Frameworks/OpenGL.framework/OpenGL";
}

struct GlContext::Native {
	NSOpenGLContext* context = nil;
	NSView*          view    = nil;
	void*            library = nullptr;
};

GlContext::GlContext()
	: m_native(std::make_unique<Native>())
{
}

GlContext::~GlContext()
{
	if (m_native->context != nil) {
		[NSOpenGLContext clearCurrentContext];
		[m_native->context clearDrawable];
	}

	if (m_native->library != nullptr) {
		dlclose(m_native->library);
	}
}

auto GlContext::create(const os::Window* t_window) -> bool
{
	const NSOpenGLPixelFormatAttribute attributes[]{
		NSOpenGLPFAOpenGLProfile,
		NSOpenGLProfileVersion3_2Core,
		NSOpenGLPFADoubleBuffer,
		NSOpenGLPFAColorSize,
		24,
		NSOpenGLPFAAlphaSize,
		8,
		NSOpenGLPFAAllowOfflineRenderers,
		0,
	};

	NSOpenGLPixelFormat* format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
	if (format == nil) return false;

	m_native->context = [[NSOpenGLContext alloc] initWithFormat:format shareContext:nil];
	m_native->library = dlopen(K_OPENGL_FRAMEWORK_PATH, RTLD_LAZY | RTLD_LOCAL);
	if (m_native->context == nil || m_native->library == nullptr) return false;

	m_native->view                                  = os::macos::window_handle(t_window).contentView;
	m_native->view.wantsBestResolutionOpenGLSurface = YES;

	const GLint swap_interval = 1;
	[m_native->context setValues:&swap_interval forParameter:NSOpenGLContextParameterSwapInterval];
	[m_native->context setView:m_native->view];
	[m_native->context makeCurrentContext];

	return true;
}

auto GlContext::resize() -> void
{
	[m_native->context update];
}

auto GlContext::present() -> void
{
	// A context cannot attach to a view whose window is not on screen yet, so the attachment is retried once the window shows.
	if (m_native->context.view == nil && m_native->view.window.isVisible) {
		[m_native->context setView:m_native->view];
	}

	[m_native->context flushBuffer];
}

auto GlContext::proc_address(const char* t_name) const -> void*
{
	return dlsym(m_native->library, t_name);
}
