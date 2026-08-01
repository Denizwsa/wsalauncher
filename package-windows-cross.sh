#!/usr/bin/env bash
# Build portable Wsalauncher-Windows.zip on Linux (no sudo).
# Uses: llvm-mingw (Linux host) + Qt via aqt (win64_llvm_mingw + linux_gcc_64 host).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

LLVM_MINGW="${LLVM_MINGW_ROOT:-$HOME/.local/llvm-mingw}"
QT_WIN="${QT_WIN_ROOT:-$HOME/Qt/6.8.3/llvm-mingw_64}"
QT_HOST="${QT_HOST_ROOT:-$HOME/Qt/6.8.3/gcc_64}"
BUILD="$ROOT/build-win-cross"
DIST="$ROOT/dist/Wsalauncher-Windows"
ZIP="$ROOT/dist/Wsalauncher-Windows.zip"
VENV_AQT="$ROOT/.venv/bin/aqt"

need() { echo "Missing: $1" >&2; exit 1; }

[[ -x "$LLVM_MINGW/bin/x86_64-w64-mingw32-clang++" ]] || {
  echo "Install llvm-mingw (Linux host):" >&2
  echo "  curl -fL -o /tmp/lm.tar.xz https://github.com/mstorsjo/llvm-mingw/releases/download/20260616/llvm-mingw-20260616-ucrt-ubuntu-22.04-x86_64.tar.xz" >&2
  echo "  mkdir -p $LLVM_MINGW && tar -xJf /tmp/lm.tar.xz -C $LLVM_MINGW --strip-components=1" >&2
  exit 1
}

if [[ ! -f "$QT_WIN/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
  command -v "$VENV_AQT" >/dev/null 2>&1 || need "aqt ($VENV_AQT — pip install aqtinstall in .venv)"
  "$VENV_AQT" install-qt windows desktop 6.8.3 win64_llvm_mingw -O "$HOME/Qt" --archives qtbase qtsvg qttranslations
  QT_WIN="$HOME/Qt/6.8.3/llvm-mingw_64"
fi

if [[ ! -f "$QT_HOST/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
  "$VENV_AQT" install-qt linux desktop 6.8.3 linux_gcc_64 -O "$HOME/Qt" --archives qtbase qtsvg qttranslations
  QT_HOST="$HOME/Qt/6.8.3/gcc_64"
fi

export LLVM_MINGW_ROOT="$LLVM_MINGW"
rm -rf "$BUILD"
cmake -S "$ROOT" -B "$BUILD" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/toolchain-llvm-mingw.cmake" \
  -DCMAKE_PREFIX_PATH="$QT_WIN" \
  -DQT_HOST_PATH="$QT_HOST"
cmake --build "$BUILD" -j"$(nproc 2>/dev/null || echo 4)"

EXE="$BUILD/wsalauncher.exe"
[[ -f "$EXE" ]] || need "$EXE"

STAGE="$ROOT/dist/_win_stage"
rm -rf "$STAGE" "$DIST"
mkdir -p "$STAGE" "$DIST/app"
cp -f "$EXE" "$STAGE/wsalauncher.exe"
cp -f "$QT_WIN/bin"/Qt6*.dll "$STAGE/" 2>/dev/null || true
cp -f "$LLVM_MINGW/bin"/libc++.dll "$LLVM_MINGW/bin"/libc++abi.dll "$LLVM_MINGW/bin"/libunwind.dll "$STAGE/" 2>/dev/null || true
for sub in platforms styles tls networkinformation generic imageformats; do
  mkdir -p "$STAGE/$sub"
  cp -r "$QT_WIN/plugins/$sub"/* "$STAGE/$sub/" 2>/dev/null || true
done
printf '[Paths]\nPrefix=.\nPlugins=.\nLibraries=.\n' > "$STAGE/qt.conf"
for logo in "$ROOT/app/logo.png" "$ROOT/logo.png"; do [[ -f "$logo" ]] && cp -f "$logo" "$STAGE/logo.png" && break; done
cp -a "$STAGE/." "$DIST/app/"
cp -f "$ROOT/run.bat" "$DIST/"
rm -rf "$STAGE"
rm -f "$ZIP"
( cd "$ROOT/dist" && zip -r -q "$(basename "$ZIP")" Wsalauncher-Windows )

echo "DONE: $ZIP"
