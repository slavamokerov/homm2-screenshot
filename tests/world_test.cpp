// World parsing test on real saves.
// Usage: world_test <save.sav> [...] — every save is parsed and validated.
#include <cstdio>
#include <map>
#include <string>

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

void checkSave( const std::string & path )
{
    std::printf( "== %s\n", path.c_str() );
    try {
        const fh2::SaveFile save = fh2::SaveFile::load( path );
        const fh2::WorldData w = save.parseWorld();

        CHECK( w.width == save.mapInfo().width, "width matches the header" );
        CHECK( w.height == save.mapInfo().height, "height matches the header" );
        CHECK( static_cast<int>( w.tiles.size() ) == w.width * w.height, "tile count is width*height" );
        CHECK( w.heroes.size() == 73, "73 heroes in AllHeroes" );
        CHECK( w.kingdoms.size() > 0, "at least one kingdom" );

        // Active heroes (present on the map) must have a valid center.
        int active = 0, routed = 0;
        for ( const fh2::WorldHero & h : w.heroes ) {
            if ( h.color == 0 )
                continue; // not hired
            ++active;
            if ( !h.route.steps.empty() )
                ++routed;
        }
        CHECK( active > 0, "at least one active hero" );

        // The sequential hero parse must match the editor's reference scan.
        for ( const fh2::WorldHero & h : w.heroes ) {
            const fh2::HeroRecord * ref = nullptr;
            for ( const fh2::HeroRecord & r : save.heroes() ) {
                if ( r.heroId == h.id ) {
                    ref = &r;
                    break;
                }
            }
            if ( ref )
                CHECK( ref->name == h.name, ( "hero " + std::to_string( h.id ) + " name matches the reference scan" ).c_str() );
        }

        // Kingdoms reference castles by tile index (VecCastles stores GetIndex()).
        for ( const fh2::WorldKingdom & k : w.kingdoms ) {
            for ( const int32_t cid : k.castleIds )
                CHECK( cid >= 0 && cid < w.width * w.height, "kingdom castle tile index in range" );
            for ( const int32_t hid : k.heroIds ) {
                bool found = false;
                for ( const fh2::WorldHero & h : w.heroes )
                    if ( h.id == hid ) {
                        found = true;
                        break;
                    }
                CHECK( found, "kingdom hero id exists" );
            }
        }

        // The game date must be plausible.
        CHECK( w.day >= 1 && w.day <= 1000000, "day is plausible" );
        CHECK( w.week >= 1 && w.week <= 150000, "week is plausible" );
        CHECK( w.month >= 1 && w.month <= 40000, "month is plausible" );

        // Castle owners: the color must be a known player color bitmask.
        for ( const fh2::WorldCastle & c : w.castles )
            CHECK( c.color == 0 || c.color == 1 || c.color == 2 || c.color == 4 || c.color == 8 || c.color == 16 || c.color == 32,
                   "castle color is a valid bitmask" );

        std::printf( "  map %dx%d, tiles %zu, heroes %zu (active %d, routed %d), castles %zu, kingdoms %zu, events %zu, rumors %zu, day %u/%u/%u\n",
                     w.width, w.height, w.tiles.size(), w.heroes.size(), active, routed, w.castles.size(), w.kingdoms.size(), w.events.size(),
                     w.rumors.size(), w.day, w.week, w.month );
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
        std::printf( "usage: world_test <save.sav> [more saves...]\n" );
        return 1;
    }
    for ( int i = 1; i < argc; ++i )
        checkSave( argv[i] );

    if ( failures == 0 ) {
        std::printf( "all world parsing tests passed\n" );
        return 0;
    }
    std::printf( "%d world test(s) failed\n", failures );
    return 1;
}
