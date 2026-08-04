#!/usr/bin/env bash
# Wsalauncher — ./run.sh / ./Wsalauncher (Linux + macOS; Windows = run.bat)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

APP="$ROOT/app"
CACHE="$HOME/.wsalauncher/cache"
UNAME="$(uname -s)"

LINUX_ASSET_URL="${WSALAUNCHER_LINUX_URL:-https://wsamiawclient.lol/releases/launcher/Wsalauncher-Linux.tar.gz}"
MACOS_ASSET_URL="${WSALAUNCHER_MACOS_URL:-https://wsamiawclient.lol/releases/launcher/Wsalauncher-macOS.tar.gz}"

if [[ "${1:-}" == "--reinstall" ]]; then
  shift
  rm -rf "$APP"
fi

copy_logo() {
  local dest="$1"
  for logo in \
    "$ROOT/logo.png" \
    "$ROOT/wsamiaw-recodelogo.png" \
    "$APP/logo.png" \
    "$HOME/Documents/wsamiaw-recodelogo.png"
  do
    if [[ -f "$logo" ]]; then
      cp -f "$logo" "$dest/logo.png" 2>/dev/null || true
      return 0
    fi
  done
}

fail_wrong_platform() {
  echo "This Wsalauncher package is for $1, but you are on $2." >&2
  echo "" >&2
  case "$2" in
    Darwin)
      echo "On Mac, download **Wsalauncher-macOS** from https://wsamiawclient.lol/download.html" >&2
      echo "Do not use Wsalauncher-Linux on macOS." >&2
      ;;
    Linux)
      echo "On Linux, download **Wsalauncher-Linux** (not Windows .zip or macOS)." >&2
      ;;
  esac
  exit 1
}

check_not_elf_on_mac() {
  local f="$1"
  [[ -f "$f" ]] || return 0
  [[ "$UNAME" != "Darwin" ]] && return 0
  if file -b "$f" 2>/dev/null | grep -qi 'ELF'; then
    fail_wrong_platform "Linux" "Darwin"
  fi
}

macos_bin() {
  for p in \
    "$APP/Wsalauncher.app/Contents/MacOS/wsalauncher" \
    "$APP/wsalauncher.app/Contents/MacOS/wsalauncher"
  do
    if [[ -x "$p" ]]; then
      echo "$p"
      return 0
    fi
  done
  return 1
}

linux_bin() {
  [[ -x "$APP/wsalauncher" ]] && echo "$APP/wsalauncher" && return 0
  return 1
}

launcher_binary_stale() {
  local bin="$1"
  [[ -f "$bin" ]] || return 1
  if strings "$bin" 2>/dev/null | grep -qF 'api.github.com/repos/Denizwsa'; then
    return 0
  fi
  return 1
}

purge_stale_launcher() {
  echo "Eski launcher sürümü bulundu (GitHub JAR) — site paketi indiriliyor…" >&2
  rm -rf "$APP"
}

install_linux_from_dist() {
  local dist_bin="$ROOT/dist/Wsalauncher-Linux/app/wsalauncher"
  [[ -x "$dist_bin" ]] || return 1
  mkdir -p "$APP"
  cp -f "$dist_bin" "$APP/wsalauncher"
  chmod +x "$APP/wsalauncher"
  copy_logo "$APP"
}

install_linux_from_url() {
  echo "Downloading Wsalauncher (Linux) …"
  mkdir -p "$CACHE" "$APP"
  local archive="$CACHE/Wsalauncher-Linux.tar.gz"
  local tmp
  tmp="$(mktemp -d)"
  curl -fsSL -o "$archive" "$LINUX_ASSET_URL" || { rm -rf "$tmp"; return 1; }
  tar -xzf "$archive" -C "$tmp"
  local found
  found="$(find "$tmp" -type f -name wsalauncher | head -1 || true)"
  [[ -n "$found" && -f "$found" ]] || { rm -rf "$tmp"; return 1; }
  cp -f "$found" "$APP/wsalauncher"
  chmod +x "$APP/wsalauncher"
  copy_logo "$APP"
  rm -rf "$tmp"
}

install_macos_from_dist() {
  local dist_app="$ROOT/dist/Wsalauncher-macOS/Wsalauncher.app"
  [[ -d "$dist_app" ]] || dist_app="$ROOT/dist/Wsalauncher-macOS/app/Wsalauncher.app"
  [[ -d "$dist_app" ]] || return 1
  mkdir -p "$APP"
  rm -rf "$APP/Wsalauncher.app" "$APP/wsalauncher.app"
  cp -R "$dist_app" "$APP/"
  copy_logo "$APP"
}

install_macos_from_url() {
  echo "Downloading Wsalauncher (macOS) …"
  mkdir -p "$CACHE" "$APP"
  local archive="$CACHE/Wsalauncher-macOS.tar.gz"
  local tmp
  tmp="$(mktemp -d)"
  curl -fsSL -o "$archive" "$MACOS_ASSET_URL" || { rm -rf "$tmp"; return 1; }
  tar -xzf "$archive" -C "$tmp"
  local found
  found="$(find "$tmp" -name 'wsalauncher.app' -o -name 'Wsalauncher.app' | head -1 || true)"
  if [[ -z "$found" ]]; then
    found="$(find "$tmp" -path '*/Contents/MacOS/wsalauncher' | head -1 || true)"
    [[ -n "$found" ]] && found="$(dirname "$(dirname "$(dirname "$found")")")"
  fi
  [[ -n "$found" && -d "$found" ]] || { rm -rf "$tmp"; return 1; }
  rm -rf "$APP/Wsalauncher.app" "$APP/wsalauncher.app"
  cp -R "$found" "$APP/"
  copy_logo "$APP"
  rm -rf "$tmp"
}

ensure_linux() {
  local bin
  bin="$(linux_bin 2>/dev/null || true)"
  if [[ -n "$bin" ]] && launcher_binary_stale "$bin"; then
    purge_stale_launcher
    bin=""
  fi
  if [[ -n "$bin" ]]; then
    check_not_elf_on_mac "$bin"
    return 0
  fi
  install_linux_from_dist || install_linux_from_url || {
    echo "Linux wsalauncher not found in app/" >&2
    exit 1
  }
}

ensure_macos() {
  if macos_bin >/dev/null; then
    return 0
  fi
  # Legacy mistake: Linux binary in app/
  if [[ -f "$APP/wsalauncher" ]]; then
    check_not_elf_on_mac "$APP/wsalauncher"
  fi
  install_macos_from_dist || install_macos_from_url || {
    echo "macOS Wsalauncher.app not found." >&2
    echo "Download Wsalauncher-macOS from https://wsamiawclient.lol/download.html" >&2
    echo "Building: on a Mac with Qt6, run ./package-macos.sh and share the .tar.gz." >&2
    exit 1
  }
}

case "$UNAME" in
  Darwin)
    ensure_macos
    copy_logo "$APP"
    exec "$(macos_bin)"
    ;;
  Linux)
    ensure_linux
    copy_logo "$APP"
    exec "$(linux_bin)"
    ;;
  *)
    echo "Use run.bat on Windows. This script supports Linux and macOS only." >&2
    exit 1
    ;;
esac
