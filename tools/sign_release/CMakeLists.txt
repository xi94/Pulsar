# sign_release: a standalone release-time CLI (see sign_release.cpp's own header comment) -
# deliberately its own executable target, not folded into the Pulsar target, since it has
# nothing to do with the shipped app and shouldn't need a full app rebuild to iterate on.
# Reuses the exact same vendored libsodium static lib the app itself links (see the root
# src/CMakeLists.txt's own comment on why that pair of prebuilt libs is there) so the key
# format this tool signs with can never drift from what core/updater.cpp verifies against.

add_executable(sign_release "sign_release.cpp")
target_compile_definitions(sign_release PRIVATE SODIUM_STATIC)
target_include_directories(sign_release PRIVATE "${CMAKE_SOURCE_DIR}/src" "${CMAKE_SOURCE_DIR}/src/third_party/libsodium/include")
target_compile_options(sign_release PRIVATE "/FIcore/force_include.h")

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(SIGN_RELEASE_SODIUM_LIB_DIR "${CMAKE_SOURCE_DIR}/src/third_party/libsodium/lib/x64-debug")
else()
  set(SIGN_RELEASE_SODIUM_LIB_DIR "${CMAKE_SOURCE_DIR}/src/third_party/libsodium/lib/x64-release")
endif()
target_link_libraries(sign_release PRIVATE "${SIGN_RELEASE_SODIUM_LIB_DIR}/libsodium.lib")

# version.lib - GetFileVersionInfo, which is how the tool reads the version out of the exe it is
# signing rather than being told it.
target_link_libraries(sign_release PRIVATE version)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET sign_release PROPERTY CXX_STANDARD 20)
endif()
