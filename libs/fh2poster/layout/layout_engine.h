#pragma once

#include <map>
#include <string>

#include "layout_types.h"

namespace layout {

// Computes the poster geometry. mapPx — the rendered map size in pixels (a square).
// counts — how many elements each grid zone has (by zone id; "title" needs no count).
// Zones with count == 0 get zero thickness. The map always takes all the remaining space.
LayoutResult computeLayout( const LayoutParams & params, int mapPx, const std::map<std::string, int> & counts );

// The specialized in-game "cartouche" layout: a square map on top, a row of
// info chips, then kingdom groups by player color (hero cards → castle renders,
// which grow downward). See CartoucheInput.
LayoutResult computeCartoucheLayout( const CartoucheInput & input );

} // namespace layout
