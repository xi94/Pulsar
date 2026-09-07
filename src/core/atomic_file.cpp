#include "core/atomic_file.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <Windows.h>

bool CAtomicFile::BackupPathFor(const char *pPath, char *pOutBuffer, usize bufferSize)
{
	const int written = std::snprintf(pOutBuffer, bufferSize, "%s.bak", pPath);

	return written > 0 && static_cast<usize>(written) < bufferSize;
}

namespace {
// A save that changes nothing must not touch the disk. The app saves on every click that does
// anything at all, and almost none of those clicks change the file - without this, the rotation
// below would retire a known-good .bak on each one, leaving the backup never more than a single
// save old and worthless the moment two bad saves land in a row.
bool ContentMatchesFile(const char *pPath, const void *pData, usize length)
{
	u8 *pExisting = nullptr;
	usize existingLength = 0;
	if (!CAtomicFile::ReadFile(pPath, &pExisting, &existingLength)) return false;

	const bool same = existingLength == length && std::memcmp(pExisting, pData, length) == 0;
	std::free(pExisting);

	return same;
}
} // namespace

bool CAtomicFile::WriteAtomic(const char *pPath, const void *pData, usize length)
{
	if (ContentMatchesFile(pPath, pData, length)) return true;

	char tmpPath[MAX_PATH];
	const int written = std::snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", pPath);
	if (written <= 0 || static_cast<usize>(written) >= sizeof(tmpPath)) return false;

	const HANDLE hFile = CreateFileA(tmpPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hFile == INVALID_HANDLE_VALUE) return false;

	DWORD bytesWritten = 0;
	const bool wroteOk =
		WriteFile(hFile, pData, static_cast<DWORD>(length), &bytesWritten, nullptr) != 0 && bytesWritten == length;

	// CloseHandle alone survives a process crash but not power loss - which is the whole
	// point of this mechanism, so the flush has to be explicit.
	const bool flushOk = wroteOk && FlushFileBuffers(hFile) != 0;
	CloseHandle(hFile);

	if (!flushOk) {
		DeleteFileA(tmpPath);
		return false;
	}

	char backupPath[MAX_PATH];
	if (CAtomicFile::BackupPathFor(pPath, backupPath, sizeof(backupPath))) {
		// Best effort: on the very first save there is nothing to rotate and this fails.
		MoveFileExA(pPath, backupPath, MOVEFILE_REPLACE_EXISTING);
	}

	if (!MoveFileExA(tmpPath, pPath, MOVEFILE_REPLACE_EXISTING)) {
		DeleteFileA(tmpPath);
		return false;
	}

	return true;
}

bool CAtomicFile::ReadFile(const char *pPath, u8 **ppOutData, usize *pOutLength)
{
	FILE *pFile = nullptr;
	if (fopen_s(&pFile, pPath, "rb") != 0 || pFile == nullptr) return false;

	std::fseek(pFile, 0, SEEK_END);
	const long fileSize = std::ftell(pFile);
	std::fseek(pFile, 0, SEEK_SET);

	if (fileSize <= 0) {
		std::fclose(pFile);
		return false;
	}

	auto *pBuffer = static_cast<u8 *>(std::malloc(static_cast<usize>(fileSize)));
	if (pBuffer == nullptr) {
		std::fclose(pFile);
		return false;
	}

	const usize readCount = std::fread(pBuffer, 1, static_cast<usize>(fileSize), pFile);
	std::fclose(pFile);

	if (readCount != static_cast<usize>(fileSize)) {
		std::free(pBuffer);
		return false;
	}

	*ppOutData = pBuffer;
	*pOutLength = readCount;

	return true;
}
