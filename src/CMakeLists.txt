# Everything here builds on every platform and never includes an OS header. Each platform adds its os/, render/ and
# login/ backends further down.
add_library (pulsar_portable OBJECT
  "main.cpp"
  "app.cpp"
  "setup_app.cpp"

  "core/animation.cpp"
  "core/crypto.cpp"
  "core/debug_log.cpp"
  "core/file.cpp"
  "core/library.cpp"
  "core/profiler.cpp"
  "core/storage.cpp"
  "core/str.cpp"
  "core/updater.cpp"

  "gfx/assets.cpp"
  "gfx/draw_list.cpp"
  "gfx/font.cpp"

  "login/login_attempt.cpp"

  "render/renderer.cpp"

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
  "ui/window_layout.cpp"

  "third_party/stb_impl.cpp"
)

target_include_directories(pulsar_portable PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/third_party" "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libsodium/include")
target_compile_definitions(pulsar_portable PUBLIC SODIUM_STATIC "$<$<NOT:$<CONFIG:Release>>:PULSAR_PROFILING>")
set_property(TARGET pulsar_portable PROPERTY CXX_STANDARD 26)

if (MSVC)
  target_compile_options(pulsar_portable PUBLIC "/FIcore/force_include.h")
else()
  target_compile_options(pulsar_portable PUBLIC "SHELL:-include \"${CMAKE_CURRENT_SOURCE_DIR}/core/force_include.h\"")
endif()

if (WIN32)
  # <Windows.h> expands to nothing once its include guard is defined, so portable code that reaches for Win32 fails to
  # compile here instead of first failing on a Mac.
  target_compile_definitions(pulsar_portable PRIVATE _WINDOWS_)

  add_executable (Pulsar
    "os/win32/app_icon.cpp"
    "os/win32/clipboard.cpp"
    "os/win32/crash_handler.cpp"
    "os/win32/files.cpp"
    "os/win32/fonts.cpp"
    "os/win32/http.cpp"
    "os/win32/input.cpp"
    "os/win32/installation.cpp"
    "os/win32/path_picker.cpp"
    "os/win32/process.cpp"
    "os/win32/self_update.cpp"
    "os/win32/system.cpp"
    "os/win32/tray.cpp"
    "os/win32/win32.cpp"
    "os/win32/window.cpp"

    "render/d3d11/renderer.cpp"

    "login/win32/riot_client.cpp"
    "login/win32/ui_automation.cpp"

    "app.manifest"
    "app.rc"
  )

  target_compile_definitions(Pulsar PRIVATE UNICODE _UNICODE NOMINMAX)
  set_property(TARGET Pulsar PROPERTY CXX_STANDARD 26)

  if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(SODIUM_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libsodium/lib/x64-debug")
  else()
    set(SODIUM_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libsodium/lib/x64-release")
  endif()

  find_package(Threads REQUIRED)
  target_link_libraries(Pulsar PRIVATE pulsar_portable "${SODIUM_LIB_DIR}/libsodium.lib" Threads::Threads)
  target_link_libraries(Pulsar PRIVATE shell32 dwmapi UIAutomationCore ole32 oleaut32 dbghelp comctl32 winhttp msimg32 d3d11 dxgi d3dcompiler version)

  # WIN32_EXECUTABLE implies a WinMain entry; mainCRTStartup keeps main() as the one entry point in both configs.
  if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set_property(TARGET Pulsar PROPERTY WIN32_EXECUTABLE TRUE)
    target_link_options(Pulsar PRIVATE "/ENTRY:mainCRTStartup")
  endif()
endif()
