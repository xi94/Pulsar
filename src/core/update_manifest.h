#pragma once

/// The parsed shape of update.json, published as a release asset alongside every build.
/// Fixed-capacity buffers rather than std::string, so no heap ownership crosses the boundary
/// between CUpdater's worker and the render thread.
struct UpdateManifest {
	char szVersion[32]{};
	char szMinUpgradeVersion[32]{}; // below this, in-app auto-update is refused
	char szUrl[512]{};
	char szSha256Hex[65]{};		   // 64 lowercase hex chars plus a terminator
	char szSignatureBase64[128]{}; // Ed25519 over the raw 32-byte digest above
	char szNotes[1024]{};
};

/// Version, url, sha256 and signature are required; a missing minimum or notes is not a parse
/// error. On failure *pOutManifest holds whatever partial state parsing reached, so only look
/// at it once this returns true.
bool ParseUpdateManifest(const char *pJson, u64 jsonLength, UpdateManifest *pOutManifest);
