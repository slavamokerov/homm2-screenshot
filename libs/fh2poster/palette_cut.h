#ifndef FH2POSTER_PALETTE_CUT_H
#define FH2POSTER_PALETTE_CUT_H

// Shared indexed-PNG palette math (median-cut). Used by both the native Qt build
// and the rastercompat mini-Qt (WASM) build, so it must only rely on API present
// in both: QImage, QRgb, qAlpha/qRed/qGreen/qBlue, plus the std library.

#include <algorithm>
#include <unordered_map>
#include <utility>
#include <vector>

#include <QImage>

namespace fh2poster {

inline int paletteChan( QRgb c, int ch )
{
    if ( ch == 0 )
        return qAlpha( c );
    if ( ch == 1 )
        return qRed( c );
    if ( ch == 2 )
        return qGreen( c );
    return qBlue( c );
}

// Histogram the image's unique ARGB colours and run a frequency-weighted
// median-cut down to <= maxColors representative colours.
inline std::vector<QRgb> buildQuantPalette( const QImage & img, int maxColors, std::unordered_map<QRgb, int> & hist )
{
    hist.reserve( std::size_t( 1 ) << 20 );
    for ( int y = 0; y < img.height(); ++y ) {
        const QRgb * row = reinterpret_cast<const QRgb *>( img.constBits() + static_cast<std::size_t>( y ) * img.bytesPerLine() );
        for ( int x = 0; x < img.width(); ++x ) {
            QRgb c = row[x];
            if ( qAlpha( c ) < 16 )
                c = 0u; // near-invisible -> transparent, saves a palette slot
            const auto it = hist.find( c );
            if ( it == hist.end() )
                hist.emplace( c, 1 );
            else
                ++it->second;
        }
    }

    std::vector<std::pair<QRgb, int>> items( hist.begin(), hist.end() );
    std::vector<QRgb> pal;

    if ( static_cast<int>( items.size() ) <= maxColors ) {
        pal.reserve( items.size() );
        for ( const auto & kv : items )
            pal.push_back( kv.first );
        return pal;
    }

    struct Box
    {
        size_t begin, end;
    };
    std::vector<Box> boxes;
    boxes.push_back( { 0, items.size() } );

    while ( static_cast<int>( boxes.size() ) < maxColors ) {
        int best = -1, bestChannel = -1;
        long long bestScore = -1;
        for ( size_t b = 0; b < boxes.size(); ++b ) {
            if ( boxes[b].end - boxes[b].begin < 2 )
                continue;
            long long cnt = 0;
            for ( size_t i = boxes[b].begin; i < boxes[b].end; ++i )
                cnt += items[i].second;
            for ( int ch = 0; ch < 4; ++ch ) {
                int mn = 255, mx = 0;
                for ( size_t i = boxes[b].begin; i < boxes[b].end; ++i ) {
                    const int v = paletteChan( items[i].first, ch );
                    if ( v < mn )
                        mn = v;
                    if ( v > mx )
                        mx = v;
                }
                const long long score = static_cast<long long>( mx - mn ) * cnt;
                if ( score > bestScore ) {
                    bestScore = score;
                    best = static_cast<int>( b );
                    bestChannel = ch;
                }
            }
        }
        if ( best < 0 )
            break;
        Box & bb = boxes[best];
        std::sort( items.begin() + bb.begin, items.begin() + bb.end, [&]( const auto & A, const auto & B ) {
            return paletteChan( A.first, bestChannel ) < paletteChan( B.first, bestChannel );
        } );
        const size_t mid = ( bb.begin + bb.end ) / 2;
        Box right{ mid, bb.end };
        bb.end = mid;
        boxes.push_back( right );
    }

    for ( const Box & b : boxes ) {
        long long r = 0, g = 0, bl = 0, a = 0, t = 0;
        for ( size_t i = b.begin; i < b.end; ++i ) {
            const QRgb c = items[i].first;
            const int n0 = items[i].second;
            r += static_cast<long long>( qRed( c ) ) * n0;
            g += static_cast<long long>( qGreen( c ) ) * n0;
            bl += static_cast<long long>( qBlue( c ) ) * n0;
            a += static_cast<long long>( qAlpha( c ) ) * n0;
            t += n0;
        }
        if ( t == 0 )
            continue;
        pal.push_back( qRgba( static_cast<int>( r / t ), static_cast<int>( g / t ), static_cast<int>( bl / t ), static_cast<int>( a / t ) ) );
    }
    return pal;
}

inline int nearestPaletteIndex( const std::vector<QRgb> & pal, QRgb c )
{
    int bestIdx = 0, bestDist = 0x7fffffff;
    for ( int i = 0; i < static_cast<int>( pal.size() ); ++i ) {
        const QRgb p = pal[i];
        const int dr = qRed( c ) - qRed( p );
        const int dg = qGreen( c ) - qGreen( p );
        const int db = qBlue( c ) - qBlue( p );
        const int da = qAlpha( c ) - qAlpha( p );
        const int dist = dr * dr + dg * dg + db * db + da * da * 2;
        if ( dist < bestDist ) {
            bestDist = dist;
            bestIdx = i;
        }
    }
    return bestIdx;
}

} // namespace fh2poster

#endif // FH2POSTER_PALETTE_CUT_H
