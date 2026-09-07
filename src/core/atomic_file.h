#pragma once

/// Crash-safe file replacement: write a sibling .tmp, flush it to the disk itself, rotate
/// the previous file to .bak, then rename. The rename is atomic on a same-volume move, so
/// the target is always either the old file or the whole new one.
class CAtomicFile {
  public:
	static bool WriteAtomic(const char *pPath, const void *pData, usize length);

	/// Allocates *ppOutData with malloc; the caller owns it.
	static bool ReadFile(const char *pPath, u8 **ppOutData, usize *pOutLength);

	static bool BackupPathFor(const char *pPath, char *pOutBuffer, usize bufferSize);
};
