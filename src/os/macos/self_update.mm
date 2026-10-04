#include "os/self_update.h"

namespace os {

auto can_self_update() -> bool
{
	return false;
}

auto install_update(const std::vector<u8>&, std::string_view, std::string* t_out_error) -> bool
{
	*t_out_error = "automatic updates aren't available on macOS yet";

	return false;
}

auto handed_off_to_repaired_copy() -> bool
{
	return false;
}

}
