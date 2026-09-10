#!/usr/bin/env bash
# Builds the WASM core of the browser poster generator (GitHub Pages).
# Requires the Emscripten SDK (emcc/em++) on PATH. Output: web/deploy/.
#
# This is the HEADLESS, Qt-free build: it compiles the shared render code
# (fh2resources + fh2poster) against the rastercompat mini-Qt (libs/rastercompat/
# include), so no Qt for WebAssembly is needed. Same em++ pattern as the sibling
# projects (fheroes2-save-editor, homm2-to-fheroes2).
set -euo pipefail

ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )"
OUT="$ROOT/web/deploy"
mkdir -p "$OUT"

INC=(
  "-I$ROOT/src"
  "-I$ROOT/libs/fh2poster"
  "-I$ROOT/libs/fh2poster/layout"
  "-I$ROOT/libs/rastercompat/include"
  "-I$ROOT/vendor/fh2core"
  "-I$ROOT/vendor/h2core"
)

SRC=(
  # fh2core (save parsing, no Qt)
  "$ROOT/vendor/fh2core/constants.cpp"
  "$ROOT/vendor/fh2core/gettextmo.cpp"
  "$ROOT/vendor/fh2core/savefile.cpp"
  "$ROOT/vendor/fh2core/worldparse.cpp"
  # fh2resources (games assets, mini-Qt)
  "$ROOT/vendor/fh2core/aggicn.cpp"
  "$ROOT/vendor/fh2core/assets.cpp"
  "$ROOT/vendor/fh2core/gamefont.cpp"
  # h2core (HoMM2 converter)
  "$ROOT/vendor/h2core/homm2_save.cpp"
  "$ROOT/vendor/h2core/fheroes2_save.cpp"
  "$ROOT/vendor/h2core/convert.cpp"
  # fh2poster (rendering, mini-Qt)
  "$ROOT/libs/fh2poster/maprender.cpp"
  "$ROOT/libs/fh2poster/castlerender.cpp"
  "$ROOT/libs/fh2poster/herocard.cpp"
  "$ROOT/libs/fh2poster/infochips.cpp"
  "$ROOT/libs/fh2poster/poster.cpp"
  "$ROOT/libs/fh2poster/layout_json.cpp"
  "$ROOT/libs/fh2poster/layout/layout_engine.cpp"
  "$ROOT/libs/fh2poster/layout/layout_presets.cpp"
  # rastercompat (mini-Qt software renderer)
  "$ROOT/libs/rastercompat/rastercompat.cpp"
  # this project's bridge + wasm entry
  "$ROOT/src/homm2_bridge.cpp"
  "$ROOT/src/wasm_api.cpp"
)

em++ -O2 -std=c++17 "${INC[@]}" \
  --no-entry \
  -s WASM=1 \
  -s MODULARIZE=1 \
  -s EXPORT_NAME=createFh2PosterModule \
  -s USE_ZLIB=1 \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s MAXIMUM_MEMORY=4GB \
  -s STACK_SIZE=8388608 \
  -s ENVIRONMENT=web,node \
  -s EXPORTED_RUNTIME_METHODS=[] \
  -s FILESYSTEM=0 \
  -s DISABLE_EXCEPTION_CATCHING=0 \
  --bind \
  -o "$OUT/fh2poster.js" \
  "${SRC[@]}"

# Static site files.
cp "$ROOT/web/index.html" "$ROOT/web/style.css" "$OUT/"
if [ -f "$ROOT/web/robots.txt" ]; then cp "$ROOT/web/robots.txt" "$OUT/"; fi
if [ -f "$ROOT/web/sitemap.xml" ]; then cp "$ROOT/web/sitemap.xml" "$OUT/"; fi
if [ -f "$ROOT/web/icon.png" ]; then cp "$ROOT/web/icon.png" "$OUT/"; fi
if [ -f "$ROOT/web/og.png" ]; then cp "$ROOT/web/og.png" "$OUT/"; fi

echo "Built. Deploy folder: $OUT"
ls -lh "$OUT"
