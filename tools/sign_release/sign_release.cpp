// A release-time CLI, not shipped in Pulsar.exe itself: given the built exe and the Ed25519
// secret key core/update_signing_key.h's own public key is paired with, computes the exact
// SHA-256 + signature core/updater.cpp verifies on the other end, and writes them straight
// into a ready-to-upload update.json - see RELEASING.md at the repo root for the full
// release walkthrough this is one step of.
//
// Deliberately built against the exact same vendored libsodium the app itself links (see
// third_party/libsodium) rather than some other tool (OpenSSL's own Ed25519 support uses a
// different key encoding - PEM/PKCS8 - not the raw 64-byte "seed+pubkey" form
// crypto_sign_keypair hands back and this tool expects), so there's no format-mismatch risk
// between what signs a release and what verifies it.
//
// Usage:
//   sign_release --exe <path-to-Pulsar.exe> [--notes <text>]
//                [--version <X.Y.Z>] [--url <download-url>] [--out <path>]
//                [--min-upgrade-version <X.Y.Z>] [--key-hex <128-hex-char-secret-key>]
//
// Only the exe is required. Everything else is derived from it, and the derived values are the
// ones that used to be worth getting wrong:
//
//   --version  read from the exe's own VERSIONINFO resource, so it cannot disagree with the
//              binary being signed - a mismatch there means a release that either never offers
//              itself or offers itself forever
//   --url      built from core/app_identity.h's release-URL convention and that version
//   --out      update.json beside the exe, which is where it gets uploaded from
//
// Each still takes an override for the one-off case: a release published under a different tag,
// or a manifest written somewhere else.
//
// The secret key is read from --key-hex if given, otherwise from the PULSAR_SIGNING_KEY_HEX
// environment variable (the recommended path for CI - see RELEASING.md) - never hard-coded
// here, never written to update.json, never logged.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <Windows.h>

#include <sodium.h>

#include "core/app_identity.h"

namespace {
bool HexDecode(const std::string &hex, unsigned char *pOut, usize outLength)
{
	if (hex.size() != outLength * 2) {
		return false;
	}
	for (usize i = 0; i < outLength; i += 1) {
		unsigned int byte = 0;
		if (sscanf_s(hex.c_str() + i * 2, "%2x", &byte) != 1) {
			return false;
		}
		pOut[i] = static_cast<unsigned char>(byte);
	}
	return true;
}

std::string HexEncode(const unsigned char *pData, usize length)
{
	static constexpr char kHexDigits[] = "0123456789abcdef";
	std::string out(length * 2, '0');
	for (usize i = 0; i < length; i += 1) {
		out[i * 2] = kHexDigits[pData[i] >> 4];
		out[i * 2 + 1] = kHexDigits[pData[i] & 0x0F];
	}
	return out;
}

// Minimal - update.json's own fields are all release notes/version strings/URLs an author
// writes by hand, not arbitrary untrusted input, so this only needs to cover what's actually
// likely to show up in them.
std::string JsonEscape(const std::string &text)
{
	std::string out;
	out.reserve(text.size());
	for (char c : text) {
		switch (c) {
			case '"':
				out += "\\\"";
				break;
			case '\\':
				out += "\\\\";
				break;
			case '\n':
				out += "\\n";
				break;
			case '\r':
				break; // dropped - \n alone is enough, and this avoids double line breaks on
					   // a Windows-authored notes string
			default:
				out += c;
		}
	}
	return out;
}

bool ReadWholeFile(const std::string &path, std::vector<unsigned char> &outBytes)
{
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file) {
		return false;
	}
	const std::streamsize size = file.tellg();
	if (size < 0) {
		return false;
	}
	file.seekg(0, std::ios::beg);
	outBytes.resize(static_cast<usize>(size));
	return static_cast<bool>(file.read(reinterpret_cast<char *>(outBytes.data()), size));
}

[[noreturn]] void Fail(const char *pMessage)
{
	std::fprintf(stderr, "sign_release: %s\n", pMessage);
	std::exit(1);
}

// Reads the FILEVERSION the resource compiler baked into the exe from core/app_identity.h's own
// macros. That makes the binary the single source of truth for what version this release is: there
// is no way to sign one build while claiming another's number.
//
// The product-version string is not used - it is a free-form field, while FILEVERSION is four
// integers the OS itself parses, so it cannot have been typed wrong in a different format.
std::string ReadExeVersion(const std::string &exePath)
{
	const DWORD infoSize = GetFileVersionInfoSizeA(exePath.c_str(), nullptr);
	if (infoSize == 0) return "";

	std::vector<unsigned char> buffer(infoSize);
	if (GetFileVersionInfoA(exePath.c_str(), 0, infoSize, buffer.data()) == FALSE) return "";

	VS_FIXEDFILEINFO *pFixed = nullptr;
	UINT fixedSize = 0;
	if (VerQueryValueA(buffer.data(), "\\", reinterpret_cast<void **>(&pFixed), &fixedSize) == FALSE) return "";

	if (pFixed == nullptr || fixedSize < sizeof(VS_FIXEDFILEINFO)) return "";

	char version[32];
	std::snprintf(version, sizeof(version), "%u.%u.%u", HIWORD(pFixed->dwFileVersionMS),
				  LOWORD(pFixed->dwFileVersionMS), HIWORD(pFixed->dwFileVersionLS));

	return version;
}

