#include "login/riot_client.h"

#include <algorithm>
#include <filesystem>
#include <ranges>
#include <span>
#include <system_error>

#include <nlohmann/json.hpp>

#include "core/file.h"
#include "os/files.h"

namespace {
// RiotClientInstalls.json names the client's executable under these keys, the one in use first.
constexpr const char* K_INSTALLS_KEYS[]{"rc_default", "rc_live", "rc_beta"};

constexpr const char*      K_PRIVATE_SETTINGS_FOLDER = "Riot Games/Riot Client/Data";
constexpr const char*      K_PRIVATE_SETTINGS_NAME   = "RiotGamesPrivateSettings.yaml";
constexpr std::string_view K_STORED_SIGN_IN_PARENT   = "psl";
constexpr std::string_view K_STORED_SIGN_IN_KEY      = "authorization";

[[nodiscard]] auto lines_of(std::string_view t_text) -> std::vector<std::string_view>
{
	std::vector<std::string_view> lines;

	for (usize start = 0; start < t_text.size();) {
		const usize end = std::min(t_text.find('\n', start), t_text.size() - 1) + 1;
		lines.push_back(t_text.substr(start, end - start));
		start = end;
	}

	return lines;
}

[[nodiscard]] auto indentation_of(std::string_view t_line) -> usize
{
	return std::min(t_line.find_first_not_of(' '), t_line.size());
}

[[nodiscard]] auto is_blank(std::string_view t_line) -> bool
{
	return t_line.find_first_not_of(" \t\r\n") == std::string_view::npos;
}

[[nodiscard]] auto starts_key(std::string_view t_line, std::string_view t_key) -> bool
{
	const std::string_view content = t_line.substr(indentation_of(t_line));

	return content.starts_with(t_key) && content.substr(t_key.size()).starts_with(':');
}
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

// "Stay signed in" keeps the client's tokens under psl.authorization, and a freshly started client signs itself back in with them. Everything else in the file, Riot's trusted-device cookie included, is kept as it was.
auto RiotClient::without_stored_sign_in(std::string_view t_private_settings) -> std::string
{
	const std::vector<std::string_view> lines  = lines_of(t_private_settings);
	const auto                          parent = std::ranges::find_if(lines, [](std::string_view t_line) { return indentation_of(t_line) == 0 && starts_key(t_line, K_STORED_SIGN_IN_PARENT); });
	if (parent == lines.end()) return std::string{t_private_settings};

	const auto parent_index = static_cast<usize>(parent - lines.begin());
	usize      block_end    = parent_index + 1;

	while (block_end < lines.size() && (is_blank(lines[block_end]) || indentation_of(lines[block_end]) > 0)) {
		block_end += 1;
	}

	const auto  children     = std::span{lines}.subspan(parent_index + 1, block_end - parent_index - 1);
	const auto  first_child  = std::ranges::find_if(children, [](std::string_view t_line) { return !is_blank(t_line); });
	const usize child_indent = first_child != children.end() ? indentation_of(*first_child) : 0;

	std::vector<bool> removed(lines.size(), false);
	bool              removing      = false;
	bool              children_kept = false;

	for (const usize index : std::views::iota(parent_index + 1, block_end)) {
		if (!is_blank(lines[index]) && indentation_of(lines[index]) <= child_indent) {
			removing = starts_key(lines[index], K_STORED_SIGN_IN_KEY);
		}

		removed[index] = removing;
		children_kept  = children_kept || (!removing && !is_blank(lines[index]));
	}

	if (!children_kept) {
		std::fill(removed.begin() + static_cast<std::ptrdiff_t>(parent_index), removed.begin() + static_cast<std::ptrdiff_t>(block_end), true);
	}

	std::string kept;
	kept.reserve(t_private_settings.size());

	for (const usize index : std::views::iota(usize{0}, lines.size())) {
		if (!removed[index]) {
			kept += lines[index];
		}
	}

	return kept;
}

// Only safe while the client is closed: a running client would write its tokens straight back.
auto RiotClient::forget_stored_sign_in() -> bool
{
	const std::string path = joined_path(joined_path(os::user_data_folder(), K_PRIVATE_SETTINGS_FOLDER), K_PRIVATE_SETTINGS_NAME);

	std::vector<u8> bytes;
	if (!read_whole_file(path, &bytes)) return false;

	const std::string_view stored{reinterpret_cast<const char*>(bytes.data()), bytes.size()};
	const std::string      forgotten = without_stored_sign_in(stored);
	if (forgotten.size() == stored.size()) return false;

	const std::string temporary = path + ".pulsar";
	if (os::write_file_durably(temporary, forgotten) && os::replace_file(temporary, path)) return true;

	std::error_code ignored;
	std::filesystem::remove(to_path(temporary), ignored);

	return false;
}
