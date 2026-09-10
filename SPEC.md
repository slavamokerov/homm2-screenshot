# homm2-screenshot — poster generator (design spec)

A standalone project, a neighbour of fheroes2-save-editor. From a save file
(`*.sav / *.savc / *.savm / *.savh`, or an original HoMM2 `.GM1/.GMC/.GXC`) it
renders a large PNG poster, pleasant to print on a wall:

- the whole adventure map with the explored area (fog of war from the point of
  view of the chosen player; an option to disable fog);
- the full views of every castle "as in the game" (3D buildings per the built
  structures + garrison + captain + visitors);
- cards for all heroes (portrait, class, level, army, artifacts);
- optionally: minimap, player resources, event calendar, rumors, the obelisk
  puzzle, the date, and the victory/loss conditions.

## Goal

Implement a CLI tool `homm2-screenshot` that opens an fheroes2 save (format
10032–10034), renders the map and the castles, lays out the poster and saves a
PNG (300 DPI for printing). The same renderer also builds for the browser as
WASM (see `AGENTS.md` — "Web build").

## Local repositories (mandatory sources)

- `~/Projects/fheroes2-reference` (commit 41f7112) — a clone of the fheroes2
  sources. The only source for formats and render algorithms:
  `src/fheroes2/maps/maps_tiles_render.cpp`, `gui/interface_gamearea.cpp`,
  `castle/castle_building.cpp`, `castle/castle_dialog.cpp`,
  `maps/maps_tiles_helper.cpp` (fog), `world/world.cpp` (serialization).
  Do not download sources from the internet or open them.
- `~/Projects/fheroes2-save-editor` — the neighbouring project (save editor):
  ready `fh2core` (save parsing, `src/savefile.cpp`) and the resource loader
  AGG/ICN/TIL/KB.PAL (`src/aggicn.cpp`, pixel-verified against the engine).
  Save format documentation: `FH2_SAVE_FORMAT.md`.
- How to wire the editor code into this project (submodule / symlink /
  vendor copy) — decided during implementation (vendor + live modes, see
  `AGENTS.md`).

## Result of the research (what is already known)

### Data in the save (all big-endian, in the zlib block)

- World: width/height u32, `vec_tiles` (width×height tiles, ~90% of the stream),
  AllHeroes (73), AllCastles, Kingdoms, customRumors, `vec_eventsday`,
  `map_captureobj`, `_ultimateArtifact`, `_day/_week/_month`,
  heroIdAsWin/LossCondition, `map_objects`, `_seed`.
- Tile: `_index` i32, `_terrainImageIndex` u16 (→ GROUND32), `_terrainFlags` u8,
  `_tilePassabilityDirections` u16, `_mainObjectPart` (layerType u8, uid i32,
  icnType i32, icnIndex i32), `_mainObjectType` u16, `_fogColors` u8
  (per-player fog, bitmask), `_metadata` (3×u32), `_occupantHeroId` u8,
  `_isTileMarkedAsRoad` u8, `_groundObjectPart`/`_topObjectPart` lists
  (u32 count + parts), `_boatOwnerColor` u8.
- Castle: position (i16×2), modes u32, race, constructedBuildings u32,
  disabledBuildings u32, captain (full HeroBase), color u8, name (string),
  mageGuild (2×SpellStorage), dwelling (u32 count + u32 each), army
  (5 slots: i32 monsterId + u32 count).
- Kingdom: modes u32, color u8, 7×u32 resources, lostTownDays i32,
  castles (vector<i32> indices), heroes (vector<i32> ids), recruits,
  visit_object, puzzle (string + 3 zone bytes), tents, topCastle, topHero,
  (10034: monstersUnderVision).
- EventDate (`vec_eventsday`): Funds, isAI bool, day u32, period u32,
  colors u8, message/title strings.
- Players: colors u8, currentColor u8, Player: modes u32, control u8
  (CONTROL_HUMAN/AI), color u8, race, friendsColors u8, name, focus,
  personality, handicap.
- GameOver::Result — the high-score table.

### Map rendering (ported from `~/Projects/fheroes2-reference`)

- Terrain: GROUND32[`_terrainImageIndex`]; roads/rivers: STON (TERRAIN_LAYER).
- Objects: ICN by the tile part's icnType/icnIndex; layer order 3→1→2→0;
  bottom parts first, then top parts; tall objects (taller than 2 tiles) last.
  Source: `gui/interface_gamearea.cpp` (Redraw), `maps/maps_tiles_render.cpp`.
- Units: heroes — ICN::UNIFORM + shadow, monsters — ICN::MONS32, boats —
  TILEMAN/BOAT32, castle flags — OBJNFLAG, mine ghosts.
- Fog: `_fogColors` → direction computation by neighbours
  (Maps::updateFogDirectionsInArea); full fog — CLOF32 (index `(x+y)%4`),
  border — STON indices by direction.
- Animated objects — static frame 0.

### Castle rendering (port of Castle Screen)

- castle_building.cpp: RedrawBuildingSprite3D — CASTBKNG (background) + buildings
  by race (CASTL_*, TWN*EXT* additions, captain's quarters, etc.) per
  constructedBuildings; garrison and portraits — STRIP frames
  (`castle_dialog.cpp`), hero portraits — already known to the editor.

### Architecture

- Resources: the AGG/ICN/TIL/KB.PAL decoder exists in fheroes2-save-editor
  (`src/aggicn.cpp`), pixel-verified against the engine.
- Parsing: extend `fh2core` (`src/savefile.cpp`) with the Tile/Castle/Kingdom/
  EventDate structures and the per-tile offset table (width×height×4 bytes).
- CLI: `homm2-screenshot <save> [--out f.png] [--fog auto|color:N|none]
  [--scale 1|2] [--dpi 300] [--no-castles] [--no-heroes]`.
- Poster: title (map name, date, difficulty, victory conditions) + map + castle
  panel + hero panel + info blocks (resources, events, rumors, puzzle,
  minimap, records).

### Risks

- Edge cases of object layer order — verify against game screenshots.
- XL maps ×2: QImage 9216×9216 ≈ 340 MB — render tile by tile.
- Names in the save are CP1251 — use fheroes2 translations for captions.
