#include "core/update_manifest.h"

#include <cstring>

#include <nlohmann/json.hpp>

namespace {
// Truncates rather than failing on overflow, matching std::string_view::CopyTo's convention. False
// means the key is missing or not a string.
bool CopyStringField(const nlohmann::json &json, const char *pKey, char *pDest, usize destCapacity)
{
	const auto it = json.find(pKey);
	if (it == json.end() || !it->is_string()) return false;

	const std::string &value = it->get_ref<const std::string &>();
	const usize length = value.size() < destCapacity - 1 ? value.size() : destCapacity - 1;
	std::memcpy(pDest, value.data(), length);
	pDest[length] = '\0';

	return true;
}
} // namespace

bool ParseUpdateManifest(const char *pJson, u64 jsonLength, UpdateManifest *pOutManifest)
{
	nlohmann::json parsed;
	try {
		parsed = nlohmann::json::parse(pJson, pJson + jsonLength);
	} catch (const nlohmann::json::exception &) {
		return false;
	}

	if (!parsed.is_object()) return false;

	const bool hasRequiredFields =
		CopyStringField(parsed, "version", pOutManifest->szVersion, sizeof(pOutManifest->szVersion)) &&
		CopyStringField(parsed, "url", pOutManifest->szUrl, sizeof(pOutManifest->szUrl)) &&
		CopyStringField(parsed, "sha256", pOutManifest->szSha256Hex, sizeof(pOutManifest->szSha256Hex)) &&
		CopyStringField(parsed, "signature", pOutManifest->szSignatureBase64, sizeof(pOutManifest->szSignatureBase64));
	if (!hasRequiredFields) return false;

	if (!CopyStringField(parsed, "min_upgrade_version", pOutManifest->szMinUpgradeVersion,
						 sizeof(pOutManifest->szMinUpgradeVersion))) {
		std::memcpy(pOutManifest->szMinUpgradeVersion, "0.0.0", sizeof("0.0.0"));
	}

	if (!CopyStringField(parsed, "notes", pOutManifest->szNotes, sizeof(pOutManifest->szNotes))) {
		pOutManifest->szNotes[0] = '\0';
	}

	return true;
}
