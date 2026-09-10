// WebAssembly (Emscripten/embind) entry for the browser poster generator.
//
// This is the Qt-free web entry of homm2-screenshot: it renders against the
// rastercompat mini-Qt (NOT real Qt) so it builds with plain em++. A save comes
// in as bytes, the two game archives (HEROES2.AGG + HEROES2X.AGG, picked by the
// user from their computer) as bytes, and the PNG poster goes out as bytes.
//
// Comments are in English (project convention).

#include <algorithm>
#include <cstdint>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <zlib.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonArray>

#include "assets.h"
#include "castlerender.h"
#include "herocard.h"
#include "homm2_bridge.h"
#include "infochips.h"
#include "palette_cut.h"
#include "poster.h"
#include "savefile.h"

namespace {

std::vector<uint8_t> g_lastPng;

// Retained parse state so renderLegendCard() (browser drag previews) can draw a
// single legend card without re-parsing the save / reloading the AGG archives.
fh2::Assets g_chipAssets;
fh2::SaveFile g_chipSave;
fh2::WorldData g_chipWorld;
int g_chipColor = 0;
bool g_chipReady = false;
std::vector<uint8_t> g_chipPng;

// Inserts a PNG pHYs chunk (physical pixel size) right after IHDR. The native
// CLI sets it through QImage::setDotsPerMeter; the mini-Qt toPng() does not, so
// without it printers assume 72/96 DPI and the printed size would be wrong.
static std::vector<uint8_t> withPngDpi( std::vector<uint8_t> png, int dpi )
{
    if ( png.size() < 33 || png[0] != 0x89 || png[1] != 'P' || png[2] != 'N' || png[3] != 'G' )
        return png;
    const uint32_t ppm = static_cast<uint32_t>( dpi * 100.0 / 2.54 + 0.5 );
    std::vector<uint8_t> chunk;
    const auto push32 = [&chunk]( uint32_t v ) {
        chunk.push_back( static_cast<uint8_t>( v >> 24 ) );
        chunk.push_back( static_cast<uint8_t>( v >> 16 ) );
        chunk.push_back( static_cast<uint8_t>( v >> 8 ) );
        chunk.push_back( static_cast<uint8_t>( v ) );
    };
    push32( 9 ); // data length
    chunk.push_back( 'p' );
    chunk.push_back( 'H' );
    chunk.push_back( 'Y' );
    chunk.push_back( 's' );
    push32( ppm );
    push32( ppm );
    chunk.push_back( 1 ); // unit: metre
    push32( static_cast<uint32_t>( crc32( 0, chunk.data() + 4, 13 ) ) );

    std::vector<uint8_t> out;
    out.reserve( png.size() + chunk.size() );
    out.insert( out.end(), png.begin(), png.begin() + 33 );
    out.insert( out.end(), chunk.begin(), chunk.end() );
    out.insert( out.end(), png.begin() + 33, png.end() );
    return out;
}

// Encodes the poster as an indexed PNG under the rastercompat mini-Qt: builds a
// median-cut palette and an Indexed8 image, then lets mini-Qt's toPng() emit the
// indexed PNG (PLTE + tRNS). One byte per pixel instead of four.
static std::vector<uint8_t> quantizePosterPng( const QImage & poster )
{
    std::unordered_map<QRgb, int> hist;
    const std::vector<QRgb> pal = fh2poster::buildQuantPalette( poster, 256, hist );

    QImage indexed( poster.width(), poster.height(), QImage::Format_Indexed8 );
    indexed.setColorTable( pal );

    std::unordered_map<QRgb, int> cache;
    cache.reserve( hist.size() );
    const int srcBytes = poster.bytesPerLine();
    for ( int y = 0; y < poster.height(); ++y ) {
        const QRgb * row = reinterpret_cast<const QRgb *>( poster.constBits() + static_cast<std::size_t>( y ) * srcBytes );
        for ( int x = 0; x < poster.width(); ++x ) {
            const QRgb key = row[x];
            const auto it = cache.find( key );
            if ( it != cache.end() )
                indexed.setPixel( x, y, static_cast<uint32_t>( it->second ) );
            else {
                const int idx = fh2poster::nearestPaletteIndex( pal, key );
                cache.emplace( key, idx );
                indexed.setPixel( x, y, static_cast<uint32_t>( idx ) );
            }
        }
    }
    return indexed.toPng();
}

std::vector<uint8_t> valToBytes( const emscripten::val & v )
{
    const unsigned len = v["byteLength"].as<unsigned>();
    std::vector<uint8_t> out( len );
    emscripten::val view = emscripten::val( emscripten::typed_memory_view( len, out.data() ) );
    view.call<void>( "set", v );
    return out;
}

emscripten::val bytesToVal( const std::vector<uint8_t> & bytes )
{
    return emscripten::val( emscripten::typed_memory_view( bytes.size(),
                                                            const_cast<uint8_t *>( bytes.data() ) ) );
}

std::string esc( const std::string & s )
{
    std::string o = "\"";
    for ( unsigned char c : s ) {
        switch ( c ) {
        case '"': o += "\\\""; break;
        case '\\': o += "\\\\"; break;
        case '\n': o += "\\n"; break;
        case '\r': o += "\\r"; break;
        case '\t': o += "\\t"; break;
        default:
            if ( c < 0x20 || c >= 0x80 ) {
                char b[8];
                std::snprintf( b, sizeof b, "\\u%04x", c );
                o += b;
            }
            else
                o += static_cast<char>( c );
            break;
        }
    }
    o += '"';
    return o;
}

uint32_t toNum( const QJsonObject & o, const char * key, uint32_t def )
{
    return o.contains( key ) ? ( uint32_t )o.value( key ).toDouble( def ) : def;
}

std::string toStr( const QJsonObject & o, const char * key, const char * def )
{
    return o.contains( key ) ? o.value( key ).toString( QString::fromAscii( def ) ).toStdString() : def;
}

// Maps the --fog-* style options into the poster's selectedColor (PlayerColor).
int resolveFogColor( const std::string & fog, const fh2::SaveFile & save, const fh2::WorldData & world )
{
    if ( fog == "none" )
        return 0;
    if ( fog.rfind( "color:", 0 ) == 0 ) {
        int c = 0;
        try { c = std::stoi( fog.substr( 6 ) ); } catch ( ... ) {}
        return c;
    }
    // auto: the first human color of the save; matches the CLI (0 = no fog).
    // HoMM2-converted saves don't carry a parsed player list (findPlayers is
    // fheroes2-only), so fall back to a castle / hero color, then red.
    const std::vector<int> hc = save.humanColors();
    if ( !hc.empty() )
        return hc[0];
    if ( !world.castles.empty() && world.castles[0].color != 0 )
        return world.castles[0].color;
    if ( !world.heroes.empty() && world.heroes[0].color != 0 )
        return world.heroes[0].color;
    return 1;
}

// Lists the player colors that actually exist in a save (have a castle or a
// hero) so the web UI can offer only meaningful fog-of-war choices.
std::string listKingdoms( const emscripten::val & saveBytes )
{
    try {
        const std::vector<uint8_t> saveB = valToBytes( saveBytes );

        // An original HoMM2 save -> convert to an fheroes2 .sav in memory.
        std::vector<uint8_t> savB;
        if ( homm2::isHomm2SaveBytes( saveB ) ) {
            std::string err;
            if ( !homm2::loadHomm2SaveToSavBytesFromBytes( saveB, savB, err ) )
                return "{\"ok\":false,\"error\":" + esc( err ) + "}";
        }
        else {
            savB = saveB;
        }

        fh2::SaveFile save = fh2::SaveFile::loadFromBytes( "save", savB );
        const fh2::WorldData world = save.parseWorld();

        std::vector<int> colors;
        const auto add = [&colors]( int c ) {
            if ( c != 0 && std::find( colors.begin(), colors.end(), c ) == colors.end() )
                colors.push_back( c );
        };
        for ( const fh2::WorldCastle & c : world.castles )
            add( c.color );
        for ( const fh2::WorldHero & h : world.heroes )
            add( h.color );
        std::sort( colors.begin(), colors.end() );

        const auto label = []( int c ) -> const char * {
            switch ( c ) {
            case 0x01: return "Blue";
            case 0x02: return "Green";
            case 0x04: return "Red";
            case 0x08: return "Yellow";
            case 0x10: return "Orange";
            case 0x20: return "Purple";
            default: return "Kingdom";
            }
        };

        std::string out = "{\"ok\":true,\"colors\":[";
        for ( size_t i = 0; i < colors.size(); ++i ) {
            if ( i )
                out += ",";
            out += "{\"color\":" + std::to_string( colors[i] ) + ",\"label\":\"" + label( colors[i] ) + "\"}";
        }
        out += "]}";
        return out;
    }
    catch ( const std::exception & e ) {
        return "{\"ok\":false,\"error\":" + esc( e.what() ) + "}";
    }
    catch ( ... ) {
        return "{\"ok\":false,\"error\":\"Unknown error.\"}";
    }
}

std::string renderPoster( const emscripten::val & saveBytes, const emscripten::val & aggBytes,
                          const emscripten::val & aggXBytes, const std::string & optionsJson )
{
    try {
        const std::vector<uint8_t> saveB = valToBytes( saveBytes );
        const std::vector<uint8_t> aggB = valToBytes( aggBytes );
        const std::vector<uint8_t> aggXB = valToBytes( aggXBytes );

        // Load the game resources first, then the save (so a missing/mismatched
        // AGG surfaces before touching the save).
        fh2::Assets assets = fh2::Assets::loadFromBytes( aggB, aggXB );
        if ( !assets.valid() )
            return "{\"ok\":false,\"error\":\"Failed to read HEROES2.AGG / HEROES2X.AGG.\"}";

        // An original HoMM2 save -> convert to an fheroes2 .sav in memory.
        std::vector<uint8_t> savB;
        if ( homm2::isHomm2SaveBytes( saveB ) ) {
            std::string err;
            if ( !homm2::loadHomm2SaveToSavBytesFromBytes( saveB, savB, err ) )
                return "{\"ok\":false,\"error\":" + esc( err ) + "}";
        }
        else {
            savB = saveB;
        }

        fh2::SaveFile save = fh2::SaveFile::loadFromBytes( "save", savB );
        fh2::WorldData world = save.parseWorld();

        fh2poster::PosterOptions opts;
        QJsonDocument doc = QJsonDocument::fromJson( QByteArray( optionsJson ) );
        if ( doc.isObject() ) {
            const QJsonObject o = doc.object();
            opts.layoutName = toStr( o, "layout", "cardushe" );
            opts.layoutPriority = toStr( o, "layoutPriority", "map" );
            opts.scale = toNum( o, "scale", 2 );
            opts.blocks = toStr( o, "blocks", "" );
            opts.chips = toStr( o, "chips", "" );
            opts.routes = ( toStr( o, "routes", "all" ) == "none" ) ? fh2poster::RouteMode::None
                        : ( toStr( o, "routes", "all" ) == "player" ) ? fh2poster::RouteMode::Player
                        : ( toStr( o, "routes", "all" ) == "visible" ) ? fh2poster::RouteMode::Visible
                                                                        : fh2poster::RouteMode::All;
            const std::string fog = toStr( o, "fog", "auto" );
            opts.withFog = ( fog != "none" );
            opts.selectedColor = resolveFogColor( fog, save, world );
        }

        layout::LayoutResult lay;
        QString renderError;
        const QImage poster = fh2poster::renderPoster( save, world, assets, opts, &lay, &renderError );
        if ( poster.isNull() )
            return "{\"ok\":false,\"error\":" + esc( renderError.toStdString() ) + "}";

        // Keep the parsed state so a subsequent render of a single legend card is
        // not needed; the browser only shows the final poster (no drag).
        g_chipAssets = std::move( assets );
        g_chipSave = std::move( save );
        g_chipWorld = std::move( world );
        g_chipColor = opts.selectedColor;
        g_chipReady = true;

        g_lastPng = withPngDpi( quantizePosterPng( poster ), 300 );

        std::string info = "{\"ok\":true";
        info += ",\"name\":" + esc( g_chipSave.mapInfo().name );
        info += ",\"width\":" + std::to_string( g_chipWorld.width );
        info += ",\"height\":" + std::to_string( g_chipWorld.height );
        info += ",\"castles\":" + std::to_string( g_chipWorld.castles.size() );
        info += ",\"heroes\":" + std::to_string( g_chipWorld.heroes.size() );
        info += ",\"day\":" + std::to_string( g_chipWorld.day );
        info += ",\"week\":" + std::to_string( g_chipWorld.week );
        info += ",\"month\":" + std::to_string( g_chipWorld.month );
        info += ",\"png\":" + std::to_string( g_lastPng.size() );
        info += ",\"pxW\":" + std::to_string( poster.width() ) + ",\"pxH\":" + std::to_string( poster.height() );
        info += ",\"sel\":" + std::to_string( opts.selectedColor ) + ",\"scale\":" + std::to_string( ( int )opts.scale );

        // Map rect (for the drag canvas / snapping) in poster pixels.
        info += "}";
        return info;
    }
    catch ( const std::exception & e ) {
        return "{\"ok\":false,\"error\":" + esc( e.what() ) + "}";
    }
    catch ( ... ) {
        return "{\"ok\":false,\"error\":\"Unknown error.\"}";
    }
}

// Renders a single legend chip as a PNG (for diagnostics / small previews).
emscripten::val renderLegendCard( const std::string & kind, const std::string & chipId, int index, int w, int h )
{
    if ( !g_chipReady || w <= 0 || h <= 0 )
        return emscripten::val( emscripten::typed_memory_view( 0, ( const uint8_t * )nullptr ) );
    QImage img;
    if ( kind == "chip" ) {
        fh2poster::InfoChips chips( g_chipAssets );
        img = chips.renderChip( chipId, g_chipWorld, g_chipSave.mapInfo(), g_chipColor, w, h );
    }
    else if ( kind == "hero" && index >= 0 && index < static_cast<int>( g_chipWorld.heroes.size() ) ) {
        fh2poster::HeroCardRender hr( g_chipAssets );
        img = hr.render( g_chipWorld.heroes[index] );
    }
    else if ( kind == "castle" && index >= 0 && index < static_cast<int>( g_chipWorld.castles.size() ) ) {
        fh2poster::CastleRender cr( g_chipAssets );
        img = cr.render( g_chipWorld.castles[index] );
    }
    g_chipPng = quantizePosterPng( img );
    return bytesToVal( g_chipPng );
}

emscripten::val takeLastPng()
{
    return bytesToVal( g_lastPng );
}

} // namespace

EMSCRIPTEN_BINDINGS( fh2poster )
{
    emscripten::function( "renderPoster", &renderPoster );
    emscripten::function( "takeLastPng", &takeLastPng );
    emscripten::function( "renderLegendCard", &renderLegendCard );
    emscripten::function( "listKingdoms", &listKingdoms );
}
