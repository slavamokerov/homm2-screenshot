#include "poster.h"

#include <QFile>
#include <QPainter>
#include <algorithm>
#include <cstdlib>
#include <map>
#include <set>
#include <sstream>

#include "castlerender.h"
#include "gamefont.h"
#include "layout_json.h"
#include "herocard.h"
#include "infochips.h"
#include "layout_engine.h"
#include "textutil.h"

namespace fh2poster {

namespace {

// Draws an image into a rectangle preserving its aspect ratio, centered
// (letterboxing instead of stretching). Used for castle views, hero cards and
// info chips so they are never distorted by the layout cell.
void drawContained( QPainter & p, const QRect & r, const QImage & img )
{
    if ( img.isNull() || r.width() <= 0 || r.height() <= 0 )
        return;
    const double s = std::min( static_cast<double>( r.width() ) / img.width(),
                               static_cast<double>( r.height() ) / img.height() );
    const int w = std::max( 1, static_cast<int>( std::lround( img.width() * s ) ) );
    const int h = std::max( 1, static_cast<int>( std::lround( img.height() * s ) ) );
    const int x = r.x() + ( r.width() - w ) / 2;
    const int y = r.y() + ( r.height() - h ) / 2;
    p.drawImage( QRect( x, y, w, h ), img );
}

// The ADVBORD frame, ported 1:1 from the engine's GameBorderRedraw (good
// interface, src/fheroes2/gui/interface_border.cpp): every strip is assembled
// from the same sections the game uses — corner blocks and transition blocks
// around the repeated mid tiles — so the joints (corners, TOP↔LEFT, MIDDLE
// border caps) line up exactly like in the game. The engine's repeatPattern
// helper is ported too: the leftover of a strip is blitted from the START of
// the tile, so a strip never ends with a cut-off tile.
constexpr int ADV_B = 16;      // borderWidthPx
constexpr int ADV_TILE = 32;   // tileWidthPx
constexpr int ADV_RADAR = 144; // radarWidthPx

void repeatPattern( QPainter & p, const QImage & in, int inX, int inY, int inW, int inH, int outX, int outY, int width, int height )
{
    if ( inX < 0 || inY < 0 || outX < 0 || outY < 0 || inW <= 0 || inH <= 0 || width <= 0 || height <= 0 )
        return;
    const int countX = width / inW;
    const int countY = height / inH;
    const int restWidth = width % inW;
    const int restHeight = height % inH;
    for ( int y = 0; y < countY; ++y ) {
        for ( int x = 0; x < countX; ++x )
            p.drawImage( outX + x * inW, outY + y * inH, in.copy( inX, inY, inW, inH ) );
        if ( restWidth != 0 )
            p.drawImage( outX + width - restWidth, outY + y * inH, in.copy( inX, inY, restWidth, inH ) );
    }
    if ( restHeight != 0 ) {
        for ( int x = 0; x < countX; ++x )
            p.drawImage( outX + x * inW, outY + height - restHeight, in.copy( inX, inY, inW, restHeight ) );
        if ( restWidth != 0 )
            p.drawImage( outX + width - restWidth, outY + height - restHeight, in.copy( inX, inY, restWidth, restHeight ) );
    }
}

// TOP / BOTTOM strip (good): [193 corner][6][24][32-repeats][25][6][354 corner].
void drawAdvStripH( QPainter & p, const QImage & b, int x, int y, int w, bool bottom )
{
    const int srcY = bottom ? b.height() - ADV_B : 0;
    if ( w < 640 ) {
        // The engine has no layout for tiny displays; keep the mid tiles simple.
        repeatPattern( p, b, 223, srcY, ADV_TILE, ADV_B, x, y, w, ADV_B );
        return;
    }
    const int extra = w - 640;
    const int repeatW = ( extra / ADV_TILE + 1 ) * ADV_TILE;
    const int pad = extra % ADV_TILE;
    const int padL = pad / 2;
    const int padR = pad - padL;
    const int rightW = std::max( 0, b.width() - 286 );
    int cx = x;
    p.drawImage( cx, y, b.copy( 0, srcY, 193, ADV_B ) );                     cx += 193;
    repeatPattern( p, b, 193, srcY, 6, ADV_B, cx, y, 6 + padL, ADV_B );      cx += 6 + padL;
    p.drawImage( cx, y, b.copy( 199, srcY, 24, ADV_B ) );                    cx += 24;
    repeatPattern( p, b, 223, srcY, ADV_TILE, ADV_B, cx, y, repeatW, ADV_B ); cx += repeatW;
    p.drawImage( cx, y, b.copy( 255, srcY, 25, ADV_B ) );                    cx += 25;
    repeatPattern( p, b, 280, srcY, 6, ADV_B, cx, y, 6 + padR, ADV_B );      cx += 6 + padR;
    p.drawImage( cx, y, b.copy( 286, srcY, rightW, ADV_B ) );
}

// LEFT / RIGHT strip (good): [239 corner][32-repeats][125][4-repeats][48].
void drawAdvStripV( QPainter & p, const QImage & b, int x, int y, int h, bool right )
{
    const int srcX = right ? b.width() - ADV_B : 0;
    if ( h < 448 ) {
        repeatPattern( p, b, srcX, 255, ADV_B, ADV_TILE, x, y, ADV_B, h );
        return;
    }
    const int extra = h - 448;
    const int repeatH = ( extra / ADV_TILE + 1 ) * ADV_TILE;
    const int pad = extra % ADV_TILE;
    int cy = y;
    p.drawImage( x, cy, b.copy( srcX, 16, ADV_B, 239 ) );                      cy += 239;
    repeatPattern( p, b, srcX, 255, ADV_B, ADV_TILE, x, cy, ADV_B, repeatH );  cy += repeatH;
    p.drawImage( x, cy, b.copy( srcX, 287, ADV_B, 125 ) );                     cy += 125;
    repeatPattern( p, b, srcX, 412, ADV_B, 4, x, cy, ADV_B, 4 + pad );         cy += 4 + pad;
    p.drawImage( x, cy, b.copy( srcX, 416, ADV_B, b.height() - 16 - 416 ) );
}

// MIDDLE BORDER (good) — the vertical strip that connects the top and bottom
// frames in the game: [239 cap][top repeats][50][32][bottom repeats][43]
// [8-repeats][tail]. Used for the internal separators.
void drawAdvMiddleV( QPainter & p, const QImage & b, int x, int y, int h )
{
    const int srcX = b.width() - ADV_RADAR - 2 * ADV_B;
    if ( h < 448 ) {
        repeatPattern( p, b, srcX, 255, ADV_B, ADV_TILE, x, y, ADV_B, h );
        return;
    }
    const int extra = h - 448;
    const int repeatH = ( extra / ADV_TILE + 1 ) * ADV_TILE;
    const int iconsCount = extra / ADV_TILE > 3 ? 8 : ( extra / ADV_TILE < 3 ? 4 : 7 );
    const int topH = ( iconsCount - 3 ) * ADV_TILE;
    const int bottomH = repeatH - topH;
    const int pad = extra % ADV_TILE;
    int cy = y;
    p.drawImage( x, cy, b.copy( srcX, 16, ADV_B, 239 ) );                       cy += 239;
    repeatPattern( p, b, srcX, 255, ADV_B, ADV_TILE, x, cy, ADV_B, topH );      cy += topH;
    p.drawImage( x, cy, b.copy( srcX, 287, ADV_B, 50 ) );                       cy += 50;
    p.drawImage( x, cy, b.copy( srcX, 337, ADV_B, ADV_TILE ) );                 cy += ADV_TILE;
    repeatPattern( p, b, srcX, 337, ADV_B, ADV_TILE, x, cy, ADV_B, bottomH );   cy += bottomH;
    p.drawImage( x, cy, b.copy( srcX, 369, ADV_B, 43 ) );                       cy += 43;
    repeatPattern( p, b, srcX, 412, ADV_B, 8, x, cy, ADV_B, 8 + pad );          cy += 8 + pad;
    p.drawImage( x, cy, b.copy( srcX, 420, ADV_B, b.height() - 16 - 420 ) );
}

// Renders the adventure-map border in the in-game style (ADVBORD), ported from
// the engine's GameBorderRedraw (good interface).
void drawGameBorder( QPainter & p, const fh2::IcnSprite & bord, int x, int y, int w, int h )
{
    const QImage & b = bord.image;
    drawAdvStripH( p, b, x, y, w, false );
    drawAdvStripH( p, b, x, y + h - ADV_B, w, true );
    drawAdvStripV( p, b, x, y + ADV_B, h - 2 * ADV_B, false );
    drawAdvStripV( p, b, x + w - ADV_B, y + ADV_B, h - 2 * ADV_B, true );
}

// Player color -> display name and accent band color (kingdom groups).
const char * colorLabel( int c )
{
    switch ( c ) {
    case 0x01: return "Blue kingdom";
    case 0x02: return "Green kingdom";
    case 0x04: return "Red kingdom";
    case 0x08: return "Yellow kingdom";
    case 0x10: return "Orange kingdom";
    case 0x20: return "Purple kingdom";
    default: return "Kingdom";
    }
}

QColor colorName( int c )
{
    switch ( c ) {
    case 0x02: return QColor( 60, 140, 60 );
    case 0x04: return QColor( 180, 50, 45 );
    case 0x08: return QColor( 200, 170, 60 );
    case 0x10: return QColor( 200, 130, 50 );
    case 0x20: return QColor( 150, 80, 170 );
    default: return QColor( 70, 110, 200 );
    }
}

// The info chips in their fixed display order (matches the legend and the layout).
const char * chipNames[] = { "minimap", "resources", "calendar", "rumors", "puzzle", "victory" };

} // namespace

QImage renderMap( const fh2::WorldData & world, const fh2::Assets & assets, int mapPx, int selectedColor, bool withFog, RouteMode routes )
{
    MapRender mapRender( assets );
    return mapRender.render( world, mapPx, selectedColor, withFog, routes );
}

int resourceColor( const fh2::SaveFile & save, const fh2::WorldData & world, int selectedColor )
{
    if ( selectedColor != 0 )
        return selectedColor;
    const auto hasKingdom = [&]( int c ) {
        for ( const fh2::WorldKingdom & k : world.kingdoms )
            if ( k.color == c )
                return true;
        return false;
    };
    for ( int c : save.humanColors() ) {
        if ( hasKingdom( c ) )
            return c;
    }
    for ( const fh2::WorldCastle & c : world.castles )
        if ( c.color != 0 )
            return c.color;
    for ( const fh2::WorldHero & h : world.heroes )
        if ( h.color != 0 )
            return h.color;
    return 1;
}

QImage renderPoster( const fh2::SaveFile & save, const fh2::WorldData & world, const fh2::Assets & assets, const PosterOptions & options,
                     layout::LayoutResult * layoutOut, QString * errorOut )
{
    if ( world.width <= 0 )
        return {};
    const int mapPx = static_cast<int>( world.width * 32 * options.scale );

    // Count the legend entries for the chosen player.
    int castleCount = 0;
    for ( const fh2::WorldCastle & c : world.castles )
        if ( options.selectedColor == 0 || c.color == options.selectedColor )
            ++castleCount;
    int heroCount = 0;
    for ( const fh2::WorldHero & h : world.heroes )
        if ( h.color != 0 && ( options.selectedColor == 0 || h.color == options.selectedColor ) )
            ++heroCount;
    if ( options.castlesOverride >= 0 )
        castleCount = options.castlesOverride;
    if ( options.heroesOverride >= 0 )
        heroCount = options.heroesOverride;

    // Chips are enabled / selected; the legend keeps a slot per chip (their
    // heights/widths differ per type).
    int chipCount = 0;
    for ( const char * cn : chipNames )
        if ( options.chips.empty() || options.chips.find( cn ) != std::string::npos )
            ++chipCount;

    // The kingdom whose resources are shown: the selected player when fog is on;
    // otherwise (all kingdoms visible) the human player of the save. Color 0 is
    // the neutral kingdom, whose resources are always zero.
    const int infoColor = resourceColor( save, world, options.selectedColor );

    // --blocks: a comma list of enabled zones ("map,castles,heroes,chips,title").
    // Empty means all zones. Disabled zones are removed so they collapse to zero
    // thickness. "map" has no zone definition and is skipped when drawing.
    std::set<std::string> enabledBlocks;
    if ( !options.blocks.empty() ) {
        std::istringstream ss( options.blocks );
        std::string id;
        while ( std::getline( ss, id, ',' ) ) {
            if ( !id.empty() )
                enabledBlocks.insert( id );
        }
    }
    const auto blockEnabled = [&]( const std::string & id ) {
        return enabledBlocks.empty() || enabledBlocks.count( id ) != 0;
    };

    const bool isCartouche = ( options.layoutName == "cardushe" );
    layout::LayoutResult res;

    if ( isCartouche ) {
        // The in-game layout: a square map on top, a row of info chips, then the
        // kingdom groups (player colors) growing downward.
        layout::CartoucheInput in;
        in.mapPx = mapPx;
        in.frame = 16; // the content sits flush against the in-game frame
        in.cardGap = 18; // room for the frame-piece separators between cards
        in.chipGap = 18;
        in.hasTitle = blockEnabled( "title" );
        const int s = static_cast<int>( options.scale );
        // Castle cards keep their content height (the race background + garrison
        // strip) so the kingdom cells don't leave an empty strip below them.
        CastleRender castleMetrics( assets );
        int castleMaxH = 100;
        for ( const fh2::WorldCastle & c : world.castles )
            if ( options.selectedColor == 0 || c.color == options.selectedColor )
                castleMaxH = std::max( castleMaxH, castleMetrics.contentHeight( c ) );
        in.castleCard = { 360 * s, castleMaxH * s };
        // Hero cards keep their content height (artifacts/army rows) so the
        // kingdom cells don't leave a tall empty strip under the card.
        HeroCardRender heroMetrics( assets );
        int heroMaxH = 130;
        for ( const fh2::WorldHero & h : world.heroes )
            if ( h.color != 0 && ( options.selectedColor == 0 || h.color == options.selectedColor ) )
                heroMaxH = std::max( heroMaxH, heroMetrics.contentHeight( h ) );
        in.heroCard = { 360 * s, heroMaxH * s };
        // The chips row: minimap (fog-aware square), the compact info aggregate
        // (resources + victory + events + rumors) and the puzzle (a square of the
        // same height as the minimap). Everything measures its content size.
        const auto chipEnabled = [&]( const std::string & n ) {
            return options.chips.empty() || options.chips.find( n ) != std::string::npos;
        };
        static const char * textChips4[] = { "resources", "calendar", "rumors", "victory" };
        bool anyTextChip = false;
        for ( const char * tn : textChips4 )
            anyTextChip = anyTextChip || chipEnabled( tn );
        const int sq = std::max( 64, 300 * s );
        InfoChips cm( assets );
        std::set<std::string> enabledTextChips;
        for ( const char * tn : textChips4 )
            if ( chipEnabled( tn ) )
                enabledTextChips.insert( tn );
        if ( chipEnabled( "minimap" ) ) {
            const QImage m = cm.renderChip( "minimap", world, save.mapInfo(), options.selectedColor, sq, sq );
            in.chipSizes.push_back( { std::min( std::max( 64, m.width() ), sq + 8 ), std::min( std::max( 24, m.height() ), sq + 8 ) } );
        }
        if ( anyTextChip ) {
            const QImage m = cm.renderInfo( world, save.mapInfo(), infoColor, 258 * s, enabledTextChips );
            in.chipSizes.push_back( { std::max( 64, m.width() ), std::max( 24, m.height() ) } );
        }
        if ( chipEnabled( "puzzle" ) ) {
            // The puzzle cell is the same square as the minimap (content is
            // contained, so the 448 panel never exceeds the minimap).
            in.chipSizes.push_back( { sq, sq } );
        }
        // All chips go into one row below the map; if the measured widths do not
        // fit into the content width, shrink them uniformly so the row always
        // fits on a single line no taller than the minimap.
        {
            const int innerW = in.mapPx; // flush with the frame on both sides
            long long sumW = 0;
            for ( const auto & cs : in.chipSizes )
                sumW += cs.w;
            const int n = static_cast<int>( in.chipSizes.size() );
            const long long avail = innerW - ( n > 1 ? ( n - 1 ) * in.chipGap : 0 );
            if ( avail > 0 && sumW > avail ) {
                const double f = static_cast<double>( avail ) / sumW;
                for ( auto & cs : in.chipSizes ) {
                    cs.w = std::max( 40, static_cast<int>( cs.w * f ) );
                    cs.h = std::max( 24, static_cast<int>( cs.h * f ) );
                }
            }
        }

        // Kingdom groups, in color order (1,2,4,8,16,32). With fog, only the
        // selected color's kingdom is shown; otherwise all kingdoms with content.
        std::vector<int> colors;
        auto addColor = [&]( int c ) {
            if ( c != 0 && ( options.selectedColor == 0 || c == options.selectedColor ) ) {
                if ( std::find( colors.begin(), colors.end(), c ) == colors.end() )
                    colors.push_back( c );
            }
        };
        for ( const fh2::WorldHero & h : world.heroes )
            addColor( h.color );
        for ( const fh2::WorldCastle & c : world.castles )
            addColor( c.color );
        std::sort( colors.begin(), colors.end() );
        for ( int c : colors ) {
            int hc = 0;
            for ( const fh2::WorldHero & h : world.heroes )
                if ( h.color == c && h.color != 0 )
                    ++hc;
            int cc = 0;
            for ( const fh2::WorldCastle & cs : world.castles )
                if ( cs.color == c )
                    ++cc;
            if ( hc <= 0 && cc <= 0 )
                continue;
            in.groups.push_back( { c, hc, cc } );
        }

        res = layout::computeCartoucheLayout( in );
    }
    else {
        // A user JSON schema (--layout file.json).
        layout::LayoutParams params;
        QFile file( QString::fromStdString( options.layoutName ) );
        if ( !file.open( QIODevice::ReadOnly ) ) {
            if ( errorOut )
                *errorOut = QStringLiteral( "cannot open the layout file %1" ).arg( QString::fromStdString( options.layoutName ) );
            return {};
        }
        QString error;
        if ( !layoutFromJson( file.readAll(), params, &error ) ) {
            if ( errorOut )
                *errorOut = QStringLiteral( "invalid layout %1: %2" ).arg( QString::fromStdString( options.layoutName ), error );
            return {};
        }
        params.scale = static_cast<int>( options.scale );
        params.zones.erase( std::remove_if( params.zones.begin(), params.zones.end(),
                                            [&]( const layout::ZoneDef & z ) { return !blockEnabled( z.id ); } ),
                            params.zones.end() );
        const std::map<std::string, int> counts = {
            { "castles", castleCount },
            { "heroes", heroCount },
            { "chips", chipCount },
        };
        // Heroes pack at their own content height.
        HeroCardRender heroRenderMetrics( assets );
        for ( layout::ZoneDef & z : params.zones ) {
            if ( z.id != "heroes" || !z.grid.fixedW )
                continue;
            std::vector<int> hs;
            for ( size_t i = 0; i < world.heroes.size(); ++i )
                if ( world.heroes[i].color != 0
                     && ( options.selectedColor == 0 || world.heroes[i].color == options.selectedColor ) )
                    hs.push_back( heroRenderMetrics.contentHeight( world.heroes[i] ) );
            if ( !hs.empty() )
                z.grid.fixedHList = std::move( hs );
        }
        // Chips measure their content height (crop the empty strip).
        InfoChips chipsMetrics( assets );
        for ( layout::ZoneDef & z : params.zones ) {
            if ( z.id != "chips" || !z.grid.fixedW )
                continue;
            const std::vector<int> presetsW = z.grid.fixedWList;
            const std::vector<int> presetsH = z.grid.fixedHList;
            std::vector<int> ws, hs;
            size_t idx = 0;
            for ( const char * cn : chipNames ) {
                if ( !( options.chips.empty() || options.chips.find( cn ) != std::string::npos ) )
                    continue;
                const int cw = ( idx < presetsW.size() && presetsW[idx] > 0 ) ? presetsW[idx] : z.grid.fixedW;
                const bool fixed = ( std::string( cn ) == "minimap" || std::string( cn ) == "puzzle" );
                const int ch = fixed ? ( ( idx < presetsH.size() && presetsH[idx] > 0 ) ? presetsH[idx] : z.grid.fixedH )
                                     : std::max( 1, chipsMetrics.renderChip( cn, world, save.mapInfo(), options.selectedColor, cw, 2000 ).height() );
                ws.push_back( std::max( 64, cw ) );
                hs.push_back( std::max( 24, ch ) );
                ++idx;
            }
            if ( !ws.empty() ) {
                z.grid.fixedWList = std::move( ws );
                z.grid.fixedHList = std::move( hs );
            }
        }
        res = layout::computeLayout( params, mapPx, counts );
    }
    if ( layoutOut )
        *layoutOut = res;

    // Card order: for the cartouche (groups) the heroes/castles are grouped by
    // color to match the layout's group layout; otherwise the natural order.
    std::vector<fh2::WorldHero> heroOrder;
    std::vector<fh2::WorldCastle> castleOrder;
    if ( !res.groups.empty() ) {
        for ( const layout::GroupRect & g : res.groups ) {
            for ( const fh2::WorldHero & h : world.heroes )
                if ( h.color == g.color && h.color != 0 )
                    heroOrder.push_back( h );
            for ( const fh2::WorldCastle & c : world.castles )
                if ( c.color == g.color )
                    castleOrder.push_back( c );
        }
    }
    else {
        for ( const fh2::WorldHero & h : world.heroes )
            if ( h.color != 0 && ( options.selectedColor == 0 || h.color == options.selectedColor ) )
                heroOrder.push_back( h );
        for ( const fh2::WorldCastle & c : world.castles )
            if ( options.selectedColor == 0 || c.color == options.selectedColor )
                castleOrder.push_back( c );
    }

    const QImage mapImage = renderMap( world, assets, mapPx, options.selectedColor, options.withFog, options.routes );
    if ( mapImage.isNull() )
        return {};

    CastleRender castleRender( assets );
    HeroCardRender heroRender( assets );
    InfoChips chips( assets );

    QImage poster( res.canvasW, res.canvasH, QImage::Format_ARGB32 );
    {
        QPainter p( &poster );
        p.setRenderHint( QPainter::SmoothPixmapTransform, false );
        // Star-sky background (fog tiles), like the editor's app background.
        for ( int y = 0; y < res.canvasH; y += 32 ) {
            for ( int x = 0; x < res.canvasW; x += 32 ) {
                const fh2::IcnSprite & fogTile = assets.tilSprite( "CLOF32.TIL", ( x / 32 + y / 32 ) % 4 );
                if ( !fogTile.isNull() )
                    p.drawImage( x, y, fogTile.image );
            }
        }

        // The single in-game ADVBORD frame wraps the whole poster (the same
        // light metal-and-gems metal border as the game interface).
        const fh2::IcnSprite & advbord = assets.icnSprite( "ADVBORD.ICN", 0 );
        const bool hasAdbord = !advbord.isNull();
        if ( hasAdbord )
            drawGameBorder( p, advbord, 0, 0, res.canvasW, res.canvasH );

        if ( blockEnabled( "map" ) ) {
            p.drawImage( res.map.x, res.map.y, mapImage );
        }

        // Kingdom groups: a banded header (no per-group frames); the thin
        // separators between blocks/cards are drawn after all content.
        const auto drawGroups = [&]() {
            fh2::GameFont gfont( &assets );
            // FLAG32 frame of the player color (like getCastleLeftFlagIcnIndex).
            const auto flagIndex = []( int c ) {
                switch ( c ) {
                case 0x01: return 0;  // blue
                case 0x02: return 2;  // green
                case 0x04: return 4;  // red
                case 0x08: return 6;  // yellow
                case 0x10: return 8;  // orange
                case 0x20: return 10; // purple
                default: return 0;
                }
            };
            // Bottom of the capital letters (the text baseline) relative to the y
            // passed to drawText: measured with a probe so the flag's bottom edge
            // can be aligned with the letters, independent of the font metrics.
            int baselineOffset = 0;
            if ( gfont.valid() ) {
                QImage probe( 32, 32, QImage::Format_ARGB32 );
                probe.fill( Qt::transparent );
                QPainter pp( &probe );
                gfont.drawText( pp, 0, 0, QStringLiteral( "H" ), fh2::GameFont::Size::NORMAL, fh2::GameFont::Color::YELLOW );
                pp.end();
                for ( int y = 0; y < probe.height(); ++y )
                    for ( int x = 0; x < probe.width(); ++x )
                        if ( qAlpha( probe.pixel( x, y ) ) > 0 )
                            baselineOffset = std::max( baselineOffset, y );
                if ( baselineOffset <= 0 )
                    baselineOffset = gfont.lineHeight( fh2::GameFont::Size::NORMAL ) - 2;
            }
            for ( const layout::GroupRect & g : res.groups ) {
                const QColor band = colorName( g.color );
                const QRect hb( g.header.x, g.header.y, g.header.w, g.header.h );
                p.fillRect( hb, QColor( 20, 18, 16 ) );
                p.fillRect( hb.x(), hb.y(), hb.width(), 3, band );
                const int lh = gfont.valid() ? gfont.lineHeight( fh2::GameFont::Size::NORMAL ) : 17;
                const int textY = hb.y() + ( hb.height() - lh ) / 2;
                int textX = hb.x() + 18;
                const fh2::IcnSprite & flag = assets.icnSprite( "FLAG32.ICN", flagIndex( g.color ) );
                if ( !flag.isNull() ) {
                    p.drawImage( hb.x() + 10, textY + baselineOffset - flag.image.height() + 1, flag.image );
                    textX = hb.x() + 10 + flag.image.width() + 10;
                }
                if ( gfont.valid() ) {
                    const QString label = QString::fromStdString( colorLabel( g.color ) );
                    gfont.drawText( p, textX, textY, label, fh2::GameFont::Size::NORMAL, fh2::GameFont::Color::YELLOW );
                }
            }
        };
        drawGroups();

        // Castles.
        if ( res.blocks.count( "castles" ) ) {
            const auto & rects = res.blocks.at( "castles" );
            const size_t n = std::min( rects.size(), castleOrder.size() );
            for ( size_t i = 0; i < n; ++i ) {
                const QImage view = castleRender.render( castleOrder[i] );
                if ( view.isNull() )
                    continue;
                const layout::Rect & r = rects[i];
                drawContained( p, QRect( r.x, r.y, r.w, r.h ), view );
            }
        }

        // Heroes.
        if ( res.blocks.count( "heroes" ) ) {
            const auto & rects = res.blocks.at( "heroes" );
            const size_t n = std::min( rects.size(), heroOrder.size() );
            for ( size_t i = 0; i < n; ++i ) {
                const QImage card = heroRender.render( heroOrder[i] );
                if ( card.isNull() )
                    continue;
                const layout::Rect & r = rects[i];
                drawContained( p, QRect( r.x, r.y, r.w, r.h ), card );
            }
        }

        // Info chips: minimap, the info aggregate (resources/victory/events/
        // rumors) and the puzzle — same order the layout measured them in.
        if ( res.blocks.count( "chips" ) ) {
            const auto chipEnabledD = [&]( const std::string & n ) {
                return options.chips.empty() || options.chips.find( n ) != std::string::npos;
            };
            static const char * textChips4D[] = { "resources", "calendar", "rumors", "victory" };
            std::set<std::string> enabledText4;
            bool anyTextD = false;
            for ( const char * tn : textChips4D )
                if ( chipEnabledD( tn ) ) {
                    enabledText4.insert( tn );
                    anyTextD = true;
                }
            std::vector<QImage> chipImgs;
            const int sD = static_cast<int>( options.scale );
            if ( chipEnabledD( "minimap" ) )
                chipImgs.push_back( chips.renderChip( "minimap", world, save.mapInfo(), options.selectedColor, 300 * sD, 300 * sD ) );
            if ( anyTextD )
                chipImgs.push_back( chips.renderInfo( world, save.mapInfo(), infoColor, 258 * sD, enabledText4 ) );
            if ( chipEnabledD( "puzzle" ) )
                chipImgs.push_back( chips.renderChip( "puzzle", world, save.mapInfo(), options.selectedColor, 300 * sD, 300 * sD ) );
            const auto & rects = res.blocks.at( "chips" );
            const size_t n = std::min( rects.size(), chipImgs.size() );
            for ( size_t i = 0; i < n; ++i ) {
                if ( chipImgs[i].isNull() )
                    continue;
                drawContained( p, QRect( rects[i].x, rects[i].y, rects[i].w, rects[i].h ), chipImgs[i] );
            }
        }

        // Title in the game font: rendered at the natural size, then scaled
        // up (nearest neighbor) to fill the title strip.
        if ( res.blocks.count( "title" ) && !res.blocks.at( "title" ).empty() ) {
            const layout::Rect & r = res.blocks.at( "title" ).front();
            const std::string mapName = save.mapInfo().name.empty() ? "Untitled" : save.mapInfo().name;
            const QString titleText = QStringLiteral( "%1 — day %2, week %3, month %4" )
                                          .arg( QString::fromStdString( mapName ) )
                                          .arg( world.day )
                                          .arg( world.week )
                                          .arg( world.month );
            fh2::GameFont font( &assets );
            if ( font.valid() ) {
                const int naturalW = font.textWidth( titleText, fh2::GameFont::Size::NORMAL );
                const int naturalH = font.lineHeight( fh2::GameFont::Size::NORMAL );
                const int scale = std::max( 1, std::min( ( r.w - 40 ) / std::max( 1, naturalW ), ( r.h - 12 ) / naturalH ) );
                QImage buf( std::max( 1, naturalW + 4 ), std::max( 1, naturalH + 2 ), QImage::Format_ARGB32 );
                buf.fill( Qt::transparent );
                QPainter tp( &buf );
                font.drawText( tp, 2, 0, titleText, fh2::GameFont::Size::NORMAL, fh2::GameFont::Color::YELLOW );
                tp.end();
                const int w2 = buf.width() * scale;
                const int h2 = buf.height() * scale;
                const QImage scaled = buf.scaled( w2, h2, Qt::IgnoreAspectRatio, Qt::FastTransformation );
                p.drawImage( r.x + ( r.w - w2 ) / 2, r.y + ( r.h - h2 ) / 2, scaled );
            }
        }

        // Separators between blocks/cards are the engine's own strips: horizontal
        // ones use the TOP/BOTTOM sections (corner blocks at both ends), vertical
        // ones the MIDDLE BORDER section (capped column). They are drawn edge to
        // edge so the joints with the outer frame are seamless.
        if ( hasAdbord ) {
            const QImage & bb = advbord.image;
            const int canvasW = res.canvasW;
            const int sepX0 = ADV_B;
            const int sepX1 = canvasW - ADV_B;
            const auto hSep = [&]( int yc ) {
                drawAdvStripH( p, bb, sepX0, yc - ADV_B / 2, sepX1 - sepX0, false );
            };
            const auto vSep = [&]( int xc, int y0, int y1 ) {
                if ( y1 - y0 >= ADV_B )
                    drawAdvMiddleV( p, bb, xc - ADV_B / 2, y0, y1 - y0 );
            };
            // The first/last horizontal strips around the chips row: the vertical
            // separators are extended to them (like the MIDDLE border connects
            // TOP and BOTTOM in the game).
            const auto & chipsR = res.blocks["chips"];
            auto blockBottom = []( const std::vector<layout::Rect> & rs ) {
                int bottom = 0;
                for ( const layout::Rect & r : rs )
                    bottom = std::max( bottom, r.y + r.h );
                return bottom;
            };
            int yMapSep = -1;
            int yFirstGroupSep = -1;
            if ( !chipsR.empty() )
                yMapSep = ( res.map.y + res.map.h + chipsR[0].y ) / 2;
            if ( !res.groups.empty() ) {
                const int topLimit = chipsR.empty() ? res.map.y + res.map.h : blockBottom( chipsR );
                yFirstGroupSep = ( topLimit + res.groups[0].header.y ) / 2;
            }
            // Vertical separators between the cards of a row, horizontal between
            // the rows / big blocks.
            const auto grid = [&]( const std::vector<layout::Rect> & rs ) {
                if ( rs.empty() )
                    return;
                std::vector<std::pair<int, std::vector<int>>> rows;
                for ( size_t i = 0; i < rs.size(); ++i ) {
                    int ry = rs[i].y;
                    bool found = false;
                    for ( auto & row : rows ) {
                        if ( std::abs( row.first - ry ) <= 2 ) {
                            row.second.push_back( static_cast<int>( i ) );
                            found = true;
                            break;
                        }
                    }
                    if ( !found )
                        rows.push_back( { ry, { static_cast<int>( i ) } } );
                }
                std::sort( rows.begin(), rows.end(), []( const auto & a, const auto & b ) { return a.first < b.first; } );
                for ( auto & row : rows ) {
                    std::sort( row.second.begin(), row.second.end(), [&]( int a, int b ) { return rs[a].x < rs[b].x; } );
                    // The separators run the full height of the row (like a
                    // spreadsheet grid), not just the overlap of two neighbors.
                    int rowTop = rs[row.second.front()].y;
                    int rowBottom = rs[row.second.front()].y + rs[row.second.front()].h;
                    for ( int idx : row.second ) {
                        rowTop = std::min( rowTop, rs[idx].y );
                        rowBottom = std::max( rowBottom, rs[idx].y + rs[idx].h );
                    }
                    const int vTop = ( yMapSep >= 0 && yMapSep + ADV_B / 2 < rowTop ) ? yMapSep + ADV_B / 2 : rowTop;
                    const int vBottom = ( yFirstGroupSep >= 0 && yFirstGroupSep - ADV_B / 2 > rowBottom ) ? yFirstGroupSep - ADV_B / 2 : rowBottom;
                    for ( size_t i = 0; i + 1 < row.second.size(); ++i ) {
                        const layout::Rect & a = rs[row.second[i]];
                        const layout::Rect & b = rs[row.second[i + 1]];
                        const int gapX = b.x - ( a.x + a.w );
                        if ( gapX < 16 )
                            continue;
                        // A chip flush with the right content edge (the puzzle is
                        // always right-aligned) gets the separator right against its
                        // left edge; other pairs keep it centered in the gap.
                        const bool rightAligned = ( b.x + b.w == res.map.x + res.map.w );
                        vSep( rightAligned ? ( b.x - ADV_B / 2 ) : ( a.x + a.w + gapX / 2 ), vTop, vBottom );
                    }
                }
            };
            grid( res.blocks["chips"] );
            // Separators between the big blocks: map -> chips -> groups -> groups.
            // Each horizontal separator sits in the GAP between the blocks (not
            // inside them): below the map/chips bottom, above the next header.
            if ( !chipsR.empty() )
                hSep( yMapSep );
            for ( size_t i = 0; i < res.groups.size(); ++i ) {
                const layout::GroupRect & g = res.groups[i];
                const int topLimit = ( i == 0 ) ? ( chipsR.empty() ? res.map.y + res.map.h : blockBottom( chipsR ) )
                                                : res.groups[i - 1].body.y + res.groups[i - 1].body.h;
                hSep( ( topLimit + g.header.y ) / 2 + 4 );
            }
        }
    }

    return poster;
}

} // namespace fh2poster
