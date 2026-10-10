#pragma once

#define PULSAR_VERSION_MAJOR 0
#define PULSAR_VERSION_MINOR 8
#define PULSAR_VERSION_PATCH 2

#define PULSAR_STRINGIFY_IMPL(x) #x
#define PULSAR_STRINGIFY(x)      PULSAR_STRINGIFY_IMPL(x)

#define PULSAR_VERSION_STRING                                                                                                                                  \
	PULSAR_STRINGIFY(PULSAR_VERSION_MAJOR)                                                                                                                     \
	"." PULSAR_STRINGIFY(PULSAR_VERSION_MINOR) "." PULSAR_STRINGIFY(PULSAR_VERSION_PATCH)

#define PULSAR_APP_NAME          "Pulsar"
#define PULSAR_BUNDLE_IDENTIFIER "io.github.xi94.Pulsar"
#define PULSAR_EXE_NAME          "Pulsar.exe"
#define PULSAR_RELEASE_REPO      "https://github.com/xi94/Pulsar"
#define PULSAR_APP_ICON_RESOURCE 101

#ifndef RC_INVOKED

constexpr const char* K_APP_VERSION = PULSAR_VERSION_STRING;
constexpr const char* K_APP_NAME    = PULSAR_APP_NAME;

constexpr const char* K_APP_DATA_FOLDER_NAME = PULSAR_APP_NAME;
constexpr const char* K_LEGACY_DATA_FOLDER_NAMES[]{"Rift", "f4-rockstar"};

constexpr const char* K_UPDATE_MANIFEST_URL         = PULSAR_RELEASE_REPO "/releases/latest/download/update.json";
constexpr const char* K_RELEASE_DOWNLOAD_URL_FORMAT = PULSAR_RELEASE_REPO "/releases/download/v%s/" PULSAR_EXE_NAME;

constexpr const char* K_DEBUG_LOG_ENVIRONMENT_VARIABLE = "PULSAR_DEBUG_LOG";

#ifdef NDEBUG
constexpr bool K_IS_DEBUG_BUILD = false;
#else
constexpr bool K_IS_DEBUG_BUILD = true;
#endif

#endif
