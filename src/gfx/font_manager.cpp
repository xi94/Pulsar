#include "gfx/font_manager.h"

#include "core/str.h"

#include <cstdio>
#include <cstring>
#include <print>

#include <Windows.h>

namespace {
// The settings panel's Font Size is a friendly display number rather than a literal pixel
// count: text at a given size read noticeably smaller than the same number in other apps.
// This is the single place that conversion happens, so the startup bake and the re-bake
// cannot drift apart. CFont::GetPixelHeight then holds the real baked size, so every layout
// consumer downstream already reasons in real pixels.
constexpr float kFontSizeDisplayScale = 1.5f;

// In display units, for the startup bake before settings are loaded. Secondary is its own
// value, not a ratio of body.
constexpr float kBodyPixelHeight = 16.0f;
constexpr float kSecondaryPixelHeight = 12.0f;

constexpr const char *kDefaultFontFileName = "segoeui.ttf";

// Resolves a font file name against the system Fonts directory. pName is a real C string,
// since it feeds CRT path functions rather than being rendered.
bool FontPathFor(const char *pName, char *pBuffer, usize bufferSize)
{
	char windowsDir[MAX_PATH];
	const UINT length = GetWindowsDirectoryA(windowsDir, MAX_PATH);
	if (length == 0 || length >= MAX_PATH) return false;

	return std::snprintf(pBuffer, bufferSize, "%s\\Fonts\\%s", windowsDir, pName) > 0;
}
} // namespace

bool CFontManager::Load(IRenderer *pRenderer, float dpiScale)
{
	char path[MAX_PATH + 64];
	if (!FontPathFor(kDefaultFontFileName, path, sizeof(path))) {
		std::println("Failed to resolve the system Fonts directory.");
		return false;
	}

	return m_body.LoadFromFile(pRenderer, path, kBodyPixelHeight * kFontSizeDisplayScale, dpiScale) &&
		   m_secondary.LoadFromFile(pRenderer, path, kSecondaryPixelHeight * kFontSizeDisplayScale, dpiScale);
}

bool CFontManager::ApplyBody(IRenderer *pRenderer, std::string_view fontFileName, float bodyPixelSize,
							 float secondaryPixelSize, float dpiScale)
{
	char name[128];
	if (fontFileName.empty() || fontFileName.size() >= sizeof(name)) return false;

	CopyTo(fontFileName, name, sizeof(name));

	char path[MAX_PATH + 64];
	if (!FontPathFor(name, path, sizeof(path))) return false;

	// Both bake into temporaries and only swap in once both succeed: a bad typed font name
	// must never leave the UI with one face updated and the other stale.
	CFont bodyCandidate;
	if (!bodyCandidate.LoadFromFile(pRenderer, path, bodyPixelSize * kFontSizeDisplayScale, dpiScale)) return false;

	CFont secondaryCandidate;
	if (!secondaryCandidate.LoadFromFile(pRenderer, path, secondaryPixelSize * kFontSizeDisplayScale, dpiScale)) {
		return false;
	}

	m_body = std::move(bodyCandidate);
	m_secondary = std::move(secondaryCandidate);

	return true;
}
