set(PULSAR_GFX_BACKEND "d3d11" CACHE STRING "Graphics backend implementing gfx/renderer.h's IRenderer")
set_property(CACHE PULSAR_GFX_BACKEND PROPERTY STRINGS d3d11 gl)

if (PULSAR_GFX_BACKEND STREQUAL "d3d11")
  set(PULSAR_GFX_SOURCES "gfx/d3d11/renderer_d3d11.cpp")
else()
  message(FATAL_ERROR "PULSAR_GFX_BACKEND=${PULSAR_GFX_BACKEND} has no implementation")
endif()

add_executable (Pulsar
  "main.cpp"
  "app.cpp"

  "core/account.cpp"
  "core/animator.cpp"
  "core/atomic_file.cpp"
  "core/app_paths.cpp"
  "core/crash_handler.cpp"
  "core/crypto.cpp"
  "core/debug_log.cpp"
  "core/login_attempt.cpp"
  "core/master_key.cpp"
  "core/memory_arena.cpp"
  "core/profiler.cpp"
  "core/riot_client.cpp"
  "core/semver.cpp"
  "core/storage.cpp"
  "core/ui_automation.cpp"
  "core/update_manifest.cpp"
  "core/updater.cpp"

  "gfx/asset_manager.cpp"
  "gfx/font.cpp"
  "gfx/font_manager.cpp"
  "gfx/texture.cpp"
  ${PULSAR_GFX_SOURCES}

  "platform/app_icon.cpp"
  "platform/clipboard.cpp"
  "platform/overlay_guard.cpp"
  "platform/single_instance.cpp"
  "platform/tray.cpp"
  "platform/window.cpp"

  "ui/account_modal.cpp"
  "ui/app_menu.cpp"
  "ui/carousel.cpp"
  "ui/color_picker.cpp"
  "ui/confirm_latch.cpp"
  "ui/context_menu.cpp"
  "ui/controls.cpp"
  "ui/draggable.cpp"
  "ui/draw_list.cpp"
  "ui/game_select_popup.cpp"
  "ui/hoverable.cpp"
  "ui/layout.cpp"
  "ui/profiler_overlay.cpp"
  "ui/scrollable.cpp"
  "ui/settings_panel.cpp"
  "ui/text.cpp"
  "ui/text_input.cpp"
  "ui/title_bar.cpp"
  "ui/toast.cpp"
  "ui/tooltip.cpp"
  "ui/unlock_screen.cpp"
  "ui/update_overlay.cpp"
  "ui/widget.cpp"
  "ui/widget_stack.cpp"

  "third_party/stb_impl.cpp"

  # CMake's Ninja+MSVC generator threads a .manifest-suffixed source into the linker's
  # manifest-merge pass; a hand-authored resource script would not get that treatment.
  "app.manifest"
  "app.rc"
)

target_include_directories(Pulsar PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
target_compile_definitions(Pulsar PRIVATE UNICODE _UNICODE NOMINMAX)

# Force-included ahead of every translation unit, so the short integer aliases (u32, i8, ...)
# are in scope without a single file having to include anything for them.
target_compile_options(Pulsar PRIVATE "/FIcore/force_include.h")

# The frame profiler and its overlay compile to nothing without this, so a Release build carries
# neither the timing calls nor the widget that reports them.
target_compile_definitions(Pulsar PRIVATE "$<$<NOT:$<CONFIG:Release>>:PULSAR_PROFILING>")

# libsodium ships as a prebuilt static lib; only the x64 Debug/Release variants are vendored.
# SODIUM_STATIC stops sodium.h assuming the __declspec(dllimport) of the DLL build, keeping
# Pulsar.exe a single self-contained binary.
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(SODIUM_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libsodium/lib/x64-debug")
else()
  set(SODIUM_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libsodium/lib/x64-release")
endif()
target_include_directories(Pulsar PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libsodium/include")
target_compile_definitions(Pulsar PRIVATE SODIUM_STATIC)
target_link_libraries(Pulsar PRIVATE "${SODIUM_LIB_DIR}/libsodium.lib")

# dwmapi        - native drop shadow / rounded corners on the custom-drawn title bar
# UIAutomationCore, ole32, oleaut32 - core/ui_automation.cpp's IUIAutomation client
# dbghelp, comctl32                 - core/crash_handler.cpp's minidump and dialog
# winhttp                           - core/updater.cpp's manifest fetch and download
target_link_libraries(Pulsar PRIVATE shell32 dwmapi UIAutomationCore ole32 oleaut32 dbghelp comctl32 winhttp msimg32)

find_package(Threads REQUIRED)
target_link_libraries(Pulsar PRIVATE Threads::Threads)

if (PULSAR_GFX_BACKEND STREQUAL "d3d11")
  target_link_libraries(Pulsar PRIVATE d3d11 dxgi d3dcompiler)
endif()

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET Pulsar PROPERTY CXX_STANDARD 26)
endif()

# Release is GUI-subsystem so no console flashes up on launch; Debug stays console-subsystem
# so main.cpp's startup diagnostics have somewhere to go. /ENTRY overrides the WinMain entry
# WIN32_EXECUTABLE would otherwise imply, keeping main() the single entry point either way.
if (CMAKE_BUILD_TYPE STREQUAL "Release")
  set_property(TARGET Pulsar PROPERTY WIN32_EXECUTABLE TRUE)
  target_link_options(Pulsar PRIVATE "/ENTRY:mainCRTStartup")
endif()
