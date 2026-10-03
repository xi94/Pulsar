#include "core/file.h"

#include <cstring>
#include <fstream>
#include <system_error>

#include "core/app_identity.h"
#include "os/files.h"

namespace {
[[nodiscard]] auto file_already_holds(const std::string& t_path, std::string_view t_contents) -> bool
{
	std::vector<u8> existing;
	if (!read_whole_file(t_path, &existing)) return false;

	return existing.size() == t_contents.size() && std::memcmp(existing.data(), t_contents.data(), existing.size()) == 0;
}
}

auto to_path(std::string_view t_utf8) -> std::filesystem::path
{
	return std::filesystem::path{std::u8string_view{reinterpret_cast<const char8_t*>(t_utf8.data()), t_utf8.size()}};
}

auto from_path(const std::filesystem::path& t_path) -> std::string
{
	const std::u8string utf8 = t_path.u8string();

	return std::string{reinterpret_cast<const char*>(utf8.data()), utf8.size()};
}

auto joined_path(std::string_view t_folder, std::string_view t_name) -> std::string
{
	return from_path((to_path(t_folder) / to_path(t_name)).make_preferred());
}

auto read_whole_file(const std::string& t_path, std::vector<u8>* t_out_bytes) -> bool
{
	std::ifstream file(to_path(t_path), std::ios::binary | std::ios::ate);
	if (!file) return false;

	const std::streamoff size = file.tellg();
	if (size <= 0) return false;

	t_out_bytes->resize(static_cast<usize>(size));
	file.seekg(0);

	return static_cast<bool>(file.read(reinterpret_cast<char*>(t_out_bytes->data()), size));
}

auto write_file_atomic(const std::string& t_path, std::string_view t_contents) -> bool
{
	// Saves happen on nearly every click, so rewriting unchanged content would rotate the last good .bak away.
	if (file_already_holds(t_path, t_contents)) return true;

	const std::string temporary_path = t_path + ".tmp";
	std::error_code   error;

	if (!os::write_file_durably(temporary_path, t_contents)) {
		std::filesystem::remove(to_path(temporary_path), error);
		return false;
	}

	os::replace_file(t_path, backup_path_for(t_path));

	if (!os::replace_file(temporary_path, t_path)) {
		std::filesystem::remove(to_path(temporary_path), error);
		return false;
	}

	return true;
}

auto backup_path_for(const std::string& t_path) -> std::string
{
	return t_path + ".bak";
}

auto app_data_subdirectory(const char* t_subfolder) -> std::string
{
	const std::string root = os::user_data_folder();
	if (root.empty()) return {};

	const std::string directory = joined_path(joined_path(root, K_APP_NAME), t_subfolder);

	std::error_code error;
	std::filesystem::create_directories(to_path(directory), error);

	return directory;
}
