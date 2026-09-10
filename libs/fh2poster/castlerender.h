#pragma once

#include <QImage>

#include "assets.h"
#include "worldparse.h"

namespace fh2poster {

// Renders a castle as in the game's castle screen: the 3D town view built from
// the constructed buildings plus the garrison army strip below.
class CastleRender
{
public:
    explicit CastleRender( const fh2::Assets & assets );

    // Returns the castle view at natural scale (640 wide; the height depends on
    // the race background + the garrison strip).
    QImage render( const fh2::WorldCastle & castle ) const;

    // The height of the rendered card at card width (360), used by the layout so
    // the cell matches the content and no empty strip is left below it.
    int contentHeight( const fh2::WorldCastle & castle ) const;

private:
    const fh2::Assets & _a;
};

} // namespace fh2poster
