#include "os/window.h"

#include <cerrno>
#include <chrono>
#include <cmath>
#include <string>
#include <thread>
#include <utility>

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

#include "core/app_identity.h"
#include "os/macos/macos.h"

namespace {
constexpr float   K_PRECISE_SCROLL_POINTS_PER_NOTCH = 48.0f;
constexpr CGFloat K_WINDOW_BUTTON_SPACING           = 20.0;
constexpr auto    K_ACTIVATE_EXISTING_TIMEOUT       = std::chrono::milliseconds(3000);
constexpr auto    K_ACTIVATE_EXISTING_POLL          = std::chrono::milliseconds(100);

constexpr NSWindowButton K_WINDOW_BUTTONS[]{NSWindowCloseButton, NSWindowMiniaturizeButton, NSWindowZoomButton};

class ViewEvents {
  public:
	virtual auto mouse_down(NSEvent* t_event) -> void     = 0;
	virtual auto mouse_up(NSEvent* t_event) -> void       = 0;
	virtual auto mouse_moved(NSEvent* t_event) -> void    = 0;
	virtual auto mouse_exited() -> void                   = 0;
	virtual auto right_mouse_up(NSEvent* t_event) -> void = 0;
	virtual auto scroll_wheel(NSEvent* t_event) -> void   = 0;
	virtual auto key_down(NSEvent* t_event) -> void       = 0;
	virtual auto insert_text(NSString* t_text) -> void    = 0;
	virtual auto cursor_update() -> void                  = 0;
	virtual auto frame_changed() -> void                  = 0;
	virtual auto backing_changed() -> void                = 0;
	virtual auto window_resized() -> void                 = 0;
	virtual auto should_close() -> void                   = 0;

  protected:
	~ViewEvents() = default;
};

[[nodiscard]] auto window_lock_name(os::WindowKind t_kind) -> std::string_view
{
	return t_kind == os::WindowKind::Dialog ? "setup-window.lock" : "main-window.lock";
}

[[nodiscard]] auto activation_event(os::WindowKind t_kind) -> std::string_view
{
	return t_kind == os::WindowKind::Dialog ? "activate-setup" : "activate-main";
}

[[nodiscard]] auto system_cursor(CursorKind t_cursor) -> NSCursor*
{
	switch (t_cursor) {
		case CursorKind::Hand: {
			return NSCursor.pointingHandCursor;
		}

		case CursorKind::IBeam: {
			return NSCursor.IBeamCursor;
		}

		case CursorKind::Move: {
			return NSCursor.openHandCursor;
		}

		case CursorKind::Arrow:
		case CursorKind::Drag: {
			break;
		}
	}

	return NSCursor.arrowCursor;
}

[[nodiscard]] auto plain_string(id t_text) -> NSString*
{
	return [t_text isKindOfClass:NSAttributedString.class] ? static_cast<NSAttributedString*>(t_text).string : static_cast<NSString*>(t_text);
}
}

@interface PulsarView : NSView <NSTextInputClient, NSWindowDelegate>
- (instancetype)initWithEvents:(ViewEvents*)t_events;
- (void)detach;
@end

@implementation PulsarView {
	ViewEvents*      m_events;
	NSMutableString* m_marked_text;
}

- (instancetype)initWithEvents:(ViewEvents*)t_events
{
	self = [super initWithFrame:NSZeroRect];
	if (self == nil) return nil;

	m_events      = t_events;
	m_marked_text = [[NSMutableString alloc] init];

	const NSTrackingAreaOptions options =
		NSTrackingMouseMoved | NSTrackingMouseEnteredAndExited | NSTrackingCursorUpdate | NSTrackingActiveAlways | NSTrackingInVisibleRect;
	[self addTrackingArea:[[NSTrackingArea alloc] initWithRect:NSZeroRect options:options owner:self userInfo:nil]];

	return self;
}

- (void)detach
{
	m_events = nullptr;
}

- (BOOL)isFlipped
{
	return YES;
}

- (BOOL)acceptsFirstResponder
{
	return YES;
}

