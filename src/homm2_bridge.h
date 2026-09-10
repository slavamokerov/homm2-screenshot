#pragma once

// Bridge to the homm2-to-fheroes2 core (h2core): lets the poster be generated
// directly from an ORIGINAL Heroes of Might and Magic II save (.GM1/.GMC/.GXC).
//
// Deliberately does NOT include any h2core header here: h2core defines its own
// (h2conv) world writer model, which would collide with the parsed fh2::WorldData
// used by fh2core/fh2poster. The h2core includes live only in homm2_bridge.cpp.

#include <cstdint>
#include <string>
#include <vector>

namespace homm2 {

// True if the file looks like an original HoMM2 save (by extension, or by the
// FF FF FF FF expansion-marker signature). Not a substitute for a real parse:
// main.cpp also falls back to this when the fheroes2 parser rejects the file.
bool isHomm2Save( const std::string & path );

// True if the in-memory bytes look like a HoMM2 save (expansion marker / LE map
// width). Used by the WASM entry.
bool isHomm2SaveBytes( const std::vector<uint8_t> & data );

// Parses an original HoMM2 save from a file and converts it into fheroes2 .sav
// bytes (parseSave -> convert -> buildSaveFile). Returns false on failure.
bool loadHomm2SaveToSavBytes( const std::string & srcPath, std::vector<uint8_t> & savBytes, std::string & error );

// Same, but the source save comes from memory (WASM path).
bool loadHomm2SaveToSavBytesFromBytes( const std::vector<uint8_t> & data, std::vector<uint8_t> & savBytes, std::string & error );

} // namespace homm2
