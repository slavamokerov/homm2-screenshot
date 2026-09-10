#pragma once

#include <QImage>
#include <QString>
#include <string>

#include "assets.h"
#include "layout_types.h"
#include "maprender.h"
#include "savefile.h"
#include "worldparse.h"

namespace fh2poster {

// Poster generation options (the CLI maps its flags onto this struct; the
// save editor GUI will use the same struct for its "Export poster" dialog).
struct PosterOptions
{
    std::string layoutName = "cardushe"; // preset name (a JSON layout comes later)
    std::string layoutPriority = "map";  // map | legend
    int selectedColor = 0;               // fog point of view (0 = no fog)
    bool withFog = true;
    RouteMode routes = RouteMode::All;
    double scale = 2.0;                  // map scale multiplier
    std::string blocks;                  // enabled blocks (comma list, empty = all)
    std::string chips;                   // enabled info chips (comma list, empty = all)
    int castlesOverride = -1;            // castle count override (-1 = from save)
    int heroesOverride = -1;             // hero count override (-1 = from save)
};

// Renders the full poster. Returns a null image on failure.
// If layoutOut is not null, the computed geometry is stored there (for
// diagnostics and tests).
QImage renderPoster( const fh2::SaveFile & save, const fh2::WorldData & world, const fh2::Assets & assets, const PosterOptions & options,
                     layout::LayoutResult * layoutOut = nullptr, QString * errorOut = nullptr );

// The map image alone (used by the CLI's --crop diagnostics).
QImage renderMap( const fh2::WorldData & world, const fh2::Assets & assets, int mapPx, int selectedColor, bool withFog, RouteMode routes );

// The kingdom whose resources are shown in the info chips: the selected player,
// or — when no player is selected (no fog / all kingdoms) — the human player of
// the save. Color 0 is the neutral kingdom, whose resources are always zero.
int resourceColor( const fh2::SaveFile & save, const fh2::WorldData & world, int selectedColor );

} // namespace fh2poster
