#!/usr/bin/env bash
# Render-vs-engine comparison harness.
#
# Usage:
#   compare.sh <render.png> <screenshot.png> [--crop x,y,w,h] [--match]
#
#   render.png      our --crop / scale-1 map render
#   screenshot.png  F12 screenshot from the real game
#   --crop ...      crop the RENDER to the tile region matching the screenshot
#   --match         auto-locate the screenshot inside the render (for pan/zoom drift)
#
set -euo pipefail
DIR="$(cd "$(dirname "$0")" && pwd)"
python3 "$DIR/pngdiff.py" "$@"
