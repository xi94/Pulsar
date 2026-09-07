#pragma once

#include <string>

/// %LOCALAPPDATA%\Pulsar\<pSubfolder>, created if absent. Empty if the location cannot be
/// resolved at all, which every caller treats as "write nothing".
///
/// Resolved from the LOCALAPPDATA environment variable, with the known folder as the fallback
/// for the rare process that inherited no environment. Order matters: core/storage.cpp reads
/// the same variable, so honouring it here is what makes every file the app writes move
/// together when it is redirected - which is what a test run needs to stay off the real
/// install's data. Splitting the two would send logs and crash dumps back to the real folder
/// while the settings and vault went somewhere else.
std::wstring AppDataSubdirectory(const wchar_t *pSubfolder);
