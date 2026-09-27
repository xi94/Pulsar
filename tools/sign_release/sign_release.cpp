#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <Windows.h>

#include <sodium.h>

#include "core/app_identity.h"

namespace {
constexpr const char *usage =
	"sign_release --exe <Pulsar.exe> [--notes <text>] [--version <X.Y.Z>] [--url <download-url>] "
	"[--out <update.json>] [--min-upgrade-version <X.Y.Z>] [--key-hex <128 hex chars>]";

[[noreturn]] void fail(const char *t_message)
{
	std::fprintf(stderr, "sign_release: %s\n", t_message);
	std::exit(1);
}

bool from_hex(const std::string &t_hex, u8 *t_out, usize t_length)
{
	usize decoded = 0;

	return t_hex.size() == t_length * 2 &&
		   sodium_hex2bin(t_out, t_length, t_hex.c_str(), t_hex.size(), nullptr, &decoded, nullptr) == 0 &&
		   decoded == t_length;
}

std::string to_hex(const u8 *t_data, usize t_length)
{
	std::string hex(t_length * 2 + 1, '\0');
	sodium_bin2hex(hex.data(), hex.size(), t_data, t_length);
	hex.pop_back();

	return hex;
}

std::string json_escaped(std::string_view t_text)
{
	std::string escaped;
	escaped.reserve(t_text.size());

	for (const char character : t_text) {
		switch (character) {
			case '"':
				escaped += "\\\"";
				break;
			case '\\':
				escaped += "\\\\";
				break;
			case '\n':
				escaped += "\\n";
				break;
			case '\r':
				break;
			default:
				escaped += character;
				break;
		}
	}

	return escaped;
}

bool read_whole_file(const std::string &t_path, std::vector<u8> &t_out_bytes)
{
	std::ifstream file(t_path, std::ios::binary | std::ios::ate);
	if (!file) return false;

	const std::streamsize size = file.tellg();
	if (size < 0) return false;

	file.seekg(0, std::ios::beg);
	t_out_bytes.resize(static_cast<usize>(size));

	return static_cast<bool>(file.read(reinterpret_cast<char *>(t_out_bytes.data()), size));
}

std::string exe_version(const std::string &t_exe_path)
{
	const DWORD info_size = GetFileVersionInfoSizeA(t_exe_path.c_str(), nullptr);
	if (info_size == 0) return {};

	std::vector<u8> info(info_size);
	if (!GetFileVersionInfoA(t_exe_path.c_str(), 0, info_size, info.data())) return {};

	VS_FIXEDFILEINFO *fixed = nullptr;
	UINT fixed_size = 0;
	if (!VerQueryValueA(info.data(), "\\", reinterpret_cast<void **>(&fixed), &fixed_size) || fixed == nullptr ||
		fixed_size < sizeof(VS_FIXEDFILEINFO)) {
		return {};
	}

	char version[32];
	std::snprintf(version, sizeof(version), "%u.%u.%u", HIWORD(fixed->dwFileVersionMS), LOWORD(fixed->dwFileVersionMS),
				  HIWORD(fixed->dwFileVersionLS));

	return version;
}

std::string release_url(const std::string &t_version)
{
	char url[512];
	std::snprintf(url, sizeof(url), release_download_url_format, t_version.c_str());

	return url;
}

std::string key_from_environment()
{
	char *value = nullptr;
	usize length = 0;
	std::string key;

	if (_dupenv_s(&value, &length, "PULSAR_SIGNING_KEY_HEX") == 0 && value != nullptr) {
		key = value;
	}

	std::free(value);

	return key;
}
}

int main(int t_argc, char **t_argv)
{
	std::string exe_path;
	std::string version;
	std::string url;
	std::string notes;
	std::string min_upgrade_version = "0.0.0";
	std::string out_path;
	std::string key_hex;

	for (int i = 1; i < t_argc; i += 1) {
		const std::string_view option = t_argv[i];
		if (i + 1 >= t_argc) fail(usage);

		const std::string value = t_argv[i + 1];
		i += 1;

		if (option == "--exe") {
			exe_path = value;
		} else if (option == "--version") {
			version = value;
		} else if (option == "--url") {
			url = value;
		} else if (option == "--notes") {
			notes = value;
		} else if (option == "--min-upgrade-version") {
			min_upgrade_version = value;
		} else if (option == "--out") {
			out_path = value;
		} else if (option == "--key-hex") {
			key_hex = value;
		} else {
			fail(usage);
		}
	}

	if (exe_path.empty()) fail("--exe is required");

	if (version.empty()) {
		version = exe_version(exe_path);
		if (version.empty()) fail("could not read a version from --exe - pass --version to override");
	}

	if (url.empty()) {
		url = release_url(version);
	}

	if (out_path.empty()) {
		out_path = std::filesystem::path(exe_path).replace_filename("update.json").string();
	}

	if (key_hex.empty()) {
		key_hex = key_from_environment();
	}

	if (key_hex.empty()) fail("no secret key - pass --key-hex or set PULSAR_SIGNING_KEY_HEX");
	if (sodium_init() < 0) fail("libsodium failed to initialize");

	u8 secret_key[crypto_sign_SECRETKEYBYTES];
	if (!from_hex(key_hex, secret_key, sizeof(secret_key))) {
		fail("--key-hex/PULSAR_SIGNING_KEY_HEX must be exactly 128 hex characters (the 64-byte libsodium secret key)");
	}

	std::vector<u8> exe_bytes;
	if (!read_whole_file(exe_path, exe_bytes)) fail("could not read --exe");

	u8 digest[crypto_hash_sha256_BYTES];
	crypto_hash_sha256(digest, exe_bytes.data(), exe_bytes.size());

	u8 signature[crypto_sign_BYTES];
	crypto_sign_detached(signature, nullptr, digest, sizeof(digest), secret_key);
	sodium_memzero(secret_key, sizeof(secret_key));

	char signature_base64[sodium_base64_ENCODED_LEN(crypto_sign_BYTES, sodium_base64_VARIANT_ORIGINAL)];
	sodium_bin2base64(signature_base64, sizeof(signature_base64), signature, sizeof(signature),
					  sodium_base64_VARIANT_ORIGINAL);

	const std::string sha256_hex = to_hex(digest, sizeof(digest));

	std::ostringstream manifest;
	manifest << "{\n";
	manifest << "  \"version\": \"" << json_escaped(version) << "\",\n";
	manifest << "  \"min_upgrade_version\": \"" << json_escaped(min_upgrade_version) << "\",\n";
	manifest << "  \"url\": \"" << json_escaped(url) << "\",\n";
	manifest << "  \"sha256\": \"" << sha256_hex << "\",\n";
	manifest << "  \"signature\": \"" << signature_base64 << "\",\n";
	manifest << "  \"notes\": \"" << json_escaped(notes) << "\"\n";
	manifest << "}\n";

	std::ofstream out_file(out_path, std::ios::binary | std::ios::trunc);
	if (!out_file) fail("could not write --out");

	out_file << manifest.str();

	std::printf("Wrote %s\n\n", out_path.c_str());
	std::printf("version:   %s\n", version.c_str());
	std::printf("url:       %s\n", url.c_str());
	std::printf("sha256:    %s\n", sha256_hex.c_str());
	std::printf("signature: %s\n", signature_base64);

	return 0;
}
