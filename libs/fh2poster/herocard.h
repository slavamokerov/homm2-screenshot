#pragma once

#include <QImage>

#include "assets.h"
#include "worldparse.h"

namespace fh2poster {

// Renders a hero card: portrait, name, class, level, primary skills, army and
// artifacts. The card is drawn at a fixed natural size and can be scaled when
// composed onto the poster.
class HeroCardRender
{
public:
    explicit HeroCardRender( const fh2::Assets & assets );

    // The card's background panel is transparent (content stays opaque), so it can
    // sit over the map.
    QImage render( const fh2::WorldHero & hero ) const;

    // The rendered card's natural height (artifacts add rows; no artifacts shrink
    // to the army area). Used so the legend packs cards at their own size.
    int contentHeight( const fh2::WorldHero & hero ) const;

    static constexpr int CARD_W = 360;
    static constexpr int CARD_H = 210;

private:
    const fh2::Assets & _a;
};

} // namespace fh2poster
