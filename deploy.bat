@echo off
setlocal

set "root=%~dp0"
set "build=%root%build\x64-release"
set "app=%build%\src\Pulsar.exe"
set "signer=%build%\tools\sign_release\sign_release.exe"
set "release=%root%release"

if "%~1"=="" (
	echo Usage: deploy.bat "What changed in this release"
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

"%signer%" --exe "%release%\Pulsar.exe" --notes "%~1" --out "%release%\update.json" || exit /b 1

echo.
echo Upload both files in %release% to a GitHub release tagged v^<version above^>.
