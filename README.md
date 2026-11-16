# Wsalauncher

Qt6 C++ launcher for **Wsamiaw** (Fabric Minecraft **26.2**).

Supports **Linux**, **Windows**, and **macOS**.

## End users (no source)

Only `run.sh` / `run.bat` (or a release zip) is needed — **no `src/`**, no CMake.

```bash
./run.sh          # Linux: installs into ./app/ then opens UI
run.bat           # Windows: installs into .\app\ then opens UI
```

First run copies or downloads the binary into `app/`. After that you can delete `src/`, `build/`, and CMake files.

Linux menu shortcut (once per install folder):

```bash
./install-desktop.sh
```

Prebuilt packages:

- [launcher-0.3.0](https://github.com/Denizwsa/wsamiaw-recode/releases/tag/launcher-0.3.0)  
  - `Wsalauncher-Linux.tar.gz`  
  - `Wsalauncher-Windows.zip`

Override download URL if needed:

```bash
export WSALAUNCHER_LINUX_URL='https://…/Wsalauncher-Linux.tar.gz'
./run.sh
```

```bat
set WSALAUNCHER_WINDOWS_URL=https://…/Wsalauncher-Windows.zip
run.bat
```

## Requirements (runtime)

| | |
|--|--|
| Game | Java **25+** |
| Account | Sign in on **Account** tab (same as [wsamiawclient.lol](https://wsamiawclient.lol/account.html)) |
| Wsamiaw JAR | Licensed download via API when signed in + HWID on site; else public GitHub release |
| Linux launcher | Qt6 Widgets + Network (system packages) |
| Windows launcher | Nothing else — Qt DLLs ship in `app/` |

## Developers

- CMake 3.16+, Qt 6 (Widgets + Network)

```bash
./package-linux.sh     # build with CMake, output dist/ (developers only)
./run.sh               # never runs CMake — only app/ or download
./run.sh --reinstall   # drop app/ and re-fetch prebuilt
```

```bat
set CMAKE_PREFIX_PATH=C:\Qt\6.8.0\msvc2022_64
package-windows.bat
```

### Windows `.exe` from Linux (cross-compile)

Yes — with **MinGW-w64** + **mingw-w64-qt6** (AUR on Arch/CachyOS):

```bash
sudo pacman -S --needed mingw-w64-gcc mingw-w64-cmake qt6-base qt6-tools
# AUR: mingw-w64-qt6-base mingw-w64-qt6-tools
./package-windows-cross.sh
```

Output: `dist/Wsalauncher-Windows.zip` (same layout as `package-windows.bat`).  
Optional: `wine` so `windeployqt` can run; otherwise the script copies common Qt DLLs.

Without AUR Qt, use **Windows** + `package-windows.bat` or a **GitHub Actions** Windows runner.

## Game data

| OS | Game folder |
|----|-------------|
| Linux | `~/.wsalauncher-minecraft` |
| macOS | `~/Library/Application Support/wsalauncher-minecraft` |
| Windows | `%APPDATA%\wsalauncher-minecraft` |

Launcher config/cache: `~/.wsalauncher` (Linux), `%APPDATA%\wsalauncher` (Windows), macOS Application Support similarly.

## Logo

Place `logo.png` or `wsamiaw-recodelogo.png` next to `run.sh` / inside `app/`.
---
<div align="center">

[![Buy me a coffee](https://img.buymeacoffee.com/button-api/?text=Buy%20me%20a%20coffee&emoji=%E2%98%95&slug=denizkwsa&button_colour=FFDD00&font_colour=000000&font_family=Cookie&outline_colour=000000&coffee_colour=ffffff)](https://buymeacoffee.com/denizkwsa)

</div>
