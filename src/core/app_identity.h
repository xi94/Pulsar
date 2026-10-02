#pragma once

#define PULSAR_VERSION_MAJOR 0
#define PULSAR_VERSION_MINOR 7
#define PULSAR_VERSION_PATCH 0

#define PULSAR_STRINGIFY_IMPL(x) #x
#define PULSAR_STRINGIFY(x)      PULSAR_STRINGIFY_IMPL(x)

#define PULSAR_VERSION_STRING                                                                                                                                  \
	PULSAR_STRINGIFY(PULSAR_VERSION_MAJOR)                                                                                                                     \
	"." PULSAR_STRINGIFY(PULSAR_VERSION_MINOR) "." PULSAR_STRINGIFY(PULSAR_VERSION_PATCH)

#define PULSAR_APP_NAME          "Pulsar"
#define PULSAR_EXE_NAME          "Pulsar.exe"
#define PULSAR_RELEASE_REPO      "https://github.com/xi94/Pulsar"
#define PULSAR_APP_ICON_RESOURCE 101

#ifndef RC_INVOKED

constexpr const char*    K_APP_VERSION   = PULSAR_VERSION_STRING;
constexpr const char*    K_APP_NAME      = PULSAR_APP_NAME;
constexpr const wchar_t* K_APP_NAME_WIDE = L"" PULSAR_APP_NAME;

constexpr const char* K_APP_DATA_FOLDER_NAME = PULSAR_APP_NAME;
constexpr const char* K_LEGACY_DATA_FOLDER_NAMES[]{"Rift", "f4-rockstar"};

constexpr const wchar_t* K_MAIN_WINDOW_CLASS_NAME         = L"PulsarDesktopAppWindow";
constexpr const wchar_t* K_SETUP_WINDOW_CLASS_NAME        = L"PulsarDesktopAppSetup";
constexpr const wchar_t* K_QUIT_INSTANCE_MESSAGE_NAME     = L"PulsarDesktopAppQuitForSetup";
constexpr const wchar_t* K_TRAY_WINDOW_CLASS_NAME         = L"PulsarDesktopAppTray";
constexpr const wchar_t* K_SINGLE_INSTANCE_MUTEX_NAME     = L"Local\\Pulsar.DesktopApp.SingleInstance";
constexpr const wchar_t* K_ACTIVATE_INSTANCE_MESSAGE_NAME = L"PulsarDesktopAppActivateExistingInstance";
constexpr const wchar_t* K_APP_USER_MODEL_ID              = L"Pulsar.DesktopApp.AccountManager";

constexpr const wchar_t* K_UPDATE_MANIFEST_URL         = L"" PULSAR_RELEASE_REPO "/releases/latest/download/update.json";
constexpr const char*    K_RELEASE_DOWNLOAD_URL_FORMAT = PULSAR_RELEASE_REPO "/releases/download/v%s/" PULSAR_EXE_NAME;

constexpr const wchar_t* K_DEBUG_LOG_ENVIRONMENT_VARIABLE = L"PULSAR_DEBUG_LOG";

#ifdef NDEBUG
constexpr bool K_IS_DEBUG_BUILD = false;
#else
constexpr bool K_IS_DEBUG_BUILD = true;
#endif

#endif
