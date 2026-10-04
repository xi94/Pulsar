#include "os/fonts.h"

#include <algorithm>
#include <utility>

#include "os/macos/macos.h"

namespace {
constexpr std::string_view K_SYSTEM_FONTS_FOLDER = "/System/Library/Fonts/";
constexpr const char*      K_SYSTEM_FONT_NAME    = "System Font";

struct FallbackSample {
	NSString* text;
	NSString* language;
};

[[nodiscard]] auto font_path(CTFontRef t_font) -> NSString*
{
	NSURL* url = CFBridgingRelease(CTFontCopyAttribute(t_font, kCTFontURLAttribute));

	return url.path;
}

[[nodiscard]] auto system_ui_font() -> CTFontRef
{
	return CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, 0.0, nullptr);
}

[[nodiscard]] auto has_outlines(CTFontRef t_font) -> bool
{
	for (const CTFontTableTag table : {kCTFontTableGlyf, kCTFontTableCFF}) {
		if (const CFDataRef data = CTFontCopyTable(t_font, table, kCTFontTableOptionNoOptions); data != nullptr) {
			CFRelease(data);
			return true;
		}
	}

	return false;
}

[[nodiscard]] auto listed_name(NSString* t_path) -> NSString*
{
	NSArray* descriptors = CFBridgingRelease(CTFontManagerCreateFontDescriptorsFromURL((__bridge CFURLRef)[NSURL fileURLWithPath:t_path]));
	if (descriptors.count == 0) return nil;

	const auto descriptor = (__bridge CTFontDescriptorRef)descriptors.firstObject;
	NSString*  family     = CFBridgingRelease(CTFontDescriptorCopyAttribute(descriptor, kCTFontFamilyNameAttribute));
	if (family == nil || [family hasPrefix:@"."]) return nil;

	return CFBridgingRelease(CTFontDescriptorCopyAttribute(descriptor, kCTFontDisplayNameAttribute));
}

[[nodiscard]] auto stored_file(NSString* t_path) -> std::string
{
	std::string path = os::macos::to_utf8(t_path);
	if (path.starts_with(K_SYSTEM_FONTS_FOLDER) && path.find('/', K_SYSTEM_FONTS_FOLDER.size()) == std::string::npos) {
		path.erase(0, K_SYSTEM_FONTS_FOLDER.size());
	}

	return path;
}
}

namespace os {

auto system_fonts() -> std::vector<SystemFont>
{
	NSArray<NSURL*>*         urls = CFBridgingRelease(CTFontManagerCopyAvailableFontURLs());
	NSMutableSet<NSString*>* seen = [NSMutableSet setWithCapacity:urls.count];

	const CTFontRef ui_font      = system_ui_font();
	NSString*       ui_font_path = font_path(ui_font);
	CFRelease(ui_font);

	std::vector<SystemFont> fonts;
	fonts.reserve(urls.count);

	for (NSURL* url in urls) {
		NSString* path = url.path;
		if ([seen containsObject:path]) continue;

		[seen addObject:path];

		NSString* name = [path isEqualToString:ui_font_path] ? @(K_SYSTEM_FONT_NAME) : listed_name(path);
		if (name != nil) {
			fonts.push_back(SystemFont{macos::to_utf8(name), stored_file(path)});
		}
	}

	return fonts;
}

auto system_font_path(std::string_view t_file) -> std::string
{
	if (t_file.starts_with('/')) return std::string{t_file};

	return std::string{K_SYSTEM_FONTS_FOLDER} + std::string{t_file};
}

auto fallback_font_paths() -> std::vector<std::string>
{
	const FallbackSample samples[]{
		{@"ไทย", @"th"}, {@"日本語", @"ja"}, {@"한국어", @"ko"}, {@"繁體中文", @"zh-Hant"}, {@"简体中文", @"zh-Hans"}, {@"⌘★✓♪", nil},
	};

	std::vector<std::string> paths;
	const auto               add = [&paths](CTFontRef t_font) {
		std::string path = macos::to_utf8(font_path(t_font));
		if (path.empty() || !has_outlines(t_font) || std::ranges::find(paths, path) != paths.end()) return;

		paths.push_back(std::move(path));
	};

	const CTFontRef ui_font = system_ui_font();
	add(ui_font);

	for (const FallbackSample& sample : samples) {
		const CFRange   range = CFRangeMake(0, static_cast<CFIndex>(sample.text.length));
		const CTFontRef font  = CTFontCreateForStringWithLanguage(ui_font, (__bridge CFStringRef)sample.text, range, (__bridge CFStringRef)sample.language);
		add(font);
		CFRelease(font);
	}

	CFRelease(ui_font);

	return paths;
}

}
