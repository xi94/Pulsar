#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "core/file.h"
#include "os/files.h"
#include "os/fonts.h"
#include "test.h"

// Things the operating system has to provide. Pulsar can't ship them, so a machine without them should fail here rather than at startup.

TEST_CASE("the OS ships the default interface font")
{
	CHECK(std::filesystem::is_regular_file(to_path(os::system_font_path(PULSAR_DEFAULT_FONT_FILE))));
}

TEST_CASE("at least one fallback font for other scripts is installed")
{
	const std::vector<std::string> paths = os::fallback_font_paths();

	REQUIRE(!paths.empty());
	CHECK(std::ranges::any_of(paths, [](const std::string& t_path) { return std::filesystem::is_regular_file(to_path(t_path)); }));
}

TEST_CASE("the user data folder exists")
{
	CHECK(std::filesystem::is_directory(to_path(os::user_data_folder())));
}
