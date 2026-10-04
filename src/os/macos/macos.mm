#include "os/macos/macos.h"

#include <utility>

#include "core/app_identity.h"
#include "core/file.h"
#include "os/window.h"

@interface PulsarApplicationDelegate : NSObject <NSApplicationDelegate>
@end

namespace {
PulsarApplicationDelegate* g_application_delegate = nil;
bool                       g_quit_requested       = false;
bool                       g_reopen_requested     = false;

auto build_main_menu() -> void
{
	NSString* name = os::macos::to_ns_string(K_APP_NAME);

	NSMenu* application_menu = [[NSMenu alloc] initWithTitle:name];
	[application_menu addItemWithTitle:[@"About " stringByAppendingString:name] action:@selector(orderFrontStandardAboutPanel:) keyEquivalent:@""];
	[application_menu addItem:NSMenuItem.separatorItem];
	[application_menu addItemWithTitle:[@"Hide " stringByAppendingString:name] action:@selector(hide:) keyEquivalent:@"h"];
	NSMenuItem* hide_others               = [application_menu addItemWithTitle:@"Hide Others" action:@selector(hideOtherApplications:) keyEquivalent:@"h"];
	hide_others.keyEquivalentModifierMask = NSEventModifierFlagOption | NSEventModifierFlagCommand;
	[application_menu addItemWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""];
	[application_menu addItem:NSMenuItem.separatorItem];
	[application_menu addItemWithTitle:[@"Quit " stringByAppendingString:name] action:@selector(terminate:) keyEquivalent:@"q"];

	NSMenu* window_menu = [[NSMenu alloc] initWithTitle:@"Window"];
	[window_menu addItemWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
	[window_menu addItemWithTitle:@"Zoom" action:@selector(performZoom:) keyEquivalent:@""];
	[window_menu addItem:NSMenuItem.separatorItem];
	[window_menu addItemWithTitle:@"Close" action:@selector(performClose:) keyEquivalent:@"w"];

	NSMenu* menu_bar                                                           = [[NSMenu alloc] init];
	[menu_bar addItemWithTitle:@"" action:nil keyEquivalent:@""].submenu       = application_menu;
	[menu_bar addItemWithTitle:@"Window" action:nil keyEquivalent:@""].submenu = window_menu;

	NSApp.mainMenu    = menu_bar;
	NSApp.windowsMenu = window_menu;
}
}

@implementation PulsarApplicationDelegate

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)t_sender
{
	g_quit_requested = true;
	os::macos::wake_event_loop();

	return NSTerminateCancel;
}

- (BOOL)applicationShouldHandleReopen:(NSApplication*)t_sender hasVisibleWindows:(BOOL)t_has_visible_windows
{
	g_reopen_requested = true;

	return NO;
}

@end

namespace os::macos {

auto to_ns_string(std::string_view t_utf8) -> NSString*
{
	NSString* string = [[NSString alloc] initWithBytes:t_utf8.data() length:t_utf8.size() encoding:NSUTF8StringEncoding];

	return string != nil ? string : @"";
}

auto to_utf8(NSString* t_string) -> std::string
{
	const char* utf8 = t_string.UTF8String;

	return utf8 != nullptr ? std::string{utf8} : std::string{};
}

auto window_handle(const Window* t_window) -> NSWindow*
{
	return (__bridge NSWindow*)t_window->native_handle();
}

auto prepare_application() -> void
{
	if (g_application_delegate != nil) return;

	[NSApplication sharedApplication];
	g_application_delegate = [[PulsarApplicationDelegate alloc] init];
	NSApp.delegate         = g_application_delegate;
	[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
	build_main_menu();
	[NSApp finishLaunching];
}

auto activate_application() -> void
{
	if (@available(macOS 14.0, *)) {
		[NSApp activate];
	} else {
		[NSApp activateIgnoringOtherApps:YES];
	}
}

auto wake_event_loop() -> void
{
	NSEvent* wake = [NSEvent otherEventWithType:NSEventTypeApplicationDefined
									   location:NSZeroPoint
								  modifierFlags:0
									  timestamp:0
								   windowNumber:0
										context:nil
										subtype:0
										  data1:0
										  data2:0];
	[NSApp postEvent:wake atStart:NO];
}

auto take_quit_request() -> bool
{
	return std::exchange(g_quit_requested, false);
}

auto take_reopen_request() -> bool
{
	return std::exchange(g_reopen_requested, false);
}

auto lock_file_path(std::string_view t_name) -> std::string
{
	return joined_path(app_data_subdirectory(""), t_name);
}

auto notification_name(std::string_view t_event) -> NSString*
{
	return to_ns_string(std::string{PULSAR_BUNDLE_IDENTIFIER "."} + std::string{t_event});
}

}