- (BOOL)acceptsFirstMouse:(NSEvent*)t_event
{
	return YES;
}

- (BOOL)mouseDownCanMoveWindow
{
	return NO;
}

- (void)setFrameSize:(NSSize)t_size
{
	[super setFrameSize:t_size];
	if (m_events != nullptr) m_events->frame_changed();
}

- (void)viewDidChangeBackingProperties
{
	[super viewDidChangeBackingProperties];
	if (m_events != nullptr) m_events->backing_changed();
}

- (void)mouseDown:(NSEvent*)t_event
{
	if (m_events != nullptr) m_events->mouse_down(t_event);
}

- (void)mouseUp:(NSEvent*)t_event
{
	if (m_events != nullptr) m_events->mouse_up(t_event);
}

- (void)mouseDragged:(NSEvent*)t_event
{
	if (m_events != nullptr) m_events->mouse_moved(t_event);
}

- (void)mouseMoved:(NSEvent*)t_event
{
	if (m_events != nullptr) m_events->mouse_moved(t_event);
}

- (void)mouseEntered:(NSEvent*)t_event
{
	if (m_events != nullptr) m_events->mouse_moved(t_event);
}

- (void)mouseExited:(NSEvent*)t_event
{
	if (m_events != nullptr) m_events->mouse_exited();
}

- (void)rightMouseUp:(NSEvent*)t_event
{
	if (m_events != nullptr) m_events->right_mouse_up(t_event);
}

- (void)scrollWheel:(NSEvent*)t_event
{
	if (m_events != nullptr) m_events->scroll_wheel(t_event);
}

- (void)keyDown:(NSEvent*)t_event
{
	if (m_events != nullptr) m_events->key_down(t_event);
}

- (void)cursorUpdate:(NSEvent*)t_event
{
	if (m_events != nullptr) m_events->cursor_update();
}

- (BOOL)windowShouldClose:(NSWindow*)t_window
{
	if (m_events != nullptr) m_events->should_close();

	return NO;
}

- (void)windowDidResize:(NSNotification*)t_notification
{
	if (m_events != nullptr) m_events->window_resized();
}

- (void)windowDidExitFullScreen:(NSNotification*)t_notification
{
	if (m_events != nullptr) m_events->window_resized();
}

- (void)insertText:(id)t_text replacementRange:(NSRange)t_range
{
	[m_marked_text setString:@""];
	if (m_events != nullptr) m_events->insert_text(plain_string(t_text));
}

- (void)doCommandBySelector:(SEL)t_selector
{
}

- (void)setMarkedText:(id)t_text selectedRange:(NSRange)t_selected replacementRange:(NSRange)t_replacement
{
	[m_marked_text setString:plain_string(t_text)];
}

- (void)unmarkText
{
	[m_marked_text setString:@""];
}

- (NSRange)selectedRange
{
	return NSMakeRange(NSNotFound, 0);
}

- (NSRange)markedRange
{
	return m_marked_text.length > 0 ? NSMakeRange(0, m_marked_text.length) : NSMakeRange(NSNotFound, 0);
}

- (BOOL)hasMarkedText
{
	return m_marked_text.length > 0;
}

- (NSAttributedString*)attributedSubstringForProposedRange:(NSRange)t_range actualRange:(NSRangePointer)t_actual
{
	return nil;
}

- (NSArray<NSAttributedStringKey>*)validAttributesForMarkedText
{
	return @[];
}

- (NSRect)firstRectForCharacterRange:(NSRange)t_range actualRange:(NSRangePointer)t_actual
{
	const NSPoint mouse = NSEvent.mouseLocation;

	return NSMakeRect(mouse.x, mouse.y, 0.0, 0.0);
}

- (NSUInteger)characterIndexForPoint:(NSPoint)t_point
{
	return NSNotFound;
}

@end

namespace os {

struct Window::Native final : ViewEvents {
	Window*     owner               = nullptr;
	NSWindow*   window              = nil;
	PulsarView* view                = nil;
	id          activation_observer = nil;
	int         window_lock         = -1;
	Vec2        restored_size{};
	bool        mouse_inside  = false;
	bool        control_click = false;

