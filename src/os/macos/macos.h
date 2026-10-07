#pragma once

// MacTypes.h, which every Apple framework pulls in, declares a QuickDraw Rect that collides with Pulsar's Rect. Apple headers are therefore
// only included here, with that Rect renamed while they are read.
#define Rect QuickDrawRect
#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>
#import <CoreText/CoreText.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <Carbon/Carbon.h>
#undef Rect

#include <functional>
#include <string>
#include <string_view>

#include "core/types.h"
#include "os/input.h"

namespace os {
class Window;
}

namespace os::macos {

[[nodiscard]] auto to_ns_string(std::string_view t_utf8) -> NSString*;
[[nodiscard]] auto to_utf8(NSString* t_string) -> std::string;

[[nodiscard]] auto window_handle(const Window* t_window) -> NSWindow*;
[[nodiscard]] auto key_from_event(NSEvent* t_event) -> Key;

auto prepare_application() -> void;
auto activate_application() -> void;
auto wake_event_loop() -> void;
auto set_session_end_handler(std::function<void()> t_handler) -> void;
[[nodiscard]] auto take_quit_request() -> bool;
[[nodiscard]] auto take_reopen_request() -> bool;

[[nodiscard]] auto lock_file_path(std::string_view t_name) -> std::string;
[[nodiscard]] auto notification_name(std::string_view t_event) -> NSString*;

}
