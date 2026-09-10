#pragma once

#include <QImage>
#include <set>
#include <string>

#include "assets.h"
#include "savefile.h"
#include "worldparse.h"

namespace fh2poster {

// Renders the info chips of the poster legend: minimap, resources, calendar,
// rumors, obelisk puzzle and victory conditions.
class InfoChips
{
public:
    explicit InfoChips( const fh2::Assets & assets );

    // chip: "minimap" | "resources" | "calendar" | "rumors" | "puzzle" | "victory".
    // The background is transparent for the text chips (opaque content); the
    // minimap and the puzzle keep an opaque backdrop.
    QImage renderChip( const std::string & chip, const fh2::WorldData & world, const fh2::MapInfo & mapInfo, int selectedColor, int w, int h ) const;

    // The compact info aggregate: resources + victory conditions on the first
    // row, events + rumors on the second (2x2, no borders). If a sub-chip is
    // disabled (enabledNames empty = all of them), it is skipped.
    QImage renderInfo( const fh2::WorldData & world, const fh2::MapInfo & mapInfo, int selectedColor, int colW, const std::set<std::string> & enabledNames ) const;

private:
    const fh2::Assets & _a;
};

} // namespace fh2poster