	auto mouse_down(NSEvent* t_event) -> void override;
	auto mouse_up(NSEvent* t_event) -> void override;
	auto mouse_moved(NSEvent* t_event) -> void override;
	auto mouse_exited() -> void override;
	auto right_mouse_up(NSEvent* t_event) -> void override;
	auto scroll_wheel(NSEvent* t_event) -> void override;
	auto key_down(NSEvent* t_event) -> void override;
	auto insert_text(NSString* t_text) -> void override;
	auto cursor_update() -> void override;
	auto frame_changed() -> void override;
	auto backing_changed() -> void override;
	auto window_resized() -> void override;
	auto should_close() -> void override;

	[[nodiscard]] auto point_of(NSEvent* t_event) const -> Vec2;
	[[nodiscard]] auto is_caption(Vec2 t_point) const -> bool;
	auto push_mouse(InputEventType t_type, Vec2 t_point) -> void;
	auto update_size() -> void;
	auto layout_window_buttons() const -> void;
	auto act_on_title_bar_double_click() const -> void;
	auto apply_cursor() const -> void;
};

Window::Window()
	: m_native(std::make_unique<Native>())
{
	m_native->owner = this;
}

Window::~Window()
{
	Native* native = m_native.get();

	if (native->activation_observer != nil) {
		[NSDistributedNotificationCenter.defaultCenter removeObserver:native->activation_observer];
	}

	if (native->window_lock >= 0) {
		::close(native->window_lock);
	}

	[native->view detach];
	native->window.delegate = nil;
	[native->window close];
}

auto Window::create(std::string_view t_title, u32 t_width, u32 t_height, WindowKind t_kind) -> bool
{
	macos::prepare_application();

	Native* native = m_native.get();
	m_kind         = t_kind;

	NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskFullSizeContentView;
	if (t_kind == WindowKind::Main) {
		style |= NSWindowStyleMaskResizable;
	}

	native->window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0.0, 0.0, t_width, t_height) styleMask:style backing:NSBackingStoreBuffered defer:NO];
	if (native->window == nil) return false;

	NSWindow* window                  = native->window;
	window.title                      = macos::to_ns_string(t_title);
	window.titleVisibility            = NSWindowTitleHidden;
	window.titlebarAppearsTransparent = YES;
	window.releasedWhenClosed         = NO;
	window.tabbingMode                = NSWindowTabbingModeDisallowed;

	[window standardWindowButton:NSWindowZoomButton].hidden = t_kind == WindowKind::Dialog;

	native->view            = [[PulsarView alloc] initWithEvents:native];
	native->view.wantsLayer = YES;
	window.contentView      = native->view;
	window.delegate         = native->view;
	[window makeFirstResponder:native->view];
	[window center];

	native->update_size();
	native->layout_window_buttons();

	const std::string lock_path = macos::lock_file_path(window_lock_name(t_kind));
	native->window_lock         = open(lock_path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
	if (native->window_lock >= 0) {
		flock(native->window_lock, LOCK_EX | LOCK_NB);
	}

	const auto bring_forward = ^(NSNotification*) {
		restore();
		macos::wake_event_loop();
	};

	NSDistributedNotificationCenter* notifications = NSDistributedNotificationCenter.defaultCenter;
	native->activation_observer                    = [notifications addObserverForName:macos::notification_name(activation_event(t_kind))
																				object:nil
																				 queue:NSOperationQueue.mainQueue
																			usingBlock:bring_forward];

	return true;
}

auto Window::set_min_size(Vec2 t_size) -> void
{
	m_min_size                      = t_size;
	m_native->window.contentMinSize = NSMakeSize(t_size.x, t_size.y);
}

auto Window::set_title_bar(float t_height, std::function<bool(Vec2)> t_is_button) -> void
{
	m_title_bar_height    = t_height;
	m_is_title_bar_button = std::move(t_is_button);
	m_native->layout_window_buttons();
}