std::string ReleaseUrlFor(const std::string &version)
{
	char url[512];
	std::snprintf(url, sizeof(url), kReleaseDownloadUrlFormat, version.c_str());

	return url;
}

// Beside the exe rather than in the working directory: that is the folder the release is uploaded
// from, so the manifest and the binary it describes stay together.
std::string DefaultManifestPath(const std::string &exePath)
{
	std::filesystem::path path(exePath);
	path.replace_filename("update.json");

	return path.string();
}
} // namespace

int main(int argc, char **argv)
{
	std::string exePath;
	std::string version;
	std::string url;
	std::string notes;
	std::string minUpgradeVersion = "0.0.0";
	std::string outPath;
	std::string keyHex;

	for (int i = 1; i < argc; i += 1) {
		const std::string arg = argv[i];
		const auto NextArg = [&]() -> std::string {
			if (i + 1 >= argc) {
				Fail("missing value after option");
			}
			i += 1;
			return argv[i];
		};

		if (arg == "--exe") {
			exePath = NextArg();
		} else if (arg == "--version") {
			version = NextArg();
		} else if (arg == "--url") {
			url = NextArg();
		} else if (arg == "--notes") {
			notes = NextArg();
		} else if (arg == "--min-upgrade-version") {
			minUpgradeVersion = NextArg();
		} else if (arg == "--out") {
			outPath = NextArg();
		} else if (arg == "--key-hex") {
			keyHex = NextArg();
		} else {
			Fail("unknown argument - see this file's own header comment for usage");
		}
	}

	if (exePath.empty()) {
		Fail("--exe is required");
	}

	if (version.empty()) {
		version = ReadExeVersion(exePath);

		if (version.empty()) {
			Fail("could not read a version from --exe - pass --version to override");
		}
	}

	if (url.empty()) {
		url = ReleaseUrlFor(version);
	}

	if (outPath.empty()) {
		outPath = DefaultManifestPath(exePath);
	}

	// _dupenv_s rather than getenv: the returned buffer is this thread's alone, so nothing else in
	// the process can invalidate it between reading and copying, and it is freed the moment the key
	// is in hand.
	if (keyHex.empty()) {
		char *pEnv = nullptr;
		usize envLength = 0;

		if (_dupenv_s(&pEnv, &envLength, "PULSAR_SIGNING_KEY_HEX") == 0 && pEnv != nullptr) {
			keyHex = pEnv;
		}

		std::free(pEnv);
	}
	if (keyHex.empty()) {
		Fail("no secret key - pass --key-hex or set PULSAR_SIGNING_KEY_HEX");
	}

	if (sodium_init() < 0) {
		Fail("libsodium failed to initialize");
	}

	unsigned char secretKey[crypto_sign_SECRETKEYBYTES];
	if (!HexDecode(keyHex, secretKey, sizeof(secretKey))) {
		Fail("--key-hex/PULSAR_SIGNING_KEY_HEX must be exactly 128 hex characters (the 64-byte "
			 "libsodium secret key) - see RELEASING.md");
	}

	std::vector<unsigned char> exeBytes;
	if (!ReadWholeFile(exePath, exeBytes)) {
		Fail("could not read --exe");
	}

	unsigned char digest[crypto_hash_sha256_BYTES];
	crypto_hash_sha256(digest, exeBytes.data(), exeBytes.size());

	unsigned char signature[crypto_sign_BYTES];
	crypto_sign_detached(signature, nullptr, digest, sizeof(digest), secretKey);

	char signatureBase64[sodium_base64_ENCODED_LEN(crypto_sign_BYTES, sodium_base64_VARIANT_ORIGINAL)];
	sodium_bin2base64(signatureBase64, sizeof(signatureBase64), signature, sizeof(signature),
					  sodium_base64_VARIANT_ORIGINAL);

	const std::string sha256Hex = HexEncode(digest, sizeof(digest));

	std::ostringstream json;
	json << "{\n";
	json << "  \"version\": \"" << JsonEscape(version) << "\",\n";
	json << "  \"min_upgrade_version\": \"" << JsonEscape(minUpgradeVersion) << "\",\n";
	json << "  \"url\": \"" << JsonEscape(url) << "\",\n";
	json << "  \"sha256\": \"" << sha256Hex << "\",\n";
	json << "  \"signature\": \"" << signatureBase64 << "\",\n";
	json << "  \"notes\": \"" << JsonEscape(notes) << "\"\n";
	json << "}\n";

	std::ofstream outFile(outPath, std::ios::binary | std::ios::trunc);
	if (!outFile) {
		Fail("could not write --out");
	}
	outFile << json.str();
	outFile.close();

	// The version and URL are echoed because they are usually derived rather than typed, and a
	// release is the wrong thing to find out afterwards was pointed at the wrong tag.
	std::printf("Wrote %s\n\n", outPath.c_str());
	std::printf("version:   %s\n", version.c_str());
	std::printf("url:       %s\n", url.c_str());
	std::printf("sha256:    %s\n", sha256Hex.c_str());
	std::printf("signature: %s\n", signatureBase64);

	// Zero the secret key from memory before this process exits - it lived in a plain local
	// array with no other protection, so this is the only cleanup available to it.
	sodium_memzero(secretKey, sizeof(secretKey));
	return 0;
}
