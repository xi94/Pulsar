#pragma once

// Every name this app is known by, in one place. Anything user-visible, anything Windows keys
// off, and anything that ends up in a path lives here rather than as a literal at the call site,
// so a rename is one edit rather than a search.
//
// The suffixed names exist because several of them must not collide with anything else on the
// machine, and a bare product name is the most likely thing to collide.

#define PULSAR_VERSION_MAJOR 1
#define PULSAR_VERSION_MINOR 3
#define PULSAR_VERSION_PATCH 2

#define PULSAR_STRINGIFY_IMPL(x) #x
#define PULSAR_STRINGIFY(x) PULSAR_STRINGIFY_IMPL(x)

#define PULSAR_VERSION_STRING                                                                                          \
	PULSAR_STRINGIFY(PULSAR_VERSION_MAJOR)                                                                             \
	"." PULSAR_STRINGIFY(PULSAR_VERSION_MINOR) "." PULSAR_STRINGIFY(PULSAR_VERSION_PATCH)

#define PULSAR_APP_NAME "Pulsar"
#define PULSAR_EXE_NAME "Pulsar.exe"

/// The GitHub repository releases are published to. Every URL below is built from it, so moving
/// the project is one edit here rather than a search for half-remembered literals.
#define PULSAR_RELEASE_REPO "https://github.com/xi94/Pulsar"

// The macros above are also read by app.rc, which the resource compiler parses without a C++
// front end - everything below here must stay out of that build.
#ifndef RC_INVOKED

constexpr const char *kAppVersion = PULSAR_VERSION_STRING;

/// The product name, for anything a person reads.
constexpr const char *kAppName = PULSAR_APP_NAME;
constexpr const wchar_t *kAppNameW = L"" PULSAR_APP_NAME;

/// The folder under %LOCALAPPDATA% holding the vault, settings, logs and crash dumps.
constexpr const char *kAppDataFolderName = PULSAR_APP_NAME;

/// Folders this app has previously stored data under, newest first. GetStorageDirectory renames
/// the first one it finds, so an existing install's data follows the app through a rename.
constexpr const char *kLegacyDataFolderNames[]{"Rift", "f4-rockstar"};

/// Window class names are matched by anything scanning the desktop, Discord's game detection
/// included, so neither is the bare product name.
constexpr const wchar_t *kMainWindowClassName = L"PulsarDesktopAppWindow";
constexpr const wchar_t *kTrayWindowClassName = L"PulsarDesktopAppTray";

/// A machine-wide name, so it carries a suffix that nothing else would pick.
constexpr const wchar_t *kSingleInstanceMutexName = L"Local\\Pulsar.DesktopApp.SingleInstance";

/// Registered with RegisterWindowMessageW, which is a global atom table.
constexpr const wchar_t *kActivateInstanceMessageName = L"PulsarDesktopAppActivateExistingInstance";

/// Where a running client looks for the manifest. "latest" always resolves to whichever release
/// GitHub currently marks as such, so nothing here has to know the newest version number.
constexpr const wchar_t *kUpdateManifestUrl = L"" PULSAR_RELEASE_REPO "/releases/latest/download/update.json";

/// Where one release's binary lives, with the version substituted in. The tag is v<version> and
/// the asset keeps the executable's own name; sign_release builds a download URL from this, so the
/// convention lives here rather than in a hand-typed argument.
constexpr const char *kReleaseDownloadUrlFormat = PULSAR_RELEASE_REPO "/releases/download/v%s/" PULSAR_EXE_NAME;

/// Overridden by an environment variable of this name in any build, not just Debug.
constexpr const wchar_t *kDebugLogEnvironmentVariable = L"PULSAR_DEBUG_LOG";

#ifdef NDEBUG
constexpr bool kIsDebugBuild = false;
#else
constexpr bool kIsDebugBuild = true;
#endif

#endif // RC_INVOKED