auto Window::show() -> void
{
	[m_native->window makeKeyAndOrderFront:nil];
	m_native->layout_window_buttons();
	macos::activate_application();
}

auto Window::show_minimized() -> void
{
	[m_native->window miniaturize:nil];
}

auto Window::minimize() -> void
{
	[m_native->window miniaturize:nil];
}

auto Window::toggle_maximized() -> void
{
	[m_native->window zoom:nil];
}

auto Window::restore() -> void
{
	if (m_native->window.isMiniaturized) {
		[m_native->window deminiaturize:nil];
	}

	[m_native->window makeKeyAndOrderFront:nil];
	macos::activate_application();
}

auto Window::bring_to_front() -> void
{
	[m_native->window makeKeyAndOrderFront:nil];
	macos::activate_application();
}

auto Window::close() -> void
{
	if (m_close_to_tray) {
		[m_native->window orderOut:nil];
	} else {
		m_should_quit = true;
	}
}

auto Window::restored_size() const -> Vec2
{
	const Vec2 restored = m_native->restored_size;

	return restored.x > 0.0f && restored.y > 0.0f ? restored : size();
}

auto Window::on_redraw(std::function<void()> t_callback) -> void
{
	m_redraw = std::move(t_callback);
}

auto Window::on_dpi_changed(std::function<void()> t_callback) -> void
{
	m_dpi_changed = std::move(t_callback);
}

auto Window::redraw() -> void
{
	if (m_redraw) {
		m_redraw();
	}
}

auto Window::pump_messages() -> void
{
	m_input_event_count = 0;

	@autoreleasepool {
		for (;;) {
			NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:NSDate.distantPast inMode:NSDefaultRunLoopMode dequeue:YES];
			if (event == nil) break;

			[NSApp sendEvent:event];
		}
	}

	if (macos::take_quit_request()) {
		m_should_quit = true;
	}

	if (macos::take_reopen_request()) {
		restore();
	}
}

auto Window::wait_for_messages(float t_seconds) const -> bool
{
	@autoreleasepool {
		NSDate* deadline = [NSDate dateWithTimeIntervalSinceNow:t_seconds];

		return [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:deadline inMode:NSDefaultRunLoopMode dequeue:NO] != nil;
	}
}

auto Window::native_handle() const -> void*
{
	return (__bridge void*)m_native->window;
}

auto Window::is_hidden() const -> bool
{
	return !m_native->window.isVisible && !m_native->window.isMiniaturized;
}

auto Window::is_focused() const -> bool
{
	return m_native->window.isKeyWindow && NSApp.isActive;
}

auto Window::is_minimized() const -> bool
{
	return m_native->window.isMiniaturized;
}

auto Window::is_maximized() const -> bool
{
	return m_native->window.isZoomed || (m_native->window.styleMask & NSWindowStyleMaskFullScreen) != 0;
}

auto Window::set_cursor(CursorKind t_cursor) -> void
{
	if (t_cursor == m_cursor) return;

	m_cursor = t_cursor;

	if (m_native->mouse_inside) {
		m_native->apply_cursor();
	}
}

auto Window::set_excluded_from_capture(bool t_excluded) -> void
{
	if (t_excluded == m_excluded_from_capture) return;

	m_native->window.sharingType = t_excluded ? NSWindowSharingNone : NSWindowSharingReadOnly;
	m_excluded_from_capture      = t_excluded;
}

auto Window::push_input(const InputEvent& t_event) -> void
{
	if (m_input_event_count >= K_MAX_INPUT_EVENTS) return;

	m_input_events[m_input_event_count] = t_event;
	m_input_event_count += 1;
}

auto Window::Native::mouse_down(NSEvent* t_event) -> void
{
	const Vec2 point = point_of(t_event);

	if (is_caption(point)) {
		if (t_event.clickCount == 2) {
			act_on_title_bar_double_click();
		} else {
			[window performWindowDragWithEvent:t_event];
		}

		return;
	}

	control_click = (t_event.modifierFlags & NSEventModifierFlagControl) != 0;

	if (!control_click) {
		push_mouse(InputEventType::MouseDown, point);
	}
}

