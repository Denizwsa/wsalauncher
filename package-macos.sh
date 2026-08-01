#!/usr/bin/env bash
# macOS release: Wsalauncher.app + tar.gz (run on a Mac with Qt6 from Homebrew).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "Build the macOS package on a Mac (Qt6 + CMake)." >&2
  exit 1
fi

APP_BUNDLE=""
if [[ -d "$ROOT/build/wsalauncher.app" ]]; then
  APP_BUNDLE="$ROOT/build/wsalauncher.app"
elif [[ -d "$ROOT/build/Release/wsalauncher.app" ]]; then
  APP_BUNDLE="$ROOT/build/Release/wsalauncher.app"
elif [[ -d "$ROOT/app/Wsalauncher.app" ]]; then
  APP_BUNDLE="$ROOT/app/Wsalauncher.app"
else
  echo "Building wsalauncher.app …"
  mkdir -p build
  (cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build . -j"$(sysctl -n hw.ncpu 2>/dev/null || echo 4)")
  APP_BUNDLE="$ROOT/build/wsalauncher.app"
fi

[[ -d "$APP_BUNDLE" ]] || { echo "No wsalauncher.app found after build"; exit 1; }

DIST="$ROOT/dist/Wsalauncher-macOS"
ARCHIVE="$ROOT/dist/Wsalauncher-macOS.tar.gz"
rm -rf "$DIST"
mkdir -p "$DIST/app"

cp -R "$APP_BUNDLE" "$DIST/app/Wsalauncher.app"

MACDEPLOYQT=""
for q in "$(brew --prefix qt@6 2>/dev/null)" "$(brew --prefix qt 2>/dev/null)"; do
  [[ -x "$q/bin/macdeployqt" ]] && MACDEPLOYQT="$q/bin/macdeployqt" && break
done
if [[ -n "$MACDEPLOYQT" ]]; then
  echo "Running macdeployqt …"
  "$MACDEPLOYQT" "$DIST/app/Wsalauncher.app" -always-overwrite
else
  echo "Tip: install Qt6 (brew install qt) and re-run for a self-contained .app"
fi
cp -f "$ROOT/run.sh" "$DIST/run.sh"
cp -f "$ROOT/Wsalauncher" "$DIST/Wsalauncher"
chmod +x "$DIST/run.sh" "$DIST/Wsalauncher"

for logo in "$ROOT/app/logo.png" "$ROOT/logo.png"; do
  [[ -f "$logo" ]] && cp -f "$logo" "$DIST/logo.png" && break
done

cat > "$DIST/README.txt" << 'EOF'
Wsalauncher (macOS)
===================
1. Double-click Wsalauncher or run: ./run.sh
2. Needs Java 25+ for Minecraft. Qt is inside the .app (if you used macdeployqt).
3. Do NOT use Wsalauncher-Linux on Mac — use this package only.
EOF

rm -f "$ARCHIVE"
tar -C "$ROOT/dist" -czf "$ARCHIVE" Wsalauncher-macOS

echo "DONE:"
echo "  $DIST"
echo "  $ARCHIVE"
