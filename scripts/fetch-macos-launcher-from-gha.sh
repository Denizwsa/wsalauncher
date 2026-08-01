#!/usr/bin/env bash
# Trigger GitHub Actions macOS build and optionally copy artifact to site releases/.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REPO="${WSALAUNCHER_GITHUB_REPO:-Denizwsa/wsalauncher}"
SITE_LAUNCHER="${WSAMIAW_SITE_DIR:-$ROOT/../İndirilenler/wsamiaw-site-push}/releases/launcher"

if ! command -v gh >/dev/null 2>&1; then
  echo "Install GitHub CLI (gh) and authenticate: gh auth login" >&2
  exit 1
fi

echo "Repository: $REPO"
echo "Starting workflow build-launcher-macos.yml …"
gh workflow run build-launcher-macos.yml -R "$REPO" || {
  echo "If the repo does not exist yet, push wsalaunchere to github.com/$REPO first." >&2
  exit 1
}

echo "Waiting for run to finish (may take 10–20 min) …"
sleep 15
RUN_ID="$(gh run list -R "$REPO" -w build-launcher-macos.yml -L 1 --json databaseId -q '.[0].databaseId')"
gh run watch "$RUN_ID" -R "$REPO" --exit-status

TMP="$(mktemp -d)"
gh run download "$RUN_ID" -R "$REPO" -n Wsalauncher-macOS -D "$TMP"
ART="$TMP/Wsalauncher-macOS.tar.gz"
[[ -f "$ART" ]] || ART="$(find "$TMP" -name 'Wsalauncher-macOS.tar.gz' | head -1)"
[[ -f "$ART" ]] || { echo "Artifact not found"; exit 1; }

mkdir -p "$SITE_LAUNCHER"
cp -f "$ART" "$SITE_LAUNCHER/Wsalauncher-macOS.tar.gz"
python3 <<PY
import json
p = "$SITE_LAUNCHER/manifest.json"
m = json.load(open(p))
m.setdefault("macos", {})["file"] = "Wsalauncher-macOS.tar.gz"
json.dump(m, open(p, "w"), indent=2)
open(p, "a").write("\n")
print("Updated", p)
PY

echo "Done: $SITE_LAUNCHER/Wsalauncher-macOS.tar.gz"
echo "Deploy site: npx wrangler pages deploy …"
