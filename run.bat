@echo off
REM Wsalauncher — cift tikla: app\wsalauncher.exe (CMake/derleme yok)
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"

set "ROOT=%~dp0"
set "APP=%ROOT%app"
set "BIN=%APP%\wsalauncher.exe"
set "CACHE=%LOCALAPPDATA%\wsalauncher\cache"
if not defined WSALAUNCHER_WINDOWS_URL (
  set "WSALAUNCHER_WINDOWS_URL=https://cdn.wsamiawclient.lol/releases/launcher/Wsalauncher-Windows.zip"
)

if /i "%~1"=="--reinstall" (
  shift
  if exist "%APP%" rmdir /s /q "%APP%"
)

if exist "%BIN%" goto :launch

if exist "%ROOT%dist\Wsalauncher-Windows\app\wsalauncher.exe" (
  echo Installing from dist\Wsalauncher-Windows\app ...
  if not exist "%APP%" mkdir "%APP%"
  xcopy /e /i /y /q "%ROOT%dist\Wsalauncher-Windows\app\*" "%APP%\" >nul
  goto :launch
)
if exist "%ROOT%dist\Wsalauncher-Windows\wsalauncher.exe" (
  echo Installing from dist\Wsalauncher-Windows ...
  if not exist "%APP%" mkdir "%APP%"
  xcopy /e /i /y /q "%ROOT%dist\Wsalauncher-Windows\*" "%APP%\" >nul
  if exist "%APP%\run.bat" del /f /q "%APP%\run.bat"
  goto :launch
)

echo Downloading prebuilt Wsalauncher ...
if not exist "%CACHE%" mkdir "%CACHE%"
set "ZIP=%CACHE%\Wsalauncher-Windows.zip"
powershell -NoProfile -Command ^
  "try { Invoke-WebRequest -Uri $env:WSALAUNCHER_WINDOWS_URL -OutFile '%ZIP%' -UseBasicParsing } catch { exit 1 }"
if errorlevel 1 goto :missing

set "TMP=%TEMP%\wsalauncher-extract-%RANDOM%"
if exist "%TMP%" rmdir /s /q "%TMP%"
mkdir "%TMP%"
powershell -NoProfile -Command "Expand-Archive -Path '%ZIP%' -DestinationPath '%TMP%' -Force"
if errorlevel 1 goto :missing

set "FOUND="
for /r "%TMP%" %%F in (wsalauncher.exe) do (
  if not defined FOUND set "FOUND=%%F"
)
if not defined FOUND goto :missing

if not exist "%APP%" mkdir "%APP%"
for %%I in ("%FOUND%") do set "FOUNDIR=%%~dpI"
xcopy /e /i /y /q "%FOUNDIR%*" "%APP%\" >nul
rmdir /s /q "%TMP%"
goto :launch

:missing
echo wsalauncher.exe not found at %BIN%
echo Put the built exe in app\ or ship dist\Wsalauncher-Windows from package-windows.bat.
echo run.bat never compiles — build on a Qt machine first.
pause
exit /b 1

:launch
if not exist "%BIN%" (
  echo wsalauncher.exe not found at %BIN%
  pause
  exit /b 1
)

if exist "%ROOT%logo.png" copy /y "%ROOT%logo.png" "%APP%\logo.png" >nul 2>&1
if exist "%ROOT%wsamiaw-recodelogo.png" copy /y "%ROOT%wsamiaw-recodelogo.png" "%APP%\logo.png" >nul 2>&1

start "" "%BIN%" %*
exit /b 0
