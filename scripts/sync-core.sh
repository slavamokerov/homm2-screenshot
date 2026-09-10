#!/usr/bin/env bash
# Syncs the shared core (fh2core + fh2resources) from the fheroes2-save-editor
# repository into this project's vendor/fh2core/ folder.
#
# The authoritative file list lives in the editor's CMakeLists.txt
# (FH2CORE_SOURCES/FH2CORE_HEADERS/FH2RESOURCES_SOURCES/FH2RESOURCES_HEADERS) —
# this script is kept in sync with it manually (see AGENTS.md).
#
# Usage: scripts/sync-core.sh [path-to-fheroes2-save-editor]

set -euo pipefail

EDITOR_DIR="${1:-$HOME/Projects/fheroes2-save-editor}"
PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
VENDOR_DIR="$PROJECT_DIR/vendor/fh2core"

if [[ ! -f "$EDITOR_DIR/src/savefile.cpp" ]]; then
    echo "error: fheroes2-save-editor sources not found at $EDITOR_DIR" >&2
    exit 1
fi

FILES=(
    # fh2core (save parsing, no Qt)
    constants.cpp constants.h
    gettextmo.cpp gettextmo.h
    savefile.cpp savefile.h
    worldparse.cpp worldparse.h
    codepages.h
    # fh2resources (Qt6::Gui)
    aggicn.cpp aggicn.h
    assets.cpp assets.h
    gamefont.cpp gamefont.h
    textutil.h
)

mkdir -p "$VENDOR_DIR"
for f in "${FILES[@]}"; do
    if [[ ! -f "$EDITOR_DIR/src/$f" ]]; then
        echo "error: $f is missing in the editor; update the file list" >&2
        exit 1
    fi
    cp "$EDITOR_DIR/src/$f" "$VENDOR_DIR/"
done

if git -C "$EDITOR_DIR" rev-parse --short HEAD >/dev/null 2>&1; then
    git -C "$EDITOR_DIR" rev-parse HEAD > "$VENDOR_DIR/EDITOR_SHA.txt"
else
    echo "unknown" > "$VENDOR_DIR/EDITOR_SHA.txt"
fi

grep -oE 'FH2CORE_VERSION = [0-9]+' "$VENDOR_DIR/worldparse.h" > "$VENDOR_DIR/CORE_VERSION.txt" || true

echo "synced fh2core -> $VENDOR_DIR"
echo "editor: $(cat "$VENDOR_DIR/EDITOR_SHA.txt")  $(cat "$VENDOR_DIR/CORE_VERSION.txt")"
echo "commit the vendor changes separately (see AGENTS.md)"
