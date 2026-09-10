#pragma once

#include <QByteArray>
#include <QString>

#include "layout_types.h"

namespace fh2poster {

// Parses a poster layout from JSON. The schema mirrors layout::LayoutParams:
//
// {
//   "name": "my-layout",
//   "frame": 60,               // outer canvas margin, px
//   "sideMinFrac": 0.078,      // min side zone width as a fraction of the map
//   "sideWidth": 0,            // fixed right-side width, px (0 = auto)
//   "zones": [
//     { "id": "title", "side": "top", "single": true, "singleAspect": 0.045 },
//     { "id": "castles", "side": "bottom",
//       "grid": { "capW": 1280, "floorW": 480, "aspect": 0.75, "maxRows": 2,
//                 "maxZoneFrac": 0.20, "gap": 50, "padding": 20 } }
//   ]
// }
//
// "side" is "top" | "bottom" | "left" | "right". For "single" zones the grid
// block is optional; for grid zones "single" is optional (defaults to false).
//
// Returns false and fills *error on a malformed schema.
bool layoutFromJson( const QByteArray & json, layout::LayoutParams & out, QString * error = nullptr );

// Serializes a layout into the JSON schema above (a template for edits).
QByteArray layoutToJson( const layout::LayoutParams & params );

} // namespace fh2poster
