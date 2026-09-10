#include "herocard.h"

#include <QPainter>
#include <algorithm>

#include "constants.h"
#include "gamefont.h"
#include "textutil.h"

namespace fh2poster {

HeroCardRender::HeroCardRender( const fh2::Assets & assets )
    : _a( assets )
{}

// Height of the card content: the fixed header/army area, plus artifact rows
// (each row 36px). Cards with no artifacts shrink to the army area only, so the
// legend packs tightly (castles stay fixed).
int HeroCardRender::contentHeight( const fh2::WorldHero & hero ) const
{
    const int slot = 46, ay = 76;
    const int armyBottom = ay + slot;
    int artifactCount = 0;
    for ( const auto & [id, ext] : hero.artifacts )
        if ( id > 0 && id <= 82 )
            ++artifactCount;
    if ( artifactCount == 0 )
        return armyBottom + 8;
    const int colsPerRow = std::max( 1, ( CARD_W - 16 ) / 34 );
    const int rows = ( artifactCount + colsPerRow - 1 ) / colsPerRow;
    const int artY = armyBottom + 6;
    return artY + rows * 36 + 4;
}

QImage HeroCardRender::render( const fh2::WorldHero & hero ) const
{
    // Keep the card's natural size, but drop the empty strip below the content
    // (no artifacts) so the legend packs tightly; the portrait/text keep their
    // normal size.
    const int cardH = std::max( 64, contentHeight( hero ) );
    QImage img( CARD_W, cardH, QImage::Format_ARGB32 );
    img.fill( qRgba( 0, 0, 0, 0 ) );
    QPainter p( &img );
    p.setRenderHint( QPainter::SmoothPixmapTransform, false );

    fh2::GameFont font( &_a );

    // Portrait, top-left (56x56, fills its slot like a monster).
    const QImage portrait = _a.portraitImage( hero.portrait > 0 ? hero.portrait : hero.id );
    if ( !portrait.isNull() ) {
        QImage small = portrait.scaled( 56, 56, Qt::IgnoreAspectRatio, Qt::FastTransformation );
        p.drawImage( 8, 8, small );
    }

    // Info block to the right of the portrait: name, class, level + base skills.
    const int ix = 72;
    int y = 14;
    const QString name = fh2::decodeCp1251( hero.name );
    font.drawText( p, ix, y, name, fh2::GameFont::Size::NORMAL, fh2::GameFont::Color::YELLOW );
    y += 22;

    const QString cls = QString::fromStdString( fh2::raceName( static_cast<int>( hero.race ) ) );
    font.drawText( p, ix, y, cls, fh2::GameFont::Size::NORMAL, fh2::GameFont::Color::WHITE );
    y += 22;

    const int level = fh2::heroLevel( hero.experience );
    const QString skills = QString( "Lv %1  A%2 D%3 P%4 K%5" )
                               .arg( level )
                               .arg( hero.primary[0] )
                               .arg( hero.primary[1] )
                               .arg( hero.primary[3] )
                               .arg( hero.primary[2] );
    font.drawText( p, ix, y, skills, fh2::GameFont::Size::NORMAL, fh2::GameFont::Color::WHITE );

    // Army row (5 slots, sized to the monster icons).
    const int slot = 46;
    const int step = 50;
    const int x0 = ( CARD_W - ( 5 * step - 4 ) ) / 2;
    const int ay = 76;
    for ( int i = 0; i < 5; ++i ) {
        const int mid = hero.monsterId[i];
        const QRect cell( x0 + i * step, ay + ( slot - 44 ) / 2, slot, slot );
        p.setPen( QPen( QColor( 60, 55, 45 ), 1 ) );
        p.setBrush( QColor( 40, 34, 24, 0 ) );
        p.drawRect( cell );
        if ( mid > 0 && mid <= 72 ) {
            const fh2::IcnSprite & mon = _a.icnSprite( "MONS32.ICN", mid - 1 );
            if ( !mon.isNull() )
                p.drawImage( cell.x() + ( slot - mon.image.width() ) / 2, cell.y() + ( slot - mon.image.height() ) / 2, mon.image );
            if ( hero.monsterCount[i] != 0 ) {
                const QString count = QString::number( hero.monsterCount[i] );
                font.drawText( p, cell.right() - font.textWidth( count, fh2::GameFont::Size::SMALL ) - 2, cell.bottom() - font.lineHeight( fh2::GameFont::Size::SMALL ) + 1,
                               count, fh2::GameFont::Size::SMALL, fh2::GameFont::Color::WHITE );
            }
        }
    }

    // Artifacts (no label), icons wrapped.
    {
        int ax = 8;
        int artY = ay + slot + 6;
        for ( const auto & [id, ext] : hero.artifacts ) {
            if ( id <= 0 || id > 82 )
                continue;
            const fh2::IcnSprite & art = _a.icnSprite( "ARTFX.ICN", id - 1 );
            if ( art.isNull() )
                continue;
            if ( ax + 32 > CARD_W - 4 ) {
                ax = 8;
                artY += 36;
            }
            p.drawImage( ax, artY, art.image );
            ax += 34;
        }
    }

    p.end();
    return img;
}

} // namespace fh2poster
