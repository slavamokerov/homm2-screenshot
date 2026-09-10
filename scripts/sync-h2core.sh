#!/usr/bin/env bash
# Syncs the homm2-to-fheroes2 core (h2core: parseSave / convert / buildSaveFile)
# from the homm2-to-fheroes2 repository into this project's vendor/h2core/ folder.
#
# The authoritative file list lives in homm2-to-fheroes2/CMakeLists.txt
# (h2core sources) — this script is kept in sync with it manually (see AGENTS.md).
#
# Usage: scripts/sync-h2core.sh [path-to-homm2-to-fheroes2]

set -euo pipefail

H2_DIR="${1:-$HOME/Projects/homm2-to-fheroes2}"
PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
VENDOR_DIR="$PROJECT_DIR/vendor/h2core"

if [[ ! -f "$H2_DIR/src/convert.cpp" ]]; then
    echo "error: homm2-to-fheroes2 sources not found at $H2_DIR" >&2
    exit 1
fi

FILES=(
    homm2_save.cpp homm2_save.h
    fheroes2_save.cpp fheroes2_save.h
    convert.cpp convert.h
    type_by_icn.inc
)

mkdir -p "$VENDOR_DIR"
for f in "${FILES[@]}"; do
    if [[ ! -f "$H2_DIR/src/$f" ]]; then
        echo "error: $f is missing in homm2-to-fheroes2; update the file list" >&2
        exit 1
    fi
    cp "$H2_DIR/src/$f" "$VENDOR_DIR/"
done

if git -C "$H2_DIR" rev-parse --short HEAD >/dev/null 2>&1; then
    git -C "$H2_DIR" rev-parse HEAD > "$VENDOR_DIR/H2_SHA.txt"
else
    echo "unknown" > "$VENDOR_DIR/H2_SHA.txt"
fi

echo "synced h2core -> $VENDOR_DIR"
echo "converter: $(cat "$VENDOR_DIR/H2_SHA.txt")"
echo "commit the vendor changes separately (see AGENTS.md)"
