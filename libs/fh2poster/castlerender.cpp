#include "castlerender.h"

#include <QPainter>

#include "gamefont.h"
#include "textutil.h"
#include <cstdint>
#include <cstdio>
#include <vector>

namespace fh2poster {

namespace {

// Building bit flags (fheroes2 BuildingType).
enum Build : uint32_t {
    B_THIEVESGUILD = 0x00000001,
    B_TAVERN = 0x00000002,
    B_SHIPYARD = 0x00000004,
    B_WELL = 0x00000008,
    B_STATUE = 0x00000010,
    B_LEFTTURRET = 0x00000020,
    B_RIGHTTURRET = 0x00000040,
    B_MARKETPLACE = 0x00000080,
    B_WEL2 = 0x00000100,
    B_MOAT = 0x00000200,
    B_SPEC = 0x00000400,
    B_CASTLE = 0x00000800,
    B_CAPTAIN = 0x00001000,
    B_SHRINE = 0x00002000,
    B_MAGEGUILD1 = 0x00004000,
    B_MAGEGUILD2 = 0x00008000,
    B_MAGEGUILD3 = 0x00010000,
    B_MAGEGUILD4 = 0x00020000,
    B_MAGEGUILD5 = 0x00040000,
    B_TENT = 0x00080000,
    D_MONSTER1 = 0x00100000,
    D_MONSTER2 = 0x00200000,
    D_MONSTER3 = 0x00400000,
    D_MONSTER4 = 0x00800000,
    D_MONSTER5 = 0x01000000,
    D_MONSTER6 = 0x02000000,
    D_UPGRADE2 = 0x04000000,
    D_UPGRADE3 = 0x08000000,
    D_UPGRADE4 = 0x10000000,
    D_UPGRADE5 = 0x20000000,
    D_UPGRADE6 = 0x40000000,
    D_UPGRADE7 = 0x80000000
};

// Race bitmasks.
enum Race : uint32_t {
    R_KNGT = 0x01,
    R_BARB = 0x02,
    R_SORC = 0x04,
    R_WRLK = 0x08,
    R_WZRD = 0x10,
    R_NECR = 0x20
};

const char * townBkgIcn( uint32_t race )
{
    switch ( race ) {
    case R_KNGT: return "TOWNBKG0.ICN";
    case R_BARB: return "TOWNBKG1.ICN";
    case R_SORC: return "TOWNBKG2.ICN";
    case R_WRLK: return "TOWNBKG3.ICN";
    case R_WZRD: return "TOWNBKG4.ICN";
    case R_NECR: return "TOWNBKG5.ICN";
    default: return nullptr;
    }
}

// Port of Castle::GetICNBuilding (castle.cpp); the ICN enum names are the AGG file names.
const char * buildingIcn( uint32_t building, uint32_t race )
{
    if ( race == R_BARB ) {
        switch ( building ) {
        case B_CASTLE: return "TWNBCSTL.ICN";
        case B_TENT: return "TWNBTENT.ICN";
        case B_SPEC: return "TWNBSPEC.ICN";
        case B_CAPTAIN: return "TWNBCAPT.ICN";
        case B_WEL2: return "TWNBWEL2.ICN";
        case B_LEFTTURRET: return "TWNBLTUR.ICN";
        case B_RIGHTTURRET: return "TWNBRTUR.ICN";
        case B_MOAT: return "TWNBMOAT.ICN";
        case B_MARKETPLACE: return "TWNBMARK.ICN";
        case B_THIEVESGUILD: return "TWNBTHIE.ICN";
        case B_TAVERN: return "TWNBTVRN.ICN";
        case B_WELL: return "TWNBWELL.ICN";
        case B_STATUE: return "TWNBSTAT.ICN";
        case B_SHIPYARD: return "TWNBDOCK.ICN";
        case B_MAGEGUILD1:
        case B_MAGEGUILD2:
        case B_MAGEGUILD3:
        case B_MAGEGUILD4:
        case B_MAGEGUILD5: return "TWNBMAGE.ICN";
        case D_MONSTER1: return "TWNBDW_0.ICN";
        case D_MONSTER2: return "TWNBDW_1.ICN";
        case D_UPGRADE2: return "TWNBUP_1.ICN";
        case D_MONSTER3: return "TWNBDW_2.ICN";
        case D_MONSTER4: return "TWNBDW_3.ICN";
        case D_UPGRADE4: return "TWNBUP_3.ICN";
        case D_MONSTER5: return "TWNBDW_4.ICN";
        case D_UPGRADE5: return "TWNBUP_4.ICN";
        case D_MONSTER6: return "TWNBDW_5.ICN";
        default: break;
        }
    }
    else if ( race == R_KNGT ) {
        switch ( building ) {
        case B_CASTLE: return "TWNKCSTL.ICN";
        case B_TENT: return "TWNKTENT.ICN";
        case B_SPEC: return "TWNKSPEC.ICN";
        case B_CAPTAIN: return "TWNKCAPT.ICN";
        case B_WEL2: return "TWNKWEL2.ICN";
        case B_LEFTTURRET: return "TWNKLTUR.ICN";
        case B_RIGHTTURRET: return "TWNKRTUR.ICN";
        case B_MOAT: return "TWNKMOAT.ICN";
        case B_MARKETPLACE: return "TWNKMARK.ICN";
        case B_THIEVESGUILD: return "TWNKTHIE.ICN";
        case B_TAVERN: return "TWNKTVRN.ICN";
        case B_WELL: return "TWNKWELL.ICN";
        case B_STATUE: return "TWNKSTAT.ICN";
        case B_SHIPYARD: return "TWNKDOCK.ICN";
        case B_MAGEGUILD1:
        case B_MAGEGUILD2:
        case B_MAGEGUILD3:
        case B_MAGEGUILD4:
        case B_MAGEGUILD5: return "TWNKMAGE.ICN";
        case D_MONSTER1: return "TWNKDW_0.ICN";
        case D_MONSTER2: return "TWNKUP_1.ICN";
        case D_UPGRADE2: return "TWNKDW_1.ICN";
        case D_MONSTER3: return "TWNKDW_2.ICN";
        case D_UPGRADE3: return "TWNKUP_2.ICN";
        case D_MONSTER4: return "TWNKDW_3.ICN";
        case D_UPGRADE4: return "TWNKUP_3.ICN";
        case D_MONSTER5: return "TWNKDW_4.ICN";
        case D_UPGRADE5: return "TWNKUP_4.ICN";
        case D_MONSTER6: return "TWNKDW_5.ICN";
        case D_UPGRADE6: return "TWNKUP_5.ICN";
        default: break;
        }
    }
    else if ( race == R_NECR ) {
        switch ( building ) {
        case B_CASTLE: return "TWNNCSTL.ICN";
        case B_TENT: return "TWNNTENT.ICN";
        case B_SPEC: return "TWNNSPEC.ICN";
        case B_CAPTAIN: return "TWNNCAPT.ICN";
        case B_WEL2: return "TWNNWEL2.ICN";
        case B_LEFTTURRET: return "TWNNLTUR.ICN";
        case B_RIGHTTURRET: return "TWNNRTUR.ICN";
        case B_MOAT: return "TWNNMOAT.ICN";
        case B_MARKETPLACE: return "TWNNMARK.ICN";
        case B_THIEVESGUILD: return "TWNNTHIE.ICN";
        case B_SHRINE: return "TWNNTVRN.ICN";
        case B_WELL: return "TWNNWELL.ICN";
        case B_STATUE: return "TWNNSTAT.ICN";
        case B_SHIPYARD: return "TWNNDOCK.ICN";
        case B_MAGEGUILD1:
        case B_MAGEGUILD2:
        case B_MAGEGUILD3:
        case B_MAGEGUILD4:
        case B_MAGEGUILD5: return "TWNNMAGE.ICN";
        case D_MONSTER1: return "TWNNDW_0.ICN";
        case D_MONSTER2: return "TWNNDW_1.ICN";
        case D_UPGRADE2: return "TWNNUP_1.ICN";
        case D_MONSTER3: return "TWNNDW_2.ICN";
        case D_UPGRADE3: return "TWNNUP_2.ICN";
        case D_MONSTER4: return "TWNNDW_3.ICN";
        case D_UPGRADE4: return "TWNNUP_3.ICN";
        case D_MONSTER5: return "TWNNDW_4.ICN";
        case D_UPGRADE5: return "TWNNUP_4.ICN";
        case D_MONSTER6: return "TWNNDW_5.ICN";
        default: break;
        }
    }
    else if ( race == R_SORC ) {
        switch ( building ) {
        case B_CASTLE: return "TWNSCSTL.ICN";
        case B_TENT: return "TWNSTENT.ICN";
        case B_SPEC: return "TWNSSPEC.ICN";
        case B_CAPTAIN: return "TWNSCAPT.ICN";
        case B_WEL2: return "TWNSWEL2.ICN";
        case B_LEFTTURRET: return "TWNSLTUR.ICN";
        case B_RIGHTTURRET: return "TWNSRTUR.ICN";
        case B_MOAT: return "TWNSMOAT.ICN";
        case B_MARKETPLACE: return "TWNSMARK.ICN";
        case B_THIEVESGUILD: return "TWNSTHIE.ICN";
        case B_TAVERN: return "TWNSTVRN.ICN";
        case B_WELL: return "TWNSWELL.ICN";
        case B_STATUE: return "TWNSSTAT.ICN";
        case B_SHIPYARD: return "TWNSDOCK.ICN";
        case B_MAGEGUILD1:
        case B_MAGEGUILD2:
        case B_MAGEGUILD3:
        case B_MAGEGUILD4:
        case B_MAGEGUILD5: return "TWNSMAGE.ICN";
        case D_MONSTER1: return "TWNSDW_0.ICN";
        case D_MONSTER2: return "TWNSDW_1.ICN";
        case D_UPGRADE2: return "TWNSUP_1.ICN";
        case D_MONSTER3: return "TWNSDW_2.ICN";
        case D_UPGRADE3: return "TWNSUP_2.ICN";
        case D_MONSTER4: return "TWNSDW_3.ICN";
        case D_UPGRADE4: return "TWNSUP_3.ICN";
        case D_MONSTER5: return "TWNSDW_4.ICN";
        case D_MONSTER6: return "TWNSDW_5.ICN";
        default: break;
        }
    }
    else if ( race == R_WRLK ) {
        switch ( building ) {
        case B_CASTLE: return "TWNWCSTL.ICN";
        case B_TENT: return "TWNWTENT.ICN";
        case B_SPEC: return "TWNWSPEC.ICN";
        case B_CAPTAIN: return "TWNWCAPT.ICN";
        case B_WEL2: return "TWNWWEL2.ICN";
        case B_LEFTTURRET: return "TWNWLTUR.ICN";
        case B_RIGHTTURRET: return "TWNWRTUR.ICN";
        case B_MOAT: return "TWNWMOAT.ICN";
        case B_MARKETPLACE: return "TWNWMARK.ICN";
        case B_THIEVESGUILD: return "TWNWTHIE.ICN";
        case B_TAVERN: return "TWNWTVRN.ICN";
        case B_WELL: return "TWNWWELL.ICN";
        case B_STATUE: return "TWNWSTAT.ICN";
        case B_SHIPYARD: return "TWNWDOCK.ICN";
        case B_MAGEGUILD1:
        case B_MAGEGUILD2:
        case B_MAGEGUILD3:
        case B_MAGEGUILD4:
        case B_MAGEGUILD5: return "TWNWMAGE.ICN";
        case D_MONSTER1: return "TWNWDW_0.ICN";
        case D_MONSTER2: return "TWNWDW_1.ICN";
        case D_MONSTER3: return "TWNWDW_2.ICN";
        case D_MONSTER4: return "TWNWDW_3.ICN";
        case D_UPGRADE4: return "TWNWUP_3.ICN";
        case D_MONSTER5: return "TWNWDW_4.ICN";
        case D_MONSTER6: return "TWNWDW_5.ICN";
        case D_UPGRADE6: return "TWNWUP_5.ICN";
        case D_UPGRADE7: return "TWNWUP5B.ICN";
        default: break;
        }
    }
    else if ( race == R_WZRD ) {
        switch ( building ) {
        case B_CASTLE: return "TWNZCSTL.ICN";
        case B_TENT: return "TWNZTENT.ICN";
        case B_SPEC: return "TWNZSPEC.ICN";
        case B_CAPTAIN: return "TWNZCAPT.ICN";
        case B_WEL2: return "TWNZWEL2.ICN";
        case B_LEFTTURRET: return "TWNZLTUR.ICN";
        case B_RIGHTTURRET: return "TWNZRTUR.ICN";
        case B_MOAT: return "TWNZMOAT.ICN";
        case B_MARKETPLACE: return "TWNZMARK.ICN";
        case B_THIEVESGUILD: return "TWNZTHIE.ICN";
        case B_TAVERN: return "TWNZTVRN.ICN";
        case B_WELL: return "TWNZWELL.ICN";
        case B_STATUE: return "TWNZSTAT.ICN";
        case B_SHIPYARD: return "TWNZDOCK.ICN";
        case B_MAGEGUILD1:
        case B_MAGEGUILD2:
        case B_MAGEGUILD3:
        case B_MAGEGUILD4:
        case B_MAGEGUILD5: return "TWNZMAGE.ICN";
        case D_MONSTER1: return "TWNZDW_0.ICN";
        case D_MONSTER2: return "TWNZDW_1.ICN";
        case D_UPGRADE2: return "TWNZUP_1.ICN";
        case D_MONSTER3: return "TWNZDW_2.ICN";
        case D_UPGRADE3: return "TWNZUP_2.ICN";
        case D_MONSTER4: return "TWNZDW_3.ICN";
        case D_UPGRADE4: return "TWNZUP_3.ICN";
        case D_MONSTER5: return "TWNZDW_4.ICN";
        case D_UPGRADE5: return "TWNZUP_4.ICN";
        case D_MONSTER6: return "TWNZDW_5.ICN";
        default: break;
        }
    }
    return nullptr;
}

// The sprite index for mage guild buildings (castle_building.cpp redrawCastleBuilding).
int mageGuildIndex( uint32_t building, uint32_t race )
{
    if ( race == R_NECR ) {
        switch ( building ) {
        case B_MAGEGUILD2: return 6;
        case B_MAGEGUILD3: return 12;
        case B_MAGEGUILD4: return 18;
        case B_MAGEGUILD5: return 24;
        default: break;
        }
    }
    else {
        switch ( building ) {
        case B_MAGEGUILD2: return 1;
        case B_MAGEGUILD3: return 2;
        case B_MAGEGUILD4: return 3;
        case B_MAGEGUILD5: return 4;
        default: break;
        }
    }
    return 0;
}

// Drawing order per race (port of getBuildingDrawingPriorities).
const std::vector<uint32_t> & buildingOrder( uint32_t race )
{
    static const std::vector<uint32_t> kngt = {
        B_TENT, B_WEL2, B_CASTLE, B_SPEC, B_CAPTAIN, B_LEFTTURRET, B_RIGHTTURRET, B_MOAT, B_MARKETPLACE, D_MONSTER2, D_UPGRADE2,
        B_THIEVESGUILD, B_TAVERN, B_MAGEGUILD1, B_MAGEGUILD2, B_MAGEGUILD3, B_MAGEGUILD4, B_MAGEGUILD5, D_MONSTER5, D_UPGRADE5,
        D_MONSTER6, D_UPGRADE6, D_MONSTER1, D_MONSTER3, D_UPGRADE3, D_MONSTER4, D_UPGRADE4, B_WELL, B_SHIPYARD, B_STATUE
    };
    static const std::vector<uint32_t> barb = {
        B_SPEC, B_WEL2, D_MONSTER6, B_MAGEGUILD1, B_MAGEGUILD2, B_MAGEGUILD3, B_MAGEGUILD4, B_MAGEGUILD5, B_CAPTAIN, B_TENT,
        B_CASTLE, B_LEFTTURRET, B_RIGHTTURRET, B_MOAT, D_MONSTER3, B_THIEVESGUILD, D_MONSTER1, B_MARKETPLACE, D_MONSTER2,
        D_UPGRADE2, B_TAVERN, D_MONSTER4, D_UPGRADE4, D_MONSTER5, D_UPGRADE5, B_WELL, B_STATUE, B_SHIPYARD
    };
    static const std::vector<uint32_t> sorc = {
        B_SPEC, D_MONSTER6, B_MAGEGUILD1, B_MAGEGUILD2, B_MAGEGUILD3, B_MAGEGUILD4, B_MAGEGUILD5, B_CAPTAIN, B_TENT, B_CASTLE,
        B_LEFTTURRET, B_RIGHTTURRET, B_MOAT, D_MONSTER3, D_UPGRADE3, B_SHIPYARD, B_MARKETPLACE, D_MONSTER2, D_UPGRADE2,
        B_THIEVESGUILD, D_MONSTER1, B_TAVERN, B_STATUE, B_WEL2, D_MONSTER4, D_UPGRADE4, B_WELL, D_MONSTER5
    };
    static const std::vector<uint32_t> wrlk = {
        D_MONSTER5, D_MONSTER3, B_TENT, B_CASTLE, B_LEFTTURRET, B_RIGHTTURRET, B_MOAT, B_CAPTAIN, B_SHIPYARD, B_MAGEGUILD1,
        B_MAGEGUILD2, B_MAGEGUILD3, B_MAGEGUILD4, B_MAGEGUILD5, B_TAVERN, B_THIEVESGUILD, B_MARKETPLACE, B_STATUE, D_MONSTER1,
        B_WEL2, B_SPEC, D_MONSTER4, D_UPGRADE4, D_MONSTER2, D_MONSTER6, D_UPGRADE6, D_UPGRADE7, B_WELL
    };
    static const std::vector<uint32_t> wzrd = {
        D_MONSTER6, D_UPGRADE6, B_TENT, B_CASTLE, B_LEFTTURRET, B_RIGHTTURRET, B_MOAT, B_CAPTAIN, D_MONSTER2, B_THIEVESGUILD,
        B_TAVERN, B_SHIPYARD, B_WELL, D_MONSTER3, D_UPGRADE3, D_MONSTER5, D_UPGRADE5, B_MAGEGUILD1, B_MAGEGUILD2, B_MAGEGUILD3,
        B_MAGEGUILD4, B_MAGEGUILD5, B_SPEC, B_STATUE, D_MONSTER1, D_MONSTER4, B_MARKETPLACE, B_WEL2
    };
    static const std::vector<uint32_t> necr = {
        B_SPEC, B_SHRINE, B_TENT, B_CASTLE, B_LEFTTURRET, B_RIGHTTURRET, B_MOAT, B_CAPTAIN, D_MONSTER6, D_MONSTER1,
        B_THIEVESGUILD, D_MONSTER3, D_UPGRADE3, D_MONSTER5, D_UPGRADE5, D_MONSTER2, D_UPGRADE2, D_MONSTER4, D_UPGRADE4,
        B_MAGEGUILD1, B_MAGEGUILD2, B_MAGEGUILD3, B_MAGEGUILD4, B_MAGEGUILD5, B_SHIPYARD, B_WEL2, B_MARKETPLACE, B_STATUE, B_WELL
    };

    switch ( race ) {
    case R_KNGT: return kngt;
    case R_BARB: return barb;
    case R_SORC: return sorc;
    case R_WRLK: return wrlk;
    case R_WZRD: return wzrd;
    case R_NECR: return necr;
    default: break;
    }
    static const std::vector<uint32_t> empty;
    return empty;
}

// Player color palettes (KB.PAL indices) used to repaint castle flags
// (port of getModifiedPaletteByPlayerColor, ui_castle.cpp).
// Original flag colors: palette indices 152..174 (23 colors).
constexpr int FLAG_START = 152;
constexpr int FLAG_COUNT = 23;

int colorStartId( int colorMask )
{
    switch ( colorMask ) {
    case 0x01: return 63;    // blue
    case 0x02: return 85;    // green
    case 0x04: return 175;   // red
    case 0x08: return 108;   // yellow
    case 0x10: return 199;   // orange
    case 0x20: return 132;   // purple
    default: return -1;
    }
}

int colorLength( int colorMask )
{
    switch ( colorMask ) {
    case 0x01: return 16 + 6;
    case 0x02: return 16 + 7;
    case 0x04: return 16 + 7;
    case 0x08: return 16 + 7;
    case 0x10: return 16;
    case 0x20: return 16 + 5;
    default: return FLAG_COUNT;
    }
}

// ICN files whose flag colors depend on the castle owner (ui_castle.cpp).
bool isColorDependent( const std::string & name )
{
    return name == "TWNZCSTL.ICN" || name == "TWNKCSTL.ICN" || name == "TWNKDW_4.ICN" || name == "TWNKUP_4.ICN" || name == "TWNKLTUR.ICN"
           || name == "TWNKRTUR.ICN";
}

} // namespace

CastleRender::CastleRender( const fh2::Assets & assets )
    : _a( assets )
{}

int CastleRender::contentHeight( const fh2::WorldCastle & castle ) const
{
    const char * bkgName = townBkgIcn( castle.race );
    if ( !bkgName )
        return 0;
    const fh2::IcnSprite & bkg = _a.icnSprite( bkgName, 0 );
    if ( bkg.isNull() )
        return 0;
    const int cardW = 360;
    const int tw = std::max( bkg.image.width(), 640 );
    const int th = bkg.image.height();
    const int townH = std::max( 1, th * cardW / tw );
    return townH + 8 + 50 + 8 + 24; // town + garrison strip + name line
}

QImage CastleRender::render( const fh2::WorldCastle & castle ) const
{
    const char * bkgName = townBkgIcn( castle.race );
    if ( !bkgName )
        return {};

    const fh2::IcnSprite & bkg = _a.icnSprite( bkgName, 0 );
    if ( bkg.isNull() )
        return {};

    // The castle card matches the hero card width (like a 7-slot row): the 3D
    // town is scaled to that width, the garrison slots keep their size and are
    // centered (as in the hero card).
    const int slot = 50;
    const int step = 54;
    const int cardW = 360; // = HeroCardRender::CARD_W (hero-card width)

    // Render the town (stone window + background + buildings) at its natural size
    // into its own buffer, then scale it down to the card width.
    const int tw = std::max( bkg.image.width(), 640 );
    const int th = bkg.image.height();
    QImage townBuf( tw, th, QImage::Format_ARGB32 );
    townBuf.fill( Qt::transparent );
    {
        QPainter tp( &townBuf );
        const fh2::IcnSprite & panel = _a.icnSprite( "STONEBAK.ICN", 0 );
        if ( !panel.isNull() )
            tp.drawImage( 0, 0, panel.image );
        tp.drawImage( 0, 0, bkg.image );

        const std::vector<uint32_t> & order = buildingOrder( castle.race );
        for ( const uint32_t building : order ) {
            if ( !( castle.constructedBuildings & building ) )
                continue;
            if ( building == B_TENT )
                continue;
            const char * name = buildingIcn( building, castle.race );
            if ( !name )
                continue;
            const int index = mageGuildIndex( building, castle.race );
            fh2::IcnSprite sprite = _a.icnSprite( name, index );
            if ( sprite.isNull() )
                continue;
            QImage im = sprite.image;
            if ( isColorDependent( name ) && castle.color != 0 ) {
                const int start = colorStartId( castle.color );
                if ( start >= 0 ) {
                    std::vector<uint8_t> remap( 256 );
                    for ( int i = 0; i < 256; ++i )
                        remap[i] = static_cast<uint8_t>( i );
                    const int outLen = colorLength( castle.color );
                    for ( int i = 0; i < FLAG_COUNT; ++i )
                        remap[FLAG_START + i] = static_cast<uint8_t>( start + i * outLen / FLAG_COUNT );
                    const QImage & idx = sprite.idx;
                    if ( !idx.isNull() ) {
                        const int w = std::min( im.width(), idx.width() );
                        const int h = std::min( im.height(), idx.height() );
                        for ( int y = 0; y < h; ++y ) {
                            QRgb * line = reinterpret_cast<QRgb *>( im.scanLine( y ) );
                            const uint8_t * idxLine = idx.scanLine( y );
                            for ( int x = 0; x < w; ++x ) {
                                const int pi = idxLine[x];
                                if ( pi >= FLAG_START && pi < FLAG_START + FLAG_COUNT )
                                    line[x] = _a.paletteColor( remap[pi] ).rgba();
                            }
                        }
                    }
                }
            }
            tp.drawImage( sprite.offsetX, sprite.offsetY, im );
        }
    }
    const QImage town = townBuf.scaled( cardW, std::max( 1, th * cardW / tw ), Qt::IgnoreAspectRatio, Qt::FastTransformation );

    const int viewW = cardW;
    const int viewH = town.height() + 8 + slot + 8 + 24; // town + garrison + name line
    QImage img( viewW, viewH, QImage::Format_ARGB32 );
    img.fill( qRgba( 12, 10, 8, 255 ) );
    QPainter p( &img );
    p.drawImage( 0, 0, town );

    // Garrison strip: 5 army slots in a single row, centered (like the hero card).
    const int x0 = ( viewW - ( 5 * step - 4 ) ) / 2;
    const int y0 = town.height() + 8;
    for ( int i = 0; i < 5; ++i ) {
        const int sx = x0 + i * step;
        p.setPen( QPen( QColor( 60, 55, 45 ), 1 ) );
        p.setBrush( QColor( 40, 34, 24, 200 ) );
        p.drawRect( QRect( sx, y0, slot, slot ) );

        const int mid = castle.garrisonMonsterId[i];
        if ( mid <= 0 || mid > 72 )
            continue;
        const fh2::IcnSprite & monster = _a.icnSprite( "MONS32.ICN", mid - 1 );
        if ( monster.isNull() )
            continue;
        p.drawImage( sx + ( slot - monster.image.width() ) / 2, y0 + ( slot - monster.image.height() ) / 2, monster.image );
        if ( castle.garrisonCount[i] != 0 ) {
            fh2::GameFont font( &_a );
            if ( font.valid() ) {
                const QString count = QString::number( castle.garrisonCount[i] );
                font.drawText( p, sx + slot - font.textWidth( count, fh2::GameFont::Size::SMALL ) - 2, y0 + slot - font.lineHeight( fh2::GameFont::Size::SMALL ) + 1, count,
                               fh2::GameFont::Size::SMALL, fh2::GameFont::Color::WHITE );
            }
        }
    }

    // Castle name under the garrison strip (GameFont, cp1251).
    {
        fh2::GameFont font( &_a );
        if ( font.valid() ) {
            const QString name = fh2::decodeCp1251( castle.name );
            const int w = font.textWidth( name, fh2::GameFont::Size::NORMAL );
            const int y = y0 + slot + 12;
            const int x = std::max( 0, ( viewW - w ) / 2 );
            font.drawText( p, x, y, name, fh2::GameFont::Size::NORMAL, fh2::GameFont::Color::WHITE );
        }
    }

    p.end();
    return img;
}

} // namespace fh2poster
