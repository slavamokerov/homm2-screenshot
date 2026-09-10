#include "maprender.h"

#include <QPainter>
#include <algorithm>
#include <array>
#include <cstdint>

namespace fh2poster {

namespace {

constexpr int TILE = 32;

// Direction bit masks (fheroes2 Direction).
enum Dir : uint16_t {
    DIR_TOP_LEFT = 0x0001,
    DIR_TOP = 0x0002,
    DIR_TOP_RIGHT = 0x0004,
    DIR_RIGHT = 0x0008,
    DIR_BOTTOM_RIGHT = 0x0010,
    DIR_BOTTOM = 0x0020,
    DIR_BOTTOM_LEFT = 0x0040,
    DIR_LEFT = 0x0080,
    DIR_CENTER = 0x0100
};

constexpr uint16_t DIR_TOP_ROW = DIR_TOP_LEFT | DIR_TOP | DIR_TOP_RIGHT;
constexpr uint16_t DIR_BOTTOM_ROW = DIR_BOTTOM_LEFT | DIR_BOTTOM | DIR_BOTTOM_RIGHT;
constexpr uint16_t DIR_CENTER_ROW = DIR_LEFT | DIR_CENTER | DIR_RIGHT;
constexpr uint16_t DIR_CENTER_COL = DIR_TOP | DIR_CENTER | DIR_BOTTOM;
constexpr uint16_t DIR_ALL = DIR_TOP_ROW | DIR_BOTTOM_ROW | DIR_CENTER_ROW;
constexpr uint16_t DIR_TOP_RIGHT_CORNER = DIR_TOP | DIR_TOP_RIGHT | DIR_RIGHT;
constexpr uint16_t DIR_TOP_LEFT_CORNER = DIR_TOP | DIR_TOP_LEFT | DIR_LEFT;
constexpr uint16_t DIR_BOTTOM_RIGHT_CORNER = DIR_BOTTOM | DIR_BOTTOM_RIGHT | DIR_RIGHT;
constexpr uint16_t DIR_BOTTOM_LEFT_CORNER = DIR_BOTTOM | DIR_BOTTOM_LEFT | DIR_LEFT;

// Object types.
enum Obj : uint16_t {
    OBJ_NON_ACTION_CASTLE = 35,
    OBJ_MINE = 151,
    OBJ_MONSTER = 152,
    OBJ_CASTLE = 163,
    OBJ_BOAT = 171,
    OBJ_RANDOM_MONSTER = 175,
    OBJ_RANDOM_MONSTER_WEAK = 176,
    OBJ_RANDOM_MONSTER_MEDIUM = 177,
    OBJ_RANDOM_MONSTER_STRONG = 178,
    OBJ_RANDOM_MONSTER_VERY_STRONG = 179,
    OBJ_HERO = 183,
    OBJ_ABANDONED_MINE = 192
};

// Spell ids (metadata[2] of a mine tile): haunted mine and the mine guardians.
enum MineSpell : uint32_t {
    SPELL_HAUNT = 61,
    SPELL_SETGUARDIAN = 62,
    SPELL_SETWGUARDIAN = 65
};

constexpr uint32_t HERO_ENABLE_MOVE = 0x00000002;

// ObjectIcnType -> ICN file name in AGG (nullptr — must not be drawn directly).
const char * icnFileName( int icnType )
{
    static const char * names[] = {
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, // 0-5
        "BOAT32.ICN",                                         // 6
        nullptr, nullptr, nullptr,                            // 7-9
        "OBJNHAUN.ICN",                                       // 10
        "OBJNARTI.ICN",                                       // 11
        nullptr,                                              // 12 MONS32 (special)
        nullptr,                                              // 13
        "FLAG32.ICN",                                         // 14
        nullptr, nullptr, nullptr, nullptr, nullptr,          // 15-19
        nullptr,                                              // 20 MINIMON (special)
        nullptr,                                              // 21 MINIHERO (editor)
        "MTNSNOW.ICN", "MTNSWMP.ICN", "MTNLAVA.ICN", "MTNDSRT.ICN", "MTNDIRT.ICN", "MTNMULT.ICN", // 22-27
        nullptr,                                              // 28
        "EXTRAOVR.ICN", "ROAD.ICN", "MTNCRCK.ICN", "MTNGRAS.ICN", "TREJNGL.ICN", "TREEVIL.ICN", // 29-34
        "OBJNTOWN.ICN", "OBJNTWBA.ICN", "OBJNTWSH.ICN", "OBJNTWRD.ICN", "OBJNXTRA.ICN", "OBJNWAT2.ICN", // 35-40
        "OBJNMUL2.ICN", "TRESNOW.ICN", "TREFIR.ICN", "TREFALL.ICN", "STREAM.ICN", "OBJNRSRC.ICN", // 41-46
        nullptr,                                              // 47
        "OBJNGRA2.ICN", "TREDECI.ICN", "OBJNWATR.ICN", "OBJNGRAS.ICN", "OBJNSNOW.ICN", "OBJNSWMP.ICN", // 48-53
        "OBJNLAVA.ICN", "OBJNDSRT.ICN", "OBJNDIRT.ICN", "OBJNCRCK.ICN", "OBJNLAV3.ICN", "OBJNMULT.ICN", // 54-59
        "OBJNLAV2.ICN", "X_LOC1.ICN", "X_LOC2.ICN", "X_LOC3.ICN" // 60-63
    };
    if ( icnType < 0 || icnType >= static_cast<int>( sizeof( names ) / sizeof( names[0] ) ) )
        return nullptr;
    return names[icnType];
}

bool isRandomMonsterType( uint16_t objectType )
{
    return objectType == OBJ_RANDOM_MONSTER || objectType == OBJ_RANDOM_MONSTER_WEAK || objectType == OBJ_RANDOM_MONSTER_MEDIUM
           || objectType == OBJ_RANDOM_MONSTER_STRONG || objectType == OBJ_RANDOM_MONSTER_VERY_STRONG;
}

// Object parts that animate in place (an animated object's base frame + its
// first animation frame overlay). A static poster freezes the animation at its
// first frame, so we overlay frame icnIndex + 1 on top of the base — exactly
// the engine's renderObjectPart() (secondaryFrameIndex = icnIndex + animIdx%frames + 1).
// The combined frame-1-elsewhere guard (in blitPart) ensures we never overlay a
// frame that is actually a DIFFERENT tile's "building block" (which caused
// multi-tile buildings to double/misalign).
bool animates( uint8_t icnType, uint8_t icnIndex )
{
    if ( icnType == 59 && icnIndex == 131 )
        return true; // Campfire (OBJNMULT frame 131) — in-place flame, one tile.

    // Animated object parts (icnType -> set of icnIndex) ported from the engine's
    // object-part table (map_object_info.cpp), where animationFrames > 0.
    static const std::pair<uint8_t, uint8_t> animated[] = {
        { 41, 19 }, { 41, 27 }, { 41, 61 }, { 41, 83 }, { 41, 90 }, { 41, 98 },
        { 41, 105 }, { 41, 129 }, { 41, 166 }, { 41, 173 }, { 41, 180 }, { 41, 190 },
        { 41, 241 },
        { 48, 23 }, { 48, 27 }, { 48, 31 }, { 48, 35 }, { 48, 39 }, { 48, 43 },
        { 48, 47 }, { 48, 51 }, { 48, 55 }, { 48, 59 }, { 48, 63 }, { 48, 70 },
        { 48, 77 }, { 48, 84 }, { 48, 93 }, { 48, 100 }, { 48, 107 }, { 48, 114 },
        { 52, 96 }, { 52, 100 }, { 52, 104 }, { 52, 108 }, { 52, 112 }, { 52, 116 },
        { 52, 120 }, { 52, 124 }, { 52, 128 }, { 52, 132 }, { 52, 151 }, { 52, 162 },
        { 52, 169 }, { 52, 177 }, { 52, 184 },
        { 53, 0 }, { 53, 7 }, { 53, 14 }, { 53, 22 }, { 53, 34 }, { 53, 43 },
        { 53, 51 }, { 53, 58 }, { 53, 67 }, { 53, 74 },
        { 54, 78 }, { 54, 88 }, { 54, 98 },
        { 56, 153 }, { 56, 157 }, { 56, 161 }, { 56, 165 }, { 56, 169 }, { 56, 173 },
        { 56, 177 }, { 56, 181 }, { 56, 185 }, { 56, 189 },
        { 57, 80 }, { 57, 91 }, { 57, 102 }, { 57, 113 }, { 57, 124 }, { 57, 137 },
        { 57, 148 }, { 57, 159 }, { 57, 170 }, { 57, 190 }, { 57, 202 },
        { 58, 0 }, { 58, 15 }, { 58, 30 }, { 58, 45 }, { 58, 60 }, { 58, 75 },
        { 58, 90 }, { 58, 105 }, { 58, 120 }, { 58, 135 }, { 58, 150 }, { 58, 165 },
        { 58, 180 }, { 58, 195 }, { 58, 210 }, { 58, 225 },
        { 59, 5 }, { 59, 15 }, { 59, 25 }, { 59, 36 }, { 59, 45 }, { 59, 90 },
        { 59, 97 }, { 59, 104 },
        { 60, 0 }, { 60, 7 }, { 60, 14 }, { 60, 21 }, { 60, 33 }, { 60, 44 },
        { 60, 55 }, { 60, 67 },
        { 61, 4 }, { 61, 13 }, { 61, 22 }, { 61, 31 }, { 61, 40 }, { 61, 50 },
        { 61, 59 }, { 61, 85 }, { 61, 94 }, { 61, 103 },
        { 63, 0 }, { 63, 10 }, { 63, 20 }, { 63, 32 }, { 63, 41 }, { 63, 50 },
    };
    for ( const auto & e : animated ) {
        if ( e.first == icnType && e.second == icnIndex )
            return true;
    }
    return false;
}

// Composites two frames of one sprite (e.g. a monster's base frame + its
// animated overlay frame) positioned by their own offsets, like the engine
// draws the base and the animation frame on top of each other.
struct CompositedSprite
{
    QImage image;
    int ox = 0, oy = 0;
};

CompositedSprite overlayFrames( const fh2::IcnSprite & a, const fh2::IcnSprite & b )
{
    const int x0 = std::min( a.offsetX, b.offsetX );
    const int y0 = std::min( a.offsetY, b.offsetY );
    const int x1 = std::max( a.offsetX + a.image.width(), b.offsetX + b.image.width() );
    const int y1 = std::max( a.offsetY + a.image.height(), b.offsetY + b.image.height() );
    QImage img( x1 - x0, y1 - y0, QImage::Format_ARGB32 );
    img.fill( Qt::transparent );
    QPainter p( &img );
    p.setRenderHint( QPainter::SmoothPixmapTransform, false );
    p.drawImage( a.offsetX - x0, a.offsetY - y0, a.image );
    p.drawImage( b.offsetX - x0, b.offsetY - y0, b.image );
    p.end();
    return CompositedSprite{ img, x0, y0 };
}

int heroDirBaseIndex( int32_t direction, bool & flip )
{
    flip = false;
    switch ( direction ) {
    case DIR_TOP: return 0;
    case DIR_TOP_RIGHT: return 9;
    case DIR_RIGHT: return 18;
    case DIR_BOTTOM_RIGHT: return 27;
    case DIR_BOTTOM: return 36;
    case DIR_BOTTOM_LEFT: flip = true; return 27;
    case DIR_LEFT: flip = true; return 18;
    case DIR_TOP_LEFT: flip = true; return 9;
    default: return 0;
    }
}

int heroShadowIndex( int32_t direction )
{
    switch ( direction ) {
    case DIR_TOP: return 0;
    case DIR_TOP_RIGHT: return 9;
    case DIR_RIGHT: return 18;
    case DIR_BOTTOM_RIGHT: return 27;
    case DIR_BOTTOM: return 36;
    case DIR_BOTTOM_LEFT: return 77;
    case DIR_LEFT: return 68;
    case DIR_TOP_LEFT: return 59;
    default: return 0;
    }
}

// Shadow frame index while on a boat (BOATSHAD) — no reflection, per direction.
int heroBoatShadowIndex( int32_t direction )
{
    switch ( direction ) {
    case DIR_TOP: return 0;
    case DIR_TOP_RIGHT: return 9;
    case DIR_RIGHT: return 18;
    case DIR_BOTTOM_RIGHT: return 27;
    case DIR_BOTTOM: return 36;
    case DIR_BOTTOM_LEFT: return 45;
    case DIR_LEFT: return 54;
    case DIR_TOP_LEFT: return 63;
    default: return 0;
    }
}

const char * heroIcnByRace( uint32_t race )
{
    switch ( race & 0xFF ) {
    case 0x01: return "KNGT32.ICN";
    case 0x02: return "BARB32.ICN";
    case 0x04: return "SORC32.ICN";
    case 0x08: return "WRLK32.ICN";
    case 0x10: return "WZRD32.ICN";
    case 0x20: return "NECR32.ICN";
    default: return nullptr;
    }
}

// The hero's color flag ICN (port of getFlagSpriteInfo); matches the color bitmask.
const char * heroFlagIcnByColor( uint8_t color )
{
    switch ( color ) {
    case 0x01: return "B-FLAG32.ICN";
    case 0x02: return "G-FLAG32.ICN";
    case 0x04: return "R-FLAG32.ICN";
    case 0x08: return "Y-FLAG32.ICN";
    case 0x10: return "O-FLAG32.ICN";
    case 0x20: return "P-FLAG32.ICN";
    default: return nullptr;
    }
}

// The boat flag ICN (isShipMaster): B/G/R/Y/O/P-BFLG32.ICN.
const char * heroBoatFlagIcnByColor( uint8_t color )
{
    switch ( color ) {
    case 0x01: return "B-BFLG32.ICN";
    case 0x02: return "G-BFLG32.ICN";
    case 0x04: return "R-BFLG32.ICN";
    case 0x08: return "Y-BFLG32.ICN";
    case 0x10: return "O-BFLG32.ICN";
    case 0x20: return "P-BFLG32.ICN";
    default: return nullptr;
    }
}

// Route arrow index (port of Route::Path::GetIndexSprite).
int routeIndex( int from, int to, int cost )
{
    int index = 1;
    switch ( cost ) {
    case 200: index = 121; break;
    case 175: index = 97; break;
    case 150: index = 73; break;
    case 125: index = 49; break;
    case 100: index = 25; break;
    default: break;
    }

    int delta = -1;
    switch ( from ) {
    case DIR_TOP:
        switch ( to ) {
        case DIR_TOP: delta = 8; break;
        case DIR_TOP_RIGHT: delta = 17; break;
        case DIR_RIGHT: delta = 18; break;
        case DIR_LEFT: delta = 6; break;
        case DIR_TOP_LEFT: delta = 7; break;
        case DIR_BOTTOM_LEFT: delta = 5; break;
        case DIR_BOTTOM_RIGHT: delta = 19; break;
        default: break;
        }
        break;
    case DIR_TOP_RIGHT:
        switch ( to ) {
        case DIR_TOP: delta = 0; break;
        case DIR_TOP_RIGHT: delta = 9; break;
        case DIR_RIGHT: delta = 18; break;
        case DIR_BOTTOM_RIGHT: delta = 19; break;
        case DIR_TOP_LEFT: delta = 7; break;
        case DIR_BOTTOM: delta = 20; break;
        case DIR_LEFT: delta = 6; break;
        default: break;
        }
        break;
    case DIR_RIGHT:
        switch ( to ) {
        case DIR_TOP: delta = 0; break;
        case DIR_BOTTOM: delta = 20; break;
        case DIR_BOTTOM_RIGHT: delta = 19; break;
        case DIR_RIGHT: delta = 10; break;
        case DIR_TOP_RIGHT: delta = 1; break;
        case DIR_TOP_LEFT: delta = 7; break;
        case DIR_BOTTOM_LEFT: delta = 21; break;
        default: break;
        }
        break;
    case DIR_BOTTOM_RIGHT:
        switch ( to ) {
        case DIR_TOP_RIGHT: delta = 1; break;
        case DIR_RIGHT: delta = 2; break;
        case DIR_BOTTOM_RIGHT: delta = 11; break;
        case DIR_BOTTOM: delta = 20; break;
        case DIR_BOTTOM_LEFT: delta = 21; break;
        case DIR_TOP: delta = 0; break;
        case DIR_LEFT: delta = 22; break;
        default: break;
        }
        break;
    case DIR_BOTTOM:
        switch ( to ) {
        case DIR_RIGHT: delta = 2; break;
        case DIR_BOTTOM_RIGHT: delta = 3; break;
        case DIR_BOTTOM: delta = 12; break;
        case DIR_BOTTOM_LEFT: delta = 21; break;
        case DIR_LEFT: delta = 22; break;
        case DIR_TOP_LEFT: delta = 16; break;
        case DIR_TOP_RIGHT: delta = 1; break;
        default: break;
        }
        break;
    case DIR_BOTTOM_LEFT:
        switch ( to ) {
        case DIR_BOTTOM_RIGHT: delta = 3; break;
        case DIR_BOTTOM: delta = 4; break;
        case DIR_BOTTOM_LEFT: delta = 13; break;
        case DIR_LEFT: delta =22; break;
        case DIR_TOP_LEFT: delta = 23; break;
        case DIR_TOP: delta = 16; break;
        case DIR_RIGHT: delta = 2; break;
        default: break;
        }
        break;
    case DIR_LEFT:
        switch ( to ) {
        case DIR_TOP: delta = 16; break;
        case DIR_BOTTOM: delta = 4; break;
        case DIR_BOTTOM_LEFT: delta = 5; break;
        case DIR_LEFT: delta = 14; break;
        case DIR_TOP_LEFT: delta = 23; break;
        case DIR_TOP_RIGHT: delta = 17; break;
        case DIR_BOTTOM_RIGHT: delta = 3; break;
        default: break;
        }
        break;
    case DIR_TOP_LEFT:
        switch ( to ) {
        case DIR_TOP: delta = 16; break;
        case DIR_TOP_RIGHT: delta = 17; break;
        case DIR_BOTTOM_LEFT: delta = 5; break;
        case DIR_LEFT: delta = 6; break;
        case DIR_TOP_LEFT: delta = 15; break;
        case DIR_BOTTOM: delta = 4; break;
        case DIR_RIGHT: delta = 18; break;
        default: break;
        }
        break;
    default:
        break;
    }

    if ( delta < 0 )
        return 0;
    return index + delta;
}

// CLOP32 fog border index (port of Maps::drawFog).
void fogBorderIndex( uint16_t fogDirection, int tileIndex, int & index, bool & revert )
{
    index = 0;
    revert = false;
    const auto has = [fogDirection]( uint16_t dir ) { return ( fogDirection & dir ) == dir; };

    if ( !( fogDirection & ( DIR_TOP | DIR_BOTTOM | DIR_LEFT | DIR_RIGHT ) ) )
        index = 10;
    else if ( has( DIR_TOP ) && !( fogDirection & ( DIR_BOTTOM | DIR_LEFT | DIR_RIGHT ) ) )
        index = 6;
    else if ( has( DIR_RIGHT ) && !( fogDirection & ( DIR_TOP | DIR_BOTTOM | DIR_LEFT ) ) )
        index = 7;
    else if ( has( DIR_LEFT ) && !( fogDirection & ( DIR_TOP | DIR_BOTTOM | DIR_RIGHT ) ) ) {
        index = 7;
        revert = true;
    }
    else if ( has( DIR_BOTTOM ) && !( fogDirection & ( DIR_TOP | DIR_LEFT | DIR_RIGHT ) ) )
        index = 8;
    else if ( has( DIR_CENTER_COL ) && !( fogDirection & ( DIR_LEFT | DIR_RIGHT ) ) )
        index = 9;
    else if ( has( DIR_CENTER_ROW ) && !( fogDirection & ( DIR_TOP | DIR_BOTTOM ) ) )
        index = 29;
    else if ( fogDirection == ( DIR_ALL & ~DIR_TOP_RIGHT ) )
        index = 15;
    else if ( fogDirection == ( DIR_ALL & ~DIR_TOP_LEFT ) ) {
        index = 15;
        revert = true;
    }
    else if ( fogDirection == ( DIR_ALL & ~DIR_BOTTOM_RIGHT ) )
        index = 22;
    else if ( fogDirection == ( DIR_ALL & ~DIR_BOTTOM_LEFT ) ) {
        index = 22;
        revert = true;
    }
    else if ( fogDirection == ( DIR_ALL & ~( DIR_TOP_RIGHT | DIR_BOTTOM_RIGHT ) ) )
        index = 16;
    else if ( fogDirection == ( DIR_ALL & ~( DIR_TOP_LEFT | DIR_BOTTOM_LEFT ) ) ) {
        index = 16;
        revert = true;
    }
    else if ( fogDirection == ( DIR_ALL & ~( DIR_TOP_RIGHT | DIR_BOTTOM_LEFT ) ) )
        index = 17;
    else if ( fogDirection == ( DIR_ALL & ~( DIR_TOP_LEFT | DIR_BOTTOM_RIGHT ) ) ) {
        index = 17;
        revert = true;
    }
    else if ( fogDirection == ( DIR_ALL & ~( DIR_TOP_LEFT | DIR_TOP_RIGHT ) ) )
        index = 18;
    else if ( fogDirection == ( DIR_ALL & ~( DIR_BOTTOM_LEFT | DIR_BOTTOM_RIGHT ) ) )
        index = 23;
    else if ( fogDirection == ( DIR_ALL & ~DIR_TOP_RIGHT_CORNER ) )
        index = 13;
    else if ( fogDirection == ( DIR_ALL & ~DIR_TOP_LEFT_CORNER ) ) {
        index = 13;
        revert = true;
    }
    else if ( fogDirection == ( DIR_ALL & ~DIR_BOTTOM_RIGHT_CORNER ) )
        index = 14;
    else if ( fogDirection == ( DIR_ALL & ~DIR_BOTTOM_LEFT_CORNER ) ) {
        index = 14;
        revert = true;
    }
    else if ( has( DIR_LEFT | DIR_BOTTOM_LEFT | DIR_BOTTOM ) && !( fogDirection & ( DIR_TOP | DIR_RIGHT ) ) )
        index = 11;
    else if ( has( DIR_RIGHT | DIR_BOTTOM_RIGHT | DIR_BOTTOM ) && !( fogDirection & ( DIR_TOP | DIR_LEFT ) ) ) {
        index = 11;
        revert = true;
    }
    else if ( has( DIR_LEFT | DIR_TOP_LEFT | DIR_TOP ) && !( fogDirection & ( DIR_BOTTOM | DIR_RIGHT ) ) )
        index = 12;
    else if ( has( DIR_RIGHT | DIR_TOP_RIGHT | DIR_TOP ) && !( fogDirection & ( DIR_BOTTOM | DIR_LEFT ) ) ) {
        index = 12;
        revert = true;
    }
    else if ( has( DIR_CENTER_ROW | DIR_BOTTOM | DIR_TOP | DIR_TOP_LEFT )
              && !( fogDirection & ( DIR_BOTTOM_LEFT | DIR_BOTTOM_RIGHT | DIR_TOP_RIGHT ) ) )
        index = 19;
    else if ( has( DIR_CENTER_ROW | DIR_BOTTOM | DIR_TOP | DIR_TOP_RIGHT )
              && !( fogDirection & ( DIR_BOTTOM_LEFT | DIR_BOTTOM_RIGHT | DIR_TOP_LEFT ) ) ) {
        index = 19;
        revert = true;
    }
    else if ( has( DIR_CENTER_ROW | DIR_BOTTOM | DIR_TOP | DIR_BOTTOM_LEFT )
              && !( fogDirection & ( DIR_TOP_RIGHT | DIR_BOTTOM_RIGHT | DIR_TOP_LEFT ) ) )
        index = 20;
    else if ( has( DIR_CENTER_ROW | DIR_BOTTOM | DIR_TOP | DIR_BOTTOM_RIGHT )
              && !( fogDirection & ( DIR_TOP_RIGHT | DIR_BOTTOM_LEFT | DIR_TOP_LEFT ) ) ) {
        index = 20;
        revert = true;
    }
    else if ( has( DIR_CENTER_ROW | DIR_BOTTOM | DIR_TOP ) && !( fogDirection & ( DIR_TOP_RIGHT | DIR_BOTTOM_RIGHT | DIR_BOTTOM_LEFT | DIR_TOP_LEFT ) ) )
        index = 21;
    else if ( has( DIR_CENTER_ROW | DIR_BOTTOM | DIR_BOTTOM_LEFT ) && !( fogDirection & ( DIR_TOP | DIR_BOTTOM_RIGHT ) ) )
        index = 24;
    else if ( has( DIR_CENTER_ROW | DIR_BOTTOM | DIR_BOTTOM_RIGHT ) && !( fogDirection & ( DIR_TOP | DIR_BOTTOM_LEFT ) ) ) {
        index = 24;
        revert = true;
    }
    else if ( has( DIR_CENTER_COL | DIR_LEFT | DIR_TOP_LEFT ) && !( fogDirection & ( DIR_RIGHT | DIR_BOTTOM_LEFT ) ) )
        index = 25;
    else if ( has( DIR_CENTER_COL | DIR_RIGHT | DIR_TOP_RIGHT ) && !( fogDirection & ( DIR_LEFT | DIR_BOTTOM_RIGHT ) ) ) {
        index = 25;
        revert = true;
    }
    else if ( has( DIR_CENTER_COL | DIR_BOTTOM_LEFT | DIR_LEFT ) && !( fogDirection & ( DIR_RIGHT | DIR_TOP_LEFT ) ) )
        index = 26;
    else if ( has( DIR_CENTER_COL | DIR_BOTTOM_RIGHT | DIR_RIGHT ) && !( fogDirection & ( DIR_LEFT | DIR_TOP_LEFT ) ) ) {
        index = 26;
        revert = true;
    }
    else if ( has( DIR_CENTER_ROW | DIR_TOP_LEFT | DIR_TOP ) && !( fogDirection & ( DIR_BOTTOM | DIR_TOP_RIGHT ) ) )
        index = 30;
    else if ( has( DIR_CENTER_ROW | DIR_TOP_RIGHT | DIR_TOP ) && !( fogDirection & ( DIR_BOTTOM | DIR_TOP_LEFT ) ) ) {
        index = 30;
        revert = true;
    }
    else if ( has( DIR_BOTTOM | DIR_LEFT ) && !( fogDirection & ( DIR_TOP | DIR_RIGHT | DIR_BOTTOM_LEFT ) ) )
        index = 27;
    else if ( has( DIR_BOTTOM | DIR_RIGHT ) && !( fogDirection & ( DIR_TOP | DIR_LEFT | DIR_BOTTOM_RIGHT ) ) ) {
        index = 27;
        revert = true;
    }
    else if ( has( DIR_LEFT | DIR_TOP ) && !( fogDirection & ( DIR_TOP_LEFT | DIR_RIGHT | DIR_BOTTOM ) ) )
        index = 28;
    else if ( has( DIR_RIGHT | DIR_TOP ) && !( fogDirection & ( DIR_TOP_RIGHT | DIR_LEFT | DIR_BOTTOM ) ) ) {
        index = 28;
        revert = true;
    }
    else if ( has( DIR_CENTER_ROW | DIR_TOP ) && !( fogDirection & ( DIR_BOTTOM | DIR_TOP_LEFT | DIR_TOP_RIGHT ) ) )
        index = 31;
    else if ( has( DIR_CENTER_COL | DIR_RIGHT ) && !( fogDirection & ( DIR_LEFT | DIR_TOP_RIGHT | DIR_BOTTOM_RIGHT ) ) )
        index = 32;
    else if ( has( DIR_CENTER_COL | DIR_LEFT ) && !( fogDirection & ( DIR_RIGHT | DIR_TOP_LEFT | DIR_BOTTOM_LEFT ) ) ) {
        index = 32;
        revert = true;
    }
    else if ( has( DIR_CENTER_ROW | DIR_BOTTOM ) && !( fogDirection & ( DIR_TOP | DIR_BOTTOM_LEFT | DIR_BOTTOM_RIGHT ) ) )
        index = 33;
    else if ( has( DIR_CENTER_ROW | DIR_BOTTOM_ROW ) && !( fogDirection & DIR_TOP ) )
        index = ( tileIndex % 2 ) ? 0 : 1;
    else if ( has( DIR_CENTER_ROW | DIR_TOP_ROW ) && !( fogDirection & DIR_BOTTOM ) )
        index = ( tileIndex % 2 ) ? 4 : 5;
    else if ( has( DIR_CENTER_COL | DIR_LEFT | DIR_BOTTOM_LEFT | DIR_TOP_LEFT ) && !( fogDirection & DIR_RIGHT ) )
        index = ( tileIndex % 2 ) ? 2 : 3;
    else if ( has( DIR_CENTER_COL | DIR_RIGHT | DIR_BOTTOM_RIGHT | DIR_TOP_RIGHT ) && !( fogDirection & DIR_LEFT ) ) {
        index = ( tileIndex % 2 ) ? 2 : 3;
        revert = true;
    }
    else {
        index = -1; // unknown — draw the full fog tile
    }
}

// Splits a sprite into tile-row bands: the part below the unit's tile, the part on
// the tile and the part above it. Returns source rects (y is in the sprite space).
struct Bands
{
    QRect below, on, above;
};

Bands splitBands( int oy, int h, int width )
{
    Bands b;
    // Band "on the tile": intersection of [oy, oy+h) with [0, 32).
    const int onLo = std::max( 0, oy );
    const int onHi = std::min( TILE, oy + h );
    if ( onHi > onLo )
        b.on = QRect( 0, onLo - oy, width, onHi - onLo );
    // Band "below": [32, oy+h).
    if ( oy + h > TILE ) {
        const int lo = std::max( TILE, oy );
        b.below = QRect( 0, lo - oy, width, oy + h - lo );
    }
    // Band "above": [oy, 0).
    if ( oy < 0 ) {
        const int hi = std::min( 0, oy + h );
        if ( hi > oy )
            b.above = QRect( 0, 0, width, hi - oy );
    }
    return b;
}

// A tile-unfit unit (hero, monster or boat) with its sprite and shadow.
struct Unit
{
    int tileIndex = 0;
    QImage sprite;
    QImage shadow;
    int ox = 0, oy = 0;       // sprite offset from the unit's tile (game pixels)
    int shOx = 0, shOy = 0;   // shadow offset
    QImage flag;              // hero's color flag (drawn on top of the body)
    int fOx = 0, fOy = 0;     // flag offset
    bool flip = false;
    bool isHero = false;
};

const fh2::WorldHero * heroAt( const fh2::WorldData & world, int tileIndex )
{
    const uint8_t id = world.tiles[tileIndex].occupantHeroId;
    if ( id == 0 || id >= 73 )
        return nullptr;
    const fh2::WorldHero & h = world.heroes[id];
    return h.color != 0 ? &h : nullptr;
}

} // namespace

MapRender::MapRender( const fh2::Assets & assets )
    : _a( assets )
{}

QImage MapRender::render( const fh2::WorldData & world, int mapPx, int selectedColor, bool withFog, RouteMode routes ) const
{
    const int w = world.width;
    const int h = world.height;
    const size_t tileCount = static_cast<size_t>( w ) * h;

    QImage img( mapPx, mapPx, QImage::Format_ARGB32 );
    img.fill( qRgba( 0, 0, 0, 255 ) );
    QPainter p( &img );
    const double scale = static_cast<double>( mapPx ) / ( w * TILE );
    p.scale( scale, scale );
    p.setRenderHint( QPainter::SmoothPixmapTransform, false );
    p.setRenderHint( QPainter::Antialiasing, false );

    // --- Fog directions ---
    std::vector<uint16_t> fogDir( tileCount, 0 );
    if ( withFog && selectedColor != 0 ) {
        const auto fogged = [&]( int x, int y ) {
            if ( x < 0 || y < 0 || x >= w || y >= h )
                return true;
            return ( world.tiles[static_cast<size_t>( y ) * w + x].fogColors & selectedColor ) != 0;
        };
        for ( int y = 0; y < h; ++y ) {
            for ( int x = 0; x < w; ++x ) {
                const size_t i = static_cast<size_t>( y ) * w + x;
                if ( !fogged( x, y ) )
                    continue;
                uint16_t d = DIR_CENTER;
                if ( fogged( x - 1, y - 1 ) ) d |= DIR_TOP_LEFT;
                if ( fogged( x, y - 1 ) ) d |= DIR_TOP;
                if ( fogged( x + 1, y - 1 ) ) d |= DIR_TOP_RIGHT;
                if ( fogged( x - 1, y ) ) d |= DIR_LEFT;
                if ( fogged( x + 1, y ) ) d |= DIR_RIGHT;
                if ( fogged( x - 1, y + 1 ) ) d |= DIR_BOTTOM_LEFT;
                if ( fogged( x, y + 1 ) ) d |= DIR_BOTTOM;
                if ( fogged( x + 1, y + 1 ) ) d |= DIR_BOTTOM_RIGHT;
                // A castle owned by the selected player stays fully visible
                // through that player's fog (like in the game).
                for ( const fh2::WorldCastle & c : world.castles )
                    if ( c.x == x && c.y == y && c.color == selectedColor ) {
                        d = 0;
                        break;
                    }
                fogDir[i] = d;
            }
        }
    }

    const auto blitTile = [&]( const QImage & src, int tx, int ty ) { p.drawImage( tx * TILE, ty * TILE, src ); };
    const auto blitAt = [&]( const QImage & src, int gx, int gy ) { p.drawImage( gx, gy, src ); };

    // Frames actually placed on the map (as a part). Used to guard the static
    // animation overlay: an overlay frame that is ALSO a placed part belongs to a
    // multi-tile object (a building / volcano "block"), so it is NOT an in-place
    // animation and must not be drawn on top.
    std::vector<bool> usedFrames( 64 * 256, false );
    for ( const fh2::WorldTile & t : world.tiles ) {
        const auto mark = [&]( const fh2::ObjectPart & p ) {
            if ( p.icnType < 64 )
                usedFrames[static_cast<size_t>( p.icnType ) * 256 + p.icnIndex] = true;
        };
        mark( t.mainPart );
        for ( const fh2::ObjectPart & p : t.groundParts )
            mark( p );
        for ( const fh2::ObjectPart & p : t.topParts )
            mark( p );
    }

    const auto blitPart = [&]( const fh2::ObjectPart & part, int tx, int ty ) {
        const char * name = icnFileName( part.icnType );
        if ( !name )
            return;
        const fh2::IcnSprite & sp = _a.icnSprite( name, part.icnIndex );
        if ( sp.isNull() )
            return;
        blitAt( sp.image, tx * TILE + sp.offsetX, ty * TILE + sp.offsetY );

        // Static in-place animation overlay (e.g. the campfire flame). Only when
        // the base part is an animated object AND the next frame is not used by
        // any placed part (otherwise it is a neighbouring tile's building block).
        const int next = part.icnIndex + 1;
        if ( next > 255 || !animates( part.icnType, part.icnIndex ) )
            return;
        if ( usedFrames[static_cast<size_t>( part.icnType ) * 256 + next] )
            return;
        const fh2::IcnSprite & ov = _a.icnSprite( name, next );
        if ( ov.isNull() )
            return;
        blitAt( ov.image, tx * TILE + ov.offsetX, ty * TILE + ov.offsetY );
    };

    // The main object of a tile is rendered within the pass matching its own
    // layer (like Maps::redrawBottomLayerObjects). Roads/shadows/lakes often
    // live in the main part rather than in groundParts, so we must not restrict
    // the main part to the OBJECT layer only. Tile-unfit sprites (boats,
    // monsters, hero) are handled separately as units.
    const auto blitMain = [&]( const fh2::WorldTile & t, int tx, int ty ) {
        const fh2::ObjectPart & mp = t.mainPart;
        if ( mp.icnType == 0 || mp.icnIndex == 255 )
            return;
        // Boats (6), MONS32 (12, random handled below), MINIMON (20) and
        // MINIHERO (21) are tile-unfit and rendered separately as units.
        if ( mp.icnType == 6 || mp.icnType == 20 || mp.icnType == 21 )
            return;
        if ( mp.icnType == 12 && !isRandomMonsterType( t.mainObjectType ) )
            return;
        blitPart( mp, tx, ty );
    };

    // --- 1. Terrain ---
    for ( int y = 0; y < h; ++y ) {
        for ( int x = 0; x < w; ++x ) {
            const size_t i = static_cast<size_t>( y ) * w + x;
            const fh2::WorldTile & t = world.tiles[i];
            if ( withFog && fogDir[i] == DIR_ALL ) {
                blitTile( _a.tilSprite( "CLOF32.TIL", ( x + y ) % 4 ).image, x, y );
                continue;
            }
            QImage ground = _a.tilSprite( "GROUND32.TIL", t.terrainImageIndex ).image;
            const int shape = t.terrainFlags & 0x3;
            if ( shape & 2 )
                ground = ground.mirrored( true, false );
            if ( shape & 1 )
                ground = ground.mirrored( false, true );
            blitTile( ground, x, y );
        }
    }

    // --- Collect tile-unfit units ---
    std::vector<Unit> units;
    for ( int y = 0; y < h; ++y ) {
        for ( int x = 0; x < w; ++x ) {
            const size_t i = static_cast<size_t>( y ) * w + x;
            const fh2::WorldTile & t = world.tiles[i];
            const uint16_t objType = t.mainObjectType;
            if ( withFog && fogDir[i] == DIR_ALL )
                continue;

            if ( objType == OBJ_HERO ) {
                const fh2::WorldHero * hero = heroAt( world, static_cast<int>( i ) );
                if ( !hero )
                    continue;
                bool flip = false;
                const int base = heroDirBaseIndex( hero->direction, flip );
                // The hero stands on water -> he is on a boat. In-game a boat hero
                // is drawn with BOAT32 (not the race sprite), a boat flag (isShipMaster)
                // and a BOATSHAD shadow. Water == terrainImageIndex < GROUND's
                // GRASS_START_IMAGE_INDEX (30) and is fixed in the resources.
                const bool onWater = t.terrainImageIndex < 30;
                const char * icn = onWater ? "BOAT32.ICN" : heroIcnByRace( hero->race );
                if ( !icn )
                    continue;
                const fh2::IcnSprite & sp = _a.icnSprite( icn, base );
                if ( sp.isNull() )
                    continue;
                Unit u;
                u.tileIndex = static_cast<int>( i );
                u.sprite = sp.image;
                u.flip = flip;
                u.isHero = true;
                if ( onWater ) {
                    // Boat sprites are shifted to align with other boats (offset.y -= 11).
                    u.ox = flip ? ( TILE + 1 - sp.offsetX - sp.image.width() ) : sp.offsetX;
                    u.oy = sp.offsetY + TILE - 11;
                }
                else {
                    u.ox = flip ? ( TILE + 1 - sp.offsetX - sp.image.width() ) : sp.offsetX;
                    u.oy = sp.offsetY + TILE - 1;
                }
                const fh2::IcnSprite & sh = _a.icnSprite( onWater ? "BOATSHAD.ICN" : "SHADOW32.ICN",
                                                         onWater ? heroBoatShadowIndex( hero->direction ) : heroShadowIndex( hero->direction ) );
                if ( !sh.isNull() ) {
                    u.shadow = sh.image;
                    u.shOx = sh.offsetX;
                    u.shOy = sh.offsetY + TILE - ( onWater ? 11 : 0 );
                }
                // Hero's flag by color + direction (frame 0 for a static poster).
                const char * flagIcn = onWater ? heroBoatFlagIcnByColor( hero->color ) : heroFlagIcnByColor( hero->color );
                if ( flagIcn ) {
                    const fh2::IcnSprite & fsp = _a.icnSprite( flagIcn, base );
                    if ( !fsp.isNull() ) {
                        u.flag = fsp.image;
                        u.fOx = flip ? ( TILE - fsp.offsetX - fsp.image.width() ) : fsp.offsetX;
                        u.fOy = fsp.offsetY + TILE - ( onWater ? 11 : 1 );
                    }
                }
                units.push_back( std::move( u ) );
            }
            else if ( objType == OBJ_MONSTER ) {
                const int monsterId = t.mainPart.icnIndex + 1; // MONS32: monster index base = id*9
                const int frameBase = ( monsterId - 1 ) * 9;
                const fh2::IcnSprite & sp0 = _a.icnSprite( "MINIMON.ICN", frameBase );
                if ( sp0.isNull() )
                    continue;
                // The engine overlays the animation frame (base + 1 + seq) on top
                // of the base frame — the animated head/parts live there.
                CompositedSprite comb;
                if ( sp0.image.width() > 0 && frameBase + 1 < _a.icnFrameCount( "MINIMON.ICN" ) ) {
                    const fh2::IcnSprite & sp1 = _a.icnSprite( "MINIMON.ICN", frameBase + 1 );
                    comb = sp1.isNull() ? CompositedSprite{ sp0.image, sp0.offsetX, sp0.offsetY } : overlayFrames( sp0, sp1 );
                }
                else {
                    comb = CompositedSprite{ sp0.image, sp0.offsetX, sp0.offsetY };
                }
                Unit u;
                u.tileIndex = static_cast<int>( i );
                u.sprite = comb.image;
                u.ox = comb.ox + 16;
                u.oy = comb.oy + 30;
                units.push_back( std::move( u ) );
            }
            else if ( objType == OBJ_BOAT ) {
                int idx = t.mainPart.icnIndex;
                if ( idx == 255 )
                    idx = 18;
                const bool reflect = idx > 128;
                const int icnIndex = idx % 128;
                const fh2::IcnSprite & sp = _a.icnSprite( "BOAT32.ICN", icnIndex );
                if ( sp.isNull() )
                    continue;
                Unit u;
                u.tileIndex = static_cast<int>( i );
                u.sprite = sp.image;
                u.flip = reflect;
                u.ox = reflect ? ( TILE + 1 - sp.offsetX - sp.image.width() ) : sp.offsetX;
                u.oy = sp.offsetY + TILE - 11;
                const fh2::IcnSprite & sh = _a.icnSprite( "BOATSHAD.ICN", icnIndex );
                if ( !sh.isNull() ) {
                    u.shadow = sh.image;
                    u.shOx = sh.offsetX;
                    u.shOy = sh.offsetY + TILE - 11;
                }
                units.push_back( std::move( u ) );
            }
        }
    }

    // --- 2. Terrain and background layers ---
    for ( int y = 0; y < h; ++y ) {
        for ( int x = 0; x < w; ++x ) {
            const size_t i = static_cast<size_t>( y ) * w + x;
            const fh2::WorldTile & t = world.tiles[i];
            if ( withFog && fogDir[i] == DIR_ALL )
                continue;
            for ( const fh2::ObjectPart & part : t.groundParts ) {
                if ( part.layerType == 3 || part.layerType == 1 )
                    blitPart( part, x, y );
            }
            if ( t.mainPart.layerType == 3 || t.mainPart.layerType == 1 )
                blitMain( t, x, y );
        }
    }

    // --- 3. Monsters and boats: parts below their tiles ---
    for ( const Unit & u : units ) {
        if ( u.isHero )
            continue;
        const int tx = u.tileIndex % w;
        const int ty = u.tileIndex / w;
        const Bands b = splitBands( u.oy, u.sprite.height(), u.sprite.width() );
        if ( !b.below.isEmpty() )
            p.drawImage( QPoint( tx * TILE + u.ox + ( u.flip ? u.sprite.width() - b.below.width() : 0 ), ty * TILE + TILE ),
                         u.sprite.mirrored( u.flip, false ).copy( b.below ) );
    }

    // --- 4. Shadows ---
    for ( int y = 0; y < h; ++y ) {
        for ( int x = 0; x < w; ++x ) {
            const size_t i = static_cast<size_t>( y ) * w + x;
            const fh2::WorldTile & t = world.tiles[i];
            if ( withFog && fogDir[i] == DIR_ALL )
                continue;
            for ( const fh2::ObjectPart & part : t.groundParts ) {
                if ( part.layerType == 2 )
                    blitPart( part, x, y );
            }
            if ( t.mainPart.layerType == 2 )
                blitMain( t, x, y );
        }
    }
    for ( const Unit & u : units ) {
        if ( u.shadow.isNull() )
            continue;
        const int tx = u.tileIndex % w;
        const int ty = u.tileIndex / w;
        blitAt( u.shadow, tx * TILE + u.shOx, ty * TILE + u.shOy );
    }

    // --- 5. Hero lower parts (over shadows, under objects) ---
    for ( const Unit & u : units ) {
        if ( !u.isHero )
            continue;
        const int tx = u.tileIndex % w;
        const int ty = u.tileIndex / w;
        const Bands b = splitBands( u.oy, u.sprite.height(), u.sprite.width() );
        if ( !b.below.isEmpty() )
            p.drawImage( QPoint( tx * TILE + u.ox + ( u.flip ? u.sprite.width() - b.below.width() : 0 ), ty * TILE + TILE ),
                         u.sprite.mirrored( u.flip, false ).copy( b.below ) );
    }

    // --- 6. Object layer ---
    for ( int y = 0; y < h; ++y ) {
        for ( int x = 0; x < w; ++x ) {
            const size_t i = static_cast<size_t>( y ) * w + x;
            const fh2::WorldTile & t = world.tiles[i];
            if ( withFog && fogDir[i] == DIR_ALL )
                continue;
            for ( const fh2::ObjectPart & part : t.groundParts ) {
                if ( part.layerType == 0 ) {
                    if ( part.icnType == 12 && !isRandomMonsterType( t.mainObjectType ) )
                        continue;
                    blitPart( part, x, y );
                }
            }
            // main object part (layer 0), skip tile-unfit types
            if ( t.mainPart.layerType == 0 )
                blitMain( t, x, y );
            // A guarded mine shows its spell guardian (OBJNXTRA frame) on top.
            if ( t.mainObjectType == OBJ_MINE && t.metadata[2] >= SPELL_SETGUARDIAN && t.metadata[2] <= SPELL_SETWGUARDIAN ) {
                const fh2::ObjectPart guard = fh2::ObjectPart{ 0, 0, 39, static_cast<uint8_t>( t.metadata[2] - SPELL_SETGUARDIAN ) };
                blitPart( guard, x, y );
            }
        }
    }

    // --- 7. Units on their tiles ---
    for ( const Unit & u : units ) {
        const int tx = u.tileIndex % w;
        const int ty = u.tileIndex / w;
        const Bands b = splitBands( u.oy, u.sprite.height(), u.sprite.width() );
        if ( !b.on.isEmpty() ) {
            const QImage src = u.sprite.mirrored( u.flip, false );
            p.drawImage( QPoint( tx * TILE + u.ox + ( u.flip ? u.sprite.width() - b.on.width() : 0 ), ty * TILE + std::max( 0, u.oy ) ), src.copy( b.on ) );
        }
    }

    // --- 8. Top parts (ordinary) ---
    std::vector<std::pair<const fh2::ObjectPart *, int>> tallParts;
    for ( int y = 0; y < h; ++y ) {
        for ( int x = 0; x < w; ++x ) {
            const size_t i = static_cast<size_t>( y ) * w + x;
            const fh2::WorldTile & t = world.tiles[i];
            if ( withFog && fogDir[i] == DIR_ALL )
                continue;
            for ( const fh2::ObjectPart & part : t.topParts ) {
                bool tall = false;
                if ( y + 1 < h ) {
                    const fh2::WorldTile & below = world.tiles[static_cast<size_t>( y + 1 ) * w + x];
                    for ( const fh2::ObjectPart & lower : below.topParts ) {
                        if ( lower.uid == part.uid && lower.icnType != 14 ) {
                            tall = true;
                            break;
                        }
                    }
                }
                if ( tall )
                    tallParts.emplace_back( &part, static_cast<int>( i ) );
                else
                    blitPart( part, x, y );
            }
        }
    }

    // --- 9. Units above their tiles ---
    for ( const Unit & u : units ) {
        const int tx = u.tileIndex % w;
        const int ty = u.tileIndex / w;
        const Bands b = splitBands( u.oy, u.sprite.height(), u.sprite.width() );
        if ( !b.above.isEmpty() ) {
            const QImage src = u.sprite.mirrored( u.flip, false );
            p.drawImage( QPoint( tx * TILE + u.ox + ( u.flip ? u.sprite.width() - b.above.width() : 0 ), ty * TILE + u.oy ), src.copy( b.above ) );
        }
        // The hero's flag, drawn on top of the body.
        if ( u.isHero && !u.flag.isNull() ) {
            const QImage fsrc = u.flag.mirrored( u.flip, false );
            p.drawImage( tx * TILE + u.fOx, ty * TILE + u.fOy, fsrc );
        }
    }

    // --- 10. Tall top parts ---
    for ( const auto & [part, tileIndex] : tallParts ) {
        blitPart( *part, tileIndex % w, tileIndex / w );
    }

    // --- 10b. Flying ghosts over haunted / abandoned mines (OBJNHAUN) ---
    for ( int y = 0; y < h; ++y ) {
        for ( int x = 0; x < w; ++x ) {
            const size_t i = static_cast<size_t>( y ) * w + x;
            const fh2::WorldTile & t = world.tiles[i];
            if ( withFog && fogDir[i] == DIR_ALL )
                continue;
            const bool haunted = ( t.mainObjectType == OBJ_MINE && t.metadata[2] == SPELL_HAUNT );
            if ( t.mainObjectType != OBJ_ABANDONED_MINE && !haunted )
                continue;
            const fh2::IcnSprite & ghost = _a.icnSprite( "OBJNHAUN.ICN", 0 ); // frame 0 for a static poster
            if ( ghost.isNull() )
                continue;
            blitAt( ghost.image, x * TILE + ghost.offsetX, y * TILE + ghost.offsetY );
        }
    }

    // --- 11. Hero routes and intentions (drawn before the fog, like in the game) ---
    const auto stepDelta = [w]( int direction ) -> int {
        switch ( direction ) {
        case DIR_TOP: return -w;
        case DIR_TOP_RIGHT: return -w + 1;
        case DIR_RIGHT: return 1;
        case DIR_BOTTOM_RIGHT: return w + 1;
        case DIR_BOTTOM: return w;
        case DIR_BOTTOM_LEFT: return w - 1;
        case DIR_LEFT: return -1;
        case DIR_TOP_LEFT: return -w - 1;
        default: return 0;
        }
    };

    if ( routes != RouteMode::None ) {
        for ( const fh2::WorldHero & hero : world.heroes ) {
            if ( hero.color == 0 || hero.route.steps.empty() )
                continue;
            bool draw = false;
            switch ( routes ) {
            case RouteMode::None: break;
            case RouteMode::Player: draw = ( hero.color == selectedColor ); break;
            case RouteMode::Visible: draw = !hero.route.hide; break;
            case RouteMode::All: draw = true; break;
            }
            if ( !draw )
                continue;

            uint32_t movePoints = hero.movePoints;
            const auto & steps = hero.route.steps;
            size_t first = 0;
            if ( ( hero.modes & HERO_ENABLE_MOVE ) != 0 && !steps.empty() && hero.direction == steps[0].direction )
                first = 1;

            for ( size_t s = first; s < steps.size(); ++s ) {
                const fh2::RouteStep & st = steps[s];
                const int stepIndex = st.from + stepDelta( st.direction );

                if ( stepIndex < 0 || stepIndex >= w * h )
                    continue;
                const int nextDir = ( s + 1 < steps.size() ) ? steps[s + 1].direction : 0;
                const int cost = static_cast<int>( st.penalty );
                const int arrowIndex = routeIndex( st.direction, nextDir, cost );
                const bool green = ( movePoints >= st.penalty );
                if ( green )
                    movePoints -= st.penalty;

                const fh2::IcnSprite & arrow = _a.icnSprite( green ? "ROUTE.ICN" : "ROUTERED.ICN", arrowIndex );
                if ( arrow.isNull() )
                    continue;
                const int sx = stepIndex % w;
                const int sy = stepIndex / w;
                blitAt( arrow.image, sx * TILE + arrow.offsetX - 12, sy * TILE + arrow.offsetY + 2 );
            }

            // Intention icon at the destination tile: the game's adventure map
            // cursors (ADVMCO): 5 = crossed swords (attack), 9 = rearing horse
            // (visiting), 3 = castle (returning to an own castle).
            const fh2::RouteStep & last = steps.back();
            const int destIndex = last.from + stepDelta( last.direction );
            if ( destIndex >= 0 && destIndex < w * h ) {
                const fh2::WorldTile & t = world.tiles[destIndex];
                int icon = -1;
                const uint16_t objType = t.mainObjectType;
                if ( objType == OBJ_CASTLE || objType == OBJ_NON_ACTION_CASTLE ) {
                    bool own = false;
                    const int dx = destIndex % w;
                    const int dy = destIndex / w;
                    for ( const fh2::WorldCastle & c : world.castles ) {
                        if ( c.x == dx && c.y == dy && c.color == hero.color ) {
                            own = true;
                            break;
                        }
                    }
                    icon = own ? 3 : 5;
                }
                else if ( objType == OBJ_HERO || objType == OBJ_MONSTER ) {
                    icon = 5;
                }
                else if ( objType != 0 ) {
                    icon = 9; // any other object: visiting
                }

                if ( icon >= 0 ) {
                    const fh2::IcnSprite & ic = _a.icnSprite( "ADVMCO.ICN", icon );
                    if ( !ic.isNull() ) {
                        const int dx = destIndex % w;
                        const int dy = destIndex / w;
                        blitAt( ic.image, dx * TILE + TILE / 2 - ic.image.width() / 2, dy * TILE + TILE / 2 - ic.image.height() / 2 );
                    }
                }
            }
        }
    }

    // --- 12. Fog (drawn over the routes, like in the game) ---
    if ( withFog && selectedColor != 0 ) {
        for ( int y = 0; y < h; ++y ) {
            for ( int x = 0; x < w; ++x ) {
                const size_t i = static_cast<size_t>( y ) * w + x;
                if ( fogDir[i] == 0 )
                    continue;
                if ( fogDir[i] == DIR_ALL ) {
                    blitTile( _a.tilSprite( "CLOF32.TIL", ( x + y ) % 4 ).image, x, y );
                }
                else {
                    int idx = 0;
                    bool revert = false;
                    fogBorderIndex( fogDir[i], static_cast<int>( i ), idx, revert );
                    if ( idx < 0 )
                        blitTile( _a.tilSprite( "CLOF32.TIL", ( x + y ) % 4 ).image, x, y );
                    else {
                        const fh2::IcnSprite & sp = _a.icnSprite( "CLOP32.ICN", idx );
                        if ( !sp.isNull() ) {
                            const QImage & im = sp.image;
                            const int ox = revert ? ( TILE - sp.offsetX - im.width() ) : sp.offsetX;
                            blitAt( revert ? im.mirrored( true, false ) : im, x * TILE + ox, y * TILE + sp.offsetY );
                        }
                    }
                }
            }
        }

        // End of the fog pass. Own castles are forced visible above, so no
        // castle parts are drawn over the fog — enemy castles stay hidden.
    }

    p.end();
    return img;
}




} // namespace fh2poster
