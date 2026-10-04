#include "os/clipboard.h"

#include "os/macos/macos.h"

namespace {
NSPasteboardType const K_CONCEALED_TYPE = @"org.nspasteboard.ConcealedType";
NSPasteboardType const K_TRANSIENT_TYPE = @"org.nspasteboard.TransientType";

auto put_text(std::string_view t_text, bool t_keep_out_of_history) -> void
{
	NSPasteboard* pasteboard = NSPasteboard.generalPasteboard;
	[pasteboard prepareForNewContentsWithOptions:t_keep_out_of_history ? NSPasteboardContentsCurrentHostOnly : 0];
	[pasteboard setString:os::macos::to_ns_string(t_text) forType:NSPasteboardTypeString];

	if (t_keep_out_of_history) {
		[pasteboard setData:[NSData data] forType:K_CONCEALED_TYPE];
		[pasteboard setData:[NSData data] forType:K_TRANSIENT_TYPE];
	}
}
}

namespace os {

auto set_clipboard_text(std::string_view t_text) -> void
{
	put_text(t_text, false);
}

auto set_clipboard_secret(std::string_view t_secret) -> void
{
	put_text(t_secret, true);
}

auto clipboard_text() -> std::string
{
	return macos::to_utf8([NSPasteboard.generalPasteboard stringForType:NSPasteboardTypeString]);
}

auto clipboard_has_text() -> bool
{
	return [NSPasteboard.generalPasteboard availableTypeFromArray:@[ NSPasteboardTypeString ]] != nil;
}

auto clipboard_sequence() -> u32
{
	return static_cast<u32>(NSPasteboard.generalPasteboard.changeCount);
}

auto clear_clipboard_if_unchanged(u32 t_sequence) -> void
{
	NSPasteboard* pasteboard = NSPasteboard.generalPasteboard;

	if (static_cast<u32>(pasteboard.changeCount) == t_sequence) {
		[pasteboard clearContents];
	}
}

}
