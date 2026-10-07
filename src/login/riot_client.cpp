#include "login/riot_client.h"

#include <nlohmann/json.hpp>

namespace {
// RiotClientInstalls.json names the client's executable under these keys, the one in use first.
constexpr const char* K_INSTALLS_KEYS[]{"rc_default", "rc_live", "rc_beta"};
}

auto RiotClient::paths_in_installs_file(std::string_view t_json) -> std::vector<std::string>
{
	const nlohmann::json installs = nlohmann::json::parse(t_json.begin(), t_json.end(), nullptr, false);
	if (!installs.is_object()) return {};

	std::vector<std::string> paths;

	for (const char* key : K_INSTALLS_KEYS) {
		const auto path = installs.find(key);
		if (path != installs.end() && path->is_string() && !path->get_ref<const std::string&>().empty()) {
			paths.push_back(path->get<std::string>());
		}
	}

	return paths;
}
