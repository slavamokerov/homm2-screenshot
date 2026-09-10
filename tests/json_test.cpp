// JSON layout schema tests: roundtrip of the presets and malformed schemas.
#include <cstdio>
#include <string>

#include "layout_json.h"
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

bool gridEquals( const layout::GridParams & a, const layout::GridParams & b )
{
    return a.capW == b.capW && a.floorW == b.floorW && std::abs( a.aspect - b.aspect ) < 0.001f && a.maxRows == b.maxRows
           && std::abs( a.maxZoneFrac - b.maxZoneFrac ) < 0.001f && a.gap == b.gap && a.padding == b.padding;
}

bool paramsEqual( const layout::LayoutParams & a, const layout::LayoutParams & b )
{
    if ( a.frame != b.frame || std::abs( a.sideMinFrac - b.sideMinFrac ) > 0.001f || a.sideWidth != b.sideWidth || a.zones.size() != b.zones.size() )
        return false;
    for ( size_t i = 0; i < a.zones.size(); ++i ) {
        const layout::ZoneDef & za = a.zones[i];
        const layout::ZoneDef & zb = b.zones[i];
        if ( za.id != zb.id || za.side != zb.side || za.single != zb.single || std::abs( za.singleAspect - zb.singleAspect ) > 0.001f )
            return false;
        if ( !gridEquals( za.grid, zb.grid ) )
            return false;
    }
    return true;
}

void checkRoundtrip( const layout::LayoutParams & params )
{
    const QByteArray json = fh2poster::layoutToJson( params );
    layout::LayoutParams parsed;
    QString error;
    CHECK( fh2poster::layoutFromJson( json, parsed, &error ), ( params.name + ": parse" ).c_str() );
    CHECK( paramsEqual( params, parsed ), ( params.name + ": roundtrip equality" ).c_str() );
    if ( !error.isEmpty() )
        std::printf( "  (unexpected error: %s)\n", error.toUtf8().constData() );
}

void checkMalformed( const QByteArray & json, const char * name )
{
    layout::LayoutParams parsed;
    QString error;
    CHECK( !fh2poster::layoutFromJson( json, parsed, &error ), name );
}

} // namespace

int main()
{
    checkRoundtrip( layout::makePreset( "cardushe", "map" ) );
    checkRoundtrip( layout::makePreset( "cardushe", "legend" ) );

    // Malformed schemas must be rejected with an error.
    checkMalformed( R"({"zones": []})", "empty zones" );
    checkMalformed( R"({"zones": [{"id": "x"}]})", "missing side" );
    checkMalformed( R"({"zones": [{"id": "x", "side": "diagonal"}]})", "bad side" );
    checkMalformed( R"({"zones": [{"id": "x", "side": "top", "single": true}]})", "single without singleAspect" );
    checkMalformed( R"({"zones": [{"id": "x", "side": "top", "single": false}]})", "grid missing" );
    checkMalformed( R"({"zones": [{"id": "x", "side": "top", "grid": {"aspect": 0, "maxRows": 2}}]})", "zero aspect" );
    checkMalformed( R"({"zones": [{"id": "x", "side": "top", "grid": {"aspect": 0.5, "maxRows": 0}}]})", "zero maxRows" );
    checkMalformed( R"(not json)", "garbage" );

    if ( failures == 0 ) {
        std::printf( "all json layout tests passed\n" );
        return 0;
    }
    std::printf( "%d json layout test(s) failed\n", failures );
    return 1;
}
