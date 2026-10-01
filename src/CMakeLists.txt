add_executable (Pulsar
  "main.cpp"
  "app.cpp"
  "setup_app.cpp"

  "core/animation.cpp"
  "core/crash_handler.cpp"
  "core/crypto.cpp"
  "core/debug_log.cpp"
  "core/file.cpp"
  "core/library.cpp"
  "core/profiler.cpp"
  "core/storage.cpp"
  "core/str.cpp"
  "core/updater.cpp"

  "login/login_attempt.cpp"
  "login/riot_client.cpp"
  "login/ui_automation.cpp"

  "gfx/assets.cpp"
  "gfx/draw_list.cpp"
  "gfx/font.cpp"
  "gfx/renderer.cpp"

  "platform/app_icon.cpp"
  "platform/clipboard.cpp"
  "platform/installation.cpp"
  "platform/process.cpp"
  "platform/tray.cpp"
  "platform/window.cpp"

  "ui/account_modal.cpp"
  "ui/account_search.cpp"
  "ui/app_menu.cpp"
  "ui/carousel.cpp"
  "ui/color_picker.cpp"
  "ui/context_menu.cpp"
  "ui/controls.cpp"
  "ui/list_popup.cpp"
  "ui/profiler_overlay.cpp"
  "ui/scrollable.cpp"
  "ui/settings_panel.cpp"
  "ui/setup_screen.cpp"
  "ui/snowfall.cpp"
  "ui/text.cpp"
  "ui/text_input.cpp"
  "ui/theme.cpp"
  "ui/title_bar.cpp"
  "ui/toasts.cpp"
  "ui/tooltip.cpp"
  "ui/truncation_hint.cpp"
  "ui/unlock_screen.cpp"
  "ui/update_overlay.cpp"
  "ui/widget.cpp"

  "third_party/stb_impl.cpp"

  "app.manifest"
  "app.rc"
)

target_include_directories(Pulsar PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
target_compile_definitions(Pulsar PRIVATE UNICODE _UNICODE NOMINMAX)
target_compile_options(Pulsar PRIVATE "/FIcore/force_include.h")
target_compile_definitions(Pulsar PRIVATE "$<$<NOT:$<CONFIG:Release>>:PULSAR_PROFILING>")

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(SODIUM_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libsodium/lib/x64-debug")
else()
  set(SODIUM_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libsodium/lib/x64-release")
endif()
target_include_directories(Pulsar PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libsodium/include")
target_compile_definitions(Pulsar PRIVATE SODIUM_STATIC)
target_link_libraries(Pulsar PRIVATE "${SODIUM_LIB_DIR}/libsodium.lib")

target_link_libraries(Pulsar PRIVATE shell32 dwmapi UIAutomationCore ole32 oleaut32 dbghelp comctl32 winhttp msimg32 d3d11 dxgi d3dcompiler version)

find_package(Threads REQUIRED)
target_link_libraries(Pulsar PRIVATE Threads::Threads)

set_property(TARGET Pulsar PROPERTY CXX_STANDARD 26)

# WIN32_EXECUTABLE implies a WinMain entry; mainCRTStartup keeps main() as the one entry point in both configs.
if (CMAKE_BUILD_TYPE STREQUAL "Release")
  set_property(TARGET Pulsar PROPERTY WIN32_EXECUTABLE TRUE)
  target_link_options(Pulsar PRIVATE "/ENTRY:mainCRTStartup")
endif()
