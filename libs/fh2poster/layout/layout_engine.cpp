#include "layout_engine.h"

#include <algorithm>
#include <cmath>

namespace layout {

namespace {

struct GridPick
{
    int k = 0;       // grid columns
    int rows = 0;
    int w = 0;       // element width (fallback; per-element widths override)
    int h = 0;       // element height (fallback; per-element heights override)
    int content = 0; // grid extent along the zone thickness (with padding)
    std::vector<int> widths;  // per-element widths (scale px); empty = uniform w
    std::vector<int> heights; // per-element heights (scale px); empty = uniform h
};

// Picks the fixed-size grid. Each card keeps its own width/height (from
// fixedWList/fixedHList or the fixedW/fixedH defaults); with per-element widths
// the cards pack a single column, otherwise columns are chosen to fill the zone.
GridPick pickFixed( const GridParams & g, int count, int zoneLen, int maxExtent, bool horizontal, std::vector<std::string> & warnings,
                    const std::string & zoneId, int scale )
{
    GridPick best;
    const int pad2 = g.padding * 2;
    const bool perW = !g.fixedWList.empty();
    const bool perH = !g.fixedHList.empty();
    const int w = std::max( 1, g.fixedW * scale );
    const int h = std::max( 1, g.fixedH * scale );
    std::vector<int> ws, hs;
    int wmax = w, hmax = h;
    if ( perW ) {
        ws.resize( count );
        for ( int i = 0; i < count; ++i ) {
            ws[i] = w;
            if ( i < static_cast<int>( g.fixedWList.size() ) && g.fixedWList[i] > 0 )
                ws[i] = std::max( 1, g.fixedWList[i] * scale );
            wmax = std::max( wmax, ws[i] );
        }
    }
    if ( perH ) {
        hs.resize( count );
        for ( int i = 0; i < count; ++i ) {
            hs[i] = h;
            if ( i < static_cast<int>( g.fixedHList.size() ) && g.fixedHList[i] > 0 )
                hs[i] = std::max( 1, g.fixedHList[i] * scale );
            hmax = std::max( hmax, hs[i] );
        }
    }
    const auto slotH = [&]( int i ) { return perH ? hs[i] : h; };

    int k = 0;
    if ( perW ) {
        k = 1; // one column of independently-sized cards
    }
    else {
        for ( int kk = 1; kk <= count; ++kk ) {
            const int need = kk * w + ( kk - 1 ) * g.gap + pad2;
            if ( need <= zoneLen )
                k = kk;
            else
                break;
        }
    }
    if ( k <= 0 ) {
        k = 1;
        if ( w > zoneLen - pad2 )
            warnings.push_back( "zone '" + zoneId + "': element is wider than the zone length; reduce the map scale" );
    }
    const int rows = ( count + k - 1 ) / k;

    std::vector<int> rowH( rows, 1 );
    for ( int r = 0; r < rows; ++r ) {
        const int start = r * k;
        const int end = std::min( count, start + k );
        int rh = 1;
        for ( int i = start; i < end; ++i )
            rh = std::max( rh, slotH( i ) );
        rowH[r] = rh;
    }

    long long content = pad2;
    for ( int r = 0; r < rows; ++r ) {
        content += rowH[r];
        if ( r < rows - 1 )
            content += g.gap;
    }
    if ( !horizontal && content > maxExtent )
        content = maxExtent;

    best.k = k;
    best.rows = rows;
    best.w = wmax;
    best.h = hmax;
    best.content = static_cast<int>( content );
    best.widths = std::move( ws );
    best.heights = std::move( hs );
    return best;
}

// Picks the grid maximizing the element width.
// horizontal: the grid fills rows along the zone thickness; the thickness is capped by maxZoneFrac.
// vertical: the grid fills columns; its height must fit into maxExtent (the available stack space).
GridPick pickGrid( const GridParams & g, int count, int zoneLen, int maxExtent, bool horizontal, std::vector<std::string> & warnings,
                   const std::string & zoneId, int scale = 1 )
{
    GridPick best;
    const int pad2 = g.padding * 2;

    // Constant-size grid: keep the element at fixedW x fixedH * scale and only pick
    // how many columns fit the zone length. Cards no longer grow/shrink with the map
    // size — they scale with the poster scale only.
    if ( g.fixedW > 0 && ( g.fixedH > 0 || !g.fixedHList.empty() ) ) {
        return pickFixed( g, count, zoneLen, maxExtent, horizontal, warnings, zoneId, scale );
    }

    const int cap = ( horizontal && g.maxZoneFrac > 0 ) ? static_cast<int>( g.maxZoneFrac * zoneLen ) : 0;

    for ( int k = 1; k <= count; ++k ) {
        const int rows = ( count + k - 1 ) / k;
        if ( rows > g.maxRows )
            continue;

        int w = ( zoneLen - pad2 - g.gap * ( k - 1 ) ) / k;
        if ( g.capW > 0 )
            w = std::min( w, g.capW );
        if ( w < g.floorW )
            continue;

        int h = static_cast<int>( std::lround( w * g.aspect ) );
        int extent = rows * h + ( rows - 1 ) * g.gap + pad2;

        if ( horizontal ) {
            // The cap limits the grid content itself; zone padding adds on top of it.
            if ( cap > 0 && extent - pad2 > cap )
                continue;
        }
        else if ( extent > maxExtent ) {
            h = ( maxExtent - pad2 - ( rows - 1 ) * g.gap ) / rows;
            if ( h <= 0 )
                continue;
            w = static_cast<int>( std::lround( h / g.aspect ) );
            if ( w < g.floorW )
                continue;
            extent = rows * h + ( rows - 1 ) * g.gap + pad2;
        }

        if ( best.k == 0 || w > best.w || ( w == best.w && extent < best.content ) )
            best = GridPick{ k, rows, w, h, extent };
    }

    if ( best.k == 0 ) {
        // Second pass: the zone limit prevented any grid from fitting (typically
        // a small number of capped elements on a small map). Repeat without the
        // zone limit — the cap itself keeps the zone reasonable.
        for ( int k = 1; k <= count; ++k ) {
            const int rows = ( count + k - 1 ) / k;
            if ( rows > g.maxRows )
                continue;
            int w = ( zoneLen - pad2 - g.gap * ( k - 1 ) ) / k;
            if ( g.capW > 0 )
                w = std::min( w, g.capW );
            if ( w < g.floorW )
                continue;
            const int h = static_cast<int>( std::lround( w * g.aspect ) );
            const int extent = rows * h + ( rows - 1 ) * g.gap + pad2;
            if ( best.k == 0 || w > best.w || ( w == best.w && extent < best.content ) )
                best = GridPick{ k, rows, w, h, extent };
        }
    }

    if ( best.k == 0 ) {
        // Fallback: fit as many columns as possible; the floor is violated.
        const int k = std::max( 1, std::min( count, 6 ) );
        const int rows = ( count + k - 1 ) / k;
        int w = std::max( 1, ( zoneLen - pad2 - g.gap * ( k - 1 ) ) / k );
        if ( g.capW > 0 )
            w = std::min( w, g.capW );
        int h = std::max( 1, static_cast<int>( std::lround( w * g.aspect ) ) );
        int extent = rows * h + ( rows - 1 ) * g.gap + pad2;
        if ( !horizontal && extent > maxExtent ) {
            h = std::max( 1, ( maxExtent - pad2 - ( rows - 1 ) * g.gap ) / rows );
            w = std::max( 1, static_cast<int>( std::lround( h / g.aspect ) ) );
            extent = rows * h + ( rows - 1 ) * g.gap + pad2;
        }
        if ( g.floorW > 0 )
            warnings.push_back( "zone '" + zoneId + "': elements are smaller than the readable minimum (" + std::to_string( g.floorW )
                                + " px); reduce the map scale or the element count" );
        best = GridPick{ k, rows, w, h, extent };
    }

    return best;
}

} // namespace

LayoutResult computeLayout( const LayoutParams & params, int mapPx, const std::map<std::string, int> & counts )
{
    LayoutResult res;
    if ( mapPx <= 0 ) {
        res.warnings.push_back( "map size is not positive" );
        return res;
    }

    struct ZoneGeom
    {
        ZoneDef def;
        int count = 0;
        int thick = 0; // zone thickness (with padding, without the inter-zone gap)
        GridPick pick;
    };

    std::vector<ZoneGeom> topZones, bottomZones, leftZones, rightZones;
    for ( const ZoneDef & z : params.zones ) {
        ZoneGeom zg;
        zg.def = z;
        const auto it = counts.find( z.id );
        zg.count = ( it != counts.end() ) ? it->second : 0;
        if ( z.single ) {
            zg.thick = ( z.singleFixedH > 0 ) ? z.singleFixedH * params.scale
                                              : std::max( 1, static_cast<int>( std::lround( mapPx * z.singleAspect ) ) );
        }
        else if ( zg.count > 0 ) {
            const bool horizontal = ( z.side == Side::TOP || z.side == Side::BOTTOM );
            // Vertical zones are picked later, when the side width is known.
            if ( horizontal ) {
                zg.pick = pickGrid( z.grid, zg.count, mapPx, 0, true, res.warnings, z.id, params.scale );
                zg.thick = zg.pick.content;
            }
        }
        else {
            continue; // disabled or empty zone
        }
        switch ( z.side ) {
        case Side::TOP: topZones.push_back( zg ); break;
        case Side::BOTTOM: bottomZones.push_back( zg ); break;
        case Side::LEFT: leftZones.push_back( zg ); break;
        case Side::RIGHT: rightZones.push_back( zg ); break;
        }
    }

    const int zgap = 8;
    int topTotal = 0, bottomTotal = 0;
    for ( const ZoneGeom & zg : topZones )
        topTotal += zg.thick + zgap;
    for ( const ZoneGeom & zg : bottomZones )
        bottomTotal += zg.thick + zgap;

    const int canvasH = params.frame + topTotal + mapPx + bottomTotal + params.frame;
    // Side width: accommodate the widest constant-size side card (+ padding), but
    // never narrower than the side-min fraction of the map. Cards are constant, so
    // this does not grow with the map size.
    int sideNeed = 0;
    const auto zoneWidth = [&]( const ZoneGeom & zg ) {
        if ( zg.def.grid.fixedW <= 0 )
            return 0;
        int wmax = zg.def.grid.fixedW;
        for ( const int w : zg.def.grid.fixedWList )
            wmax = std::max( wmax, w );
        return wmax * params.scale;
    };
    for ( const ZoneGeom & zg : leftZones )
        sideNeed = std::max( sideNeed, zoneWidth( zg ) );
    for ( const ZoneGeom & zg : rightZones )
        sideNeed = std::max( sideNeed, zoneWidth( zg ) );
    if ( sideNeed > 0 )
        sideNeed += 2 * 6 + 2 * 8 + 8; // padding + inset + gap margins
    int sideW = std::max( static_cast<int>( std::lround( params.sideMinFrac * mapPx ) ), sideNeed );
    int leftW = sideW;
    int rightW = sideW;
    if ( params.sideWidth > 0 ) {
        rightW = params.sideWidth;
        leftW = 0;
    }
    const int canvasW = params.frame + leftW + mapPx + rightW + params.frame;

    // Vertical zones stack top-down; each zone is picked within the remaining height.
    // In a column layout (a fixed side width) the side column may exceed the map, so
    // the poster grows vertically and the map stays centered.
    auto pickStack = [&]( std::vector<ZoneGeom> & zones, int zoneLen, int zoneX ) {
        int avail = ( params.sideWidth > 0 ) ? 100000 : mapPx;
        for ( ZoneGeom & zg : zones ) {
            if ( zg.def.single ) {
                zg.thick = std::min( zg.thick, avail );
                avail -= zg.thick + zgap;
                continue;
            }
            zg.pick = pickGrid( zg.def.grid, zg.count, zoneLen, avail, false, res.warnings, zg.def.id, params.scale );
            avail -= zg.pick.content + zgap;
        }
        (void)zoneX;
    };
    pickStack( leftZones, leftW, 0 );
    pickStack( rightZones, rightW, 0 );

    // Let the poster grow if a side stack is taller than the map (left and right
    // are separate columns — take the taller one).
    const auto sideSum = [&]( const std::vector<ZoneGeom> & zs ) {
        int total = 0;
        for ( const ZoneGeom & zg : zs )
            if ( !zg.def.single )
                total += zg.pick.content + zgap;
        return total;
    };
    const int sideOver = std::max( 0, std::max( sideSum( leftZones ), sideSum( rightZones ) ) - mapPx );
    int canvasH2 = canvasH + sideOver;

    res.canvasW = canvasW;
    res.canvasH = canvasH2;
    const int mapX = params.frame + leftW;
    const int mapY = params.frame + topTotal + sideOver / 2;
    res.map = Rect{ mapX, mapY, mapPx, mapPx };

    auto placeHorizontal = [&]( std::vector<ZoneGeom> & zones, int yCursor ) {
        for ( ZoneGeom & zg : zones ) {
            if ( zg.def.single ) {
                res.blocks[zg.def.id].push_back( Rect{ mapX, yCursor, mapPx, zg.thick } );
                yCursor += zg.thick + zgap;
                continue;
            }
            const GridParams & g = zg.def.grid;
            const int rows = zg.pick.rows;
            const bool perW = !zg.pick.widths.empty();
            const bool perEl = !zg.pick.heights.empty() || perW;
            std::vector<int> rowH( rows, 1 );
            for ( int r = 0; r < rows; ++r ) {
                const int start = r * zg.pick.k;
                const int end = std::min( zg.count, start + zg.pick.k );
                int rh = 1;
                for ( int i = start; i < end; ++i )
                    rh = std::max( rh, perEl ? zg.pick.heights[i] : zg.pick.h );
                rowH[r] = rh;
            }
            long long rowTotal = g.padding * 2;
            for ( int r = 0; r < rows; ++r ) {
                rowTotal += rowH[r];
                if ( r < rows - 1 )
                    rowTotal += g.gap;
            }
            int y = yCursor + g.padding + std::max<int>( 0, ( zg.thick - static_cast<int>( rowTotal ) ) / 2 );
            int placed = 0;
            for ( int r = 0; r < rows && placed < zg.count; ++r ) {
                const int inRow = std::min( zg.pick.k, zg.count - placed );
                const int rowW = inRow * zg.pick.w + ( inRow - 1 ) * g.gap;
                const int x0 = mapX + ( mapPx - rowW ) / 2;
                for ( int i = 0; i < inRow; ++i ) {
                    const int idx = placed + i;
                    const int sw = perW ? zg.pick.widths[idx] : zg.pick.w;
                    const int sh = perEl ? zg.pick.heights[idx] : zg.pick.h;
                    const int x = perW ? mapX + ( mapPx - sw ) / 2 : x0 + i * ( zg.pick.w + g.gap );
                    res.blocks[zg.def.id].push_back( Rect{ x, y, sw, sh } );
                }
                placed += inRow;
                y += rowH[r] + g.gap;
            }
            yCursor += zg.thick + zgap;
        }
    };
    placeHorizontal( topZones, params.frame );
    placeHorizontal( bottomZones, mapY + mapPx );

    auto placeVertical = [&]( std::vector<ZoneGeom> & zones, int zoneX, int zoneLen, bool mapOnRight ) {
        int yCursor = mapY;
        for ( ZoneGeom & zg : zones ) {
            if ( zg.def.single ) {
                res.blocks[zg.def.id].push_back( Rect{ zoneX, yCursor, zoneLen, zg.thick } );
                yCursor += zg.thick + zgap;
                continue;
            }
            const GridParams & g = zg.def.grid;
            const int rows = zg.pick.rows;
            const bool perW = !zg.pick.widths.empty();
            const bool perEl = !zg.pick.heights.empty() || perW;
            std::vector<int> rowH( rows, 1 );
            for ( int r = 0; r < rows; ++r ) {
                const int start = r * zg.pick.k;
                const int end = std::min( zg.count, start + zg.pick.k );
                int rh = 1;
                for ( int i = start; i < end; ++i )
                    rh = std::max( rh, perEl ? zg.pick.heights[i] : zg.pick.h );
                rowH[r] = rh;
            }
            int y = yCursor + g.padding;
            int placed = 0;
            for ( int r = 0; r < rows && placed < zg.count; ++r ) {
                const int inRow = std::min( zg.pick.k, zg.count - placed );
                for ( int i = 0; i < inRow; ++i ) {
                    const int idx = placed + i;
                    const int sw = perW ? zg.pick.widths[idx] : zg.pick.w;
                    const int sh = perEl ? zg.pick.heights[idx] : zg.pick.h;
                    // Per-element-width cards hug the map: on a left zone the map is
                    // to the right (align right), on a right zone it is to the left
                    // (align left = the zone's left edge).
                    int x;
                    if ( perW )
                        x = mapOnRight ? zoneX + zoneLen - sw - g.padding : zoneX + g.padding;
                    else
                        x = zoneX + g.padding + i * ( zg.pick.w + g.gap );
                    res.blocks[zg.def.id].push_back( Rect{ x, y, sw, sh } );
                }
                placed += inRow;
                y += rowH[r] + g.gap;
            }
            yCursor += zg.pick.content + zgap;
        }
    };
    placeVertical( leftZones, params.frame, leftW, true );
    placeVertical( rightZones, params.frame + leftW + mapPx, rightW, false );

    return res;
}

// Specialized in-game "cartouche" layout. The square map sits on top; below it a
// fixed row of info chips; then, per player color (kingdom), a header band, the
// hero cards and (optionally) the castle renders — growing downward.
LayoutResult computeCartoucheLayout( const CartoucheInput & in )
{
    LayoutResult res;
    if ( in.mapPx <= 0 ) {
        res.warnings.push_back( "map size is not positive" );
        return res;
    }

    const int M = in.frame; // frame thickness: the content sits flush against it
    const int contentW = in.mapPx;
    const int contentX = M;
    int y = M;

    // Title band (the day / week string), if enabled: centered between the frame
    // and the map.
    if ( in.hasTitle ) {
        const int th = std::max( 34, static_cast<int>( in.mapPx * 0.02 ) );
        const int pad = 12;
        res.blocks["title"].push_back( Rect{ contentX, y + pad, contentW, th } );
        y += pad + th + pad;
    }

    // The map (the frame is drawn around it by the renderer), flush with the
    // frame on the left and right.
    res.map = Rect{ contentX, y, contentW, in.mapPx };
    y += in.mapPx;

    // Info chips: one row below the map's separator strip, flush with the frame
    // on the left and right (the first chip hugs the left edge, the last one the
    // right edge). The surrounding frame is drawn by the renderer.
    {
        const int n = static_cast<int>( in.chipSizes.size() );
        int cx = contentX;
        int rowH = 0;
        for ( int i = 0; i < n; ++i ) {
            const layout::Size & s = in.chipSizes[i];
            const int x = ( i + 1 == n && n > 1 ) ? ( contentX + contentW - s.w ) : cx;
            res.blocks["chips"].push_back( Rect{ x, y + 16, s.w, s.h } );
            cx = x + s.w + in.chipGap;
            rowH = std::max( rowH, s.h );
        }
        y += 16 + rowH;
    }

    // Kingdom groups: header band, then hero cards then castle renders, all
    // left-aligned (cards never centered), flush with the frame on the left.
    const auto fitPerRow = [&]( int cardW ) {
        return std::max( 1, ( contentW + in.cardGap ) / ( cardW + in.cardGap ) );
    };
    const auto rowWidth = [&]( int count, int cardW ) {
        if ( count <= 0 )
            return 0;
        const int per = fitPerRow( cardW );
        const int inRow = std::min( per, count );
        return inRow * cardW + ( inRow - 1 ) * in.cardGap;
    };
    for ( const CartoucheInput::Group & g : in.groups ) {
        if ( g.heroCount <= 0 && g.castleCount <= 0 )
            continue;
        const int heroPer = fitPerRow( in.heroCard.w );
        const int castlePer = fitPerRow( in.castleCard.w );
        const int groupW = std::max( rowWidth( g.heroCount, in.heroCard.w ), rowWidth( g.castleCount, in.castleCard.w ) );
        const int gh = std::max( 34, in.heroCard.h / 4 );
        const int gx = contentX;

        // The separator strip between the previous block and this header sits in
        // the gap right above the header.
        const Rect header{ gx, y + 16, groupW, gh };
        int yy = y + 16 + gh + in.cardGap;

        // Hero rows (left-aligned).
        int placed = 0;
        while ( placed < g.heroCount ) {
            const int inRow = std::min( heroPer, g.heroCount - placed );
            for ( int i = 0; i < inRow; ++i ) {
                res.blocks["heroes"].push_back( Rect{ gx + i * ( in.heroCard.w + in.cardGap ), yy, in.heroCard.w, in.heroCard.h } );
            }
            placed += inRow;
            yy += in.heroCard.h + in.cardGap;
        }

        // Castle rows (left-aligned).
        placed = 0;
        while ( placed < g.castleCount ) {
            const int inRow = std::min( castlePer, g.castleCount - placed );
            for ( int i = 0; i < inRow; ++i ) {
                res.blocks["castles"].push_back( Rect{ gx + i * ( in.castleCard.w + in.cardGap ), yy, in.castleCard.w, in.castleCard.h } );
            }
            placed += inRow;
            yy += in.castleCard.h + in.cardGap;
        }

        const int contentBottom = ( g.castleCount > 0 || g.heroCount > 0 ) ? yy - in.cardGap : ( y + 16 );
        const Rect body{ gx, y, groupW, contentBottom - y };
        res.groups.push_back( GroupRect{ g.color, header, body } );
        y = contentBottom;
    }

    res.canvasW = contentW + 2 * M;
    res.canvasH = y + M;
    return res;
}

} // namespace layout
