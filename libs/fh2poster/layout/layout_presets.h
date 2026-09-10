#pragma once

#include <string>

#include "layout_types.h"

namespace layout {

// Built-in presets. priority affects the "cardushe" preset:
// "map" — thinner legend zones (the map dominates),
// "legend" — thicker zones, elements up to the cap.
LayoutParams makePreset( const std::string & name, const std::string & priority = "map" );

} // namespace layout
