#include <atomic>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "core/file.h"
#include "login/login_attempt.h"
#include "login/riot_client.h"
#include "test.h"

// Where the Riot Client installs, the file that records it and the text its login screen shows all belong to Riot. These tests pin down how
// Pulsar reads each of them, so a mistake on our side, or a format Riot changed, shows up here instead of as a login that quietly stops working.

namespace {
#ifdef _WIN32
constexpr std::string_view K_CLIENT_IN_INSTALL = "Riot Client/RiotClientServices.exe";
constexpr std::string_view K_CLIENT_TO_PICK    = "Riot Client/RiotClientServices.exe";
constexpr std::string_view K_GAME_IN_INSTALL   = "League of Legends/LeagueClient.exe";
constexpr std::string_view K_GAME_TO_PICK      = "League of Legends/LeagueClient.exe";
#else
constexpr std::string_view K_CLIENT_IN_INSTALL = "Riot Client.app/Contents/MacOS/RiotClientServices";
constexpr std::string_view K_CLIENT_TO_PICK    = "Riot Client.app";
constexpr std::string_view K_GAME_IN_INSTALL   = "League of Legends.app/Contents/MacOS/LeagueClient";
constexpr std::string_view K_GAME_TO_PICK      = "League of Legends.app";
#endif

auto create_file(const std::filesystem::path& t_path) -> void
{
	std::filesystem::create_directories(t_path.parent_path());
	std::ofstream{t_path} << "";
}

// A throwaway folder with a fake Riot Client install, laid out the way the real one is on this OS.
struct FakeInstall {
	std::filesystem::path root;
	std::filesystem::path client;

	FakeInstall()
		: root(std::filesystem::temp_directory_path() / ("pulsar-test-" + std::to_string(std::random_device{}())))
		, client(root / "Riot Games" / to_path(K_CLIENT_IN_INSTALL))
	{
		create_file(client);
		create_file(root / "Riot Games" / to_path(K_GAME_IN_INSTALL));
		create_file(root / "Desktop" / "notes.txt");
	}

	~FakeInstall()
	{
		std::error_code ignored;
		std::filesystem::remove_all(root, ignored);
	}

	FakeInstall(const FakeInstall&)                    = delete;
	auto operator=(const FakeInstall&) -> FakeInstall& = delete;

	[[nodiscard]] auto path_of(std::string_view t_inside_riot_games) const -> std::string
	{
		return from_path(root / "Riot Games" / to_path(t_inside_riot_games));
	}

	[[nodiscard]] auto is_client(std::string_view t_path) const -> bool
	{
		std::error_code error;

		return !t_path.empty() && std::filesystem::equivalent(to_path(t_path), client, error);
	}
};
}

TEST_CASE("Riot's installs file gives the client's executable, the one in use first")
{
	constexpr std::string_view JSON = R"({
		"associated_client": {"C:/Riot Games/League of Legends/": "C:/Riot Games/Riot Client/RiotClientServices.exe"},
		"patchlines": {"KeystoneFoundationLiveWin": "live"},
		"rc_default": "C:/Riot Games/Riot Client/RiotClientServices.exe",
		"rc_live": "D:/Games/Riot Client/RiotClientServices.exe"
	})";

	const std::vector<std::string> paths = RiotClient::paths_in_installs_file(JSON);

	REQUIRE(paths.size() == 2);
	CHECK(paths[0] == "C:/Riot Games/Riot Client/RiotClientServices.exe");
	CHECK(paths[1] == "D:/Games/Riot Client/RiotClientServices.exe");
}

TEST_CASE("a broken or unexpected installs file gives nothing")
{
	CHECK(RiotClient::paths_in_installs_file("").empty());
	CHECK(RiotClient::paths_in_installs_file("{\"rc_default\": \"C:/Riot").empty());
	CHECK(RiotClient::paths_in_installs_file(R"(["rc_default", "C:/Riot Games"])").empty());
	CHECK(RiotClient::paths_in_installs_file(R"({"rc_default": 42, "rc_live": null, "rc_beta": ""})").empty());
}

TEST_CASE("picking the Riot Client finds its executable")
{
	const FakeInstall install;

	CHECK(install.is_client(RiotClient::executable_near(install.path_of(K_CLIENT_TO_PICK))));
}

TEST_CASE("picking a game Riot installed finds the client beside it")
{
	const FakeInstall install;

	CHECK(install.is_client(RiotClient::executable_near(install.path_of(K_GAME_TO_PICK))));
}

TEST_CASE("picking something unrelated finds no client")
{
	const FakeInstall install;

	CHECK(RiotClient::executable_near(from_path(install.root / "Desktop" / "notes.txt")).empty());
}

TEST_CASE("a remembered client path is used while the file is there")
{
	const FakeInstall       install;
	const std::atomic<bool> cancel{false};
	RiotClient              client{&cancel};

	REQUIRE(client.resolve_executable_path(from_path(install.client)));
	CHECK(install.is_client(client.executable_path()));
}

TEST_CASE("a remembered client path that's gone isn't used")
{
	const std::string       gone = from_path(std::filesystem::temp_directory_path() / "pulsar-test-gone" / to_path(K_CLIENT_IN_INSTALL));
	const std::atomic<bool> cancel{false};
	RiotClient              client{&cancel};

	static_cast<void>(client.resolve_executable_path(gone));
	CHECK(client.executable_path() != gone);
}

TEST_CASE("errors the Riot Client shows become the right message")
{
	CHECK(std::string_view{login_failure_message("Your login credentials don't match an account in our system.")} == "Wrong username or password.");
	CHECK(std::string_view{login_failure_message("Sorry, we're having trouble signing you in right now. Please try again later.")}.starts_with(
		"Riot's servers are busy"));
	CHECK(std::string_view{login_failure_message("")}.starts_with("Sign-in failed"));
}
