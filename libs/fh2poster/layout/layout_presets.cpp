#include "layout_presets.h"

#include <utility>
#include <vector>

namespace layout {

namespace {

ZoneDef singleZone( const std::string & id, Side side, float aspect, int fixedH = 0 )
{
    ZoneDef z;
    z.id = id;
    z.side = side;
    z.single = true;
    z.singleAspect = aspect;
    z.singleFixedH = fixedH;
    return z;
}

ZoneDef gridZone( const std::string & id, Side side, int capW, int floorW, float aspect, int maxRows, float maxZoneFrac, int gap, int padding,
                  int fixedW = 0, int fixedH = 0, std::vector<int> fixedWList = {}, std::vector<int> fixedHList = {} )
{
    ZoneDef z;
    z.id = id;
    z.side = side;
    z.grid.capW = capW;
    z.grid.floorW = floorW;
    z.grid.aspect = aspect;
    z.grid.maxRows = maxRows;
    z.grid.maxZoneFrac = maxZoneFrac;
    z.grid.gap = gap;
    z.grid.padding = padding;
    z.grid.fixedW = fixedW;
    z.grid.fixedH = fixedH;
    z.grid.fixedWList = std::move( fixedWList );
    z.grid.fixedHList = std::move( fixedHList );
    return z;
}

// Castle cards: compact — the town scaled to the width of the 5-slot garrison
// row (266px), so several fit in a row. The monster (MONS32) is the reference.
ZoneDef castlesZone( Side side, bool priorityLegend )
{
    return gridZone( "castles", side, 1280, 480, 0.74f, priorityLegend ? 3 : 2, priorityLegend ? 0.5f : 0.20f, 6, 8, 266, 196 );
}

ZoneDef heroesZone( Side side )
{
    return gridZone( "heroes", side, 700, 280, 0.58f, 4, 0.0f, 6, 6, 360, 210 );
}

ZoneDef chipsZone( Side side )
{
    // All chips keep their own size (minimap wide, resources narrow, puzzle wide),
    // so the legend packs them tightly without wasted space. Order: minimap,
    // resources, calendar, rumors, puzzle, conditions (see chipNames[] in poster.cpp).
    return gridZone( "chips", side, 500, 0, 0.6f, 3, 0.0f, 6, 6, 300, 0,
                     { 300, 132, 258, 220, 280, 258 }, { 300, 230, 150, 176, 280, 130 } );
}

} // namespace

LayoutParams makePreset( const std::string & name, const std::string & priority )
{
    // The in-game "cartouche" layout is built by computeCartoucheLayout() directly
    // (see poster.cpp); this generic preset remains only for the JSON schema path
    // and the layout tests. banner / column presets were removed.
    const bool legendPriority = ( priority == "legend" );
    LayoutParams p;
    p.name = name;

    if ( name == "cardushe" ) {
        p.frame = 60;
        p.sideMinFrac = 0.078f;
        p.zones = {
            singleZone( "title", Side::TOP, 0.045f, 46 ),
            castlesZone( Side::BOTTOM, legendPriority ),
            heroesZone( Side::RIGHT ),
            chipsZone( Side::LEFT ),
        };
    }
    return p;
}

} // namespace layout