auto Window::Native::mouse_up(NSEvent* t_event) -> void
{
	const Vec2 point = point_of(t_event);
	push_mouse(std::exchange(control_click, false) ? InputEventType::RightClick : InputEventType::MouseUp, point);

	if (!NSPointInRect(NSMakePoint(point.x, point.y), view.bounds)) {
		mouse_exited();
	}
}

auto Window::Native::mouse_moved(NSEvent* t_event) -> void
{
	mouse_inside = true;
	push_mouse(InputEventType::MouseMove, point_of(t_event));
}

auto Window::Native::mouse_exited() -> void
{
	// The view keeps receiving drag events while a button is held, so leaving now would make the next drag jump.
	if ((NSEvent.pressedMouseButtons & 1) != 0) return;

	mouse_inside = false;
	push_mouse(InputEventType::MouseMove, Vec2{-1.0f, -1.0f});
}

auto Window::Native::right_mouse_up(NSEvent* t_event) -> void
{
	push_mouse(InputEventType::RightClick, point_of(t_event));
}

auto Window::Native::scroll_wheel(NSEvent* t_event) -> void
{
	const auto  delta_y = static_cast<float>(t_event.scrollingDeltaY);
	const float delta   = t_event.hasPreciseScrollingDeltas ? delta_y / K_PRECISE_SCROLL_POINTS_PER_NOTCH : delta_y;
	if (delta == 0.0f) return;

	owner->push_input(InputEvent{.type = InputEventType::MouseWheel, .position = point_of(t_event), .wheel_delta = delta});
}

auto Window::Native::key_down(NSEvent* t_event) -> void
{
	owner->push_input(InputEvent{.type = InputEventType::KeyDown, .key = macos::key_from_event(t_event)});

	if ((t_event.modifierFlags & (NSEventModifierFlagCommand | NSEventModifierFlagControl)) == 0) {
		[view interpretKeyEvents:@[ t_event ]];
	}
}

auto Window::Native::insert_text(NSString* t_text) -> void
{
	const NSUInteger length = t_text.length;

	for (NSUInteger i = 0; i < length;) {
		const unichar unit      = [t_text characterAtIndex:i];
		u32           codepoint = unit;
		i += 1;

		if (CFStringIsSurrogateHighCharacter(unit) && i < length && CFStringIsSurrogateLowCharacter([t_text characterAtIndex:i])) {
			codepoint = static_cast<u32>(CFStringGetLongCharacterForSurrogatePair(unit, [t_text characterAtIndex:i]));
			i += 1;
		}

		owner->push_input(InputEvent{.type = InputEventType::Character, .codepoint = codepoint});
	}
}

auto Window::Native::cursor_update() -> void
{
	apply_cursor();
}

auto Window::Native::frame_changed() -> void
{
	update_size();

	if (owner->m_width > 0 && owner->m_height > 0) {
		owner->redraw();
	}
}

auto Window::Native::backing_changed() -> void
{
	const auto scale = static_cast<float>(window.backingScaleFactor);
	if (scale == owner->m_dpi_scale) return;

	owner->m_dpi_scale = scale;

	if (owner->m_dpi_changed) {
		owner->m_dpi_changed();
	}

	frame_changed();
}

auto Window::Native::window_resized() -> void
{
	layout_window_buttons();
}

auto Window::Native::should_close() -> void
{
	owner->close();
}

auto Window::Native::point_of(NSEvent* t_event) const -> Vec2
{
	const NSPoint point = [view convertPoint:t_event.locationInWindow fromView:nil];

	return Vec2{static_cast<float>(point.x), static_cast<float>(point.y)};
}

auto Window::Native::is_caption(Vec2 t_point) const -> bool
{
	const bool over_button = owner->m_is_title_bar_button && owner->m_is_title_bar_button(t_point);

	return t_point.y >= 0.0f && t_point.y < owner->m_title_bar_height && !over_button;
}

auto Window::Native::push_mouse(InputEventType t_type, Vec2 t_point) -> void
{
	owner->push_input(InputEvent{.type = t_type, .position = t_point});
}

