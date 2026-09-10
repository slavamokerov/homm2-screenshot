#include <cstdio>
#include <cstdlib>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <vector>

#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>

#include "args.h"
#include "assets.h"
#include "castlerender.h"
#include "herocard.h"
#include "homm2_bridge.h"
#include "infochips.h"
#include "layout_engine.h"
#include "layout_json.h"
#include "layout_presets.h"
#include "maprender.h"
#include "poster.h"
#include "poster_quantize.h"
#include "savefile.h"

// The shared core must not be older than the code in this project expects.
// If this fails, run scripts/sync-core.sh (or update the live editor checkout).
static_assert( fh2::FH2CORE_VERSION >= 1, "fh2core is out of date; run scripts/sync-core.sh" );

namespace {

const char * usageText = R"(homm2-screenshot — poster generator from a Heroes of Might and Magic II save

Usage: homm2-screenshot <save> [options]

Input: an ORIGINAL Heroes of Might and Magic II save (.GM1/.GMC/.GXC) or an
       fheroes2 save (.sav/.savc/.savm/.savh) — HoMM2 saves are converted in
       memory into an fheroes2 save before rendering.

Options:
  --out <f.png>              output file (default: poster.png)
  --fog auto|color:N|none    fog of war mode (default: auto)
  --scale 1|1.5|2            map scale multiplier (default: 2)
  --dpi <n>                  output DPI for PNG metadata (default: 300)
  --layout <name|file.json>  layout: "cardushe" (in-game frame + kingdom groups)
                             or a path to a JSON layout (default: cardushe)
  --layout-priority map|legend
                             cardushe zone thickness: map (default) | legend
                             (only used for JSON layouts)
  --blocks <list>            enabled blocks, e.g. "map,castles,heroes,chips,title"
  --chips <list>             enabled info chips, e.g. "minimap,resources,calendar,rumors,puzzle,victory,records"
  --routes none|player|visible|all
                             hero route arrows (default: all — including AI intentions)
  --castles <n>              override the castle count shown (default: from save)
  --heroes <n>               override the hero count shown (default: from save)
  --no-castles               alias for --castles 0
  --no-heroes                alias for --heroes 0
  --data-dir <folder>        folder with HEROES2.AGG (default: auto-detect)
  -h, --help                 this help
)";

int parseInt( const std::string & s, int def )
{
    if ( s.empty() )
        return def;
    try {
        return std::stoi( s );
    }
    catch ( ... ) {
        return def;
    }
}

} // namespace

