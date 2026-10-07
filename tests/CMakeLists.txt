# Tests for the code where a silent break would hurt, and for what Pulsar depends on but doesn't control (Riot's install file,
# login messages and folders, OS fonts, GitHub's URLs): update signatures, the vault's encryption, version checks, text handling
# and the title bar layout. They link the same code as the app, so `ctest` runs them against the real thing.
#
#   cmake --build build --target pulsar_tests
#   ctest --test-dir build --output-on-failure        (add -C Release with Visual Studio)
#
# A new file needs only to be listed here; TEST_CASE registers itself. Pass part of a test name to the executable to run just those.
add_executable (pulsar_tests
  "main.cpp"
  "os_test.cpp"
  "riot_test.cpp"
  "text_input_test.cpp"
  "text_test.cpp"
  "title_bar_test.cpp"
  "update_test.cpp"
  "vault_test.cpp"
)

target_link_libraries(pulsar_tests PRIVATE pulsar_portable pulsar_platform)
set_property(TARGET pulsar_tests PROPERTY CXX_STANDARD 26)

if (WIN32)
  # Tests are portable code too, and the test strings are UTF-8 that MSVC would otherwise read in the system code page.
  target_compile_definitions(pulsar_tests PRIVATE _WINDOWS_)
  target_compile_options(pulsar_tests PRIVATE "/utf-8")

  # The Windows backends import common controls v6, which only loads for an executable whose manifest asks for it.
  target_sources(pulsar_tests PRIVATE "${PROJECT_SOURCE_DIR}/src/app.manifest")
endif()

add_test (NAME pulsar_tests COMMAND pulsar_tests)
