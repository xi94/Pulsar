#include "os/files.h"

#include <cerrno>
#include <cstdio>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "os/macos/macos.h"

namespace {
[[nodiscard]] auto write_all(int t_file, std::string_view t_contents) -> bool
{
	usize written = 0;

	while (written < t_contents.size()) {
		const ssize_t result = write(t_file, t_contents.data() + written, t_contents.size() - written);
		if (result < 0 && errno == EINTR) continue;
		if (result <= 0) return false;

		written += static_cast<usize>(result);
	}

	return true;
}
}

namespace os {

auto user_data_folder() -> std::string
{
	NSArray<NSString*>* folders = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);

	return macos::to_utf8(folders.firstObject);
}

auto write_file_durably(const std::string& t_path, std::string_view t_contents) -> bool
{
	const int file = open(t_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
	if (file < 0) return false;

	// fsync only reaches the drive's cache on macOS; F_FULLFSYNC asks the drive itself to commit the data.
	const bool ok = write_all(file, t_contents) && (fcntl(file, F_FULLFSYNC) == 0 || fsync(file) == 0);
	close(file);

	return ok;
}

auto replace_file(const std::string& t_from, const std::string& t_to) -> bool
{
	return std::rename(t_from.c_str(), t_to.c_str()) == 0;
}

auto open_log_file(const std::string& t_path) -> std::FILE*
{
	return std::fopen(t_path.c_str(), "wb");
}

auto map_file(const std::string& t_path) -> std::span<const u8>
{
	const int file = open(t_path.c_str(), O_RDONLY | O_CLOEXEC);
	if (file < 0) return {};

	struct stat status{};
	const bool  sized = fstat(file, &status) == 0 && status.st_size > 0;
	void*       view  = sized ? mmap(nullptr, static_cast<usize>(status.st_size), PROT_READ, MAP_PRIVATE, file, 0) : MAP_FAILED;
	close(file);
	if (view == MAP_FAILED) return {};

	return std::span{static_cast<const u8*>(view), static_cast<usize>(status.st_size)};
}

auto unmap_file(std::span<const u8> t_mapping) -> void
{
	if (!t_mapping.empty()) {
		munmap(const_cast<u8*>(t_mapping.data()), t_mapping.size());
	}
}

}
