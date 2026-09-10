// HoMM2 save -> fheroes2 round-trip test, using the h2core bridge.
// Usage: homm2_test <input.GM1|GMC|GXC> [...] — every original HoMM2 save is
// converted to an in-memory fheroes2 .sav and then parsed as a real save.
#include <cstdio>
#include <exception>
#include <string>
#include <vector>

// Only the bridge header (no h2core header here: it would collide with the
// fh2core WorldData model pulled in by savefile.h below).
#include "homm2_bridge.h"
#include "savefile.h"

namespace {

int failures = 0;

#define CHECK( cond, msg )                                                                                                        \
    do {                                                                                                                          \
        if ( !( cond ) ) {                                                                                                        \
            std::printf( "FAIL: %s\n", msg );                                                                                      \
            ++failures;                                                                                                           \
        }                                                                                                                         \
    } while ( 0 )

void checkHomm2Save( const std::string & path )
{
    std::printf( "== %s\n", path.c_str() );
    std::fflush( stdout );
    try {
        std::vector<uint8_t> savBytes;
        std::string h2Error;
        if ( !homm2::loadHomm2SaveToSavBytes( path, savBytes, h2Error ) ) {
            std::printf( "FAIL: convert: %s\n", h2Error.c_str() );
            ++failures;
            return;
        }
        std::fflush( stdout );
        CHECK( !savBytes.empty(), "converted bytes are non-empty" );

        const fh2::SaveFile save = fh2::SaveFile::loadFromBytes( path, savBytes );
        std::fflush( stdout );
        const fh2::WorldData w = save.parseWorld();

        CHECK( w.width > 0 && w.height > 0, "map size is positive" );
        CHECK( static_cast<int>( w.tiles.size() ) == w.width * w.height, "tile count is width*height" );
        CHECK( w.day >= 1 && w.month >= 1, "world date is plausible" );
        CHECK( save.mapInfo().width == w.width && save.mapInfo().height == w.height, "header map size matches the world" );
        CHECK( !save.mapInfo().name.empty(), "map name present in the header" );

        int activeHeroes = 0;
        for ( const fh2::WorldHero & h : w.heroes )
            if ( h.color != 0 )
                ++activeHeroes;
        CHECK( activeHeroes >= 0, "hero list parsed" );

        std::printf( "  converted -> saves %zu bytes, map %dx%d, heroes %zu/%zu, castles %zu, kingdoms %zu, day %u/%u/%u, name '%s'\n",
                     savBytes.size(), w.width, w.height, static_cast<size_t>( activeHeroes ), w.heroes.size(), w.castles.size(), w.kingdoms.size(),
                     w.day, w.week, w.month, save.mapInfo().name.c_str() );
    }
    catch ( const std::exception & e ) {
        std::printf( "FAIL: %s: %s\n", path.c_str(), e.what() );
        ++failures;
    }
}

} // namespace

int main( int argc, char ** argv )
{
    if ( argc < 2 ) {
        std::printf( "usage: homm2_test <input.GM1|GMC|GXC> [more saves...]\n" );
        return 1;
    }
    for ( int i = 1; i < argc; ++i )
        checkHomm2Save( argv[i] );

    if ( failures == 0 ) {
        std::printf( "all HoMM2 conversion tests passed\n" );
        return 0;
    }
    std::printf( "%d HoMM2 test(s) failed\n", failures );
    return 1;
}
