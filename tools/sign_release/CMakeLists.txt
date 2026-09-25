add_executable(sign_release "sign_release.cpp")
target_compile_definitions(sign_release PRIVATE SODIUM_STATIC)
target_include_directories(sign_release PRIVATE "${CMAKE_SOURCE_DIR}/src" "${CMAKE_SOURCE_DIR}/src/third_party/libsodium/include")
target_compile_options(sign_release PRIVATE "/FIcore/force_include.h")

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(SIGN_RELEASE_SODIUM_LIB_DIR "${CMAKE_SOURCE_DIR}/src/third_party/libsodium/lib/x64-debug")
else()
  set(SIGN_RELEASE_SODIUM_LIB_DIR "${CMAKE_SOURCE_DIR}/src/third_party/libsodium/lib/x64-release")
endif()
target_link_libraries(sign_release PRIVATE "${SIGN_RELEASE_SODIUM_LIB_DIR}/libsodium.lib" version)

set_property(TARGET sign_release PROPERTY CXX_STANDARD 20)
