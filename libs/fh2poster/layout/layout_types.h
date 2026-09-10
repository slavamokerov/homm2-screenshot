#pragma once

#include <map>
#include <string>
#include <vector>

namespace layout {

struct Size
{
    int w = 0;
    int h = 0;
};

struct Rect
{
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;

    bool empty() const
    {
        return w <= 0 || h <= 0;
    }

    bool intersects( const Rect & o ) const
    {
        return !( x + w <= o.x || o.x + o.w <= x || y + h <= o.y || o.y + o.h <= y );
    }

    bool inside( const Rect & o ) const
    {
        return x >= o.x && y >= o.y && x + w <= o.x + o.w && y + h <= o.y + o.h;
    }
};

enum class Side
{
    TOP,
    BOTTOM,
    LEFT,
    RIGHT
};

// Parameters of the auto-grid for a zone (port of the prototype algorithm).
struct GridParams
{
    int capW = 0;              // element width ceiling (0 — unlimited)
    int floorW = 0;            // element width floor (0 — unlimited)
    float aspect = 0.75f;      // element height / width
    int maxRows = 2;           // max number of rows (horizontal zone) or columns (vertical zone)
    float maxZoneFrac = 0.19f; // max zone thickness as a fraction of the map size
    int gap = 50;              // gap between elements
    int padding = 20;          // zone inset from the map

    // Constant element size (scale-1 px). When > 0 the grid keeps the element at
    // this size multiplied by the poster scale (independent of the map size) and
    // only picks how many columns fit — instead of resizing the element to fill
    // the zone length.
    int fixedW = 0;
    int fixedH = 0;

    // Optional per-element sizes (scale-1 px), used with fixedW>0. When not empty
    // each element keeps its own width / height (empty entries fall back to fixedW
    // / fixedH). Lets a mixed group (a big minimap + compact text chips) pack
    // without wasted space: cards keep their own size.
    std::vector<int> fixedWList;
    std::vector<int> fixedHList;
};

// One legend zone on one of the four sides of the map.
// single == true: one block spanning the whole zone length, thickness = length * singleAspect
// (used for the title). Otherwise: a grid of count elements, sized by GridParams.
struct ZoneDef
{
    std::string id;
    Side side = Side::BOTTOM;
    bool single = false;
    float singleAspect = 0.0f;   // thickness = length * singleAspect (used when singleFixedH == 0)
    int singleFixedH = 0;        // constant thickness (scale-1 px), independent of the map size
    GridParams grid;
};

struct LayoutParams
{
    std::string name;
    int frame = 60;               // outer canvas margin
    float sideMinFrac = 0.078f;   // min side zone width as a fraction of the map size
    int sideWidth = 0;            // fixed right-side width (0 = auto from the canvas height)
    int scale = 1;                // poster scale multiplier (applied to every element, not the map tile count)
    std::vector<ZoneDef> zones;   // legend zones (the map itself is always in the center)
};

// One kingdom group (a player color): its header band and the body rectangle
// that wraps its hero cards and (optionally) castle renders.
struct GroupRect
{
    int color = 0;
    Rect header; // title band (color name)
    Rect body;   // boxed area holding the group's cards
};

// Result: canvas size, the map rect and the rects of every legend element.
struct LayoutResult
{
    int canvasW = 0;
    int canvasH = 0;
    Rect map;
    std::map<std::string, std::vector<Rect>> blocks; // zone id → element rects
    std::vector<GroupRect> groups;                    // kingdom groups (cartouche)
    std::vector<std::string> warnings;

    Rect canvas() const
    {
        return Rect{ 0, 0, canvasW, canvasH };
    }
};

// Input for the specialized "cartouche" (in-game style) layout: a square map on
// top, a fixed row of info chips, then kingdom groups (heroes → castles) that
// grow downward. All sizes are in poster pixels (scale applied).
struct CartoucheInput
{
    int mapPx = 0;                                   // the square map size in px
    int frame = 30;                                  // outer margin (border + padding)
    bool hasTitle = false;
    std::vector<Size> chipSizes;                     // info chips, in order (w,h)
    struct Group
    {
        int color = 0;
        int heroCount = 0;   // number of hero cards
        int castleCount = 0; // number of castle renders (may be 0)
    };
    std::vector<Group> groups; // in display order

    // Card sizes (px).
    Size heroCard{ 360, 210 };
    Size castleCard{ 266, 196 };
    int cardGap = 10;
    int chipGap = 12;
};

} // namespace layout
