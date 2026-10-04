#include "os/process.h"

#include <cerrno>
#include <cstdlib>

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

#include "os/macos/macos.h"

namespace {
constexpr std::string_view K_INSTANCE_LOCK_NAME = "instance.lock";

[[nodiscard]] auto split_arguments(std::string_view t_arguments) -> NSArray<NSString*>*
{
	NSMutableArray<NSString*>* arguments = [NSMutableArray array];

	for (usize start = 0; start < t_arguments.size();) {
		const usize end = std::min(t_arguments.find(' ', start), t_arguments.size());

		if (end > start) {
			[arguments addObject:os::macos::to_ns_string(t_arguments.substr(start, end - start))];
		}

		start = end + 1;
	}

	return arguments;
}
}

namespace os {

struct SingleInstanceGuard::Native {
	int lock = -1;
};

auto executable_path() -> std::string
{
	return macos::to_utf8(NSBundle.mainBundle.executablePath);
}

auto launch_process(std::string_view t_executable, std::string_view t_arguments) -> void
{
	NSURL*  executable = [NSURL fileURLWithPath:macos::to_ns_string(t_executable)];
	NSTask* task       = [[NSTask alloc] init];

	task.executableURL       = executable;
	task.arguments           = split_arguments(t_arguments);
	task.currentDirectoryURL = executable.URLByDeletingLastPathComponent;

	[task launchAndReturnError:nil];
}

auto open_path(std::string_view t_path) -> void
{
	[NSWorkspace.sharedWorkspace openURL:[NSURL fileURLWithPath:macos::to_ns_string(t_path)]];
}

auto open_url(std::string_view t_url) -> void
{
	if (NSURL* url = [NSURL URLWithString:macos::to_ns_string(t_url)]; url != nil) {
		[NSWorkspace.sharedWorkspace openURL:url];
	}
}

auto register_app_identity() -> void {}

auto block_injection() -> InjectionGuard
{
	return InjectionGuard::Unsupported;
}

auto injected_overlay() -> std::optional<std::string>
{
	const char* inserted = std::getenv("DYLD_INSERT_LIBRARIES");
	if (inserted == nullptr || *inserted == '\0') return std::nullopt;

	return std::string{inserted};
}

auto last_error() -> u32
{
	return static_cast<u32>(errno);
}

SingleInstanceGuard::SingleInstanceGuard()
	: m_native(std::make_unique<Native>())
{
	const std::string path = macos::lock_file_path(K_INSTANCE_LOCK_NAME);

	m_native->lock   = open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
	m_first_instance = m_native->lock >= 0 && flock(m_native->lock, LOCK_EX | LOCK_NB) == 0;
}

SingleInstanceGuard::~SingleInstanceGuard()
{
	release();
}

auto SingleInstanceGuard::release() -> void
{
	if (m_native->lock < 0) return;

	close(m_native->lock);
	m_native->lock = -1;
}

}
