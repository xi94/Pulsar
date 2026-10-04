#include "os/path_picker.h"

#include <utility>

#include <fnmatch.h>

#include "os/macos/macos.h"
#include "os/window.h"

namespace {
struct PickState {
	bool                       open     = false;
	bool                       finished = false;
	std::optional<std::string> result;
};

[[nodiscard]] auto nearest_existing_folder(NSString* t_path) -> NSURL*
{
	BOOL is_folder = NO;

	while (t_path.length > 1 && !([NSFileManager.defaultManager fileExistsAtPath:t_path isDirectory:&is_folder] && is_folder)) {
		t_path = t_path.stringByDeletingLastPathComponent;
	}

	return t_path.length > 1 ? [NSURL fileURLWithPath:t_path isDirectory:YES] : nil;
}
}

@interface PulsarPathFilter : NSObject <NSOpenSavePanelDelegate>
- (instancetype)initWithPattern:(const char*)t_pattern;
@end

@implementation PulsarPathFilter {
	std::string m_pattern;
}

- (instancetype)initWithPattern:(const char*)t_pattern
{
	self = [super init];
	if (self != nil) {
		m_pattern = t_pattern;
	}

	return self;
}

- (BOOL)panel:(id)t_sender shouldEnableURL:(NSURL*)t_url
{
	NSNumber* is_folder  = nil;
	NSNumber* is_package = nil;
	[t_url getResourceValue:&is_folder forKey:NSURLIsDirectoryKey error:nil];
	[t_url getResourceValue:&is_package forKey:NSURLIsPackageKey error:nil];

	if (is_folder.boolValue && !is_package.boolValue) return YES;

	return fnmatch(m_pattern.c_str(), t_url.lastPathComponent.fileSystemRepresentation, FNM_CASEFOLD) == 0;
}

@end

namespace os {

struct PathPicker::Native {
	NSOpenPanel*               panel  = nil;
	PulsarPathFilter*          filter = nil;
	std::shared_ptr<PickState> state  = std::make_shared<PickState>();
};

PathPicker::PathPicker()
	: m_native(std::make_unique<Native>())
{
}

PathPicker::~PathPicker()
{
	if (m_native->state->open) {
		[m_native->panel cancel:nil];
	}
}

auto PathPicker::open(const Window* t_owner, PathRequest t_request) -> void
{
	if (is_open()) return;

	Native* native = m_native.get();

	NSOpenPanel* panel            = [NSOpenPanel openPanel];
	panel.canChooseDirectories    = t_request.kind == PathKind::Folder;
	panel.canChooseFiles          = t_request.kind == PathKind::File;
	panel.canCreateDirectories    = t_request.kind == PathKind::Folder;
	panel.allowsMultipleSelection = NO;
	panel.title                   = macos::to_ns_string(t_request.title);
	panel.message                 = panel.title;
	panel.directoryURL            = nearest_existing_folder(macos::to_ns_string(t_request.start_path));

	if (t_request.ok_label != nullptr) {
		panel.prompt = macos::to_ns_string(t_request.ok_label);
	}

	native->filter = t_request.file_type_pattern != nullptr ? [[PulsarPathFilter alloc] initWithPattern:t_request.file_type_pattern] : nil;
	panel.delegate = native->filter;
	native->panel  = panel;

	const std::shared_ptr<PickState> state = native->state;
	state->open                            = true;
	state->finished                        = false;
	state->result.reset();

	const auto completion = ^(NSModalResponse t_response) {
		if (t_response == NSModalResponseOK && panel.URL != nil) {
			state->result = macos::to_utf8(panel.URL.path);
		}

		state->open     = false;
		state->finished = true;
	};

	NSWindow* owner = t_owner != nullptr ? macos::window_handle(t_owner) : nil;

	if (owner != nil && owner.isVisible) {
		[panel beginSheetModalForWindow:owner completionHandler:completion];
	} else {
		[panel beginWithCompletionHandler:completion];
	}
}

auto PathPicker::is_open() const -> bool
{
	return m_native->state->open;
}

auto PathPicker::take_result() -> std::optional<std::string>
{
	PickState* state = m_native->state.get();
	if (!state->finished) return std::nullopt;

	state->finished = false;

	return std::exchange(state->result, std::nullopt);
}

}
