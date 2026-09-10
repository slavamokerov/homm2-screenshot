#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "layout_engine.h"
#include "layout_presets.h"

namespace {

int failures = 0;

#define CHECK( cond, msg )                                                                                                        \
    do {                                                                                                                          \
        if ( !( cond ) ) {                                                                                                        \
            std::printf( "FAIL: %s\n", msg );                                                                                      \
            ++failures;                                                                                                           \
        }                                                                                                                         \
    } while ( 0 )

void checkInvariants( const layout::LayoutResult & res, const std::string & name )
{
    const layout::Rect canvas = res.canvas();
    CHECK( res.canvasW > 0 && res.canvasH > 0, ( name + ": canvas is positive" ).c_str() );
    CHECK( res.map.inside( canvas ), ( name + ": map inside canvas" ).c_str() );

    std::vector<layout::Rect> all;
    all.push_back( res.map );
    for ( const auto & [id, rects] : res.blocks ) {
        for ( size_t i = 0; i < rects.size(); ++i ) {
            CHECK( rects[i].inside( canvas ), ( name + ": block " + id + " #" + std::to_string( i ) + " inside canvas" ).c_str() );
            for ( size_t j = i + 1; j < rects.size(); ++j )
                CHECK( !rects[i].intersects( rects[j] ), ( name + ": block " + id + " overlaps itself" ).c_str() );
            all.push_back( rects[i] );
        }
    }
    for ( size_t i = 0; i < all.size(); ++i )
        for ( size_t j = i + 1; j < all.size(); ++j )
            CHECK( !all[i].intersects( all[j] ), ( name + ": blocks overlap" ).c_str() );
}

} // namespace

int main()
{
    const std::map<std::string, int> counts12 = { { "castles", 12 }, { "heroes", 8 }, { "chips", 6 } };
    const std::map<std::string, int> counts2 = { { "castles", 2 }, { "heroes", 2 }, { "chips", 6 } };

    // XL (144 tiles) at scale 2 -> map 9216 px.
    {
        const layout::LayoutResult res = layout::computeLayout( layout::makePreset( "cardushe", "map" ), 9216, counts12 );
        checkInvariants( res, "cardushe/map XL" );
        CHECK( res.blocks.at( "castles" ).size() == 12, "cardushe/map XL: 12 castles" );
        CHECK( res.blocks.at( "heroes" ).size() == 8, "cardushe/map XL: 8 heroes" );
        CHECK( res.blocks.at( "chips" ).size() == 6, "cardushe/map XL: 6 chips" );
        CHECK( res.blocks.at( "title" ).size() == 1, "cardushe/map XL: title" );
        // Cards are constant (relative to the monster): castle 266x196, hero 360x210.
        CHECK( res.blocks.at( "castles" )[0].w == 266, "cardushe/map XL: castle width 266" );
        CHECK( res.blocks.at( "castles" )[0].h == 196, "cardushe/map XL: castle height 196" );
        CHECK( res.blocks.at( "heroes" )[0].w == 360, "cardushe/map XL: hero width 360" );
    }

    // Legend priority: the constant-size cards do not change with priority.
    {
        const layout::LayoutResult res = layout::computeLayout( layout::makePreset( "cardushe", "legend" ), 9216, counts12 );
        checkInvariants( res, "cardushe/legend XL" );
        CHECK( res.blocks.at( "castles" )[0].w == 266, "cardushe/legend XL: castle width 266" );
        CHECK( res.blocks.at( "castles" )[0].h == 196, "cardushe/legend XL: castle height 196" );
    }

    // Few castles: the card keeps its size (not stretched over the whole line).
    {
        const layout::LayoutResult res = layout::computeLayout( layout::makePreset( "cardushe", "map" ), 9216, counts2 );
        checkInvariants( res, "cardushe/map XL 2 castles" );
        CHECK( res.blocks.at( "castles" ).size() == 2, "cardushe XL 2 castles: count" );
        CHECK( res.blocks.at( "castles" )[0].w == 266, "cardushe XL 2 castles: constant 266" );
    }

    // Medium map (72 tiles) at scale 2 -> 4608 px: cards stay constant (not scaled).
    {
        const layout::LayoutResult res = layout::computeLayout( layout::makePreset( "cardushe", "map" ), 4608, counts12 );
        checkInvariants( res, "cardushe/map M" );
        CHECK( res.blocks.at( "castles" )[0].w == 266, "cardushe/map M: castle width 266" );
    }

    // Disabled castles: the zone collapses and the map takes the space.
    {
        const std::map<std::string, int> countsNoCastles = { { "heroes", 8 }, { "chips", 6 } };
        const layout::LayoutResult full = layout::computeLayout( layout::makePreset( "cardushe", "map" ), 9216, counts12 );
        const layout::LayoutResult slim = layout::computeLayout( layout::makePreset( "cardushe", "map" ), 9216, countsNoCastles );
        checkInvariants( slim, "cardushe/map XL no castles" );
        CHECK( slim.blocks.count( "castles" ) == 0, "no-castles: zone removed" );
        CHECK( slim.canvasH < full.canvasH, "no-castles: canvas shrinks" );
    }

    // The in-game cartouche layout: map + chips + kingdom groups.
    {
        layout::CartoucheInput in;
        in.mapPx = 4608;
        in.frame = 30;
        in.hasTitle = true;
        in.chipSizes = { { 300, 300 }, { 132, 224 }, { 258, 150 }, { 220, 176 }, { 300, 280 }, { 258, 130 } };
        in.groups = { { 0x04, 4, 3 }, { 0x02, 2, 1 } };
        const layout::LayoutResult res = layout::computeCartoucheLayout( in );
        checkInvariants( res, "cartouche" );
        CHECK( res.map.w == 4608, "cartouche: map width" );
        CHECK( res.blocks.at( "title" ).size() == 1, "cartouche: title" );
        CHECK( res.blocks.at( "chips" ).size() == 6, "cartouche: 6 chips" );
        CHECK( res.blocks.at( "heroes" ).size() == 6, "cartouche: 6 heroes" );
        CHECK( res.blocks.at( "castles" ).size() == 4, "cartouche: 4 castles" );
        CHECK( res.groups.size() == 2, "cartouche: 2 kingdom groups" );
        // Heroes come before castles inside a group; hero cards keep their size.
        CHECK( res.blocks.at( "heroes" )[0].w == 360, "cartouche: hero width" );
        CHECK( res.blocks.at( "castles" )[0].w == 266, "cartouche: castle width" );
    }

    if ( failures == 0 ) {
        std::printf( "all layout tests passed\n" );
        return 0;
    }
    std::printf( "%d layout test(s) failed\n", failures );
    return 1;
}
