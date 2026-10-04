#include "os/app_icon.h"

#include "os/macos/macos.h"

#include <algorithm>

namespace os {

auto app_icon_pixels(u32 t_size) -> std::vector<u8>
{
	NSImage* icon = [NSApplication sharedApplication].applicationIconImage;
	if (icon == nil) return {};

	std::vector<u8>       pixels(static_cast<usize>(t_size) * t_size * 4);
	const CGColorSpaceRef srgb    = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
	const CGContextRef    context = CGBitmapContextCreate(pixels.data(), t_size, t_size, 8, static_cast<usize>(t_size) * 4, srgb,
	                                                      static_cast<u32>(kCGImageAlphaPremultipliedLast) | static_cast<u32>(kCGBitmapByteOrder32Big));
	CGColorSpaceRelease(srgb);
	if (context == nullptr) return {};

	[NSGraphicsContext saveGraphicsState];
	NSGraphicsContext.currentContext = [NSGraphicsContext graphicsContextWithCGContext:context flipped:NO];
	[icon drawInRect:NSMakeRect(0.0, 0.0, t_size, t_size) fromRect:NSZeroRect operation:NSCompositingOperationCopy fraction:1.0];
	[NSGraphicsContext restoreGraphicsState];
	CGContextRelease(context);

	for (usize i = 0; i < pixels.size(); i += 4) {
		const u32 alpha = pixels[i + 3];
		if (alpha == 0 || alpha == 255) continue;

		for (usize channel = 0; channel < 3; channel += 1) {
			pixels[i + channel] = static_cast<u8>(std::min<u32>(255, (pixels[i + channel] * 255 + alpha / 2) / alpha));
		}
	}

	return pixels;
}

}
