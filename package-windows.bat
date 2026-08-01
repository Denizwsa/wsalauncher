@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"

REM Portable Windows build: wsalauncher.exe + Qt DLLs (no Qt install needed on other PCs)
REM Requires on THIS machine only: Qt6, CMake, C++ compiler (MSVC or MinGW)

where cmake >nul 2>&1
if errorlevel 1 (
  echo ERROR: cmake not found. Install CMake and Qt, then open "Qt MSVC" or "x64 Native Tools" prompt.
  exit /b 1
)

if not defined CMAKE_PREFIX_PATH (
  if defined Qt6_DIR set "CMAKE_PREFIX_PATH=%Qt6_DIR%\.."
)

echo === Configuring ===
if not exist build-win mkdir build-win
cd build-win

if defined CMAKE_PREFIX_PATH (
  cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%CMAKE_PREFIX_PATH%"
) else (
  cmake .. -DCMAKE_BUILD_TYPE=Release
)
if errorlevel 1 exit /b 1

echo === Building ===
cmake --build . --config Release
if errorlevel 1 exit /b 1

set "EXE="
if exist "Release\wsalauncher.exe" set "EXE=%CD%\Release\wsalauncher.exe"
if exist "wsalauncher.exe" set "EXE=%CD%\wsalauncher.exe"
if not defined EXE (
  echo ERROR: wsalauncher.exe not found after build
  exit /b 1
)

echo === Packaging portable folder ===
cd ..
set "DIST=%CD%\dist\Wsalauncher-Windows"
set "STAGE=%CD%\dist\_win_stage"
if exist "%DIST%" rmdir /s /q "%DIST%"
if exist "%STAGE%" rmdir /s /q "%STAGE%"
mkdir "%STAGE%"
copy /y "%EXE%" "%STAGE%\wsalauncher.exe" >nul

REM Locate windeployqt
set "WINDEPLOYQT="
where windeployqt >nul 2>&1 && set "WINDEPLOYQT=windeployqt"
if not defined WINDEPLOYQT if defined CMAKE_PREFIX_PATH (
  if exist "%CMAKE_PREFIX_PATH%\bin\windeployqt.exe" set "WINDEPLOYQT=%CMAKE_PREFIX_PATH%\bin\windeployqt.exe"
)
if not defined WINDEPLOYQT (
  for /d %%Q in ("C:\Qt\6.*") do (
    for /d %%T in ("%%Q\msvc*_64" "%%Q\mingw*_64") do (
      if exist "%%T\bin\windeployqt.exe" set "WINDEPLOYQT=%%T\bin\windeployqt.exe"
    )
  )
)

if not defined WINDEPLOYQT (
  echo ERROR: windeployqt.exe not found. Set CMAKE_PREFIX_PATH to your Qt kit, e.g.
  echo   set CMAKE_PREFIX_PATH=C:\Qt\6.8.0\msvc2022_64
  exit /b 1
)

echo Using: %WINDEPLOYQT%
"%WINDEPLOYQT%" --release --compiler-runtime --network "%STAGE%\wsalauncher.exe"
if errorlevel 1 exit /b 1

REM Portable Qt paths
(
  echo [Paths]
  echo Prefix=.
  echo Plugins=.
  echo Libraries=.
) > "%STAGE%\qt.conf"

REM Optional logo
if exist "wsamiaw-recodelogo.png" copy /y "wsamiaw-recodelogo.png" "%STAGE%\logo.png" >nul
if exist "logo.png" copy /y "logo.png" "%STAGE%\logo.png" >nul

REM Final layout: run.bat + app\ (exe + Qt) — no src
mkdir "%DIST%\app"
xcopy /e /i /y /q "%STAGE%\*" "%DIST%\app\" >nul
copy /y "run.bat" "%DIST%\run.bat" >nul
rmdir /s /q "%STAGE%"

REM Seed repo .\app for local run.bat
set "APPDIR=%CD%\app"
if exist "%APPDIR%" rmdir /s /q "%APPDIR%"
mkdir "%APPDIR%"
xcopy /e /i /y /q "%DIST%\app\*" "%APPDIR%\" >nul

echo === ZIP ===
set "ZIP=%CD%\dist\Wsalauncher-Windows.zip"
if exist "%ZIP%" del /f "%ZIP%"
powershell -NoProfile -Command "Compress-Archive -Path '%DIST%' -DestinationPath '%ZIP%' -Force"
if errorlevel 1 (
  echo ZIP failed — folder is still ready at:
  echo   %DIST%
  exit /b 0
)

echo.
echo DONE. Portable package:
echo   %ZIP%
echo   %DIST%\run.bat
echo   %DIST%\app\wsalauncher.exe
echo.
echo Ship the zip — open run.bat. Qt install NOT required. src not needed.
endlocal
