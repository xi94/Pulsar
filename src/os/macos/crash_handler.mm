#include "os/crash_handler.h"

#include <sys/resource.h>

namespace os {

auto install_crash_handler() -> void
{
	// macOS already writes a crash report without memory contents; a core dump would hold the vault key and every decrypted password.
	const rlimit no_core_dumps{.rlim_cur = 0, .rlim_max = 0};
	setrlimit(RLIMIT_CORE, &no_core_dumps);
}

auto write_diagnostic_dump(const char*) -> std::string
{
	return {};
}

}
