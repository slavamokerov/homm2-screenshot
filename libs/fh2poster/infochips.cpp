#include "infochips.h"

#include <QPainter>
#include <algorithm>
#include <cstdio>

#include "gamefont.h"
#include "textutil.h"

namespace fh2poster {

namespace {

const fh2::WorldKingdom * kingdomOfColor( const fh2::WorldData & world, int color )
{
    for ( const fh2::WorldKingdom & k : world.kingdoms )
        if ( k.color == color )
            return &k;
    return nullptr;
}

// Packs an image to its content: crop the transparent padding around the
// opaque pixels (both horizontally and vertically), with a small slack so
// glyph tails are not cut. The minimap (a solid fog-aware square) and the
// puzzle panel are opaque to their edges, so they keep almost their full size.
// fixedTop >= 0 pins the top edge (text chips: the crop must not depend on the
// tallest glyph, otherwise lines with different ascenders are not aligned).
QImage cropContent( QImage img, int fixedTop = -1 )
{
    const int w = img.width(), h = img.height();
    int minX = w, minY = h, maxX = -1, maxY = -1;
    for ( int yy = 0; yy < h; ++yy ) {
        for ( int xx = 0; xx < w; ++xx ) {
            if ( qAlpha( img.pixel( xx, yy ) ) > 0 ) {
                if ( xx < minX ) minX = xx;
                if ( xx > maxX ) maxX = xx;
                if ( yy < minY ) minY = yy;
                if ( yy > maxY ) maxY = yy;
            }
        }
    }
    if ( maxX >= 0 ) {
        // Symmetric slack on both axes: square chips (minimap, puzzle) must not
        // lose a few pixels on one axis only.
        const int pad = 4;
        const int x0 = std::max( 0, minX - pad );
        const int y0 = ( fixedTop >= 0 ) ? std::min( std::max( 0, fixedTop ), h - 1 ) : std::max( 0, minY - pad );
        const int x1 = std::min( w, maxX + 1 + pad );
        const int y1 = std::min( h, maxY + 1 + pad );
        img = img.copy( x0, y0, x1 - x0, y1 - y0 );
    }
    return img;
}

} // namespace

InfoChips::InfoChips( const fh2::Assets & assets )
    : _a( assets )
{}

QImage InfoChips::renderChip( const std::string & chip, const fh2::WorldData & world, const fh2::MapInfo & mapInfo, int selectedColor, int w, int h ) const
{
    QImage img( w, h, QImage::Format_ARGB32 );
    // Transparent chip backgrounds: the content itself (minimap with fog, the
    // puzzle panel) is opaque, so no panel backdrop is needed.
    img.fill( Qt::transparent );
    QPainter p( &img );
    p.setRenderHint( QPainter::SmoothPixmapTransform, false );

    fh2::GameFont font( &_a );
    // All chip text is left-aligned at the chip's left edge (like the game's
    // info panels); only the resource counts are centered under their icons.
    const auto body = [&]( int y, const QString & text, const fh2::GameFont::Color color = fh2::GameFont::Color::WHITE ) {
        if ( font.valid() )
            font.drawText( p, 4, y, text, fh2::GameFont::Size::NORMAL, color );
    };

    if ( chip == "minimap" ) {
        const int tiles = std::max( world.width, world.height );
        const int size = std::min( w, h ); // flush with the chip edges
        const int mw = size * world.width / tiles;
        const int mh = size * world.height / tiles;
        const int TILE = 32;
        const bool fogActive = selectedColor != 0;
        QImage worldImg( world.width * TILE, world.height * TILE, QImage::Format_ARGB32 );
        worldImg.fill( qRgba( 22, 22, 26, 255 ) );
        {
            QPainter wp( &worldImg );
            for ( int ty = 0; ty < world.height; ++ty ) {
                for ( int tx = 0; tx < world.width; ++tx ) {
                    const fh2::WorldTile & t = world.tiles[static_cast<size_t>( ty ) * world.width + tx];
                    QImage g = _a.tilSprite( "GROUND32.TIL", t.terrainImageIndex ).image;
                    if ( !g.isNull() ) {
                        // Orient the tile like the main map (maprender): the ground
                        // tiles carry a water/shore direction in terrainFlags.
                        const int shape = t.terrainFlags & 0x3;
                        if ( shape & 2 )
                            g = g.mirrored( true, false );
                        if ( shape & 1 )
                            g = g.mirrored( false, true );
                        wp.drawImage( tx * TILE, ty * TILE, g );
                    }
                    if ( fogActive && ( t.fogColors & selectedColor ) ) {
                        // Solid fog for the selected player (like CLOF32 on the map).
                        wp.fillRect( tx * TILE, ty * TILE, TILE, TILE, QColor( 16, 18, 24 ) );
                    }
                }
            }
        }
        QImage mini = worldImg.scaled( mw, mh, Qt::IgnoreAspectRatio, Qt::SmoothTransformation );
        {
            QPainter mp( &mini );
            const auto fogged = [&]( int x, int y ) {
                if ( x < 0 || y < 0 || x >= world.width || y >= world.height )
                    return true;
                return ( world.tiles[static_cast<size_t>( y ) * world.width + x].fogColors & selectedColor ) != 0;
            };
            const auto colorRgb = []( uint8_t cbit ) {
                switch ( cbit ) {
                case 0x01: return QColor( 220, 40, 40 );   // red
                case 0x02: return QColor( 40, 180, 60 );   // green
                case 0x04: return QColor( 50, 90, 220 );   // blue
                case 0x08: return QColor( 220, 200, 40 );  // yellow
                case 0x10: return QColor( 230, 140, 30 );  // orange
                case 0x20: return QColor( 160, 60, 200 );  // purple
                default:   return QColor( 200, 200, 200 );
                }
            };
            for ( const fh2::WorldCastle & c : world.castles ) {
                if ( fogActive && fogged( c.x, c.y ) )
                    continue;
                mp.fillRect( c.x * mw / world.width, c.y * mh / world.height, 5, 5, colorRgb( c.color ) );
            }
            for ( const fh2::WorldHero & hh : world.heroes ) {
                if ( fogActive && fogged( hh.centerX, hh.centerY ) )
                    continue;
                mp.fillRect( hh.centerX * mw / world.width, hh.centerY * mh / world.height, 3, 3, colorRgb( hh.color ) );
            }
        }
        p.drawImage( ( w - mw ) / 2, ( h - mh ) / 2, mini );
        goto done;
    }

    if ( chip == "resources" ) {
        // In-game resource panel layout (Castle::drawResourcePanel): two tight
        // columns (icon width + a 2px gap, like the engine's 39 + 2 + 39), three
        // rows of two, and gold centered under them in the fourth row.
        const fh2::WorldKingdom * k = kingdomOfColor( world, selectedColor );
        if ( !k ) {
            body( 8, QStringLiteral( "no data" ) );
            goto done;
        }
        // RESOURCE.ICN indices in the display order the games lays them out.
        static const int order[7] = { 0, 3, 4, 1, 2, 5, 6 }; // wood, sulfur, crystal, mercury, ore, gems, gold
        int iconW = 0, iconH = 0;
        for ( int i = 0; i < 7; ++i ) {
            const fh2::IcnSprite & icon = _a.icnSprite( "RESOURCE.ICN", i );
            iconW = std::max( iconW, icon.image.width() );
            iconH = std::max( iconH, icon.image.height() );
        }
        const int numH = font.valid() ? font.lineHeight( fh2::GameFont::Size::SMALL ) : 11;
        const int rowH = std::max( 1, iconH ) + numH + 2;
        const int gap = 2;
        const int blockW = 2 * iconW + gap;
        const int blockX = 4;
        const int y0 = 4;
        for ( int i = 0; i < 7; ++i ) {
            const int row = i / 2; // i == 6 (gold) lands in the fourth row
            const fh2::IcnSprite & icon = _a.icnSprite( "RESOURCE.ICN", order[i] );
            const int iw = icon.image.width();
            const int cx = ( i < 6 ) ? blockX + ( i % 2 ) * ( iconW + gap ) + ( iconW - iw ) / 2
                                     : blockX + ( blockW - iw ) / 2;
            const int cy = y0 + row * rowH;
            if ( !icon.isNull() )
                p.drawImage( cx, cy, icon.image );
            if ( font.valid() ) {
                const QString num = QString::number( k->resources[order[i]] );
                font.drawText( p, cx + ( iw - font.textWidth( num, fh2::GameFont::Size::SMALL ) ) / 2,
                               cy + iconH - 2, num, fh2::GameFont::Size::SMALL, fh2::GameFont::Color::WHITE );
            }
        }
        goto done;
    }

    if ( chip == "calendar" ) {
        // Only the upcoming events.
        int y = 8;
        int shown = 0;
        for ( const fh2::WorldEvent & e : world.events ) {
            if ( shown >= 5 )
                break;
            QString line = QStringLiteral( "Day %1 (+%2)" ).arg( e.day ).arg( e.period );
            if ( !e.title.empty() )
                line += QStringLiteral( ": %1" ).arg( fh2::decodeCp1251( e.title ) );
            body( y, line, fh2::GameFont::Color::GRAY );
            y += 20;
            ++shown;
        }
        if ( world.events.empty() )
            body( 8, QStringLiteral( "no events" ) );
        goto done;
    }

    if ( chip == "rumors" ) {
        int y = 8;
        int shown = 0;
        for ( const std::string & rumor : world.rumors ) {
            if ( shown >= 4 )
                break;
            const QString text = fh2::decodeCp1251( rumor );
            if ( font.valid() ) {
                // Wrap to the chip width with LEFT alignment (the shared
                // drawTextWrapped centers each line, which looks wrong here).
                const int lh = font.lineHeight( fh2::GameFont::Size::NORMAL );
                QStringList lines;
                for ( const QString & paragraph : text.split( QLatin1Char( '\n' ) ) ) {
                    QString line;
                    for ( const QString & word : paragraph.split( QLatin1Char( ' ' ) ) ) {
                        const QString candidate = line.isEmpty() ? word : line + QLatin1Char( ' ' ) + word;
                        if ( !line.isEmpty() && font.textWidth( candidate, fh2::GameFont::Size::NORMAL ) > w - 8 ) {
                            lines.push_back( line );
                            line = word;
                        }
                        else {
                            line = candidate;
                        }
                    }
                    lines.push_back( line );
                }
                for ( const QString & l : lines ) {
                    font.drawText( p, 4, y, l, fh2::GameFont::Size::NORMAL, fh2::GameFont::Color::WHITE );
                    y += lh;
                }
            }
            y += 4;
            ++shown;
        }
        if ( world.rumors.empty() )
            body( 8, QStringLiteral( "no rumors" ) );
        goto done;
    }

    if ( chip == "puzzle" ) {
        // Pick the kingdom whose puzzle to show: the selected color when given,
        // otherwise the one with the most revealed cells (the obelisk map).
        const fh2::WorldKingdom * k = nullptr;
        if ( selectedColor != 0 ) {
            for ( const fh2::WorldKingdom & kk : world.kingdoms )
                if ( kk.color == selectedColor ) {
                    k = &kk;
                    break;
                }
        }
        if ( !k ) {
            int bestOpen = -1;
            for ( const fh2::WorldKingdom & kk : world.kingdoms ) {
                int open = 0;
                for ( char c : kk.puzzleBits )
                    if ( c == '1' )
                        ++open;
                if ( open > bestOpen ) {
                    bestOpen = open;
                    k = &kk;
                }
            }
        }

        // Engine port (Puzzle::ShowMapsDialog / GenerateUltimateArtifactAreaSurface):
        // a 448x448 area of the world around the Ultimate Artifact in the TAN
        // palette (dark wood engraving), with the 48 shaped PUZZLE pieces overlaid;
        // open cells show the engraving (with the artifact marker), closed ones
        // are hidden behind the wooden pieces.
        constexpr int PW = 448;
        QImage panel( PW, PW, QImage::Format_ARGB32 );
        panel.fill( qRgba( 28, 22, 16, 255 ) );
        {
            QPainter pp( &panel );
            // TAN palette (engine/pal.cpp tanTable): engraving map color ids.
            static const uint8_t tanTable[256] = {
                213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 198, 198, 198, 198, 198, 198,
                198, 198, 198, 198, 198, 198, 198, 200, 201, 203, 204, 206, 207, 208, 209, 210,
                211, 213, 213, 213, 213, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198,
                198, 198, 198, 199, 200, 201, 203, 204, 205, 207, 207, 209, 210, 211, 212, 198,
                198, 198, 198, 198, 201, 203, 205, 207, 208, 210, 210, 211, 212, 212, 213, 213,
                213, 213, 213, 213, 213, 198, 198, 198, 199, 201, 203, 204, 206, 207, 208, 209,
                210, 211, 212, 213, 213, 213, 213, 213, 213, 213, 213, 213, 198, 198, 198, 198,
                198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 200, 201, 203,
                204, 206, 207, 198, 198, 198, 198, 198, 198, 198, 198, 198, 200, 201, 203, 204,
                206, 207, 208, 209, 210, 212, 213, 213, 198, 198, 198, 198, 198, 198, 198, 198,
                198, 198, 198, 198, 199, 201, 203, 204, 206, 207, 208, 209, 211, 212, 213, 198,
                198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198,
                199, 201, 202, 204, 206, 207, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198,
                198, 198, 198, 199, 202, 205, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198,
                198, 200, 203, 204, 207, 208, 209, 201, 203, 207, 209, 206, 209, 208, 198, 198,
                207, 213, 198, 201, 206, 208, 201, 203, 207, 206, 198, 198, 198, 198, 213, 213,
            };
            const auto engrave = [&]( const QImage & idximg, int dstX, int dstY, bool flipH, bool flipV ) {
                const int w32 = idximg.width(), h32 = idximg.height();
                for ( int y = 0; y < h32; ++y ) {
                    for ( int x = 0; x < w32; ++x ) {
                        const int id = idximg.pixelIndex( flipH ? h32 - 1 - x : x, flipV ? w32 - 1 - y : y );
                        if ( id >= 0xFF ) // 255 = transparent
                            continue;
                        pp.fillRect( dstX + x, dstY + y, 1, 1, _a.paletteColor( tanTable[id] ) );
                    }
                }
            };
            bool hasSurface = false;
            if ( world.ultimateArtifactIndex >= 0 && !world.tiles.empty() ) {
                const int cx = ( world.ultimateArtifactIndex % world.width ) * 32 + 16;
                const int cy = ( world.ultimateArtifactIndex / world.width ) * 32 + 16;
                const int x0 = cx - PW / 2, y0 = cy - PW / 2;
                for ( int py = 0; py < PW; py += 32 ) {
                    for ( int px = 0; px < PW; px += 32 ) {
                        const int gx = ( x0 + px ) >> 5;
                        const int gy = ( y0 + py ) >> 5;
                        if ( gx < 0 || gy < 0 || gx >= world.width || gy >= world.height )
                            continue;
                        const fh2::WorldTile & t = world.tiles[static_cast<size_t>( gy ) * world.width + gx];
                        const fh2::IcnSprite & sp = _a.tilSprite( "GROUND32.TIL", t.terrainImageIndex );
                        if ( sp.idx.isNull() )
                            continue;
                        const int shape = t.terrainFlags & 0x3;
                        // Mirror like the map does, then engrave.
                        engrave( sp.idx, px, py, ( shape & 2 ) != 0, ( shape & 1 ) != 0 );
                        hasSurface = true;
                    }
                }
                // The artifact marker (ROUTE[0]) at the center, engraved too.
                const fh2::IcnSprite & marker = _a.icnSprite( "ROUTE.ICN", 0 );
                if ( !marker.idx.isNull() ) {
                    const int mx = PW / 2 + 8, my = PW / 2 + 8;
                    for ( int y = 0; y < marker.idx.height() && my + y < PW; ++y ) {
                        for ( int x = 0; x < marker.idx.width() && mx + x < PW; ++x ) {
                            const int id = marker.idx.pixelIndex( x, y );
                            if ( id >= 0xFF )
                                continue;
                            pp.fillRect( mx + x, my + y, 1, 1, _a.paletteColor( tanTable[id] ) );
                        }
                    }
                }
            }
            (void)hasSurface;
        }
        // Now the shaped wooden pieces; the open cells (puzzleBits == '1') stay
        // visible, the closed ones are covered by their piece.
        for ( int i = 0; i < 48; ++i ) {
            bool open = false;
            if ( k && i < static_cast<int>( k->puzzleBits.size() ) )
                open = ( k->puzzleBits[i] == '1' );
            if ( open )
                continue;
            const fh2::IcnSprite & piece = _a.icnSprite( "PUZZLE.ICN", i );
            if ( piece.image.isNull() )
                continue;
            const int px = piece.offsetX - 16;
            const int py = piece.offsetY - 16;
            QPainter pp( &panel );
            pp.drawImage( px, py, piece.image );
        }
        // Fit the polished panel into the chip, keep it square (no larger than
        // the minimap) and flush with the chip edges.
        const int availH = h;
        const int availW = w;
        const int side = std::max( 1, std::min( std::min( availW, availH ), PW ) );
        const QImage small = panel.scaled( side, side, Qt::IgnoreAspectRatio, Qt::FastTransformation );
        p.drawImage( ( availW - side ) / 2, ( availH - side ) / 2, small );
        goto done;
    }

    if ( chip == "victory" ) {
        // Decode the fheroes2 Victory/LossCondition enums (maps_fileinfo.h);
        // HoMM2 saves come through with the original condition type, which for
        // the common cases (defeat everyone / obtain artifact / accumulate
        // gold / lose everything / lose town / lose hero) matches these values.
        const auto victoryName = [&]( int t ) -> QString {
            switch ( t ) {
            case 0: return QStringLiteral( "Defeat everyone" );
            case 1: return QStringLiteral( "Capture a town" );
            case 2: return QStringLiteral( "Kill the champion hero" );
            case 3: return QStringLiteral( "Obtain an artifact" );
            case 4: return QStringLiteral( "Defeat the other side" );
            case 5: return QStringLiteral( "Collect gold" );
            default: return QStringLiteral( "type %1" ).arg( t );
            }
        };
        const auto lossName = [&]( int t ) -> QString {
            switch ( t ) {
            case 0: return QStringLiteral( "Lose everything" );
            case 1: return QStringLiteral( "Lose a town" );
            case 2: return QStringLiteral( "Lose a hero" );
            case 3: return QStringLiteral( "Run out of time" );
            default: return QStringLiteral( "type %1" ).arg( t );
            }
        };
        QString win = victoryName( mapInfo.victoryConditionType );
        if ( mapInfo.victoryConditionType == 5 && mapInfo.victoryConditionParams[0] > 0 )
            win += QStringLiteral( " (%1)" ).arg( mapInfo.victoryConditionParams[0] * 1000 );
        body( 8, QStringLiteral( "Win: %1" ).arg( win ) );
        QString loss = lossName( mapInfo.lossConditionType );
        if ( mapInfo.lossConditionType == 3 && mapInfo.lossConditionParams[0] > 0 )
            loss += QStringLiteral( " (in %1 days)" ).arg( mapInfo.lossConditionParams[0] );
        body( 32, QStringLiteral( "Lose: %1" ).arg( loss ) );
        body( 56, QStringLiteral( "Difficulty: %1    Players: %2" ).arg( mapInfo.difficulty ).arg( mapInfo.kingdomMax ) );
        goto done;
    }

    body( 40, QStringLiteral( "chip: %1" ).arg( QString::fromStdString( chip ) ) );
    done:
    p.end();
    // Text chips: the first line always starts at y = 8, so pin the crop top to
    // 8 - pad. Otherwise the crop would follow the tallest glyph of each chip
    // and lines with different ascenders would not line up in the info block.
    const bool textChip = ( chip == "calendar" || chip == "rumors" || chip == "victory" );
    return cropContent( img, textChip ? 4 : -1 );
}

QImage InfoChips::renderInfo( const fh2::WorldData & world, const fh2::MapInfo & mapInfo, int selectedColor, int colW, const std::set<std::string> & enabledNames ) const
{
    const auto enabled = [&]( const std::string & n ) {
        return enabledNames.empty() || enabledNames.count( n ) != 0;
    };
    // Top row: resources + victory; bottom row: events + rumors.
    const struct { const char * left; const char * right; } rows[2] = {
        { "resources", "victory" },
        { "calendar", "rumors" },
    };
    QImage cells[2][2];
    for ( int r = 0; r < 2; ++r ) {
        if ( enabled( rows[r].left ) )
            cells[r][0] = renderChip( rows[r].left, world, mapInfo, selectedColor, colW, 2000 );
        if ( enabled( rows[r].right ) )
            cells[r][1] = renderChip( rows[r].right, world, mapInfo, selectedColor, colW, 2000 );
    }
    const auto h0 = [&]( int r, int c ) { return cells[r][c].isNull() ? 0 : cells[r][c].height(); };
    const auto w0 = [&]( int r, int c ) { return cells[r][c].isNull() ? 0 : cells[r][c].width(); };
    const int col0W = std::max( w0( 0, 0 ), w0( 1, 0 ) );
    const int col1W = std::max( w0( 0, 1 ), w0( 1, 1 ) );
    const bool twoCols = col0W > 0 && col1W > 0;
    if ( col0W + col1W <= 0 )
        return {};
    const int row0H = std::max( h0( 0, 0 ), h0( 0, 1 ) );
    const int row1H = std::max( h0( 1, 0 ), h0( 1, 1 ) );
    const bool twoRows = row0H > 0 && row1H > 0;
    constexpr int GX = 14, GY = 14;
    QImage out( twoCols ? col0W + GX + col1W : col0W + col1W, twoRows ? row0H + GY + row1H : row0H + row1H, QImage::Format_ARGB32 );
    out.fill( Qt::transparent );
    QPainter p( &out );
    const int yR0 = 0;
    const int yR1 = twoRows ? row0H + GY : 0;
    // Left column: resources on top, events below it.
    if ( twoCols || !twoRows ) {
        if ( !cells[0][0].isNull() )
            p.drawImage( 0, yR0, cells[0][0] );
        else
            p.drawImage( 0, yR0, cells[1][0] );
        if ( twoRows && !cells[1][0].isNull() )
            p.drawImage( 0, yR1, cells[1][0] );
    }
    // Right column: victory on top, rumors below.
    if ( twoCols ) {
        if ( !cells[0][1].isNull() )
            p.drawImage( col0W + GX, yR0, cells[0][1] );
        else
            p.drawImage( col0W + GX, yR0, cells[1][1] );
        if ( twoRows && !cells[1][1].isNull() )
            p.drawImage( col0W + GX, yR1, cells[1][1] );
    }
    p.end();
    return cropContent( out );
}

} // namespace fh2poster
