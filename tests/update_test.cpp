#include <algorithm>
#include <cstdio>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include <sodium.h>

#include "core/app_identity.h"
#include "core/updater.h"
#include "test.h"

namespace {
struct Signer {
	u8 public_key[crypto_sign_PUBLICKEYBYTES];
	u8 secret_key[crypto_sign_SECRETKEYBYTES];
};

[[nodiscard]] auto make_signer() -> Signer
{
	Signer signer{};
	crypto_sign_keypair(signer.public_key, signer.secret_key);

	return signer;
}

[[nodiscard]] auto bytes_of(std::string_view t_text) -> std::vector<u8>
{
	return std::vector<u8>(t_text.begin(), t_text.end());
}

// Signs the way tools/sign_release does: the signature covers the build's SHA-256, not the build itself.
[[nodiscard]] auto signed_manifest(const std::vector<u8>& t_build, const Signer& t_signer) -> UpdateManifest
{
	u8 digest[crypto_hash_sha256_BYTES];
	crypto_hash_sha256(digest, t_build.data(), t_build.size());

	u8 signature[crypto_sign_BYTES];
	crypto_sign_detached(signature, nullptr, digest, sizeof(digest), t_signer.secret_key);

	UpdateManifest manifest{};
	sodium_bin2hex(manifest.sha256_hex, sizeof(manifest.sha256_hex), digest, sizeof(digest));
	sodium_bin2base64(manifest.signature_base64, sizeof(manifest.signature_base64), signature, sizeof(signature), sodium_base64_VARIANT_ORIGINAL);

	return manifest;
}
}

TEST_CASE("a signed build passes verification")
{
	const Signer          signer   = make_signer();
	const std::vector<u8> build    = bytes_of("pretend this is Pulsar.exe");
	const UpdateManifest  manifest = signed_manifest(build, signer);
	std::string           error;

	CHECK(verify_update(build, manifest, signer.public_key, &error));
	CHECK(error.empty());
}

TEST_CASE("a build changed after signing is refused")
{
	const Signer         signer   = make_signer();
	std::vector<u8>      build    = bytes_of("pretend this is Pulsar.exe");
	const UpdateManifest manifest = signed_manifest(build, signer);
	std::string          error;

	build[0] ^= 1;

	CHECK(!verify_update(build, manifest, signer.public_key, &error));
	CHECK(!error.empty());
}

TEST_CASE("a swapped build with a matching hash but the old signature is refused")
{
	const Signer          signer   = make_signer();
	const std::vector<u8> original = bytes_of("the real build");
	const std::vector<u8> swapped  = bytes_of("someone else's build");
	UpdateManifest        manifest = signed_manifest(original, signer);
	std::string           error;

	const UpdateManifest swapped_hash = signed_manifest(swapped, signer);
	std::copy(std::begin(swapped_hash.sha256_hex), std::end(swapped_hash.sha256_hex), std::begin(manifest.sha256_hex));

	CHECK(!verify_update(swapped, manifest, signer.public_key, &error));
}

TEST_CASE("a build signed with another key is refused")
{
	const Signer          release  = make_signer();
	const Signer          stranger = make_signer();
	const std::vector<u8> build    = bytes_of("pretend this is Pulsar.exe");
	const UpdateManifest  manifest = signed_manifest(build, stranger);
	std::string           error;

	CHECK(!verify_update(build, manifest, release.public_key, &error));
}

TEST_CASE("a malformed signature is refused")
{
	const Signer          signer   = make_signer();
	const std::vector<u8> build    = bytes_of("pretend this is Pulsar.exe");
	UpdateManifest        manifest = signed_manifest(build, signer);
	std::string           error;

	std::copy_n("not base64 at all!", 19, manifest.signature_base64);

	CHECK(!verify_update(build, manifest, signer.public_key, &error));
}

TEST_CASE("a complete manifest parses")
{
	const std::vector<u8> json = bytes_of(R"({"version": "0.8.0", "url": "https://example.com/Pulsar.exe", "sha256": "ab", "signature": "cd",
	                                        "min_upgrade_version": "0.6.0", "notes": "Fixes"})");
	UpdateManifest        manifest{};

	REQUIRE(parse_update_manifest(json, &manifest));
	CHECK(std::string_view{manifest.version} == "0.8.0");
	CHECK(std::string_view{manifest.url} == "https://example.com/Pulsar.exe");
	CHECK(std::string_view{manifest.min_upgrade_version} == "0.6.0");
	CHECK(std::string_view{manifest.notes} == "Fixes");
}

TEST_CASE("a manifest without optional fields gets their defaults")
{
	const std::vector<u8> json = bytes_of(R"({"version": "0.8.0", "url": "u", "sha256": "ab", "signature": "cd"})");
	UpdateManifest        manifest{};

	REQUIRE(parse_update_manifest(json, &manifest));
	CHECK(std::string_view{manifest.min_upgrade_version} == "0.0.0");
	CHECK(std::string_view{manifest.notes}.empty());
}

TEST_CASE("a manifest missing a signature or not JSON at all is refused")
{
	UpdateManifest manifest{};

	CHECK(!parse_update_manifest(bytes_of(R"({"version": "0.8.0", "url": "u", "sha256": "ab"})"), &manifest));
	CHECK(!parse_update_manifest(bytes_of("<html>404</html>"), &manifest));
	CHECK(!parse_update_manifest(bytes_of(R"(["version", "0.8.0"])"), &manifest));
}

// GitHub's release URLs are its own; the updater and tools/sign_release both rely on this layout.
TEST_CASE("update URLs follow GitHub's release layout")
{
	char build_url[256];
	std::snprintf(build_url, sizeof(build_url), K_RELEASE_DOWNLOAD_URL_FORMAT, "0.8.0");

	CHECK(std::string_view{K_UPDATE_MANIFEST_URL} == "https://github.com/xi94/Pulsar/releases/latest/download/update.json");
	CHECK(std::string_view{build_url} == "https://github.com/xi94/Pulsar/releases/download/v0.8.0/Pulsar.exe");
}
