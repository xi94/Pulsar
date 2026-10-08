# Everything here builds on every platform and never includes an OS header. Each platform adds its os/, render/ and
# login/ backends as pulsar_platform further down. The app is main.cpp on top of both, and so are the tests in tests/.
add_library (pulsar_portable OBJECT
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
  "login/riot_client.cpp"

  "render/renderer.cpp"

  "ui/account_modal.cpp"
  "ui/account_search.cpp"
  "ui/app_menu.cpp"
  "ui/carousel.cpp"
  "ui/color_picker.cpp"
  "ui/context_menu.cpp"
  "ui/controls.cpp"
  "ui/library_view.cpp"
  "ui/list_popup.cpp"
  "ui/login_session.cpp"
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
  target_compile_definitions(pulsar_portable PUBLIC PULSAR_DEFAULT_FONT_FILE="segoeui.ttf" PULSAR_SHORTCUT_KEY="Ctrl")

  add_library (pulsar_platform OBJECT
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
    "render/opengl/renderer.cpp"
    "render/opengl/win32/context.cpp"

    "login/win32/riot_client.cpp"
    "login/win32/ui_automation.cpp"
  )

  target_compile_definitions(pulsar_platform PRIVATE UNICODE _UNICODE NOMINMAX)
  set_property(TARGET pulsar_platform PROPERTY CXX_STANDARD 26)

  if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(SODIUM_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libsodium/lib/x64-debug")
  else()
    set(SODIUM_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libsodium/lib/x64-release")
  endif()

  find_package(Threads REQUIRED)
  target_link_libraries(pulsar_platform PUBLIC pulsar_portable "${SODIUM_LIB_DIR}/libsodium.lib" Threads::Threads)
  target_link_libraries(pulsar_platform PUBLIC shell32 dwmapi UIAutomationCore ole32 oleaut32 dbghelp comctl32 winhttp msimg32 d3d11 dxgi d3dcompiler opengl32
                                               version)

  add_executable (Pulsar
    "main.cpp"
    "app.manifest"
    "app.rc"
  )

  target_compile_definitions(Pulsar PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:_WINDOWS_>")
  target_link_libraries(Pulsar PRIVATE pulsar_portable pulsar_platform)
  set_property(TARGET Pulsar PROPERTY CXX_STANDARD 26)

  # WIN32_EXECUTABLE implies a WinMain entry; mainCRTStartup keeps main() as the one entry point in both configs.
  if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set_property(TARGET Pulsar PROPERTY WIN32_EXECUTABLE TRUE)
    target_link_options(Pulsar PRIVATE "/ENTRY:mainCRTStartup")
  endif()
elseif (APPLE)
  enable_language(OBJCXX)

  target_compile_definitions(pulsar_portable PUBLIC PULSAR_DEFAULT_FONT_FILE="SFNS.ttf" PULSAR_SHORTCUT_KEY="⌘")

  file(STRINGS "core/app_identity.h" IDENTITY_LINES REGEX "^#define PULSAR_(VERSION_MAJOR|VERSION_MINOR|VERSION_PATCH|BUNDLE_IDENTIFIER) ")
  foreach (LINE IN LISTS IDENTITY_LINES)
    string(REGEX MATCH "^#define (PULSAR_[A-Z_]+) +\"?([^\" ]+)\"?" MATCHED "${LINE}")
    set(${CMAKE_MATCH_1} "${CMAKE_MATCH_2}")
  endforeach()
  set(APP_VERSION "${PULSAR_VERSION_MAJOR}.${PULSAR_VERSION_MINOR}.${PULSAR_VERSION_PATCH}")

  set(APP_ICON "${PROJECT_SOURCE_DIR}/resources/Pulsar.icns")
  set(MENU_BAR_ICON "${PROJECT_SOURCE_DIR}/resources/MenuBarIcon.png")
  set_source_files_properties("${APP_ICON}" "${MENU_BAR_ICON}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

  add_library (pulsar_platform OBJECT
    "os/macos/clipboard.mm"
    "os/macos/crash_handler.mm"
    "os/macos/files.mm"
    "os/macos/fonts.mm"
    "os/macos/http.mm"
    "os/macos/input.mm"
    "os/macos/installation.mm"
    "os/macos/macos.mm"
    "os/macos/path_picker.mm"
    "os/macos/process.mm"
    "os/macos/self_update.mm"
    "os/macos/system.mm"
    "os/macos/tray.mm"
    "os/macos/window.mm"

    "render/metal/renderer.mm"
    "render/opengl/renderer.cpp"
    "render/opengl/macos/context.mm"

    "login/macos/riot_client.mm"
  )

  set_target_properties(pulsar_platform PROPERTIES CXX_STANDARD 26 OBJCXX_STANDARD 26)
  target_compile_definitions(pulsar_platform PRIVATE GL_SILENCE_DEPRECATION)
  target_compile_options(pulsar_platform PRIVATE "$<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>")
  target_link_libraries(pulsar_platform PUBLIC pulsar_portable "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libsodium/lib/macos-universal/libsodium.a")
  target_link_libraries(pulsar_platform PUBLIC "-framework AppKit" "-framework ApplicationServices" "-framework CoreText" "-framework Metal"
                                              "-framework QuartzCore")

  add_executable (Pulsar MACOSX_BUNDLE
    "main.cpp"
    "${APP_ICON}"
    "${MENU_BAR_ICON}"
  )

  set_target_properties(Pulsar PROPERTIES
    CXX_STANDARD 26
    OBJCXX_STANDARD 26
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
    MACOSX_BUNDLE_BUNDLE_NAME "Pulsar"
    MACOSX_BUNDLE_GUI_IDENTIFIER "${PULSAR_BUNDLE_IDENTIFIER}"
    MACOSX_BUNDLE_ICON_FILE "Pulsar.icns"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${APP_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${APP_VERSION}"
  )

  target_link_libraries(Pulsar PRIVATE pulsar_portable pulsar_platform)

  # The linker signs only the arm64 slice; a downloaded app needs the whole bundle sealed, and ad-hoc signing does that until a Developer ID is used.
  add_custom_command(TARGET Pulsar POST_BUILD COMMAND codesign --force --sign - "$<TARGET_BUNDLE_DIR:Pulsar>" VERBATIM)

  # `cmake --build build --target package` writes Pulsar-<version>.dmg: the app next to an Applications shortcut to drag it onto.
  install(TARGETS Pulsar BUNDLE DESTINATION .)
  set(CPACK_GENERATOR "DragNDrop")
  set(CPACK_PACKAGE_NAME "Pulsar")
  set(CPACK_PACKAGE_VERSION "${APP_VERSION}")
  set(CPACK_PACKAGE_FILE_NAME "Pulsar-${APP_VERSION}")
  set(CPACK_DMG_VOLUME_NAME "Pulsar")
  include(CPack)
endif()
