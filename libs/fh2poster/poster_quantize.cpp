#include "poster_quantize.h"

#include "palette_cut.h"

#include <QBuffer>
#include <unordered_map>

namespace fh2poster {

// Native (real Qt) path: builds an indexed QImage (Format_Indexed8) with a
// median-cut palette and lets Qt encode the indexed PNG (with per-entry alpha
// via tRNS). Container type is QVector<QRgb> for real Qt's setColorTable.
QByteArray toQuantizedPng( const QImage & img )
{
    std::unordered_map<QRgb, int> hist;
    const std::vector<QRgb> pal = buildQuantPalette( img, 256, hist );

    QImage indexed( img.width(), img.height(), QImage::Format_Indexed8 );
    QVector<QRgb> qpal;
    qpal.reserve( pal.size() );
    for ( QRgb c : pal )
        qpal.push_back( c );
    indexed.setColorTable( qpal );

    uchar * dst = indexed.bits();
    const int dstBytes = indexed.bytesPerLine();
    const int srcBytes = img.bytesPerLine();
    std::unordered_map<QRgb, int> cache;
    cache.reserve( hist.size() );
    for ( int y = 0; y < img.height(); ++y ) {
        const QRgb * srow = reinterpret_cast<const QRgb *>( img.constBits() + static_cast<std::size_t>( y ) * srcBytes );
        uchar * drow = dst + static_cast<std::size_t>( y ) * dstBytes;
        for ( int x = 0; x < img.width(); ++x ) {
            const QRgb key = srow[x];
            const auto it = cache.find( key );
            if ( it != cache.end() ) {
                drow[x] = static_cast<uchar>( it->second );
            }
            else {
                const int idx = nearestPaletteIndex( pal, key );
                cache.emplace( key, idx );
                drow[x] = static_cast<uchar>( idx );
            }
        }
    }
    QBuffer buf;
    buf.open( QIODevice::WriteOnly );
    if ( !indexed.save( &buf, "PNG" ) )
        return {};
    return buf.data();
}

} // namespace fh2poster
