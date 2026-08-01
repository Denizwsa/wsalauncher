#!/usr/bin/env bash
# Creates dist/Wsalauncher-Linux/ (no src) + .tar.gz for GitHub releases.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

BIN=""
if [[ -x "$ROOT/app/wsalauncher" ]]; then
  BIN="$ROOT/app/wsalauncher"
elif [[ -x "$ROOT/build/wsalauncher" ]]; then
  BIN="$ROOT/build/wsalauncher"
else
  echo "Building…"
  mkdir -p build
  (cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build . -j"$(nproc 2>/dev/null || echo 4)")
  BIN="$ROOT/build/wsalauncher"
fi

if [[ ! -x "$BIN" ]]; then
  echo "No binary to package"
  exit 1
fi

DIST="$ROOT/dist/Wsalauncher-Linux"
rm -rf "$DIST"
mkdir -p "$DIST/app"

cp -f "$BIN" "$DIST/app/wsalauncher"
chmod +x "$DIST/app/wsalauncher"

# Bootstrap scripts (install/run without src)
cp -f "$ROOT/run.sh" "$DIST/run.sh"
chmod +x "$DIST/run.sh"
cp -f "$ROOT/Wsalauncher" "$DIST/Wsalauncher"
chmod +x "$DIST/Wsalauncher"
cp -f "$ROOT/install-desktop.sh" "$DIST/install-desktop.sh"
chmod +x "$DIST/install-desktop.sh"

for logo in "$ROOT/app/logo.png" "$ROOT/logo.png" "$ROOT/wsamiaw-recodelogo.png" \
            "$HOME/Belgeler/wsamiaw-recodelogo.png"; do
  if [[ -f "$logo" ]]; then
    cp -f "$logo" "$DIST/app/logo.png"
    cp -f "$logo" "$DIST/logo.png"
    break
  fi
done

cat > "$DIST/README.txt" << 'EOF'
Wsalauncher (Linux only)
========================
For macOS use Wsalauncher-macOS from the download page — not this archive.

1. Double-click Wsalauncher or run: ./run.sh
2. Optional menu icon: ./install-desktop.sh
3. Needs system Qt6 (Widgets + Network) and Java 25+ for the game.
4. No source / CMake required — binary is in app/
   You may delete src/, build/, CMakeLists.txt after first run.
EOF

ARCHIVE="$ROOT/dist/Wsalauncher-Linux.tar.gz"
rm -f "$ARCHIVE"
tar -C "$ROOT/dist" -czf "$ARCHIVE" Wsalauncher-Linux

echo "DONE:"
echo "  $DIST"
echo "  $ARCHIVE"
