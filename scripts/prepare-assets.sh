#!/usr/bin/env bash
# Copy the game data into the APK assets and write the manifest the game uses to unpack it
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ASSETS="$ROOT/android/app/src/main/assets"
rm -rf "$ASSETS"
mkdir -p "$ASSETS"
cd "$ROOT"
cp -r config packages bot mods "$ASSETS/"
cd "$ASSETS"
STAMP="${VERSION_NAME:-dev}-$(cd "$ROOT" && git rev-parse --short HEAD 2>/dev/null || date +%s)"
{ echo "$STAMP"; find config packages bot mods -type f | sort; } > manifest.txt
echo "assets: $(($(wc -l < manifest.txt) - 1)) files, stamp $STAMP"
