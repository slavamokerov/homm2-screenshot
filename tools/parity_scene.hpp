// Shared parity scene — MUST compile against BOTH real Qt and the rastercompat
// mini-Qt. Exercises the primitives whose semantics are the highest risk for
// pixel mismatch: SourceOver alpha blend, drawImage (+sub-rect), drawRect
// (brush fill + pen stroke), QImage::scaled (nearest) and pixel accessors.
#pragma once

#include <QImage>
#include <QPainter>
#include <QRect>
#include <QColor>
#include <QPoint>

#include <cstdint>

inline QImage buildSprite()
{
    // 8x6 sprite with transparent, half-alpha, full-alpha and shadow-like pixels.
    QImage s( 8, 6, QImage::Format_ARGB32 );
    s.fill( Qt::transparent );
    // Full-opaque block.
    for ( int y = 0; y < 3; ++y )
        for ( int x = 0; x < 3; ++x )
            s.setPixelColor( x, y, QColor( 255, 64, 32, 255 ) );
    // Half-transparent block.
    for ( int y = 0; y < 3; ++y )
        for ( int x = 3; x < 6; ++x )
            s.setPixelColor( x, y, QColor( 32, 200, 64, 128 ) );
    // Semi-transparent shadow-ish bottom row.
    for ( int x = 0; x < 8; ++x )
        s.setPixelColor( x, 5, QColor( 0, 0, 0, 96 ) );
    return s;
}

inline QImage drawScene()
{
    QImage out( 64, 48, QImage::Format_ARGB32 );
    out.fill( QColor( 20, 20, 26, 255 ) );

    QPainter p( &out );
    p.setRenderHint( QPainter::SmoothPixmapTransform, false );

    // Opaque app background rectangle (SourceOver over opaque -> fine).
    p.fillRect( QRect( 0, 0, 64, 48 ), QColor( 40, 36, 48, 255 ) );

    // The sprite blitted with transparent/alpha pixels (SourceOver blend).
    const QImage s = buildSprite();
    p.drawImage( 6, 5, s );

    // Sub-rect blit (bottom 8x3 of the sprite).
    p.drawImage( 20, 5, s, 0, 3, 8, 3 );

    // Nearest scaling 2x of the sprite (exercises QImage::scaled fast).
    const QImage big = s.scaled( 16, 12, Qt::IgnoreAspectRatio, Qt::FastTransformation );
    p.drawImage( 36, 8, big );

    // drawRect with brush fill + pen stroke.
    p.setPen( QPen( QColor( 90, 90, 200, 255 ), 1 ) );
    p.setBrush( QColor( 60, 30, 90, 200 ) ); // semi-transparent fill
    p.drawRect( QRect( 40, 24, 18, 14 ) );

    // Painter scale (as the map render uses: p.scale(2,2) then draw tiles).
    p.save();
    p.scale( 2, 2 );
    p.drawImage( 4, 30, s, 0, 0, 8, 6 );
    p.fillRect( QRect( 16, 30, 6, 4 ), QColor( 120, 40, 30, 180 ) );
    p.restore();

    p.end();

    // Second pass through setPixelColor/pixelColor to exercise accessors.
    out.setPixelColor( 1, 1, QColor( 255, 255, 0, 255 ) );
    const QColor c = out.pixelColor( 1, 1 );
    if ( c.red() != 255 )
        out.setPixelColor( 2, 1, QColor( 0, 255, 255, 255 ) );

    return out;
}

inline uint64_t hashRgba( const QImage & img )
{
    // Canonical RGBA byte order, row-major, stride = width*4.
    // Real Qt: convertToFormat(RGBA8888); mini-Qt: toRgba8().
    uint64_t h = 14695981039346656037ULL;
    auto mix = [&]( uint32_t v ) {
        h ^= v;
        h *= 1099511628211ULL;
    };
    for ( int y = 0; y < img.height(); ++y ) {
        for ( int x = 0; x < img.width(); ++x ) {
            const QColor c = img.pixelColor( x, y );
            if ( c.alpha() == 0 ) {
                mix( 0 );
                continue;
            }
            mix( ( ( uint32_t )c.red() << 24 ) | ( ( uint32_t )c.green() << 16 ) | ( ( uint32_t )c.blue() << 8 ) | ( uint32_t )c.alpha() );
        }
    }
    return h;
}
