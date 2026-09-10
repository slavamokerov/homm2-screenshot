# Contributing

Thanks for your interest in the project. This page covers how to build, test
and contribute to homm2-screenshot.

## Code of Conduct

Everyone participating in the project is expected to follow the
[Code of Conduct](CODE_OF_CONDUCT.md).

## Building

See [Building from source](README.md#building-from-source) in the README:
CMake ≥ 3.21, Qt6 (Core, Gui), ZLIB and a C++17 compiler.

## Running the tests

```bash
ctest --test-dir build
```

The save round-trip tests (`world_test`, `homm2_test`) run against real save
files and are only added to the test list when the fixtures are present:

```bash
FH2_TEST_SAVES="$HOME/Library/Application Support/fheroes2/files/save" \
H2_TEST_FIXTURES="$HOME/Projects/homm2-to-fheroes2/tests/fixtures" \
ctest --test-dir build
```

Saves are deliberately not committed. Tests only read them.

## Web build

The browser version is the same render code compiled with Emscripten against a
small software reimplementation of the Qt primitives it uses
(`libs/rastercompat/`):

```bash
./web/build.sh   # -> web/deploy/
```

## How the code is organized

- `vendor/fh2core/` — save parsing and resource loading, shared with
  [fheroes2-save-editor](https://github.com/slavamokerov/fheroes2-save-editor).
  Changes here are made in that repository and synced with
  `scripts/sync-core.sh`.
- `vendor/h2core/` — the HoMM2 → fheroes2 save converter, shared with
  [homm2-to-fheroes2](https://github.com/slavamokerov/homm2-to-fheroes2) and
  synced with `scripts/sync-h2core.sh`.
- `libs/fh2poster/` — the poster renderer (map, castles, hero cards, chips,
  layouts).
- `libs/rastercompat/` — the mini-Qt used by the web build.
- `src/` — the CLI and the WASM entry point.

Parts of the renderer are ports of
[fheroes2](https://github.com/ihhub/fheroes2) (GPL-2.0); keep the game's look
and feel as the reference.

## Checklist

- Code comments are in English.
- The native and web builds stay in sync (same render code).
- `ctest --test-dir build` is green, and the CI checks pass.
- For render changes, compare a poster before and after (native CLI vs
  `node web/run_node.cjs`).
