#pragma once

#define PULSAR_VERSION_MAJOR 0
#define PULSAR_VERSION_MINOR 6
#define PULSAR_VERSION_PATCH 1

#define PULSAR_STRINGIFY_IMPL(x) #x
#define PULSAR_STRINGIFY(x) PULSAR_STRINGIFY_IMPL(x)

#define PULSAR_VERSION_STRING                                                                                          \
	PULSAR_STRINGIFY(PULSAR_VERSION_MAJOR)                                                                             \
	"." PULSAR_STRINGIFY(PULSAR_VERSION_MINOR) "." PULSAR_STRINGIFY(PULSAR_VERSION_PATCH)

#define PULSAR_APP_NAME "Pulsar"
#define PULSAR_EXE_NAME "Pulsar.exe"
#define PULSAR_RELEASE_REPO "https://github.com/xi94/Pulsar"
#define PULSAR_APP_ICON_RESOURCE 101

#ifndef RC_INVOKED

constexpr const char *app_version = PULSAR_VERSION_STRING;
constexpr const char *app_name = PULSAR_APP_NAME;
constexpr const wchar_t *app_name_wide = L"" PULSAR_APP_NAME;

constexpr const char *app_data_folder_name = PULSAR_APP_NAME;
constexpr const char *legacy_data_folder_names[]{"Rift", "f4-rockstar"};

constexpr const wchar_t *main_window_class_name = L"PulsarDesktopAppWindow";
constexpr const wchar_t *setup_window_class_name = L"PulsarDesktopAppSetup";
constexpr const wchar_t *quit_instance_message_name = L"PulsarDesktopAppQuitForSetup";
constexpr const wchar_t *tray_window_class_name = L"PulsarDesktopAppTray";
constexpr const wchar_t *single_instance_mutex_name = L"Local\\Pulsar.DesktopApp.SingleInstance";
constexpr const wchar_t *activate_instance_message_name = L"PulsarDesktopAppActivateExistingInstance";
constexpr const wchar_t *app_user_model_id = L"Pulsar.DesktopApp.AccountManager";

constexpr const wchar_t *update_manifest_url = L"" PULSAR_RELEASE_REPO "/releases/latest/download/update.json";
constexpr const char *release_download_url_format = PULSAR_RELEASE_REPO "/releases/download/v%s/" PULSAR_EXE_NAME;

constexpr const wchar_t *debug_log_environment_variable = L"PULSAR_DEBUG_LOG";

#ifdef NDEBUG
constexpr bool is_debug_build = false;
#else
constexpr bool is_debug_build = true;
#endif

#endif
