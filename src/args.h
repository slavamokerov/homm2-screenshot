#pragma once

#include <string>
#include <vector>

namespace fh2poster {

struct Args
{
    std::string savePath;
    std::string out = "poster.png";
    std::string fog = "auto";     // auto | color:N | none
    double scale = 2.0;           // 1 | 1.5 | 2
    int dpi = 300;
    std::string layout = "cardushe";      // preset name or a .json file
    std::string layoutPriority = "map";   // map | legend
    std::string blocks;                   // enabled blocks (comma list, empty = all)
    std::string chips;                    // enabled info chips (comma list, empty = all)
    std::string routes = "all";           // none | player | visible | all
    std::string dataDir;                  // folder with HEROES2.AGG (empty = auto)
    int castlesOverride = -1;             // override the castle count (-1 = from save)
    int heroesOverride = -1;
    int crop[4] = { -1, -1, -1, -1 };     // dev: crop the map in tiles (x, y, w, h)
    int castleIndex = -2;                 // dev: render one castle view (-1 lists, index renders)
    std::string icnDump;                  // dev: dump an ICN sprite ("name.icn:index")
    std::string icnSheet;                 // dev: sprite sheet ("name.icn:from:to:cols")
    int heroId = -1;                      // dev: render one hero card (hero id)
    std::string chipDump;                 // dev: render one info chip ("minimap" etc.)
    std::string dumpLayout;               // dev: print a preset as a JSON template
    bool debugMap = false;                // dev: print water-object/hero-on-water tiles
    bool quiet = false;                   // suppress route logs + info summary in stdout
    bool showHelp = false;
    std::string error;
};

Args parseArgs( int argc, char ** argv );

} // namespace fh2poster
