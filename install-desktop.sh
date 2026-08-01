#!/usr/bin/env bash
# Menü kısayolu: ~/.local/share/applications/wsalauncher.desktop
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
DESKTOP_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
DESKTOP_FILE="$DESKTOP_DIR/wsalauncher.desktop"
ICON="$ROOT/app/logo.png"
[[ -f "$ICON" ]] || ICON="$ROOT/logo.png"
[[ -f "$ICON" ]] || ICON=""

mkdir -p "$DESKTOP_DIR"
cat > "$DESKTOP_FILE" <<EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=Wsalauncher
Comment=Wsamiaw · Minecraft 26.2 launcher
Exec=$ROOT/Wsalauncher
Path=$ROOT
Icon=${ICON:-$ROOT/app/logo.png}
Terminal=false
Categories=Game;
StartupNotify=true
EOF

if command -v update-desktop-database >/dev/null 2>&1; then
  update-desktop-database "$DESKTOP_DIR" 2>/dev/null || true
fi

echo "Installed menu entry → $DESKTOP_FILE"
echo "Launcher folder: $ROOT"