int main( int argc, char ** argv )
{
    // Headless CLI: force the offscreen QPA platform before the app is created.
#ifdef _WIN32
    _putenv_s( "QT_QPA_PLATFORM", "offscreen" );
#else
    setenv( "QT_QPA_PLATFORM", "offscreen", 1 );
#endif
    QGuiApplication app( argc, argv );

    const fh2poster::Args args = fh2poster::parseArgs( argc, argv );

    if ( args.showHelp ) {
        std::printf( "%s", usageText );
        return 0;
    }
    if ( !args.error.empty() ) {
        std::fprintf( stderr, "error: %s\n\n%s", args.error.c_str(), usageText );
        return 1;
    }

    // Parse the save (fh2core from fheroes2_save_editor). An ORIGINAL HoMM2
    // save (.GM1/.GMC/.GXC) is converted in memory into an fheroes2 .sav by
    // the h2core bridge, then parsed with the same code path.
    fh2::SaveFile save;
    bool fromHomm2 = false;
    if ( homm2::isHomm2Save( args.savePath ) ) {
        std::vector<uint8_t> savBytes;
        std::string h2Error;
        if ( !homm2::loadHomm2SaveToSavBytes( args.savePath, savBytes, h2Error ) ) {
            std::fprintf( stderr, "error: %s\n", h2Error.c_str() );
            return 1;
        }
        save = fh2::SaveFile::loadFromBytes( args.savePath, savBytes );
        fromHomm2 = true;
    }
    else {
        try {
            save = fh2::SaveFile::load( args.savePath );
        }
        catch ( const std::exception & e ) {
            // Not an fheroes2 save — maybe an original HoMM2 save under an
            // unexpected name; fall back to the converter.
            std::vector<uint8_t> savBytes;
            std::string h2Error;
            if ( !homm2::loadHomm2SaveToSavBytes( args.savePath, savBytes, h2Error ) ) {
                std::fprintf( stderr, "error: %s\n", e.what() );
                return 1;
            }
            save = fh2::SaveFile::loadFromBytes( args.savePath, savBytes );
            fromHomm2 = true;
        }
    }
    if ( fromHomm2 )
        std::printf( "note: original HoMM2 save loaded (converted to an fheroes2 save in memory)\n" );

    const fh2::WorldData world = save.parseWorld();
    const int tiles = world.width;
    if ( tiles <= 0 ) {
        std::fprintf( stderr, "error: invalid map size in the save\n" );
        return 1;
    }

    if ( args.debugMap ) {
        const int w = world.width, h = world.height;
        auto printParts = [&]( const std::vector<fh2::ObjectPart> & parts ) {
            for ( const fh2::ObjectPart & p : parts ) {
                if ( p.icnType != 0 && p.icnIndex != 255 )
                    std::fprintf( stderr, "   g[L%d t%u i%u uid%u]\n", p.layerType, p.icnType, p.icnIndex, p.uid );
                else
                    std::fprintf( stderr, "   g[L%d t%u i%u]\n", p.layerType, p.icnType, p.icnIndex );
            }
        };
        for ( int y = 0; y < h; ++y ) {
            for ( int x = 0; x < w; ++x ) {
                const fh2::WorldTile & t = world.tiles[static_cast<size_t>( y ) * w + x];
                const bool water = t.terrainImageIndex < 30;
                const int obj = t.mainObjectType;
                // Show water tiles and obelisks (OBJ_OBELISK=153, plus their snow
                // 139/140/141 and grass 127/128/129 parts) for diagnosing objects.
                auto isObeliskPart = [&]( const fh2::ObjectPart & p ) {
                    if ( p.icnType == 52 && p.icnIndex >= 139 && p.icnIndex <= 141 )
                        return true;
                    if ( p.icnType == 48 && p.icnIndex >= 127 && p.icnIndex <= 129 )
                        return true;
                    return false;
                };
                const bool obelisk = obj == 153 || isObeliskPart( t.mainPart );
                if ( !water && !obelisk )
                    continue;
                if ( water ) {
                    std::fprintf( stderr, "WATER (%d,%d) terr=%u objType=%u main[L%d t%u i%u]",
                                  x, y, t.terrainImageIndex, obj, t.mainPart.layerType, t.mainPart.icnType, t.mainPart.icnIndex );
                    printParts( t.groundParts );
                    std::fprintf( stderr, "   top:\n" );
                    printParts( t.topParts );
                }
                else {
                    std::fprintf( stderr, "OBELISK (%d,%d) terr=%u objType=%u main[L%d t%u i%u]",
                                  x, y, t.terrainImageIndex, obj, t.mainPart.layerType, t.mainPart.icnType, t.mainPart.icnIndex );
                    printParts( t.groundParts );
                    std::fprintf( stderr, "   top:\n" );
                    printParts( t.topParts );
                }
            }
        }
    }
    const int mapPx = static_cast<int>( tiles * 32 * args.scale );

    // The player whose fog of war / legend is used.
    int selectedColor = 0;
    if ( args.fog != "none" ) {
        if ( args.fog == "auto" ) {
            const std::vector<int> hc = save.humanColors();
            if ( !hc.empty() ) {
                selectedColor = hc[0];
            }
            else if ( !world.castles.empty() && world.castles[0].color != 0 ) {
                selectedColor = world.castles[0].color;
            }
            else if ( !world.heroes.empty() && world.heroes[0].color != 0 ) {
                selectedColor = world.heroes[0].color;
            }
            else {
                selectedColor = 1; // Red is the default human in a skirmish.
            }
        }
        else if ( args.fog.rfind( "color:", 0 ) == 0 ) {
            selectedColor = parseInt( args.fog.substr( 6 ), 0 );
        }
    }

    // Load the game resources.
    const std::string dataDir = args.dataDir.empty() ? fh2::Assets::defaultDataDir() : args.dataDir;
    if ( dataDir.empty() || !QFileInfo::exists( QString::fromStdString( dataDir ) + "/HEROES2.AGG" ) ) {
        std::fprintf( stderr, "error: game data not found (set --data-dir to the folder with HEROES2.AGG)\n" );
        return 1;
    }

    fh2::Assets assets = fh2::Assets::load( dataDir );
    if ( !assets.valid() ) {
        std::fprintf( stderr, "error: failed to load game resources from %s\n", dataDir.c_str() );
        return 1;
    }

    fh2poster::RouteMode routeMode = fh2poster::RouteMode::All;
    if ( args.routes == "none" )
        routeMode = fh2poster::RouteMode::None;
    else if ( args.routes == "player" )
        routeMode = fh2poster::RouteMode::Player;
    else if ( args.routes == "visible" )
        routeMode = fh2poster::RouteMode::Visible;

    // Diagnostics shortcuts (dev tools).
    if ( args.crop[2] > 0 && args.crop[3] > 0 ) {
        const QImage mapImage = fh2poster::renderMap( world, assets, mapPx, selectedColor, args.fog != "none", routeMode );
        const double scale = static_cast<double>( mapPx ) / ( tiles * 32 );
        const QRect r( static_cast<int>( args.crop[0] * 32 * scale ), static_cast<int>( args.crop[1] * 32 * scale ),
                       static_cast<int>( args.crop[2] * 32 * scale ), static_cast<int>( args.crop[3] * 32 * scale ) );
        const QImage cropped = mapImage.copy( r );
        if ( !cropped.save( QString::fromStdString( args.out ) ) ) {
            std::fprintf( stderr, "error: failed to write %s\n", args.out.c_str() );
            return 1;
        }
        std::printf( "saved crop %s (%dx%d px)\n", args.out.c_str(), cropped.width(), cropped.height() );
        return 0;
    }

    if ( !args.icnDump.empty() ) {
        const size_t colon = args.icnDump.rfind( ':' );
        const std::string name = args.icnDump.substr( 0, colon );
        const int idx = parseInt( args.icnDump.substr( colon + 1 ), 0 );
        const fh2::IcnSprite & sp = assets.icnSprite( name, idx );
        if ( sp.isNull() ) {
            std::fprintf( stderr, "sprite %s[%d] not found\n", name.c_str(), idx );
            return 1;
        }
        if ( !sp.image.save( QString::fromStdString( args.out ) ) ) {
            std::fprintf( stderr, "error: failed to write %s\n", args.out.c_str() );
            return 1;
        }
        std::printf( "saved sprite %s[%d] %dx%d offset (%d,%d)\n", name.c_str(), idx, sp.image.width(), sp.image.height(), sp.offsetX, sp.offsetY );
        return 0;
    }

    if ( !args.icnSheet.empty() ) {
        // "name.icn:from:to:cols" — a labeled sprite sheet (dev tool).
        int from = 0, to = 0, cols = 8;
        std::sscanf( args.icnSheet.c_str(), "%*[^:]:%d:%d:%d", &from, &to, &cols );
        const std::string name = args.icnSheet.substr( 0, args.icnSheet.find( ':' ) );
        if ( cols <= 0 )
            cols = 8;
        if ( to < from )
            to = from;
        const int count = to - from + 1;
        const int rows = ( count + cols - 1 ) / cols;
        const int cellW = 96;
        const int cellH = 96;
        QImage sheet( cols * cellW, rows * cellH, QImage::Format_ARGB32 );
        sheet.fill( qRgba( 30, 30, 30, 255 ) );
        QPainter sp( &sheet );
        for ( int i = 0; i < count; ++i ) {
            const fh2::IcnSprite & s = assets.icnSprite( name, from + i );
            const int cx = ( i % cols ) * cellW;
            const int cy = ( i / cols ) * cellH;
            if ( !s.isNull() )
                if ( !s.isNull() ) {
                    QImage big = s.image.scaled( s.image.width() * 3, s.image.height() * 3, Qt::IgnoreAspectRatio, Qt::FastTransformation );
                    sp.drawImage( cx + ( cellW - big.width() ) / 2, cy + ( cellH - big.height() ) / 2, big );
                }
            sp.setPen( QColor( 90, 200, 120 ) );
            QFont f;
            f.setPixelSize( 10 );
            sp.setFont( f );
            sp.drawText( QRect( cx, cy, cellW, 12 ), Qt::AlignLeft, QString::number( from + i ) );
        }
        sp.end();
        if ( !sheet.save( QString::fromStdString( args.out ) ) ) {
            std::fprintf( stderr, "error: failed to write %s\n", args.out.c_str() );
            return 1;
        }
        std::printf( "saved sheet %s (%dx%d px)\n", args.out.c_str(), sheet.width(), sheet.height() );
        return 0;
    }

    if ( !args.dumpLayout.empty() ) {
        const layout::LayoutParams params = layout::makePreset( args.dumpLayout, args.layoutPriority );
        std::printf( "%s", fh2poster::layoutToJson( params ).constData() );
        return 0;
    }

    if ( !args.chipDump.empty() ) {
        fh2poster::InfoChips chips( assets );
        const int chipColor = ( args.chipDump == "resources" ) ? fh2poster::resourceColor( save, world, selectedColor ) : selectedColor;
        const QImage chip = chips.renderChip( args.chipDump, world, save.mapInfo(), chipColor, 500, 300 );
        if ( !chip.save( QString::fromStdString( args.out ) ) ) {
            std::fprintf( stderr, "error: failed to write %s\n", args.out.c_str() );
            return 1;
        }
        std::printf( "saved chip %s (%dx%d px)\n", args.out.c_str(), chip.width(), chip.height() );
        return 0;
    }

    if ( args.heroId >= 0 ) {
        fh2poster::HeroCardRender heroRender( assets );
        QImage card;
        for ( const fh2::WorldHero & h : world.heroes ) {
            if ( h.id == args.heroId ) {
                card = heroRender.render( h );
                break;
            }
        }
        if ( card.isNull() ) {
            std::fprintf( stderr, "error: hero id %d not found\n", args.heroId );
            return 1;
        }
        if ( !card.save( QString::fromStdString( args.out ) ) ) {
            std::fprintf( stderr, "error: failed to write %s\n", args.out.c_str() );
            return 1;
        }
        std::printf( "saved hero card %s (%dx%d px)\n", args.out.c_str(), card.width(), card.height() );
        return 0;
    }

    if ( args.castleIndex == -1 ) {
        for ( size_t i = 0; i < world.castles.size(); ++i ) {
            const fh2::WorldCastle & c = world.castles[i];
            std::printf( "  castle[%zu] '%s' color %u race %02x at (%d,%d) bld %08x garrison %d\n", i, c.name.c_str(), c.color, c.race, c.x, c.y,
                         c.constructedBuildings, c.garrisonMonsterId[0] );
        }
        return 0;
    }
    if ( args.castleIndex >= 0 ) {
        if ( args.castleIndex >= static_cast<int>( world.castles.size() ) ) {
            std::fprintf( stderr, "error: castle index %d out of range (%zu castles)\n", args.castleIndex, world.castles.size() );
            return 1;
        }
        fh2poster::CastleRender castleRender( assets );
        const QImage view = castleRender.render( world.castles[args.castleIndex] );
        if ( !view.save( QString::fromStdString( args.out ) ) ) {
            std::fprintf( stderr, "error: failed to write %s\n", args.out.c_str() );
            return 1;
        }
        std::printf( "saved castle view %s (%dx%d px)\n", args.out.c_str(), view.width(), view.height() );
        return 0;
    }

    if ( !args.quiet && routeMode != fh2poster::RouteMode::None ) {
        for ( const fh2::WorldHero & h : world.heroes ) {
            if ( h.color == 0 || h.route.steps.empty() )
                continue;
            const auto stepDelta = [&]( int direction ) -> int {
                switch ( direction ) {
                case 0x0002: return -tiles;
                case 0x0004: return -tiles + 1;
                case 0x0008: return 1;
                case 0x0010: return tiles + 1;
                case 0x0020: return tiles;
                case 0x0040: return tiles - 1;
                case 0x0080: return -1;
                case 0x0001: return -tiles - 1;
                default: return 0;
                }
            };
            const fh2::RouteStep & last = h.route.steps.back();
            const int dest = last.from + stepDelta( last.direction );
            const uint16_t objType = ( dest >= 0 && dest < tiles * tiles ) ? world.tiles[dest].mainObjectType : 0;
            std::printf( "  route: hero %d (color %d) at (%d,%d), %zu steps, hidden=%d, dest (%d,%d) obj %u\n", h.id, h.color, h.centerX, h.centerY,
                         h.route.steps.size(), h.route.hide ? 1 : 0, dest % tiles, dest / tiles, objType );
        }
    }


    // The main path: build the poster.
    fh2poster::PosterOptions options;
    options.layoutName = args.layout;
    options.layoutPriority = args.layoutPriority;
    options.selectedColor = selectedColor;
    options.withFog = ( args.fog != "none" );
    options.routes = routeMode;
    options.scale = args.scale;
    options.blocks = args.blocks;
    options.chips = args.chips;
    options.castlesOverride = args.castlesOverride;
    options.heroesOverride = args.heroesOverride;

    layout::LayoutResult res;
    QString renderError;
    QImage poster = fh2poster::renderPoster( save, world, assets, options, &res, &renderError );
    if ( poster.isNull() ) {
        std::fprintf( stderr, "error: %s\n", renderError.isEmpty() ? "poster rendering failed" : renderError.toUtf8().constData() );
        return 1;
    }

    if ( args.debugMap ) {
        std::fprintf( stderr, "canvas %dx%d map(%d,%d %dx%d)\n", res.canvasW, res.canvasH, res.map.x, res.map.y, res.map.w, res.map.h );
        for ( const auto & [id, rects] : res.blocks ) {
            std::fprintf( stderr, "block %s: %zu\n", id.c_str(), rects.size() );
            for ( const layout::Rect & r : rects )
                std::fprintf( stderr, "   (%d,%d %dx%d)\n", r.x, r.y, r.w, r.h );
        }
        std::fprintf( stderr, "groups: %zu\n", res.groups.size() );
        for ( const layout::GroupRect & g : res.groups )
            std::fprintf( stderr, "   color %d header(%d,%d %dx%d) body(%d,%d %dx%d)\n", g.color, g.header.x, g.header.y, g.header.w, g.header.h, g.body.x, g.body.y,
                          g.body.w, g.body.h );
    }

    if ( !args.quiet ) {
        std::printf( "game: day %u, week %u, month %u | castles %zu, heroes %zu, kingdoms %zu, events %zu, rumors %zu\n", world.day, world.week,
                     world.month, world.castles.size(), world.heroes.size(), world.kingdoms.size(), world.events.size(), world.rumors.size() );
        std::printf( "canvas: %dx%d px (%.1f x %.1f cm), map share: %.0f%%\n", res.canvasW, res.canvasH,
                     res.canvasW / static_cast<double>( args.dpi ) * 2.54, res.canvasH / static_cast<double>( args.dpi ) * 2.54,
                     100.0 * mapPx * mapPx / ( res.canvasW * res.canvasH ) );
        for ( const auto & [id, rects] : res.blocks ) {
            if ( rects.empty() )
                continue;
            std::printf( "  %-8s x%-2zu  first %dx%d at (%d,%d)\n", id.c_str(), rects.size(), rects[0].w, rects[0].h, rects[0].x, rects[0].y );
        }
    }
    for ( const std::string & w : res.warnings )
        std::printf( "  warning: %s\n", w.c_str() );

    // Write the requested DPI into the PNG metadata (1 in = 2.54 cm).
    const double dotsPerMeter = args.dpi * 100.0 / 2.54;
    poster.setDotsPerMeterX( dotsPerMeter );
    poster.setDotsPerMeterY( dotsPerMeter );

    const QByteArray png = fh2poster::toQuantizedPng( poster );
    QFile outFile( QString::fromStdString( args.out ) );
    if ( !outFile.open( QIODevice::WriteOnly ) || outFile.write( png ) != png.size() ) {
        std::fprintf( stderr, "error: failed to write %s\n", args.out.c_str() );
        return 1;
    }
    std::printf( "saved %s (%dx%d px)\n", args.out.c_str(), poster.width(), poster.height() );
    return 0;
}