auto Window::Native::update_size() -> void
{
	const NSRect bounds  = view.bounds;
	const NSRect backing = [view convertRectToBacking:bounds];

	owner->m_dpi_scale       = static_cast<float>(window.backingScaleFactor);
	owner->m_width           = static_cast<u32>(std::lround(NSWidth(bounds)));
	owner->m_height          = static_cast<u32>(std::lround(NSHeight(bounds)));
	owner->m_physical_width  = static_cast<u32>(std::lround(NSWidth(backing)));
	owner->m_physical_height = static_cast<u32>(std::lround(NSHeight(backing)));

	if (!window.isZoomed && (window.styleMask & NSWindowStyleMaskFullScreen) == 0) {
		restored_size = Vec2{static_cast<float>(NSWidth(bounds)), static_cast<float>(NSHeight(bounds))};
	}
}

auto Window::Native::layout_window_buttons() const -> void
{
	const CGFloat height    = owner->m_title_bar_height;
	NSView*       title_bar = [window standardWindowButton:NSWindowCloseButton].superview;
	NSView*       container = title_bar.superview;
	if (height <= 0.0 || container == nil || (window.styleMask & NSWindowStyleMaskFullScreen) != 0) return;

	// AppKit sizes the title bar for its own 28pt caption; growing it keeps the buttons centred in Pulsar's taller one.
	container.frame = NSMakeRect(0.0, NSHeight(window.frame) - height, NSWidth(window.frame), height);
	title_bar.frame = container.bounds;

	CGFloat right = 0.0;

	for (usize i = 0; i < std::size(K_WINDOW_BUTTONS); i += 1) {
		NSButton* button = [window standardWindowButton:K_WINDOW_BUTTONS[i]];
		if (button == nil || button.hidden) continue;

		const CGFloat inset = std::round((height - NSHeight(button.frame)) * 0.5);
		[button setFrameOrigin:NSMakePoint(inset + static_cast<CGFloat>(i) * K_WINDOW_BUTTON_SPACING, inset)];
		right = NSMaxX(button.frame) + inset;
	}

	owner->m_native_controls_width = static_cast<float>(right);
}

auto Window::Native::act_on_title_bar_double_click() const -> void
{
	NSString* action = [NSUserDefaults.standardUserDefaults stringForKey:@"AppleActionOnDoubleClick"];

	if ([action isEqualToString:@"Minimize"]) {
		[window performMiniaturize:nil];
	} else if (![action isEqualToString:@"None"] && owner->m_kind == WindowKind::Main) {
		[window performZoom:nil];
	}
}

auto Window::Native::apply_cursor() const -> void
{
	[system_cursor(owner->m_cursor) set];
}

auto activate_running_instance() -> bool
{
	const auto deadline = std::chrono::steady_clock::now() + K_ACTIVATE_EXISTING_TIMEOUT;

	while (!bring_window_to_front(WindowKind::Main)) {
		if (std::chrono::steady_clock::now() >= deadline) return false;

		std::this_thread::sleep_for(K_ACTIVATE_EXISTING_POLL);
	}

	return true;
}

auto bring_window_to_front(WindowKind t_kind) -> bool
{
	const std::string lock_path = macos::lock_file_path(window_lock_name(t_kind));
	const int         lock      = open(lock_path.c_str(), O_RDONLY | O_CLOEXEC);
	if (lock < 0) return false;

	const bool held_elsewhere = flock(lock, LOCK_EX | LOCK_NB) != 0 && errno == EWOULDBLOCK;
	::close(lock);
	if (!held_elsewhere) return false;

	if (@available(macOS 14.0, *)) {
		[[NSApplication sharedApplication] yieldActivationToApplicationWithBundleIdentifier:@PULSAR_BUNDLE_IDENTIFIER];
	}

	[NSDistributedNotificationCenter.defaultCenter postNotificationName:macos::notification_name(activation_event(t_kind))
																 object:nil
															   userInfo:nil
													 deliverImmediately:YES];

	return true;
}

}
