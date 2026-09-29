@echo off
setlocal

set "root=%~dp0"
set "build=%root%build\x64-release"
set "app=%build%\src\Pulsar.exe"
set "signer=%build%\tools\sign_release\sign_release.exe"
set "release=%root%release"

set "notes="
set "notes_file="

if "%~1"=="" goto default_notes
if exist "%~1" set "notes_file=%~f1"
if not defined notes_file set "notes=%*"
goto have_notes

:default_notes
if exist "%release%\notes.txt" set "notes_file=%release%\notes.txt"

:have_notes
if defined notes set "notes=%notes:"=%"

if not defined notes if not defined notes_file (
	echo Usage: deploy.bat [notes-file]
	echo.
	echo Write the release notes to release\notes.txt, or pass another file.
	echo   - Lines starting with "- " become bullets.
	echo   - A short line right above bullets becomes a section heading.
	echo   - At most 1023 characters.
	echo deploy.bat "One line of notes" still works for single-line notes.
	exit /b 1
)

if not exist "%app%" (
	echo Missing %app%
	echo Build the x64-release preset first.
	exit /b 1
)

if not exist "%signer%" (
	echo Missing %signer%
	echo Build the x64-release preset first.
	exit /b 1
)

if not defined PULSAR_SIGNING_KEY_HEX (
	echo PULSAR_SIGNING_KEY_HEX is not set. Set it once with:
	echo     setx PULSAR_SIGNING_KEY_HEX your-128-hex-character-key
	echo Then open a new terminal.
	exit /b 1
)

for %%f in ("%app%") do echo Signing Pulsar.exe built %%~tf
echo.

if not exist "%release%" mkdir "%release%"
copy /y "%app%" "%release%\Pulsar.exe" >nul || exit /b 1

if defined notes_file goto sign_with_file

"%signer%" --exe "%release%\Pulsar.exe" --notes "%notes%" --out "%release%\update.json" || exit /b 1
goto signed

:sign_with_file
echo Using notes from %notes_file%
"%signer%" --exe "%release%\Pulsar.exe" --notes-file "%notes_file%" --out "%release%\update.json" || exit /b 1

:signed
echo.
echo Upload Pulsar.exe and update.json from %release% to a GitHub release tagged v^<version above^>.
