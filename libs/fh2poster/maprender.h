#pragma once

#include <QImage>

#include "assets.h"
#include "worldparse.h"

namespace fh2poster {

enum class RouteMode
{
    None,
    Player,
    Visible,
    All
};

// Renders the world map into a square image (mapPx = tiles * 32 * scale).
// selectedColor — the player whose fog of war is used (0 = no fog).
class MapRender
{
public:
    explicit MapRender( const fh2::Assets & assets );

    QImage render( const fh2::WorldData & world, int mapPx, int selectedColor, bool withFog, RouteMode routes ) const;

private:
    const fh2::Assets & _a;
};

} // namespace fh2poster
